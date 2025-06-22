#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(char* path, char* target) {
  char buf[512], *p;  
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type) {
  case T_FILE:
    fprintf(2, "find: cannot run find on %s, which is a file\n", path);
    break;

  case T_DIR:
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if(de.inum == 0)
        continue;
      if((strcmp(de.name, ".") == 0) ||
         (strcmp(de.name, "..")) == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (stat(buf, &st) < 0) {
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      switch(st.type) {
      case T_FILE:
        if (strcmp(de.name, target) == 0) {
          printf("%s\n", buf);
        }
        break;

      case T_DIR:
        find(buf, target);
        break;
      }
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc != 3) {
    printf("Incorrect number of args, expected 2.");
    exit(0);
  }
  char buf[512];
  strcpy(buf, argv[1]);
  find(".", argv[2]);
  exit(0);
}