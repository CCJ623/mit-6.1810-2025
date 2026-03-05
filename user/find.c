#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"

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

void find(string *path, const string *target, char **exec_argv,
          int exec_argv_size) {
  struct dirent de;
  struct stat st;

  if (stat(path->buffer, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path->buffer);
    return;
  }

  switch (st.type) {
  case T_DEVICE:
    break;
  case T_FILE: { // printf("[DEBUG] file:%s\n", path->buffer);
    char *file_name_ptr = string_find_last(path, '/') + 1;
    if (strcmp(target->buffer, file_name_ptr) == 0) {
      if (exec_argv_size == 0) {
        printf("%s\n", path->buffer);
      } else {
        // exec_argv[exec_argv_size] = file_name_ptr;
        exec_argv[exec_argv_size] = path->buffer;
        ++exec_argv_size;
        exec_argv[exec_argv_size] = 0;

        int pid = fork();
        if (pid == 0) {
          exec(exec_argv[0], exec_argv);
        } else if (pid < 0) {
          fprintf(2, "fork error\n");
        }
        wait(0);
      }
    }
    break;
  }

  case T_DIR: {
    // printf("[DEBUG] directory:%s\n", path->buffer);

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
      find(path, target, exec_argv, exec_argv_size);
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
  if (argc < 3) {
    fprintf(2, "%s\n", "argument error");
    exit(1);
  }

  string target;
  string directory;
  char *exec_argv[MAXARG];
  int exec_argv_size = 0;

  if (argc > 3) {
    if (strcmp(argv[3], "-exec") != 0) {
      fprintf(2, "%s\n", "argument error");
      exit(1);
    }

    for (int index = 0; argv[4 + index] != 0; ++index) {
      exec_argv[index] = argv[4 + index];
      ++exec_argv_size;
    }

    exec_argv[exec_argv_size] = 0;
  }

  init_string(&directory, argv[1]);
  init_string(&target, argv[2]);

  find(&directory, &target, exec_argv, exec_argv_size);

  return 0;
}