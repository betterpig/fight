#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/uio.h>

#define BUFFER_SIZE 1024
//定义两种HTTP状态码和状态信息
static const char* status_line[2]={"200 ok","500 internal server error"};

int main(int argc,char *argv[])
{
    if(argc<=3)
    {
        printf("usage: %s ip_address port_number filename\n",basename(argv[0]));
        return 1;
    }

    const char *ip=argv[1];
    int port=atoi(argv[2]);
    const char* file_name=argv[3];//将目标文件作为程序的第三个参数传入

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
        printf("errno is: %d\n",errno);
    else
    {
        char header_buf[BUFFER_SIZE];//用于存放HTTP应答的状态行，头部字段和一个空行的缓存区
        char* file_buf;//用于存放目标文件内容
        struct stat file_stat;//用于获取目标文件的属性，比如是否为目录，文件大小等
        bool valid=true;//记录目标文件是否是有效文件
        int len=0;//缓存区header_buf目前已经使用了多少字节的空间
        if(stat(file_name,&file_stat)<0)//stat函数获取目标文件状态，返回值小于0说明文件不存在
            valid=false;
        else
        {
            if(S_ISDIR(file_stat.st_mode))//目标文件是一个目录
                valid=false;
            else if(file_stat.st_mode & S_IROTH)//当前用户有读取目标文件的权限
            {
                int fd=open(file_name,O_RDONLY);//打开该文件，获得该文件的描述符
                file_buf=new char[file_stat.st_size+1];//分配比文件size加1的内存
                memset(file_buf,'\0',file_stat.st_size+1);//该内存清零
                if(read(fd,file_buf,file_stat.st_size)<0)//将该文件写到file_buf指向的内存中，并判断是否成功
                    valid=false;
            }
            else
                valid=false;
        }
        if(valid)//目标文件有效，发送正常的HTTP应答
        {
            ret=snprintf(header_buf,BUFFER_SIZE,"%s %s\r\n","HTTP/1.1",status_line[0]);//将状态行写到headerbuf中
            len+=ret;
            ret=snprintf(header_buf+len,BUFFER_SIZE-1-len,"Content-Length: %d\r\n",file_stat.st_size);//将内容长度头部字段写到headerbuf中
            len+=ret;//指针偏移
            ret=snprintf(header_buf+len,BUFFER_SIZE-1-len,"%s","\r\n");//将空行写到headerbuf中
            struct iovec iv[2];//定义iovec数组，描述若干块内存区
            iv[0].iov_base=header_buf;//iovec结构体，内容包括内存的起始地址和长度
            iv[0].iov_len=strlen(header_buf);
            iv[1].iov_base=file_buf;
            iv[1].iov_len=file_stat.st_size;
            ret=writev(connection_fd,iv,2);//writev函数将这两块内存里的内容依次写到连接描述符指向的“文件”
        }
        else//目标文件无效，通知客户端 ，服务器发生了内部错误，状态行的状态是500
        {
            ret=snprintf(header_buf,BUFFER_SIZE,"%s %s\r\n","HTTP/1.1",status_line[1]);
            len+=ret;
            ret=snprintf(header_buf+len,BUFFER_SIZE-1-len,"%s","\r\n");
            send(connection_fd,header_buf,strlen(header_buf),0);
        }
        close(connection_fd);
        delete []file_buf;//释放动态分配的内存
    }
    //关闭监听描述符
    close(listenfd);
    return 0;
}