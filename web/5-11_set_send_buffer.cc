#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 512

int main(int argc,char* argv[])
{
    if(argc<=3)//原书是2，应该搞错了
    {
        printf("usage: %s ip_address port_number recv_buffer_size\n",basename(argv[0]));
        return 1;
    }

    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family=AF_INET;
    //inet_pton 成功时 返回1，失败时 返回0.所以当函数输入参数中ip的对应项为主机名时，地址转换失败，
    //server_address仍为清零之后的值0.0.0.0,相当于本机地址，但本机并没有启动对应端口的服务，所以
    //连接失败。这也是为什么启动服务端就可以的原因，因为服务端本就是用本机地址，而在本机再启动客户端也
    //可以，就是连本机地址的对应端口。
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    server_address.sin_port=htons(port);

    char* p=new char[16];
    printf(" now address is %s\n",inet_ntop(AF_INET,&server_address.sin_addr,p,INET_ADDRSTRLEN));
    int connectionfd=socket(PF_INET,SOCK_STREAM,0);
    assert(connectionfd>=0);

    int sendbuf=atoi(argv[3]);
    int len=sizeof(sendbuf);
    //先设置发送缓冲区的大小，然后立即读取
    setsockopt(connectionfd,SOL_SOCKET,SO_RCVBUF,&sendbuf,len);
    getsockopt(connectionfd,SOL_SOCKET,SO_RCVBUF,&sendbuf,(socklen_t*) &len);
    printf("the tcp send buffer size after setting is %d\n",sendbuf);

    if(connect(connectionfd,(struct sockaddr*) &server_address,sizeof(server_address))!=-1)
    {
        char buffer[BUFFER_SIZE];
        memset(buffer,'a',BUFFER_SIZE);
        send(connectionfd,buffer,BUFFER_SIZE,0);
    }
    else
        printf("connect failed ,errno is: %d\n",errno);
    close (connectionfd);
    return 0;
}