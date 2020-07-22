#ifndef TIME_HEAP
#define TIME_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>

using std::exception;

#define BUFFER_SIZE 64

class Timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    Timer* timer;
};

class Timer
{
public:
    time_t expire;
    void (*cb_func) (client_data*);
    client_data* user_data;

    Timer(int delay)
    {
        expire=time(nullptr) + delay;
    }
};

class TimeHeap
{
private:
    Timer* (*array);
    int capacity;
    int cur_size;

public:
    TimeHeap(int cap) throw (std::exception):capacity(cap),cur_size(0)
    {
        array=new Timer* [capacity];
        if(!array)
            throw std::exception();
        for(int i=0;i<capacity;++i)
            array[i]=nullptr;
    }

    TimeHeap(Timer** init_array,int size,int capacity) throw(std::exception):cur_size(size),capacity(capacity)
    {
        if(capacity<size)
            throw std::exception();
        array=new Timer* [capacity];
        if(!array)
            throw std::exception();
        for(int i=0;i<capacity;++i)
            array[i]=nullptr;
        if(size!=0)
        {
            for(int i=0;i<size;++i)
                array[i]=init_array[i];
            for(int i=(cur_size-1)/2;i>=0;--i)
                PercolateDown(i);
        }
    }

    ~TimeHeap()
    {
        for(int i=0;i<cur_size;++i)
            delete array[i];
        delete []array;
    }

    void AddTimer(Timer* timer) throw(std::exception)
    {
        if(!timer)
            return;
        if(cur_size>=capacity)
            Resize();
        int hole=cur_size++;
        int parent=0;
        for(;hole>0;hole=parent)
        {
            parent=(hole-1)/2;
            if(array[parent]->expire<=timer->expire)
                break;
            array[hole]=array[parent];
        }
        array[hole]=timer;
    }

    void DeleteTimer(Timer* timer)
    {
        if(!timer)
            return;
        timer->cb_func=nullptr;
    }
    
    Timer* top() const
    {
        if(empty())
            return nullptr;
        return array[0];
    }

    void Pop()
    {
        if(empty())
            return;
        if(array[0])
        {
            delete array[0];
            array[0]=array[cur_size--];
            PercolateDown(0);
        }
    }

    void Tick()
    {
        Timer* tmp=array[0];
        time_t cur=time(nullptr);
        while(!empty())
        {
            if(!tmp)
                break;
            if(tmp->expire>cur)
                break;
            if(array[0]->cb_func)
                array[0]->cb_func(array[0]->user_data);
            Pop();
            tmp=array[0];
        }
    }

    bool empty() const { return cur_size==0;}

private:
    void PercolateDown(int hole)
    {
        Timer* tmp=array[hole];
        int child=0;
        for(;(hole*2+1)<=cur_size-1;hole=child)
        {
            child=hole*2+1;
            if((child<cur_size-1) && (array[child+1]->expire<array[child]->expire))
                ++child;
            if(array[child]->expire<tmp->expire)
                array[hole]=array[child];
            else
                break;
        }
        array[hole]=tmp;
    }

    void Resize() throw(std::exception)
    {
        Timer** tmp=new Timer* (2*capacity);
        for(int i=0;i<2*capacity;++i)
            tmp[i]=nullptr;
        if(!tmp)
            throw std::exception();
        capacity=2*capacity;
        for(int i=0;i<cur_size;++i)
            tmp[i]=array[i];
        delete []array;
        array=tmp;
    }
};

#endif