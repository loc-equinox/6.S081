#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int* p)
{
    int prime, cur; // The prime number for this process
    int newp[2], hasRight=0, pid=getpid();
    close(p[1]);
    cur = read(p[0], (char*)&cur, 4);
    printf("cur: %d\n", cur);
    prime = cur;
    if(cur > 36) {
      return;
    }
    printf("prime %d", prime);
    cur = read(p[0], (char*)&cur, sizeof(cur));
    printf("cur: %d\n", cur);
    while(cur != 0) {
      if(cur % prime != 0) {
        printf("cur: %d\n", cur);
        if(hasRight) {
          write(newp[1], (char*)&cur, sizeof(cur));
        } else {
          hasRight = 1;
          pipe(newp);
          if(fork() == 0) {
            sieve(newp);
          } else {
            close(newp[0]);
            write(newp[1], (char*)&cur, sizeof(cur));
          }
        }
      }
      cur = read(p[0], (char*)&cur, sizeof(cur));
    }
    if(getpid() == pid) {
      close(p[0]);
      close(newp[1]);
    }
    wait((int*)0);
    exit(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);
  if(fork() == 0) {
    sieve(p);
  } else {
    close(p[0]);
    for(int i = 2; i <= 35; i++) {
      printf("%d\n", i);
      write(p[1], (char*)&i, sizeof(i));
    }
    wait((int*)0);
    close(p[1]);
  }
  exit(0);
}