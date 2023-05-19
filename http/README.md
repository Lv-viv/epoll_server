  http连接处理类
  ===============
  根据状态转移,通过主从状态机封装了http连接类。其中,主状态机在内部调用从状态机,从状态机将处理状态和数据传给主状态机
  > * 客户端发出http连接请求
  > * 从状态机读取数据,更新自身状态和接收数据,传给主状态机
  > * 主状态机根据从状态机状态,更新自身状态,决定响应请求还是继续读取

> * EPOLLSHOT相当于说，某次循环中epoll_wait唤醒该事件fd后，就会从注册中删除该fd,也就是说以后不会epollfd的表格中将不会再有这个fd,也就不会出现多个线程同时处理一个fd的情况
> * //EPOLLRDHUP 要想知道对端是否关闭

> *  Request URL:http://www.baidu.com/
> *  Remote Address:61.135.169.125:80
> *  Request URL:http://www.baidu.com/
> *  Request Method:GET
> *  Status Code:200 OK

==============================================
* class http_conn http连接类，每次连接创建一个类，加入线程池队列 
GET / HTTP/1.1\r\n
Host: 127.0.0.1:9007\r\n






















