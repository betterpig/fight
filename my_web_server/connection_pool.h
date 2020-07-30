#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H


#include <string>
#include <mysql/mysql.h>
#include <queue>
#include "locker.h"

using namespace std;

class connection_pool
 {
 public:
    //局部静态变量单例模式
    static connection_pool *GetInstance();
    void init(string url,int port,string user,string password,string dbname,unsigned int maxconn);
    MYSQL* GetConnection();
    bool ReleaseConnection(MYSQL* conn);
    void DestroyPool();
    
 private:
    string url;
    int port;
    string user;
    string password;
    string databasename;
    queue<MYSQL*> conn_queue;
    sem_t reserve;
    Locker locker;
    connection_pool(){}
    ~connection_pool()
    {
        DestroyPool();
    }
};

class connectionRAII
{
public:
    connectionRAII(MYSQL** con,connection_pool* connpool)
    {
        *con=connpool->GetConnection();
        conRAII=*con;
        poolRAII=connpool;
    }
    ~connectionRAII()
    {
        poolRAII->ReleaseConnection(conRAII);
    }
private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
};

#endif