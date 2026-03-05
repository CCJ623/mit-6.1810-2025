#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "%s\n", "argument error");
    exit(1);
  }

  int ticks = atoi(argv[1]);
  if(pause(ticks) < 0){
    fprintf(2, "%s\n", "pause error");
    exit(1);
  }

  return 0;
}