//数据库连接池
#ifndef _CONNECTION_POOL_H_
#define _CONNECTION_POOL_H_
#include <stdio.h>
#include <mysql/mysql.h>
#include <list>
#include <error.h>
#include <iostream>
#include <string>
#include <string.h>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

//连接池
class connection_pool
{
public:
    MYSQL* GetConnention(); //获取数据库连接
    bool ReleaseConnection(MYSQL* con); //释放连接
    int GetFreeConn(); //获取连接
    void DestroyPool(); //销毁所有连接

    //单例模式
    static connection_pool* GetInstance();

    void init(string url, string User, string Password, string DatabaseName, int Port, int MaxCon, int close_log);

private:
    connection_pool(); //构造函数
    ~connection_pool(); //析沟函数

    int m_MaxConn; //最大连接数
    int m_Curconn; //当前连接数
    int m_FreeConn; //空闲的连接
    locker lock; //🔓
    list<MYSQL*> connList; //连接池
    sem reserve; // 信号量
public:
    std::string m_url; //主机地址
    std::string m_Port; //数据库端口号
    std::string m_User; //数据库用户名
    std::string m_PassWord; //数据库密码
    std::string m_DatabaseName; //数据库名
    int m_close_log; //日志开关
};

//利用构造，析构自动销毁 管理资源
class connectionRAII
{
public:
    connectionRAII(MYSQL** con, connection_pool* pool);
    ~connectionRAII();

private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
};

#endif