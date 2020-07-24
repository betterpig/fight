#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "14-7_locker.h"

template<typename T>
class ThreadPool
{
private:
    int m_thread_number;
    int m_max_requests;
    pthread_t* m_threads;
    std::list<T*> m_work_queue;
    Locker m_queue_locker;
    Sem m_queue_stat;
    bool m_stop;

    static void* Worker(void* arg);
    void Run();

public:
    ThreadPool(int thread_number=8,int max_requests=10000);
    ~ThreadPool();
    bool Append(T* request);
};

template<typename T>
ThreadPool<T>::ThreadPool(int thread_number,int max_requests)
:m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(nullptrl)
{
    if((thread_number<=0) || (max_requests<=0))
        throw std::exception();
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(int i=0;i<thread_number;++i)
    {
        printf("create the %dth thread\n",i);
        if(pthread_create(m_threads+i,nullptr,Worker,this)!=0)
        {
            delete []m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete []m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete []m_threads;
    m_stop=true;
}

template<typename T>
bool ThreadPool<T>::Append(T* request)
{
    m_queue_locker.Lock();
    if(m_work_queue.size()>m_max_requests)
    {
        m_queue_locker.Unlock();
        return false;
    }
    m_work_queue.push_back(reqest);
    m_queue_locker.Unlock();
    m_queue_stat.Post();
    return true;
}

template<typename T>
void* ThreadPool<T>::Worker(void* arg)
{
    ThreadPool* pool=(ThreadPool*) arg;
    pool->Run();
    return pool;
}

template<typename T>
void ThreadPool<T>::Run()
{
    while(!m_stop)
    {
        m_queue_stat.wait();
        m_queue_locker.Lock();
        if(m_work_queue.empty())
        {
            m_queue_locker.Unlock();
            continue;
        }
        T* request=m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_locker.Unlock();
        if(!request)
            continue;
        request->Process();
    }   
}

#endif