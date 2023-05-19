#ifndef LOCK_H
#define LOCK_H
#include <pthread.h>
#include <exception>
#include <semaphore.h>

class sem
{
public:
    //构造函数
    sem()
    {
        //信号量初始
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }

    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }

    bool wait()
    {
        //如果信号量的值大于零，则递减继续，函数立即返回
        return sem_wait(&m_sem) == 0; //等待信号
    }

    bool post()
    {
        //如果信号量的值因此变得大于零，那么在sem_wait（3）调用中被阻塞的另一个进程或线程将被唤醒并继续锁定信号量 
        return sem_post(&m_sem) == 0; //发送信号
    }

    //析沟函数
    ~sem()
    {
        sem_destroy(&m_sem);
    }

private:
    sem_t m_sem; //信号量
};

class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0; 
    }

    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex; //锁
};

class cond
{

public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *pm_mutex)
    {
        int ret = 0;
        //pthread_mutex_lock(pm_mutex);
        ret = pthread_cond_wait(&m_cond, pm_mutex);
        //pthread_mutex_unlock(pm_mutex);
        return ret == 0;
    }

    bool timewait(pthread_mutex_t *pm_mutex, struct timespec t)
    {
        int ret = 0;
        //pthread_mutex_lock(pm_mutex);
        ret = pthread_cond_timedwait(&m_cond, pm_mutex, &t);
        //pthread_mutex_unlock(pm_mutex);
        return ret == 0;
    }

    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0; //发送一个信号
    }

    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0; // 唤醒所有信号
    }

private:
//static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond; //条件变量
};

#endif