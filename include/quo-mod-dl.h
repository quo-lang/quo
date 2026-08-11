/*

DESCRIPTION:
    Quo module for dynamic linking

USAGE IN C:
    #include "quo-mod-dl.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_dl_init, NULL);
    ...

USAGE IN QUO:
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

static inline QuoVar quo__mod_dl_open(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("dl.open() takes library path string");
  QuoStr *name = quo_var_as_str(&argv[1]);
  void *handle = dlopen(name->data, RTLD_LAZY);
  if (!handle) return quo_var_new_err(dlerror());
  QuoDLHandle *dl = (QuoDLHandle *)quo_state_get_type_instance(s, "QuoDLHandle", -1);
  dl->handle = handle;
  return quo_var_new_obj(dl);
}

static inline QuoVar quo__mod_dl_sym(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("sym() takes symbol name string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  QuoStr *name = quo_var_as_str(&argv[1]);
  void *sym = dlsym(dl->handle, name->data);
  if (!sym) return quo_var_new_err(dlerror());
  QuoDLSym *result = (QuoDLSym *)quo_state_get_type_instance(s, "QuoDLSym", -1);
  result->sym = sym;
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_dl_call(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("call() takes function name string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  QuoStr *name = quo_var_as_str(&argv[1]);
  void *func_ptr = dlsym(dl->handle, name->data);
  if (!func_ptr) return quo_var_new_err(dlerror());

  QuoVar arg = quo_var_to_num(&argv[2]);

  typedef double (*double_func_t)(double);
  double result = ((double_func_t)func_ptr)(arg.val_num);
  return quo_var_new_num(result);
}

static inline QuoVar quo__mod_dl_close(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_obj(&argv[0])) return quo_var_new_err("close() takes dl handle");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  if (dlclose(dl->handle) != 0) return quo_var_new_err(dlerror());
  return quo_var_new_nil();
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_dl_init(QuoState *s) {
  QuoObj *dl_handle_type = quo_state_register_type(s, "QuoDLHandle", -1, sizeof(QuoDLHandle));
  quo_state_type_add_cfn(s, dl_handle_type, "sym", -1, quo__mod_dl_sym);
  quo_state_type_add_cfn(s, dl_handle_type, "call", -1, quo__mod_dl_call);
  quo_state_type_add_cfn(s, dl_handle_type, "close", -1, quo__mod_dl_close);

  QuoObj *dl_sym_type = quo_state_register_type(s, "QuoDLSym", -1, sizeof(QuoDLSym));

  QuoDict *dl_ns = quo_state_register_namespace(s, "dl");
  quo_state_namespace_add_cfn(s, dl_ns, "open", quo__mod_dl_open);
  return true;
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_DL_H
