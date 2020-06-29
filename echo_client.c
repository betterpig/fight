#include <unp.h>

int main(int argc,char **argv)
{
    int i,connfd;//连接描述符
    struct sockaddr_in servaddr;//服务器套接字
    if(argc!=2)
        err_quit("usage:tcpcli <IPaddress>");

    connfd=Socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符
    bzero(&servaddr,sizeof(servaddr));//先将服务器套接字结构体置0
    servaddr.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、端口
    servaddr.sin_port=htons(SERV_PORT);
    Inet_pton(AF_INET,argv[1],&servaddr.sin_addr);//按给定协议AF_INET，将argv[1]参数（点分十进制IP地址）转换成二进制存放在sin_addr中
    Connect(connfd,(SA*) &servaddr,sizeof(servaddr));//与给IP地址对应的服务器连接，成功后将连接描述符状态变为established
    str_cli(stdin,connfd);//调用函数，从标准输入读数据，并传到该已连接描述符对应的“文件”
    exit(0);//退出
}