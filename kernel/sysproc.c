#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  pagetable_t pagetable = 0;
  char* tempBuffer = 0;
  uint64 va;
  int pagecount;
  uint64 bitaddress;

  argaddr(0, &va);
  argint(1, &pagecount);
  argaddr(2, &bitaddress);

  pagetable = myproc()->pagetable;
  if(pagetable == 0)
    panic("pgaccess error: process no right root pagetable");

  tempBuffer = (char*)kalloc();
  if(!tempBuffer)
    panic("alloc tmp buffer error!");
  memset(tempBuffer, 0, PGSIZE);

  va = PGROUNDDOWN(va);

  for(int i = 0; i < pagecount; ++i)
  {
    uint64 tmpVa = va + i * PGSIZE;
    pte_t *pte = walk(pagetable, tmpVa, 0);
    if(pte && (*pte & PTE_A))
    {
      *pte &= ~PTE_A;
      char* tmpStartByte = tempBuffer + i / 8;
      *tmpStartByte |= 1 << (i % 8);
    }
  }
  if(copyout(pagetable, bitaddress, tempBuffer, (pagecount + 8 - 1) / 8 ) < 0)
    panic("pgaccess copy buffer from kenerl to user error!");

  kfree(tempBuffer);
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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
