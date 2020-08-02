#include "http_conn.h"
#include "connection_pool.h"
#include "log.h"

const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_form="Your request has bad syntax or is inherently impossible to saticfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_form="You do not have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_form="The requested file was not found on this server.\n";
const char* error_500_title="Internal Error";
const char* error_500_form="There was an unusual problem serving the requested file.\n";
const char* doc_root="/home/sing/code/fight/my_web_server/root";

map<string,string> users;
Locker locker;

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

static void Removefd(int epollfd,int fd)//将文件描述符fd从内核事件表中移除，并关闭该文件
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void Modfd(int epollfd,int fd,int ev)//在内核事件表中修改该文件的注册事件
{
    epoll_event event;
    event.data.fd=fd;
    event.events=ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;//并上给定事件，ET、ONESHOT、RDHUP（TCP连接被对方关闭）这三个一直有
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

int HttpConn::m_user_count=0;//类的静态数据成员只能在类外以类作用域的方式定义并初始化
int HttpConn::m_epollfd=-1;

void HttpConn::CloseConn(bool real_close)
{
    if(real_close && (m_sockfd!=-1))//检查连接描述符是否有效
    {
        Removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;//客户数量减一
    }
}

void HttpConn::Init(int sockfd,const sockaddr_in& addr)
{
    m_sockfd=sockfd;//给类内数据成员赋值
    m_address=addr;
    int reuse=1;
    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));//强制进程立即使用处于TIME_WAIT状态的端口
    Addfd(m_epollfd,sockfd,true);//将该连接描述符添加到内核事件表中
    m_user_count++;//客户数量加一
    Init();//类的其他数据成员初始化
}

void HttpConn::Init()
{
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_linger=false;
    m_method=GET;//方法默认为GET
    m_url=0;
    m_version=0;
    m_content_length=0;
    m_host=0;
    m_start_line=0;
    m_checked_idx=0;
    m_read_idx=0;
    m_write_idx=0;
    m_write_idx=0;
    memset(m_read_buf,'\0',READ_BUFFER_SIZE);//清零
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILENAME_LEN);
}

HttpConn::LINE_STATUS HttpConn::ParseLine()//截取出一行给解析函数分析内容
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
            {//把回车换行符、空格符、制表符等置为空，是因为后面很多函数，都是根据空字符来判断字符串结束的！
                m_read_buf[m_checked_idx++] = '\0';//将回车符置为空
                m_read_buf[m_checked_idx++] = '\0';//将换行符置为空，checked_idx自增两次后，到达了下一行的起始位置
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
    while(true)//因为是oneshot事件，所以要一直读完数据
    {//从连接描述符connction_fd对应的“文件”读数据，放在buffer+read_index偏移后的指针指向的内存中，该缓冲区还剩下BUFFER_SIZE-read_index长度的内存
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);//data_read保存实际读取到的数据长度
        if(bytes_read==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)//这两个错误码表明数据读取结束
                break;
            return false;
        }
        else if(bytes_read==0)//当读数据函数返回0时，表明对方关闭了连接
        {
            return false;
        }
        m_read_idx+=bytes_read;//read_idx总是指向已保存数据的下一个字节位置
    }
    return true;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char* text)//解析请求行
{//请求行的格式 GET http://www.xyz.edu.cn/dir/index.html HTTP/1.1
//url格式  协议：//域名：端口/文件路径
    m_url=strpbrk(text," \t");//返回空格或制表符在temp字符串中第一次出现的位置
    if(!m_url)
        return BAD_REQUEST;//如果没有空格和制表符，说明有问题
    *m_url++='\0';//先将当前字符（空格或制表符）置为空，再把指针移动到下一个位置
    //方法
    char* method=text;
    if(strcasecmp(method,"GET") ==0 )//检查temp中是否包含“GET”
        m_method=GET;
    else if(strcasecmp(method,"POST") ==0 )
    {
        m_method=POST;
        cgi=1;
    }
    else
        return BAD_REQUEST;//只支持GET
    //url
    m_url += strspn(m_url, " \t");//在url字符串中查找空格和制表符，返回其不匹配的第一个位置，也就是说，url现在指向第一个h
    //version
    m_version =strpbrk(m_url," \t");//在url起始的字符串中查找空格或制表符，返回第一次出现的位置，也就是H的前一个空格
    if(!m_version)//没有空格，连在一起了，就没有版本号
        return BAD_REQUEST;
    *m_version++='\0';//将空格或制表符置为空，再把version指针移到下一位，现在version就指向H
    m_version+=strspn(m_version, " \t");//避免有很多空格的情况，先把空格都跳过，就指向了H
    if(strcasecmp(m_version, "HTTP/1.1") != 0)//在version起始的字符串中查找该字符串
        return BAD_REQUEST;
    
    //url
    if(strncasecmp(m_url, "http://",7) == 0)//在url指针起始的字符串中找该字段
    {
        m_url+=7;//偏移到//之后，指向w
        m_url=strchr(m_url, '/');//把偏移后的url指向url中第一次出现/的位置,后面至少还有一个/，因为主机：端口和文件路径肯定要用/隔开
    }
    if(!m_url || m_url[0] != '/')//如果后面没有/，或者指向的不是/，那就没有文件路径，不是正确的url格式
        return BAD_REQUEST;
    
    m_check_state=CHECK_STATE_HEADER;//转向下一个状态：分析首部行
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeaders(char* text)//解析首部行
{//因为每次都是传来一行，所以每次只会进入一个if中
    if(text[0] == '\0')//遇到空行，说明HTTP请求已经解析完了
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0)//连接设置
    {
        text+=11;
        text+=strspn(text," \t");//跳过空格和制表符
        if(strcasecmp(text,"keep-alive")==0)//如果是keep-alive，linger置为true
            m_linger=true;
    }
    else if(strncasecmp(text,"Content-Length:",15)==0)//请求内容的长度
    {
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);//将字符转换为长整型
    }
    else if(strncasecmp(text, "Host:",5)==0)//比较前5个字符
    {
        text+=5;//移到下一个位置
        text+=strspn(text, " \t");//找到空格和制表符并跳到其后的第一个位置
        m_host=text;
    }
    else    {}
        //printf("oop! unknow header %s\n",text);//这里只解析了host和空行，实际还有其他首部行：connection等等
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseContent(char* text)
{//这里并没有真正解析请求内容，只是检查是否被完整读入，事实上请求报文也很少有内容实体主体
    if(m_read_idx>=(m_content_length+m_checked_idx))//当已读数据指针大于待检测指针+请求内容长度时，说明剩下的待检测的就是请求内容
    {
        text[m_content_length]='\0';//这里书里面的代码没有问题，传进来的text已经指向了请求内容的开头
        m_string=text;
        return GET_REQUEST;
    }
    
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ProcessRead()//主状态机，用于从buffer中取出所有完整的行
{
    LINE_STATUS linestatus=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    while(( (m_check_state==CHECK_STATE_CONTENT) && (linestatus==LINE_OK)) || (linestatus=ParseLine())==LINE_OK)//取出一行数据是否格式正确并返回结果，并已经将checked_index递增到下一行的起始位置
    {//parse_line函数保证了每次传给解析请求行和首部行函数的text，其后面都有空字符，作为字符串的结束标志
    //即让它们每次能够只分析一行
        text=GetLine();//text指向当前行的起始位置
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
                return DoRequest();
            break;
        }
        case CHECK_STATE_CONTENT://解析实体主体，通常不用
        {
            ret=ParseContent(text);
            if(ret==GET_REQUEST)
                return DoRequest();//分析请求报文结束，下面开始响应请求
            linestatus=LINE_OPEN;//如果已读数据少于已分析数据加content_length，说明还没读完整
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
}

void HttpConn::GetDataBase(connection_pool* connpool)
{
    MYSQL* mysql=nullptr;
    connectionRAII mysqlcon(&mysql,connpool);
    if(mysql_query(mysql,"SELECT user_name,passwd FROM user_data"))
        LOG_ERROR("mysql errorno is %d",mysql_error(mysql));

    MYSQL_RES* result=mysql_store_result(mysql);
    int num_fieleds=mysql_num_fields(result);
    MYSQL_FIELD *fields=mysql_fetch_fields(result);
    while(MYSQL_ROW row=mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1]=temp2;
    }
}


HttpConn::HTTP_CODE HttpConn::DoRequest()//分析客户请求的目标文件，存在，可读且不是目录，则将其映射到内存中，并返回成功
{
    strcpy(m_real_file,doc_root);//先将服务器根目录复制到m_real_file中
    int len=strlen(doc_root);

    const char* p=strrchr(m_url,'/');
    LOG_INFO("request option is %d",*(p+1)-'\0');
    if(cgi==1 && ( *(p+1)=='2' || *(p+1)=='3'))
    {
        char name[100],passward[100];
        memset(name,'\0',100);
        memset(passward,'\0',100);
        int i;
        for(i=5;m_string[i]!='&';++i)
            name[i-5]=m_string[i];
        name[i-5]='\0';
        int j=0;
        
        for(i=i+10;m_string[i]!='\0';++i)
            passward[j++]=m_string[i];
        passward[j]='\0';
        if(*(p+1)=='3')
        {
            if(users.find(name)==users.end())
            {
                char* sql_insert=(char*) malloc( sizeof(char)*500 );
                strcpy(sql_insert,"INSERT INTO user_data(user_name,passwd) VALUES(");
                strcat(sql_insert,"'");
                strcat(sql_insert,name);
                strcat(sql_insert,"', '");
                strcat(sql_insert,passward);
                strcat(sql_insert,"')");
                
                locker.Lock();
                int res=mysql_query(mysql,sql_insert);
                users.insert(pair<string,string>(name,passward));
                locker.Unlock();
                free(sql_insert);

                if(!res)
                    strcpy(m_url,"/log.html");
                else 
                    strcpy(m_url,"/registerError.html");
            }
            else
                strcpy(m_url,"/registerError.html");
        }
        else if(*(p+1)=='2')
        {
            if(users.find(name) != users.end() && users[name]==passward)
                strcpy(m_url,"/picture.html");
            else
                strcpy(m_url,"/logError.html");
        }
    }

    if(*(p+1)=='\0')
    {
        char* m_url_new=(char*) malloc(sizeof(char)*100);
        strcpy(m_url_new,"/welcome.html");
        strncpy(m_real_file+len,m_url_new,strlen(m_url_new));
        free(m_url_new);
    }
    else if(*(p+1)=='0')
    {
        char* m_url_new=(char*) malloc(sizeof(char)*100);
        strcpy(m_url_new,"/register.html");
        strncpy(m_real_file+len,m_url_new,strlen(m_url_new));
        free(m_url_new);
    }
    else if(*(p+1)=='1')
    {
        char* m_url_new=(char*) malloc(sizeof(char)*100);
        strcpy(m_url_new,"/log.html");
        strncpy(m_real_file+len,m_url_new,strlen(m_url_new));
        
        free(m_url_new);
    }
    else
        strncpy(m_real_file+len,m_url,strlen(m_url));

    if(stat(m_real_file,&m_file_stat)<0)//查看文件状态失败->目标文件不存在
    {
        //printf("file is not exist,errno is: %d\n",errno);
        return NO_RESOURCE;
    }
    if(!(m_file_stat.st_mode & S_IROTH))//当前用户没有读取目标文件的权限
        return FORBIDDEN_REQUEST;

    if(S_ISDIR(m_file_stat.st_mode))//目标文件是一个目录
        return BAD_REQUEST;

    int fd=open(m_real_file,O_RDONLY);//以只读的方式打开该文件，获得文件描述符
    //将文件描述符fd对应的文件映射到内存中，返回该内存的首地址
    //内存长度即文件大小，该内存可读，进程私有（对内存的修改不会修改源文件，从文件的相对开头偏移位置为0的地方开始映射
    m_file_address=(char*) mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);//关闭文件，现在内存中已经有该文件的一份副本了
    return FILE_REQUEST;//返回文件请求成功标志
}

void HttpConn::Unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);//释放之前由mmap创建的映射内存
        m_file_address=0;//将指针设为空指针
    }
}

bool HttpConn::AddResponse(const char* format,...)//往写缓冲区写入待发送的数据
{//可变参数函数，像printf一样使用
    if(m_write_idx>=WRITE_BUFFER_SIZE)//超过缓冲区大小
        return false;
    va_list arg_list;//处理可变参数的一组宏：va_list va_start va_end
    va_start(arg_list,format);
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx))
        return false;
    m_write_idx+=len;//更新写指针的位置
    va_end(arg_list);
    return true;
}

bool HttpConn::AddStatusLine(int status,const char* title)
{
    return AddResponse("%s %d %s\r\n","HTTP/1.1",status,title);//状态行：版本、状态码、短语
}

bool HttpConn::AddHeaders(int content_len)//首部行
{
    AddResponse("Content-Length: %d\r\n",content_len);//相应内容长度
    AddResponse("Connection: %s\r\n",(m_linger==true)?"keep-alive":"close");//连接状态
    AddResponse("Content-Type: %s; charset=%s\r\n","text/html","UTF-8");
    AddResponse("%s","\r\n");//空行
}

bool HttpConn::AddContent(const char* content)
{
    return AddResponse("%s",content);//实体主体
}

bool HttpConn::ProcessWrite(HTTP_CODE ret)
{
    switch(ret)//根据状态码写入不同内容
    {
        case INTERNAL_ERROR://内部错误
        {
            AddStatusLine(500,error_500_title);
            AddHeaders(strlen(error_500_form));
            if(!AddContent(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST://请求无效
        {
            AddStatusLine(400,error_400_title);
            AddHeaders(strlen(error_400_form));
            if(!AddContent(error_400_form))
                return false;
            break;
        }
        case NO_RESOURCE://未找到请求的资源
        {
            AddStatusLine(404,error_404_title);
            AddHeaders(strlen(error_404_form));
            if(!AddContent(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST://禁止访问
        {
            AddStatusLine(403,error_403_title);
            AddHeaders(strlen(error_403_form));
            if(!AddContent(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST://请求成功
        {
            AddStatusLine(200,ok_200_title);//状态行
            if(m_file_stat.st_size!=0)
            {
                AddHeaders(m_file_stat.st_size);//首部行
                m_iv[0].iov_base=m_write_buf;//第一块内存是写缓冲区
                m_iv[0].iov_len=m_write_idx;//第一块内存的长度，就是写指针的值
                m_iv[1].iov_base=m_file_address;//第二块内存是目标文件的映射内存
                m_iv[1].iov_len=m_file_stat.st_size;//第二块内存的长度，就是文件大小
                m_iv_count=2;//指示io向量共包含了多少块内存，循环的时候要用到
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

bool HttpConn::Write()//通过writev把要发送的东西发送出去
{
    int temp=0;
    int bytes_have_send=0;//已经发送的字节数
    int bytes_to_send=0;//还未发送的字节数
    for(int i=0;i<m_iv_count;i++)
        bytes_to_send+=m_iv[i].iov_len;
    if(bytes_to_send==0)//写缓冲区还没数据
    {
        Modfd(m_epollfd,m_sockfd,EPOLLIN);//继续监听可读事件
        Init();
        return true;
    }
    while(1)
    {
        temp=writev(m_sockfd,m_iv,m_iv_count);//将io向量中的各块内存集中往连接描述符上写
        if(temp<=-1)
        {
            if(errno==EAGAIN)//下次这来，发送缓冲区已满
            {
                if(bytes_have_send>=m_iv[0].iov_len)
                {
                    m_iv[0].iov_len=0;
                    m_iv[1].iov_len=m_iv[1].iov_len-(bytes_have_send-m_iv[0].iov_len);
                    m_iv[1].iov_base=m_file_address+(bytes_have_send-m_iv[0].iov_len);
                }
                else
                {
                    m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;
                    m_iv[0].iov_base=m_write_buf+bytes_have_send;
                }
                Modfd(m_epollfd,m_sockfd,EPOLLOUT);//监听连接描述符上的可写事件，等可写了继续写之前没写完的内容
                return true;
            }
            Unmap();//释放映射内存
            return false;
        }

        bytes_to_send-=temp;//更新未发送的字节数已发送字节数
        bytes_have_send+=temp;
        if(bytes_to_send<=0)//如果已发送字节数大于未发送的，说明已经发完了
        {
            Unmap();
            if(m_linger)//根据linger决定是否要关闭连接
            {
                Init();
                Modfd(m_epollfd,m_sockfd,EPOLLIN);//重置oneshot，重新监听可读事件
                return true;
            }
            else
            {
                Modfd(m_epollfd,m_sockfd,EPOLLIN);//都要关闭连接了，干嘛还要修改
                return false;
            }
        }
    }
}

void HttpConn::Process()
{
    HTTP_CODE read_ret=ProcessRead();//读取HTTP报文
    if(read_ret==NO_REQUEST)
    {
        Modfd(m_epollfd,m_sockfd,EPOLLIN);//没有请求，就重置该连接描述符的oneshot事件，让该连接描述符可以被其他线程接管
        return;
    }
    bool write_ret=ProcessWrite(read_ret);//根据状态码发送不同的HTTP响应报文
    if(!write_ret)//写失败，关闭连接
        CloseConn();
    Modfd(m_epollfd,m_sockfd,EPOLLOUT);//写成功则注册该连接描述符上的可写事件，当发送缓冲有空位时，就可写，就能调用write函数，把要发送的数据写到发送缓冲中，等待发送出去了
}