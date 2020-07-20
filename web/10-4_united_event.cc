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
#include <signal.h>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

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

void SigHandler(int sig)//信号处理函数
{
    int save_errno=errno;//暂存原来的errno，在函数最后恢复，保证函数的可重入性
    int msg=sig;
    send(pipefd[1],(char*) &msg,1,0);//将信号值写入管道中，那么管道的可读事件将就绪，会触发epoll
    errno=save_errno;
}

void AddSig(int sig)//设置给定信号对应的信号处理函数
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=SigHandler;//本程序中所有指定信号的信号处理函数都是上面这个，即将信号值写入管道
    sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr) != -1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
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

    epoll_event events[MAX_EVENT_NUMBER];//存放epoll返回的就绪事件
    int epollfd=epoll_create(5);//创建内核事件表描述符
    assert(epollfd!=-1);
    Addfd(epollfd,listenfd);//将监听描述符加入到内核事件表中

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);//创建socket管道，双向的，两头都可读可写
    assert(ret!=-1);
    SetNonBlocking(pipefd[1]);//将管道1端设为非阻塞
    Addfd(epollfd,pipefd[0]);//将管道0端描述符加入到内核事件表中，监听管道的可读事件。（事件上两头都可读，这里加入fd[0]而不是fd[1],是因为在信号处理函数中，是往fd[1]中写入信号值，也就是说把双向管道当成单向来用了

    AddSig(SIGHUP);//为这些信号设置信号处理函数，都是在捕获到信号时，把信号值写到管道中
    AddSig(SIGCHLD);
    AddSig(SIGTERM);
    AddSig(SIGINT);
    bool stop_server=false;

    while(!stop_server)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)
        {
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(sockfd==listenfd)
            {
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                Addfd(epollfd,connection_fd);
            }
            else if((sockfd==pipefd[0]) && (events[i].events & EPOLLIN))//如果是管道0端的可读事件就绪了
            {//就处理信号
                int sig;
                char signals[1024];
                ret=recv(pipefd[0],signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
                if(ret==-1)
                    continue;
                else if(ret==0)
                    continue;
                else
                {
                    for(int i=0;i<ret;++i)//每个信号占一字节，可以直接按字节来处理信号
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD://子进程退出或暂停
                            case SIGHUP://控制终端被挂起
                            {
                                continue;
                            }
                            case SIGTERM://终止进程
                            case SIGINT://键盘输入以终止进程
                                stop_server=true;//设置标志位
                        }
                    }
                }
            }
            else
            {

            }
        }
    }

    printf("close fds\n");
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    return 0;
}

