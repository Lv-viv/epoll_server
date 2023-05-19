> *数据库连接池
* connection_pool(): 初始化Mysql库,设置空闲连接为0, 已有连接为0
* GetInstance(): 单例模式获得静态对象
* init（string url, string User, string Password, string DatabaseName, int Port, int MaxCon, int close_log）: 初始化主机地址，用户名，密码，数据库名称，端空，是否关闭日志，根据MaxCon循环初始化连接并获取结果集，将初始化的MYSQL*con加入链表增加空闲连接，并将信号初始化为空闲连接数，最大连接数为空闲连接数
* GetConnention():加锁从连接池中获取mysql句柄，调用wait()当连接数量大于最大句柄数时等待释放信号，空闲连接数减一，已用的连接增加
* ReleaseConnection(MYSQL* con):加锁向连接池中添加句柄并发送释放信号增加空闲数，减少已用的句柄

* DestroyPool(): 通过迭代器释放数据库句柄，并将空闲句柄和已有句柄数置为0

* GetFreeConn(): 获得空闲句柄数
* ~connection_pool(): 调用DestroyPoll()释放句柄池

> *RAII管理单元
connectionRAII 数据库连接池RAII管理类
* connectionRAII(MYSQL** SQL, connection_pool* connPool):通过二级指针将connPool中的句柄传出并复制给RAII管理类， connPool由于单例模式给管理类成员poolRAII
* ~connectionRAII():通过poolRAII回收connRAII


