#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if((r_scause() == 13 || r_scause() == 15) && hy_uvmcheckcowpage(r_stval())){
    //发生页面错误，并且检测出错误是写时复制机制导致的页面不可写，则执行写时复制
    if(hy_uvmcowcopy(r_stval()) == -1){
      p->killed = 1;
    }
  } else {
    uint64 fault_va = r_stval();  //获取引发缺页异常的虚拟地址
    if((r_scause()==13 || r_scause()==15) && hy_uvmshouldallocate(fault_va)){
      hy_uvmlazyallocate(fault_va);
    } else {
      printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      p->killed = 1;
    }
  }

  if(p->killed)
    exit(-1);

  if (which_dev == 2) {
    if (p->hy_alarm_interval != 0 && --p->hy_alarm_ticks <= 0 && p->hy_alarm_goingoff == 0) {
          // 是否设置了时钟 && 时钟倒计时是否结束 && 没有其他时钟正在运行
          // 如果一个时钟到期的时候已经有一个时钟处理函数正在运行，
          // 则会推迟到原处理函数运行完成后的下一个 tick 才触发这次时钟
          p->hy_alarm_ticks = p->hy_alarm_interval;      // 重置时钟倒计时
          *p->hy_alarm_trapframe = *p->trapframe;          // 保存当前进程陷阱帧
          p->trapframe->epc = (uint64)p->hy_alarm_handler; // 跳转到时钟回调函数
          p->hy_alarm_goingoff = 1;                        // 标记当前已有时钟正在运行
      }
    yield();
  }

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

//检查虚拟地址所在页是否是COW页
int
hy_uvmcheckcowpage(uint64 va){
  pte_t* pte;
  struct proc* p = myproc();
  //地址在进程内存范围内  &&  地址有映射  && 地址有效且是COW页
  return va < p->sz &&
        ((pte = walk(p->pagetable, va, 0)) != 0) &&
        (*pte & PTE_V) &&
        (*pte & PTE_COW);
}

//实现写时复制
int
hy_uvmcowcopy(uint64 va){
  pte_t* pte;
  struct proc* p = myproc();
  if((pte = walk(p->pagetable, va, 0)) == 0){ //获取虚拟地址的页表项
    panic("uvmcowcopy : wakl");
  }
  uint64 pa = PTE2PA(*pte); //获取映射的物理地址
  uint64 new = (uint64)hy_kcopy_n_deref((void*)pa); //获取新分配的物理页（如果原本的物理页引用数为1，则获取到的还是原本的物理页）
  if(new==0){ //内存不足的情况
    return -1;
  }
  //修改新的映射，恢复写权限，清除COW标志
  uint64 flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
  uvmunmap(p->pagetable, PGROUNDDOWN(va), 1, 0);              //清除旧的映射
  if (mappages(p->pagetable, va, 1, new, flags) == -1){       //新的映射
    panic("uvmcowcopy: mappages");
  }

  return 0;
}

// 设置进程中时钟的相关属性
int 
hy_sigalarm(int ticks, void(*handler)())
{
  struct proc* p = myproc();
  p->hy_alarm_ticks = ticks;
  p->hy_alarm_handler = handler;
  p->hy_alarm_interval = ticks;
  return 0;
}

//将进程恢复到alarm中断前的状态
int
hy_sigreturn()
{
  struct proc* p = myproc();
  *p->trapframe = *p->hy_alarm_trapframe;
  p->hy_alarm_goingoff = 0;
  return 0;
}
