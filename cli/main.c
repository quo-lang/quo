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
  int64_t exit_code = 0;
  char *cwd = quo_dirname(path);
  QuoState *s = quo_state_new(cwd);
  quo_dealloc(cwd);

  // Load modules
  quo_state_register_module(s, quo_mod_base64_init, NULL);
  quo_state_register_module(s, quo_mod_csv_init, NULL);
  quo_state_register_module(s, quo_mod_dl_init, NULL);
  quo_state_register_module(s, quo_mod_env_init, NULL);
  quo_state_register_module(s, quo_mod_fs_init, NULL);
  quo_state_register_module(s, quo_mod_json_init, NULL);
  quo_state_register_module(s, quo_mod_math_init, NULL);
  quo_state_register_module(s, quo_mod_net_init, quo_mod_net_cleanup);
  quo_state_register_module(s, quo_mod_os_init, NULL);
  quo_state_register_module(s, quo_mod_time_init, NULL);
  quo_state_register_module(s, quo_mod_uuid_init, NULL);

  // Run code
  QuoParser *p = quo_parser_new(s, path);
  if (p && quo_parser_parse(p)) {
    QuoCompiler *c = quo_compiler_new(s, "main", -1);
    QuoFn *main = quo_compiler_compile(c, p->ast);
    quo_compiler_free(c);
    QuoVM *vm = quo_vm_new(s);
    QuoVar result = quo_vm_run(vm, main);
    if (quo_var_is_err(&result)) fprintf(stderr, "Runtime error: %s\n", result.val_err);
    else if (quo_var_is_num(&result)) exit_code = (int64_t)result.val_num;
    quo_var_unref(&result);
    quo_vm_free(vm);
  }

  quo_parser_free(p);
  quo_state_free(s);

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
