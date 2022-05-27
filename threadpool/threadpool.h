#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../locker/locker.h"
#include "../db/connection_pool.h"

template <typename U>
class threadpool
{
private:
    int m_thread_number;            //线程池中的线程数
    int m_max_requests;             //请求队列中允许的最大请求数
    pthread_t* m_threads;           //用于描述线程池的数组
    std::queue<U*> m_queue;         //请求队列
    locker m_locker;                //保护请求队列的互斥锁
    sem m_stat;                     //信号量，指示是否有任务需要处理
    connection_pool* m_conn_pool;   //数据库连接池
    int m_actor_model;              //用于模型切换

private:
    // 工作线程运行函数，从请求队列中取出任务并执行
    static void* worker(void* arg);
    void run();

public:
    threadpool(int actor_model, connection_pool* conn_pool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(U* request, int state);
    bool append_p(U* request);
};

template <typename U>
threadpool<U>::threadpool(int actor_mode, connection_pool* conn_pool, int thread_number, int max_requests) : m_actor_model(actor_mode),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL), m_conn_pool(conn_pool)
{
    if(thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(int i = 0; i < thread_number; ++i)
    {
        /* 
         * 循环创建线程
         * pthread_create函数中将类的对象作为参数传递给静态函数(worker),在静态函数中引用这个对象,并调用其动态方法(run)
         */
        if(pthread_create(m_threads + i,NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        // 分离这些线程
        if(pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename U>
threadpool<U>::~threadpool()
{
    delete[] m_threads;
}

template <typename U>
bool threadpool<U>::append(U* request, int state)
{
    m_locker.lock();
    if(m_queue.size() >= m_max_requests)
    {
        m_locker.unlock();
        return false;
    }
    request -> m_state = state;
    // 添加任务
    m_queue.emplace(request);
    m_locker.unlock();

    // 提醒有任务需要处理
    m_stat.post();
    return true;
}

template <typename U>
bool threadpool<U>::append_p(U* request)
{
    m_locker.lock();
    if(m_queue.size() >= m_max_requests)
    {
        m_locker.unlock();
        return false;
    }
    m_queue.emplace(request);
    m_locker.unlock();
    m_stat.post();
    return true;
}

template <typename U>
void* threadpool<U>::worker(void* arg)
{
    //将参数强转为线程池类，调用成员方法
    threadpool* pool = (threadpool*) arg;
    pool -> run();
    return pool;
}

// 执行任务
template <typename U>
void threadpool<U>::run()
{
    while(true)
    {
        // 信号量等待
        m_stat.wait();
        // 被唤醒后先加互斥锁
        m_locker.lock();
        if(m_queue.empty())
        {
            m_locker.unlock();
            continue;
        }

        // 从请求队列中取出第一个任务
        U* request = m_queue.front();
        m_queue.pop();
        m_locker.unlock();
        if(!request)
            continue;
        
        // Reactor模式，子线程负责读写数据
        if (1 == m_actor_model)
        {
            // 读数据
            if (0 == request->m_state)
            {
                // 读取成功
                if (request->read_once())
                {
                    request->improv = 1;
                    // 为该http连接绑定一个mysql连接
                    connectionRAII mysqlcon(&request->mysql, m_conn_pool);
                    // 处理请求
                    request->process();
                }
                // 读取失败
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            // 写数据
            else
            {
                // 写入成功
                if (request->write())
                {
                    request->improv = 1;
                }
                // 写入失败
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        // Proactor模式，子线程只负责业务逻辑
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_conn_pool);
            request->process();
        }
    }
}
#endif