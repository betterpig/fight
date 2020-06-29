#include "unp.h"

void str_cli(FILE *fp,int connfd)
{
    char sendline[MAXLINE],recvline[MAXLINE];//定义发送消息的数组和接受消息的数组
    while(Fgets(sendline,MAXLINE,fp)!=NULL)//从标准输入读取数据
    {
        Writen(connfd,sendline,strlen(sendline));//将该数据写到已连接描述符对应的“文件”
        if(Readline(connfd,recvline,MAXLINE)==0)//从已连接描述符对应的“文件”读数据
            err_quit("str_sli: server terminated prematurely");
        Fputs(recvline,stdout);//将从服务器收到的数据写到标准输出
    }
}