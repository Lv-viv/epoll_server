- day 2 : 错误 段错误
* log类中的时间错误, write_log函数中 m_is_async && !m_log_queue->full() &&写成||

- day 3 ; 错误 超时信号处理不能循环
* void Utils::addsig(int sig, void(handler)(int), bool restart) 中 memset(&sa,\0',sizeof(0)); -> sizeof 0 改为 sizeof(sa);

- day 3 : 错误 无法加载页面
* do_request函数 for (i = i + 10; m_string[i] != '\0'; ++i, ++j) //ini i 改为 i
* parse_headers函数 if (text[0] == '\0') //'\n' 改为 '\0'

- day4 : 错误 只能v登陆



./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
* -p，自定义端口号
	* 默认9006
* -l，选择日志写入方式，默认同步写入
	* 0，同步写入
	* 1，异步写入
* -m，listenfd和connfd的模式组合，默认使用LT + LT
	* 0，表示使用LT + LT
	* 1，表示使用LT + ET
    * 2，表示使用ET + LT
    * 3，表示使用ET + ET
* -o，优雅关闭连接，默认不使用
	* 0，不使用
	* 1，使用
* -s，数据库连接数量
	* 默认为8
* -t，线程数量
	* 默认为8
* -c，关闭日志，默认打开
	* 0，打开日志
	* 1，关闭日志
* -a，选择反应堆模型，默认Proactor
	* 0，Proactor模型
	* 1，Reactor模型