#pragma once

#include "quo.h"

#include <math.h>

static QuoVar quo__mod_math_floor(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("floor() requires a number");
  return quo_var_new_num(floor(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_ceil(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("ceil() requires a number");
  return quo_var_new_num(ceil(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_round(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("round() requires a number");
  return quo_var_new_num(round(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_trunc(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("trunc() requires a number");
  return quo_var_new_num(trunc(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_abs(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("abs() requires a number");
  if (quo_var_is_num(&argv[0])) return quo_var_new_num(llabs((int64_t)argv[0].val_num));
  return quo_var_new_num(fabs(argv[0].val_num));
}

static QuoVar quo__mod_math_sqrt(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("sqrt() requires a number");
  return quo_var_new_num(sqrt(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_cbrt(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("cbrt() requires a number");
  return quo_var_new_num(cbrt(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_pow(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_num(&argv[0]) || !quo_var_is_num(&argv[1])) return quo_var_new_err("pow() requires two numbers");
  return quo_var_new_num(pow(quo_var_as_num(&argv[0]), quo_var_as_num(&argv[1])));
}

static QuoVar quo__mod_math_exp(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("exp() requires a number");
  return quo_var_new_num(exp(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_log(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("log() requires a number");
  double val = quo_var_as_num(&argv[0]);
  if (val <= 0) return quo_var_new_err("log() argument must be positive");
  return quo_var_new_num(log(val));
}

static QuoVar quo__mod_math_log2(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("log2() requires a number");
  double val = quo_var_as_num(&argv[0]);
  if (val <= 0) return quo_var_new_err("log2() argument must be positive");
  return quo_var_new_num(log2(val));
}

static QuoVar quo__mod_math_log10(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("log10() requires a number");
  double val = quo_var_as_num(&argv[0]);
  if (val <= 0) return quo_var_new_err("log10() argument must be positive");
  return quo_var_new_num(log10(val));
}

static QuoVar quo__mod_math_sin(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("sin() requires a number");
  return quo_var_new_num(sin(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_cos(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("cos() requires a number");
  return quo_var_new_num(cos(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_tan(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("tan() requires a number");
  return quo_var_new_num(tan(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_asin(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("asin() requires a number");
  double val = quo_var_as_num(&argv[0]);
  if (val < -1 || val > 1) return quo_var_new_err("asin() argument must be between -1 and 1");
  return quo_var_new_num(asin(val));
}

static QuoVar quo__mod_math_acos(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("acos() requires a number");
  double val = quo_var_as_num(&argv[0]);
  if (val < -1 || val > 1) return quo_var_new_err("acos() argument must be between -1 and 1");
  return quo_var_new_num(acos(val));
}

static QuoVar quo__mod_math_atan(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("atan() requires a number");
  return quo_var_new_num(atan(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_atan2(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_num(&argv[0]) || !quo_var_is_num(&argv[1])) return quo_var_new_err("atan2() requires two numbers");
  return quo_var_new_num(atan2(quo_var_as_num(&argv[0]), quo_var_as_num(&argv[1])));
}

static QuoVar quo__mod_math_sinh(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("sinh() requires a number");
  return quo_var_new_num(sinh(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_cosh(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("cosh() requires a number");
  return quo_var_new_num(cosh(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_tanh(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("tanh() requires a number");
  return quo_var_new_num(tanh(quo_var_as_num(&argv[0])));
}

static QuoVar quo__mod_math_min(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_num(&argv[0]) || !quo_var_is_num(&argv[1])) return quo_var_new_err("min() requires two numbers");
  if (quo_var_is_num(&argv[0]) && quo_var_is_num(&argv[1]))
    return quo_var_new_num(argv[0].val_num < argv[1].val_num ? argv[0].val_num : argv[1].val_num);
  double a = quo_var_as_num(&argv[0]), b = quo_var_as_num(&argv[1]);
  return quo_var_new_num(a < b ? a : b);
}

static QuoVar quo__mod_math_max(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_num(&argv[0]) || !quo_var_is_num(&argv[1])) return quo_var_new_err("max() requires two numbers");
  if (quo_var_is_num(&argv[0]) && quo_var_is_num(&argv[1]))
    return quo_var_new_num(argv[0].val_num > argv[1].val_num ? argv[0].val_num : argv[1].val_num);
  double a = quo_var_as_num(&argv[0]), b = quo_var_as_num(&argv[1]);
  return quo_var_new_num(a > b ? a : b);
}

static QuoVar quo__mod_math_clamp(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 3 || !quo_var_is_num(&argv[0]) || !quo_var_is_num(&argv[1]) || !quo_var_is_num(&argv[2]))
    return quo_var_new_err("clamp() requires three numbers");
  double val = quo_var_as_num(&argv[0]);
  double min = quo_var_as_num(&argv[1]);
  double max = quo_var_as_num(&argv[2]);
  if (val < min) val = min;
  if (val > max) val = max;
  return quo_var_new_num(val);
}

static QuoVar quo__mod_math_random_float(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  return quo_var_new_num((double)rand() / RAND_MAX);
}

static QuoVar quo__mod_math_random(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("random() requires an integer max");
  return quo_var_new_num(rand() % (int64_t)argv[0].val_num);
}

static QuoVar quo__mod_math_deg_to_rad(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("deg_to_rad() requires a number");
  return quo_var_new_num(quo_var_as_num(&argv[0]) * 3.1415926535897932384 / 180.0);
}

static QuoVar quo__mod_math_rad_to_deg(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_num(&argv[0])) return quo_var_new_err("rad_to_deg() requires a number");
  return quo_var_new_num(quo_var_as_num(&argv[0]) * 180.0 / 3.1415926535897932384);
}

static inline void quo_mod_math_load(void) {
  QuoModule *m = quo_module_new(NULL, "math", NULL, NULL);
  quo_module_register_cfn(m, "floor", -1, quo__mod_math_floor);
  quo_module_register_cfn(m, "ceil", -1, quo__mod_math_ceil);
  quo_module_register_cfn(m, "round", -1, quo__mod_math_round);
  quo_module_register_cfn(m, "trunc", -1, quo__mod_math_trunc);
  quo_module_register_cfn(m, "abs", -1, quo__mod_math_abs);
  quo_module_register_cfn(m, "sqrt", -1, quo__mod_math_sqrt);
  quo_module_register_cfn(m, "cbrt", -1, quo__mod_math_cbrt);
  quo_module_register_cfn(m, "pow", -1, quo__mod_math_pow);
  quo_module_register_cfn(m, "exp", -1, quo__mod_math_exp);
  quo_module_register_cfn(m, "log", -1, quo__mod_math_log);
  quo_module_register_cfn(m, "log2", -1, quo__mod_math_log2);
  quo_module_register_cfn(m, "log10", -1, quo__mod_math_log10);
  quo_module_register_cfn(m, "sin", -1, quo__mod_math_sin);
  quo_module_register_cfn(m, "cos", -1, quo__mod_math_cos);
  quo_module_register_cfn(m, "tan", -1, quo__mod_math_tan);
  quo_module_register_cfn(m, "asin", -1, quo__mod_math_asin);
  quo_module_register_cfn(m, "acos", -1, quo__mod_math_acos);
  quo_module_register_cfn(m, "atan", -1, quo__mod_math_atan);
  quo_module_register_cfn(m, "atan2", -1, quo__mod_math_atan2);
  quo_module_register_cfn(m, "sinh", -1, quo__mod_math_sinh);
  quo_module_register_cfn(m, "cosh", -1, quo__mod_math_cosh);
  quo_module_register_cfn(m, "tanh", -1, quo__mod_math_tanh);
  quo_module_register_cfn(m, "min", -1, quo__mod_math_min);
  quo_module_register_cfn(m, "max", -1, quo__mod_math_max);
  quo_module_register_cfn(m, "clamp", -1, quo__mod_math_clamp);
  quo_module_register_cfn(m, "random", -1, quo__mod_math_random);
  quo_module_register_cfn(m, "random_float", -1, quo__mod_math_random_float);
  quo_module_register_cfn(m, "deg_to_rad", -1, quo__mod_math_deg_to_rad);
  quo_module_register_cfn(m, "rad_to_deg", -1, quo__mod_math_rad_to_deg);

  quo_module_register_var(m, quo_str_new_interned("pi", -1), quo_var_new_num(M_PI));
  quo_module_register_var(m, quo_str_new_interned("e", -1), quo_var_new_num(M_E));
  quo_module_register_var(m, quo_str_new_interned("tau", -1), quo_var_new_num(M_PI * 2));
}
