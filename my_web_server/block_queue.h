#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <pthread.h>

template<typename T>
class BlockQueue
{
public:
    BlockQueue(int max_size=1000)
    {
        if(max_size<=0)
            exit(-1);
        m_max_size=max_size;
        m_array=new T[max_size];
        m_size=0;
        m_front=-1;
        m_back=-1;

        m_mutex=new pthread_mutex_t;
        m_cond=new pthread_cond_t;
        pthread_cond_init(m_cond,nullptr);
    }

    bool push(const T& item)
    {
        pthread_mutex_lock(m_mutex);
        if(m_size>=m_max_size)
        {
            pthread_cond_broadcast(m_cond);
            pthread_mutex_unlock(m_mutex);
            return false;
        }
        m_back=(m_bcak+1) % m_max_size;
        m_array[m_back]=item;
        m_size++;

        pthread_cond_broadcast(m_cond);
        pthread_mutex_unlock(m_mutex);
    }

    bool pop(T& item)
    {
        pthread_mutex_lock(m_mutex);
        while(m_size<=0)
        {
            if(0!=pthread_cond_wait(m_con,m_mutex))
            {
                pthread_mutex_unlock(m_mutex);
                return false;
            }
        }
        m_front=(m_front+1) % m_max_size;
        item=m_array[m_front];
        m_size--;
        pthread_mutex_unlock(m_mutex);
        return true;
    }

private:
    int m_max_size=max_size;
    T* m_array=new T[max_size];
    int m_size=0;
    int m_front=-1;
    int m_back=-1;

    pthread_mutex_t* m_mutex;
    pthread_cond_t* m_cond;
};

#endif