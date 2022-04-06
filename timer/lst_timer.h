#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

// 定时器类
class util_timer;

// 连接数据结构
struct client_data
{
    sockaddr_in address; //连接地址
    int sockfd;          //socket描述符
    util_timer* timer;   //对应的定时器
};

class util_timer{
public:
    time_t expire;                  //任务超时时间(绝对时间)
    void (*cb_func)(client_data*);  //任务回调函数
    client_data* user_data;         //回调函数处理的连接数据
    util_timer* prev;               //指向前一个定时器
    util_timer* next;               //指向后一个定时器

public:
    util_timer() : prev(NULL), next(NULL) {}
};

// 升序的定时器链表
class sort_timer_lst{
private:
    util_timer* head;               //头指针，指向链表头结点
    util_timer* tail;               //尾指针，指向链表尾结点

    // 重载的私有函数，被公有成员add_timer和adjust_time调用
    // 将timer添加到结点lst_head之后的目标链表上
    void add_timer(util_timer* timer, util_timer* lst_head);

public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer* timer);    //添加定时器，内部调用私有成员add_timer
    void adjust_timer(util_timer* timer); //调整定时器，任务发生变化时，调整定时器在链表中的位置
    void del_timer(util_timer* timer);    //删除定时器
    void tick();                          //心跳函数
};

// 工具类，组合定时器链表、信号管道和事件集描述符
class Utils
{
public:
    // 声明为静态的可使多个链表共用一个pipefd. 下同
    static int* u_pipefd;       
    static int u_epollfd;
    sort_timer_lst m_timer_lst;
    int m_TIMESLOT;     // 处理非活动连接的期限

public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char* info);
};

void cb_func(client_data* user_data);
#endif