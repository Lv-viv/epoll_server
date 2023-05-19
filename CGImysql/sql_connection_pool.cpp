#include <mysql/mysql.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <list>
#include <stdio.h>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    m_FreeConn = 0; // 空闲连接
    m_Curconn = 0;  // 已有连接
    mysql_library_init(0,0,0); //初始化mysql
}

connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string Password, string DatabaseName, int Port, int MaxCon, int close_log)
{ // 构造初始化 MYSQL线程池没有使用MYSQL获取数据
    m_url = url;
    m_User = User;
    m_PassWord = Password;
    m_DatabaseName = DatabaseName;
    m_Port = Port;
    m_close_log = close_log;

    for (int i = 0; i < MaxCon; ++i)
    {
        MYSQL *con = NULL;
        con = mysql_init(con); // 数据库初始化
        if (con == NULL)
        {
            LOG_ERROR("MySQL init Error "); // 日志错误
            exit(1);
        }

        con = mysql_real_connect(con, m_url.c_str(), m_User.c_str(), m_PassWord.c_str(), m_DatabaseName.c_str(), Port, NULL, 0);
        if (con == NULL)
        {
            string err_info(mysql_error(con)); // 错误信息
            err_info = (string("[mysql_real_connect error erron=") +
                        std::to_string(mysql_errno(con)) + string("]: ") + err_info);
            LOG_ERROR(err_info.c_str());
            exit(1);
        }

        // 更新连接池和空闲连接数量
        connList.push_back(con);
        ++m_FreeConn;
    }

    // 将信号量初始化为最大的连接数
    reserve = sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL* connection_pool::GetConnention()
{
    MYSQL* con = NULL;

    if (0 == connList.size())
    {
        return NULL;
    }

    reserve.wait(); //等待post释放信号

    lock.lock();
    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_Curconn;

    lock.unlock();

    return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL* con)
{
    if (NULL == con)
    {
        return false;
    }

    lock.lock();
    connList.push_back(con);
    ++m_FreeConn;
    --m_Curconn;
    lock.unlock();


    reserve.post(); //发送信号唤醒wait线程
    return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock.lock();

    if (connList.size() > 0)
    {
        list<MYSQL*>::iterator it;

        for(it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL* con = *it;
            mysql_close(con);

        }
        m_Curconn = 0;
        m_FreeConn = 0;
        connList.clear();
        mysql_library_end();
    }

    lock.unlock();
}
//当前空闲的句柄数
int connection_pool::GetFreeConn()
{
    return m_FreeConn;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool)
{
    *SQL = connPool->GetConnention();
    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}


