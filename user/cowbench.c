#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STRIDE 4096                 // 4KB page stride

void print_report(const char* name, int ticks, int iters) {
  printf("  %s: %d ticks total (%d iterations, avg %d ms/iter)\n", 
         name, ticks, iters, (ticks * 100) / iters);
}

// Case 1: Fork and immediate exit
void test_fork_exit(char *mem, int sz, int iters) {
  int start = uptime();
  for (int i = 0; i < iters; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      exit(0);
    }
    wait(0);
  }
  int end = uptime();
  print_report("Fork-Exit (No Write)", end - start, iters);
}

// Case 2: Fork and read-only access
void test_fork_read(char *mem, int sz, int iters) {
  int start = uptime();
  for (int i = 0; i < iters; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      volatile char dummy;
      for (int j = 0; j < sz; j += STRIDE) {
        dummy = mem[j];
      }
      (void)dummy;
      exit(0);
    }
    wait(0);
  }
  int end = uptime();
  print_report("Fork-Read (Read Only)", end - start, iters);
}

// Case 3: Fork and write to partial memory (e.g. 10%)
void test_fork_write_partial(char *mem, int sz, int iters, int percent) {
  int start = uptime();
  int step = STRIDE * (100 / percent);
  for (int i = 0; i < iters; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      for (int j = 0; j < sz; j += step) {
        mem[j] = 1;
      }
      exit(0);
    }
    wait(0);
  }
  int end = uptime();
  printf("  Fork-Write-%d%%: %d ticks total (%d iterations, avg %d ms/iter)\n", 
         percent, end - start, iters, ((end - start) * 100) / iters);
}

// Case 4: Fork and write to all memory (100%)
void test_fork_write_all(char *mem, int sz, int iters) {
  int start = uptime();
  for (int i = 0; i < iters; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      for (int j = 0; j < sz; j += STRIDE) {
        mem[j] = 1;
      }
      exit(0);
    }
    wait(0);
  }
  int end = uptime();
  print_report("Fork-Write-All (100%)", end - start, iters);
}

// Case 5: Capacity limit test
void test_capacity_limit() {
  int sz = 70 * 1024 * 1024;            // Allocate 70MB (greater than half of 128MB physical memory)
  
  printf("  [Capacity] sbrk(70MB)... ");
  char *p = sbrk(sz);
  if (p == (char*)-1) {
    printf("FAILED (sbrk failed to allocate 70MB)\n");
    return;
  }
  
  // Touch pages to force physical allocation in parent
  for (int i = 0; i < sz; i += STRIDE * 10) {
    p[i] = 1;
  }
  printf("Allocated. Forking... ");
  
  int pid = fork();
  if (pid < 0) {
    printf("FAILED (fork failed)\n");
  } else if (pid == 0) {
    exit(0);
  } else {
    wait(0);
    printf("SUCCESS (fork succeeded!)\n");
  }
  
  sbrk(-sz); // Deallocate
}

int main(int argc, char *argv[]) {
  int mem_mb = 8; // Default 8MB
  int iters = 50; // Default 50 iterations
  
  if (argc >= 2) mem_mb = atoi(argv[1]);
  if (argc >= 3) iters = atoi(argv[2]);
  
  int sz = mem_mb * 1024 * 1024;
  printf("COW Benchmark: Parent Memory = %d MB, Iterations = %d\n", mem_mb, iters);
  
  // Allocate parent memory and touch all pages
  char *mem = sbrk(sz);
  if (mem == (char*)-1 || mem == 0) {
    printf("sbrk failed\n");
    exit(1);
  }
  for (int i = 0; i < sz; i += STRIDE) {
    mem[i] = 0;
  }
  
  printf("Starting performance tests...\n");
  test_fork_exit(mem, sz, iters);
  test_fork_read(mem, sz, iters);
  test_fork_write_partial(mem, sz, iters, 10); // 10%
  test_fork_write_all(mem, sz, iters);
  
  sbrk(-sz);
  
  printf("Starting capacity limit test...\n");
  test_capacity_limit();
  
  exit(0);
}
