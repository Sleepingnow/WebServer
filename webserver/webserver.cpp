#include "webserver.h"

WebServer::WebServer()
{
    // 初始化http_conn对象数组
    users = new http_conn[MAX_FD];

    // root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);
}

// 关闭连接，关闭文件描述符，释放资源
WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string password, string databaseName, int log_write,
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = password;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void WebServer::trig_mode()
{
    // LT + ET
    if(m_TRIGMode == 0)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    // LT + ET
    else if(m_TRIGMode == 1)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    // ET + LT
    else if(m_TRIGMode == 2)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    // ET + ET
    else if(m_TRIGMode == 3)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write()
{
    if(m_close_log == 0)
    {
        // 异步写入日志
        if(m_log_write == 1)
            log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void WebServer::sql_pool()
{
    // 初始化数据库连接池
    m_connPool = connection_pool::getInstance();
    m_connPool -> init("localhost", m_user, m_password, m_databaseName, 3306, m_sql_num, m_close_log);

    users -> initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    // 线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::event_listen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    if(m_OPT_LINGER == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(m_OPT_LINGER == 1)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    // 创建epoll事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    // 创建信号管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    // 为每一个客户端绑定一个http_conn对象
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_password, m_databaseName);

    //初始化client_data数据, 客户端套接字标识符作为数组下标
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    
    //创建定时器，设置回调函数和超时时间，绑定用户数据
    util_timer* timer = new util_timer;
    timer -> user_data = &users_timer[connfd];
    timer -> cb_func = cb_func;
    time_t cur = time(NULL);
    // 超时时间为绝对时间
    timer -> expire = cur + 3 * TIMESLOT;
    
    // client_data与timer双向绑定；将定时器添加到链表中
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer* timer)
{
    time_t cur = time(NULL);
    timer -> expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer");
}

// 关闭连接相关删除工作
void WebServer::deal_timer(util_timer* timer, int sockfd)
{
    // 删除注册事件、关闭文件描述符等
    timer -> cb_func(&users_timer[sockfd]);
    // 将定时器从链表中移除
    if(timer)
        utils.m_timer_lst.del_timer(timer);
    
    // 打印日志
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

// 监听连接
bool WebServer::deal_clientdata()
{
    struct sockaddr_in client_address;
    socklen_t len = sizeof(client_address);
    // LT
    if(m_LISTENTrigmode == 0)
    {
        int connfd = accept(m_listenfd, (struct sockaddr*)& client_address, &len);
        if(connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    // ET
    else
    {
        while(1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr*) &client_address, &len);
            if(connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

// 参数为2个引用用于指示信号的到来
bool WebServer::deal_signal(bool& timeout, bool& stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    // 接收信号
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1)
        return false;
    else if(ret == 0)
        return false;
    else
    {
        for(int i = 0; i < ret; ++i)
        {
            switch(signals[i])
            {
                case SIGALRM:
                    timeout = true;
                    break;
                case SIGTERM:
                    stop_server = true;
                    break;
            }
        }
    }
    return true;
}   

void WebServer::deal_read(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;

    // Reactor
    if(m_actormodel == 1)
    {
        // 检测到读事件，刷新定时器过期时间
        if(timer)
            adjust_timer(timer);
        
        // 将该事件放入请求队列，由子线程进行读写并处理，主线程只起到通知作用
        m_pool -> append(users + sockfd, 0);
    
        /* 
         * 在threadpool<T>::run()中的request->read_once()或request->write()完成后imporv会被置1，
         * 标志着http连接的读写任务已完成（请求已处理完毕）；
         * 如果http连接的读写任务失败（请求处理出错）timer_flag会被置1。
         * 在 WebServer::dealwith_read()和WebServer::dealwith_write()中，
         * 会循环等待连接improv被置1，也就是一直等待该http连接的请求处理完毕，如果请求处理失败则关闭该http连接。
         */
        while(true)
        {
            // 读写任务完成
            if(users[sockfd].improv == 1)
            {
                // 出错时关闭连接
                if(users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    // Proactor
    else
    {
        // 主线程读取客户端发来的数据
        if(users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address() -> sin_addr));

            // 若监测到读事件，将该事件放入请求队列
            m_pool -> append_p(users + sockfd);

            if(timer)
                adjust_timer(timer);
        }
        // 读取失败时关闭连接
        else
            deal_timer(timer, sockfd);
    }
}

void WebServer::deal_write(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;
    // Reactor
    if(m_actormodel == 1)
    {
        // 更新定时器超时时间
        if(timer)
            adjust_timer(timer);

        m_pool -> append(users + sockfd, 1);
            
        while(true)
        {
            if(users[sockfd].improv == 1)
            {
                if(users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    // Proactor
    else
    {
        // 主线程负责写入数据
        if(users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address() -> sin_addr));

            if(timer)
                adjust_timer(timer);
        }
        else
            deal_timer(timer, sockfd);
    }
}

void WebServer::event_loop()
{
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if(sockfd == m_listenfd)
            {
                bool flag = deal_clientdata();
                if(flag == false)
                    continue;
            }
            // 出错时
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                util_timer* timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理信号
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_signal(timeout, stop_server);
                if(flag == false)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            // 处理客户连接上接收到的数据
            else if(events[i].events & EPOLLIN)
            {
                deal_read(sockfd);
            }
            // 有数据待发送
            else if(events[i].events & EPOLLOUT)
            {
                deal_write(sockfd);
            }
        }
        // 定期处理非活动连接
        if(timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}