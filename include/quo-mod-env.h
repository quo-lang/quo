/*

DESCRIPTION:
    Quo module for environment variables (cross-platform, no dependencies)

C API:
    #include "quo-mod-env.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_env_init, NULL);
    ...

QUO API:
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

static inline QuoVar quo__mod_env_get(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("env.get() requires a string argument");
  const char *value = getenv(quo_var_as_str(&argv[1])->data);
  return value ? quo_var_new_obj(quo_str_new(s, value, -1)) : quo_var_new_nil();
}

static inline QuoVar quo__mod_env_set(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_str(&argv[1]) || !quo_var_is_str(&argv[2]))
    return quo_var_new_err("env.set() requires key and value strings");
#ifdef _WIN32
  char *str = quo_strdupf("%s=%s", quo_var_as_str(&argv[1])->data, quo_var_as_str(&argv[2])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  setenv(quo_var_as_str(&argv[1])->data, quo_var_as_str(&argv[2])->data, 1);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_env_unset(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("env.unset() requires a string argument");
#ifdef _WIN32
  char *str = quo_strdupf("%s=", quo_var_as_str(&argv[1])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  unsetenv(quo_var_as_str(&argv[1])->data);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_env_has(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("env.has() requires a string argument");
  return quo_var_new_bool(getenv(quo_var_as_str(&argv[1])->data) != NULL);
}

static inline QuoVar quo__mod_env_all(QuoState *s, int64_t argc, QuoVar *argv) {
  QuoDict *dict = quo_dict_new();
#ifdef _WIN32
  char *env_block = GetEnvironmentStrings();
  if (env_block) {
    char *p = env_block;
    while (*p) {
      char *eq = strchr(p, '=');
      if (eq) {
        *eq = '\0';
        QuoStr *key = quo_str_new(s, p, -1);
        QuoVar val = quo_var_new_obj(quo_str_new(s, eq + 1, -1));
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
      QuoStr *key = quo_str_new(s, *env, eq - *env);
      QuoVar val = quo_var_new_obj(quo_str_new(s, eq + 1, -1));
      quo_dict_set(dict, key, &val);
    }
  }
#endif
  return quo_var_new_obj(dict);
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_env_init(QuoState *s) {
  QuoDict *ns = quo_state_register_namespace(s, "env");
  quo_state_namespace_add_cfn(s, ns, "get", quo__mod_env_get);
  quo_state_namespace_add_cfn(s, ns, "set", quo__mod_env_set);
  quo_state_namespace_add_cfn(s, ns, "unset", quo__mod_env_unset);
  quo_state_namespace_add_cfn(s, ns, "has", quo__mod_env_has);
  quo_state_namespace_add_cfn(s, ns, "all", quo__mod_env_all);
  return true;
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_ENV_H
