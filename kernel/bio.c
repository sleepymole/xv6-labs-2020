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

extern uint64 sys_uptime(void);

#define NBUCKET 13
#define NGETSLOTS 13

struct bucket {
  struct buf *head;
  struct spinlock lock;
};

struct {
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKET];
  struct spinlock getlocks[NGETSLOTS];
} bcache;

void
binit(void)
{
  for(int i = 0; i < NBUCKET; i++)
    initlock(&bcache.buckets[i].lock,"bcache");
  for(int i = 0; i < NGETSLOTS; i++)
    initlock(&bcache.getlocks[i], "bcache");
  for(int i = 0; i < NBUF; i++){
    uint bucketno  = i % NBUCKET;
    struct buf *b = &bcache.buf[i];
    initlock(&b->guard, "bcache");
    b->next = bcache.buckets[bucketno].head;
    bcache.buckets[bucketno].head = b->next;
    b->bucketno = bucketno;
  }
}

// get returns a unlocked buffer for blockno. It's thread-safe 
// but it doesn't guarantee that at most one buffer is allocated
// for the same blockno.
static struct buf*
get(uint dev, uint blockno)
{
  struct buf *b;
  uint bucketno = ((dev << 16) ^ blockno) % NBUCKET;

  // Is the block already cached?
  struct bucket *bucket = &bcache.buckets[bucketno];
  acquire(&bucket->lock);
  for(b = bucket->head; b != 0; b = b->next){
    acquire(&b->guard);
    if(b->bucketno == bucketno && b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&b->guard);
      release(&bucket->lock);
      return b;
    }
    release(&b->guard);
  }
  release(&bucket->lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *lru = 0;
  for(b = bcache.buf; b != bcache.buf + NBUF; b++){
    acquire(&b->guard);
    if(b->refcnt != 0){
      release(&b->guard);
      continue;
    }
    if(lru == 0){
      lru = b;
    }else if(b->last_used < lru->last_used){
      release(&lru->guard);
      lru = b;
    }else{
      release(&b->guard);
    }
  }
  if(!lru)
    panic("bget: no buffers");
  b = lru;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  if(b->bucketno == bucketno){
    release(&b->guard);
    return b;
  }
  uint old_bucketno = b->bucketno;
  b->bucketno = bucketno;
  release(&b->guard);

  // Remove the buffer from the previous bucket;
  struct bucket* old_bucket = &bcache.buckets[old_bucketno];
  acquire(&old_bucket->lock);
  struct buf **next = &old_bucket->head;
  while(*next != 0){
    if(*next == b){
      *next = (*next)->next;
    }else{
      next = &(*next)->next;
    }
  }
  release(&old_bucket->lock);

  // Add the buffer to its new bucket;
  acquire(&bucket->lock);
  b->next = bucket->head;
  bucket->head = b;
  release(&bucket->lock);

  return b;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint slot = ((dev << 16) ^ blockno) % NGETSLOTS;
  acquire(&bcache.getlocks[slot]);
  b = get(dev, blockno);
  release(&bcache.getlocks[slot]);
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

  acquire(&b->guard);
  b->refcnt--;
  b->last_used = sys_uptime();
  release(&b->guard);
}

void
bpin(struct buf *b) {
  acquire(&b->guard);
  b->refcnt++;
  release(&b->guard);
}

void
bunpin(struct buf *b) {
  acquire(&b->guard);
  b->refcnt--;
  release(&b->guard);
}
