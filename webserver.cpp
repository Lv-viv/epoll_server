#include "webserver.h"

WebServer::WebServer()
{
    users = new http_conn[MAX_FD]; // 申请http请求

    // root文件名路径
    char server_path[200];
    getcwd(server_path, 200); // 获取当前文件路径 /home/v/MyItem/MyNetWorkItem/src
    char root[6] = "/root"; //html文件所在目录
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path); // 当前路径拷贝到m_root
    strcat(m_root, root);

    // m_root = /home/v/MyItem/MyNetWorkItem/src/root
    users_timer = new clinet_data[MAX_FD]; //连接的用户信息
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);

    delete[] users_timer;
    delete[] users;
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write,
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_modle)
{
    m_port = port;
    m_user = user;
    m_passWold = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;

    m_OPT_LINGER = opt_linger; //setsockopt
    m_TRIGMode = trigmode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actormodel = actor_modle;
}

void WebServer::trig_mode()
{ // LT = 0             ET = 1
    // LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0; 
        m_CONNTrigmode = 0;
    }
    // LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    // ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    // ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        // 初始化日志
        if (1 == m_log_write)
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
        { //日志队列初始为0
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
        }
        //LOG_INFO("%s", "Hello");
    }
}

void WebServer::sql_pool()
{
    // 初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWold, m_databaseName, 3306, m_sql_num, m_close_log);

    // 初始化数据库读取表
    users->initmysql_result(m_connPool); //获取一次即可
}


void WebServer::eventListen()
{//监听事件
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
//O_LINGER选项用于控制socket关闭时的行为，
//如果设置了SO_LINGER选项，那么在关闭socket时，
//会等待一段时间，直到所有数据都发送完毕或者超时，
//才会真正关闭socke
    if (0 == m_OPT_LINGER)
    { ////优雅关闭连接 //用于任意类型、任意状态套接口的设置选项值
        struct linger temp = {0, 1}; //非延迟关闭
        // 为了允许SO_LINGER，应用程序应将l_onoff设为非零，
        // 将l_linger设为零或需要的超时值（以秒为单位），然后调用setsockopt()

        // 在TCP连接中，recv等函数默认为阻塞模式(block)，即直到有数据到来之前函数不会返回，
        // 而我们有时则需要一种超时机制使其在一定时间后返回而不管是否有数据到来
        // ，这里我们就会用到setsockopt()函数：

        // SO_LINGER struct linger FAR* 如关闭时有未发送数据，则逗留。
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));
        //在server 的TCP 的连接没有完全断开之前不允许重新监听 因为tcp没有完全断开（指connfd）没有断开而我们重新监听的(listenfd) 
        //解决 setsockopt()设置socket描述符的选择SO_REUSEADDR 为创建端口号相同但IP不同的多个socket描述符
    }
    else if (1 == m_OPT_LINGER)
    {
        // 如果在发送数据的过程中(send()没有完成，还有数据没发送)而调用了closesocket(),以前我们
        //  一般采取的措施是"从容关闭"shutdown(s,SD_BOTH),但是数据是肯定丢失了，如何设置让程序满足具体
        //  应用的要求(即让没发完的数据发送出去后在关闭socket)？
        //  struct linger {
        //  u_short l_onoff; 非零延迟关闭 
        //  u_short l_linger; 延迟时间
        //  };
        //  linger m_sLinger;
        //  m_sLinger.l_onoff=1;//(在closesocket()调用,但是还有数据没发送完毕的时候容许逗留)
        //  // 如果m_sLinger.l_onoff=0;则功能和2.)作用相同;
        //  m_sLinger.l_linger=5;//(容许逗留的时间为5秒)
        // setsockopt(s,SOL_SOCKET,SO_LINGER,(const char*)&m_sLinger,sizeof(linger));

        struct linger temp = {1, 1}; //延迟关闭
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)); //不占用PORT
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT); //设置时间信号发送周期

    // epoll 创建事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5); // size参数通知内核调用程序期望的文件描述符的数量
    assert(m_epollfd != -1);

//epoll_ctl 默认LT监听
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    // 在域domain中创建两个类型为type的新套接字，并使用
    //  协议protocol，它们彼此连接，并放置文件
    //  FDS[0]和FDS[1]中的它们的描述符。如果PROTOCOL为零，
    //  将自动选择一个。成功时返回0，错误时返回-1。
    // socketpair确是创建了一个文件，在读写的时候是读写的文件映射到内存上的缓冲页面
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd); //创建信号监听管道
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);//将管道写端设为非阻塞
    utils.addfd(m_epollfd, m_pipefd[0], false, 0); //将管道读端注册到监听树 当信号处理函数写入时处理

    
    // 当服务器close一个连接时，若client端接着发数据。根据TCP协议的规定，
    // 会收到一个RST响应，client再往这个服务器发送数据时，系统会发出一个SIGPIPE信号给进程，
    // 告诉进程这个连接已经断开了，不要再写了。不想客户端退出可以把SIGPIPE设为SIG_IGN 这时SIGPIPE交给了系统处理。
    utils.addsig(SIGPIPE, SIG_IGN); //RET响应信号 不关闭客户端交给系统处理
    utils.addsig(SIGALRM, utils.sig_handler, false); // 闹钟 默认当被终止时不会重启信号处理函数
    utils.addsig(SIGTERM, utils.sig_handler, false); // 终止请求

    alarm(TIMESLOT); //注册进程时间
    // 工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::thread_pool()
{ //创建线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::timer(int connfd, struct sockaddr_in client_addr)
{
    //初始化连接的http处理类
    users[connfd].init(connfd, client_addr, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWold, m_databaseName);

    // 初始花clint_data
    // 创建定时器 设置回调函数和超时时间 绑定用户数据将定时器设定链表中
    users_timer[connfd].address = client_addr; //用户的地址
    users_timer[connfd].sockfd = connfd; //用户的inode

    util_timer *timer = new util_timer; //时间处理类
    timer->user_data = &users_timer[connfd]; //用户信息
    timer->cb_func = cb_func; //设置回调函数

    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT; //将当前时间延迟3*最小超时单位
    users_timer[connfd].timer = timer;
    utils.m_timer_list.add_timer(timer); //加入时间处理队列
}

// 若有数据传输，则将定时器往后延迟3个单位
// 并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT; //增加3倍最小超时时间
    utils.m_timer_list.adjust_timer(timer);
    LOG_INFO("%s", "adjust_timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{   
    assert(timer != nullptr);
    timer->cb_func(&users_timer[sockfd]); //调用回调函数处理超时时间
    if (timer) //timer不为NULL
    {
        utils.m_timer_list.add_timer(timer); //添加进入队列
    }
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclinetdata()
{ //处理连接
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address); //
    if (0 == m_LISTENTrigmode) //LT 触发
    {//获取连接
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:erron is: %d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD) //连接池满了
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address); //加入信号处理队列
    }
    else // ET模式
    {
        while (1) //循环处理连接或返回错误
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is: %s", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024]; //信号数组

    ret = recv(m_pipefd[0], signals, sizeof(signals), 0); //从管道中读信号然后分类处理
    if (-1 == ret)
    {
        return false;
    }
    else if (0 == ret)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; i++)
        {
            switch (signals[i])
            {
            case SIGALRM: // 超时
            {
                timeout = true;
                break;
            }
            case SIGTERM: //进程发送终止信号情报
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd) //处理读事件
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor Reactor: 采用同步IO,用户层执行IO操作时,处于挂起状态,等待内核层完成
    if (1 == m_actormodel)
    {
        if (timer) // 设置了定时器
        {
            adjust_timer(timer); //调整定时
        }
        // 若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0); //立即加入线程池的处理队列 读事件
        while (true) // 阻塞等待处理
        {
            if (1 == users[sockfd].improv) //处理了事件
            {
                if (1 == users[sockfd].timer_falg) //超时了
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_falg = 0; //将http处理队列的定时取消
                }
                users[sockfd].improv = 0; //设为没有处理
                break; //退出等待
            }
        }
    }
    else
    {
        // Proactor:采用异步IO,用户层执行IO操作时,可以一边等待内核操作IO,一边自己去处理其他事情...
        if (users[sockfd].read_once()) //将connfd的消息读到缓冲并不着急处理
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            // 若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd); //加入队列等待线程调度并返回不会循环等待任务处理
            if (timer)
            {
                adjust_timer(timer); //调整定时
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd) //处理写事件
{
    util_timer *timer = users_timer[sockfd].timer;

    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1); //加入任务队列 写事件
        while (true) //循环等待
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_falg)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_falg = 0;
                }
                users[sockfd].improv = 0; //置0当处理连接置1
                break;
            }
        }
    }
    else
    {
        if (users[sockfd].write()) //发送消息
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop() //循环处理epoll_wait消息
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1); //阻塞等待
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclinetdata();
                if (false == flag)
                {
                    continue;
                }
            } 
            //對端正常斷開連接 EPOLLIN 2.6.17 版本內核中增加了 EPOLLRDHUP 事件 有了這個事件，對端斷開連接的異常就可以在底層進行處理了，不用再移交到上層
            //EPOLLHUP 表示读写都关闭。本端调用shutdown(SHUT_RDWR)。 不能是close，close 之后，文件描述符已经失效。这个应该是本端（server端）出错才触发的
            //server 读/写 关闭
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务端关闭连接 移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            else if ((sockfd == m_pipefd[0])  && (events[i].events & EPOLLIN))
            {
                // 处理信号
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                {
                    LOG_ERROR("%s", "dealclientdata failure");
                }
            }
            else if (events[i].events & EPOLLIN) // 处理客户连接上收到的数据
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "trmer tick");
            timeout = false;
        }
    }
}
