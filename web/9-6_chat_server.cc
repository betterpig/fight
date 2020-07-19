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
#include <errno.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

struct client_data//客户类
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

    client_data* users=new client_data[FD_LIMIT];//创建client_data类对象数组，并把首地址赋给users
    pollfd fds[USER_LIMIT+1];//定义包含limit+1个pollfd结构体对象的数组，+1是因为还要监听“监听描述符”上的可读事件
    int user_counter=0;//记录当前已连接的客户端的数量
    for(int i=1;i<=USER_LIMIT;++i)
    {
        fds[i].fd=-1;//初始化pollfd结构体数组
        fds[i].events=0;
    }
    fds[0].fd=listenfd;//监听 “监听描述符”上的可读事件和错误事件
    fds[0].events=POLLIN | POLLERR;
    fds[0].revents=0;

    while(1)
    {//poll第二个参数是指监听的文件描述符的总数，它总是等于当前连接的用户数+1，因为每建立一个连接，fds数组就多一个有效的文件描述符，1是指一直监听着的”监听描述符“
        ret=poll(fds,user_counter+1,-1);//监听就绪事件，若无就绪事件则永远阻塞
        if(ret<0)
        {
            printf("poll failure\n");
            break;
        }

        for(int i=0;i<user_counter+1;i++)//这就是poll的缺点，需要对每个监听的文件描述符去检查哪个文件描述符的哪个事件就绪了，不像epoll，只返回就绪事件
        {//只有”监听描述符“上的可读事件是不能与其他描述符共用代码的。其他的连接描述符，只要事件相同代码都是一样的。”监听描述符“的错误事件与其他连接描述符代码也是公用的
            if( (fds[i].fd==listenfd) && (fds[i].revents & POLLIN))
            {//建立连接
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                if(connection_fd<0)
                {
                    printf("errno is: %d\n",errno);
                    continue;
                }
                if(user_counter>=USER_LIMIT)//如果已经有5个客户连接了
                {
                    const char* info="too many users\n";
                    printf("%s",info);//在服务端打印
                    send(connection_fd,info,strlen(info),0);//并且把该信息发送给客户端
                    close(connection_fd);//然后关闭该连接
                    continue;//所以其实还是可以连接的，也连接成功了，只是服务器认为已经超过了承载能力，主动将该连接关闭了
                }

                user_counter++;//如果还没有满，才能真正的建立长时间的连接，享受服务端的服务
                users[connection_fd].client_address=client_address;//客户对象数组的大小是65535，这是文件描述符的数量限制。所以客户数组是按连接描述符来索引的，而不是按客户编号12345.因为连接描述符是唯一的，且在范围内，所以这样做是可行的
                SetNonBlocking(connection_fd);//将该连接描述符设置为非阻塞形式
                fds[user_counter].fd=connection_fd;//fds是按客户连接数量索引的，把fd设为有效的连接描述符，下一次调用poll，就会监听该连接描述符上的指定事件了
                fds[user_counter].events=POLLIN | POLLRDHUP | POLLERR;//可读事件、连接被对方关闭事件、错误事件
                fds[user_counter].revents=0;
                printf("comes a new user,now have %d users\n",user_counter);
            }
            else if(fds[i].revents & POLLERR)
            {
                printf("get an error from %d\n",fds[i].fd);
                char errors[100];
                memset(errors,'\0',100);
                socklen_t length=sizeof(errors);
                if(getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,&errors,&length)<0)//获取并清除socket错误状态
                    printf("get socket option failed\n");
                continue;
            }
            else if(fds[i].revents & POLLRDHUP)//连接被对方关闭
            {//fds[i].fd就是发生该关闭事件的文件描述符
                users[fds[i].fd]=users[fds[user_counter].fd];//这个不知道有何意义
                close(fds[i].fd);//关闭该连接
                fds[i]=fds[user_counter];//把最后一个有效的的fds结构体复制给该连接关闭对应的结构体，然后counter--，
                i--;//比如本来有4个客户，现在少一个，下次循环时就只会循环到3，那第四个客户怎么办呢，它已经转移到前面被关闭的客户位置上去了。
                user_counter--;//要是第四个客户就是被关闭的，那就是把第四个转移到第四个，下一次循环也不会访问到它，尽管它的数据依然保存在数组中
                printf("a client left，now only %d client exitst\n",user_counter);
            }
            else if(fds[i].revents & POLLIN)
            {
                int connfd=fds[i].fd;
                memset(users[connfd].buf,'\0',BUFFER_SIZE);
                ret=recv(connfd,users[connfd].buf,BUFFER_SIZE-1,0);//把读取到的数据放在客户对象的buf里面
                printf("get %d bytes of client data %s from %d\n",ret,users[connfd].buf,connfd);
                if(ret <0)
                {
                    if(errno != EAGAIN)//如果不是读取完毕的问题
                    {
                        close(connfd);//关闭连接
                        users[fds[i].fd]=users[fds[user_counter].fd];
                        fds[i]=fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if (ret==0)
                {

                }
                else
                {//准备把从一个客户那里接收到的数据，发给其他客户
                    for(int j=1;j<=user_counter;++j)
                    {
                        if(fds[j].fd==connfd)//对除了自身的其他连接描述符
                            continue;
                        fds[j].events |= ~POLLIN;//不再监听可读事件
                        fds[j].events |= POLLOUT;//监听可写事件，不出意外下一次poll就会返回这些连接描述符上的可写事件就绪
                        users[fds[j].fd].write_buf=users[connfd].buf;//并且把要写的数据（来自前面的从某个客户连接读取到的数据）放在每个客户对象的buf上
                    }
                }
            }
            else if(fds[i].revents & POLLOUT)
            {
                int connfd=fds[i].fd;
                if(!users[connfd].write_buf)//如果可写，但是buf里又没数据，按理说这是有问题的，只要进入了这个if，buf就应该有数据
                    continue;
                ret=send(connfd,users[connfd].write_buf,strlen(users[connfd].write_buf),0);//把客户对象里buf的数据写到连接描述符上
                users[connfd].write_buf=NULL;//清空buf
                fds[i].events |= ~POLLOUT;//重新开始监听可读事件，不再监听可写事件
                fds[i].events |= POLLIN;
            }
        }
    }

    delete []users;
    close(listenfd);
    return 0;
}