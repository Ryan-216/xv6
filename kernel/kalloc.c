// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PA2PGREF_ID(p) (((p)-KERNBASE)/PGSIZE)      // 由物理地址获取物理页id
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)      // 物理页数上限

int pageref[PGREF_MAX_ENTRIES];         //每个物理页的引用数 数组（pageref[i]表示第i个物理页的引用数）
struct spinlock pgreflock;              //用于pageref数组的锁，防止竞态条件引起内存泄漏

#define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))]       //获取地址对应物理页引用数

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

char* keme_lock_names[] = {
    "kmem_cpu_0",
    "kmem_cpu_1",
    "kmem_cpu_2",
    "kmem_cpu_3",
    "kmem_cpu_4",
    "kmem_cpu_5",
    "kmem_cpu_6",
    "kmem_cpu_7",
};

void
kinit()
{
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmem[i].lock, keme_lock_names[i]);
  }
  
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  //当页面的引用数<=0的时候释放页面
  acquire(&pgreflock);
  if(--PA2PGREF(pa) <= 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

  push_off();

  int cpu = cpuid(); // 获取cpu编号。中断关闭时调用cpuid才是安全的，所以上面用push_off关闭中断

  acquire(&kmem[cpu].lock);         //将释放的页插入当前CPU的freelist中
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();                //重新打开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();						//关闭中断

  int cpu = cpuid();

  acquire(&kmem[cpu].lock);

  if (!kmem[cpu].freelist) {        // 当前CPU已经没有freelist的时候，去其他CPU偷内存页
      int steal_left = 64;          // 这里指定偷64个内存页
      for (int i = 0;i < NCPU;++i) {
          if (i == cpu)
              continue;             // 跳过当前CPU
          
          acquire(&kmem[i].lock);
          if (!kmem[i].freelist) {      // 如果在想要偷页的cpu也没有freelist了，就释放锁跳过
              release(&kmem[i].lock);
              continue;
          }

          struct run* rr = kmem[i].freelist;
          while (rr && steal_left) {           // 循环将kmem[i]的freelist移动到kmem[cpu]中
              kmem[i].freelist = rr->next;
              rr->next = kmem[cpu].freelist;  //  头插法
              kmem[cpu].freelist = rr;
              rr = kmem[i].freelist;
              steal_left--;
          }

          release(&kmem[i].lock);
          
          if (steal_left == 0)       // 偷到指定页数后退出循环
              break;
      }
  }

  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);

  pop_off();				//打开中断

  if (r) {
      memset((char*)r, 5, PGSIZE); // fill with junk
      PA2PGREF(r) = 1;        //将新分配的物理页的引用数设置为1（刚分配的页还没映射，不会有进程来使用，不用加锁）
  }

  return (void*)r;
}

// 获取空闲内存
void
hy_freebytes(uint64* dst)
{
  *dst = 0;
  struct run* p = kmem.freelist;  //空闲链表机制存储空闲物理内存页
  acquire(&kmem.lock);  //获得锁

  while (p)
  {
    *dst += PGSIZE;
    p=p->next;
  }
  
  release(&kmem.lock);  //释放锁
}

//物理页引用数+1
void
hy_krefpage(void* pa) {
    acquire(&pgreflock);
    PA2PGREF(pa)++;
    release(&pgreflock);
}
//写时复制一个新的物理地址返回 
//如果该物理页的引用数>1，则将引用数-1，并分配一个新的物理页返回
//如果引用数 <=1，则无需操作直接返回该物理页
void*
hy_kcopy_n_deref(void* pa){
  acquire(&pgreflock);

  //当前物理页的引用数为1，无需分配新的物理页
  if(PA2PGREF(pa) <= 1){
    release(&pgreflock);
    return pa;
  }

  //分配新的物理页，并把旧页中的数据复制到新页
  uint64 newpa = (uint64)kalloc();

  if(newpa == 0){  //内存不够
    release(&pgreflock);
    return 0;
  }
  memmove((void*)newpa, (void*)pa, PGSIZE);
  
  //旧页引用数-1
  PA2PGREF(pa)--;
  release(&pgreflock);

  return (void*)newpa;
}
