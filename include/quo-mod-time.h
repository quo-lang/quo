/*

DESCRIPTION:
    Quo module for time and date.

QUO API:
    var time = import("time")

    # Sleep for N seconds
    time.sleep(69)

    # Get current time in seconds since epoch
    var now = time.now()

    # Get clock for benchmarking
    var clock = time.clock()

*/

#pragma once

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- PRIVATE API ---------- //

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

// ---------- PUBLIC API ---------- //

static inline void quo_mod_time_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "time", NULL, NULL);
  quo_module_register_cfn(m, "sleep", -1, quo__mod_time_sleep);
  quo_module_register_cfn(m, "now", -1, quo__mod_time_now);
  quo_module_register_cfn(m, "clock", -1, quo__mod_time_clock);
}

#ifdef __cplusplus
}
#endif
