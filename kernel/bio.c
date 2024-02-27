// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKET_COUNT 13

struct {
  struct spinlock freelock[BUCKET_COUNT];
  struct buf* freebuf[BUCKET_COUNT];

  struct spinlock bucketlock[BUCKET_COUNT];
  struct buf bucket[BUCKET_COUNT];
} bcache;



int getbucketindex(uint dev, uint blockno)
{
  return dev * blockno % BUCKET_COUNT;
}

void
binit(void)
{
  for(int i = 0; i < BUCKET_COUNT; ++i)
  {
    initlock(&bcache.bucketlock[i],"bcache.bucket");
    initlock(&bcache.freelock[i],"bcache.bucket");
    bcache.bucket[i].next = 0;
    bcache.bucket[i].prev = 0;
    bcache.freebuf[i] = 0;

    struct buf* buf1 = 0;
    struct buf* buf2 = 0;
    void* pagebuf = kalloc();
    if(!pagebuf)
      panic("95");
    buf1 = ( struct buf*)pagebuf;
    buf2 = ( struct buf*)(pagebuf + PGSIZE / 2);
    initsleeplock(&buf1->lock,"buf_sleep");
    initsleeplock(&buf2->lock,"buf_sleep");
    buf1->next = buf2;
    buf2->next = bcache.freebuf[i];
    bcache.freebuf[i] = buf1;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b = 0;
  struct buf* head;
  int bucketindex = getbucketindex(dev, blockno);
  acquire(&bcache.bucketlock[bucketindex]);
  head = &bcache.bucket[bucketindex];
  b = head->next;
  for(; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlock[bucketindex]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  int startindex = bucketindex;
  do
  {
    acquire(&bcache.freelock[startindex]);
    if(bcache.freebuf[startindex])
    {
      b = bcache.freebuf[startindex];
      bcache.freebuf[startindex] = b->next;
      release(&bcache.freelock[startindex]);
      break;
    }
    release(&bcache.freelock[startindex]);
    startindex = (startindex + 1) % BUCKET_COUNT;
  } while (startindex != bucketindex);

  if(!b)
  {
    struct buf* buf1 = 0;
    struct buf* buf2 = 0;
    void* pagebuf = kalloc();
    if(!pagebuf)
      panic("95");
    buf1 = ( struct buf*)pagebuf;
    buf2 = ( struct buf*)(pagebuf + PGSIZE / 2);
    initsleeplock(&buf1->lock,"buf_sleep");
    initsleeplock(&buf2->lock,"buf_sleep");

    b = buf1;

    acquire(&bcache.freelock[bucketindex]);
    buf2->next = bcache.freebuf[bucketindex];
    bcache.freebuf[bucketindex] = buf2;
    release(&bcache.freelock[bucketindex]);
  }

  b->next = head->next;
  b->prev = head;
  if(head->next)
    head->next->prev = b;
  head->next = b;

  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;

  release(&bcache.bucketlock[bucketindex]);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  int index = getbucketindex(b->dev, b->blockno);
  acquire(&bcache.bucketlock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // freebuf(b);
    acquire(&bcache.freelock[index]);
    b->prev->next = b->next;
    if(b->next)
      b->next->prev = b->prev;

    b->next = bcache.freebuf[index];
    bcache.freebuf[index] = b;
    release(&bcache.freelock[index]);
  }
  release(&bcache.bucketlock[index]);
}

void
bpin(struct buf *b) {
  int index = getbucketindex(b->dev, b->blockno);
  acquire(&bcache.bucketlock[index]);
  b->refcnt++;
  release(&bcache.bucketlock[index]);
}

void
bunpin(struct buf *b) {
  int index = getbucketindex(b->dev, b->blockno);
  acquire(&bcache.bucketlock[index]);
  b->refcnt--;
  release(&bcache.bucketlock[index]);
}


