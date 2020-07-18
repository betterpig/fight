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

    pollfd fds[2];
    fds[0].fd=0;
    fds[0].events=POLLIN;
    fds[0].revents=0;
    fds[1].fd=connectionfd;
    fds[1].events=POLLIN | POLLRDHUP;
    fds[1].revents=0;
    
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret=pipe(pipefd);
    assert(ret!=-1);

    while(1)
    {
        ret=poll(fds,2,-1);
        if(ret<0)
        {
            printf("poll failure\n");
            break;
        }

        if(fds[1].revents & POLLRDHUP)
        {
            printf("server close the connection\n");
            break;
        }
        else if(fds[1].revents & POLLIN)
        {
            memset(read_buf,'\0',BUFFER_SIZE);
            recv(fds[1].fd,read_buf,BUFFER_SIZE-1,0);
            printf("%s\n",read_buf);
        }

        if(fds[0].events & POLLIN)
        {
            ret=splice(0,nullptr,pipefd[1],nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
            ret=splice(pipefd[0],nullptr,connectionfd,nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);

        }
    }

    close(connectionfd);
    return 0;
}