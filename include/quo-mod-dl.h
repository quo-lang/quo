/*

DESCRIPTION:
    Quo module for dynamic linking with libffi support.

DEPENDENCIES:
    - libffi development files
    - Add -lffi to your linker flags to link against libffi

QUO API:
    var dl = import("dl")
    var libm = dl.open("libm.so.6")

    # Simple call (defaults to double/double)
    var sqrt_func = libm.sym("sqrt")
    var result = sqrt_func.call(16.0)

    # Call with explicit types: sym(name, return_type, [arg_types])
    var atoi = libm.sym("atoi", "i", ["s"])  # int return, string arg
    var num = atoi.call("42")

    # Types:
    # "v" = void
    # "i" = int
    # "d" = double
    # "f" = float
    # "s" = string (char*)
    # "p" = pointer
    # "b" = bool

    libm.close()
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
  ffi_type *return_type;
  ffi_type **arg_types;
  int num_args;
} QuoDLSym;

QUO_DEFINE_USER_TYPE(QuoDLSym, sym)

// ---------- GLOBALS ---------- //

QuoArena dl__arena = {0};

// ---------- UTILS ---------- //

// Parse type string to ffi_type
static inline ffi_type *quo__mod_dl_parse_type(char c) {
  switch (c) {
  case 'v': return &ffi_type_void;
  case 'i': return &ffi_type_sint;
  case 'u': return &ffi_type_uint;
  case 'l': return &ffi_type_slong;
  case 'd': return &ffi_type_double;
  case 'f': return &ffi_type_float;
  case 's': return &ffi_type_pointer; // char*
  case 'p': return &ffi_type_pointer; // void*
  case 'b': return &ffi_type_sint;    // bool as int
  default: return NULL;
  }
}

// Convert QuoVar to ffi type and value based on declared type
static inline bool quo__mod_dl_var_to_ffi(QuoVar *var, ffi_type *type, void **value_ptr) {
  // Handle based on the declared type
  if (type == &ffi_type_double) {
    if (!quo_var_is_num(var)) return false;
    *value_ptr = &var->val_num;
  } else if (type == &ffi_type_float) {
    static float float_val;
    if (!quo_var_is_num(var)) return false;
    float_val = (float)var->val_num;
    *value_ptr = &float_val;
  } else if (type == &ffi_type_sint || type == &ffi_type_uint || type == &ffi_type_slong) {
    static int int_val;
    static unsigned int uint_val;
    static long long_val;
    if (quo_var_is_num(var)) {
      if (type == &ffi_type_sint) {
        int_val = (int)var->val_num;
        *value_ptr = &int_val;
      } else if (type == &ffi_type_uint) {
        uint_val = (unsigned int)var->val_num;
        *value_ptr = &uint_val;
      } else {
        long_val = (long)var->val_num;
        *value_ptr = &long_val;
      }
    } else if (quo_var_is_bool(var)) {
      if (type == &ffi_type_sint) {
        int_val = quo_var_as_bool(var) ? 1 : 0;
        *value_ptr = &int_val;
      } else if (type == &ffi_type_uint) {
        uint_val = quo_var_as_bool(var) ? 1 : 0;
        *value_ptr = &uint_val;
      } else {
        long_val = quo_var_as_bool(var) ? 1 : 0;
        *value_ptr = &long_val;
      }
    } else {
      return false;
    }
  } else if (type == &ffi_type_pointer) {
    if (quo_var_is_str(var)) {
      QuoStr *str = quo_var_as_str(var);
      *value_ptr = &str->data;
    } else if (quo_var_is_nil(var)) {
      static void *null_ptr = NULL;
      *value_ptr = &null_ptr;
    } else if (quo_var_is_obj(var)) {
      *value_ptr = &var->val_obj;
    } else {
      return false;
    }
  } else {
    return false;
  }
  return true;
}

// ---------- PRIVATE API ---------- //

static inline QuoVar quo__mod_dl_open(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc == 0) return quo_var_new_err("dl.open() takes at least one library path string");
  QuoDLHandle *dl = (QuoDLHandle *)quo_type_get_instance("QuoDLHandle");
  if (!dl) return quo_var_new_err("Failed to get QuoDLHandle instance");
  for (int i = 0; i < argc; i++) {
    if (!quo_var_is_str(&argv[i])) continue;
    QuoStr *name = quo_var_as_str(&argv[i]);
    void *handle = dlopen(name->data, RTLD_LAZY);
    if (!handle) continue;
    dl->handle = handle;
    break;
  }
  if (!dl->handle) return quo_var_new_err("Failed to find library");
  return quo_var_new_obj(dl);
}

static inline QuoVar quo__mod_dl_sym(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 4) return quo_var_new_err("sym() requires: symbol name, return type, and argument types array");
  QuoDLHandle *dl = quo_var_as_dl(&argv[0]);
  if (!quo_var_is_str(&argv[1])) return quo_var_new_err("Second argument must be a symbol name string");
  QuoStr *name = quo_var_as_str(&argv[1]);
  // Validate return type
  if (!quo_var_is_str(&argv[2])) return quo_var_new_err("Third argument must be a return type string");
  QuoStr *ret_type_str = quo_var_as_str(&argv[2]);
  if (ret_type_str->len == 0) return quo_var_new_err("Return type string cannot be empty");
  ffi_type *ret_type = quo__mod_dl_parse_type(ret_type_str->data[0]);
  if (!ret_type) return quo_var_new_err("Invalid return type");
  // Validate argument types array
  if (!quo_var_is_arr(&argv[3])) return quo_var_new_err("Fourth argument must be an array of argument type strings");
  QuoArr *args_arr = quo_var_as_arr(&argv[3]);
  int num_args = quo_arr_len(args_arr);
  // Allocate and parse argument types
  ffi_type **arg_types = NULL;
  if (num_args > 0) {
    arg_types = quo_alloc(NULL, sizeof(ffi_type *) * num_args);
    for (int i = 0; i < num_args; i++) {
      QuoVar arg_type_var = quo_arr_get(args_arr, i);
      if (!quo_var_is_str(&arg_type_var)) {
        quo_dealloc(arg_types);
        return quo_var_new_err("Argument type must be a string");
      }
      QuoStr *type_str = quo_var_as_str(&arg_type_var);
      if (type_str->len == 0) {
        quo_dealloc(arg_types);
        return quo_var_new_err("Argument type string cannot be empty");
      }
      ffi_type *type = quo__mod_dl_parse_type(type_str->data[0]);
      if (!type) {
        quo_dealloc(arg_types);
        return quo_var_new_err("Invalid argument type");
      }
      arg_types[i] = type;
    }
  }
  // Clear any existing error
  dlerror();
  void *sym = dlsym(dl->handle, name->data);
  char *error = dlerror();
  if (error) {
    quo_dealloc(arg_types);
    return quo_var_new_err(error);
  }
  // Create and initialize QuoDLSym object
  QuoDLSym *result = (QuoDLSym *)quo_type_get_instance("QuoDLSym");
  if (!result) {
    quo_dealloc(arg_types);
    return quo_var_new_err("Failed to get QuoDLSym instance");
  }
  result->sym = sym;
  result->return_type = ret_type;
  result->arg_types = arg_types;
  result->num_args = num_args;
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_dl_close(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QuoDLHandle *dl = quo_var_as_dl(&argv[0]);
  if (dlclose(dl->handle) != 0) return quo_var_new_err(dlerror());
  return quo_var_new_nil();
}

// Call function with libffi
static inline QuoVar quo__mod_dl_call(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_obj(&argv[0])) return quo_var_new_err("Invalid function call");

  QuoDLSym *func = (QuoDLSym *)quo_var_as_obj(&argv[0]);
  void (*fn_ptr)() = (void (*)())func->sym;

  int expected_args = func->num_args;
  int provided_args = argc - 1; // Remove function object from args
  if (expected_args != provided_args) return quo_var_new_err("Argument count mismatch");

  ffi_type **arg_types = NULL;
  void **arg_values = NULL;

  if (provided_args > 0) {
    arg_types = quo_alloc(NULL, sizeof(ffi_type *) * provided_args);
    arg_values = quo_alloc(NULL, sizeof(void *) * provided_args);
    for (int i = 0; i < provided_args; i++) {
      ffi_type *type = (func->arg_types) ? func->arg_types[i] : &ffi_type_double;
      if (!quo__mod_dl_var_to_ffi(&argv[i + 1], type, &arg_values[i])) {
        quo_dealloc(arg_types);
        quo_dealloc(arg_values);
        return quo_var_new_err("Type mismatch in argument");
      }
      arg_types[i] = type;
    }
  }

  // Prepare ffi call interface
  ffi_cif cif;
  ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, provided_args, func->return_type, arg_types);

  if (status != FFI_OK) {
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_err("Failed to prepare ffi call");
  }

  // Call the function and handle return type
  if (func->return_type == &ffi_type_void) {
    ffi_call(&cif, fn_ptr, NULL, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_nil();
  } else if (func->return_type == &ffi_type_double) {
    double result;
    ffi_call(&cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num(result);
  } else if (func->return_type == &ffi_type_float) {
    float result;
    ffi_call(&cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->return_type == &ffi_type_sint) {
    int result;
    ffi_call(&cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->return_type == &ffi_type_uint) {
    unsigned int result;
    ffi_call(&cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->return_type == &ffi_type_slong) {
    long result;
    ffi_call(&cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->return_type == &ffi_type_pointer) {
    void *result;
    ffi_call(&cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    if (result == NULL) return quo_var_new_nil();
    // Assume pointer return is a string
    return quo_var_new_obj(quo_str_new((const char *)result, -1));
  }

  quo_dealloc(arg_types);
  quo_dealloc(arg_values);
  return quo_var_new_err("Unsupported return type");
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_dl_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent->cwd, "dl", NULL, NULL);
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
