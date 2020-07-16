#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int main(int argc,char *argv[])
{
    if(argc<=1)
    {
        printf("usage: %s <file>\n",basename(argv[0]));
        return 1;
    }
    int filefd=open(argv[1],O_CREAT | O_WRONLY | O_TRUNC,0666);//打开文件，获得文件描述符
    assert(filefd>0);

    int pipefd_stdout[2];
    int ret=pipe(pipefd_stdout);//标准输出管道
    assert(ret!=-1);

    int pipefd_file[2];
    ret=pipe(pipefd_file);//文件管道
    assert(ret!=-1);

    //将标准输入“文件”的内容移动到标准输出管道
    ret=splice(STDIN_FILENO,nullptr,pipefd_stdout[1],nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret!=-1);

    //将标准输出管道的内容复制到文件管道，标准输出管道仍然保留该份数据
    ret=tee(pipefd_stdout[0],pipefd_file[1],32768,SPLICE_F_NONBLOCK);
    assert(ret!=-1);

    //将文件管道的内容移动到文件描述符对应的文件中
    ret=splice(pipefd_file[0],nullptr,filefd,nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret!=-1);

    //将标准输出管道的内容移动到标准输出“文件”->打印在终端，标准输出管道此时已经没有内容
    ret=splice(pipefd_stdout[0],nullptr,STDOUT_FILENO,nullptr,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret!=-1);

    close(filefd);//关闭各个文件描述符对应的文件
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);
    close(pipefd_file[0]);
    close(pipefd_file[1]);
    
    return 0;
}