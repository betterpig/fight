#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc,char* argv[])
{
    if(argc<=3)
    {
        printf("usage: %s ip_address port_number recv_buffer_size\n",basename(argv[0]));
        return 1;
    }

    const char* ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    server_address.sin_port=htons(port);

    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    int recvbuf=atoi(argv[3]);
    int len=sizeof(recvbuf);
    //先设置接收缓冲区的大小，然后立即读取
    setsockopt(listenfd,SOL_SOCKET,SO_RCVBUF,&recvbuf,len);
    getsockopt(listenfd,SOL_SOCKET,SO_RCVBUF,&recvbuf,(socklen_t*) &len);
    printf("the tcp receive buffer size after setting is %d\n",recvbuf);

    int ret = bind(listenfd, (struct sockaddr*) &server_address,sizeof(server_address));
    assert(ret!=-1);

    ret = listen(listenfd,5);
    assert(ret!=-1);

    struct sockaddr_in client_address;
    socklen_t client_address_len=sizeof(client_address);
    int connectionfd=accept(listenfd, (struct sockaddr*) &client_address,&client_address_len);
    if(connectionfd<0)
        printf("errno is : %d\n",errno);
    else
    {
        printf("waiting for data\n");
        char buffer[BUFFER_SIZE];
        memset(buffer,'\0',BUFFER_SIZE);
        while(recv(connectionfd,buffer,BUFFER_SIZE-1,0)>0)
        {   ;}
        printf("receive data first is :%c\n",buffer[0]);
        close(connectionfd);
    }
    close (listenfd);
    return 0;
}