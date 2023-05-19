#ifndef LOG_H
#define LOG_H


#include <iostream>
#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

// stdarg.h 头文件定义了一个变量类型 va_list 和三个宏，
//这三个宏可用于在参数个数未知（即参数个数可变）时获取函数中的参数

using namespace std;

class Log //单例模式
{
public:
    //c++11 后使用局部变量不用加锁
    static Log* get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void* flush_log_thread(void* arg) //线程函数
    {
        Log::get_instance()->async_log_write();
    }

    //可选择的日志文件，日志缓冲区大小，最大行数，最长日志队列
    bool init(const char* file_name, int close_log, int log_buf_size = 8192, 
                                int split_line = 5000000, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);
    void flush(void);

private:

    Log();
    virtual ~Log();
    void* async_log_write() //异步写日志
    {
        string sigle_log; 
        //从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(sigle_log))
        {
            m_mutex.lock();
            fputs(sigle_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; //路经名
    char log_name[128]; //日志名
    int m_split_lines; //日志最大行数
    int m_log_buf_size; //日志缓冲大小
    long long m_count; //日志行数记录
    int m_today; //因为按天记录，记录当天是什么时候
    FILE* m_fp; //打开log文件的指针
    char* m_buf; //数据缓冲区
    block_queue<string>* m_log_queue; // 阻塞队列
    bool m_is_async; //是否是同步
    locker m_mutex; //锁对象
    int m_close_log; //关闭日志文件
};

//C 语言中 VA_ARGS 是一个可变参数的宏，是新的 C99 规范中新增的，需要配合 define 使用，总体来说就是将左边宏中 … 的内容原样抄写在右边 VA_ARGS 所在的位置;
// 如果可变参数被忽略或为空，## 操作将使预处理器（preprocessor）去除掉它前面的那个逗号.
// 如果你在宏调用时，确实提供了一些可变参数，GNU CPP 也会工作正常，它会把这些可变参数放到逗号的后面。
#define LOG_DEBUG(format, ...) if(0 == m_close_log) { Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if (0 == m_close_log) { Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if (0 == m_close_log) { Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if (0 == m_close_log) { Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif
