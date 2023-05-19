#include "config.h"

Config::Config()
{
    // 端口号
    PORT = 9007;

    // 日志写入方式 默认同步
    LOGWrite = 0;

    // 触发组合 默认listent LT + connfd LT
    TRIGMode = 0;

    // listenfd 触发模式 LT
    LISTENTrigMode = 0;

    // connfd 触发模式 LT
    CONNTrigMode = 0;

    // 优雅关闭 默认不使用
    OPT_LINGER = 0;

    // 数据库数量
    //sql_num = 8;
    sql_num = 8;
    // 线程数量
    //thread_num = 8;
    thread_num = 8;
    // 是否关闭日志 不关闭
    close_log = 0;

    // 并发模型选择 proactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char *argv[])
{

    int opt = 0;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOGWrite = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
        case 'a':
        {
            actor_model = atoi(optarg);
            break;
        }
        default:
        {
            break;
        }
        }
    }
}