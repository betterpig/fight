//测试listen函数：不调用accept，也就不会从已完成连接队列中取出连接。查看backlog参数对listen函数的影响
//按理说内核版本超过2.2了，backlog应该是已完成连接队列的上限，但是通过多次连接该服务器发现，已完成连接个
//数为backlog+1，没有RECV队列。按理说应该要有RECV队列

#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool stop=false;

//SIGTERM 信号的处理函数，触发式结束主程序中的循环
static void handle_term(int sig)
{
    stop=true;
}

int main(int argc,char **argv)
{
    signal(SIGTERM,handle_term);

    if(argc<=3)
    {
        printf("usage: %s ip_address port_number backlog\n",basename(argv[0]));
        return 1;
    }

    const char *ip=argv[1];
    int port=atoi(argv[2]);
    int backlog=atoi(argv[3]);

    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);

    //创建IPv4 socket 地址
    struct sockaddr_in address;//定义服务端套接字
    bzero(&address,sizeof(address));//先将服务器套接字结构体置0
    address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&address.sin_addr);//point to net
    address.sin_port=htons(port);//host to net short

    int ret=bind(listenfd,(struct sockaddr*) &address,sizeof(address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);

    ret=listen(listenfd,backlog);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);
    
    //循环等待连接，直到有SIGTERM信号将它终端
    while(!stop)
    {
        sleep(1);
    }

    //关闭listenfd
    close(listenfd);
    return 0;
}