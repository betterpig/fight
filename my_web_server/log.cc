#include "log.h"
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

Log::Log()
{
    m_count=0;
    m_is_async=false;
}

Log::~Log()
{
    if(!m_fp)
        fclose(m_fp);
    delete m_buf;
}

bool Log::init(const char* file_name,int log_buf_size,int split_lines,int max_queue_size)
{
    if(max_queue_size>=1)//如果定义了阻塞队列的大小，就是异步写
    {
        m_is_async=true;
        m_log_queue=new BlockQueue<string> (max_queue_size);//生成阻塞队列
        pthread_t tid;
        pthread_create(&tid,nullptr,FlushLogThread,nullptr);//创建往文件中写日志的线程
    }

    m_log_buf_size=log_buf_size;
    m_buf=new char[m_log_buf_size];
    memset(m_buf,'\0',sizeof(m_buf));
    m_split_lines=split_lines;

    time_t t=time(nullptr);//获取当前日期
    struct tm* sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;

    const char* p=strrchr(file_name,'/');//返回字符串中最后一次出现/的地址
    char log_full_name[256]={0};//日志文件的全名：路径+日期+指定后缀名
    if(p==nullptr)//如果输入的filename不包含路径，那就保存到当前工作目录
        //当前目录：日期+给定文件名
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,file_name);
    else//如果输入的filename包含路径
    {
        strcpy(log_name,p+1);
        strncpy(dir_name,file_name,p-file_name+1);//把指定路径替换默认路径
        //路径+日期+给定的文件名
        snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name,my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,log_name);
    }

    m_today=my_tm.tm_mday;
    m_fp=fopen(log_full_name,"a");//打开文件，返回文件指针
    //open是系统调用，返回文件描述符，并未真正读入数据，需要使用数据时还是要通过read函数
    //fopen是c库函数，返回文件指针，即已经将部分文件数据读入到为该文件创建的内存缓冲区中，读入多少取决于缓冲区的大小。写也是先写到缓冲区，默认缓冲区满才会写到文件中
    if(m_fp==nullptr);
        return false;
    return true;
}

void Log::WriteLog(int level,const char* format,...)
{
    struct timeval now={0,0};
    gettimeofday(&now,nullptr);//获得当前时间
    time_t t=now.tv_sec;
    struct tm* sys_tm=localtime(&t);//获得当前日期
    struct tm my_tm=*sys_tm;
    char s[16]={0};

    switch (level)
    {
    case 0:
        strcpy(s,"[debug]:");
        break;
    case 1:
        strcpy(s,"[info]:");
        break;
    case 2:
        strcpy(s,"[warn]:");
        break;
    case 3:
        strcpy(s,"[error]:");
        break;
    default:
        strcpy(s,"[info]:");
        break;
    }

    locker.Lock();//涉及到日志文件的关闭和创建，加锁。因为很可能多个线程同时遇到该情况
    m_count++;
    if(m_today!=my_tm.tm_mday || m_count % m_split_lines==0)//如果不是同一天，或者已经到达了最大行数，就生成新的日志文件
    {
        char new_log[256]={0};
        fflush(m_fp);//因为要创建新文件了，所以把原文件缓冲区的内容赶紧写进文件中
        fclose(m_fp);//关闭原文件
        char tail[16]={0};//后缀：日期
        snprintf(tail,16,"%d_%02d_%02d_",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);
        if(m_today!=my_tm.tm_mday)
        {
            snprintf(new_log,255,"%s%s%s",dir_name,tail,log_name);
            m_today=my_tm.tm_mday;
            m_count=0;
        }
        else//如果是行数超过了最大行，则在文件名后面加上是今天的第几个文件
        {
            snprintf(new_log,255,"%s%s%s.%lld",dir_name,tail,log_name,long(m_count/m_split_lines));
        }
        m_fp=fopen(new_log,"a");//以追加的方式打开文件，可读可写
    }

    locker.Unlock();
    va_list valst;//可变参数的解析
    va_start(valst,format);
    string log_str;
    locker.Lock();
    //因为snprintf在将字符串复制到m_buf所指向的内存中时，会在结尾加‘\0'空字符，所以不用每次给m_buf清零也可以
    int n=snprintf(m_buf,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s",//m_buf是所有线程共享的，所以要上锁
                    my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,
                    my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);
    int m=vsnprintf(m_buf+n,m_log_buf_size-1,format,valst);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\n';
    log_str=m_buf;//用char*指针给string类型对象赋值，则会将char*所指向的字符串复制给string
    locker.Unlock();
    if(m_is_async)//如果异步则将string加入到阻塞队列中
    //这里原作者是写成了 if(m_is_async && !m_log_queue->full())，其意图是当队列满时直接写到文件缓冲区中，相当于转异步为同步
    {
        if( !m_log_queue->full())//若像我这样写，则队列满时内容就被丢弃了
            m_log_queue->push(log_str);//没有满就push
    }
    else
    {
        locker.Lock();
        fputs(log_str.c_str(),m_fp);
        locker.Unlock();
    }
    va_end(valst);
}

void Log::Flush(void)
{
    locker.Lock();
    fflush(m_fp);//强制刷新缓冲区
    locker.Unlock();
}