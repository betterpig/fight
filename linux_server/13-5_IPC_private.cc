#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun//推荐格式
{
    int val;//用于SETVAL命令，将信号量的值semval设置为val
    struct semid_ds* buf;//用于IPC_STAT 和 IPC_SET命令，
    unsigned short int* array;//用于GETALL 和 SETALL命令
    struct seminfo* _buf;//用于IPC_INFO命令
};

void PV(int sem_id,int op)//给定信号量集表示符和要执行的操作,op大于0就是v操作，小于0就是p操作
{
    struct sembuf sem_b;//操作结构体
    sem_b.sem_num=0;//指定要操作的信号量的编号：第一个
    sem_b.sem_op=op;//指定采取的操作
    sem_b.sem_flg=SEM_UNDO;//当进程退出时取消正在进行的操作
    semop(sem_id,&sem_b,1);
}

int main(int argc,char* argv[])
{
    int sem_id =semget(IPC_PRIVATE,1,0666);//创建信号量集，指定该集合包含的信号量个数
    union semun sem_un;
    sem_un.val=1;
    semctl(sem_id,0,SETVAL,sem_un);//对信号量集sem_id中的编号为0的信号量，设置其值为sem_un.val的值

    pid_t id=fork();//创建子进程
    if(id<0)
        return 1;
    else if(id==0)//返回值为0，说明当前处于子进程
    {
        printf("child try to get binary sem\n");
        PV(sem_id,-1);//对信号量集sem_id中的编号为0的信号量执行P操作，子进程可以操作父进程创建的信号量集
        printf("child get the sem and would release it after 5 seconds\n");
        sleep(5);
        PV(sem_id,1);//执行V操作
        exit(0);//子进程退出
    }
    else//返回值非0，说明当前处于父进程
    {
        sleep(2);
        printf("parent try to get binary sem\n");
        PV(sem_id,-1);
        printf("parent get the sem and would release it after 5 seconds\n");
        sleep(5);
        PV(sem_id,1);
    }
    waitpid(id,NULL,0);//等待子进程退出并释放其所占资源
    semctl(sem_id,0,IPC_RMID,sem_un);//删除信号量集，所有信号量都被删除，第二个参数：信号量编号，被忽略
    return 0;
}