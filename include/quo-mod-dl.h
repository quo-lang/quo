/*

DESCRIPTION:
    Quo module for dynamic linking

QUO API:
    var dl = import("dl")
    var libm = dl.open("libm.so.6") # Loads the libm.so.6 library
    var sqrt = dl.sym(libm, "sqrt") # Gets the sqrt symbol from the libm.so library
    var result = dl.call(sqrt, 4.0) # Calls sqrt(4.0) and returns 2.0

*/

#ifndef QUO_MOD_DL_H
#define QUO_MOD_DL_H

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  QuoObj obj;
  void *handle;
} QuoDLHandle;

QUO_DEFINE_USER_TYPE(QuoDLHandle, dl)

typedef struct {
  QuoObj obj;
  void *sym;
} QuoDLSym;

QUO_DEFINE_USER_TYPE(QuoDLSym, sym)

// ---------- PRIVATE API ---------- //

static inline QuoVar quo__mod_dl_open(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("dl.open() takes library path string");
  QuoStr *name = quo_var_as_str(&argv[0]);
  void *handle = dlopen(name->data, RTLD_LAZY);
  if (!handle) return quo_var_new_err(dlerror());
  QuoDLHandle *dl = (QuoDLHandle *)quo_module_get_type_instance(m, "QuoDLHandle", -1);
  dl->handle = handle;
  return quo_var_new_obj(dl);
}

static inline QuoVar quo__mod_dl_sym(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("sym() takes symbol name string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  QuoStr *name = quo_var_as_str(&argv[0]);
  void *sym = dlsym(dl->handle, name->data);
  if (!sym) return quo_var_new_err(dlerror());
  QuoDLSym *result = (QuoDLSym *)quo_module_get_type_instance(m, "QuoDLSym", -1);
  result->sym = sym;
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_dl_call(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("call() takes function name string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  QuoStr *name = quo_var_as_str(&argv[0]);
  void *func_ptr = dlsym(dl->handle, name->data);
  if (!func_ptr) return quo_var_new_err(dlerror());

  QuoVar arg = quo_var_to_num(&argv[0]);

  typedef double (*double_func_t)(double);
  double result = ((double_func_t)func_ptr)(arg.val_num);
  return quo_var_new_num(result);
}

static inline QuoVar quo__mod_dl_close(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_obj(&argv[0])) return quo_var_new_err("close() takes dl handle");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  if (dlclose(dl->handle) != 0) return quo_var_new_err(dlerror());
  return quo_var_new_nil();
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_dl_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "dl", NULL, NULL);
  quo_module_register_cfn(m, "open", -1, quo__mod_dl_open);

  QuoObj *dl_handle_type = quo_module_register_type(m, "QuoDLHandle", -1, sizeof(QuoDLHandle));
  quo_module_type_add_cfn(m, dl_handle_type, "sym", -1, quo__mod_dl_sym);
  quo_module_type_add_cfn(m, dl_handle_type, "call", -1, quo__mod_dl_call);
  quo_module_type_add_cfn(m, dl_handle_type, "close", -1, quo__mod_dl_close);

  quo_module_register_type(m, "QuoDLSym", -1, sizeof(QuoDLSym));
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_DL_H
