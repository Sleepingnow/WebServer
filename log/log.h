#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"
using namespace std;

class log
{
private:
    log();
    virtual ~log();
    
    /* 异步写入 */
    void* async_write_log()
    {
        string single_log;
        while(m_log_queue -> pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128];               //路径名
    char log_name[128];               //log文件名
    int m_split_lines;                //日志最大行数
    int m_log_buf_size;               //日志缓冲区大小
    long long m_count;                //日志行数记录
    int m_today;                      //记录当前日期
    char* m_buf;                      //要输出的内容
    bool m_is_async;                  //同步/异步标志
    FILE* m_fp;                       //log文件指针
    locker m_mutex;                   //互斥锁
    block_queue<string>* m_log_queue; //阻塞队列
    int m_close_log;                  //关闭日志

public:
    /* 懒汉单例模式 */
    static log* get_instance()
    {
        static log instance;
        return &instance;
    }

    static void* flush_log_thread(void* args)
    {
        log::get_instance() -> async_write_log();
    }

    /* 日志初始化 */
    bool init(const char* file_name, int close_log, int log_buf_size = 8000, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);

    void flush();
};

#define LOG_DEBUG(format, ...) if(m_close_log == 0) {log::get_instance -> write_log(0, format, ##__VA_ARGS__); log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(m_close_log == 0) {log::get_instance -> write_log(1, format, ##__VA_ARGS__); log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(m_close_log == 0) {log::get_instance -> write_log(2, format, ##__VA_ARGS__); log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(m_close_log == 0) {log::get_instance -> write_log(3, format, ##__VA_ARGS__); log::get_instance()->flush();}
#endif