#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define stdin 0
#define stdout 1
#define stderr 2

#define MAXARG 32
#define MAXLEN 256

int
main(int argc, char *argv[]) {
  char *_argv[MAXARG + 1];
  for (int i = 1; i < argc; i++) {
    _argv[i - 1] = argv[i];
  }

  char buf[MAXLEN + 1];
  int start = 0, end = 0, eof = 0;
  while (!eof) {
    if (start == 0 && end > 0) {
      fprintf(stderr, "too long argument\n");
      exit(1);
    }
    if (start >= end) {
      start = end = 0;
    } else if (start > 0) {
      memmove(buf, buf + start, end - start);
      end -= start;
      start = 0;
    }
    end += read(0, buf + end, MAXLEN - end);
    if (end < MAXLEN) {
      eof = 1;
    }

    int _argc = argc - 1;
    int i = start;
    while (i < end) {
      int j = i;
      while (j < end && buf[j] != ' ' && buf[j] != '\n') j++;
      char last = buf[j];
      if (i < j) {
        if (_argc >= MAXARG) {
          fprintf(stderr, "too many arguments\n");
          exit(1);
        }
        buf[j] = 0;
        _argv[_argc++] = buf + i;
      }
      i = j + 1;

      if ((j < end && last == '\n') || (j == end && eof)) {
        _argv[_argc] = 0;
        if (_argc > argc - 1) {
          if (fork() == 0) {
            exec(_argv[0], _argv);
            fprintf(stderr, "exec %s failed\n", _argv[0]);
            exit(1);
          } else {
            wait(0);
          }
          _argc = argc - 1;
        }
        start = j + 1;
      }
    }
  }
  exit(0);
}
