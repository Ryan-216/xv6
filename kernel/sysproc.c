#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  // if(growproc(n) < 0)
  //   return -1;

  struct proc* p = myproc();

  if(n>0){
    p->sz += n; //惰性分配，只改变sz字段
  } else if(p->sz + n > 0){
    p->sz = uvmdealloc(p->pagetable, p->sz, p->sz + n); //如果是减少内存，还是要马上执行,要检查减去内存后是否大于0
  } else {
    return -1;
  }
  return addr;
}

uint64
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

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;

  if(argint(0, &mask)<0)  //获取用户程序传入的数据
    return -1;

  myproc()->hy_syscall_trace = mask;  //设置调用进程的kama_syscall_trace掩码mask
  return 0;
}

uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  hy_freebytes(&info.freemem);
  hy_procnum(&info.nproc);

  //获取用户sysinfo结构体的虚拟地址
  uint64 dstaddr;
  argaddr(0, &dstaddr); //因为int sysinfo(struct sysinfo*);

  //从内核空间拷贝数据到用户空间
  if(copyout(myproc()->pagetable, dstaddr, (char*)&info, sizeof info) < 0){
    return -1;
  }
  return 0;
}

uint64
sys_top(void)
{
  uint64 pro_sum[5] = {0,0,0,0,0};
  struct sysinfo info;
  hy_freebytes(&info.freemem);
  hy_procnum(&info.nproc);
  printf("Free page number : %d\nActive process number : %d\n",info.freemem, info.nproc);
  hy_top_proc(pro_sum);
  printf("[Summary] %d process unused, %d process sleeping, %d process runnable, %d process running, %d process zombie\n",\
    pro_sum[0],pro_sum[1],pro_sum[2],pro_sum[3],pro_sum[4] );
  return 0;
}