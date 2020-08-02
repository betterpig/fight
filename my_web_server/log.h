#ifndef LOG_H
#define LOG_H

#include "locker.h"
#include <string>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include "block_queue.h"
using namespace std;
class Log
{
public:
    static Log* GetInstance()//单例模式
    {
        static Log instance;//局部静态变量，只定义一次，保证了该类的对象只有一个
        return &instance;
    }
    bool init(const char* file_name,int log_buf_size=8192,int split_lines=5000000,int max_queue_size=0);
    static void* FlushLogThread(void* args)//写线程要绑定的函数
    {
        Log::GetInstance()->AsyncWriteLog();
    }

    void WriteLog(int level,const char* format,...);//将内容写到缓冲区，变参数
    void Flush(void);//强制刷新系统的缓冲区，将内容写进文件

private:
    Log();
    virtual ~Log();
    void* AsyncWriteLog()
    {
        string single_log;//这里本来队列中就保存了要写的内容，这里又定义了string对象，重复了，要是能把string的指针传回来，就可以省空间
        while(m_log_queue->pop(single_log))
        {
            locker.Lock();//给日志文件上锁，确保同一时间只有一个线程在写文件，不然可能不同线程的内容凑在一起了
            fputs(single_log.c_str(),m_fp);
            locker.Unlock();
        }
    }

private:
    char dir_name[128];//目录名
    char log_name[128];//文件名
    int m_split_lines;//最大行数
    int m_log_buf_size;//
    long long m_count;//已写行数
    int m_today;//当前日期
    FILE* m_fp;//日志文件指针
    char* m_buf;//写缓冲区
    BlockQueue<string>* m_log_queue;//阻塞队列
    bool m_is_async;//是否异步写
    Locker locker;//锁
};

#define LOG_DEBUG(format,...) Log::GetInstance()->WriteLog(0,format,__VA_ARGS__)//宏定义，遇到时直接替换成后面的内容
#define LOG_INFO(format,...) Log::GetInstance()->WriteLog(1,format,__VA_ARGS__)
#define LOG_WARN(format,...) Log::GetInstance()->WriteLog(2,format,__VA_ARGS__)
#define LOG_ERROR(format,...) Log::GetInstance()->WriteLog(3,format,__VA_ARGS__)

#endif