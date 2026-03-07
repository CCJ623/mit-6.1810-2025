#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "kernel/types.h"
#include "user.h"

#define DATASIZE (8 * 4096)

char help_str[] = "This may help.";
char wrong_str[] = "(null)";

int
main(int argc, char *argv[])
{
  // Your code here.
  while (1) {
    const char *address = sbrk(DATASIZE);
    for (const char *ptr = address,
                    *end = address + DATASIZE - sizeof(help_str);
         ptr != end; ++ptr)
      if (memcmp(ptr, help_str, sizeof(help_str)) == 0 &&
          memcmp(ptr + 16, wrong_str, sizeof(wrong_str)) != 0) {
        printf("%s\n", ptr + 16);
        exit(0);
      }
  }
  exit(1);
}
