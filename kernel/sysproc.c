#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
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

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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

uint64
sys_mmap(void)
{
  // the addr argument will always be zero
  uint length, offset;
  int prot, flags, fd;
  if(argint(1, (int*)&length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, (int*)&offset) < 0)
    return -1;
  
  struct proc *p = myproc();
  int id = -1;
  for(int i = 0; i < NVMA; i++){
    if(p->pvma[i].used == 0){
      id = i;
      break;
    }
  }
  // printf("vma id: %d\n", id);
  if(id == -1)
    panic("mmap: run out of vmas");
  
  length = PGROUNDUP(length);
  if(p->vmatop - length <= p->sz)
    panic("mmap: not enough space");
  p->vmatop -= length;
  p->pvma[id].addr = p->vmatop;
  p->pvma[id].count = length/PGSIZE;
  p->pvma[id].flags = flags;
  p->pvma[id].file = p->ofile[fd];
  if(!p->ofile[fd]->writable &&
     (prot & PROT_WRITE) &&
     (flags & MAP_SHARED))
    // permission clashes
    return -1;
  filedup(p->pvma[id].file);

  p->pvma[id].length = length;
  p->pvma[id].perm = prot;
  p->pvma[id].used = 1;
  // printf("vmatop: %p, length: %d\n", p->vmatop, length);

  return p->pvma[id].addr;
}

uint64
munmap_range(uint64 addr, uint length, int idx)
{
  struct proc *p = myproc();
   
  int id = idx;
  if(id == -1){
    for(int i = 0; i < NVMA; i++){
      if(addr >= p->pvma[i].addr && 
         addr + length <= p->pvma[i].addr + p->pvma[i].length){
        id = i;
        break;
      }
    }
  }
  if(id == -1)
    panic("munmap: address range does not belong to any vma");
  if(((addr - p->pvma[id].addr) % PGSIZE != 0) ||
      length % PGSIZE != 0)
    panic("munmap: address range not aligned");

  // uvmunmap(p->pagetable, addr, (uint)numpages, 1);
  pte_t *pte;
  struct inode *ip = p->pvma[id].file->ip;
  for(uint64 a = addr; a < addr + length; a += PGSIZE){
    uint off = a - p->pvma[id].addr;
    uint size;
    if(a + PGSIZE <= addr + ip->size)
      size = PGSIZE;
    else
      size = addr + ip->size - a;
    // printf("size: %d\n", size);
    if((pte = walk(p->pagetable, a, 0)) == 0)
      // panic("munmap: walk"); // fail faster
      continue;                 // it's also possible to skip
    if((*pte & PTE_V) == 0)
      // panic("munmap: not mapped");
      continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      // panic("munmap: not a leaf");
      continue;
    if(p->pvma[id].flags & MAP_SHARED){
      begin_op();
      ilock(ip);
      if(writei(ip, 1, a, off, size) != size)
        panic("munmap: write error");
      iunlock(ip);
      end_op();
    }
    p->pvma[id].count--;
    uint64 pa = PTE2PA(*pte);
    kfree((void*)pa);
    *pte = 0;
  }
  if(p->pvma[id].count < 0)
    panic("munmap: vma page count < 0");
  if(p->pvma[id].count == 0){
    filedecref(p->pvma[id].file);
    p->pvma[id].used = 0;
  }
  return 0;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  uint length;
  if(argaddr(0, &addr) < 0 || argint(1, (int*)&length) < 0){
    return -1;
  }
  return munmap_range(addr, length, -1);
}
