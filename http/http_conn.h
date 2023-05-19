#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"
#include "../timer/lst_timer.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200; //文件名长度
    static const int READ_BUFER_SIZE = 2048; //读缓冲区大小
    static const int WRITE_BUFER_SIZE = 1024; //写缓存大小

    enum METHOD //http 协议请求
    {
        GET = 0, // 请求指定的页面信息，并返回实体主体。
        POST,   //请求服务器接受所指定的文档作为对所标识的URI的新的从属实体
        HEAD,   //只请求页面的首部
        PUT,    //从客户端向服务器传送的数据取代指定的文档的内容
        DELETE, //请求服务器删除指定的页面
        TRACE, 
        OPTIONS, //允许客户端查看服务器的性能
        CONNECT,
        PATH
    };

    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, //请求行
        CHECK_STATE_HEADER, //请求头
        CHECK_STATE_CONTENT //CONTENT 内容
    };

    enum HTTP_CODE
    {
        NO_REQUEST, //无请求
        GET_REQUEST, //获得请求
        BAD_REQUEST, //错误请求
        NO_RESOURCE, //无资源
        FORBIDDEN_REQUEST, //被禁止的请求
        FILE_REQUEST, //文件请求
        INTERNAL_ERROR, //内部错误
        CLOSE_CONNECTION //关闭连接
    };

    enum LINE_STATUS
    {
        LINE_OK = 0, //连接状态
        LINE_BAD,
        LINE_OPEN
    };
public:
    http_conn(){};
    ~http_conn(){};

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    void process(); //
    bool read_once(); //处理读缓冲
    bool write(); //写缓冲

    sockaddr_in *get_address()
    {
        return &m_address;
    }

    void initmysql_result(connection_pool *connPool); //数据库连接池获取结果
    int timer_falg; //是否超时
    int improv; //是否处理了事件

private:
    void init();
    HTTP_CODE process_read(); //读线程 
    bool process_write(HTTP_CODE ret); //写线程
    HTTP_CODE parse_request_line(char *text); //解析请求行
    HTTP_CODE parse_headers(char *text); //解析头内容
    HTTP_CODE parse_content(char *text); //判断http请求是否被完整读入
    HTTP_CODE do_request();
    char *get_line()
    {
        return m_read_buf + m_start_line;
    }
    LINE_STATUS parse_line(); //解析行
    void unmap();

    //处理写缓冲
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_liger();
    bool add_blank_line();


public:
    static int m_epollfd; //epoll 文件描述符（根节点）
    static int m_user_count; //用户链接数量
    MYSQL *mysql;
    int m_state; // 0 读 1写
private:
    int m_sockfd; //网络描述
    sockaddr_in m_address; //连接地址
    char m_read_buf[READ_BUFER_SIZE]; //读缓存
    long m_read_idx; //读下标
    long m_checked_idx; // 选择下标
    int  m_start_line; //开始行
    char m_write_buf[WRITE_BUFER_SIZE]; //写缓存
    long m_write_idx; //写下标
    CHECK_STATE m_check_state; //选择状态
    METHOD m_method; //请求方法
    char m_real_file[FILENAME_LEN]; // 文件
    char *m_url; //url 127.0.0.01：9006
    char *m_version; //版本 HTTP 1.1
    char *m_host; //主机号 
    long m_content_length; //内容长度
    bool m_linger; //徘徊 循环
    char *m_file_address; //文件地址
    struct stat m_file_stat; //文件信息
    iovec m_iv[2]; // I/O vector，与readv和wirtev操作相关的结构体
    // m_iv[0]: 指向read/write地址,m_iv[1]锁指向内存大小
    int m_iv_count; //io 数量
    int cgi; //是否启用post
    char *m_string; //存储请求头数据
    int bytes_to_send; //发送数据长度
    int bytes_have_send; //剩余的
    char *doc_root; //文档根 

    map<string, string> m_users; //用户
    int m_TRIGMode; //触发模式
    int m_close_log; //关闭日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];  
};






