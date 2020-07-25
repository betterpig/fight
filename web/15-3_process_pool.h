#ifndef PROCESS_POOL_H
#define PROCESS_POOL_H

class Process
{
public:
    pid_t m_pid;//子进程pid
    int m_pipefd[2];//父子管道

    Process():m_pid(-1) {}
};

template<typename T>
class ProcessPool
{
private:
    static const int MAX_PROCESS_NUMBER=16;//最大进程数量
    static const int USER_PER_PROCESS=5;//每个进程的最大客户数量
    static const int MAX_EVENT_NUMBER=10000;//最大事件数量
    int m_process_number;//当前进程数量
    int m_idx;//当前进程id
    int m_epollfd;//当前进程的内核事件表描述符
    int m_listenfd;//当前进程的监听描述符
    int m_stop;//当前进程是否结束的标志
    Process* m_sub_process;//所有子进程的信息
    static ProcessPool<T>* m_instance;//静态进程池实例:只允许生成一个进程池对象
    ProcessPool(int listenfd,int process_number=8);//将构造函数定义为私有的，将只能通过create函数创建对象
    void SetupSigPipe();
    void RunParent();
    void RunChild();
public:
    static ProcessPool<T>* Create(int listenfd,int process_number=8)//创建进程池对象
    {
        if(!m_instance)//如果还未创建过进程池类实例，那么创建。m_instance的static属性，保证它只会被定义一次。即使有多个进程同时进入了这段程序
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
ProcessPool<T>* ProcessPool<T>::m_instance=nullptr;//类的静态成员要在类外定义

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

static void Removefd(int epollfd,int fd)//将文件描述符fd从内核事件表中移除
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
    m_sub_process=new Process[process_number];//子进程信息数组
    assert(m_sub_process);
    for(int i=0;i<process_number;++i)//创建多个子进程
    {
        int ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);//为每一对父子进程创建父子管道
        assert(ret==0);
        m_sub_process[i].m_pid=fork();//创建子进程，把子进程的id记录在子进程信息数组中
        assert(m_sub_process[i].m_pid>=0);
        if(m_sub_process[i].m_pid>0)
        {
            close(m_sub_process[i].m_pipefd[1]);//在父进程中，关闭父子管道中的子进程这一端
            continue;//父进程才需要不断循环创建子进程
        }
        else
        {
            close(m_sub_process[i].m_pipefd[0]);//在子进程中，关闭父子管道中的父进程这一端
            m_idx=i;//子进程的id才被赋值了，而父进程id没有被赋值，一直是初始值-1
            break;//子进程运行到这里，就不用再执行循环了，直接跳出，结束函数。
        }
    }
}

template<typename T>
void ProcessPool<T>::SetupSigPipe()//建立信号管道
{
    m_epollfd=epoll_create(5);//创建内核事件表
    assert(m_epollfd!=-1);
    int ret=socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);//创建信号管道
    assert(ret!=-1);
    SetNonBlocking(sig_pipefd[1]);//将信号管道设为非阻塞
    Addfd(m_epollfd,sig_pipefd[0]);//监听信号管道

    AddSig(SIGCHLD,SigHandler);//为信号设置处理函数
    AddSig(SIGTERM,SigHandler);
    AddSig(SIGINT,SigHandler);
    AddSig(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
}

template<typename T>
void ProcessPool<T>::Run()
{
    if(m_idx!=-1)
    {
        RunChild();
        return;
    }
    RunParent();
}

template<typename T>
void ProcessPool<T>::RunChild()
{
    SetupSigPipe();//子进程先建立内核事件表，并监听信号管道描述符
    int pipefd=m_sub_process[m_idx].m_pipefd[1];
    Addfd(m_epollfd,pipefd);//监听父子管道中的子进程这一端
    epoll_event events[MAX_EVENT_NUMBER];//用来存放epoll返回的就绪事件
    T* users=new T[USER_PER_PROCESS];//保存客户端数据，这是一个保存T类型对象的数组
    assert(users);
    int number=0;
    int ret=-1;

    while(!m_stop)//每个子进程就相当于之前程序范例中的主进程，每个子进程都要负责处理三种就绪事件：新连接到来、捕获到信号、读取已有连接的数据
    {
        number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<number;i++)//子进程负责两类就绪事件：父子管道可读->接受新的连接、信号管道可读->处理信号、
        {//第三种就绪事件：连接描述符可读，是由客户连接对象处理
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if((sockfd==pipefd) && (events[i].events & EPOLLIN))//父子管道可读
            {//父子管道可读，这是父进程在通知子进程，有新连接到来，让子进程接受该连接，也就是说父进程只是通知，不负责接受新的连接
                int client=0;//这里客户编号对子进程没有作用，只是表明有或者没有数据
                ret=recv(pipefd,(char*) &client,sizeof(client),0);//把管道中的内容读到字符数组signals中
                if( ((ret<0) && (errno!=EAGAIN)) || ret==0)
                    continue;
                else
                {
                    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                    socklen_t client_addr_length=sizeof(client_address);
                    int connection_fd=accept(m_listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                    if(connection_fd<0)
                    {
                        printf("errno is : %d\n",errno);
                        continue;
                    }
                    Addfd(m_epollfd,connection_fd);//子进程监听连接描述符
                    //模板类T需要实现自己的init方法，以初始化本类对象
                    users[connection_fd].init(m_epollfd,connection_fd,client_address);//子进程保存客户端数据
                }
            }
            else if((sockfd==sig_pipefd[0]) && (events[i].events & EPOLLIN))//信号管道可读
            {
                int sig;
                char signals[1024];
                ret=recv(pipefd,signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
                if(ret<=0)
                    continue;
                else
                {//按顺序处理每个信号
                    for(int i=0;i<ret;++i)//每个信号占一字节，可以直接按字节来处理信号
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD://为什么子进程会收到SIGCHLD信号？因为客户对象会创建子程序来运行客户要运行的文件
                            {
                                pid_t pid;
                                int stat;
                                while( (pid=waitpid(-1,&stat,WNOHANG))>0)
                                    continue;
                                break;
                            }
                            case SIGTERM://终止进程，收到这两个信号时，结束本子进程
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
            else if(events[i].events & EPOLLIN)//连接描述符可读
            {
                users[sockfd].Process();//调用逻辑处理对象的process方法处理客户发送过来的请求
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
    //close(m_listenfd);//listenfd应该由其创建者销毁
    close(m_epollfd);
}

template<typename T>
void ProcessPool<T>:: RunParent()
{
    SetupSigPipe();//父进程创建信号管道
    Addfd(m_epollfd,m_listenfd);//父进程监听每个子进程的“监听描述符”
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter=0;//对子进程进行计数
    int new_conn=1;
    int number=0;
    int ret=-1;

    while(!m_stop)
    {
        int number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)
        {
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(sockfd==m_listenfd)
            {//新连接到来时，采用round robin方法选择一个子进程处理
                int i=sub_process_counter;
                do
                {//轮转法：比如现在有4个子进程，0 1 2 3.第一次来了新连接，就选第0个子进程，下一次来了新连接，就选第1个子进程，依次轮流
                    if(m_sub_process[i].m_pid!=-1)//如果该进程存在，就选择该进程了
                        break;
                    i=(i+1) % m_process_number;
                }
                while(i!=sub_process_counter);//就是把已有子进程遍历一遍
                if(m_sub_process[i].m_pid==-1)//如果没有子进程，主进程也就要关闭
                {
                    m_stop=true;
                    break;
                }
                sub_process_counter= (i+1) % m_process_number;//下次再有新连接，就轮到下一个子进程了
                send(m_sub_process[i].m_pipefd[0],(char*) &new_conn,sizeof(new_conn),0);//通过第i个子进程的父子管道的父进程这一端，告诉该子进程，有新连接来到，让它accept
                printf("send request to child %d\n",i);
            }
            else if((sockfd==sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret=recv(sig_pipefd[0],signals,sizeof(signals),0);//把管道中的内容读到字符数组signals中
                if(ret<=0)
                    continue;
                else
                {//按顺序处理每个信号
                    for(int i=0;i<ret;++i)//每个信号占一字节，可以直接按字节来处理信号
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD://收到子进程退出信号
                            {
                                pid_t pid;
                                int stat;
                                while( (pid=waitpid(-1,&stat,WNOHANG))>0)//等待所有退出子进程
                                {
                                    for(int j=0;i<m_process_number;++j)
                                    {//这里需要遍历子进程信息数组，查找pid等于waitpid返回值的那个子进程
                                        if(m_sub_process[j].m_pid==pid)
                                        {
                                            printf("child %d exit\n",j);
                                            close(m_sub_process[j].m_pipefd[0]);//关闭父子管道父进程这一端
                                            m_sub_process[i].m_pid=-1;//pid设置为无效值
                                        }
                                    }
                                }
                                m_stop=true;
                                for(int i=0;i<m_process_number;++i)
                                {
                                    if(m_sub_process[i].m_pid!=-1)//如果还有子进程在运行
                                        m_stop=false;//父进程就继续运行。
                                }
                                break;
                            }
                            case SIGTERM://收到这两个信号，父进程将杀死所有子进程，并等待它们结束
                            case SIGINT:
                            {
                                printf("kill all the child now\n");
                                for(int i=0;i<m_process_number;++i)
                                {
                                    int pid=m_sub_process[i].m_pid;
                                    if(pid!=-1)
                                        kill(pid,SIGTERM);//给每个子进程发送SIGTERM信号
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
                
            }
            else 
                continue;
        }
    }

    //close(m_listenfd);//由创建者关闭连接描述符
    close(m_epollfd);
}

#endif