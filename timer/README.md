
定时器处理非活动连接
===============
由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现一个服务器定时器，处理这种非活跃连接，释放连接资源。利用alarm函数周期性地触发SIGALRM信号,该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.
> * 统一事件源
> * 基于升序链表的定时器
> * 处理非活动连接


> * clinet_data 连接信息结构
成员:
sockaddr_in address 客户端地址
int sockfd          记录客户端文件描述符
util_timer *timer   时间节点

> * class util_timer 时间节点类
数据成员:
time_t expire        时间
void (*cb_func)(clinet_data *) 回调函数
clinet_data *user_data        记录用户数据(地址 inode等)
util_timer *prev    指向前一个节点
util_timer *next    指向后一个节点

构造函数:
* util_timer() : prev(NULL), next(NULL) {}

> * class sort_timer_lst 时间节点队列
数据成员:
* util_timer *head    队列头
* util_timer *tail    队列尾

构造函数:
* sort_timer_lst(): 对队列头，尾初始化为NULL
* ~sort_timer_lst(): 循环释放节点

成员函数:
* void add_timer(util_timer *timer)
向队列添加节点 当timer为NULL时返回，第一个节点将头尾指向timer，当timer->expire（时间）小于 head的时将timer设为head并返回,否则调用private成员函数add_timer(util_timer *timer, util_timer *lst_head)从新添加 (add_timer(timer, head))

* void adjust_timer(util_timer *timer)
调整节点将tmp指向timer下一个节点当tmp为空或当timer->expire < tmp->expire时返回(timer为尾或顺序时),否则将timer从链表中摘除调用add_timer(util_timer *timer, util_timer *lst_head)从新加入（add_timer(timer, timer->next)）

* void del_timer(util_timer *timer)
删除节点 

* void tick()
处理超时节点，获得当前时间，对于超时节点调用回调函数并移除(由于时排序队列时间按大小排序)

* void add_timer(util_timer *timer, util_timer *lst_head)
向timer列表添加timer，prev 指向前一个节点， tmp 指向当前节点 循环调用找到第一时间大于 timer->expire 的节点并将timer插入其前面， 当tmp到尾部时 将节点插入尾

> *  Utils 处理epoll任务类
static int *u_pipefd;  管道id
sort_timer_lst m_timer_list; 时间队列
static int u_epollfd;  epoll树根节d点inode
int m_TIMESLOT; 触发模式 ET | LT

构造函数:
Utils(){};
~Utils(){};

成员函数:
init(int timeslot)
设置触发模式

int setnonblocking(int fd)
对fd设置为O_NONBLOCK 返回旧的标志

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
创建epoll_event结构event 将fd加入event 当  TRIGMode == 1 事件为
EPOLLIN | EPOLLET | EPOLLRDHUP,否则为 EPOLLIN | EPOLLRDHUP
(EPOLLIN 读， EPOLLRDHUP 流套接字对等关闭连接，或关闭写入连接的一半一端关闭了连接的写入功能，但仍然可以接收对端发送的数据。而对端则仍然可以继续发送数据。这种情况通常用于在双方通信中，有一方需要先关闭写入连接，再等待对方回复后再关闭整个连接的情况。, EPOLLET 边沿触发)，向树注册fd，并调用setnonblocking(fd)将fd设为非阻塞

static void sig_handler(int sig)
信号处理函数 利用u_pipefd[1]向管道写端写入信号值,写之前保存errno写之后恢复errno

void addsig(int sig, void(handler)(int), bool restart = true)
注册信号处理函数 当 restart时 sigaction()->sa_flags |= SA_RESTART SA_RESTART 标志告诉操作系统在信号处理函数返回后自动重启被中断的系统调用，而不是让其返回 EINTR 错误。这可以简化应用程序的编写，因为它们可以像没有中断一样继续执行阻塞式系统调用（如 read 或 write），而不必担心信号的干扰

void timer_handler()
定时处理任务，重新定时以不断触发SIGALRM信号,调用 sort_timer_lst()->tick()函数处理时间，alarm(m_TIMESLOT)定时

void show_error(int connfd, const char *info)
向connfd发送info,，并关闭connfd

=================================
void cb_func(clinet_data *user_data)
回调函数 将user_data->sock_fd 从树上摘除 关闭连接 将http_conn::m_user_count减一(static 成员)




