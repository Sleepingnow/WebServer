#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http/http_conn.h"

#define MAX_EVENT_NUMBER 10000      // 最大事件数

const int MAX_FD = 65536;           // 最大文件描述符
const int TIMESLOT = 5;             // 最小超时单位

// 封装服务器类
class WebServer
{
private:
    int m_port;         // 服务器端口号
    char* m_root;       // 服务器根目录
    int m_log_write;    // 日志写入方式（同步/异步）
    int m_close_log;    // 是否关闭日志
    int m_actormodel;   // 并发模型

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    // 数据库相关
    connection_pool *m_connPool;    // 数据库连接池
    string m_user;                  // 用户名
    string m_password;              // 密码
    string m_databaseName;          // 数据库名
    int m_sql_num;

    // 线程池相关
    threadpool<http_conn>* m_pool;
    int m_thread_num;

    // epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;         // 监听socket
    int m_OPT_LINGER;       // 是否开启SO_LINGER选项
    int m_TRIGMode;         // 触发组合模式
    int m_LISTENTrigmode;   // listenfd触发模式
    int m_CONNTrigmode;     // connfd触发模式

    // 定时器相关
    client_data* users_timer;
    Utils utils;  

public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string password, string databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer* timer);
    void deal_timer(util_timer* timer, int sockfd);
    bool deal_clientdata();
    bool deal_signal(bool& timeout, bool& stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);
};
#endif