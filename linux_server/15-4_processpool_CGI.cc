#include <sys/types.h>
#include <sys/socket.h>
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
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "15-3_process_pool.h"

class CgiConn
{
private:
    static const int BUFFER_SIZE=1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;
public:
    CgiConn(){}
    ~CgiConn(){}
    void init(int epollfd,int sockfd,const sockaddr_in& client_addr)
    {
        m_epollfd=epollfd;//初始化，也就是保存该链接的相关数据
        m_sockfd=sockfd;
        m_address=client_addr;
        memset(m_buf,'\0',BUFFER_SIZE);
        m_read_idx=0;//已有数据指针
    }

    void Process()
    {
        int idx=0;//本次读取的数据起始位置指针
        int ret=-1;
        while(true)
        {
            idx=m_read_idx;
            ret=recv(m_sockfd,m_buf+idx,BUFFER_SIZE-1-idx,0);//从连接描述符中读取数据
            if(ret<0)
            {
                if(errno!=EAGAIN)
                    Removefd(m_epollfd,m_sockfd);//这是从调用process函数的子进程的内核事件表中删除该连接描述符，并关闭该连接描述符
                break;
            }
            else if(ret==0)
            {
                Removefd(m_epollfd,m_sockfd);
                break;
            }
            else
            {
                m_read_idx+=ret;//更新指针偏移
                printf("user content is: %s\n",m_buf);
                for(;idx<m_read_idx;++idx)//本次数据的起始到结尾
                {
                    if( (idx>=1) && (m_buf[idx-1]=='\r') && (m_buf[idx]=='\n'))//如果有回车换行符
                        break;
                }
                if(idx==m_read_idx)//二者相等说明上面的for循环是遍历完才结束的，不是break
                    continue;
                m_buf[idx-1]='\0';

                char* file_name=m_buf;
                if(access(file_name,F_OK)==-1)//判断客户要运行的CGI程序是否存在
                {
                    Removefd(m_epollfd,m_sockfd);
                    break;
                }
                ret=fork();//如果是文件名，创建子进程，
                if(ret==-1)
                {
                    Removefd(m_epollfd,m_sockfd);
                    break;
                }
                else if(ret>0)
                {
                    Removefd(m_epollfd,m_sockfd);//父进程删除该连接描述符
                    break;
                }
                else
                {
                    close(STDOUT_FILENO);//关闭标准输出文件描述符
                    dup(m_sockfd);//复制连接描述符，将标准输入重定向到连接描述符上，先在本来会发给标准输出的内容都会发给连接描述符
                    execl(m_buf,m_buf,0);//执行该文件
                    exit(0);
                }
            }
        }
    }
};

int CgiConn::m_epollfd=-1;

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
    
    ret=listen(listenfd,5);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);

    ProcessPool<CgiConn>* pool=ProcessPool<CgiConn>::Create(listenfd);
    if(pool)
    {
        pool->Run();
        delete pool;
    }
    close(listenfd);//main函数创建的监听描述符，就必须由main函数来关闭
    return 0;
}