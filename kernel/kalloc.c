// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
void* pagestart;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint64
kinitpagerefcount()
{
  uint64 pagecount = (PHYSTOP - (uint64)end)/PGSIZE;
  uint64 memsize = pagecount * sizeof(uint16);
  memset((void*)end, 0, memsize);
  return memsize;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  uint64 pagecountmemsize = kinitpagerefcount();
  pagestart = (void*)PGROUNDUP((uint64)end + pagecountmemsize);
  freerange(pagestart, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  struct run *r;
  char *pa;
  pa = (char*)PGROUNDUP((uint64)pa_start);
  for(; pa + PGSIZE <= (char*)pa_end; pa += PGSIZE)
  {
    acquire(&kmem.lock);
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}

void krefpage(uint64 pa)
{
  uint64 pageindex = ((void*)pa - pagestart) / PGSIZE;
  uint16* pagerefcount = (uint16*)end + pageindex;
  (*pagerefcount)++;
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < (char*)pagestart || (uint64)pa >= PHYSTOP)
    panic("kfree");
  uint64 pageindex = (pa - pagestart) / PGSIZE;
  uint16* pagerefcount = (uint16*)end + pageindex;

  acquire(&kmem.lock);
  if(*pagerefcount == 0)
  {
    panic("already free refcount error!");
  }
  
  (*pagerefcount)--;
  if(*pagerefcount == 0)
  {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }

  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    uint64 pageindex = ((void*)r - pagestart) / PGSIZE;
    uint16* pagerefcount = (uint16*)end + pageindex;
    if(*pagerefcount != 0)
    {
      panic("memalloc refcount init error!");
    }
    (*pagerefcount)++;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int
kcowpage(pagetable_t pagetable, uint64 faultva)
{
    if(faultva >= MAXVA)
      return -2;
      
    pte_t* pte = walk(pagetable, faultva, 0);
    // valid pte
    if(!pte)
    {
      return -2;
    }
    if(*pte & PTE_COW)
    {
      uint64 oldpa = PTE2PA(*pte);
      uint flags;
      void* newpage = kalloc();
      if(!newpage)
        panic("no free memory to copy cow page!");
      memmove(newpage, (void*)PTE2PA(*pte), PGSIZE);
      flags = PTE_FLAGS(*pte);
      flags = (flags | PTE_W) & (~PTE_COW);
      *pte = PA2PTE(newpage) | flags;

      //old pa ref
      kfree((void*)oldpa);
      return 0;
    }
    return -1;
}
