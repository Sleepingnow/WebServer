#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer* p = head;
    while(p)
    {
        head = p -> next;
        delete p;
        p = head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer)
{
    if(!timer)
        return;
    if(!head)
    {
        head = tail = timer;
        return;
    }
    // 在头部插入
    if(timer -> expire < head -> expire)
    {
        timer -> next = head;
        head -> prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if(!timer)
        return;
    util_timer* p = timer -> next;
    if(!p || (timer -> expire < p -> expire))
        return;
    // 将imer取出再插入
    if(timer == head)
    {
        head = head -> next;
        head -> prev = NULL;
        timer -> next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer -> prev -> next = timer -> next;
        timer -> next -> prev = timer -> prev;
        add_timer(timer, timer -> next);
    }
}

void sort_timer_lst::del_timer(util_timer* timer)
{
    if(!timer)
        return;
    // 当前链表中只有timer一个结点
    if((timer == head) && timer == tail)
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if(timer == head)
    {
        head = head -> next;
        head -> prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail)
    {
        tail = tail -> prev;
        tail -> next = NULL;
        delete timer;
        return;
    }
    timer -> prev -> next = timer -> next;
    timer -> next -> prev = timer -> prev;
    delete timer;
}

// 处理非活动连接
void sort_timer_lst::tick()
{
    if(!head)
        return;
    
    time_t cur = time(NULL);            
    util_timer* p = head;
    while(p)
    {
        // 链表中所有结点均未超时
        if(cur < p -> expire)
            break;

        // 调用定时器回调函数，删除事件，关闭文件描述符
        p -> cb_func(p -> user_data);
        
        // 头指针后移
        head = p -> next;
        if(head)
            head -> prev = NULL;
        
        // 删除超时结点
        delete p;
        p = head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* p = prev -> next;
    while(p)
    {
        if(timer -> expire < p -> expire)
        {
            prev -> next = timer;
            timer -> next = p;
            p -> prev = timer;
            timer -> prev = prev;
            break;
        }
        prev = p;
        p = p -> next;
    }
    // 没有比timer过期时间更长的定时器，将timer插入队尾
    if(!p)
    {
        prev -> next = timer;
        timer -> prev = prev;
        timer -> next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig)
{
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    // 调用tick函数处理非活动连接
    m_timer_lst.tick();
    // 重新定时
    alarm(m_TIMESLOT);
}

// 出错处理
void Utils::show_error(int connfd, const char* info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

// class Utils;

// 定时器回调函数
void cb_func(client_data* user_data)
{
    // 删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data -> sockfd, 0);
    assert(user_data);
    
    // 关闭文件描述符
    close(user_data -> sockfd);

    // 减少连接数
    http_conn::m_user_count--;
}