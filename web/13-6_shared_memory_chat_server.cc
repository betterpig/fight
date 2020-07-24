#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#define USER_LIMIT 5    //客户编号数量
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535  //文件描述符数量
#define MAX_EVENT_NUMBER 1024   //事件数量
#define PROCESS_LIMIT 65536 //进程数量

struct client_data//保存客户端地址，连接描述符，处理该连接的子进程id，和父进程通信用的管道描述符
{
    sockaddr_in address;
    int connfd;
    pid_t pid;
    int pipefd[2];
};

static const char* shm_name="/my_shm";//共享内存的名字
int sig_pipefd[2];//主进程的信号管道
int epollfd;//主进程内核事件表
int listenfd;//主进程监听描述符
int shmfd;//共享内存描述符
char* share_mem=0;//
client_data* users=0;//客户端数据数组，通过连接描述符来索引
int* sub_process=0;//子进程数组，通过进程PID来索引，每个连接对应一个子进程，保存的是进程对应的客户编号
int user_count=0;//当前客户数量
bool stop_child=false;

int SetNonBlocking(int fd)//将文件描述符设为非阻塞状态
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
    event.events=EPOLLIN | EPOLLET;//设置为ET模式：同一就绪事件只会通知一次
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

void SigHandler(int sig)//信号处理函数，把接收到的信号写到管道中
{
    int save_errno=errno;//暂存原来的errno，在函数最后恢复，保证函数的可重入性
    int msg=sig;
    send(sig_pipefd[1],(char*) &msg,1,0);//将信号值写入管道中，那么管道的可读事件将就绪，会触发epoll
    errno=save_errno;
}

void AddSig(int sig,void (*handler) (int),bool restart=true)//设置给定信号对应的信号处理函数
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;//本程序中所有指定信号的信号处理函数都是上面这个，即将信号值写入管道
    if(restart)
        sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr) != -1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
}

void DeleteResource()//关闭主进程打开的资源
{
    close(sig_pipefd[0]);//信号管道
    close(sig_pipefd[1]);
    close(listenfd);//监听描述符
    close(epollfd);//内核事件表
    shm_unlink(shm_name);//将该共享内存对象标记为等待删除，当所有使用它的进程都将其分离后，系统将销毁该共享内存
    delete []users;//删除创建的动态数组
    delete []sub_process;
}

void ChildTermHandler(int sig)
{
    stop_child=true;//终止某个子进程
}

int RunChild(int idx,client_data* users,char* share_mem)//给定客户编号，客户数组和共享内存的起始地址
{
    epoll_event events[MAX_EVENT_NUMBER];
    int child_epollfd=epoll_create(5);//子进程也有IO复用来监听多个文件描述符：客户连接描述符，与父进程通信的管道描述符
    assert(child_epollfd!=-1);
    int connfd=users[idx].connfd;//根据客户编号获得本子进程负责处理的客户端，包括：连接描述符、管道描述符
    Addfd(child_epollfd,connfd);//将连接描述符和管道描述符加入到子进程内核事件表中
    int pipefd=users[idx].pipefd[1];//管道的一端，可读可写
    Addfd(child_epollfd,pipefd);
    int ret;
    AddSig(SIGTERM,ChildTermHandler,false);//设置SIGTERM信号的处理函数：当收到信号时，把stop_child标志位设为true，不需要重启被信号中断的系统调用

    while(!stop_child)
    {
        int number=epoll_wait(child_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)
        {//就绪事件要么是连接描述符上的可读事件，要么是与父进程通信的管道描述符上的事件
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(sockfd==connfd)
            {//把共享内存分成若干连续的块，每个客户占用其中一块，写时独占，读时共享（即可以读其他客户占用的那部分内存）
                memset(share_mem+idx*BUFFER_SIZE,'\0',BUFFER_SIZE);
                ret=recv(connfd,share_mem+idx*BUFFER_SIZE,BUFFER_SIZE-1,0);//把从客户发来的消息写到自己占有的那块内存上，独占写
                if(ret<0)
                {
                    if(errno!=EAGAIN)
                        stop_child=true;
                }
                else if(ret==0)
                    stop_child=true;
                else//这里把客户编号从int变成了char类型，并写到管道上：告诉主进程，哪个客户发来了数据
                    send(pipefd,(char*) &idx,sizeof(idx),0);
            }
            else if((sockfd==pipefd) && (events[i].events & EPOLLIN))
            {//主进程通知本子进程，将共享内存中的哪个客户发来的数据发送给本子进程负责处理的客户连接上
                int client=0;
                ret=recv(sockfd,(char*) &client,sizeof(client),0);//把管道中的内容：客户编号读到client上，就知道是哪个客户发出了消息
                if(ret<0)
                {
                    if(errno!=EAGAIN)
                        stop_child=true;
                }
                else if(ret==0)
                    stop_child=true;
                else//将上面获得的客户编号，对应的那块共享内存上的数据，发送到本子进程负责的客户连接上
                    send(connfd,share_mem+client*BUFFER_SIZE,BUFFER_SIZE,0);//所以，除了发消息那个客户，其他客户都会读它的内存，共享读
            }
            else
            {
                continue;
            }

        }
    }
    close(connfd);//子进程退出时，先释放各种资源
    close(pipefd);//先关闭写端。因为往读端关闭的管道写数据是会报错的
    close(child_epollfd);
    delete []users;
    return 0;
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
    listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);

    int ret=bind(listenfd,(struct sockaddr*) &server_address,sizeof(server_address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);
    
    ret=listen(listenfd,10);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];//存放epoll返回的就绪事件
    epollfd=epoll_create(5);//创建内核事件表描述符
    assert(epollfd!=-1);
    Addfd(epollfd,listenfd);//将监听描述符加入到内核事件表中

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);//创建socket管道，双向的，两头都可读可写
    assert(ret!=-1);
    SetNonBlocking(sig_pipefd[1]);//将管道1端设为非阻塞，要是管道满了，就不写了，要写的内容就丢失了
    Addfd(epollfd,sig_pipefd[0]);//将管道0端描述符加入到内核事件表中，监听管道的可读事件。（事件上两头都可读，这里加入fd[0]而不是fd[1],是因为在信号处理函数中，是往fd[1]中写入信号值，也就是说把双向管道当成单向来用了

    AddSig(SIGCHLD,SigHandler);//统一事件源：把信号值写入管道
    AddSig(SIGTERM,SigHandler);
    AddSig(SIGINT,SigHandler);
    AddSig(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
    bool stop_server=false;
    bool terminate=false;

    users=new client_data[FD_LIMIT];//先定义好客户端数据结构体数组，按连接描述符索引
    sub_process=new int[PROCESS_LIMIT];//子进程数组，按子进程pid索引
    for(int i=0;i<PROCESS_LIMIT;++i)
        sub_process[i]=-1;
    
    shmfd=shm_open(shm_name,O_CREAT|O_RDWR,0666);//给定名字、标志（可读可写、创建）和模式创建共享内存，作为所有客户连接的读缓存
    assert(shmfd!=-1);
    ret=ftruncate(shmfd,USER_LIMIT*BUFFER_SIZE);//将该共享内存的大小改为客户数量×缓冲区尺寸那么大，每个客户占一个缓冲区
    assert(ret!=-1);
    //将创建的共享内存对象shmfd与实际的内存关联起来，返回该块内存的首地址。指定了内存大小，可读可写，在进程间共享，已经要关联的共享内存对象（虚拟文件），文件偏移量
    share_mem=(char*) mmap(nullptr,USER_LIMIT*BUFFER_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,shmfd,0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);//关闭共享内存对象的文件描述符？

    while(!stop_server)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)
        {//三种就绪事件：监听描述符上可读，信号管道上可读，父子进程管道可读
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(sockfd==listenfd)
            {
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                //Addfd(epollfd,connection_fd);//主进程不再负责监听连接描述符
                if(connection_fd<0)
                {
                    printf("errno is :%d\n",errno);
                    continue;
                }
                if(user_count>=USER_LIMIT)//如果已经有5个客户连接了
                {
                    const char* info="too many users\n";
                    printf("%s",info);//在服务端打印
                    send(connection_fd,info,strlen(info),0);//并且把该信息发送给客户端
                    close(connection_fd);//然后关闭该连接
                    continue;//所以其实还是可以连接的，也连接成功了，只是服务器认为已经超过了承载能力，主动将该连接关闭了
                }
                users[user_count].address=client_address;//更新该连接描述符对应的客户端数据
                users[user_count].connfd=connection_fd;//创建的管道文件描述符放在了客户数据数组中对应的客户编号内
                ret=socketpair(PF_UNIX,SOCK_STREAM,0,users[user_count].pipefd);//创建全双工通信的管道，在父子进程间通信：就是当某个客户发过来数据时，客户通过该管道告诉父进程是哪个客户发来的数据，然后父进程通过该管道，告诉其他子进程是哪个客户发来的数据
                assert(ret!=-1);
                pid_t pid=fork();//创建子进程
                if(pid<0)//创建失败，当前在父进程
                {
                    close(connection_fd);
                    continue;
                }
                else if(pid==0)//进入子进程
                {
                    close(epollfd);//先关闭父进程的相关资源：内核事件表
                    close(listenfd);//连接描述符（使其引用计数减一）
                    close(users[user_count].pipefd[0]);//子进程需要关闭管道的父进程这一端
                    close(sig_pipefd[0]);//关闭父进程的信号管道
                    close(sig_pipefd[1]);
                    RunChild(user_count,users,share_mem);//运行子进程的函数，把客户数组传进去
                    munmap( (void*) share_mem,USER_LIMIT*BUFFER_SIZE);//子进程函数运行完后，该子进程将与共享内存剥离，共享内存的引用计数减一
                    exit(0);//子进程退出
                }
                else//进入主进程
                {
                    close(connection_fd);//使子进程监听的连接描述符引用计数减一
                    close(users[user_count].pipefd[1]);//关闭父子管道的子进程这一端
                    Addfd(epollfd,users[user_count].pipefd[0]);//监听父子管道的自己这一端
                    users[user_count].pid=pid;//记录负责该客户的子进程id。按照写时复制规则，此时子进程先复制一个客户数组，然后父进程再修改客户数组，现在两个客户数组已经没有关联了
                    sub_process[pid]=user_count;//记录该子进负责的客户编号
                    user_count++;
                }
            }
            else if((sockfd==sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {//如果是管道可读事件就绪，就读取管道中的信号，处理信号
                int sig;
                char signals[1024];
                ret=recv(sig_pipefd[0],signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
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
                            case SIGCHLD://处理子进程退出信号：客户关闭了连接
                            {
                                pid_t pid;
                                int stat;
                                while( (pid=waitpid(-1,&stat,WNOHANG))>0 )//获得退出子进程的pid
                                {
                                    int del_user=sub_process[pid];//获得子进程对应的客户编号
                                    sub_process[pid]=-1;
                                    if( (del_user<0) || (del_user>USER_LIMIT))
                                        continue;
                                    epoll_ctl(epollfd,EPOLL_CTL_DEL,users[del_user].pipefd[0],0);//现在主进程不需要在内核事件表中监听父子进程管道的父进程这一端了
                                    close(users[del_user].pipefd[0]);//关闭父子管道的父进程这一端
                                    users[del_user]=users[user_count--];//这里书上的前置--有问题，我觉得应该是后置--
                                    //比如有4个客户：0 1 2 3，当客户1关闭时，则把客户3的内容放到客户1上，然后客户数量--，下次循环遍历时就只会遍历 0 1 2
                                    sub_process[users[del_user].pid]=del_user;//更新子进程数组的客户编号
                                    //在前一步中，客户1已经是客户3的内容了，那就要按照客户三的子进程pid索引子进程数组，把原来客户3的子进程对应的客户编号从3改成1，而1就是被关闭的客户
                                }
                                if(terminate && user_count==0)//当主进程收到了SIGTERM信号，且把所有子进程都给退出后
                                    stop_server=true;//才关闭服务器
                                break;
                            }
                            case SIGTERM://终止进程
                            case SIGINT:
                            {
                                printf("kill all the child now\n");
                                if(user_count==0)//如果没有客户，那直接关就好了
                                {
                                    stop_server=true;
                                    break;//这个break只会跳出switch，并没有跳出对信号的for循环和对事件的for循环，为什么不直接跳出到最外面的while循环呢
                                }
                                for(int i=0;i<user_count;++i)
                                {
                                    int pid=users[i].pid;
                                    kill(pid,SIGTERM);//给每个子进程发送SIGTERM信号，通知其关闭
                                }
                                terminate=true;//父进程已经通知所有子进程关闭了，接下来就是在前一个case中，等待子进程真正退出了
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN)//父子进程管道的父进程这一端可读
            {
                int client_num=0;
                ret=recv(sockfd,(char*) &client_num,sizeof(client_num),0);//获得是哪个客户连接收到了客户发来的数据
                printf("connection%d get client data \n",client_num);
                if(ret<0)
                    continue;
                else if(ret==0)//recv的返回值为0时，表明对方关闭了连接
                    continue;
                else
                {
                   for(int j=0;j<user_count;++j)//通知其他客户，是哪个编号的客户连接收到了客户发送的数据
                    {//客户数组保存了所有的父子管道（共5对）中的父进程这一端
                        if(users[j].pipefd[0]!=sockfd)//对除了收到客户数据的那个客户连接
                        {
                            printf("parent tell other connect%d to send data to client\n",j);
                            send(users[j].pipefd[0],(char*) &client_num,sizeof(client_num),0);
                            //往其他客户编号对应的父子管道的父进程这一端写数据，告诉其他客户，是哪个客户连接收到了数据，然后其他客户就会将该客户对应的共享内存上的数据，发送给本客户连接的连接描述符
                        }
                    }
                }
            }
        }
    }
    DeleteResource();//主进程退出前释放各种资源
    return 0;
}
    