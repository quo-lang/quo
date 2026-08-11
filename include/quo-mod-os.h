/*

DESCRIPTION:
    Quo module for operating system utilities.

C API:
    #include "quo-mod-os.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_os_init, NULL);
    ...

QUO API:
    # Run a system command
    var result = os.system("ls -la")

    # Get the OS name
    var name = os.name()

*/

#pragma once

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- PRIVATE API ---------- //

// Run a system command and return the result
static QuoVar quo__mod_os_system(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc == 1) return quo_var_new_err("os.system() takes command string");
  return quo_var_new_num(system(quo_var_as_str(&argv[1])->data));
}

static QuoVar quo__mod_os_name(QuoState *s, int64_t argc, QuoVar *argv) {
  const char *os_name = NULL;
#if defined(__linux__)
  os_name = "linux";
#elif defined(_WIN32)
  os_name = "windows";
#elif defined(__APPLE__) && defined(__MACH__)
  os_name = "macos";
#else
  os_name = "unknown";
#endif
  return quo_var_new_obj(quo_str_new(s, os_name, -1));
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_os_init(QuoState *s) {
  QuoDict *ns = quo_state_register_namespace(s, "os");
  quo_state_namespace_add_cfn(s, ns, "system", quo__mod_os_system);
  quo_state_namespace_add_cfn(s, ns, "name", quo__mod_os_name);
  return true;
}

#ifdef __cplusplus
}
#endif
