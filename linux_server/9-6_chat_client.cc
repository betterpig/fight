#define _GNU_SOURCE 1
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#define BUFFER_SIZE 64

int main(int argc,char* argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char* ip=argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    int connectionfd=socket(PF_INET,SOCK_STREAM,0);
    assert(connectionfd>=0);
    if(connect(connectionfd,(struct sockaddr*) &server_address,sizeof(server_address))<0)
    {
        printf("connection failed\n");
        close(connectionfd);
        return 1;
    }

    pollfd fds[2];//同时监听标准输入和连接描述符，聊天程序需要
    fds[0].fd=0;//标准输入文件描述符
    fds[0].events=POLLIN;//注册标准输入的可读事件
    fds[0].revents=0;
    fds[1].fd=connectionfd;//监听连接描述符
    fds[1].events=POLLIN | POLLRDHUP;//对应注册的事件 可读和 （连接被关闭或者对方关闭了写操作）
    fds[1].revents=0;//实际发生的事件，由内核填充
    
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret=pipe(pipefd);//创建管道，失败返回-1.pipefd[0]读 pipefd[1]写
    assert(ret!=-1);

    while(1)
    {
        ret=poll(fds,2,-1);//结构体数组，监听的文件描述符总数，等待时间（-1为永远阻塞）
        if(ret<0)//poll失败时返回-1，否则返回就绪的文件描述符的总数
        {
            printf("poll failure\n");
            break;
        }

        if(fds[1].revents & POLLRDHUP)//如果是连接描述符的连接被关闭事件就绪，说明服务端关闭了连接
        {
            printf("server close the connection\n");
            break;
        }
        else if(fds[1].revents & POLLIN)//如果是连接描述符的可读事件就绪（即服务端发来了其他客户端发的消息），那就读取数据，并打印
        {
            memset(read_buf,'\0',BUFFER_SIZE);
            recv(connectionfd,read_buf,BUFFER_SIZE-1,0);
            printf("%s\n",read_buf);
        }

        if(fds[0].revents & POLLIN)//如果是标准输入的可读事件就绪，那就先将文件描述符0（标准输入）的数据移动到管道中
        {//再把管道中的数据写到连接描述符上（发送出去），（in,offset,out,offset,length,flag)
            ret=splice(0,nullptr,pipefd[1],nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
            ret=splice(pipefd[0],nullptr,connectionfd,nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);

        }
    }

    close(connectionfd);
    return 0;
}