#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000; // 最大事件数
const int TIMESLOT = 5;             // 最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    // 初始化
    void init(int port, string user, string passWord, string databaseName, int log_write,
              int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_modle);

    void trig_mode();   // 触发模式
    void log_write();   // 设置日志
    void sql_pool();    // 初始化数据库链接池
    void thread_pool(); // 线程池
    void eventListen(); // 事件监听
    void timer(int connfd, struct sockaddr_in client_addr); //设置连接时间
    void adjust_timer(util_timer *timer);                  // 调整时间
    void deal_timer(util_timer *timer, int sockfd);        // 超时
    bool dealclinetdata();                                 // 处理连接事件
    bool dealwithsignal(bool &timeout, bool &stop_server); // 处理信号事件
    void dealwithread(int sockfd); //处理读事件
    void dealwithwrite(int sockfd); //处理写事件
    void eventLoop(); // 事件循环

public:
    // 基础
    int m_port;       // 端口
    char *m_root;     // root文件名
    int m_log_write;  // 写日志
    int m_close_log;  // 关闭日志
    int m_actormodel; // 模型

    int m_pipefd[2];  // 管道文件描述符处理信号将信号写入管道
    int m_epollfd;    // epoll 模型树的inode
    http_conn *users; // http连接类处理http请求

    // 数据库相关
    connection_pool *m_connPool; // 数据库连接池
    string m_user;               // 登陆数据库用户名
    string m_passWold;           // 登陆数据库密码
    string m_databaseName;       // 数据库名称
    int m_sql_num;               // 数据库连接数量

    // 线程池相关
    threadpool<http_conn> *m_pool; // http请求线程池
    int m_thread_num;

    // epoll event 相关
    epoll_event events[MAX_EVENT_NUMBER]; //epoll_wait的返回事件

    int m_listenfd;       // 监听inode
    int m_OPT_LINGER;     // 选择逗留时间 setsockopt是否占用PORT
    int m_TRIGMode;       // 触发模式ET/LT
    int m_LISTENTrigmode; // listenfd触发器
    int m_CONNTrigmode;   // connfd触发模式

    // 定时器
    clinet_data *users_timer; // 客户端信息数组
    Utils utils;              // 时间定时队列
};