#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define stdin 0
#define stdout 1
#define stderr 2

void
sieve() {
  int first;
  if (read(stdin, &first, 4) != 4) {
    close(stdin);
    return;
  }
  printf("prime %d\n", first);
  int fds[2];
  if (pipe(fds) < 0) {
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }
  int pid = fork();
  if (pid < 0) {
    fprintf(2, "primes: fork failed\n");
    exit(1);
  } else if (pid > 0) {
    close(fds[0]);
    close(stdout);
    dup(fds[1]);
    close(fds[1]);
    int x;
    while (read(stdin, &x, 4) == 4) {
      if (x % first) {
        write(stdout, &x, 4);
      }
    }
    close(stdin);
    close(stdout);
    wait(0);
  } else {
    close(fds[1]);
    close(stdin);
    dup(fds[0]);
    close(fds[0]);
    sieve();
  }
}

int
main(int argc, char* argv[]) {
  int fds[2];
  if (pipe(fds) < 0) {
    fprintf(stderr, "primes: pipe failed\n");
    exit(1);
  }
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "primes: fork failed\n");
    exit(1);
  } else if (pid > 0) {
    close(fds[0]);
    close(stdout);
    dup(fds[1]);
    close(fds[1]);
    for (int i = 2; i <= 35; i++) {
      write(stdout, &i, 4);
    }
    close(stdout);
    wait(0);
  } else {
    close(fds[1]);
    close(stdin);
    dup(fds[0]);
    close(fds[0]);
    sieve();
  }
  exit(0);
}
