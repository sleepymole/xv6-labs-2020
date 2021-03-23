#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[]) {
  int fda[2], fdb[2];
  if (pipe(fda) < 0) {
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }
  if (pipe(fdb) < 0) {
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }
  int pid = fork();
  if (pid < 0) {
    fprintf(2, "pingpong: fork failed\n");
    exit(1);
  } else if (pid > 0) {
    close(fda[1]);
    close(fdb[0]);
    char buf[1] = {'m'};
    write(fdb[1], buf, 1);
    close(fdb[1]);
    read(fda[0], buf, 1);
    close(fda[0]);
    fprintf(1, "%d: received pong\n", getpid());
    wait(0);
  } else {
    close(fda[0]);
    close(fdb[1]);
    char buf[1];
    read(fdb[0], buf, 1);
    close(fdb[0]);
    fprintf(1, "%d: received ping\n", getpid());
    write(fda[1], buf, 1);
    close(fda[1]);
  }
  exit(0);
}
