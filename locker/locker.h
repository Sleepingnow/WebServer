#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
/* 封装信号量 */
class sem
{
private:
    sem_t m_sem;
public:
    sem()
    {
        /* 构造函数没有返回值，可以通过抛出异常来报告错误 */
        if(sem_init(&m_sem, 0, 0) != 0)
            throw std::exception();
    }
    sem(int val)
    {
        if(sem_init(&m_sem, 0, val) != 0)
            throw std::exception();
    }
    ~sem()
    {
        /* 销毁信号量 */
        sem_destroy(&m_sem);
    }
    /* 等待信号量 */
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    /* 释放信号量 */
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
};

/* 封装互斥锁 */
class locker
{
private:
    pthread_mutex_t m_mutex;
public:
    /* 创建并初始化互斥锁 */
    locker()
    {
        if(pthread_mutex_init(&m_mutex, NULL) != 0)
            throw std::exception();
    }
    /* 销毁互斥锁 */
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    /* 获取互斥锁 */
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    /* 释放互斥锁 */
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    /* 获取互斥锁变量的地址 */
    pthread_mutex_t* get()
    {
        return &m_mutex;
    }
};

/* 封装条件变量 
 * 由于调用wait和timewait函数处已被互斥锁包围，
 * 此类中便不声明互斥锁 
 */
class cond
{
private:
    pthread_cond_t m_cond;
public:
    /* 创建并初始化条件变量 */
    cond()
    {
        if(pthread_cond_init(&m_cond, NULL) != 0)
            throw std::exception();
    }
    /* 销毁条件变量 */
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    /* 等待条件变量，mutex参数用于pthread_cond_wait函数 */
    bool wait(pthread_mutex_t* m_mutex)
    {
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }
    /* 在规定的时间内等待条件变量，mutex参数用于pthread_cond_wait函数 */
    bool timeWait(pthread_mutex_t* m_mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }
    /* 唤醒一个等待条件变量的线程 */
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    /* 唤醒所有等待条件变量的线程 */
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};
#endif