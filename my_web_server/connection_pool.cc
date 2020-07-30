#include "connection_pool.h"

connection_pool* connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url,int port,string user,string password,string dbname,unsigned int maxconn)
{
    this->url=url;
    this->port=port;
    this->user=user;
    this->password=password;
    this->databasename=dbname;
    for (int i=0;i<maxconn;i++)
    {
        MYSQL* con=nullptr;
        con=mysql_init(con);

        if(con==nullptr)
        {
            printf("error: %s",mysql_error(con));
            exit(1);
        }
        con=mysql_real_connect(con,url.c_str(),user.c_str(),password.c_str(),dbname.c_str(),port,nullptr,0);
        if(con==nullptr)
        {
            printf("error: %s\n",mysql_error(con));
            exit(1);
        }
        conn_queue.push(con);
    }
    sem_init(&reserve,0,maxconn);
}

MYSQL* connection_pool::GetConnection()
{
    MYSQL* con=nullptr;
    if(conn_queue.size()==0)
        return nullptr;
    sem_wait(&reserve);
    locker.Lock();
    con=conn_queue.front();
    conn_queue.pop();
    locker.Unlock();
    return con;
}

bool connection_pool::ReleaseConnection(MYSQL* conn)
{
    if(conn==nullptr)
        return false;
    locker.Lock();
    conn_queue.push(conn);
    locker.Unlock();
    sem_post(&reserve);
}

void connection_pool::DestroyPool()
{
    locker.Lock();
    if(conn_queue.size()>0)
    {
        for(int i=0;i<conn_queue.size();i++)
        {
            MYSQL* con=conn_queue.front();
            conn_queue.pop();
            mysql_close(con);
        }
        locker.Unlock();
    }
    else
        locker.Unlock();
}

