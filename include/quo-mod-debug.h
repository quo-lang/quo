#pragma once

#include "quo.h"

static inline QuoVar quo__mod_debug_assert(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("assert() takes boolean condition");
  if (argv[0].val_num == 0) return quo_var_new_err("Assert condition failed");
  return quo_var_new_nil();
}

static inline void quo_mod_debug_load(void) {
  QuoModule *m = quo_module_new(NULL, "debug", NULL, NULL);
  quo_module_register_cfn(m, "assert", -1, quo__mod_debug_assert);
}
