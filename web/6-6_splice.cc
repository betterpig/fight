#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

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
    
    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
    socklen_t client_addr_length=sizeof(client_address);
    int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
    if(connection_fd<0)
        printf("errno is: %d\n",errno);
    else
    {
        int pipefd[2];
        ret=pipe(pipefd);//创建管道，其中pipefd[0]只能读，pipefd[1]只能写
        //把来自连接描述符对应的“文件”的数据，写到管道中
        ret=splice(connection_fd,nullptr,pipefd[1],nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret!=-1);
        //把管道中的数据，写回到连接描述符对应的“文件”中，即发送给客户端
        ret=splice(pipefd[0],nullptr,connection_fd,nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret!=-1);
        close(connection_fd);
    }

    //关闭监听描述符
    close(listenfd);
    return 0;
}