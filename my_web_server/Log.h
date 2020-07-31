#ifndef LOG_H
#define LOG_H

#include "locker.h"
#include <string>
#include <stdio.h>
#include "block_queue.h"
using namespace std;
class Log
{
public:
    static Log* GetInstance()
    {
        static Log instance;
        return &instance;
    }
    bool init(const char* file_name,int log_buf_size=8192,int split_lines=5000000,int max_queue_size=0);
    static void* FlushLogThread(void* args)
    {
        Log::GetInstance()->AsyncWriteLog();
    }

    void WriteLog(int level,const char* format,...);
    void Flush(void);

private:
    Log();
    virtual ~Log();
    void* AsyncWriteLog()
    {
        string single_log;
        while(m_log_queue->pop(single_log))
        {
            locker.Lock();
            fputs(single_log.c_str(),m_fp);
            locker.Unlock();
        }
    }

private:
    char dir_name[128];
    char log_name[128];
    int m_split_lines;
    int m_log_buf_size;
    long long m_ocunt;
    int m_today;
    FILE* m_fp;
    char* m_buf;
    BlockQueue<string>* m_log_queue;
    bool m_is_async;
    Locker locker;
};

#endif