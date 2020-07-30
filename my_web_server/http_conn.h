#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

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
#include <pthread.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/uio.h>
#include <string>
//#include "lst_timer.h"
#include "locker.h"
#include <mysql/mysql.h>
#include <map>
#include <string>
using namespace std;

class UtilTimer;
class connection_pool;


class HttpConn
{
public:
    static const int FILENAME_LEN=200;//文件名的最大长度
    static const int READ_BUFFER_SIZE=2048;//读缓冲区的大小
    static const int WRITE_BUFFER_SIZE=1024;//写缓冲区的大小
    enum METHOD {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH};//HTTP请求方法，这里仅支持GET
    enum CHECK_STATE {  CHECK_STATE_REQUESTLINE=0,//解析客户请求时，主状态机所处的状态
                        CHECK_STATE_HEADER,
                        CHECK_STATE_CONTENT};
    enum HTTP_CODE {NO_REQUEST, //服务器处理HTTP请求的可能结果
                    GET_REQUEST, 
                    BAD_REQUEST, 
                    NO_RESOURCE,
                    FORBIDDEN_REQUEST, 
                    FILE_REQUEST,
                    INTERNAL_ERROR, 
                    CLOSED_CONNECTION};
    enum LINE_STATUS {LINE_OK=0,LINE_BAD,LINE_OPEN};//行的读取状态

public:
    static int m_epollfd;//类的静态数据成员，将被所有同类对象所共享。所有socket上的事件都注册到同一个epoll内核事件表上，
    static int m_user_count;//统计用户数量
    UtilTimer* timer;
    MYSQL* mysql;

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];//读缓冲区
    int m_read_idx;//已经读入的客户数据的最后一个字节的下一个位置
    int m_checked_idx;//当前正在分析的字符在读缓冲区的位置
    int m_start_line;//当前正在解析的行的其实位置
    char m_write_buf[WRITE_BUFFER_SIZE];//写缓冲区
    int m_write_idx;//写缓冲区中待发送的字节数

    CHECK_STATE m_check_state;//主状态机当前所处状态
    METHOD m_method;//请求方法
    int cgi;
    char m_real_file[FILENAME_LEN];//客户请求的目标文件的完整路径，内容等于doc_root+m_url,doc_root是网站根目录
    char* m_url;//客户请求的目标文件的文件名
    char* m_version;//HTTP协议版本号
    char* m_host;//主机名
    int m_content_length;//HTTP请求的消息体的长度
    std::string m_string;
    bool m_linger;//HTTP请求是否要求保持连接
    char* m_file_address;//客户请求的目标文件被mmap到内存中的起始地址
    struct stat m_file_stat;//目标文件的状态：是否存在、是否为目录、是否可读，文件大小等信息
    struct iovec m_iv[2];//io向量结构体数组，该结构体指出内存位置和内存长度,实现分散读和集中写（因为HTTP应答的前部分信息内容和后部分的文档内容通常是在不同的内存区域存储的）
    int m_iv_count;//被写内存块的数量

    
public:
    HttpConn(){}
    ~HttpConn(){}

    void Init(int sockfd,const sockaddr_in& addr);//初始化新接受的连接
    void CloseConn(bool real_close=true);//关闭连接
    void Process();//处理客户请求
    bool Read();//非阻塞读操作
    bool Write();//非阻塞写操作
    void GetDataBase(connection_pool* connpool);

private:
    void Init();//初始化连接
    HTTP_CODE ProcessRead();//解析HTTP请求的入口函数
    HTTP_CODE ParseRequestLine(char* text);//解析请求行
    HTTP_CODE ParseHeaders(char* text);//解析首部行
    HTTP_CODE ParseContent(char* text);//解析内容
    LINE_STATUS ParseLine();//每次截取一行内容，交给解析请求行或者解析首部行函数
    HTTP_CODE DoRequest();//分析客户请求的目标文件的状态
    char* GetLine() {return m_read_buf+m_start_line; }//数组首地址加行偏移
    
    bool ProcessWrite(HTTP_CODE ret);//写HTTP应答的入口函数
    bool AddResponse(const char* format,...);//加内容,最终都是调用该函数
    bool AddContent(const char* content);//加文档内容
    bool AddStatusLine(int status,const char* title);//加状态行
    bool AddHeaders(int content_length);//加首部行
    void Unmap();//释放给定的有mmap创建的内存空间
    

};

#endif