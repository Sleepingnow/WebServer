#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    m_curConn = 0;
    m_freeConn = 0;
}

/* 懒汉模式 */
connection_pool* connection_pool::getInstance()
{
    static connection_pool conn_pool;
    return &conn_pool;
}

/* 连接池初始化 */
void connection_pool::init(string url, string user, string password, string database, int port, int max_conn, int close_log)
{
    m_url = url;
    m_user = user;
    m_password = password;
    m_database = database;
    m_port = port;
    m_maxConn = max_conn;
    m_close_log = close_log;

    for(int i = 0; i < max_conn; ++i)
    {
        MYSQL* conn = NULL;
        conn = mysql_init(conn);
        if(conn == NULL)
        {
            LOG_ERROR("MySQL Error: mysql_init() returns NULL");
            exit(1);
        }

        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), database.c_str(), port, NULL, 0);
        if(conn == NULL)
        {
            cout << mysql_errno(conn) << "   " << mysql_error(conn) << endl;
            string err_info(mysql_error(conn));
			err_info = (string("MySQL Error[errno=")
				+ std::to_string(mysql_errno(conn)) + string("]: ") + err_info);
			LOG_ERROR(err_info.c_str());
            exit(1);
        }
        connList.emplace_back(conn);
        ++m_freeConn;
    }

    // 信号量初始化为连接池中连接数
    reserve = sem(m_freeConn);
    m_maxConn = m_freeConn;
}

/* 从连接池中返回一个可用连接 */
MYSQL* connection_pool::getConnection()
{
    MYSQL* conn = NULL;

    // 池中无可用连接
    if(connList.size() == 0)
        return NULL;

    // 信号量减一
    reserve.wait();

    lock.lock();

    // 从链表头部取出一个连接
    conn = connList.front();
    connList.pop_front();

    --m_freeConn;
    ++m_curConn;

    lock.unlock();
    return conn;
}

/* 释放当前使用的连接 */
bool connection_pool::releaseConnection(MYSQL* conn)
{
    if(conn == NULL)
        return false;

    lock.lock();

    // 连接插回链表尾部
    connList.emplace_back(conn);
    ++m_freeConn;
    --m_curConn;

    lock.unlock();
    // 信号量加一
    reserve.post();
}

/* 销毁数据库连接池 */
void connection_pool::destroyPool()
{
    lock.lock();
    if(connList.size() > 0)
    {
        for(auto it = connList.begin(); it != connList.end(); ++it)
        {
            // 逐一关闭连接
            MYSQL* conn = *it;
            mysql_close(conn);
        }
        m_curConn = 0;
        m_freeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

/* 当前空闲的连接数 */
int connection_pool::getFreeConn()
{
    return this -> m_freeConn;
}

connection_pool::~connection_pool()
{
    destroyPool();
}

connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* conn_pool)
{
    *SQL = conn_pool -> getConnection();
    connRAII = *SQL;
    poolRAII = conn_pool;
}

connectionRAII::~connectionRAII()
{
    poolRAII -> releaseConnection(connRAII);
}