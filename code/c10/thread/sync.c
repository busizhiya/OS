#include "sync.h"
#include "../kernel/interrupt.h"
#include "../kernel/debug.h"
#include "thread.h"

//初始化信号量
void sema_init(struct semaphore* psema, uint8_t value){
    psema->value = value;
    list_init(&psema->waiters);
}

//初始化锁plock
void lock_init(struct lock* plock){
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore,1); //信号量初始值为1
}

/*假如同时可以执行三个进程, 那么信号量初始值可以设置为3, 故sema_down最多
支持同时加入三个线程获得锁, 故需要将sema_up/down与锁的获取与释放分开
简而言之: 信号量是锁的一种实现方式, 锁是信号量的一种特殊情况(二元信号量)*/

//信号量down(获取)
void sema_down(struct semaphore* psema){
    enum intr_status old_status = intr_disable();
    while(psema->value == 0){
        //锁已被他人持有

        ASSERT(!elem_find(&psema->waiters,&running_thread()->general_tag))
        if(elem_find(&psema->waiters,&running_thread()->general_tag)){
            PANIC("sema_down: thread blocked has been in waiters list\n");
        }
        //先排队
        list_append(&psema->waiters, &running_thread()->general_tag);
        //交出执行权, 等待锁释放后再调度
        thread_block(TASK_BLOCKED);
    }

    //被唤醒
    psema->value--;
    ASSERT(psema->value == 0);
    intr_set_status(old_status);
}

//信号量up(释放)
void sema_up(struct semaphore* psema){
    enum intr_status old_status = intr_disable();
    ASSERT(psema->value == 0);//若N元信号量则为<N
   
    //询问是否存在等待锁的线程
    if(!list_empty(&psema->waiters)){
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        //释放后优先让堵塞的线程cpu
        thread_unblock(thread_blocked);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_status);
}

void lock_acquire(struct lock* plock){
    //先判断自己是否持有锁, 防止死锁(自己等待自己释放锁)
    if(plock->holder != running_thread())
    {
        sema_down(&plock->semaphore);   //P操作
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    }else{
        //允许多次获取
        plock->holder_repeat_nr++;  //连续多次获得
    }
}

void lock_release(struct lock* plock){
    //只有当前锁的持有者才能释放锁
    ASSERT(plock->holder == running_thread());
    
    if(plock->holder_repeat_nr > 1){
        plock->holder_repeat_nr--;
        return ;
    }
    ASSERT(plock->holder_repeat_nr == 1);
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);
}