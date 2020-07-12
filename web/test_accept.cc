//测试accept函数：accept只是从已完成连接队列中取一个连接出来，而队列存储在内核中。当客户端正常退出
//时，连接状态已经变成close_wait；或者当客户端网络断开，但连接状态仍为established，此时连接都是无效
//的了，但accept并不关心，它只是从队列中取一个连接出来。

#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(int argc,char *argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }

    const char *ip=argv[1];
    int port=atoi(argv[2]);

    //创建IPv4 socket 地址
    struct sockaddr_in address;//定义服务端套接字
    bzero(&address,sizeof(address));//先将服务器套接字结构体置0
    address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&address.sin_addr);//point to net
    address.sin_port=htons(port);//host to net short

    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);

    int ret=bind(listenfd,(struct sockaddr*) &address,sizeof(address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);

    ret=listen(listenfd,5);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);
    
    //暂停20秒以等待客户端连接和相关操作（掉线火车退出）完成
    sleep(20);
    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
    socklen_t client_addr_length=sizeof(client_address);
    int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
    if(connection_fd<0)
        printf("errno is: %d\n",errno);
    else
    {
        //接受连接成功则打印出客户端的IP地址和端口号
        char remote[INET_ADDRSTRLEN];//存放网络字节序地址转换为点分发ip地址的结果，inet_ntop会返回remote指针
        printf("connected with ip: %s and port: %d\n",
                inet_ntop(AF_INET,&client_address.sin_addr,remote,INET_ADDRSTRLEN),
                ntohs(client_address.sin_port));
        close(connection_fd);
    }
    //关闭监听描述符
    close(listenfd);
    return 0;
}