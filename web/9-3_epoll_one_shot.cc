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
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回就得状态标志，以便日后能够恢复
}

void Addfd(int epollfd,int fd,bool oneshot)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET;//设置需要监听的事件：数据可读、ET模式
    if(oneshot)//设置为oneshot模式：只会触发一次，直到调用epoll_ctl函数将文件描述符再次设置为EPOLLONESHOT。
        event.events=event.events | EPOLLONESHOT;//保证同一时间只有一个进程或线程对该文件描述符进行操作
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

void ResetOneShot(int epollfd,int fd)//重置就是再次调用epoll_ctl函数，操作是修改，事件还是那些事件
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void* worker(void* arg)
{
    int sockfd=((fds*) arg)->sockfd;//先把void*类型的指针转换为fds*类型，然后取出连接描述符和内核事件表描述符
    int epollfd=((fds*) arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n",sockfd);
    char buf[BUFFER_SIZE];
    memset(buf,'\0',BUFFER_SIZE);
    
    while(1)
    {
        int ret=recv(sockfd,buf,BUFFER_SIZE-1,0);
        if(ret==0)//对方关闭了连接
        {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        }
        else if(ret<0)
        {
            if(errno==EAGAIN)//数据读取完毕，并且没有新数据过来
            {//重置epollfd内核事件表中的该文件描述符sockfd的ONESHOT事件
                printf("read later\n");
                ResetOneShot(epollfd,sockfd);//释放该文件，使得其他线程可以操作该文件
                break;
            }
        }
        else//如果在5s内又传来了新数据，那recv仍然能读取到数据，循环继续，当前线程不重置oenshot，继续霸占该连接符；
        {//如果5s内没传来数据，那recv就读不到数据，当前线程就会通过重置oneshot释放该连接符。当该连接符再次就绪时，所有可用线程都有机会霸占它
            printf("get content: %s\n",buf);
            sleep(5);//休眠5s，模拟数据处理过程。
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
    
    ret=listen(listenfd,10);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);
    
    epoll_event events[MAX_EVENT_NUMBER];//定义epoll_event结构体数组，用来存放epoll_wait检测到的就绪事件
    int epollfd=epoll_create(5);//通过epoll_create创建内核事件表文件描述符
    assert(epollfd!=-1);
    Addfd(epollfd,listenfd,false);//往内核事件表中加入监听描述符，监听该描述符的可读事件，监听描述符不能注册为oneshot事件
    
    while(1)
    {
        int ret=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if(ret<0)
        {
            printf("epoll failure\n");
            break;
        }
        //主线程负责监听事件，取出就绪事件，接受连接。工作线程负责处理已建立连接的可读事件
        for(int i=0;i<ret;i++)
        {
            int sockfd=events[i].data.fd;//对每个事件，先获得其对应的文件描述符，再按类型执行不同操作
            if(sockfd==listenfd)//如果是监听描述符上的可读事件就绪了，那就说明有新的连接请求，
            {//那就建立连接，并把连接描述符的可读放到内核事件表中
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                Addfd(epollfd,connection_fd,true);//将新的连接描述符设置为oneshot事件
            }
            else if(events[i].events & EPOLLIN)//如果不是监听描述符，也就只可能是连接描述符上的可读事件了
            {//那就新建一个线程去读取数据
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd=epollfd;//内核事件表文件描述符
                fds_for_new_worker.sockfd=sockfd;//连接描述符
                pthread_create(&thread,NULL,worker,(void*) &fds_for_new_worker);//创建线程，绑定要执行的函数，并将fds的地址传给worker
            }
            else
                printf("something else happened\n");
        }
    }
    
    close(listenfd);
    return 0;
}