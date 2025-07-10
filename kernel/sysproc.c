#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
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

// int pgaccess(void *base, int len, void *mask);
int
sys_pgaccess(void)
{
  uint64 vastart, va;
  int len;
  uint64 mask, result=0;
  if((argaddr(0, &vastart)) < 0 ||
     (argint(1, &len) < 0) ||
     (argaddr(2, &mask) < 0)){
    return -1;
  }
  if(len > 64){
    printf("pgaccess: len is too long\n");
    return -1;
  }
  struct proc *p = myproc();
  va = vastart;
  for(int i = 0; i < len; i++) {
    pte_t *pte;
    if((pte = walk(p->pagetable, va, 0)) == 0){
      printf("pgaccess: walk failed\n");
      return 0;
    }
    if((*pte & PTE_V) == 0){
      printf("pgaccess: invalid page\n");
      return 0;
    }
    if((*pte & PTE_A) != 0){
      result |= (1L << i);
      *pte &= (~PTE_A);
    }
    va += PGSIZE;
  }
  copyout(p->pagetable, mask, (char *)&result, sizeof(result));
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
