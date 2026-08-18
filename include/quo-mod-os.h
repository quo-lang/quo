/*

DESCRIPTION:
    Quo module for operating system utilities.

QUO API:
    var os = import("os")

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
static QuoVar quo__mod_os_system(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("os.system() takes command string");
  return quo_var_new_num(system(quo_var_as_str(&argv[0])->data));
}

static QuoVar quo__mod_os_name(QuoModule *m, int argc, QuoVar *argv) {
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
  return quo_var_new_obj(quo_str_new(m, os_name, -1));
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_os_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "os", NULL, NULL);
  quo_module_register_cfn(m, "system", -1, quo__mod_os_system);
  quo_module_register_cfn(m, "name", -1, quo__mod_os_name);
  return true;
}

#ifdef __cplusplus
}
#endif
