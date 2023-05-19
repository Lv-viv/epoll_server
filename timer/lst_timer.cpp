#include "../timer/lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while (tmp != NULL)
    {
        /* code */
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{

    if (!timer)
    {
        return;
    }

    if (!head)
    {
        head = tail = timer;
    }

    if (timer->expire < head->expire)
    { // timer 时间 < head
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer) // 调整链表
{
    if (!timer)
    {
        return;
    }

    util_timer *tmp = timer->next;
    if (!tmp || timer->expire < tmp->expire)
    {
        return;
    }

    if (timer == head) // 将timer从链表取出
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer) // 删除timer
{
    if (!timer)
    {
        return;
    }

    if ((timer == head) && (timer == tail)) //只有一个节点
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head) //节点是头
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }

    if (timer == tail) //节点是尾
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    //其他情况
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick()
{ // 处理超时节点
    if (!head)
    {
        return;
    }

    time_t cur = time(NULL); // 当前时间
    util_timer *tmp = head;

    while (tmp)
    {
        if (cur < tmp->expire)
        {
            // 当前时间小于链表某个节点时间
            break;
        }

        tmp->cb_func(tmp->user_data); // 回调函数
        // 移除tmp节点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) // 向timer列表添加timer
{
    util_timer *prev = lst_head;  //指向前一个节点
    util_timer *tmp = prev->next; //指向当前节点

    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;

            tmp->prev = timer;
            timer->prev = prev;
            break;
        }

        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);      // 返回（作为函数结果）文件描述符标志；arg被忽略。
    int now_option = old_option | O_NONBLOCK; // 非阻塞
    fcntl(fd, F_SETFL, now_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if (1 == TRIGMode)
    {
        // 对端正常关闭（程序里close()，shell下kill或ctr+c），触发EPOLLIN和EPOLLRDHUP
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (one_shot)
    {
        event.events |= EPOLLONESHOT; // EPOLLSHOT的作用主要用于多线程中
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig)
{
    //cout << "Sig_handler" << endl;
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0); // 写端
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    // SA_RESTART信号其实就是作用于当我程序进行运行的时候
    // 这个当前的程序处于阻塞状态 比如read当前的STDIN_FILENO
    // 终端输入 时候 这个时候程序处于阻塞状态 等待输入字符串
    // 但是当我用kill指令发送给当前程序时候
    // 这个read函数阻塞状态突然就会被打断
    // errno会被赋值EINTR 并且这个系统调用read会被返回-1
    // 代表出错了 而且会立即被退出程序abort 中断程序运行
    // SA_RESTART 标志告诉操作系统在信号处理函数返回后自动重启被中断的系统调用，
    // 而不是让其返回 EINTR 错误。这可以简化应用程序的编写，
    // 因为它们可以像没有中断一样继续执行阻塞式系统调用（如 read 或 write），
    // 而不必担心信号的干扰
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa)); // sizeof 0 改为 sizeof(sa);
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);                 // 将信号集制1
    assert(sigaction(sig, &sa, NULL) != -1); // 捕捉信号
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_list.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(clinet_data *user_data) // 回调函数
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--; // 用户减少
}
