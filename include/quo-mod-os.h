#pragma once

#include "quo.h"

// Run a system command and return the result
static inline QuoVar quo__mod_os_system(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("os.system() takes command string");
  return quo_var_new_num(system(quo_var_as_str(&argv[0])->data));
}

// Get OS name
static inline QuoVar quo__mod_os_get_name(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  const char *name = NULL;
#if defined(__linux__)
  name = "linux";
#elif defined(_WIN32)
  name = "windows";
#elif defined(__APPLE__) && defined(__MACH__)
  name = "macos";
#else
  name = "unknown";
#endif
  return quo_var_new_obj(quo_str_new_interned(name, -1));
}

static inline QuoVar quo__mod_os_getenv(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("os.getenv() requires a string argument");
  const char *value = getenv(quo_var_as_str(&argv[0])->data);
  return value ? quo_var_new_obj(quo_str_new(value, -1)) : quo_var_new_nil();
}

static inline QuoVar quo__mod_os_setenv(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("os.setenv() requires key and value strings");
#ifdef _WIN32
  char *str = quo_strdupf("%s=%s", quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  setenv(quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data, 1);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_os_unsetenv(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("os.unsetenv() requires a string argument");
#ifdef _WIN32
  char *str = quo_strdupf("%s=", quo_var_as_str(&argv[0])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  unsetenv(quo_var_as_str(&argv[0])->data);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_os_hasenv(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("os.hasenv() requires a string argument");
  return quo_var_new_bool(getenv(quo_var_as_str(&argv[0])->data) != NULL);
}

static inline QuoVar quo__mod_os_allenv(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  QuoDict *dict = quo_dict_new();
#ifdef _WIN32
  char *env_block = GetEnvironmentStrings();
  if (env_block) {
    char *p = env_block;
    while (*p) {
      char *eq = strchr(p, '=');
      if (eq) {
        *eq = '\0';
        QuoStr *key = quo_str_new(p, -1);
        QuoVar val = quo_var_new_obj(quo_str_new(eq + 1, -1));
        quo_dict_set(dict, key, &val);
        *eq = '=';
      }
      p += strlen(p) + 1;
    }
    FreeEnvironmentStrings(env_block);
  }
#else
  extern char **environ;
  for (char **env = environ; *env; env++) {
    char *eq = strchr(*env, '=');
    if (eq) {
      QuoStr *key = quo_str_new(*env, eq - *env);
      QuoVar val = quo_var_new_obj(quo_str_new(eq + 1, -1));
      quo_dict_set(dict, key, &val);
    }
  }
#endif
  return quo_var_new_obj(dict);
}

// Exit the program with an optional exit code
static QuoVar quo__mod_os_exit(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  int code = 0;
  if (argc > 0 && quo_var_is_num(&argv[0])) code = (int)argv[0].val_num;
  exit(code);
  return quo_var_new_nil();
}

static inline void quo_mod_os_load(void) {
  QuoModule *m = quo_module_new(NULL, "os", NULL, NULL);
  quo_module_register_cfn(m, "system", -1, quo__mod_os_system);
  quo_module_register_cfn(m, "get_name", -1, quo__mod_os_get_name);
  quo_module_register_cfn(m, "getenv", -1, quo__mod_os_getenv);
  quo_module_register_cfn(m, "setenv", -1, quo__mod_os_setenv);
  quo_module_register_cfn(m, "unsetenv", -1, quo__mod_os_unsetenv);
  quo_module_register_cfn(m, "hasenv", -1, quo__mod_os_hasenv);
  quo_module_register_cfn(m, "allenv", -1, quo__mod_os_allenv);
  quo_module_register_cfn(m, "exit", -1, quo__mod_os_exit);
}
