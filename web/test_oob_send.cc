//测试带外数据的接收和发送

#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

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

    int connection_fd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(connection_fd>=0);

    if(connect(connection_fd,(struct sockaddr*) &server_address,sizeof(server_address))<0)
        printf("connection failed\n");
    else
    {
        const char* oob_data="abc";
        const char* normal_data="123";
        send(connection_fd,normal_data,strlen(normal_data),0);
        send(connection_fd,oob_data,strlen(oob_data),MSG_OOB);//发送紧急数据，只有c字符会被当成紧急数据
        send(connection_fd,normal_data,strlen(normal_data),0);
    }

    close(connection_fd);
    return 0;
}