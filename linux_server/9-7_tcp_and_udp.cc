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
#include <sys/epoll.h>
#include <errno.h>

#define MAX_EVENT_NUMBER 1025
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

void Addfd(int epollfd,int fd)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET;//设置需要监听的事件：数据可读、ET模式
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
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
   
    //创建IPv4 socket 地址
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short
    //创建TCPsocket，并将其绑定到端口port上
    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);

    int ret=bind(listenfd,(struct sockaddr*) &server_address,sizeof(server_address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);
    
    ret=listen(listenfd,10);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);

    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short
    //创建udp描述符
    int udpfd=socket(AF_INET,SOCK_DGRAM,0);//指定协议族：IPV4协议，套接字类型：数据报套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(udpfd>=0);
    //将udp描述符绑定到相同的ip和端口上
    int ret=bind(udpfd,(struct sockaddr*) &server_address,sizeof(server_address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);

    epoll_event events[MAX_EVENT_NUMBER];//存放epoll返回的就绪事件
    int epollfd=epoll_create(5);
    assert(epollfd!=-1);
    Addfd(epollfd,listenfd);
    Addfd(epollfd,udpfd);

    while(1)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number<0)
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)//对每个就绪事件
        {
            int sockfd=events[i].data.fd;//取出就绪事件对应的fd，分类讨论
            if(sockfd==listenfd)//如果是tcp监听描述符上的就绪事件，就接受连接，并将连接描述符加入内核事件表中
            {
                struct sockaddr_in client_address;
                socklen_t client_addr_length=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                Addfd (epollfd,connfd);
            }
            else if(sockfd==udpfd)//udp监听描述符上的就绪事件
            {
                char buf[UDP_BUFFER_SIZE];
                memset(buf,'\0',UDP_BUFFER_SIZE);
                struct sockaddr_in client_address;
                socklen_t client_addrlength=sizeof(client_address);
                //读数据的时候获得该udp描述符对应的socket地址
                ret=recvfrom(udpfd,buf,UDP_BUFFER_SIZE-1,0,(struct sockaddr*) &client_address,&client_addrlength);
                if(ret<0)//发数据的时候就往这个地址发
                    sendto(udpfd,buf,UDP_BUFFER_SIZE-1,0,(struct sockaddr*) &client_address,client_addrlength);
            }
            else if(events[i].events & EPOLLIN)//其他（连接）描述符上的可读事件，即TCP连接
            {
                char buf[TCP_BUFFER_SIZE];
                while(1)//循环读取数据把数据都读完
                {
                    memset(buf,'\0',TCP_BUFFER_SIZE);
                    ret=recv(sockfd,buf,TCP_BUFFER_SIZE-1,0);
                    if(ret<0)//recv出错时返回-1
                    {
                        if((errno==EAGAIN) || (errno==EWOULDBLOCK))//数据读完就可以退出循环了
                            break;
                        close(sockfd);//其他问题则关闭连接
                        break;
                    }
                    else if(ret==0)//recv返回0表示对方关闭了连接
                        close(sockfd);
                    else //读数据成功就把数据再发送回给客户端
                        send(sockfd,buf,ret,0);
                }
            }
            else 
                printf("something else happened\n");
        }
    }
    close(listenfd);
    return 0;
}