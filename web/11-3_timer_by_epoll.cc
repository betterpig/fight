#define TIMEOUT 5000

int timeout=TIMEOUT;
time_t start=time(nullptr);
time_t end=time(nullptr);

whiel(1)
{
    printf("the timeout is now %d mil-seconds\n",timeout);
    start=time(nullptr);
    int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,timeout);
    if((number<0) && (errno!=EINTR))
    {
        printf("epoll failure\n");
        break
    }

    if(number==0)//返回0表示没有就绪事件，那为什么会返回呢，因为timeout时间到了。然后就可以处理定时任务了
    {
        timeout=TIMEOUT;//需要重置超时时间，因为下面的代码会修改timeout，所以需要重置。
        continue;//比如第一次，有就绪事件，然后timeout被修改了，第二次没有就绪事件，到期了，这时候timeout并不等于TIMEOUT了，所以需要重置
    }

    end=time(nullptr);//如果有就绪事件，说明此时还没到期
    timeout-=(end-start)*1000;//计算剩余时间：开始的超时时间-本次循环过去的时间
    if(timeout<=0)//当剩余时间刚好为0时，说明就绪事件和到期一起发生了，这时候也要处理定时任务，
        timeout=TIMEOUT;
    
}