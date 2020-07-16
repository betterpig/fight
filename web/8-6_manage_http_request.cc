#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096

enum CHECK_STATE {CHECK_STATE_REQUESTLINE=0, CHECK_STATE_HEADER};//检查状态：请求行和首部行
enum LINE_STATUS {LINE_OK=0,LINE_BAD,LINE_OPEN};//文本行状态：一行结束、未结束、其他
enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };//http报文解析结果：
static const char* szret[] = { "I get a correst result\n", "Something wrong\n" };//响应内容
LINE_STATUS parse_line( char* buffer, int& checked_index, int& read_index)
{
    char temp;
    for(;checked_index<read_index;++checked_index)//HTTP报文格式：每一行必须以回车换行符‘\r\n'结尾，且在首部行之后有一空行
    {
        temp=buffer[checked_index];//按顺序检查每个字符
        if(temp=='\r')//如果当前字符是回车符
        {
            if((checked_index+1)==read_index)//但下一个位置又没有字符了，说明数据不完整
                return LINE_OPEN;
            else if(buffer[checked_index+1] == '\n')//并且下一个字符是换行符，说明读取到了完整的行
            {
                buffer[checked_index++] = '\0';//为什么要对回车符后的两个字符置0呢？
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n')//'\r\n',\n怎么可能出现在\r前面呢？
        {
            if((checked_index>1) && buffer[checked_index-1] == '\r')
            {
                buffer[checked_index-1]='\0';
                buffer[checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } 
    }
    return LINE_OPEN;//没有’\r‘字符，需要继续读取客户端数据才能分析
}

HTTP_CODE parse_requestline(char* temp,CHECK_STATE& checkstate)//分析请求行
{//请求行的格式： GET http://www.baidu.com/dir/index.html HTTP/1.1
    char* url=strpbrk(temp," \t");//返回空格或制表符在temp字符串中第一次出现的位置
    if(!url)
        return BAD_REQUEST;//如果没有空格和制表符，说明有问题
    *url++='\0';//先将当前字符置为空，再把指针移动到下一个位置

    char* method=temp;
    if(strcasecmp(method,"GET") ==0 )//检查temp中是否包含“GET”
        printf("the request method is GET\n");
    else
        return BAD_REQUEST;
    
    url += strspn(url, " \t");//在url字符串中查找空格和制表符，返回其不匹配的第一个位置，也就是说，url现在指向第一个h
    char* version =strpbrk(url," \t");//在url起始的字符串中查找空格或制表符，返回第一次出现的位置，也就是H的前一个空格
    if(!version)//没有空格，就没有版本号
        return BAD_REQUEST;
    *version++='\0';//将空格或制表符置为空，再把versiong指针移到下一位
    version+=strspn(version, " \t");//避免有很多空格的情况，先把空格都跳过，就指向了H
    if(strcasecmp(version, "HTTP/1.1") != 0)//在version起始的字符串中查找该字符串
        return BAD_REQUEST;

    if(strncasecmp(url, "http://",7) == 0)//在url指针起始的字符串中找该字段
    {
        url+=7;//偏移到//之后，指向w
        url=strchr(url, '/');//把偏移后的url指向url中第一次出现/的位置，防止有超过两个/
    }

    if(!url || url[0] != '/')//后面没数据了 ，后面没/了
        return BAD_REQUEST;
    printf("The request URL is: %s\n",url);//这里似乎只是输出请求的文件名，而不是完整的包括域名的url
    checkstate=CHECK_STATE_HEADER;//转向下一个状态：分析首部行
    return NO_REQUEST;
    
}

HTTP_CODE parse_headers(char* temp)
{
    if(temp[0] == '\0')//遇到空行，说明HTTP请求已经解析完了
        return GET_REQUEST;
    else if(strncasecmp(temp, "Host:",5)==0)//比较前5个字符
    {
        temp+=5;//移到下一个位置
        temp+=strspn(temp, " \t");//找到空格和制表符并跳到其后的第一个位置
        printf("the request host is: %s\n",temp);
    }
    else
        printf("I can not handle this header\n");//这里只解析了host和空行，实际还有其他首部行：connection等等
    return NO_REQUEST;
}

HTTP_CODE parse_content(char* buffer, int& checked_index, CHECK_STATE& checkstate,int& read_index,int& start_line)
{
    LINE_STATUS linestatus=LINE_OK;
    HTTP_CODE retcode=NO_REQUEST;
    //主状态机，用于从buffer中取出所有完整的行
    while((linestatus=parse_line(buffer,checked_index,read_index))==LINE_OK)//取出一行数据是否格式正确并返回结果，并已经将checked_index递增到下一行的起始位置
    {
        char* temp=buffer+start_line;//temp指向当前行的起始位置
        start_line=checked_index;//start_line指向下一行的起始位置。start_line只有读到完整的一行，才会更新为checked_index，也就是下一行的起始位置
        switch (checkstate)//checkstate初始值是CHECK_STATE_REQUESTLINE
        {
        case CHECK_STATE_REQUESTLINE:
        {//若请求行正确，parse_requestline函数将会把checkstate设置为CHECK_STATE_HEADER，在下一次循环就会转到分析首部行
            retcode=parse_requestline(temp,checkstate);
            if(retcode==BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {//若首部行正确，返回GET_REQUEST，解析请求报文完成，返回到主函数
            retcode=parse_headers(temp);
            if(retcode==BAD_REQUEST)
                return BAD_REQUEST;
            else if(retcode==GET_REQUEST)
                return GET_REQUEST;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    if(linestatus == LINE_OPEN)//语句还未结束，返回为解析出请求
        return NO_REQUEST;
    else
        return BAD_REQUEST;
}

int main(int argc, char* argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }

    const char *ip=argv[1];
    int port=atoi(argv[2]);

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
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0',BUFFER_SIZE);
        int data_read=0;
        int read_index=0;
        int checked_index=0;
        int start_line=0;//行在buffer中的起始位置

        CHECK_STATE checkstate=CHECK_STATE_REQUESTLINE;
        while(1)
        {//从连接描述符connction_fd对应的“文件”读数据，放在buffer+read_index偏移后的指针指向的内存中，该缓冲区还剩下BUFFER_SIZE-read_index长度的内存
            data_read=recv(connection_fd,buffer+read_index,BUFFER_SIZE-read_index,0);//data_read保存实际读取到的数据长度
            if(data_read==-1)
            {
                printf("reading failed\n");
                break;
            }
            else if(data_read==0)
            {
                printf("remote client has closed the connection\n");
                break;
            }

            read_index+=data_read;//buffer中已有数据的下一个位置
            //解析本次读取到的数据，即在buffer中从check_index开始到read_index结束的内存区域，不包括read_index，返回解析结果
            HTTP_CODE result=parse_content(buffer,checked_index,checkstate,read_index,start_line);
            if(result==NO_REQUEST)//尚未得到完整的HTTP请求，需要等待客户端发送更多数据
                continue;
            else if(result==GET_REQUEST)
            {//接收到了正确的HTTP请求，发送HTTP应答给客户端，可以跳出循环去做别的事了
                send(connection_fd,szret[0],strlen(szret[0]),0);
                break;
            }
            else//其他情况表示发生错误
            {
                send(connection_fd,szret[1],strlen(szret[1]),0);
                break;
            }
        }
        close(connection_fd);
    }
    //关闭监听描述符
    close(listenfd);
    return 0;
}