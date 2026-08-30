#pragma once

#include "quo.h"

typedef struct {
  QuoObj obj;
  FILE *file;
  QuoStr *path;
} QuoIOFile;

QUO_DEFINE_USER_TYPE(QuoIOFile, io_file)

static inline QuoVar quo__mod_io_open(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("io.open() requires a path string and mode string");
  QuoIOFile *file = (QuoIOFile *)quo_type_get_instance("QuoIOFile");
  file->path = quo_var_as_str(&argv[0]);
  file->file = fopen(file->path->data, quo_var_as_str(&argv[1])->data);
  if (!file->file) {
    quo_obj_unref((QuoObj *)file);
    return quo_var_new_err("Failed to open file");
  }
  return quo_var_new_obj(file);
}

static inline QuoVar quo__mod_io_file_get_path(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  return quo_var_new_obj(quo_var_as_io_file(&argv[0])->path);
}

static inline QuoVar quo__mod_io_file_read(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("read() has no arguments");
  QuoIOFile *file = quo_var_as_io_file(&argv[0]);
  char *content = quo_read_file(file->path->data);
  if (!content) return quo_var_new_err("Failed to read file");
  QuoStr *result = quo_str_new_raw(content, -1);
  quo_dealloc(content);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_io_file_write(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("write() requires a content string");
  QuoIOFile *file = quo_var_as_io_file(&argv[0]);
  return quo_var_new_bool(quo_write_file(file->path->data, quo_var_as_str(&argv[1])->data));
}

static inline QuoVar quo__mod_io_file_read_lines(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("read_lines() has no arguments");
  QuoIOFile *file = quo_var_as_io_file(&argv[0]);
  char *content = quo_read_file(file->path->data);
  if (!content) return quo_var_new_err("Failed to read file");
  QuoArr *arr = quo_arr_new();
  char *line = strtok(content, "\n");
  while (line) {
    // Remove trailing \r if present
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
    quo_arr_push(arr, quo_var_new_obj(quo_str_new_raw(line, -1)));
    line = strtok(NULL, "\n");
  }
  quo_dealloc(content);
  return quo_var_new_obj(arr);
}

// Print the values of the given arguments. Separate values with spaces and print a newline at the end.
static QuoVar quo__mod_io_print(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  for (int i = 0; i < argc; i++) {
    quo_var_print(&argv[i]);
    if (i < argc - 1) printf(" ");
  }
  printf("\n");
  return quo_var_new_nil();
}

// Read a line from stdin and return it as a string
static QuoVar quo__mod_io_input(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  // Optional prompt arguments
  for (int i = 0; i < argc; i++) quo_var_print(&argv[i]);
  // Read a line from stdin
  char *line = NULL;
  size_t len = 0;
  ssize_t read = getline(&line, &len, stdin);
  if (read == -1) {
    // EOF or error - return nil
    free(line);
    return quo_var_new_nil();
  }
  // Remove trailing newline if present
  if (read > 0 && line[read - 1] == '\n') line[--read] = '\0';
  QuoStr *str = quo_str_new(line, read);
  free(line);
  return quo_var_new_obj(str);
}

static inline void quo_mod_io_load(void) {
  QuoModule *m = quo_module_new(NULL, "io", NULL, NULL);
  quo_module_register_cfn(m, "print", -1, quo__mod_io_print);
  quo_module_register_cfn(m, "input", -1, quo__mod_io_input);
  quo_module_register_cfn(m, "open", -1, quo__mod_io_open);

  QuoObj *io_file = quo_type_register("QuoIOFile", sizeof(QuoIOFile));
  quo_type_add_cfn(io_file, "get_path", -1, quo__mod_io_file_get_path);
  quo_type_add_cfn(io_file, "read", -1, quo__mod_io_file_read);
  quo_type_add_cfn(io_file, "write", -1, quo__mod_io_file_write);
  quo_type_add_cfn(io_file, "read_lines", -1, quo__mod_io_file_read_lines);
}
