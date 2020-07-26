#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "14-7_locker.h"
#include "15-5_thread_pool.h"
#include "15-6_http_conn.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int Addfd(int epollfd,int fd,bool oneshot);
extern int Removdfd(int epollfd,int fd);

void AddSig(int sig,void (handler) (int),bool restart=true)//设置给定信号对应的信号处理函数
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;//本程序中所有指定信号的信号处理函数都是上面这个，即将信号值写入管道
    if(restart)
        sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr) != -1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
}

void ShowError(int connfd,const char* info)
{
    printf("%s",info);
    send(connfd,info,strlen(info),0);
    close(connfd);
}

int main(int argc,char *argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char *ip=argv[1];
    int port=atoi(argv[2]);
    
    AddSig(SIGPIPE,SIG_IGN);
    ThreadPool<HttpConn>* pool=nullptr;
    try
    {
        pool=new ThreadPool<HttpConn>;//建立线程池
    }
    catch(...)
    {
        return 1;
    }

    HttpConn* users=new HttpConn[100];//建立HTTP客户对象数组
    assert(users);
    int user_count=0;
    //创建IPv4 socket 地址
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    //创建TCPsocket，并将其绑定到端口port上
    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);
    struct linger tmp={1,0};
    setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

    int ret=bind(listenfd,(struct sockaddr*) &server_address,sizeof(server_address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);
    
    ret=listen(listenfd,5);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create(5);
    assert(epollfd!=1);
    Addfd(epollfd,listenfd,false);
    HttpConn::m_epollfd=epollfd;

    while(true)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)//就绪事件：监听描述符可读、信号事件、连接描述符可读、连接描述符可写
        {
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(sockfd==listenfd)
            {//
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                if(connection_fd<0)
                {
                    printf("errno is : %d\n",errno);
                    continue;
                }
                if(HttpConn::m_user_count>=MAX_FD)
                {
                    ShowError(connection_fd,"Internet server busy");
                    continue;
                }
                users[connection_fd].Init(connection_fd,client_address);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {//连接被对方关闭、管道的写端关闭、错误
                users[sockfd].CloseConn();
            }
            else if(events[i].events & EPOLLIN)
            {
                if(users[sockfd].Read())//成功读取到HTTP请求
                    pool->Append(users+sockfd);//将该请求加入到工作队列中
                else
                    users[sockfd].CloseConn();//读失败，关闭连接
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!users[sockfd].Write())//写失败
                    users[sockfd].CloseConn();//关闭连接
            }
            else 
                continue;
        }
    }

    close(epollfd);
    close(listenfd);//main函数创建的监听描述符，就必须由main函数来关闭
    delete []users;
    delete pool;
    return 0;
}