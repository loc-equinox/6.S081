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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  uint buc[NBUCKETS][NSLOTS];
  uint valid[NBUCKETS][NSLOTS];
  struct spinlock buclock[NBUCKETS];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

uint
bchash(uint blockno)
{
  return blockno % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  /*
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  */
  // Initialize hash table locks.
  for(int i = 0; i < NBUCKETS; i++)
    initlock(&bcache.buclock[i], "bcache.bucket");

  // Initialize the buffers.
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }

  // Initialize the hash table.
  for(int i = 0; i < NBUCKETS; i++)
    for(int j = 0; j < NSLOTS; j++)
      bcache.valid[i][j] = 0;
  for(int i = 0; i < NBUCKETS; i++)
    for(int j=0, k=i; j<NSLOTS && k<NBUF; j++, k+=NBUCKETS){
      bcache.buc[i][j] = k;
      bcache.valid[i][j] = 1;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  /*
  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  */

  uint bucno = bchash(blockno);
  acquire(&bcache.buclock[bucno]);
  for(int i = 0; i < NSLOTS; i++){
    if(bcache.valid[bucno][i] != 0){
      b = &bcache.buf[bcache.buc[bucno][i]];
      if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        b->bticks = ticks;
        release(&bcache.buclock[bucno]);
        acquiresleep(&b->lock);
        return b;
      }
    }
  }

  // Not cached.
  release(&bcache.buclock[bucno]);
  acquire(&bcache.lock);
  acquire(&bcache.buclock[bucno]);
  for(int i = 0; i < NSLOTS; i++){
    if(bcache.valid[bucno][i] != 0){
      b = &bcache.buf[bcache.buc[bucno][i]];
      if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        b->bticks = ticks;
        release(&bcache.lock);
        release(&bcache.buclock[bucno]);
        acquiresleep(&b->lock);
        return b;
      }
    }
  }
  release(&bcache.buclock[bucno]);
  // Re-check completed, ready to alloc new buf
  int srcbno = -1, srcsno;
  uint mxticks = 0x7ffffff, bufid = -1;
  struct buf *tb;
  // printf("bucno: %d\n", bucno);
  for(int i = 0; i < NBUCKETS; i++){
    // printf("acquiring %d\n", i);
    acquire(&bcache.buclock[i]);
    for(int j = 0; j < NSLOTS; j++){
      if(bcache.valid[i][j] == 0)
        continue;
      tb = &bcache.buf[bcache.buc[i][j]];
      if(tb->refcnt == 0 && tb->bticks < mxticks){
        b = tb;
        mxticks = tb->bticks;
        if(srcbno >= 0 && srcbno != i){
          // printf("releasing %d\n", srcbno);
          release(&bcache.buclock[srcbno]);
        }
        srcbno = i;
        srcsno = j;
        bufid = bcache.buc[i][j];
      } 
    }
    if(srcbno != i){
      // printf("releasing %d\n", i);
      release(&bcache.buclock[i]);
    }
  }
  if(bufid == -1)
    panic("bget: no buffers");
  // Found a buffer, check whether there's an
  // empty slot for it.
  // Note that we are only holding one lock, which
  // is buclock[srcbno], so we have to check whether
  // srcbno equals to bucno and act accordingly.
  // printf("valid[%d][%d]\n\n", srcbno, srcsno);
  bcache.valid[srcbno][srcsno] = 0;
  if(srcbno != bucno){
    acquire(&bcache.buclock[bucno]);
  }
  /*
  printf("bucno: %d\n", bucno);
  printf("valid: ");
  for(int i = 0; i < NSLOTS; i++)
    printf("%d ", bcache.valid[bucno][i]);
  printf("\nbuc: ");
  for(int i = 0; i < NSLOTS; i++)
    printf("%d ", bcache.buc[bucno][i]);
  printf("\n\n");
  */
  for(int i = 0; i < NSLOTS; i++){
    if(bcache.valid[bucno][i] == 0){
      // Found a slot.
      bcache.valid[bucno][i] = 1;
      bcache.buc[bucno][i] = bufid;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->bticks = ticks;
      release(&bcache.lock);
      release(&bcache.buclock[srcbno]);
      if(srcbno != bucno){
        release(&bcache.buclock[bucno]);
      }
      acquiresleep(&b->lock);
      return b;
    }
  }

  panic("bget: no empty slots");
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

  uint bucno = bchash(b->blockno);
  acquire(&bcache.buclock[bucno]);
  b->refcnt--;
  /*
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  */
  
  release(&bcache.buclock[bucno]);
}

void
bpin(struct buf *b) {
  uint bucno = bchash(b->blockno);
  acquire(&bcache.buclock[bucno]);
  b->refcnt++;
  release(&bcache.buclock[bucno]);
}

void
bunpin(struct buf *b) {
  uint bucno = bchash(b->blockno);
  acquire(&bcache.buclock[bucno]);
  b->refcnt--;
  release(&bcache.buclock[bucno]);
}


