#include "Log.h"

bool Log::init(const char* file_name,int log_buf_size,int split_lines,int max_queue_size)
{
    if(max_queue_size>=1)
    {
        m_is_async=true;
    }
}