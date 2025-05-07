
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[1];
  int pp2c[2], pc2p[2];
  pipe(pp2c);
  pipe(pc2p);
  if (fork() == 0) {
    close(pp2c[1]);
    read(pp2c[0], buf, 1);
    printf("%d: received ping\n", getpid());
    close(pp2c[0]);
    close(pc2p[0]);
    write(pc2p[1], buf, 1);
    close(pc2p[1]);
  } else {
    close(pp2c[0]);
    write(pp2c[1], "a", 1);
    close(pp2c[1]);
    close(pc2p[1]);
    read(pc2p[0], buf, 1);
    printf("%d: received pong\n", getpid());
    close(pc2p[0]);
  }
  exit(0);
}