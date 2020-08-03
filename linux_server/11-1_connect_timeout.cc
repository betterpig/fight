#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

int TimeoutConnect(const char* ip,int port,int time)
{
    int ret=0;
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    int connectionfd=socket(PF_INET,SOCK_STREAM,0);
    assert(connectionfd>=0);

    struct timeval timeout;
    timeout.tv_sec=time;
    timeout.tv_usec=0;
    socklen_t len=sizeof(timeout);
    ret=setsockopt(connectionfd,SOL_SOCKET,SO_SNDTIMEO,&timeout,len);
    assert(ret!=-1);

    ret=connect(connectionfd,(struct sockaddr*) &server_address,sizeof(server_address));
    if(ret==-1)
    {
        if(errno==EINPROGRESS)
        {
            printf("connecting timeout,process timeout logic \n");
            return -1;
        }
        printf("error occur when connecting to server\n");
        return -1;
    }
    return connectionfd;
}

int main(int argc,char* argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char* ip=argv[1];
    int port = atoi(argv[2]);

    int connection_fd=TimeoutConnect(ip,port,10);
    if(connection_fd<0)
        return 1;
    return 0;
}