#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../locker/locker.h"
#include "../log/log.h"

class connection_pool
{
public:
    std::string m_url;       //主机地址
    std::string m_port;      //数据库端口号
    std::string m_user;      //登录数据库用户名
    std::string m_password;  //登录数据库密码
    std::string m_database;  //数据库名
    int m_close_log;    //日志开关

private:
    connection_pool();
    ~connection_pool();

    int m_maxConn;      //最大连接数
    int m_curConn;      //当前已使用的连接数
    int m_freeConn;     //当前空闲的连接数
    locker lock;        //互斥锁
    std::list<MYSQL*> connList;  //连接池A
    sem reserve;        //信号量

public:
    MYSQL* getConnection();               //获取数据库连接
    bool releaseConnection(MYSQL* conn);  //释放连接
    int getFreeConn();                    //获取连接
    void destroyPool();                    //销毁所有连接

    //单例模式
    static connection_pool* getInstance();

    void init(std::string url, std::string user, std::string password, std::string database, int port, int max_conn, int close_log);
};

class connectionRAII
{
private:
    MYSQL* connRAII;
    connection_pool* poolRAII;

public:
    connectionRAII(MYSQL** conn, connection_pool* conn_pool);
    ~connectionRAII();
};
#endif