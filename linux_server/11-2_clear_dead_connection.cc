//处理非活动连接
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
#include "11-2_lst_timer.h"

#define FD_LIMIT 65535//连接描述的限制
#define MAX_EVENT_NUMBER 1024//监听事件熟练限制
#define TIMESLOT 5//

static int pipefd[2];
static SortTimerList timer_lst;//定时器容器
static int epollfd=0;

int SetNonBlocking(int fd)//将文件描述符设为非阻塞状态
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= EPOLLIN | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

void Addfd(int epollfd,int fd)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET;//设置为ET模式：同一就绪事件只会通知一次
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

void SigHandler(int sig)//信号处理函数，把接收到的信号写到管道中
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

void TimerHandler()
{
    timer_lst.Tick();//处理到期的定时器
    alarm(TIMESLOT);//重新设置闹钟，在TIMESLOT时间后，系统将向进程发出SIGALRM信号
}

void cb_func(client_data* user_data)//定时器的回调函数
{//当一个定时器到期时，就取消监听该定时器对应的连接描述符上的事件，并关闭该连接
    epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);//在内核事件表删除该连接描述符
    assert(user_data);
    close(user_data->sockfd);//关闭该连接
    printf("close dead connection with fd %d \n",user_data->sockfd);
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
    SetNonBlocking(pipefd[1]);//将管道1端设为非阻塞，要是管道满了，就不写了，要写的内容就丢失了
    Addfd(epollfd,pipefd[0]);//将管道0端描述符加入到内核事件表中，监听管道的可读事件。（事件上两头都可读，这里加入fd[0]而不是fd[1],是因为在信号处理函数中，是往fd[1]中写入信号值，也就是说把双向管道当成单向来用了

    AddSig(SIGALRM);//两个信号的信号处理函数都是SigHandler：把信号值写入管道
    AddSig(SIGTERM);
    bool stop_server=false;

    client_data* users=new client_data[FD_LIMIT];//先定义好客户端数据结构体数组，按连接描述符索引
    bool timeout =false;//通过该标志位来判断是否收到SIGALRM信号
    alarm(TIMESLOT);//设置闹钟

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

                users[connection_fd].address=client_address;//更新该连接描述符对应的客户端数据
                users[connection_fd].sockfd=connection_fd;
                UtilTimer* timer=new UtilTimer;//为该连接新建定时器节点
                timer->user_data=&users[connection_fd];//将定时器与该连接绑定
                timer->cb_func=cb_func;//设置定时器到期后要调用的函数，也就是说在定时器刚创建时它的回调函数和要用到的客户端数据就被保存好了，而在连接刚创建时，定时器也就创建好了
                time_t cur=time(nullptr);
                timer->expire=cur+3*TIMESLOT;//设置闹钟，该定时器将在3*TIMESLOT后到期，届时将关闭该连接
                users[connection_fd].timer=timer;
                timer_lst.AddTimer(timer);//将该定时器节点加到链表中
            }
            else if((sockfd==pipefd[0]) && (events[i].events & EPOLLIN))
            {//如果是管道可读事件就绪，就读取管道中的信号，处理信号
                int sig;
                char signals[1024];
                ret=recv(pipefd[0],signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
                if(ret==-1)
                    continue;
                else if(ret==0)
                    continue;
                else
                {//按顺序处理每个信号
                    for(int i=0;i<ret;++i)//每个信号占一字节，可以直接按字节来处理信号
                    {
                        switch (signals[i])
                        {
                            case SIGALRM://对于SIGALRM信号，不是马上处理，只是设置超市标志。等所有信号处理完，所有就绪事件处理完，才处理定时任务
                            {
                                timeout=true;
                                break;
                            }
                            case SIGTERM://终止进程
                                stop_server=true;//设置标志位
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN)
            {
                memset(users[sockfd].buf,'\0',BUFFER_SIZE);
                ret=recv(sockfd,users[sockfd].buf,BUFFER_SIZE-1,0);
                printf("get %d bytes of client data %s from %d\n",ret,users[sockfd].buf,sockfd);
                UtilTimer* timer=users[sockfd].timer;//取出该连接描述符对应的计时器指针
                if(ret<0)
                {
                    if(errno!=EAGAIN)//如果发生错误，则在内核事件表中删除该连接描述符，关闭连接，并删除链表中的定时器节点
                    {
                        cb_func(&users[sockfd]);//回调函数并不会删除定时器节点
                        if(timer)
                            timer_lst.DeleteTimer(timer);
                    }
                }
                else if(ret==0)//recv的返回值为0时，表明对方关闭了连接
                {
                    cb_func(&users[sockfd]);
                    if(timer)
                        timer_lst.DeleteTimer(timer);
                }
                else//读取到了数据，说明该连接是活动的，需要重新设置闹钟
                {
                    if(timer)
                    {
                        time_t cur=time(nullptr);
                        timer->expire=cur+3*TIMESLOT;//修改定时器的终止时间
                        printf("adjust timer once\n");
                        timer_lst.AdjustTimer(timer);//调整该定时器在链表中的位置
                    }
                }
            }
            else 
            {

            }
        }
        if(timeout)//等所有就绪事件处理完毕，所有信号处理完毕，再处理定时事件
        {//此事件处理函数与信号的处理函数内涵相同，都是出现某个标志时就干嘛。不同的是此处理函数标志由用户设置，调用也由用户调用。而信号处理函数，则是内核给出信号，内核调用信号处理函数
            TimerHandler();//每过一个周期，就调用一次定时事件的处理函数：处理到期的定时器，重置闹钟
            timeout=false;//把标志位复位
        }
    }
    close(listenfd);
    close(pipefd[1]);//先关闭写端。因为往读端关闭的管道写数据是会报错的
    close(pipefd[0]);
    delete []users;
    return 0;
}