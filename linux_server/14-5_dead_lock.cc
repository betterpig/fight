#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

int a=0;
int b=0;
pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void* another(void* arg)
{
    pthread_mutex_lock(&mutex_b);//尝试获得锁b
    printf("in child thread,got mutex b,waiting for mutex a\n");
    sleep(5);
    ++b;//对变量b进行修改
    pthread_mutex_lock(&mutex_a);//尝试获得锁a
    b+=a++;//对变量a b修改
    pthread_mutex_unlock(&mutex_a);//解锁
    pthread_mutex_unlock(&mutex_b);
    pthread_exit(nullptr);//退出线程
}

int main()
{
    pthread_t id;

    pthread_mutex_init(&mutex_a,nullptr);//初始化两把锁
    pthread_mutex_init(&mutex_b,nullptr);
    pthread_create(&id,nullptr,another,nullptr);//创建线程，获得该线程的id，该线程将运行another函数，不需要传入参数

    pthread_mutex_lock(&mutex_a);//尝试获得锁a
    printf("in parent thread,got mutex a,waitingfor mutex b\n");
    sleep(5);
    ++a;//获得锁a，对变量a修改
    pthread_mutex_lock(&mutex_b);//尝试获得锁b
    a+=b++;//对变量a和b修改
    pthread_mutex_unlock(&mutex_b);//先解锁b
    pthread_mutex_unlock(&mutex_a);//再解锁a。锁的获得与释放的顺序相反

    pthread_join(id,nullptr);//回收创建的子线程
    pthread_mutex_destroy(&mutex_a);//释放两把锁
    pthread_mutex_destroy(&mutex_b);
    return 0;
}