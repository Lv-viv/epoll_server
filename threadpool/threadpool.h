#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <class T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_moder, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();

    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;         // 线程池中的线程数
    int m_max_requests;          // 请求队列中允许最大请求线程数
    pthread_t *m_threads;        // 描述线程数组， 其大小为m_thread_number
    std::list<T *> m_workqueue;  // 请求队列
    locker m_queueloker;         // 保护请求队列
    sem m_queuestat;             // 是否有任务需要处理
    connection_pool *m_connPool; // 数据库
    int m_actor_model;           // 并发模型选择(由 参数传入默认为0)
};

template <class T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests)
    : m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL), m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        std::exception();

    m_threads = new pthread_t[thread_number]; // 申请线程数组
    if (!m_threads)                           // 申请空间错误
    {
        std::exception();
    }

    for (int i = 0; i < thread_number; i++)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) // 线程分离
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <class T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <class T>
bool threadpool<T>::append(T *request, int state)
{
    m_queueloker.lock();

    if (m_workqueue.size() >= m_max_requests) // 线程队列满了
    {
        m_queueloker.unlock();
        return false;
    }

    request->m_state = state;       // 设置请求状态
    m_workqueue.push_back(request); // 添加到工作数组
    m_queueloker.unlock();
    m_queuestat.post(); // 发送信号有工作要处理
    return true;
}

template <class T>
bool threadpool<T>::append_p(T *request)
{
    m_queueloker.lock();

    if (m_workqueue.size() >= m_max_requests)
    {
        m_queueloker.unlock();
        return false;
    }

    m_workqueue.push_back(request); // 工作队列
    m_queueloker.unlock();
    m_queuestat.post();
    return true;
}

template <class T>
void *threadpool<T>::worker(void *arg)
{ //工作线程 this
    threadpool *pool = static_cast<threadpool *>(arg);
    pool->run();
    return pool;
}

template <class T>
void threadpool<T>::run()
{ //线程运行实例
    while (true)
    {
        m_queuestat.wait(); // 等待队列添加请求信号
        m_queueloker.lock();

        if (m_workqueue.empty()) // 工作队列为空
        {
            m_queueloker.unlock();
            continue;
        }

        T *request = m_workqueue.front(); // 工作队列头
        m_workqueue.pop_front();

        m_queueloker.unlock();

        if (!request)
        {
            continue;
        }

        if (1 == m_actor_model) //同步
        { // 线程池状态
            if (0 == request->m_state)
            {                             // 请求队列状态
                if (request->read_once()) // 读缓冲区数据
                {
                    request->improv = 1; //已经处理请求
                    connectionRAII mysqlcon(&request->mysql, m_connPool); // 资源管理类从数据库连接池获取连接
                    request->process(); //处理读和写数据头缓冲区
                }
                else
                {
                    request->improv = 1;
                    request->timer_falg = 1; //标志关闭文件描述符
                }
            }
            else
            {
                if (request->write()) //发送返回信息
                {
                    request->improv = 1; //完成请求
                }
                else
                {
                    request->improv = 1; 
                    request->timer_falg = 1; //移除定时队列
                }
            }
        }
        else //异步
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process(); //只处理缓冲区不发送或接受信息
        }
    }
}

#endif
