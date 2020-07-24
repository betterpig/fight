//没达到预期效果
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

static const int CONTROL_LEN=CMSG_LEN(sizeof(int));

void Sendfd(int pipefd,int fd_to_send)//通过父子管道传递文件描述符fd_to_send
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];

    iov[0].iov_base=buf;
    iov[0].iov_len=1;
    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;

    cmsghdr cm;
    cm.cmsg_len=CONTROL_LEN;
    cm.cmsg_level=SOL_SOCKET;
    cm.cmsg_type=SCM_RIGHTS;
    *(int*) CMSG_DATA (&cm)=fd_to_send;
    msg.msg_control=&cm;//设置辅助数据
    msg.msg_controllen=CONTROL_LEN;

    sendmsg(pipefd,&msg,0);
}

int Recvfd(int fd)//接受文件描述符
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];

    iov[0].iov_base=buf;
    iov[0].iov_len=1;
    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;

    cmsghdr cm;
    msg.msg_control=&cm;
    msg.msg_controllen=CONTROL_LEN;

    recvmsg(fd,&msg,0);

    int fd_to_read=*(int*) CMSG_DATA (&cm);
    return fd_to_read;
}

int main()
{
    int pipefd[2];
    int fd_to_pass=0;
    int ret=socketpair(PF_UNIX,SOCK_DGRAM,0,pipefd);//创建全双工通信的父子管道
    assert(ret!=-1);

    pid_t pid=fork();//创建子进程 
    assert(pid>=0);

    if(pid==0);
    {
        close(pipefd[0]);//子进程关闭父子管道的父进程一端
        fd_to_pass=open("Makefile",O_RDWR,0666);//打开一个文件，获得该文件的文件描述符
        printf("the fd to be send is %d\n",fd_to_pass);
        Sendfd(pipefd[1],(fd_to_pass>0)? fd_to_pass:0);
        close(fd_to_pass);//子进程关闭该文件
        exit(0);
    }
    close(pipefd[1]);//父进程关闭父子管道的子进程一端
    fd_to_pass=Recvfd(pipefd[0]);//接受文件描述符。传递的不是文件描述符的值，而是传递了对同一个文件的引用
    char buf[1024];
    memset(buf,'\0',1024);
    read(fd_to_pass,buf,1024);//读取文件并打印
    printf("i got fd %d and data %s\n",fd_to_pass,buf);
    close(fd_to_pass);//父进程关闭该文件
}