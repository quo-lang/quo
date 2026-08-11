/*

DESCRIPTION:
    Quo module for time and date.

C API:
    #include "quo-mod-time.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_time_init, NULL);
    ...

QUO API:
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
static QuoVar quo__mod_time_sleep(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("time.sleep() takes number of seconds");
  int64_t sec = (int64_t)argv[1].val_num;
  if (sec < 0) sec = 0;
#ifdef _WIN32
  Sleep(sec * 1000);
#else
  sleep(sec);
#endif
  return quo_var_new_nil();
}

// Get current time in seconds since epoch
static QuoVar quo__mod_time_now(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("time.now() takes no arguments");
  return quo_var_new_num((double)time(NULL));
}

// Get clock for benchmarking
static QuoVar quo__mod_time_clock(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("time.clock() takes no arguments");
  return quo_var_new_num((double)clock() / CLOCKS_PER_SEC);
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_time_init(QuoState *s) {
  QuoDict *ns = quo_state_register_namespace(s, "time");
  quo_state_namespace_add_cfn(s, ns, "sleep", quo__mod_time_sleep);
  quo_state_namespace_add_cfn(s, ns, "now", quo__mod_time_now);
  quo_state_namespace_add_cfn(s, ns, "clock", quo__mod_time_clock);
  return true;
}

#ifdef __cplusplus
}
#endif
