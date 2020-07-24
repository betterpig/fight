#ifndef PROCESS_POOL_H
#define PROCESS_POOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

class Process
{
public:
    pid_t m_pid;
    int m_pipefd[2];

    Process():m_pid(-1) {}
};

template<typename T>
class ProcessPool
{
private:
    static const int MAX_PROCESS_NUMBER=16;
    static const int USER_PRE_PROCESS=65536;
    static const int MAX_EVENT_NUMBER=10000;
    int m_process_number;
    int m_idx;
    int m_epollfd;
    int m_listenfd;
    int m_stop;
    Process* m_sub_process;
    static ProcessPool<T>* m_instance;
    ProcessPool(int listenfd,int process_number=8);
    void SetupSigPipe();
    void RunParent();
    void RunChild();
public:
    static ProcessPool<T>* Create(int listenfd,int process_number=8)
    {
        if(!m_instance)
            m_instance=new ProcessPool<T> (listenfd,process_number);
        return m_instance;
    }
    ~ProcessPool()
    {
        delete []m_sub_process;
    }
    void Run();
};

template<typename T>
ProcessPool<T>* ProcessPool<T>::m_instance=nullptr;

static int sig_pipefd[2];

static int SetNonBlocking(int fd)//将文件描述符设为非阻塞状态
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

static void Addfd(int epollfd,int fd)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET;//设置为ET模式：同一就绪事件只会通知一次
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

static void Removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

static void SigHandler(int sig)//信号处理函数，把接收到的信号写到管道中
{
    int save_errno=errno;//暂存原来的errno，在函数最后恢复，保证函数的可重入性
    int msg=sig;
    send(sig_pipefd[1],(char*) &msg,1,0);//将信号值写入管道中，那么管道的可读事件将就绪，会触发epoll
    errno=save_errno;
}

static void AddSig(int sig,void (handler) (int),bool restart=true)//设置给定信号对应的信号处理函数
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;//本程序中所有指定信号的信号处理函数都是上面这个，即将信号值写入管道
    if(restart)
        sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr) != -1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
}

template<typename T>
ProcessPool<T>::ProcessPool(int listenfd,int process_number)
:m_listenfd(listenfd),m_process_number(process_number),m_idx(-1),m_stop(false)
{
    assert((process_number>0) && (process_number<=MAX_PROCESS_NUMBER));
    m_sub_process=new Process[process_number];
    assert(m_sub_processl);
    for(int i=0;i<process_number;++i)
    {
        int ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
        assert(ret==0);
        m_sub_process[i].m_pid=fork();
        assert(m_sub_process[i].m_pid>=0);
        if(m_sub_process[i].m_pid>0)
        {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx=i;
            break;
        }
    }
}

template<typename T>
void ProcessPool<T>::SetupSigPipe()
{
    m_epollfd=epoll_create(5);
    assert(m_epollfd!=-1);
    int ret=socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret!=-1);
    SetNonBlocking(sig_pipefd[1]);
    Addfd(m_epollfd,sig_pipefd[0]);

    AddSig(SIGCHLD,SigHandler);
    AddSig(SIGTERM,SigHandler);
    AddSig(SIGINT,SigHandler);
    AddSig(SIGPIPE,SIG_IGN);
}

template<typename T>
void ProcessPool<T>::RunChild()
{
    SetupSigPipe();
    int pipefd=m_sub_process[m_idx].m_pipefd[1];
    AddSig(m_epollfd,pipefd);
    epoll_event events[MAX_EVENT_NUMBER];
    T* users=new T[USER_PRE_PROCESS];
    assert(users);
    int number=0;
    int ret=-1;

    while(!m_stop)
    {
        number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<number;i++)
        {
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if((sockfd==pipefd) && (events[i].events & EPOLLIN))
            {//如果是管道可读事件就绪，就读取管道中的信号，处理信号
                int client=0;
                ret=recv(pipefd[0],(char*) &client,sizeof(client),0);//把管道中的内容读到字符数组signals中
                if( ((ret<0) && (errno!=EAGAIN)) || ret==0)
                    continue;
                else
                {
                    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                    socklen_t client_addr_length=sizeof(client_address);
                    int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                    if(connection_fd<0)
                    {
                        printf("errno is : %d\n",errno);
                        continue;
                    }
                    Addfd(m_epollfd,connection_fd);
                    users[connection_fd].init(m_epollfd,connection_fd,client_address);
                }
            }
            else if((sockfd==pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret=recv(pipefd[0],signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
                if(ret<=0)
                    continue;
                else
                {//按顺序处理每个信号
                    for(int i=0;i<ret;++i)//每个信号占一字节，可以直接按字节来处理信号
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD://对于SIGALRM信号，不是马上处理，只是设置超市标志。等所有信号处理完，所有就绪事件处理完，才处理定时任务
                            {
                                pid_t pid;
                                int stat;
                                while( (pid=waitpid(-1,&stat,WNOHANG))>0)
                                    continue;
                                break;
                            }
                            case SIGTERM://终止进程
                            case SIGINT:
                            {
                                m_stop=true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN)
            {
                users[sockfd].Process();
            }
            else 
            {
                continue;
            }
        }
    }

    delete []users;
    users=nullptr;
    close(pipefd);
    //close(m_listenfd);
    close(m_epollfd);
}

template<typename T>
void ProcessPool<T>:: RunParent()
{
    SetupSigPipe()
    Addfd(m_epollfd,m_listenfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter=0;
    int new_conn=1;
    int number=0;
    int ret=-1;

    while(!m_stop)
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
            if(sockfd==m_listenfd)
            {
                int i=sub_process_counter;
                do
                {
                    if(m_sub_process[i].m_pid!=-1)
                        break;
                    i=(i+1) % m_process_number;
                }
                while(i!=sub_process_counter);
                if(m_sub_procees[i].m_pid==-1)
                {
                    m_stop=true;
                    break;
                }
                sub_process_counter= (i+1) % m_process_number;
                send(m_sub_process[i].m_pipefd[0].(char*) &new_conn,sizeof(new_conn),0);
                printf("send request to child %d\n",i);
            }
            else if((sockfd==pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret=recv(pipefd[0],signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
                if(ret<=0)
                    continue;
                else
                {//按顺序处理每个信号
                    for(int i=0;i<ret;++i)//每个信号占一字节，可以直接按字节来处理信号
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD://对于SIGALRM信号，不是马上处理，只是设置超市标志。等所有信号处理完，所有就绪事件处理完，才处理定时任务
                            {
                                pid_t pid;
                                int stat;
                                while( (pid=waitpid(-1,&stat,WNOHANG))>0)
                                {
                                    for(int=0;i<m_process_number;++i)
                                    {
                                        if(m_sub_process[i].m_pid==pid)
                                        {
                                            printf("child %d join\n",i);
                                            close(m_sub_process[i].m_pipefd[0]);
                                            m_sub_process[i].m_pid=-1;
                                        }
                                    }
                                }
                                m_stop=true;
                                for(int i=0;i<m_process_number;++i)
                                {
                                    if(m_sub_process[i].m_pid!=-1)
                                        m_stop=false;
                                }
                                break;
                            }
                            case SIGTERM://终止进程
                            case SIGINT:
                            {
                                printf("kill all the child now\n");
                                for(int i=0;i<m_process_number;++i)
                                {
                                    int pid=m_sub_process[i].m_pid;
                                    if(pid!=-1)
                                        kill(pid,SIGTERM);
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
                else 
                    continue;
            }
    }

    //close(m_listenfd);
    close(m_epollfdl);
}

#endif