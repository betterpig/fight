#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <time.h>
//每个客户连接不断地向服务器发送这个请求
static const char* request="GET http://localhost/index.html HTTP/1.1\r\nConnection:keep-alive\r\n\r\nxxxxxxxxxxx";
int connection_number=0;//统计当前剩余连接数量

int SetNonBlocking(int fd)//将文件描述符设为非阻塞状态
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

void Addfd(int epollfd,int fd)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLOUT | EPOLLET | EPOLLERR;//一开始所有连接描述符都是监听可写事件
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

bool WriteBytes(int sockfd,const char* buffer,int len)
{
    int bytes_write=0;
    //printf("write out %d bytes to socket %d\n",len,sockfd);
    while(1)
    {
        bytes_write=send(sockfd,buffer,len,0);//将buffer中的内容发给服务器
        if(bytes_write==-1)//写失败
            return false;
        else if(bytes_write==0)//对方关闭连接
            return false;
        len-=bytes_write;
        buffer=buffer+bytes_write;
        if(len<=0)//所有数据发送完，跳出循环
            return true;
    }
}

bool ReadOnce(int sockfd,char* buffer,int len)
{
    int bytes_read=0;
    memset(buffer,'\0',len);
    bytes_read=recv(sockfd,buffer,len,0);//读取接收缓冲区的数据，只读一次（缓冲区不会满吗？）
    if(bytes_read==-1)
        return false;
    else if(bytes_read==0)
        return false;
    //printf("read %d bytes from socket %d with content: %s\n",bytes_read,sockfd,buffer);
    return true;
}

void StartConn(int epollfd,int num,const char* ip,int port)//建立指定数量的连接，并将连接描述符放到内核事件表中
{
    int ret=0;//服务器地址都是同一个
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    for(int i=0;i<num;i++)//循环建立连接
    {
        sleep(1);//每个连接之间要等一等，不然建立不起来
        int sockfd=socket(PF_INET,SOCK_STREAM,0);
        //printf("creadte 1 sock\n");
        if(sockfd<0)
            continue;
        if(connect(sockfd,(struct sockaddr*) &server_address,sizeof(server_address))==0)
        {
            //printf("build connection %d\n",i);
            Addfd(epollfd,sockfd);
            connection_number++;
        }
    }
    printf("total connection is %d\n",connection_number);
}

void CloseConn(int epollfd,int sockfd)//从内核事件表中删除连接描述符，关闭连接
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,sockfd,0);
    close(sockfd);
    connection_number--;//当前存活连接数量减一
}

int main(int argc,char* argv[])
{
    assert(argc==4);//四个参数：程序名称 ip port 指定连接数量
    int epollfd=epoll_create(100);
    StartConn(epollfd,atoi(argv[3]),argv[1],atoi(argv[2]));
    epoll_event events[10000];
    char buffer[2048];
    time_t start,end;
    start=time(nullptr);
    while(1)
    {
        end=time(nullptr);
        if(end-start>=5)//每隔5秒钟打印一次当前剩余连接数量
        {
            printf("now alive connection is: %d\n",connection_number);
            start=time(nullptr);
        }
        int number=epoll_wait(epollfd,events,10000,2000);
        for(int i=0;i<number;i++)//就绪事件：监听描述符可读、信号事件、连接描述符可读、连接描述符可写
        {
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(events[i].events & EPOLLIN)//某连接描述符改成了监听可读，就会有触发可读事件的时候
            {
                if(!ReadOnce(sockfd,buffer,2048))//读失败关闭连接
                    CloseConn(epollfd,sockfd);
                struct epoll_event event;//读成功了又改成监听可写事件，给服务器发数据
                event.events=EPOLLOUT | EPOLLET | EPOLLERR;
                event.data.fd=sockfd;
                epoll_ctl(epollfd,EPOLL_CTL_MOD,sockfd,&event);
            }
            else if(events[i].events & EPOLLOUT)
            {//一开始所有连接描述符都是监听可写事件，一开始发送缓冲区肯定可写，那一开始epoll肯定会触发可写事件，进入该if
                if(!WriteBytes(sockfd,request,strlen(request)))
                    CloseConn(epollfd,sockfd);//发送数据失败就关闭连接
                struct epoll_event event;//发送成功就把监听可写改成监听可读,因为服务器肯定会返回对请求的响应
                event.events=EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd=sockfd;
                epoll_ctl(epollfd,EPOLL_CTL_MOD,sockfd,&event);
            }
            else if(events[i].events & EPOLLERR)
                CloseConn(epollfd,sockfd);
        }
    }

}