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
#define BUCKETSIZE 13
struct bucket
{
  struct spinlock lock;
  struct buf head;
};
struct
{
  // struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct bucket bucket[BUCKETSIZE];
} bcache;

void binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for (int i = 0; i < BUCKETSIZE; i++)
  {
    initlock(&bcache.bucket[i].lock, "bucket");
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.bucket[0].head.next;
    b->prev = &bcache.bucket[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].head.next->prev = b;
    bcache.bucket[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
uint get_ticks()
{
  uint ans = 0;
  acquire(&tickslock);
  ans = ticks;
  release(&tickslock);
  return ans;
}
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int now = blockno % BUCKETSIZE;
  acquire(&bcache.bucket[now].lock);
  // acquire(&bcache.lock);

  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  struct buf *miss = 0;
  for (b = bcache.bucket[now].head.next; b != &bcache.bucket[now].head; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      b->lastuse = get_ticks();
      release(&bcache.bucket[now].lock);
      acquiresleep(&b->lock);
      return b;
    }
    if (b->refcnt == 0 && (miss == 0 || b->lastuse < miss->lastuse))
    {
      miss = b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // local lru
  if (miss != 0)
  {
    miss->dev = dev;
    miss->blockno = blockno;
    miss->valid = 0;
    miss->refcnt = 1;
    miss->lastuse = get_ticks();
    release(&bcache.bucket[now].lock);
    acquiresleep(&miss->lock);
    return miss;
  }
  // steal;
  release(&bcache.bucket[now].lock);
  for (int i = (now + 1) % BUCKETSIZE; i != now; i = (i + 1) % BUCKETSIZE)
  {
    acquire(&bcache.bucket[i].lock);
    for (b = bcache.bucket[i].head.next; b != &bcache.bucket[i].head; b = b->next)
    {
      if (b->refcnt == 0 && (miss == 0 || b->lastuse < miss->lastuse))
      {
        miss = b;
      }
    }
    if (miss)
    {


      miss->dev = dev;
      miss->blockno = blockno;
      miss->valid = 0;
      miss->refcnt = 1;
      miss->lastuse = get_ticks();
      miss->prev->next = miss->next;
      miss->next->prev = miss->prev;
      release(&bcache.bucket[i].lock);
      acquire(&bcache.bucket[now].lock);
      miss->next = bcache.bucket[now].head.next;
      miss->prev = &bcache.bucket[now].head;
      bcache.bucket[now].head.next = miss;
      miss->next->prev = miss;
      release(&bcache.bucket[now].lock);
      acquiresleep(&miss->lock);
      return miss;
    }
    release(&bcache.bucket[i].lock);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.bucket[b->blockno % BUCKETSIZE].lock);
  b->refcnt--;
  release(&bcache.bucket[b->blockno % BUCKETSIZE].lock);
}

void bpin(struct buf *b)
{
  acquire(&bcache.bucket[b->blockno % BUCKETSIZE].lock);
  b->refcnt++;
  release(&bcache.bucket[b->blockno % BUCKETSIZE].lock);
}

void bunpin(struct buf *b)
{
  acquire(&bcache.bucket[b->blockno % BUCKETSIZE].lock);
  b->refcnt--;
  release(&bcache.bucket[b->blockno % BUCKETSIZE].lock);
}
