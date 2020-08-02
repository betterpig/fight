#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <pthread.h>

template<typename T>
class BlockQueue//阻塞队列类
{
public:
    BlockQueue(int max_size=1000)
    {
        if(max_size<=0)
            exit(-1);
        m_max_size=max_size;//容量
        m_array=new T[max_size];//用动态数组实现循环队列，来存储元素
        m_size=0;//实际元素个数
        m_front=-1;//队列头
        m_back=-1;//队列尾

        m_mutex=new pthread_mutex_t;//互斥锁
        m_cond=new pthread_cond_t;//调节变量
        pthread_cond_init(m_cond,nullptr);
    }

    bool push(const T& item)
    {
        pthread_mutex_lock(m_mutex);//给阻塞队列上锁，确保没有其他线程修改阻塞队列内容
        if(m_size>=m_max_size)//队列满
        {
            pthread_cond_broadcast(m_cond);//同时唤醒所有等待该条件变量的线程，如果线程之间不互斥的话，将以起继续执行，如果互斥的话，那将按获得锁的顺序依次执行，即最终每个线程都会执行，不死锁的话
            pthread_mutex_unlock(m_mutex);//解锁
            return false;
        }
        m_back=(m_back+1) % m_max_size;//队列尾加1，因为是循环队列，注意取模
        m_array[m_back]=item;//存储该元素
        m_size++;//当前元素个数加1

        pthread_cond_broadcast(m_cond);//加入了元素，那肯定要通知等待该条件变量的线程
        pthread_mutex_unlock(m_mutex);//解锁
    }

    bool pop(T& item)
    {
        pthread_mutex_lock(m_mutex);//与cond_wait里的mutex是同一把锁，相当于有两个功能：一是给阻塞队列上锁，确保同一时间只有一个线程在取元素，
        while(m_size<=0) //队列没有元素的话，线程将一直循环等待，而且由于外面上了锁，
        {//如果有多个写线程，那么其他线程都在等待获得锁，只有这个线程在阻塞，等待队列有元素
            if(0!=pthread_cond_wait(m_cond,m_mutex))//二是给该条件变量对应的线程队列上锁，保证同一时间只有一个线程入队，
            {//才不会打乱顺序或者一个出队拿了资源另一个又以为还有资源,
            //所以顺序是：调用cond_wait之前上锁，保证只有一个线程能调用cond_wait，然后该线程将放入等待条件变量的队列中，然后解锁，此时其它线程也可以进入该等待条件变量的队列
            //然后cond_wait返回后，还要上锁，这是为了保持原来没有调用该函数时的状态
                pthread_mutex_unlock(m_mutex);//cond_wait返回值不为0，调用失败，给外层的阻塞队列解锁
                return false;
            }//cond_wait是阻塞的，只有当pthread_cond_signal或者pthread_cond_broadcast函数被调用时，cond_wait才会成功返回
            //所以该函数一共有三个阶段：一：将本线程放入等待条件变量的线程队列过程 二：放入后，解锁，阻塞等待broadcast 三：被激活，上锁，函数返回
            //当函数返回成功时，说明有线程调用了broadcast，也就说明此时队列不为空， 那外层的while循环中断，程序顺序执行
        }
        m_front=(m_front+1) % m_max_size;//对头往后移动
        item=m_array[m_front];//取出队头
        m_size--;//元素个数减一
        pthread_mutex_unlock(m_mutex);//解锁
        return true;
    }

    bool full()
    {
        return m_size>m_max_size;
    }

private:
    int m_max_size;
    T* m_array;
    int m_size;
    int m_front;
    int m_back;

    pthread_mutex_t* m_mutex;
    pthread_cond_t* m_cond;
};

#endif