#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024

struct fds
{
    int epollfd;
    int sockfd;
};

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option= old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void Addfd(int epollfd,int fd,bool oneshot)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN;
    if(oneshot)
        event.events=event.events | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    SetNonBlocking(fd);
}

void ResetOneShot(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void* worker(void* arg)
{
    int sockfd=((fds*) arg)->sockfd;
    int epollfd=((fds*) arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n",sockfd);
    char buf[BUFFER_SIZE];
    memset(buf,'\0',BUFFER_SIZE);
    
    while(1)
    {
        int ret=recv(sockfd,buf,BUFFER_SIZE-1,0);
        if(ret==0)
        {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        }
        else if(ret<0)
        {
            if(errno==EAGAIN)
            {
                ResetOneShot(epollfd,sockfd);
                printf("read later\n");
                break;
            }
        }
        else
        {
            printf("get conten: &s\n",buf);
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n",sockfd);
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
    
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create(5);
    assert(epollfd!=-1);
    Addfd(epollfd,listenfd,false);
    
    while(1)
    {
        int ret=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if(ret<0)
        {
            printf("epoll failure\n");
            break;
        }
        
        for(int i=0;i<ret;i++)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd)
            {
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                Addfd(epollfd,connection_fd,true);
            }
            else if(events[i].events & EPOLLIN)
            {
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd=epollfd;
                fds_for_new_worker.sockfd=sockfd;
                pthread_create(&thread,NULL,worker,(void*) &fds_for_new_worker);
            }
            else
                printf("something else happened\n");
        }
    }
    
    close(listenfd);
    return 0;
}