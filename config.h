#ifndef CONNFIG_H
#define CONNFIG_H

#include "webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config() {}

    void parse_arg(int argc, char *argv[]);

    // 端口号
    int PORT;

    // 日志写入方式
    int LOGWrite;

    // 触发组合
    int TRIGMode;

    // listenfd 触发模式
    int LISTENTrigMode;

    // connfd 触发模式
    int CONNTrigMode;

    // 优雅关闭
    int OPT_LINGER;

    // 数据库数量
    int sql_num;

    // 线程数量
    int thread_num;

    // 是否关闭日志
    int close_log;

    // 并发模型选择
    int actor_model;
};

#endif