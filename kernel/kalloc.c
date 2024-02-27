// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(int cpuid, void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int pagecount;
} kmem[NCPU];
uint64 last;
uint64 next;
void
kinit()
{
  uint64 interval = (PHYSTOP - (uint64)end) / NCPU;
  for(int i = 0; i < NCPU; ++i)
  {
    initlock(&kmem[i].lock, "kmem");
    kmem[i].pagecount = 0;
    kmem[i].freelist = 0;
  }
  last = PGROUNDDOWN((uint64)end) / PGSIZE;
  char* pos = end;
  for(int i = 0; i < NCPU - 1; ++i)
  {
    freerange(i, pos, pos + interval);
    pos += interval;
  }
  freerange(NCPU - 1,pos, (void*)PHYSTOP - 1);
  int total = 0;
  for(int i = 0; i < NCPU; ++i)
  {
    printf("%d ",kmem[i].pagecount);
    total += kmem[i].pagecount;
  }
  int _total = (PHYSTOP - (uint64)end) / PGSIZE;
  printf("\n total:%d realtotal:%d\n",total,_total);
  //check
  next = PHYSTOP / PGSIZE;
  int count = 0;
  for(int i = NCPU - 1; i >= 0; --i)
  {
    struct run* r = kmem[i].freelist;
    count = 0;
    while(r)
    {
      last = (uint64)r/PGSIZE;
      if(last + 1 != next)
      {
        printf("check fail reason:i %d,last %d, next %d\n",i ,last,next);
        panic("check fail");
      }
      count++;
      next = last;
      r = r->next;
    }
    if(count != kmem[i].pagecount)
    {
        printf("check count failed:i %d, count %d,pagecount %d\n",i,count ,kmem[i].pagecount);
        panic("");
    }
  }
}

void
freerange(int cpuid, void *pa_start, void *pa_end)
{
  struct run *r;
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  printf("start %d %p\n",cpuid,p);
  for(; p <= (char*)pa_end; p += PGSIZE)
  {
    next = PGROUNDDOWN((uint64)p) / PGSIZE;
    if(last+1 != next)
    {
      panic("error!");
    }
    last = next;
    memset(p, 1, PGSIZE);
    r = (struct run*)p;
    r->next = kmem[cpuid].freelist;
    kmem[cpuid].freelist = r;
    kmem[cpuid].pagecount++;
  }
  printf("over %d %p\n",cpuid,p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  // if(kmem[id].pagecount == 0 && kmem[id].freelist)
  // {
  //   printf("pagecount free 0,freeaddr %p\n",kmem[id].freelist);
  //   panic("");
  // }
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].pagecount++;
  release(&kmem[id].lock);
  pop_off();
}

void *
kalloc(void)
{
  struct run *r = 0;
  // int cachestealnum = -1;
  // int cachestealedid = -1;
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  //start steal
  if(!kmem[id].freelist)
  {
    int i = (id + 1) % NCPU;
    while(i != id)
    {
      acquire(&kmem[i].lock);
      if(kmem[i].pagecount > 0)
      {
        int stealnum = kmem[i].pagecount > 16 ? 16 : kmem[i].pagecount;
        // cachestealnum = stealnum;
        // cachestealedid = i;
        // if(cachestealnum == 0)
        // {
        //   panic("stealnumerror!");
        // }
        kmem[id].freelist = kmem[i].freelist;
        kmem[id].pagecount = stealnum;
        kmem[i].pagecount -= stealnum;
        while(stealnum > 0)
        {
          // if(!kmem[i].freelist || (uint64)kmem[i].freelist >= PHYSTOP || (uint64)kmem[i].freelist <= (uint64)end)
          // {
          //   panic("steal error!");
          // }
          r = kmem[i].freelist;
          kmem[i].freelist = kmem[i].freelist->next;
          stealnum--;
        }
        r->next = 0;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
      i = (i + 1) % NCPU;
    }
  }
  
  r = kmem[id].freelist;
  
  if(r)
  {
    kmem[id].freelist = r->next;
    kmem[id].pagecount--;
    // if(kmem[id].pagecount == 0 && kmem[id].freelist!=0)
    // {
    //   printf("steal num %d, id %d, stealedid %d\n",cachestealnum,id,cachestealedid);
    //   panic("error pagecount = 0!");
    // }
    // if(kmem[id].pagecount < 0)
    // {
    //   printf("steal num %d, id %d, stealedid %d\n",cachestealnum,id,cachestealedid);
    //   panic("error pagecount < 0!");
    // }
  }

  release(&kmem[id].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
