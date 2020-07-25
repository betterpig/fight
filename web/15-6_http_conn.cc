#include "15-6_http_conn.h"
#include "14-7_locker.h"

const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_form="Your request has bad syntax or is inherently impossible to saticfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_form="You do not have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_form="The requested file was not found on this server.\n";
const char* error_500_title="Internal Error";
const char* error_500_form="There was an unusual problem serving the requested file.\n";
const char* doc_root="/var/www/html";

int SetNonBlocking(int fd)//将文件描述符设为非阻塞状态
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

void Addfd(int epollfd,int fd,bool oneshot)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET | EPOLLRDHUP;//设置为ET模式：同一就绪事件只会通知一次
    if(oneshot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

static void Removefd(int epollfd,int fd)//将文件描述符fd从内核事件表中移除
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void Modfd(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
}

int HttpConn::m_user_count=0;
int HttpConn::m_epollfd=-1;

void HttpConn::CloseConn(bool real_close)
{
    if(real_close && (m_sockfd!=1))
    {
        Removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}

void HttpConn::Init(int sockfd,const sockaddr_in& addr)
{
    m_sockfd=sockfd;
    m_address=addr;
    int reuse=1;
    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    Addfd(m_epollfd,sockfd,true);
    m_user_count++;
    Init();
}

void HttpConn::Init()
{
    m_check_state=CHECK_STATE_REQUESLINE;
    m_linger=false;
    m_method=GET;
    m_url=0;
    m_version=0;
    m_content_length=0;
    m_host=0;
    m_start_line=0;
    m_checked_idx=0;
    m_read_idx=0;
    m_write_idx=0;
    m_write_idx=0;
    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILENAME_LEN);
}

HttpConn::LINE_STATUS HttpConn::ParseLine()
{
    char temp;
    for(;m_checked_idx<m_read_idx;++m_checked_idx)//HTTP报文格式：每一行必须以回车换行符‘\r\n'结尾，且在首部行之后有一空行
    {
        temp=m_read_buf[m_checked_idx];//按顺序检查每个字符
        if(temp=='\r')//如果当前字符是回车符
        {
            if((m_checked_idx+1)==m_read_idx)//但下一个位置又没有字符了，说明数据不完整
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx+1] == '\n')//并且下一个字符是换行符，说明读取到了完整的行
            {
                m_read_buf[m_checked_idx++] = '\0';//为什么要对回车符后的两个字符置0呢？
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n')//'\r\n',\n怎么可能出现在\r前面呢？
        {
            if((m_checked_idx>1) && m_read_buf[m_checked_idx-1] == '\r')
            {
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } 
    }
    return LINE_OPEN;//没有’\r‘字符，需要继续读取客户端数据才能分析
}

bool HttpConn::Read()
{
    if(m_read_idx>=READ_BUFFER_SIZE)
        return false;
    
    int bytes_read=0;
    while(true)
    {//从连接描述符connction_fd对应的“文件”读数据，放在buffer+read_index偏移后的指针指向的内存中，该缓冲区还剩下BUFFER_SIZE-read_index长度的内存
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);//data_read保存实际读取到的数据长度
        if(bytes_read==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)
                break;
            return false;
        }
        else if(bytes_read==0)
        {
            return false;
        }
        m_read_idx+=bytes_read;
    }
    return true;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char* text)
{
    m_url=strpbrk(text," \t");//返回空格或制表符在temp字符串中第一次出现的位置
    if(!m_url)
        return BAD_REQUEST;//如果没有空格和制表符，说明有问题
    *m_url++='\0';//先将当前字符置为空，再把指针移动到下一个位置

    char* method=text;
    if(strcasecmp(method,"GET") ==0 )//检查temp中是否包含“GET”
        m_method=GET;
    else
        return BAD_REQUEST;
    
    m_url += strspn(m_url, " \t");//在url字符串中查找空格和制表符，返回其不匹配的第一个位置，也就是说，url现在指向第一个h
    m_version =strpbrk(m_url," \t");//在url起始的字符串中查找空格或制表符，返回第一次出现的位置，也就是H的前一个空格
    if(!m_version)//没有空格，就没有版本号
        return BAD_REQUEST;
    *m_version++='\0';//将空格或制表符置为空，再把versiong指针移到下一位
    m_version+=strspn(m_version, " \t");//避免有很多空格的情况，先把空格都跳过，就指向了H
    if(strcasecmp(m_version, "HTTP/1.1") != 0)//在version起始的字符串中查找该字符串
        return BAD_REQUEST;

    if(strncasecmp(m_url, "http://",7) == 0)//在url指针起始的字符串中找该字段
    {
        m_url+=7;//偏移到//之后，指向w
        m_url=strchr(m_url, '/');//把偏移后的url指向url中第一次出现/的位置，防止有超过两个/
    }

    if(!m_url || m_url[0] != '/')//后面没数据了 ，后面没/了
        return BAD_REQUEST;
    printf("The request URL is: %s\n",m_url);//这里似乎只是输出请求的文件名，而不是完整的包括域名的url
    m_check_state=CHECK_STATE_HEADER;//转向下一个状态：分析首部行
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeaders(char* text)
{
    if(text[0] == '\0')//遇到空行，说明HTTP请求已经解析完了
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0)
    {
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0)
            m_linger=true;
    }
    else if(strncasecmp(text,"Content-Length:",15)==0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }
    else if(strncasecmp(text, "Host:",5)==0)//比较前5个字符
    {
        text+=5;//移到下一个位置
        text+=strspn(text, " \t");//找到空格和制表符并跳到其后的第一个位置
        m_host=text;
    }
    else
        printf("oop! unknow header %s\n",text);//这里只解析了host和空行，实际还有其他首部行：connection等等
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseContent(char* text)
{
    if(m_read_idx>=(m_content_length+m_checked_idx))
    {
        text[m_content_length]='\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ProcessRead()
{
    LINE_STATUS linestatus=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    //主状态机，用于从buffer中取出所有完整的行
    while(( (m_check_state==CHECK_STATE_CONTENT) && (linestatus==LINE_OK)) || (linestatus=ParseLine)==LINE_OK)//取出一行数据是否格式正确并返回结果，并已经将checked_index递增到下一行的起始位置
    {
        text=GetLine();//temp指向当前行的起始位置
        m_start_line=m_checked_idx;//start_line指向下一行的起始位置。start_line只有读到完整的一行，才会更新为checked_index，也就是下一行的起始位置
        switch (m_check_state)//checkstate初始值是CHECK_STATE_REQUESTLINE
        {
        case CHECK_STATE_REQUESTLINE:
        {//若请求行正确，parse_requestline函数将会把checkstate设置为CHECK_STATE_HEADER，在下一次循环就会转到分析首部行
            ret=ParseRequestLine(text);
            if(ret==BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {//若首部行正确，返回GET_REQUEST，解析请求报文完成，返回到主函数
            ret=ParseHeaders(text);
            if(ret==BAD_REQUEST)
                return BAD_REQUEST;
            else if(ret==GET_REQUEST)
                return GET_REQUEST;
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret=ParseContent(text);
            if(ret==GET_REQUEST)
                return DoRequest();
            linestatus=LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::DoRequest()
{
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
    strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    if(stat(m_real_file,&m_file_stat)<0)
        return NO_RESOURCE;
    if(!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd=open(m_real_file,O_RDONLY);
    m_file_address=(char*) mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}

void HttpConn::Unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }
}

bool HttpConn::Write()
{
    int temp=0;
    int bytes_have_send=0;
    int bytes_to_send=m_write_idx;
    if(bytes_to_send==0);
    {
        Modfd(m_epollfd,m_sockfd,EPOLLIN);
        Init();
        return true;
    }
    while(1)
    {
        temp=writev(m_sockfd,m_iv,m_iv_count);
        if(temp<=-1)
        {
            if(errno==EAGAIN)
            {
                Modfd(m_epollfd,m_sockfd,EPOLLOUT);
                return true;
            }
            Unmap();
            return false;
        }

        bytes_to_send-=temp;
        bytes_have_send+=temp;
        if(bytes_to_send<=bytes_have_send)
        {
            Unmap();
            if(m_linger)
            {
                Init();
                Modfd(m_epollfd,m_sockfd,EPOLLIN);
                return true;
            }
            else
            {
                Modfd(m_epollfd,m_sockfd,EPOLLIN);
                return false;
            }
        }
    }
}

bool HttpConn::AddResponse(const char* format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx))
        return false;
    m_write_idx+=len;
    va_end(arg_list);
    return true;
}

bool HttpConn::AddStatusLine(int status,const char* title)
{
    return AddResponse("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool HttpConn::AddHeaders(int content_len)
{
    AddResponse("Content-Length: %d\r\n",content_len);
    AddResponse("Connection: %s\r\n",(m_linger==true)?"keep-alive":"close");
    AddResponse("%S","\r\n");
}

bool HttpConn::AddContent(const char* content)
{
    return AddResponse("%s",content);
}

bool HttpConn::ProcessWrite(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            AddStatusLine(500,error_500_title);
            AddHeaders(strlen(error_500_form));
            if(!AddContent(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            AddStatusLine(400,error_400_title);
            AddHeaders(strlen(error_400_form));
            if(!AddContent(error_400_form))
                return false;
            break;
        }
        case NO_REQUEST:
        {
            AddStatusLine(404,error_404_title);
            AddHeaders(strlen(error_404_form));
            if(!AddContent(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            AddStatusLine(403,error_403_title);
            AddHeaders(strlen(error_403_form));
            if(!AddContent(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            AddStatusLine(200,ok_200_title);
            if(m_file_stat.st_size!=0)
            {
                AddHeaders(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2;
                return true;
            }
            else
            {
                const char* ok_string="<html><body></body></html>";
                AddHeaders(strlen(ok_string));
                if(!AddContent(ok_string))
                    return false;
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    return true;
}

void HttpConn::Process()
{
    HTTP_CODE read_ret=ProcessRead();
    if(read_ret==NO_REQUEST)
    {
        Modfd(m_epollfd,m_sockfd,EPOLLIN);
        return;
    }
    bool write_ret=ProcessWrite(read_ret);
    if(!write_ret)
        CloseConn();
    Modfd(m_epollfd,m_sockfd,EPOLLOUT);
}