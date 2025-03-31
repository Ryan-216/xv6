// Saved registers for kernel context switches.
struct context {
  uint64 ra;  // 保存返回地址寄存器（ra）的值
  uint64 sp;  // 保存栈指针寄存器（sp）的值。

  // callee-saved 这些寄存器通常用于存储局部变量和函数参数
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting. 记录 push_off() 调用的嵌套深度
  int intena;                 // Were interrupts enabled before push_off()? 记录在调用 push_off() 之前中断是否启用
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table  内核页表的 SATP 寄存器值。SATP 是 RISC-V 架构中用于控制页表的寄存器。这个字段保存了内核页表的根指针，用于在内核态访问内存。
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack 内核栈的栈顶指针。当进程在内核态运行时，使用这个栈指针。
  /*  16 */ uint64 kernel_trap;   // usertrap()  指向内核中处理陷阱的函数（usertrap()）。当用户态代码触发陷阱时，会跳转到这个函数。
  /*  24 */ uint64 epc;           // saved user program counter 用户态程序计数器（PC）。当用户态代码触发陷阱时，epc 保存了用户态代码的当前指令地址。
  /*  32 */ uint64 kernel_hartid; // saved kernel tp 内核的硬件线程ID（hartid）。在多核系统中，每个核心可能有自己的硬件线程ID，这个字段用于标识当前核心。
  /*  40 */ uint64 ra;  //返回地址寄存器。保存函数调用的返回地址
  /*  48 */ uint64 sp;  //用户栈指针。保存用户态栈的当前指针。
  /*  56 */ uint64 gp;  //全局指针寄存器。在 RISC-V 中，gp 是一个特殊的寄存器，用于访问全局变量。
  /*  64 */ uint64 tp;  //线程指针寄存器。用于访问线程局部存储。
  /*  72 */ uint64 t0;  //t0 到 t6: 临时寄存器。这些寄存器在函数调用中用于临时存储数据。
  /*  80 */ uint64 t1;  
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;  //s0 到 s11: 调用者保存寄存器。这些寄存器在函数调用中需要被保存和恢复，因为它们保存了重要的上下文信息。
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;  //a0 到 a7: 参数寄存器。用于传递函数调用的参数。
  /* 120 */ uint64 a1;  //a0-a5: 系统调用参数
  /* 128 */ uint64 a2;  //a7 系统调用号
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan 如果进程正在睡眠中，chan 指向它正在等待的通道
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  uint64 hy_syscall_trace;   // 存储进程的系统调用跟踪掩码,用于记录哪些系统调用需要被跟踪

  //时钟相关
  int hy_alarm_interval;       // 时钟周期，为0时表示禁止时钟
  void(*hy_alarm_handler)();      //时钟回调处理函数
  int hy_alarm_ticks;             //当前时钟信号数(ticks数)
  struct trapframe* hy_alarm_trapframe;    //时钟中断时刻进程的陷阱帧，用于恢复进程中断前的状态
  int hy_alarm_goingoff;          //是否已经有一个时钟中断正在执行且还未返回
};
