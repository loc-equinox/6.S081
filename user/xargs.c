#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
  if(argc <= 1){
    printf("Too few arguments, expected at least 1.\n");
    exit(0);
  }
  char c;
  char buf[512], *p;
  p = buf;
  while(read(0, &c, sizeof(c)) != 0){
    *p++ = c;
    if(c == '\n'){
      p--;
      *p = 0;
      if(fork() == 0){
        char* nargv[MAXARG];
        int nargc;
        if(argc < MAXARG)
          nargc = argc;
        else nargc = MAXARG;
        for(int i = 0; i < nargc - 1; i++){
          nargv[i] = argv[i + 1];
        }
        nargv[nargc - 1] = buf;
        exec(argv[1], nargv);
        printf("exec error\n");
      }
      else{
        wait(0);
      }
      memset(buf, 0, sizeof(buf));
      p = buf;
    }
  }
  exit(0);
}