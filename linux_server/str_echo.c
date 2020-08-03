#include "unp.h"

void str_echo(int connfd)
{
    ssize_t n;
    char buf[MAXLINE];//定义字符数组保存客户端传来的数据
again:
    while ((n=read(connfd,buf,MAXLINE))>0)//read函数从已连接描述符对应的“文件”中读数据（客户端发来的）并存到buf中，返回读取的字节数
        writen(connfd,buf,n);//把buf中的数据再写到已连接描述符对应的“文件”中（发送给客户端），
    if(n<0 && errno==EINTR)
        goto again;//如果发生EINTR错误则继续进行读操作
    else if(n<0)
        err_sys("str_echo: read error");
    
}