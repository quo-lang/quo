/*

DESCRIPTION:
    Quo module for dynamic linking with libffi support.

DEPENDENCIES:
    - libffi development files
    - Add -lffi to your linker flags to link against libffi

QUO API:
    var dl = import("dl")
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
  ffi_cif cif;
} QuoDLSym;

QUO_DEFINE_USER_TYPE(QuoDLSym, sym)

// ---------- GLOBALS ---------- //

QuoArena dl__arena = {0};

// ---------- PARSING ---------- //

static inline ffi_type *quo__mod_dl_parse_basic_type(char c) {
  switch (c) {
  case 'i': return &ffi_type_sint;    // int
  case 'b': return &ffi_type_sint;    // bool
  case 'u': return &ffi_type_uint;    // unsigned int
  case 'l': return &ffi_type_slong;   // long
  case 'd': return &ffi_type_double;  // double
  case 'f': return &ffi_type_float;   // float
  case 's': return &ffi_type_pointer; // char*
  case 'p': return &ffi_type_pointer; // void*
  case 'v': return &ffi_type_void;    // void
  default: return NULL;
  }
}

static inline ffi_type *quo__mod_dl_parse_struct_type(const char *s, int *pos) {
  da(ffi_type *) types = {0};
  while (s[*pos] != '}' && s[*pos] != '\0') {
    ffi_type *t;
    if (s[*pos] == '{') {
      (*pos)++;
      t = quo__mod_dl_parse_struct_type(s, pos);
    } else t = quo__mod_dl_parse_basic_type(s[*pos]);
    if (t) da_add(&types, t);
    (*pos)++;
  }
  da_add(&types, NULL);
  ffi_type *result = calloc(1, sizeof(ffi_type));
  result->type = FFI_TYPE_STRUCT;
  result->elements = types.items;
  return result;
}

static inline ffi_type **quo__mod_dl_parse_args_string(const char *s, int *pos, int *argc) {
  da(ffi_type *) types = {0};
  while (s[*pos] != '\0') {
    ffi_type *t = NULL;
    if (s[*pos] == '{') {
      (*pos)++;
      t = quo__mod_dl_parse_struct_type(s, pos);
    } else t = quo__mod_dl_parse_basic_type(s[*pos]);
    if (t) da_add(&types, t);
    (*pos)++;
  }
  *argc = types.count;
  return types.items;
}

static inline ffi_type *quo__mod_dl_parse_return_string(const char *s, int *pos) {
  ffi_type *t = NULL;
  if (s[*pos] == '{') {
    (*pos)++;
    t = quo__mod_dl_parse_struct_type(s, pos);
  } else t = quo__mod_dl_parse_basic_type(s[*pos]);
  return t;
}

static inline void quo__mod_dl_args_free(ffi_type **args, int argc) {
  if (argc == -1) {
    for (ffi_type *arg = *args; arg; arg++)
      if (arg->type == FFI_TYPE_STRUCT) {
        quo__mod_dl_args_free(arg->elements, -1);
        quo_dealloc(arg); // struct ffi_type is allocated on the heap by quo__mod_dl_parse_struct_type
      }
    quo_dealloc(args); // args is dynamic array
  } else {
    for (int i = 0; i < argc; i++)
      if (args[i]->type == FFI_TYPE_STRUCT) {
        quo__mod_dl_args_free(args[i]->elements, -1);
        quo_dealloc(args[i]); // struct ffi_type is allocated on the heap by quo__mod_dl_parse_struct_type
      }
    quo_dealloc(args); // args is dynamic array
  }
}

// static inline void quo__mod_dl_arg_size(ffi_type *arg, int *sizeb) {}

// ---------- CONVERSION ---------- //

#define quo_dtob(x)   ((x) != 0 ? true : false)
#define quo_dtof(x)   ((x) > FLT_MAX ? FLT_MAX : (x) < FLT_MIN ? FLT_MIN : (float)(x))
#define quo_dtos(x)   ((x) > SHRT_MAX ? SHRT_MAX : (x) < SHRT_MIN ? SHRT_MIN : (short)(x))
#define quo_dtoi(x)   ((x) > INT_MAX ? INT_MAX : (x) < INT_MIN ? INT_MIN : (int)(x))
#define quo_dtoui(x)  ((x) > UINT_MAX ? UINT_MAX : (x) < 0 ? 0 : (unsigned int)(x))
#define quo_dtol(x)   ((x) > LONG_MAX ? LONG_MAX : (x) < LONG_MIN ? LONG_MIN : (long)(x))
#define quo_dtoul(x)  ((x) > ULONG_MAX ? ULONG_MAX : (x) < 0 ? 0 : (unsigned long)(x))
#define quo_dtoll(x)  ((x) > LLONG_MAX ? LLONG_MAX : (x) < LLONG_MIN ? LLONG_MIN : (long long)(x))
#define quo_dtoull(x) ((x) > ULLONG_MAX ? ULLONG_MAX : (x) < 0 ? 0 : (unsigned long long)(x))

// Convert QuoVar to ffi type and value based on declared type
static inline bool quo__mod_dl_var_to_ffi(QuoVar *var, ffi_type *type, void **value_ptr) {
  static float float_val;
  static int int_val;
  static unsigned int uint_val;
  static long long_val;
  static void *null_ptr = NULL;
  // Handle based on the declared type
  if (type == &ffi_type_double) {
    if (!quo_var_is_num(var)) return false;
    *value_ptr = &var->val_num;
  } else if (type == &ffi_type_float) {
    if (!quo_var_is_num(var)) return false;
    float_val = (float)var->val_num;
    *value_ptr = &float_val;
  } else if (type == &ffi_type_sint || type == &ffi_type_uint || type == &ffi_type_slong) {
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
    if (quo_var_is_str(var)) *value_ptr = &quo_var_as_str(var)->data;
    else if (quo_var_is_nil(var)) *value_ptr = &null_ptr;
    else if (quo_var_is_obj(var)) *value_ptr = &var->val_obj;
    else return false;
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
    void *handle = NULL;
    if (quo_strprefix(name->data, "./")) {
      char *path = quo_strdupf("%s/%s", m->cwd, name->data + 2);
      handle = dlopen(path, RTLD_LAZY);
      quo_dealloc(path);
    } else handle = dlopen(name->data, RTLD_LAZY);
    if (!handle) continue;
    dl->handle = handle;
    break;
  }
  if (!dl->handle) return quo_var_new_err("Failed to find library");
  return quo_var_new_obj(dl);
}

static inline void quo__mod_dl_sym_free(QuoDLSym *s) {
  quo__mod_dl_args_free(s->cif.arg_types, s->cif.nargs);
  if (s->cif.rtype->type == FFI_TYPE_STRUCT) {
    quo__mod_dl_args_free(s->cif.rtype->elements, -1);
    quo_dealloc(s->cif.rtype);
  }
}

static inline QuoVar quo__mod_dl_sym(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 4) return quo_var_new_err("sym() requires: symbol name, return type string, and argument types string");
  QuoDLHandle *dl = quo_var_as_dl(&argv[0]);
  if (!quo_var_is_str(&argv[1])) return quo_var_new_err("Second argument must be a symbol name string");
  QuoStr *name = quo_var_as_str(&argv[1]);
  // Validate return type
  if (!quo_var_is_str(&argv[2])) return quo_var_new_err("Third argument must be a return type string");
  if (quo_var_as_str(&argv[2])->len == 0) return quo_var_new_err("Return type string cannot be empty");
  int ret_pos = 0;
  ffi_type *ret_type = quo__mod_dl_parse_return_string(quo_var_as_str(&argv[2])->data, &ret_pos);
  if (!ret_type) return quo_var_new_err("Invalid return type");
  if (!quo_var_is_str(&argv[3])) return quo_var_new_err("Fourth argument must be arguments type strings");
  if (quo_var_as_str(&argv[3])->len == 0) return quo_var_new_err("Argument types string cannot be empty");
  // Get args
  int arg_pos = 0;
  int nargs = 0;
  ffi_type **args_types = quo__mod_dl_parse_args_string(quo_var_as_str(&argv[3])->data, &arg_pos, &nargs);
  if (args_types[0] == &ffi_type_void) nargs = 0;
  void *sym = dlsym(dl->handle, name->data);
  if (!sym) {
    // quo_dealloc(arg_types);
    return quo_var_new_err(dlerror());
  }
  QuoDLSym *symbol = (QuoDLSym *)quo_type_get_instance("QuoDLSym");
  if (!symbol) {
    // quo_dealloc(arg_types);
    return quo_var_new_err("Failed to get QuoDLSym instance");
  }
  symbol->sym = sym;
  ffi_status status = ffi_prep_cif(&symbol->cif, FFI_DEFAULT_ABI, nargs, ret_type, args_types);
  if (status != FFI_OK) {
    quo__mod_dl_sym_free(symbol);
    return quo_var_new_err("Failed to prepare ffi call");
  }
  return quo_var_new_obj(symbol);
}

static inline QuoVar quo__mod_dl_sym_close(QuoModule *m, int argc, QuoVar *argv) {
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

  int expected_args = func->cif.nargs;
  int provided_args = argc - 1; // Remove function object from args
  if (expected_args != provided_args) return quo_var_new_err("Argument count mismatch");

  ffi_type **arg_types = NULL;
  void **arg_values = NULL;

  if (provided_args > 0) {
    arg_types = quo_alloc(NULL, sizeof(ffi_type *) * provided_args);
    arg_values = quo_alloc(NULL, sizeof(void *) * provided_args);
    for (int i = 0; i < provided_args; i++) {
      ffi_type *type = (func->cif.arg_types) ? func->cif.arg_types[i] : &ffi_type_double;
      if (!quo__mod_dl_var_to_ffi(&argv[i + 1], type, &arg_values[i])) {
        quo_dealloc(arg_types);
        quo_dealloc(arg_values);
        return quo_var_new_err("Type mismatch in argument");
      }
      arg_types[i] = type;
    }
  }

  // Call the function and handle return type
  if (func->cif.rtype == &ffi_type_void) {
    ffi_call(&func->cif, fn_ptr, NULL, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_nil();
  } else if (func->cif.rtype == &ffi_type_double) {
    double result;
    ffi_call(&func->cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num(result);
  } else if (func->cif.rtype == &ffi_type_float) {
    float result;
    ffi_call(&func->cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->cif.rtype == &ffi_type_sint) {
    int result;
    ffi_call(&func->cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->cif.rtype == &ffi_type_uint) {
    unsigned int result;
    ffi_call(&func->cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->cif.rtype == &ffi_type_slong) {
    long result;
    ffi_call(&func->cif, fn_ptr, &result, arg_values);
    quo_dealloc(arg_types);
    quo_dealloc(arg_values);
    return quo_var_new_num((double)result);
  } else if (func->cif.rtype == &ffi_type_pointer) {
    void *result;
    ffi_call(&func->cif, fn_ptr, &result, arg_values);
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

static inline void quo__mod_dl_cleanup(QuoModule *m) {
  QUO_UNUSED(m);
  quo_arena_destroy(&dl__arena);
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_dl_init(QuoModule *parent) {
  dl__arena = quo_arena_create(1024);

  QuoModule *m = quo_module_new(parent->cwd, "dl", NULL, quo__mod_dl_cleanup);
  quo_module_register_cfn(m, "open", -1, quo__mod_dl_open);

  QuoObj *dl_handle_type = quo_type_register("QuoDLHandle", sizeof(QuoDLHandle));
  quo_type_add_cfn(dl_handle_type, "sym", -1, quo__mod_dl_sym);
  quo_type_add_cfn(dl_handle_type, "close", -1, quo__mod_dl_sym_close);

  QuoObj *dl_sym_type = quo_type_register("QuoDLSym", sizeof(QuoDLSym));
  quo_type_add_cfn(dl_sym_type, "call", -1, quo__mod_dl_call);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_DL_H
