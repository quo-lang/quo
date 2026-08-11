/*

DESCRIPTION:
    Quo module for math operations.
    Dynamically loads libm for math functions.

C API:
    #include "quo-mod-math.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_math_init, quo_mod_math_cleanup);
    ...

QUO API:
    # Constants
    math.pi                  # 3.141592653589793
    math.e                   # 2.718281828459045
    math.tau                 # 6.283185307179586

    # Rounding
    math.floor(3.7)          # 3
    math.ceil(3.2)           # 4
    math.round(3.5)          # 4
    math.abs(-42)            # 42

    # Power and roots
    math.sqrt(16)            # 4
    math.cbrt(27)            # 3
    math.pow(2, 3)           # 8
    math.exp(1)              # 2.71828...
    math.log(2.71828)        # 1 (natural log)
    math.log2(8)             # 3
    math.log10(100)          # 2

    # Trigonometry
    math.sin(math.pi / 2)    # 1
    math.cos(0)              # 1
    math.tan(0)              # 0
    math.asin(1)             # 1.57079...
    math.acos(1)             # 0
    math.atan(1)             # 0.78539...
    math.atan2(1, 1)         # 0.78539...

    # Hyperbolic
    math.sinh(1)
    math.cosh(1)
    math.tanh(1)

    # Min/max/abs
    math.min(3, 7)           # 3
    math.max(3, 7)           # 7
    math.clamp(15, 0, 10)    # 10

    # Random
    math.random(100)         # Random int 0-99
    math.random_float()      # Random float 0.0-1.0

    # Conversions
    math.deg_to_rad(180)     # 3.14159...
    math.rad_to_deg(math.pi) # 180

*/

#pragma once

#include "quo.h"
#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------- LIBM FUNCTION POINTERS ---------- //

typedef double (*math_func_t)(double);
typedef double (*math_func2_t)(double, double);

static struct {
  void *handle;
  math_func_t floor;
  math_func_t ceil;
  math_func_t round;
  math_func_t trunc;
  math_func_t fabs;
  math_func_t sqrt;
  math_func_t cbrt;
  math_func2_t pow;
  math_func_t exp;
  math_func_t log;
  math_func_t log2;
  math_func_t log10;
  math_func_t sin;
  math_func_t cos;
  math_func_t tan;
  math_func_t asin;
  math_func_t acos;
  math_func_t atan;
  math_func2_t atan2;
  math_func_t sinh;
  math_func_t cosh;
  math_func_t tanh;
} quo__math = {0};

// ---------- PRIVATE HELPERS ---------- //

static bool quo__mod_math_load(void) {
  if (quo__math.handle) return true;

#ifdef _WIN32
  // On Windows, math functions are in msvcrt.dll or ucrtbase.dll
  const char *lib_names[] = {"msvcrt.dll", "ucrtbase.dll"};
#elif defined(__APPLE__)
  // On macOS, math functions are in libSystem.dylib
  const char *lib_names[] = {"libSystem.dylib", "libm.dylib"};
#else
  // On Linux/BSD, math functions are in libm.so
  const char *lib_names[] = {"libm.so", "libm.so.6"};
#endif

  for (size_t i = 0; i < sizeof(lib_names) / sizeof(lib_names[0]); i++) {
    quo__math.handle = dlopen(lib_names[i], RTLD_LAZY);
    if (quo__math.handle) break;
  }
  if (!quo__math.handle) return false;

  // Load all math functions
  quo__math.floor = (math_func_t)dlsym(quo__math.handle, "floor");
  quo__math.ceil = (math_func_t)dlsym(quo__math.handle, "ceil");
  quo__math.round = (math_func_t)dlsym(quo__math.handle, "round");
  quo__math.trunc = (math_func_t)dlsym(quo__math.handle, "trunc");
  quo__math.fabs = (math_func_t)dlsym(quo__math.handle, "fabs");
  quo__math.sqrt = (math_func_t)dlsym(quo__math.handle, "sqrt");
  quo__math.cbrt = (math_func_t)dlsym(quo__math.handle, "cbrt");
  quo__math.pow = (math_func2_t)dlsym(quo__math.handle, "pow");
  quo__math.exp = (math_func_t)dlsym(quo__math.handle, "exp");
  quo__math.log = (math_func_t)dlsym(quo__math.handle, "log");
  quo__math.log2 = (math_func_t)dlsym(quo__math.handle, "log2");
  quo__math.log10 = (math_func_t)dlsym(quo__math.handle, "log10");
  quo__math.sin = (math_func_t)dlsym(quo__math.handle, "sin");
  quo__math.cos = (math_func_t)dlsym(quo__math.handle, "cos");
  quo__math.tan = (math_func_t)dlsym(quo__math.handle, "tan");
  quo__math.asin = (math_func_t)dlsym(quo__math.handle, "asin");
  quo__math.acos = (math_func_t)dlsym(quo__math.handle, "acos");
  quo__math.atan = (math_func_t)dlsym(quo__math.handle, "atan");
  quo__math.atan2 = (math_func2_t)dlsym(quo__math.handle, "atan2");
  quo__math.sinh = (math_func_t)dlsym(quo__math.handle, "sinh");
  quo__math.cosh = (math_func_t)dlsym(quo__math.handle, "cosh");
  quo__math.tanh = (math_func_t)dlsym(quo__math.handle, "tanh");

  // Check essential functions loaded
  if (!quo__math.floor || !quo__math.sqrt || !quo__math.sin) {
    dlclose(quo__math.handle);
    quo__math.handle = NULL;
    return false;
  }
  return true;
}

// Helper to get number as double
static inline double quo__mod_math_as_double(QuoVar *v) { return quo_var_is_num(v) ? (double)v->val_num : v->val_num; }

// ---------- PRIVATE API ---------- //

static QuoVar quo__mod_math_floor(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("floor() requires a number");
  return quo_var_new_num((int64_t)quo__math.floor(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_ceil(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("ceil() requires a number");
  return quo_var_new_num((int64_t)quo__math.ceil(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_round(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("round() requires a number");
  return quo_var_new_num((int64_t)quo__math.round(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_trunc(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("trunc() requires a number");
  return quo_var_new_num((int64_t)quo__math.trunc(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_abs(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("abs() requires a number");
  if (quo_var_is_num(&argv[1])) return quo_var_new_num(llabs((int64_t)argv[1].val_num));
  return quo_var_new_num(quo__math.fabs(argv[1].val_num));
}

static QuoVar quo__mod_math_sqrt(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("sqrt() requires a number");
  return quo_var_new_num(quo__math.sqrt(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_cbrt(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("cbrt() requires a number");
  return quo_var_new_num(quo__math.cbrt(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_pow(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_num(&argv[1]) || !quo_var_is_num(&argv[2])) return quo_var_new_err("pow() requires two numbers");
  return quo_var_new_num(quo__math.pow(quo__mod_math_as_double(&argv[1]), quo__mod_math_as_double(&argv[2])));
}

static QuoVar quo__mod_math_exp(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("exp() requires a number");
  return quo_var_new_num(quo__math.exp(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_log(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("log() requires a number");
  double val = quo__mod_math_as_double(&argv[1]);
  if (val <= 0) return quo_var_new_err("log() argument must be positive");
  return quo_var_new_num(quo__math.log(val));
}

static QuoVar quo__mod_math_log2(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("log2() requires a number");
  double val = quo__mod_math_as_double(&argv[1]);
  if (val <= 0) return quo_var_new_err("log2() argument must be positive");
  return quo_var_new_num(quo__math.log2(val));
}

static QuoVar quo__mod_math_log10(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("log10() requires a number");
  double val = quo__mod_math_as_double(&argv[1]);
  if (val <= 0) return quo_var_new_err("log10() argument must be positive");
  return quo_var_new_num(quo__math.log10(val));
}

static QuoVar quo__mod_math_sin(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("sin() requires a number");
  return quo_var_new_num(quo__math.sin(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_cos(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("cos() requires a number");
  return quo_var_new_num(quo__math.cos(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_tan(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("tan() requires a number");
  return quo_var_new_num(quo__math.tan(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_asin(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("asin() requires a number");
  double val = quo__mod_math_as_double(&argv[1]);
  if (val < -1 || val > 1) return quo_var_new_err("asin() argument must be between -1 and 1");
  return quo_var_new_num(quo__math.asin(val));
}

static QuoVar quo__mod_math_acos(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("acos() requires a number");
  double val = quo__mod_math_as_double(&argv[1]);
  if (val < -1 || val > 1) return quo_var_new_err("acos() argument must be between -1 and 1");
  return quo_var_new_num(quo__math.acos(val));
}

static QuoVar quo__mod_math_atan(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("atan() requires a number");
  return quo_var_new_num(quo__math.atan(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_atan2(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_num(&argv[1]) || !quo_var_is_num(&argv[2])) return quo_var_new_err("atan2() requires two numbers");
  return quo_var_new_num(quo__math.atan2(quo__mod_math_as_double(&argv[1]), quo__mod_math_as_double(&argv[2])));
}

static QuoVar quo__mod_math_sinh(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("sinh() requires a number");
  return quo_var_new_num(quo__math.sinh(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_cosh(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("cosh() requires a number");
  return quo_var_new_num(quo__math.cosh(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_tanh(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("tanh() requires a number");
  return quo_var_new_num(quo__math.tanh(quo__mod_math_as_double(&argv[1])));
}

static QuoVar quo__mod_math_min(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_num(&argv[1]) || !quo_var_is_num(&argv[2])) return quo_var_new_err("min() requires two numbers");
  if (quo_var_is_num(&argv[1]) && quo_var_is_num(&argv[2]))
    return quo_var_new_num(argv[1].val_num < argv[2].val_num ? argv[1].val_num : argv[2].val_num);
  double a = quo__mod_math_as_double(&argv[1]), b = quo__mod_math_as_double(&argv[2]);
  return quo_var_new_num(a < b ? a : b);
}

static QuoVar quo__mod_math_max(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_num(&argv[1]) || !quo_var_is_num(&argv[2])) return quo_var_new_err("max() requires two numbers");
  if (quo_var_is_num(&argv[1]) && quo_var_is_num(&argv[2]))
    return quo_var_new_num(argv[1].val_num > argv[2].val_num ? argv[1].val_num : argv[2].val_num);
  double a = quo__mod_math_as_double(&argv[1]), b = quo__mod_math_as_double(&argv[2]);
  return quo_var_new_num(a > b ? a : b);
}

static QuoVar quo__mod_math_clamp(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 4 || !quo_var_is_num(&argv[1]) || !quo_var_is_num(&argv[2]) || !quo_var_is_num(&argv[3]))
    return quo_var_new_err("clamp() requires three numbers");
  double val = quo__mod_math_as_double(&argv[1]);
  double min = quo__mod_math_as_double(&argv[2]);
  double max = quo__mod_math_as_double(&argv[3]);
  if (val < min) val = min;
  if (val > max) val = max;
  return quo_var_new_num(val);
}

static QuoVar quo__mod_math_random_float(QuoState *s, int64_t argc, QuoVar *argv) { return quo_var_new_num((double)rand() / RAND_MAX); }

static QuoVar quo__mod_math_random(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("random() requires an integer max");
  return quo_var_new_num(rand() % (int64_t)argv[1].val_num);
}

static QuoVar quo__mod_math_deg_to_rad(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("deg_to_rad() requires a number");
  return quo_var_new_num(quo__mod_math_as_double(&argv[1]) * M_PI / 180.0);
}

static QuoVar quo__mod_math_rad_to_deg(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_num(&argv[1])) return quo_var_new_err("rad_to_deg() requires a number");
  return quo_var_new_num(quo__mod_math_as_double(&argv[1]) * 180.0 / M_PI);
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_math_init(QuoState *s) {
  if (!quo__mod_math_load()) {
    fprintf(stderr, "Error: quo-mod-math: libm not found, 'math' namespace not available\n");
    return false;
  }

  QuoDict *ns = quo_state_register_namespace(s, "math");

  // Constants
  quo_state_namespace_add(s, ns, quo_str_new(s, "pi", -1), quo_var_new_num(M_PI));
  quo_state_namespace_add(s, ns, quo_str_new(s, "e", -1), quo_var_new_num(M_E));
  quo_state_namespace_add(s, ns, quo_str_new(s, "tau", -1), quo_var_new_num(M_PI * 2));

  // Rounding
  quo_state_namespace_add_cfn(s, ns, "floor", quo__mod_math_floor);
  quo_state_namespace_add_cfn(s, ns, "ceil", quo__mod_math_ceil);
  quo_state_namespace_add_cfn(s, ns, "round", quo__mod_math_round);
  quo_state_namespace_add_cfn(s, ns, "trunc", quo__mod_math_trunc);
  quo_state_namespace_add_cfn(s, ns, "abs", quo__mod_math_abs);

  // Power and roots
  quo_state_namespace_add_cfn(s, ns, "sqrt", quo__mod_math_sqrt);
  quo_state_namespace_add_cfn(s, ns, "cbrt", quo__mod_math_cbrt);
  quo_state_namespace_add_cfn(s, ns, "pow", quo__mod_math_pow);
  quo_state_namespace_add_cfn(s, ns, "exp", quo__mod_math_exp);
  quo_state_namespace_add_cfn(s, ns, "log", quo__mod_math_log);
  quo_state_namespace_add_cfn(s, ns, "log2", quo__mod_math_log2);
  quo_state_namespace_add_cfn(s, ns, "log10", quo__mod_math_log10);

  // Trigonometry
  quo_state_namespace_add_cfn(s, ns, "sin", quo__mod_math_sin);
  quo_state_namespace_add_cfn(s, ns, "cos", quo__mod_math_cos);
  quo_state_namespace_add_cfn(s, ns, "tan", quo__mod_math_tan);
  quo_state_namespace_add_cfn(s, ns, "asin", quo__mod_math_asin);
  quo_state_namespace_add_cfn(s, ns, "acos", quo__mod_math_acos);
  quo_state_namespace_add_cfn(s, ns, "atan", quo__mod_math_atan);
  quo_state_namespace_add_cfn(s, ns, "atan2", quo__mod_math_atan2);

  // Hyperbolic
  quo_state_namespace_add_cfn(s, ns, "sinh", quo__mod_math_sinh);
  quo_state_namespace_add_cfn(s, ns, "cosh", quo__mod_math_cosh);
  quo_state_namespace_add_cfn(s, ns, "tanh", quo__mod_math_tanh);

  // Min/max/clamp
  quo_state_namespace_add_cfn(s, ns, "min", quo__mod_math_min);
  quo_state_namespace_add_cfn(s, ns, "max", quo__mod_math_max);
  quo_state_namespace_add_cfn(s, ns, "clamp", quo__mod_math_clamp);

  // Random
  quo_state_namespace_add_cfn(s, ns, "random", quo__mod_math_random);
  quo_state_namespace_add_cfn(s, ns, "random_float", quo__mod_math_random_float);

  // Conversions
  quo_state_namespace_add_cfn(s, ns, "deg_to_rad", quo__mod_math_deg_to_rad);
  quo_state_namespace_add_cfn(s, ns, "rad_to_deg", quo__mod_math_rad_to_deg);

  return true;
}

static inline void quo_mod_math_cleanup(void) {
  if (quo__math.handle) {
    dlclose(quo__math.handle);
    quo__math.handle = NULL;
  }
}

#ifdef __cplusplus
}
#endif
