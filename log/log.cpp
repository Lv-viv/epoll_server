#include <string.h>
#include <string>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{
    if (m_fp != nullptr)
    {
        fclose(m_fp);
    }
    if (m_buf != nullptr)
    {
        delete[] m_buf;
    }
    if (m_log_queue != nullptr)
    {
        delete m_log_queue;
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char* file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        if (-1 == pthread_create(&tid, NULL, flush_log_thread, NULL))
        {
            perror("Log::init pthread_create error");
        }
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size); //置0
    m_split_lines = split_lines;

     //此函数会返回从公元1970年1月1日的UTC时间从0时0分0秒算起到现在所经过的秒数
     //(即格林尼治时间1970年1月1日00:00:00到当前时刻的时长，时长单位是秒)。
     //如果t并非空指针的话，此函数也会将返回值存在t指针所指的内存
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t); //localtime是 把从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为本地时间
    struct tm my_tm = *sys_tm;
    
    const char* p = strrchr(file_name, '/'); //在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。
        // ./Server -> /
    
    char log_full_name[256] = {0}; //

    if (NULL == p)
    { //file_name = Server --> p = NULL
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {   //log_name = Server
        strcpy(log_name, p + 1); //日志名
        strncpy(dir_name, file_name, p - file_name + 1); //dir_name = ./ , p - file_name + 1 = 2
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
        // ./+日期 + Server
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a"); //打开文件并将文件指针复制给m_fp
    if (m_fp == NULL)
    {
        return false;
    }
    return true;
}


void Log::write_log(int level, const char* format, ...)
{
    struct timeval now = {0, 0}; //
    gettimeofday(&now, NULL); //gettimeofday是计算机函数，使用C语言编写程序需要获得当前精确时间（1970年1月1日到现在的时间），或者为执行计时，可以使用gettimeofday()函数
    
    time_t t = now.tv_sec; //计算机执行秒
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    //事务的分类
    switch (level)
    {
    case 0: 
        strcpy(s, "[debug]:");
        break;;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
    }

    //写一个日志对m_cout + 1, m_split_lines 最大行数
    m_mutex.lock();
    m_count++;

    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //时间改变了|| 日志满了
    {
        char new_log[256] = {0}; //新的日志路径
        fflush(m_fp); //刷新缓冲区将缓冲区的内容写入文件
        fclose(m_fp); //关闭文件

        char tail[16] = {0}; //临时填充标题
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday); //将日期写入

        if (m_today != my_tm.tm_mday)
        { //日期变了
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }   
        else
        {//日志满了
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count/m_split_lines); //将日志编号写入文件名
        }

        m_fp = fopen(new_log, "a"); //创建文件
    }

    m_mutex.unlock();


    va_list valst; //参数列
    va_start(valst, format); //初始化 展开可变参数
    string log_str;
    
    m_mutex.lock();

    //写入的具体时间内容格式 若成功则返回预写入的字符串长度
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                        my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, 
                        my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    //2023-05-04 21:18:01.623401 [info]:

    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
    // const char *format [in], 指定输出格式的字符串，它决定了你需要提供的可变参数的类型、个数和顺序。
    // va_list ap [in], va_list变量. va:variable-argument:可变参数
    // 2023-05-04 22:31:08.737829 [info]: trmer tick

    m_buf[n + m] = '\n'; // 2023-05-04 22:31:08.737829 [info]: trmer tick\n\0
    m_buf[m + n + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    //m_is_async 设置且队列不为空
    if (m_is_async && !m_log_queue->full())
    { //异步
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp); //写入文件
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush()
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}