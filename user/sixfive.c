#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

char separator[] = " -\r\t\n./,";

void error(char *msg) {
  fprintf(2, "%s\n", msg);
  exit(1);
}

int is_separator(char c) {
  if (strchr(separator, c) == 0) {
    return 0;
  } else {
    return 1;
  }
}

int is_digit(char c) { return ('0' <= c && c <= '9'); }

void sixfive(int num) {
  if (num % 5 == 0 || num % 6 == 0) {
    printf("%d\n", num);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    error("argument error");
  }

  for (int i = 1; i != argc; ++i) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      error("open error");
    }

    char buf[1024];
    char *end = buf;
    while (1) {
      int read_bytes = read(fd, end, 1);

    //   printf("buffer:");
    //   write(2, buf, end + 1 - buf);
    //   printf("\n");

      if (read_bytes == 0) {
        if (end != buf) {
          *end = '\0';
          int num = atoi(buf);
          sixfive(num);
        }
        break;
      } else if (read_bytes < 0) {
        error("read error");
      }

      if (is_separator(*end) && end != buf) {
        *end = '\0';
        int num = atoi(buf);
        sixfive(num);
        end = buf;
      } else if (is_digit(*end)) {
        ++end;
      }
    }

    close(fd);
  }

  return 0;
}