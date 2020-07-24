#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define handle_error_en(en,msg) do{errno=en;perror(msg);exit(EXIT_FAILURE);} while(0)

static void* sig_thread(void* arg)//信号处理线程
{
    sigset_t* set=(sigset_t*) arg;//获得要捕获的信号集
    int s,sig;
    for(;;)
    {
        s=sigwait(set,&sig);//等待内核发过来信号集中的某个信号,并把该信号放在sig中，成功时返回0，失败返回错误码
        if(s!=0)
            handle_error_en(s,"sigwait");
        printf("signal handling thread got signal %d\n",signal);//成功则打印收到的信号值
    }
}

int main(int argc,char* argv[])
{
    pthread_t thread;
    sigset_t set;
    int s;

    sigemptyset(&set);//清空信号集
    sigaddset(&set,SIGQUIT);//加上要捕获的信号
    sigaddset(&set,SIGUSR1);
    s=pthread_sigmask(SIG_BLOCK,&set,nullptr);//设置信号掩码，之后创建的所有线程都将屏蔽set中的信号
    if(s!=0)
        handle_error_en(s,"pthread_sigmask");//输出错误信息
    s=pthread_create(&thread,nullptr,&sig_thread,(void*) &set);//创建信号处理线程，专门捕获信号集中的信号
    if(s!=0)
        handle_error_en(s,"pthread_create");
    pause();
}