#include <sys/socket.h>//我这个没达到预期效果，没有发出SIGURG信号
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#define BUF_SIZE 1024

static int connfd;

void SigUrg(int sig)//SIGURG信号的信号处理函数
{
    int save_errno=errno;
    char buffer[BUF_SIZE];
    memset(buffer,'\0',BUF_SIZE);
    int ret=recv(connfd,buffer,BUF_SIZE-1,MSG_OOB);//当接收到SIGURG信号时，就采用MSG_OOB的方式读取带外数据
    printf("got %d bytes of oob data '%s' \n",ret,buffer);
    errno=save_errno;
}

void AddSig(int sig,void (*sig_handler) (int))//为信号sig设置信号处理函数，第二个参数是函数指针，参数名为sig_handle，函数类型为：返回值类型为void，参数类型为int
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=sig_handler;//用函数指针为sa的成员赋值
    sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr)!=-1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
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
    
    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
    socklen_t client_addr_length=sizeof(client_address);
    connfd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
    if(connfd<0)
        printf("errno is: %d\n",errno);
    else
    {
        AddSig(SIGURG,SigUrg);//为SIGURG信号指定信号处理函数
        fcntl(connfd,F_SETOWN,getpid());//设置连接描述符的宿主进程，即当该连接描述符上有带外数据时，系统将发出SIGURG信号给宿主进程，将触发处理函数

        char buffer[BUF_SIZE];
        while(1)//正常接收数据
        {
            memset(buffer,'\0',BUF_SIZE);
            ret=recv(connfd,buffer,BUF_SIZE-1,0);
            if(ret<=0)
                break;
            printf("got %d bytes of normal data '%s'\n",ret,buffer);
        }
        close(connfd);
    }

    close(listenfd);
    return 0;
}
    
