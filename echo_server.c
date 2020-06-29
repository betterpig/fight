#include "unp.h"

void sig_chld(int signo);

int main(int argc,char **argv)
{
    int listenfd,connfd;//定义监听描述符和连接描述符
    pid_t childpid;//子进程id
    socklen_t clilen;//套接字长度
    struct sockaddr_in cliaddr,servaddr;//定义客户端套接字和服务端套接字
    listenfd=Socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符
    bzero(&servaddr,sizeof(servaddr));//先将服务器套接字结构体置0
    servaddr.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);//host to net long
    servaddr.sin_port=htons(SERV_PORT);//host to net short
    Bind(listenfd,(SA *) &servaddr,sizeof(servaddr));//将服务端套接字与监听描述符绑定
    Listen(listenfd,LISTENQ);//将主动套接字转换为被动套接字，并指定已完成连接队列加未完成连接队列之和的最大值
    signal(SIGCHLD,sig_chld);
    for(;;)
    {
        clilen=sizeof(cliaddr);//从已完成连接队列中取出队头
        if((connfd=accept(listenfd,(SA *) &cliaddr,&clilen))<0)
        {
            if(errno==EINTR)
                continue;
            else
                err_sys("accept error");
        }
        if( (childpid=Fork())==0)//创建子进程，如果返回值为0，说明当前进程为父进程；返回值不为0，说明当前进程是子进程，就要关闭监听描述符，用已连接描述符作参数调用str_echo
        {
            Close(listenfd);//子进程关闭父进程监听描述符
            str_echo(connfd);//子进程调用str_echo函数服务该连接描述符对应的客户端
            exit(0);//结束子进程，断开连接
        }
        Close(connfd);//父进程关闭已连接描述符
    }
    
}