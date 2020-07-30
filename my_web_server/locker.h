#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class Sem//信号量
{
private:
    sem_t m_sem;
public:
    Sem(unsigned int init_value=0)
    {
        if(sem_init(&m_sem,0,init_value) !=0 )
            throw std::exception();//构造函数没有返回值，需要通过抛出异常来报告错误
    }
    ~Sem()
    {
        sem_destroy(&m_sem);
    }
    bool Wait() {return sem_wait(&m_sem) ==0; }
    bool Post() {return sem_post(&m_sem)==0; }
};

class Locker//互斥锁
{
private:
    pthread_mutex_t m_mutex;
public:
    Locker()
    {
        if(pthread_mutex_init(&m_mutex,nullptr) !=0 )
            throw std::exception();
    }
    ~Locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    bool Lock() {return pthread_mutex_lock(&m_mutex) ==0; }
    bool Unlock()   {return pthread_mutex_unlock(&m_mutex); }
};

class ConditionalPara//条件变量
{
private:
    pthread_mutex_t m_mutex;//条件变量需要用到锁
    pthread_cond_t m_cond;

public:
    ConditionalPara()
    {
        if(pthread_mutex_init(&m_mutex,nullptr)!=0)//初始化锁
            throw std::exception();
        if(pthread_cond_init(&m_cond,nullptr)!=0)//初始化条件变量
        {
            pthread_mutex_destroy(&m_mutex);//失败的话把锁给释放了
            throw std::exception();
        }
    }
    ~ConditionalPara()
    {
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

    bool Wait()
    {
        int ret=0;
        pthread_mutex_lock(&m_mutex);
        ret=pthread_cond_wait(&m_cond,&m_mutex);//在调用该函数之前，就要对锁加锁
        pthread_mutex_unlock(&m_mutex);
        return ret==0;
    }
    bool Signal()   {return pthread_cond_signal(&m_cond)==0;}
};

#endif