#include "unp.h"

void sig_chld(int signo)
{
    pid_t pid;
    int stat;
    while((pid=waitpid(-1,&stat,WNOHANG))>0)
    //pid=wait(&stat);
        printf("child %d terminated \n",pid);
    //fflush ( stdout ) ;
    return;
}