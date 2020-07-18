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
#define BUFFER_SIZE 10

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回就得状态标志，以便日后能够恢复
}

void Addfd(int epollfd,int fd,bool enable_et)//往内核时间表中添加需要监听的文件描述符和可读事件，设置ET模式或LT模式
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN;//设置需要监听的事件：数据可读
    if(enable_et)
        event.events=event.events | EPOLLET;//设置为ET模式：同一就绪事件只会通知一次
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

void LT(epoll_event* events,int number,int epollfd,int listenfd)
{
    char buf[BUFFER_SIZE];
    for(int i=0;i<number;i++)//遍历每个就绪事件
    {
        int sockfd=events[i].data.fd;//对每个事件，先获得其对应的文件描述符，再按类型执行不同操作
        if(sockfd==listenfd)//如果是监听描述符上的可读事件就绪了，那就说明有新的连接请求，
        {//那就建立连接，并把连接描述符的可读放到内核事件表中
            struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
            socklen_t client_addr_length=sizeof(client_address);
            int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
            Addfd(epollfd,connection_fd,false);
            printf("new connction %d is created and put in epoll\n",connection_fd);
        }
        //LT模式的意思就是：只要某文件描述符上有可读数据，就一直会触发其EPOLLIN事件，
        //所以这次没有读完，可以放到下次来读
        else if(events[i].events & EPOLLIN)//如果不是监听描述符，也就只可能是连接描述符上的可读事件了
        {//那就接收客户端发送过来的数据
            printf("event trigger once\n");
            memset(buf,'\0',BUFFER_SIZE);
            int ret=recv(sockfd,buf,BUFFER_SIZE-1,0);//每次只读9个字节，超过了就要下次再读了
            if(ret<=0)//如果recv函数返回值为-1时则出错了：客户端关闭了连接，那就关闭该连接描述符
            {
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s\n",ret,buf);
        }
        else
            printf("something else happened\n");
    }
}

void ET(epoll_event* events,int number,int epollfd,int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i=0;i<number;i++)//遍历每个就绪事件
    {
        int sockfd=events[i].data.fd;//对每个事件，先获得其对应的文件描述符，再按类型执行不同操作
        if(sockfd==listenfd)//如果是监听描述符上的可读事件就绪了，那就说明有新的连接请求，
        {//那就建立连接，并把连接描述符的可读放到内核事件表中
            struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
            socklen_t client_addr_length=sizeof(client_address);
            int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
            Addfd(epollfd,connection_fd,true);
            printf("new connction %d is created and put in epoll\n",connection_fd);
        }
        //ET模式只会触发一次的意思就是，直到此次数据读完之前，不会再第二次通知数据可读。
        //所以需要循环读取数据，保证把所有数据读完
        else if(events[i].events & EPOLLIN)//如果不是监听描述符，也就只可能是连接描述符上的可读事件了
        {//那就接收客户端发送过来的数据
            printf("event trigger once\n");
            while(1)
            {
                memset(buf,'\0',BUFFER_SIZE);
                int ret=recv(sockfd,buf,BUFFER_SIZE-1,0);
                if(ret<0)
                {
                    if((errno==EAGAIN) || (errno==EWOULDBLOCK))//对于非阻塞IO，该错误编号表示已经读取完毕
                    {//读取完毕，当该文件描述符又有数据可读时，epoll会再次触发该文件描述符上的EPOLLIN事件
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if(ret==0)//recv函数返回0，表示对方关闭了连接
                    close(sockfd);
                else
                    printf("get %d bytes of content: %s\n",ret,buf);
            }
        }
        else
            printf("something else happened\n");
    }
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
    
    epoll_event events[MAX_EVENT_NUMBER];//定义epoll_event结构体数组，用来存放epoll_wait检测到的就绪事件
    int epollfd=epoll_create(5);//通过epoll_create创建内核事件表文件描述符
    assert(epollfd!=-1);
    Addfd(epollfd,listenfd,true);//往内核事件表中加入监听描述符，监听该描述符的可读事件：
    
    while(1)
    {
        //ret存放函数返回的就绪事件的个数
        int ret=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);//等待内核事件表中的事件发生，时间设为-1：永远阻塞直到事件发生，
        if(ret<0)//因为设置为永远阻塞，那返回时肯定有就绪事件，否则就有问题
        {
            printf("epoll failure\n");
            break;
        }
        //LT模式，一次没读完，函数就返回到while循环，然后epoll检测到该文件描述符上还有数据可读，
        //又会触发EPOLLIN事件，然后调用LT函数再去读
        //LT(events,ret,epollfd,listenfd);
        //ET模式，在ET函数中若没有while循环一直把数据读完，函数返回后，即使该文件描述符还有数据没读完，
        //也不会触发EPOLLIN事件，所以必须加循环，不加这些数据就可能没了。然后，当这一批的数据读完，下一批
        //的数据来到时，才会触发EPOLLIN事件。
        //作者说ET可以减少触发次数，但是在LT中也加个循环把数据读完，不也可以实现只触发一次的效果吗？
        ET(events,ret,epollfd,listenfd);//神奇，我用自己g++的可执行文件执行，客户端就连接被拒。用vscode调试，或者用调试生成的可执行文件，就能连接上
    }
    
    close(listenfd);
    return 0;
}