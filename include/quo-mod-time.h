#pragma once

#include "quo.h"

// Sleep for N seconds
static QuoVar quo__mod_time_sleep(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("time.sleep() takes number of seconds");
  int64_t sec = (int64_t)argv[0].val_num;
  if (sec < 0) sec = 0;
#ifdef _WIN32
  Sleep(sec * 1000);
#else
  sleep(sec);
#endif
  return quo_var_new_nil();
}

// Get current time in seconds since epoch
static QuoVar quo__mod_time_now(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argv);
  if (argc != 0) return quo_var_new_err("time.now() takes no arguments");
  return quo_var_new_num((double)time(NULL));
}

// Get clock for benchmarking
static QuoVar quo__mod_time_clock(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argv);
  if (argc != 0) return quo_var_new_err("time.clock() takes no arguments");
  return quo_var_new_num((double)clock() / CLOCKS_PER_SEC);
}

static inline void quo_mod_time_load(void) {
  QuoModule *m = quo_module_new(NULL, "time", NULL, NULL);
  quo_module_register_cfn(m, "sleep", -1, quo__mod_time_sleep);
  quo_module_register_cfn(m, "now", -1, quo__mod_time_now);
  quo_module_register_cfn(m, "clock", -1, quo__mod_time_clock);
}
