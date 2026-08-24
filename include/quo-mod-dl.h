/*

DESCRIPTION:
    Quo module for dynamic linking with libffi support

QUO API:
    var dl = import("dl")
    var libm = dl.open("libm.so.6") # Loads the libm.so.6 library
    var sqrt_func = libm.sym("sqrt") # Gets the sqrt symbol
    var result = sqrt_func(16.0) # Calls sqrt(16.0) -> 4.0
    libm.close() # Close loaded library
*/

#ifndef QUO_MOD_DL_H
#define QUO_MOD_DL_H

#include "quo.h"
#include <ffi.h>

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
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("dl.open() takes library path string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_type_get_instance("QuoDLHandle");
  if (!dl) return quo_var_new_err("Failed to get QuoDLHandle instance");
  QuoStr *name = quo_var_as_str(&argv[0]);
  void *handle = dlopen(name->data, RTLD_LAZY);
  if (!handle) return quo_var_new_err(dlerror());
  dl->handle = handle;
  return quo_var_new_obj(dl);
}

static inline QuoVar quo__mod_dl_sym(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("sym() takes symbol name string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_var_as_obj(&argv[0]);
  QuoStr *name = quo_var_as_str(&argv[1]);

  // Clear any existing error
  dlerror();
  void *sym = dlsym(dl->handle, name->data);
  char *error = dlerror();
  if (error) return quo_var_new_err(error);

  QuoDLSym *result = (QuoDLSym *)quo_type_get_instance("QuoDLSym");
  result->sym = sym;
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_dl_close(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QuoDLHandle *dl = quo_var_as_dl(&argv[0]);
  if (dlclose(dl->handle) != 0) return quo_var_new_err(dlerror());
  return quo_var_new_nil();
}

// Convert QuoVar to ffi type and value
static inline ffi_type *quo__var_to_ffi_type(QuoVar *var, void **value_ptr) {
  switch (var->type) {
  case QUO_VAR_TYPE_NUM: *value_ptr = &var->val_num; return &ffi_type_double;
  case QUO_VAR_TYPE_BOOL: *value_ptr = &var->val_num; return &ffi_type_sint;
  case QUO_VAR_TYPE_NIL: *value_ptr = NULL; return &ffi_type_pointer;
  case QUO_VAR_TYPE_OBJ:
    switch (var->val_obj->type) {
    case QUO_OBJ_TYPE_STR: {
      QuoStr *str = quo_var_as_str(var);
      *value_ptr = &str->data;
      return &ffi_type_pointer;
    }
    default: *value_ptr = &var->val_obj; return &ffi_type_pointer;
    }
  case QUO_VAR_TYPE_ERROR:
  default: return NULL;
  }
}

// Call function with libffi
static inline QuoVar quo__mod_dl_call(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_obj(&argv[0])) return quo_var_new_err("Invalid function call");

  QuoDLSym *func = (QuoDLSym *)quo_var_as_obj(&argv[0]);
  void (*fn_ptr)() = (void (*)())func->sym;

  int num_args = argc - 1; // Remove function object from args
  ffi_type **arg_types = NULL;
  void **arg_values = NULL;

  if (num_args > 0) {
    arg_types = quo_alloc(NULL, sizeof(ffi_type *) * num_args);
    arg_values = quo_alloc(NULL, sizeof(void *) * num_args);

    for (int i = 0; i < num_args; i++) {
      ffi_type *type = quo__var_to_ffi_type(&argv[i + 1], &arg_values[i]);
      if (!type) {
        quo_dealloc(arg_types);
        quo_dealloc(arg_values);
        return quo_var_new_err("Unsupported argument type");
      }
      arg_types[i] = type;
    }
  }

  // Prepare ffi call interface
  ffi_cif cif;
  ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, num_args, &ffi_type_double, arg_types);

  if (status != FFI_OK) {
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_err("Failed to prepare ffi call");
  }

  // Call the function (assuming double return type for simplicity)
  double result;
  ffi_call(&cif, fn_ptr, &result, arg_values);

  quo_dealloc(arg_types);
  quo_dealloc(arg_values);

  return quo_var_new_num(result);
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_dl_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "dl", NULL, NULL);
  quo_module_register_cfn(m, "open", -1, quo__mod_dl_open);

  QuoObj *dl_handle_type = quo_type_register("QuoDLHandle", sizeof(QuoDLHandle));
  quo_type_add_cfn(dl_handle_type, "sym", -1, quo__mod_dl_sym);
  quo_type_add_cfn(dl_handle_type, "close", -1, quo__mod_dl_close);

  QuoObj *dl_sym_type = quo_type_register("QuoDLSym", sizeof(QuoDLSym));
  quo_type_add_cfn(dl_sym_type, "call", -1, quo__mod_dl_call);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_DL_H
