同步/异步日志系统
===============
同步/异步日志系统主要涉及了两个模块，一个是日志模块，一个是阻塞队列模块,其中加入阻塞队列模块主要是解决异步写入日志做准备.
> * 自定义阻塞队列
> * 单例模式创建日志
> * 同步日志
> * 异步日志
> * 实现按天、超行分类
================================
> * block_queue 队列模板类
成员:
    locker m_mutex 锁
    cond m_cond 信号量
    T *m_array 模板指针做数组 
    int m_size 数组元素个数
    int m_max_size 数组最大元素个数
    int m_front 数组头下标
    int m_back 数组尾下标
运用循环数组
函数:
* block_queue(int max_size = 1000)
初始化数组最大原素，创建数组，设置头尾 -1

* void clear()
锁机制将元素个数设置0 头尾 -1

* ~block_queue()
锁机制 delete数组

* bool full()
锁机制 通过元素个数与最大元素个数比较

* bool empty() 
判断m_size是否为0

* bool front(T &value) 
锁机制用引用传出头元素 需要判空return false

* bool back(T &value)
锁机制 m_back返回队尾元素

* int size()
返回m_size

* int max_size()
返回m_max_size

* bool push(const T &item)
生产者消费者模型 往队列添加元素，当队列满需要将所有使用队列的线程先唤醒, 循环队列m_back = (m_back + 1) % m_max_size向数组添加元素，添加后唤醒阻塞线程

* bool pop(T &item)
当数组元素个数为0时调用m_cond.wait(m_mutex.get())等待生产者的m_cond.broadcast()信号，否则通过m_front = (m_front + 1) % m_max_size取出元素头

* bool pop(T &item, int ms_timeout)
在bool pop(T &item)的基础上增加了当产品为空时的等待超时处理 m_cond.timewait(m_mutex.get(), t)

> * Log日志类 运用单例模式
* 成员: 
char dir_name[128]; //路经名
char log_name[128]; //日志名
int m_split_lines; //日志最大行数 
int m_log_buf_size; //日志缓冲大小
long long m_count; //日志行数记录
int m_today; //因为按天记录，记录当天是什么时候
FILE* m_fp; //打开log文件的指针
char* m_buf; //数据缓冲区
block_queue<string>* m_log_queue; // 数据队列指针存储日志内容(与同步异步有关)
bool m_is_async; //是否是同步 写入日志
locker m_mutex; //锁对象
int m_close_log; //关闭日志文件

* 成员函数:
* static Log* get_instance()
静态函数返回Log类实例

* static void* flush_log_thread(void* arg)
线程回调函数，调用成员函数async_log_write（）异步写日志
* void* async_log_write()
从m_log_queue队列中获取日志内容写入m_fp文件中

* Log()
设置日志行数0，异步写关闭

* ~Log()
关闭文件描述符并释放资源

* bool init(const char* file_name, int close_log, int log_buf_size = 8192, int split_line = 5000000, int max_queue_size = 0);

初始化日志类，当 max_queue_size >= 1时异步设置日志并创建线程将 flush_log_thread函数设为线程回调函数（成员函数）不传参，设置日志开启标志，日志缓存大小，申请缓存空间，设置日志最大行数，通过time() 函数获取当前时间，

* void write_log(int level, const char* format, ...)
写日志通过 gettimeofday()函数获得当前的时间， 根据参数level选择日志类型,加锁 加日志行数当时间改变或者日志满了创建新的日志文件，解锁; 运用c的va_list参数列实现日志内容的变参，如果是异步写入将日志内容加入队列否则写入m_fp文件

* void flush()
加锁 强制刷新写入流缓冲区