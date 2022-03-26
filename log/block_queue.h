/*************************************************************
*将生产者-消费者模型封装为阻塞队列，创建一个写线程，工作线程将要写的内容push进队列，写线程从队列中取出内容，写入日志文件
**************************************************************/
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <queue>
#include "../locker/locker.h"

template <class T>
class block_queue
{
private:
    locker m_mutex;         //互斥锁
    cond m_cond;            //条件变量
    std::queue<T> m_queue;  //队列
    int m_max_size;         //最大容量

public:
    block_queue(int max_size = 1000)
    {
        if(max_size <= 0)
            exit(-1);

        m_max_size = max_size;
    }

    bool full()
    {
        m_mutex.lock();
        int sz = m_queue.size();
        m_mutex.unlock();
        return sz == m_max_size;
    }

    int size()
    {
        m_mutex.lock();
        int ret = m_queue.size();
        m_mutex.unlock();
        return ret;
    }

    /* 往队列中添加元素，唤醒正在等待条件变量的线程 */
    bool push(const T& value)
    {
        m_mutex.lock();
        if(mq.size() == m_max_size)
        {
            //队列已满，需要通知线程取出元素
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_queue.emplace(value);
        m_cond.broadcast();
        m_mutex.unlock();
        return true;        
    }

    /* 
     * 写线程从队列中取出元素
     * 队列为空时，将会等待条件变量
     */
    bool pop(T& value)
    {
        m_mutex.lock();
        while(m_queue.empty())
        {
            if(!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }    
        }
        value = m_queue.front();
        m_queue.pop();
        m_mutex.unlock();
        return true;
    }

    /* 
     * 超时处理
     * 写线程从队列中取出元素
     * 队列为空时，将会等待条件变量
     */
    bool pop(T& value, int ms_timeout)
    {
        struct timespce t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        
        m_mutex.lock();
        if(m_queue.empty())
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if(!m_cond.timewait(m_mutex.get(), t)
            {
                m_mutex.unlock();
                return false;
            }    
        }

        if(m_queue.empty())
        {
            m_mutex.unlock();
            return false;
        }

        value = m_queue.front();
        m_queue.pop();
        m_mutex.unlock();
        return true;
    }
};
#endif