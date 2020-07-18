#define _GNU_SOURCE 1
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

struct client_data
{
    sockaddr_in client_address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
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

    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);

    int ret=bind(listenfd,(struct sockaddr*) &server_address,sizeof(server_address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);
    
    ret=listen(listenfd,10);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);

    client_data* users=new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT];
    int user_counter=0;
    for(int i=1;i<USER_LIMIT;++i)
    {
        fds[i].fd=-1;
        fds[i].events=0;
    }
    fds[0].fd=listenfd;
    fds[0].events=POLLIN | POLLERR;
    fds[0].revents=0;

    while(1)
    {
        ret=poll(fds,user_counter+1,-1);
        if(ret<0)
        {
            printf("poll failure\n");
            break;
        }

        for(int i=0;i<user_counter+1;i++)
        {
            if( (fds[i].fd==listenfd) && (fds[i].revents & POLLIN))
            {
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                if(connection_fd<0)
                {
                    printf("errno is: %d\n",errno);
                    continue;
                }
                if(user_counter>=USER_LIMIT)
                {
                    const char* info="too many users\n";
                    printf("%s",info);
                    send(connection_fd,info,strlen(info),0);
                    close(connection_fd);
                    continue;
                }

                user_counter++;
                users[connection_fd].client_address=client_address;
                SetNonBlocking(connection_fd);
                fds[user_counter].fd=connection_fd;
                fds[user_counter].events=POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents=0;
                printf("comes a new user,now have %d users\n",user_counter);
            }
            else if( fds[i].revents & POLLERR)
            {
                
            }
        }
    }
}