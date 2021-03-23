#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: sleep NUMBER...\n");
    exit(1);
  }
  for (int i = 1; i < argc; i++) {
    sleep(atoi(argv[i]));
  }
  exit(0);
}
