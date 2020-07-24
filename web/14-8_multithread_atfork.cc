#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

pthread_mutex_t mutex;

void* another(void* arg)
{
    printf("in child thread ,lock the mutex\n");
    pthread_mutex_lock(&mutex);
    sleep(5);
    pthread_mutex_unlock(&mutex);
}

void prepare()
{
    pthread_mutex_lock(&mutex);
}

void infork()
{
    pthread_mutex_unlock(&mutex);
}

int main()
{
    pthread_mutex_init(&mutex,nullptr);
    pthread_t id;
    pthread_create(&id,nullptr,another,nullptr);//创建线程
    sleep(1);//等待1s，等子线程已经获得锁
    pthread_atfork(prepare,infork,infork);//在fork前先尝试获得锁，因为主进程创建的线程正在持有锁，所以prepare会阻塞直到锁被释放
    int pid=fork();//然后prepare获得锁，防止其他程序再给锁加锁，然后创建好子进程，由于fork会复制锁的状态，所以父子进程都需要释放锁，即调用infork函数
    if(pid<0)//创建进程失败
    {
        pthread_join(id,nullptr);
        pthread_mutex_destroy(&mutex);
        return 1;
    }
    else if(pid==0)//进入子进程
    {
        printf("i am in the child,want to get the lock\n");
        pthread_mutex_lock(&mutex);//尝试获得锁
        printf("i can now run to here\n");
        pthread_mutex_unlock(&mutex);
        exit(0);
    }
    else
        wait(nullptr);
    pthread_join(id,nullptr);
    pthread_mutex_destroy(&mutex);
    return 0;
}