半同步/半反应堆线程池
===============
使用一个工作队列完全解除了主线程和工作线程的耦合关系：主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。
> * 同步I/O模拟proactor模式
> * 半同步/半反应堆
> * 线程

========================
template <class T>
class threadpool {...}; :模板线程池类，支持半同步/半反应堆

* epoll普通模型：epoll_create() --- epfd 红黑树 --- 将connfd 添加到 epfd ， epoll_ctl() --  EPOLLIN 事件 ---- epoll_wait 设置监听----- 满足条件 ---- epoll_wait 返回 ---- 数组.data.fd == connfd --- read() --- 小->大 --- write()

* epoll反应堆模型： epoll_create() --- epfd 红黑树 --- 将connfd 添加到 epfd ， epoll_ctl() --  EPOLLIN 事件 ---- epoll_wait 设置监听—— 满足条件 ---- epoll_wait 返回 ---- 数组.data.fd == connfd --- read() ---- epoll_ctl --- 将connfd从红黑树---- 修改connfd监听事件 EPOLLOUT --- 添加到epfd 树 调用epoll_wait监听 写事件----epoll_wait返回说明 connfd 可写 ---- write() ———  将connfd从红黑树摘下 ---- 修改connfd监听事件 EPOLLIN(回调函数)—— 添加到epfd 树 调用epoll_wait监听 读事件 

===========================
* 数据成员:
int m_thread_number  线程池中的线程数

int m_max_requests 请求队列中允许最大请求线程数

pthread_t *m_threads 描述线程数组， 其大小为m_thread_number

std::list<T *> m_workqueue 请求队列

locker m_queueloker  请求锁

sem m_queuestat 信号量

int m_actor_model 切换模型


* 构造函数:
 threadpool(int actor_moder, connection_pool *connPool, int thread_number = 8, int max_request = 10000)
初始列表初始m_actor_model，m_thread_number，m_max_requests，m_threads，m_connPool; 当线程池数量||最大小于线程数 <= 0时抛出异常，申请线程数组，当m_threads为NULL时std::exception(), 循环创建线程并将线程detach()，将成员函数worker作为工作函数，将this传入参数

~threadpool():
释放m_threads

成员函数:
bool append(T *request, int state)
加锁，如果工作队列满了，解锁并返回false; 否则将requset->m_state 设为参数 state，将request加入m_workqueue，解锁并发送工作信号

bool append_p(T *request)
与append(...)成员函数一样只是不设置状态

static void *worker(void *arg)
静态成员函数，将参数转换为threadpool类指针并调用run()成员函数，返回转化的指针

void run():
工作队列处理函数,死循环处理任务，调用wait()等待工作队列信号，加锁，对队列判空防止；处理错误,request获取任务头，解锁;当1 == m_actor_model反应堆处理，0 == request->m_state处理读函数，否则处理写函数， m_actor_model为其他直接处理请求头和发送头













