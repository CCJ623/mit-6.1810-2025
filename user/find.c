#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

char default_directory[] = ".";

typedef struct string {
  char *buffer;
  unsigned int size;
} string;

void init_string(string *str, char *data) {
  str->buffer = data;
  str->size = strlen(data);
}

int string_append(string *str, const char *data) {
  int length = strlen(data);
  strcpy(str->buffer + str->size, data);
  str->size += length;

  return length;
}

void string_cut_tail(string *str, int length) {
  str->size -= length;
  str->buffer[str->size] = '\0';
}

char *string_find_last(const string *str, const char c) {
  for (char *ptr = str->buffer + str->size - 1; ptr != str->buffer; --ptr) {
    if (*ptr == c)
      return ptr;
  }
  return 0;
}

void find(string *path, const string *target) {
  struct dirent de;
  struct stat st;

  if (stat(path->buffer, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path->buffer);
    return;
  }

  switch (st.type) {
  case T_DEVICE:
    break;
  case T_FILE:
    //printf("[DEBUG] file:%s\n", path->buffer);

    if (strcmp(target->buffer, string_find_last(path, '/') + 1) == 0)
      printf("%s\n", path->buffer);
    break;

  case T_DIR: {
    //printf("[DEBUG] directory:%s\n", path->buffer);

    int fd;
    string_append(path, "/");

    if ((fd = open(path->buffer, O_RDONLY)) < 0) {
      fprintf(2, "find: cannot open %s\n", path->buffer);
      return;
    }

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      int length = string_append(path, de.name);
      find(path, target);
      string_cut_tail(path, length);
    }

    close(fd);

    string_cut_tail(path, 1);
    break;
  }

  default:
    break;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2 && argc != 3) {
    fprintf(2, "%s\n", "argument error");
    exit(1);
  }

  string target;
  string directory;

  if (argc == 2) {
    init_string(&directory, default_directory);
    init_string(&target, argv[1]);
  } else if (argc == 3) {
    init_string(&directory, argv[1]);
    init_string(&target, argv[2]);
  }

  find(&directory, &target);

  return 0;
}