#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <map>

#include "../locker/locker.h"
#include "../db/connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
public:
    //设置读取文件的名称m_real_file大小
    static const int FILENAME_LEN = 200;
    //设置读缓冲区m_read_buf大小
    static const int READ_BUFFER_SIZE = 2048;
    //设置写缓冲区m_write_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;

    // 请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    // 主状态机的三种可能状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    // 服务器处理HTTP请求的结果
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    // 从状态机的三种可能状态
    enum LINE_STATUS
    {
        LINE_OK = 0,    //读取到一个完整的行
        LINE_BAD,       //行错误
        LINE_OPEN       //行数据尚且不完整
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in& addr, char*, int, int, string user, string password, string dbname);
    
    //关闭http连接
    void close_conn(bool real_close = true);
    
    void process();
    
    //读取浏览器端发来的全部数据
    bool read_once();
    
    //写入响应报文
    bool write();
    sockaddr_in* get_address()
    {
        return &m_address;
    }
    
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool* conn_pool);

    int timer_flag; // 标识子线程读写任务是否成功，值为1时表示读写出现了错误
    int improv;     // 保持子线程和主线程的同步， 值为1时表示处理了读写任务

private:
    void init();
    
    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);
    
    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char* text);
    
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char* text);
    
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char* text);
    
    //生成响应报文
    HTTP_CODE do_request();
    
    //m_start_line是已经解析的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char* get_line() {return m_read_buf + m_start_line;}
    
    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();
    
    void unmap();
    
    //根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int m_state;             //读写状态，读为0，写为1

private:
    int m_sockfd;
    sockaddr_in m_address;
    
    //存储读取的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];
    
    //缓冲区m_read_buf中数据的最后一个字节的下一个位置
    int m_read_idx;
    
    //第0～checked_index字节都已分析完毕
    int m_checked_idx;

    //行在m_read_buf中的起始位置
    int m_start_line;

    //存储发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    
    //主状态机的状态
    CHECK_STATE m_check_state;
    
    //请求方法
    METHOD m_method;
    
    //以下为解析请求报文中对应的6个变量
    //存储读取文件的名称
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;
    
    //读取服务器上的文件地址
    char* m_file_address;
    struct stat m_file_stat;
    
    //io向量机制iovec，m_iv[0]中为响应的状态行和消息报头
    //m_iv[1]中为响应主体即服务器要发送的文件
    struct iovec m_iv[2];
    int m_iv_count;
    
    //是否启用POST
    int cgi;
    
    //存储请求头数据
    char* m_string;
    
    //剩余发送字节数
    int bytes_to_send;
    
    //已发送字节数
    int bytes_have_send;
    
    //根目录
    char* doc_root;

    std::map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_password[100];
    char sql_name[100];
    
};
#endif