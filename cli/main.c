#define QUO_IMPLEMENTATION
#include "../include/quo.h"

// Include modules
#include "../include/quo-mod-base64.h"
#include "../include/quo-mod-csv.h"
#include "../include/quo-mod-dl.h"
#include "../include/quo-mod-env.h"
#include "../include/quo-mod-fs.h"
#include "../include/quo-mod-json.h"
#include "../include/quo-mod-math.h"
#include "../include/quo-mod-net.h"
#include "../include/quo-mod-os.h"
#include "../include/quo-mod-time.h"
#include "../include/quo-mod-uuid.h"

// #include "formatter.h"

// static enum { QUO_PATH_IS_NONE, QUO_PATH_IS_DIR, QUO_PATH_IS_FILE } quo__path_is(const char *path) {
//   struct stat path_stat;
//   if (stat(path, &path_stat) != 0) return QUO_PATH_IS_NONE;
//   else if (S_ISDIR(path_stat.st_mode)) return QUO_PATH_IS_DIR;
//   else if (S_ISREG(path_stat.st_mode)) return QUO_PATH_IS_FILE;
//   return QUO_PATH_IS_NONE;
// }

// static int quo__format(const char *path) {
//   if (quo__path_is(path) == QUO_PATH_IS_FILE) {
//     Quo *a = quo_new();
//     bool success = quo_compile(a, path);
//     if (!success) {
//       quo_free(a);
//       return 1;
//     }
//     // QuoFormatter f = quo_fmt_new();
//     // quo_fmt_format_module(&f, main_module);
//     // quo_write_file(path, quo_fmt_get_string(&f));
//     // quo_fmt_free(&f);
//   } else if (quo__path_is(path) == QUO_PATH_IS_DIR) {
//     fprintf(stderr, "Formatting directories is not yet supported\n");
//     return 1;
//   }
//   return 0;
// }

static int quo__run(const char *path) {
  char *source = quo_read_file(path);
  if (!source) {
    fprintf(stderr, "Failed to read file: %s\n", path);
    return 1;
  }
  char *cwd = quo_dirname(path);
  QuoModule *m = quo_module_new(NULL, cwd, path, source, NULL);
  quo_dealloc(source);
  quo_dealloc(cwd);
  // If module is NULL, there was compilation error.
  if (!m) return 1;

  // Load modules
  quo_mod_base64_init(m);
  quo_mod_csv_init(m);
  quo_mod_dl_init(m);
  quo_mod_env_init(m);
  quo_mod_fs_init(m);
  quo_mod_json_init(m);
  quo_mod_math_init(m);
  quo_mod_net_init(m);
  quo_mod_os_init(m);
  quo_mod_time_init(m);
  quo_mod_uuid_init(m);

  int exit_code = 0;
  QuoVar result = quo_module_run(m);
  if (quo_var_is_err(&result)) {
    fprintf(stderr, "Runtime Error: %s\n", result.val_err);
    exit_code = 1;
  } else if (quo_var_is_num(&result)) exit_code = (int)result.val_num;
  quo_var_unref(&result);
  quo_obj_unref((QuoObj *)m);
  // quo_vm_free(vm);

  return exit_code;
}

static void quo__print_help(const char *bin) {
  printf("Usage: %s [option] [arg]\n", bin);
  printf("Options:\n");
  printf("    format <file> ... | <directory> ...    Format file or all .quo files in the directory\n");
  printf("Arguments:\n");
  printf("    <file>    Run program from file\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    quo__print_help(argv[0]);
    return 1;
  }
  const char *arg = argv[1];

  if (strcmp(arg, "format") == 0) {
    if (argc < 3) {
      printf("Error: Expected path\n");
      quo__print_help(argv[0]);
      return 1;
    }
    const char *path = argv[2];
    // return quo__format(path);
  } else return quo__run(arg);
}
