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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  /*
  for(int i = 0; i < NCPU; i++){
    printf("%p ", (uint64)kmem[i].freelist);
  }
  printf("\n");
  */
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpui = cpuid();

  acquire(&kmem[cpui].lock);
  r->next = kmem[cpui].freelist;
  kmem[cpui].freelist = r;
  release(&kmem[cpui].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpui = cpuid();

  acquire(&kmem[cpui].lock);
  r = kmem[cpui].freelist;
  if(r)
    kmem[cpui].freelist = r->next;
  release(&kmem[cpui].lock);

  // alloc failed, try stealing:
  if(!r){
    for(int i = 0; i < NCPU; i++){
      if(r)
        break;
      if(i == cpui)
        continue;
      if(i < cpui){
        acquire(&kmem[i].lock);
        acquire(&kmem[cpui].lock);
      } else {
        acquire(&kmem[cpui].lock);
        acquire(&kmem[i].lock);
      }
      
      struct run *nr;
      nr = kmem[i].freelist;
      if(nr){
        kmem[i].freelist = nr->next;
        r = nr;
      }

      if(i < cpui){
        release(&kmem[cpui].lock);
        release(&kmem[i].lock);
      } else {
        release(&kmem[i].lock);
        release(&kmem[cpui].lock);
      }
    }
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  /*
  if(!r){
    push_off();
    printf("cpuid: %d\n", cpuid());
    printf("Alloc failed, need stealing!\n");
    for(int i = 0; i < NCPU; i++){
      printf("%p ", (uint64)kmem[i].freelist);
    }
    printf("\n");
    pop_off();
  }
  */
  return (void*)r;
}
