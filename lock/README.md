线程同步机制包装类
===============
多线程同步，确保任一时刻只能有一个线程能进入关键代码段.
> * 信号量
> * 互斥锁
> * 条件变量

=====================================
> * class sem 信号类
* sem():
* sem(int num):初始化信号量
 int sem_init(sem_t *sem, int pshared, unsigned int value);
如果pshared的值为0，那么信号量在进程的线程之间共享，并且应该位于对所有线程都可见的某个地址 
value参数指定信号量的初始值 
* wait(): 等待信号
* post(): 发送信号
* ~sem(): 释放信号量

====================================

> * locker 锁类
* locker():初始化临界值
* ~locker(): 释放临界值
* lock(): 加锁
* unlock(): 解锁
* get(): 获取临界值

====================================
> * cond 条件变量类
* cond(): pthread_cond_init() 初始化条件变量
* ~cond(): pthread_cond_destroy()
* wait(pthread_mutex_t *pm_mutex): 
等待条件
* bool timewait(pthread_mutex_t *pm_mutex, struct timespec t):
等待一段事件,当超时发送信号 ETIMEDOUT:直到abstime指定的超时时间，条件变量才发出信号;EINTR:pthread_cond_timedwait被一个信号中断
* signal(): 发送一个信号
* pthread_cond_broadcast(): 唤醒所以的等待