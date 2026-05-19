#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MEM_SIZE (16 * 1024 * 1024) // 16MB
#define STRIDE 4096                 // 4KB stride
#define ITERS 50000

int main(int argc, char *argv[]) {
  printf("superbench: aligning to 2MB...\n");
  uint64 cur = (uint64)sbrk(0);
  uint64 SUPERPGSIZE = 2 * 1024 * 1024;
  uint64 padding = SUPERPGSIZE - (cur % SUPERPGSIZE);
  if (padding != SUPERPGSIZE) {
    sbrk(padding);
  }

  printf("superbench: allocating 16MB...\n");
  char *mem = sbrk(MEM_SIZE);
  if (mem == (char*)-1 || mem == 0) {
    printf("superbench: sbrk failed\n");
    exit(1);
  }

  printf("superbench: starting benchmark...\n");
  int start_ticks = uptime();

  for (int iter = 0; iter < ITERS; iter++) {
    for (int i = 0; i < MEM_SIZE; i += STRIDE) {
      mem[i] = (char)(iter & 0xFF); 
      char dummy = mem[i];          
      (void)dummy;
    }
  }

  int end_ticks = uptime();
  printf("superbench: finished in %d ticks.\n", end_ticks - start_ticks);
  
  exit(0);
}
