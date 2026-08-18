/*

DESCRIPTION:
    Quo module for environment variables (cross-platform, no dependencies)

QUO API:
    var env = import("env")

    # Get environment variable
    var home = env.get("HOME") # Returns value or nil if not set

    # Set environment variable
    env.set("MY_VAR", "my_value")

    # Unset environment variable
    env.unset("MY_VAR")

    # Get all environment variables as dict
    var all = env.all() # Returns { "HOME": "/home/user", "PATH": "/usr/bin:...", ... }

    # Check if environment variable exists
    var exists = env.has("HOME") # Returns true or false

*/

#ifndef QUO_MOD_ENV_H
#define QUO_MOD_ENV_H

#include "quo.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---------- PRIVATE API ---------- //

static inline QuoVar quo__mod_env_get(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_str(&argv[1])) return quo_var_new_err("env.get() requires a string argument");
  const char *value = getenv(quo_var_as_str(&argv[1])->data);
  return value ? quo_var_new_obj(quo_str_new(m, value, -1)) : quo_var_new_nil();
}

static inline QuoVar quo__mod_env_set(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("env.set() requires key and value strings");
#ifdef _WIN32
  char *str = quo_strdupf("%s=%s", quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  setenv(quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data, 1);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_env_unset(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("env.unset() requires a string argument");
#ifdef _WIN32
  char *str = quo_strdupf("%s=", quo_var_as_str(&argv[0])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  unsetenv(quo_var_as_str(&argv[0])->data);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_env_has(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("env.has() requires a string argument");
  return quo_var_new_bool(getenv(quo_var_as_str(&argv[0])->data) != NULL);
}

static inline QuoVar quo__mod_env_all(QuoModule *m, int argc, QuoVar *argv) {
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
        QuoStr *key = quo_str_new(m, p, -1);
        QuoVar val = quo_var_new_obj(quo_str_new(m, eq + 1, -1));
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
      QuoStr *key = quo_str_new(m, *env, eq - *env);
      QuoVar val = quo_var_new_obj(quo_str_new(m, eq + 1, -1));
      quo_dict_set(dict, key, &val);
    }
  }
#endif
  return quo_var_new_obj(dict);
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_env_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "env", NULL, NULL);
  quo_module_register_cfn(m, "get", -1, quo__mod_env_get);
  quo_module_register_cfn(m, "set", -1, quo__mod_env_set);
  quo_module_register_cfn(m, "unset", -1, quo__mod_env_unset);
  quo_module_register_cfn(m, "has", -1, quo__mod_env_has);
  quo_module_register_cfn(m, "all", -1, quo__mod_env_all);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_ENV_H
