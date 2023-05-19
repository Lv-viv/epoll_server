#ifndef _LST_TIMER_H_
#define _LST_TIMER_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer; // 声明

struct clinet_data
{                        // 客户端信息
    sockaddr_in address; // 客户端地址
    int sockfd;          // 文件描述符
    util_timer *timer;
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire; // 时间

    void (*cb_func)(clinet_data *); // 回调函数
    clinet_data *user_data;         // 用户数据
    util_timer *prev;
    util_timer *next;
};

// timer 连表
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer); // 调整链表
    void del_timer(util_timer *timer);    // 删除timer
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head); // 向timer列表添加timer

    util_timer *head;
    util_timer *tail;
};

class Utils // 工具类
{
public:
    Utils(){};
    ~Utils(){};

    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd; // 管道id
    sort_timer_lst m_timer_list;
    static int u_epollfd; // epoll 文件描述符
    int m_TIMESLOT; //触发模式
};
void cb_func(clinet_data *user_data); // 回调函数
#endif