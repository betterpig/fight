#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define BUFFER_SIZE 1024

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

int UnblockConnect(const char* ip,int port,int time)
{
    int ret=0;
    //创建IPv4 socket 地址
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    int connectionfd=socket(PF_INET,SOCK_STREAM,0);
    int old_option=SetNonBlocking(connectionfd);//将该连接描述符设置为非阻塞形式
    //尝试连接，因为是非阻塞式，所以会直接返回结果，不会等待，如果成功，connect返回值为0；失败时返回值-1，并设置errno
    ret = connect(connectionfd,(struct sockaddr*) &server_address,sizeof(server_address));
    if(ret==0)
    {
        printf("connect with server immediately\n");//连接成功
        fcntl(connectionfd,F_SETFL,old_option);//将该连接描述符重新设置为旧的状态标志：阻塞式
        return connectionfd;//直接返回连接描述符
    }
    else if(errno != EINPROGRESS)//对非阻塞的连接描述符调用connect，而连接又没有建立时，error值是这个
    {//如果不是这个，就说明出错了
        printf("unblock connect not support\n");
        return -1;
    }

    fcntl(connectionfd,F_SETFL,old_option);//将该连接描述符重新设置为旧的状态标志：阻塞式
    fd_set writefds;
    struct timeval timeout;
    FD_ZERO(&writefds);
    FD_SET(connectionfd,&writefds);//将该未建立连接的连接描述符加入到可写事件的描述符集合中

    timeout.tv_sec=time;
    timeout.tv_usec=0;
    //这里不管怎么设置，select都不会阻塞，根本不会等待就绪。
    ret=select(connectionfd+1,nullptr,&writefds,nullptr,nullptr);//监听该连接描述符的可写事件
    if(ret<=0)//select返回值为就绪事件的个数。<0说明在timeout时间内，没有事件就绪，即连接请求还是未得到响应
    {
        printf("connection tiime out\n");
        close(connectionfd);//关闭连接描述符
        return -1;
    }
    if(!FD_ISSET(connectionfd,&writefds))//如果就绪事件数大于0，
    {//检查可写事件上的该连接描述符是否被设置，未设置说明是别的事件就绪了
        printf("no events on connectionfd found\n");
        close(connectionfd);
        return -1;
    }
    //如果监听的连接描述符可写事件就绪了，说明连接成功了，需要把该连接描述符上的错误码给清除
    int error=0;
    socklen_t length=sizeof(error);
    if(getsockopt(connectionfd,SOL_SOCKET,SO_ERROR,&error,&length)<0)//获得连接描述符上的错误状态并清除
    {
        printf("get socket option failed\n");
        close(connectionfd);
        return -1;
    }
    if(error!=0)//错误状态清除后，errno应该为0，表示正常状态
    {
        printf("connection failed after select with the error : %d \n",error);
        close(connectionfd);
        return -1;
    }
    //清除错误状态后，才能说真的连接成功了
    printf("connection ready after select with the socket: %d\n",connectionfd);
    fcntl(connectionfd,F_SETFL,old_option);
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
    
    int connectionfd=UnblockConnect(ip,port,10);//超时时间是10s
    if(connectionfd<0)
        return 1;
    close (connectionfd);
    return 0;
}