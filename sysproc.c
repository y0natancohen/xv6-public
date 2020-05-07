#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid, signum;

  if(argint(0, &pid) < 0 || argint(1, &signum) < 0)
    return -1;

  return kill(pid,signum);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_sigprocmask(void)
{
    int sigprocmask;

    if(argint(0, &sigprocmask) < 0)
        return -1;
    
    uint old = myproc()->signal_mask;
    myproc()->signal_mask=sigprocmask;

    return old;
}

int
sys_sigaction(void) 
{
    int signum;
    struct sigaction *act;
    struct sigaction *oldact;

    if(argint(0, &signum) < 0 
        || argptr(1,(void *)&act,sizeof(struct sigaction)) < 0 
        || argptr(2,(void *)&oldact,sizeof(struct sigaction)) < 0){
        
        return -1;
    }
    
    return set_sigaction(signum,act,oldact);
}
int
sys_sigret(void)
{
// todo remove if?
  if (myproc()->tf && myproc()->user_trapframe_backup){
    sigret();
  }

  //cprintf("end sys_sigret\n");
  
  return 0;
  
}
