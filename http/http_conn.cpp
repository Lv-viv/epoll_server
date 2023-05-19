#include "http_conn.h"
#include <mysql/mysql.h>
#include <fstream>
//  请求行+请求头+请求体
// 定义一些http状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
// 您的请求语法不正确，或者本质上不可能稳定。
const char *error_400_from = "Your request has bad syntax or is inherently impossible to staisfy.\n";

const char *error_403_title = "Forbidden";
// 您没有从该服务器获取文件的权限
const char *error_403_from = "Your do not have permission to get file from this server.\n";

const char *error_404_title = "Not Found";
const char *error_404_from = "The requested file not found on this server.\n";

const char *error_500_title = "Internal error";
const char *error_500_from = "There was an unusual problmer serving the request file.\n";

// 锁
locker m_lock;
map<string, string> users; // 用户名 密码

// 数据库获取结果
void http_conn::initmysql_result(connection_pool *connPool)
{
    MYSQL *mysql = NULL; // 数据库对象
    // 从数据库连接池中获取数据库
    connectionRAII mysqlcon(&mysql, connPool);

    // 在user表中检索 username passwd
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error%s\n", mysql_error(mysql));
    }

    // 检索查询结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    /// 返回查询列数
    int num_fields = mysql_num_fields(result);

    // 返回字段结构
    MYSQL_FIELD *field = mysql_fetch_field(result);

    // 获取字段结构行 将用户名 密码存入map
    while (MYSQL_ROW row = mysql_fetch_row(result)) //if 改为 while
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL); // 获取文件状态 访问模式
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将事件设置 读事件 ET 模式 选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;  // epoll 模型事件
    event.data.fd = fd; // 添加客户端

    if (1 == TRIGMode)
    { // 读 | ET模式 | 流套接字对等关闭连接，或关闭写入连接的一半。
     //（此标志对于编写简单的代码来检测对等设备特别有用使用边缘触发监控时关闭。）
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (one_shot)
    {
        event.events |= EPOLLONESHOT; // 请求关联文件描述符的一次性通知（避免重复触发事件）
    }

    // 注册epoll
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd); // 设置非阻塞
}

// 将inode 从内核监控删除
void removefd(int epollfd, int fd)
{
    // 移除监控
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd); // 关闭文件描述符
}

// 重置EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMod)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMod)
    {
        /*EPOLLSHOT相当于说，某次循环中epoll_wait唤醒该事件fd后，就会从注册中删除该fd,也就是说以后不会epollfd的表格中将不会再有这个fd,也就不会出现多个线程同时处理一个fd的情况*/
        // EPOLLRDHUP 要想知道对端是否关闭
        // 流套接字对等关闭连接，或关闭写入连接的一半(半关闭) | | 请求关联文件描述符的一次性通知
        event.events = ev | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
    }
    else
    {
        event.events = ev | EPOLLONESHOT | EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1; // sock 文件描述符

// 关闭连接
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd); //
        removefd(m_epollfd, m_sockfd); //从树上移除并关闭fd
        m_sockfd = -1;
        m_user_count--;
    }
}

// 初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr,
                    char *root, int TRIGMode, int close_flag, string user, string passwd, string sqlname)
{

    m_sockfd = sockfd; //客户端文件描述符
    m_address = addr; //客户端地址

    addfd(m_epollfd, m_sockfd, true, TRIGMode); // 注册epoll 默认开启EPOLLONESHOT
    m_user_count++; //用户数量++

    // 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root; //拫目录: /home/v/MyItem/MyNetWorkItem/src/http/root
    m_TRIGMode = TRIGMode; //触发模式
    m_close_log = close_flag; //是否关闭日志

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init();
}

void http_conn::init()
{
    // Request URL:http://www.baidu.com/
    //  Remote Address:61.135.169.125:80
    //  Request URL:http://www.baidu.com/
    //  Request Method:GET
    //  Status Code:200 OK
    mysql = NULL;
    bytes_to_send = 0; //要的数据数
    bytes_have_send = 0; //已经发送数据
    m_check_state = CHECK_STATE_REQUESTLINE; // 接收请求
    m_linger = false;                        // 保持连接标志
    m_method = GET;                          // http报头get
    m_url = 0; //url
    m_version = 0; //版本
    m_content_length = 0; //设置请求体的字节长度
    m_host = 0; //主机号
    m_start_line = 0; //需要解析的行
    m_checked_idx = 0; //已经解析的下标
    m_read_idx = 0; //已经读取的下标buf
    m_write_idx = 0; //已经写的下标
    cgi = 0; // 是否启用post
    m_state = 0; //读写状态 0是读状态 1是写状态
    timer_falg = 0;
    improv = 0; // 是否处理完读写标志

    memset(m_read_buf, 0, READ_BUFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFER_SIZE);
    memset(m_real_file, 0, FILENAME_LEN);
}

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() // 解析完一行就返回
{//这段代码的作用是保证HTTP请求报文的正确性和完整性，避免因为解析错误而导致服务器无法正确处理请求
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx]; //
        if (temp == '\r') //\r
        {
            if ((m_checked_idx + 1) == m_read_idx) // 到缓冲区尾 只有\r
            { //请求体结尾为\r
                return LINE_OPEN; 
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n') // \r\n
            { //m_read_buf=GET / HTTP/1.1\r\n -> GET / HTTP/1.1\0\0 
                m_read_buf[m_checked_idx++] = '\0'; // \r -> \0 
                m_read_buf[m_checked_idx++] = '\0'; // \n -> \0
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0'; // \r -> \0
                m_read_buf[m_checked_idx++] = '\0';   // \n -> \0
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx > READ_BUFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0; // 读取的字节数
    // LT读取数据
    if (0 == m_TRIGMode)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFER_SIZE - m_read_idx, 0); //一次读完
        m_read_idx += bytes_read; //增加读字节数

        if (bytes_read <= 0) //读取完了或者对端关闭
        {
            return false; 
        }
        return true;
    }
    else // ET读数据
    {
        while (true) //循环读取
        { // 
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1) // 读错误
            {
                // 再试一次 连续做read操作而没有数据可读 |  操作将会被阻塞，在GNU C的库中，它的另外一个名字是EAGAIN
                if (errno == EAGAIN | errno == EWOULDBLOCK)
                {
                    break;
                }
                return false;
            }
            else if (bytes_read == 0) // 读完了
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

// 解析http请求行，获得请求方法，目标url及http版本号
//  GET /hello.txt HTTP/1.1
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    //text = "POST /1 HTTP/1.1"
    m_url = strpbrk(text, " \t"); //请求报文中第一行中的第一个空格[或]制表符的位置
    //m_url = " /1 HTTP/1.1"
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; // m_url="/1 HTTP/1.1" text = "POST\0" 字符串\0结尾

    char *method = text;//method = POST
    if (strcasecmp(method, "GET") == 0) // strcasecmp用忽略大小写比较字符串.
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1; //加密
    }
    else
    {
        return BAD_REQUEST;
    }
//m_url = "/1 HTTP/1.1" -> "/1"
    m_url += strspn(m_url, " \t"); //返回0('/'的位置) 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标
    m_version = strpbrk(m_url, " \t"); //m_version = " HTTP/1.1" 
    if (!m_version)
    {
        return BAD_REQUEST;
    }

    *m_version++ = '\0'; //m_url = "/1" m_version = "HTTP/1.1"
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) //m_version != HTTP/1.1
    {
        return BAD_REQUEST;
    }

    if (strncasecmp(m_url, "http://", 7) == 0) //如果是 http://....的格式
    {
        m_url += 7; //去除 http://（7字节）
        m_url = strchr(m_url, '/'); // strchr() 用于查找字符串中的一个字符，并返回该字符在字符串中第一次出现的位置。
    }

    if (strncasecmp(m_url, "https://", 8) == 0) //https://...格式
    {
        m_url += 8;
        m_url = strchr(m_url, '/'); // strchr() 用于查找字符串中的一个字符，并返回该字符在字符串中第一次出现的位置。
    }

    if (!m_url || m_url[0] != '/') //m_url为空或不以/开头
    {
        return BAD_REQUEST;
    }

    // 当url为/时，显示判断界面
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "judge.html"); //根路径
    }
    m_check_state = CHECK_STATE_HEADER; //状态转移解析头
    return NO_REQUEST;
}

// 解析http请求的一个头部信息 请求行后一个
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0') //当请求头为空时               error: '\n' 改为 '\0'
    {
        if (m_content_length != 0) // 请求体
        {
            m_check_state = CHECK_STATE_CONTENT; // 状态转移为解析请求体
            return NO_REQUEST;
        }
        return GET_REQUEST; // 获得请求 （返回处理请求）
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) 
    {//Connection: keep-alive  连接状态
        text += 11; //text = " keep-alive"
        text += strspn(text, " \t"); //text = "keep-alive"
        if (strcasecmp(text, "keep-alive") == 0) // 保持连接 
        {
            m_linger = true; //保持连接标志置一
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {//Content-Length: 0  设置请求体的字节长度
        text += 15;//text=" 0"
        text += strspn(text, " \t"); //text="0"
        m_content_length = atol(text); //转为long
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {//Host: 127.0.0.1:9007
        text += 5;//text=" 127.0.0.1:9007"
        text += strspn(text, " \t");//text="127.0.0.1:9007"
        m_host = text;//主机名
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text); //为知头文件
    }
    return NO_REQUEST;
}

// 判断http请求是否被完整读入
//解析请求体
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{//text = "user=q&password=123456"
    if (m_read_idx >= (m_content_length + m_checked_idx)) //(m_content_length + m_checked_idx)请求的长度文本
    {

        text[m_content_length] = '\0'; //text = "user=q&password=123456\0"将text设为字符串
        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//处理读事件
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_state = LINE_OK; //连接状态 与解析请求行有关
    HTTP_CODE ret = NO_REQUEST; //Http 请求状态
    char *text = 0;
    //当 解析状态为：解析请求体并且保持连接 || 解析请求行状态正常
    while ((m_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK) || (line_state = parse_line()) == LINE_OK)
    {
        text = get_line();            // 获取一行数据 "GET / HTTP/1.1"
        m_start_line = m_checked_idx; // 选择行 parse_line改变 m_checked_idx（遇见\r\n返回）m_checked_idx下一行首
        LOG_INFO("%s", text);         // 日志信息 循环写入日志 遇见 \0 结束
        switch (m_check_state)        // 接收请求 -> 请求头 -> 保持连接
        {
        case CHECK_STATE_REQUESTLINE: //初始值
        {
            ret = parse_request_line(text); // 处理行 正常结束后 m_check_state = CHECK_STATE_HEADER
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER: // 处理请求头
        {
            ret = parse_headers(text); //解析头 返回请求状态
            if (BAD_REQUEST == ret)
            {
                return BAD_REQUEST;
            }
            else if (GET_REQUEST == ret) //头请求处理到尾(请求体为空)
            {
                return do_request(); // 处理请求
            }
            break;
        }
        case CHECK_STATE_CONTENT: //处理请求体
        {
            ret = parse_content(text);
            if (GET_REQUEST == ret) //请求体正常处理
            {
                return do_request();
            }
            line_state = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR; // 内部错误
        }
    }
    return NO_REQUEST;
}

// 处理请求
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root); // 将根目录拷贝到文件
    int len = strlen(doc_root); //文件目录长度
    // printf("m_url: %s\n", m_url);
    //m_url = "/2CGISQL.cgi"
    // p = "/2CGISQL.cgi"
    const char *p = strrchr(m_url, '/'); // 在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。

    // 处理 cgi POST连接
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    { 
        // 根据标志判断是登录检测还是注册检测
        char flag = m_url[1]; // 2 || 3  --- 2是登陆 3为注册
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/"); // m_url_real = "/"
        strcat(m_url_real, m_url + 2); // m_url_real = "/CGISQL.cgi"
        //将 m_url_real 拷贝到m_real_file = /home/v/MyItem/MyNetWorkItem/src/root/CGISQL.cgi
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1); 
        free(m_url_real);

        // 将用户名和密码提取出来
        // user=123&passwd=123
        char name[100], password[100];
        int i; 
        for (i = 5; m_string[i] != '&'; i++)
        {
            name[i - 5] = m_string[i]; //123
        }
        name[i - 5] = '\0';//123\0

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j) //ini i 改为 i
        {
            password[j] = m_string[i]; //123
        }
        password[j] = '\0'; //123\0

        if (*(p + 1) == '3')
        {
            // 如果是注册，先检测数据库中是否有重名的
            // 没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES( ");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            // 没有重复注册
            if (users.find(name) == users.end())
            {
                m_lock.lock(); //加锁
                int ret = mysql_query(mysql, sql_insert); //加入数据库
                users.insert(std::pair<std::string, std::string>(name, password)); //加入用户结构
                m_lock.unlock();

                if (!ret) //插入成功
                { // m_url = “/3CGISQL.cgi" -> "/log.html"
                    strcpy(m_url, "/log.html"); //将页面转到登陆界面
                }
                else
                {
                    strcpy(m_url, "/resgisterError.html");
                }
            }
            else
            {
                strcpy(m_url, "/resgister.html");
            }
        }
        // 如果是登录，直接判断
        // 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
            { //在数据库中
                strcpy(m_url, "/welcome.html");
            }
            else
            { //不在数据库中
                strcpy(m_url, "/logError.html");
            }
        }
    }

    //既不是登陆也不是注册
    if (*(p + 1) == '0') //欢迎界面转到注册
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/resgister.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1') //欢迎转登陆
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5') //转图片
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6') //转视频
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7') //转关注
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else //其他保持不动
    {
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }

    if (stat(m_real_file, &m_file_stat) < 0) // 文件状态
    {
        return NO_REQUEST;
    }

    if (!(m_file_stat.st_mode & S_IROTH)) // 没有读权限 have read permission
    {
        return FORBIDDEN_REQUEST; //禁止的请求
    }

    if (S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST; //目录
    }

    int fd = open(m_real_file, O_RDONLY); //只读打开
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_SHARED, fd, 0); // 将打开文件作为映射区
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address) //关闭文件映射
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0) //发送缓冲区内容为空（发完了）
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode); //将fd状态转为读
        init();
        return true; // 初始化
    }

    while (1)
    {
        // writev（）系统调用将iov描述的数据的iovcnt缓冲区写入与文件描述符fd相关联的文件（“收集输出”）
        // write面向的是连续内存块，writev面向的是分散的数据块
        // iov_base就是每个pair的基址，iov_len则是长度，不用包含“\0”。
        // 第二个参数是指向iovec数据结构的一个【指针】，其中iov_base为缓冲区首地址，iov_len为缓冲区长度，
        //参数iovcnt指定了iovec的个数

        //writev()函数是一个系统调用，用于将多个缓冲区中的数据一次性写入文件描述符中
        temp = writev(m_sockfd, m_iv, m_iv_count); // writev以顺序iov[0]、iov[1]至iov[iovcnt-1]从各缓冲区中聚集输出数据到fd
        if (temp < 0) // 发送结束
        {
            if (errno == EAGAIN)
            { // 资源短暂不可用，这个操作可能等下重试后可用
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap(); //关闭共享地址
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;

        if (bytes_to_send >= m_iv[0].iov_len) //需要发送的大小大于发送缓冲区大小（文件大小过大）
        {
            m_iv[0].iov_len = 0; //将发送缓冲的大小置为0 通过mmap共享映射发送
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            //m_iv[1].iov_len = bytes_have_send;
            m_iv[1].iov_len = bytes_to_send;
        }
        else //发送文件小通过写缓冲区发送
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            //m_iv[0].iov_len = bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0) //发送完毕
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode); //将fd转为接受

            if (m_linger) // 保持连接
            {
                init(); //初始化
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}


bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFER_SIZE) //写下标大于缓冲区大小
    {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    //处理通过 c的可变参数将可变参数写入写缓冲区
    //返回值：执行成功，返回最终生成字符串的长度，若生成字符串的长度大于size，则将字符串的前size个字符复制到str，同时将原串的长度返回（不包含终止符）
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFER_SIZE - 1 - m_write_idx)) //缓冲区大小不够满了
    {
        va_end(arg_list);
        return false;
    }

    m_write_idx += len; //
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);
    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{ //返回行：HTTP/1.1 200 OK
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_length)
{ //头信息 返回体信息长度   连接状态    \r\n
    return add_content_length(content_length) && add_liger() && add_blank_line();
}

bool http_conn::add_content_length(int content_length)
{ //文本长度
    return add_response("Content-Length:%d\r\n", content_length);
}

bool http_conn::add_content_type()
{ //文本类型
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_liger()
{//连接状态
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{//返回头和返回信息体之间的空行
    return add_response("%s", "\r\n");
}


bool http_conn::add_content(const char *content)
{//连接的状态 文字
    return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret) //写线程
{
    switch(ret)
    {
        case INTERNAL_ERROR: //请求解析返回服务器内部错误
        {
            add_status_line(500, error_500_title); //返回行
            add_headers(strlen(error_500_from)); //返回头
            if (!add_content(error_500_from)) //返回信息
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST: //错误请求
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_from));
            if (!add_content(error_404_from))
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST: //禁止访问
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_from));
            if (!add_content(error_403_from))
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST: //正常
        {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0) //文件大小不为0
            {
                add_headers(m_file_stat.st_size); //文件大小
                m_iv[0].iov_base = m_write_buf; //写缓冲区
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address; //文件地址 最为返回体
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2; //两个一起发送
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else
            {
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string))
                {
                    return false;
                }
            }
        }
        default:
            return true;
    }
    //请求错误并且写缓冲区满了
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;

    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}



void http_conn::process()
{
    HTTP_CODE read_ret = process_read(); //处理读缓冲（不是接受信息）
    if (read_ret == NO_REQUEST) //请求处理文件状态错误或缓冲区为0
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret); //处理写还缓冲（不是发送）
    if (!write_ret)//写错误 
    {
        close_conn(); //关闭连接
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode); 
    //需要判断是否建立了HTTP长连接，如果建立了，
    //那么就要尽量在一个TCP连接内完成多条HTTP报文的传送，
    //所以进入process函数，判断读缓冲区内是否存在收集到的请求，
    //有则继续监听EPOLLOUT，让本条请求的响应在下一轮epoll_wait中就绪
}



