#include "sys/socket.h"
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
    
    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
    socklen_t client_addr_length=sizeof(client_address);
    int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
    if(connection_fd<0)
    {
        printf("errno is: %d\n",errno);
        close(listenfd);
        return 0;
    }
    
    char buf[1024];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);//清零
    FD_ZERO(&exception_fds);

    while(1)
    {
        memset(buf,'\0',sizeof(buf));//buffer清零
        FD_SET(connection_fd,&read_fds);//设置监视连接描述符对应的“文件”是否可读
        FD_SET(connection_fd,&exception_fds);//设置监视连接描述符对应的“文件”是否可写
        ret=select(connection_fd+1,&read_fds,nullptr,&exception_fds,NULL);//select函数返回就绪文件描述符的总数
        if(ret<0)
        {
            printf("selection failure\n");
            break;
        }

        if(FD_ISSET(connection_fd,&read_fds))//判断连接描述是否被设置为可读状态或者异常状态
        {
            ret=recv(connection_fd,buf,sizeof(buf)-1,0);//可读状态就读取数据
            if(ret<=0)
                break;
            printf("get %d bytes of normal data: %s\n",ret,buf);
        }
        if(FD_ISSET(connection_fd,&exception_fds))//这里有问题，和test_oob_send一起测试时，若send之间没有相隔一段时间，
        {//那123和abc会一起收到，会进入第一个if，由于第二个是else if，就不会进入，这样就把带外数据c给跳过了
        //可以把else if改成if，然后在接收带外数据时，先把buf前一次读的正常数据清零
        //这里说明select有时候会把同一文件描述符的多种事件都设为就绪
            memset(buf,'\0',sizeof(buf));
            ret=recv(connection_fd,buf,sizeof(buf)-1,MSG_OOB);//采用带MSG_OOB标志的recv函数读取带外数据
            if(ret<=0)
                break;
            printf("get %d bytes of oob data: %s\n",ret,buf);
        }
        
    }
    
    close(connection_fd);
    close(listenfd);
    return 0;
}