#ifndef QUO_H
#define QUO_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define RTLD_LAZY             0
#define dlopen(path, flags)   LoadLibraryA(path)
#define dlsym(handle, symbol) GetProcAddress((HMODULE)(handle), (symbol))
#define dlclose(handle)       (FreeLibrary((HMODULE)(handle)) ? 0 : -1)
#define dlerror()             "Windows error"
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

// ------------------------------------------------------------------------------------------ //
//                                         DEFINITIONS                                        //
// ------------------------------------------------------------------------------------------ //

// --- MEMORY --- //

void *quo_alloc(void *ptr, uint64_t size);
void quo_dealloc(void *ptr);

// --- DYNAMIC ARRAY TYPES --- //

// Dynamic array
#define da(T)                                                                                                                              \
  struct {                                                                                                                                 \
    int count, capacity;                                                                                                                   \
    T *items;                                                                                                                              \
  }
#define da_count(a)    ((a)->count)
#define da_capacity(a) ((a)->capacity)
#define da_items(a)    ((a)->items)
#define da_at(a, idx)  ((a)->items[(idx)])
#define da_grow(a, cap)                                                                                                                    \
  if ((cap) > da_capacity(a)) da_capacity(a) = (cap), da_items(a) = quo_alloc(da_items(a), da_capacity(a) * sizeof(*da_items(a)))
#define da_add(a, item)                                                                                                                    \
  do {                                                                                                                                     \
    if (da_count(a) == da_capacity(a)) da_grow(a, da_capacity(a) == 0 ? 2 : da_capacity(a) * 2);                                           \
    da_items(a)[da_count(a)++] = (item);                                                                                                   \
  } while (0)
#define da_pop(a) (da_items(a)[--da_count(a)])
#define da_free(a)                                                                                                                         \
  if (da_items(a)) quo_dealloc(da_items(a))

// String Builder
typedef da(char) QuoStringBuilder;
#define quo_sb_new() ((QuoStringBuilder){0})
#define quo_sb_append(sb, str, len)                                                                                                        \
  for (uint64_t i = 0; i < (len); ++i) da_add(sb, (str)[i])
#define quo_sb_append_cstr(sb, cstr) quo_sb_append(sb, (cstr), strlen(cstr))
#define quo_sb_null_terminate(sb)    da_add(sb, '\0');
#define quo_sb_string(sb)            da_items(sb)
#define quo_sb_reset(sb)             da_count(sb) = 0
#define quo_sb_free(sb)              da_free(sb)

// Hash Table
typedef da(struct QuoHashTableEntry) QuoHashTable;

// Source code token
typedef struct {
  enum QuoTokenType {
    QUO_TT_NONE,
    QUO_TT_EOF,     // '\0'
    QUO_TT_ERROR,   // Error
    QUO_TT_COMMENT, // Comment
    QUO_TT_ID,      // Identifier
    // Literals
    QUO_TT_LITERAL_NUM, // 123.4567890, 1234567890, 10_000_000
    QUO_TT_LITERAL_STR, // "Hello, World!"
    // Keywords
    QUO_TT_VAR,      // var
    QUO_TT_FN,       // fn
    QUO_TT_LOOP,     // loop
    QUO_TT_BREAK,    // break
    QUO_TT_CONTINUE, // continue
    QUO_TT_IF,       // if
    QUO_TT_ELSE,     // else
    QUO_TT_TRUE,     // true
    QUO_TT_FALSE,    // false
    QUO_TT_NIL,      // nil
    QUO_TT_AND,      // and
    QUO_TT_OR,       // or
    QUO_TT_RETURN,   // return
    QUO_TT_IMPORT,   // import
    // Single-character symbols
    QUO_TT_DOT,      // .
    QUO_TT_OPAREN,   // (
    QUO_TT_CPAREN,   // )
    QUO_TT_OBRACE,   // {
    QUO_TT_CBRACE,   // }
    QUO_TT_OBRACKET, // [
    QUO_TT_CBRACKET, // ]
    QUO_TT_COMMA,    // ,
    QUO_TT_COLON,    // :
    QUO_TT_EQ,       // =
    QUO_TT_LT,       // <
    QUO_TT_GT,       // >
    QUO_TT_PLUS,     // +
    QUO_TT_MINUS,    // -
    QUO_TT_STAR,     // *
    QUO_TT_SLASH,    // /
    QUO_TT_BANG,     // !
    QUO_TT_MOD,      // %
    QUO_TT_QUESTION, // ?
    // Double-character symbols
    QUO_TT_BANGEQ,   // !=
    QUO_TT_DIVEQ,    // /=
    QUO_TT_DOUBLEEQ, // ==
    QUO_TT_GTEQ,     // >=
    QUO_TT_LTEQ,     // <=
    QUO_TT_MINUSEQ,  // -=
    QUO_TT_MULEQ,    // *=
    QUO_TT_PLUSEQ,   // +=
  } type;
  const char *start, *error_msg;
  int len, line, column;
} QuoToken;

const char *quo_token_type_str(enum QuoTokenType t);

typedef da(QuoToken) QuoTokenList;
typedef struct QuoExpr QuoExpr;
typedef struct QuoStmt QuoStmt;
typedef da(QuoExpr *) QuoExprList;
typedef da(QuoStmt *) QuoStmtList;
typedef QuoStmtList QuoAST;
typedef struct QuoState QuoState;
typedef struct QuoModule QuoModule;
typedef struct QuoParser QuoParser;

typedef struct {
  QuoExpr *key;
  QuoExpr *value;
} QuoExprDictPair;

struct QuoExpr {
  enum QuoExprType {
    QUO_EXPR_LITERAL,
    QUO_EXPR_ARRAY,
    QUO_EXPR_DICT,
    QUO_EXPR_FUNCTION,
    QUO_EXPR_VARIABLE,
    QUO_EXPR_BINARY,
    QUO_EXPR_UNARY,
    QUO_EXPR_GROUPING,
    QUO_EXPR_CALL,
    QUO_EXPR_ASSIGN,
    QUO_EXPR_TERNARY,
    QUO_EXPR_MEMBER_ACCESS,
  } type;
  QuoToken token;
  union {
    struct {
      QuoExpr *left, *right;
      QuoToken op;
    } binary;
    struct {
      QuoExpr *expr;
      QuoToken op;
    } unary;
    struct {
      QuoExpr *callee;
      QuoExprList arguments;
    } call;
    struct {
      QuoExpr *target, *value;
    } assign;
    struct {
      QuoExpr *condition, *then_expr, *else_expr;
    } ternary;
    struct {
      QuoExpr *object;
      QuoToken member;
    } member_access;
    struct {
      QuoExprList elements;
      bool trailing_comma;
    } array;
    struct {
      da(QuoExprDictPair) pairs;
      bool trailing_comma;
    } dict;
    struct {
      QuoToken name;
      QuoTokenList parameters;
      QuoStmt *body;
    } function;
  };
};

// Statement
struct QuoStmt {
  enum QuoStmtType {
    QUO_STMT_VAR_DECL,
    QUO_STMT_EXPRESSION,
    QUO_STMT_RETURN,
    QUO_STMT_IF,
    QUO_STMT_LOOP,
    QUO_STMT_BREAK,
    QUO_STMT_CONTINUE,
    QUO_STMT_BLOCK,
    QUO_STMT_IMPORT,
  } type;
  union {
    struct {
      QuoToken name;
      QuoExpr *initializer; // Can be NULL
    } var_decl;             // Variable declaration
    struct {
      QuoExpr *condition;
      QuoStmt *then_branch;
      QuoStmt *else_branch; // Can be NULL or another if for elif
    } if_stmt;
    struct {
      QuoStmt *initializer;
      QuoExpr *condition;
      QuoStmt *increment;
      QuoStmt *body;
    } loop; // Loop
    QuoModule *import;
    QuoStmtList block;   // Block statement (multiple statements)
    QuoExpr *expression; // QuoExpr statement or return statement expression
  };
};

// QuoVar is a tagged union representing the value of a variable.
typedef struct QuoVar {
  enum QuoVarType {
    QUO_VAR_TYPE_NIL,
    QUO_VAR_TYPE_BOOL,
    QUO_VAR_TYPE_NUM,
    QUO_VAR_TYPE_ERROR,
    QUO_VAR_TYPE_OBJ,
  } type;
  union {
    double val_num;
    const char *val_err;
    struct QuoObj *val_obj;
    void *val_ptr;
  };
} QuoVar;

#define quo_var_new_nil()     ((QuoVar){QUO_VAR_TYPE_NIL})
#define quo_var_new_bool(val) ((QuoVar){QUO_VAR_TYPE_BOOL, .val_num = (val) ? 1.0 : 0.0})
#define quo_var_new_num(val)  ((QuoVar){QUO_VAR_TYPE_NUM, .val_num = (double)(val)})
#define quo_var_new_err(msg)  ((QuoVar){QUO_VAR_TYPE_ERROR, .val_err = msg})
#define quo_var_new_obj(val)  ((QuoVar){QUO_VAR_TYPE_OBJ, .val_obj = (QuoObj *)val})

static inline bool quo_var_as_bool(QuoVar *v) { return v->val_num != 0.0; }
static inline double quo_var_as_num(QuoVar *v) { return v->val_num; }

static inline bool quo_var_is_bool(const QuoVar *v) { return v->type == QUO_VAR_TYPE_BOOL; }
static inline bool quo_var_is_num(const QuoVar *v) { return v->type == QUO_VAR_TYPE_NUM; }

void quo_var_ref(QuoVar *v);
void quo_var_unref(QuoVar *v);

#define QUO_DEFINE_USER_TYPE(TypeName, t_name)                                                                                             \
  static inline bool quo_obj_is_##t_name(QuoObj *obj) { return obj->type == QUO_OBJ_TYPE_USER && !strcmp(obj->name->data, #TypeName); }    \
  static inline TypeName *quo_obj_as_##t_name(QuoObj *obj) { return (TypeName *)obj; }                                                     \
  static inline TypeName *quo_var_as_##t_name(QuoVar *var) { return (TypeName *)var->val_obj; }

typedef enum {
  QUO_OBJ_TYPE_STR,
  QUO_OBJ_TYPE_ARR,
  QUO_OBJ_TYPE_DICT,
  QUO_OBJ_TYPE_MODULE,
  QUO_OBJ_TYPE_FN,
  QUO_OBJ_TYPE_CFN,
  QUO_OBJ_TYPE_USER,
} QuoObjType;

typedef struct QuoObj {
  QuoObjType type;      // Object type
  size_t size;          // Object size
  int ref_count;        // Reference count
  struct QuoStr *name;  // Name
  struct QuoDict *dict; // Methods/Properties dictionary
} QuoObj;

QuoObj *quo_obj_new(size_t size);
void quo_obj_ref(QuoObj *obj);
void quo_obj_unref(QuoObj *obj);

static inline bool quo_var_is_obj(const QuoVar *v) { return v->type == QUO_VAR_TYPE_OBJ; }
static inline bool quo_obj_is_str(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_STR; }
static inline bool quo_obj_is_dict(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_DICT; }
static inline bool quo_obj_is_fn(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_FN; }
static inline bool quo_obj_is_cfn(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_CFN; }

typedef struct QuoStr {
  QuoObj obj;
  char *data;            // NULL-terminated string
  unsigned int len;      // Byte length
  unsigned int char_len; // Character count (UTF-8)
  unsigned int hash;     // Hash
} QuoStr;

// Creates a new QuoStr from a C string. Process escape sequences. All strings are interned.
// Pass `len` as `-1` for NULL-terminated strings.
QuoStr *quo_str_new(QuoState *s, const char *str, int64_t len);
// Create string from raw data (no escape processing)
QuoStr *quo_str_new_raw(QuoState *s, const char *str, int64_t len);

static inline QuoStr *quo_obj_as_str(const QuoObj *o) { return (QuoStr *)o; }

typedef struct {
  QuoObj obj;
  da(QuoVar) arr;
} QuoArr;

QuoArr *quo_arr_new(void);
void quo_arr_push(QuoArr *arr, QuoVar value);
QuoVar quo_arr_pop(QuoArr *arr);
QuoVar quo_arr_get(QuoArr *arr, int64_t index);
void quo_arr_set(QuoArr *arr, int64_t index, QuoVar value);
int64_t quo_arr_len(QuoArr *arr);

static inline bool quo_obj_is_arr(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_ARR; }
static inline QuoArr *quo_obj_as_arr(const QuoObj *o) { return (QuoArr *)o; }

typedef struct QuoDict {
  QuoObj obj;
  QuoHashTable dict;
} QuoDict;

QuoDict *quo_dict_new();
QuoVar quo_dict_get(QuoDict *dict, QuoStr *key);
bool quo_dict_set(QuoDict *dict, QuoStr *key, QuoVar *value);

static inline QuoDict *quo_obj_as_dict(const QuoObj *o) { return (QuoDict *)o; }

typedef struct {
  QuoObj obj;
  QuoStr *name;
  int arity; // -1 for variadic
  da(uint64_t) instructions;
  da(QuoVar) constants;
  QuoState *state;
} QuoFn;

QuoFn *quo_function_new(QuoState *s, const char *name, uint64_t name_len);

static inline QuoFn *quo_obj_as_fn(const QuoObj *o) { return (QuoFn *)o; }

typedef QuoVar (*QuoCFunctionPtr)(QuoState *s, int64_t argc, QuoVar *argv);
typedef struct {
  QuoObj obj;
  QuoStr *name;
  QuoCFunctionPtr fn;
} QuoCFn;

QuoCFn *quo_cfunction_new(QuoState *s, const char *name, int64_t name_len, QuoCFunctionPtr ptr);

static inline QuoCFn *quo_obj_as_cfn(const QuoObj *o) { return (QuoCFn *)o; }

typedef struct QuoModule {
  QuoObj obj;
  QuoStr *name;
  QuoState *state;  // Module's own isolated state
  bool initialized; // Whether the module has been run
} QuoModule;

// Add helper functions
QuoModule *quo_module_new(QuoState *s, const char *name, int64_t name_len);
void quo_module_add_export(QuoModule *module, QuoStr *name, QuoVar value);

static inline bool quo_obj_is_module(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_MODULE; }
static inline QuoModule *quo_obj_as_module(const QuoObj *o) { return (QuoModule *)o; }
static inline bool quo_var_is_module(const QuoVar *v) { return quo_var_is_obj(v) && v->val_obj->type == QUO_OBJ_TYPE_MODULE; }
static inline QuoModule *quo_var_as_module(const QuoVar *v) { return quo_obj_as_module(v->val_obj); }

typedef void (*QuoModuleCleanupFn)(QuoState *);
typedef bool (*QuoModuleInitFn)(QuoState *);

// Quo runtime state
typedef struct QuoState {
  da(QuoModuleCleanupFn) modules_cleanup_fns;
  // User-defined types registry
  QuoHashTable types;
  // String interning
  QuoHashTable string_table;
  // Method tables
  QuoHashTable bool_methods;
  QuoHashTable num_methods;
  QuoHashTable str_methods;
  QuoHashTable arr_methods;
  QuoHashTable dict_methods;
  // Globals
  QuoHashTable globals;
  // Errors
  bool had_compile_error;
  char *cwd; // Base directory for resolving imports
} QuoState;

struct QuoParser {
  QuoState *s;
  // Lexer
  int pos, line, column;
  // Current file
  char *file_name;
  char *file_path;
  char *source;
  // Token state
  QuoToken current;
  QuoToken previous;
  // Scope tracking
  da(QuoTokenList) scopes;
  uint64_t loop_count;
  // Output
  QuoAST ast;
};

typedef struct {
  QuoToken name;
  int depth;
} QuoLocalVariable;

typedef struct QuoCompiler {
  QuoState *s;
  // Output function
  QuoFn *fn;
  // Locals tracking
  da(QuoLocalVariable) locals;
  QuoTokenList declared_globals;
  uint64_t scope_depth;
  // Loop context for break/continue
  struct QuoLoopContext {
    uint64_t start;
    uint64_t increment_start;
    da(uint64_t) breaks;
    da(uint64_t) continues;
  } *loop;
} QuoCompiler;

typedef struct QuoVM {
  QuoState *s;
  da(QuoVar) stack;
  da(struct QuoCallFrame {
    QuoFn *function;
    uint64_t *ip;
    uint64_t slots_start;
  }) frames;
} QuoVM;

// --- STATE --- //

// Create a new Quo state with the given base directory for resolving imports.
QuoState *quo_state_new(const char *cwd);
// Register a module with the given init and cleanup functions.
void quo_state_register_module(QuoState *s, QuoModuleInitFn init_fn, QuoModuleCleanupFn cleanup_fn);
// Register a global C function for accessing in quo.
// `name_len` is the length of `name`. Pass `-1` for NULL-terminated strings.
void quo_state_register_cfn(QuoState *s, const char *name, int64_t name_len, QuoCFunctionPtr fn);
// Register a type for use in quo.
// `name_len` is the length of `name`. Pass `-1` for NULL-terminated strings.
QuoObj *quo_state_register_type(QuoState *s, const char *name, int64_t name_len, size_t size);
QuoObj *quo_state_get_type_instance(QuoState *s, const char *name, int64_t name_len);
// Add a method/field to a type registered by `quo_state_register_type()`.
void quo_state_type_add(QuoObj *type, QuoStr *name, QuoVar value);
// Convenience function for adding a C function to a type.
void quo_state_type_add_cfn(QuoState *s, QuoObj *type, const char *name, int64_t name_len, QuoCFunctionPtr fn);
// Register a namespace for organizing C functions.
// It will create global dictionary e. g. `var my_namespace = { "foo": <cfn foo>, "bar": 69 }`.
// You can then use `my_namespace.foo()` and `my_namespace.bar` like any dictionary to access fields.
QuoDict *quo_state_register_namespace(QuoState *s, const char *name);
// Add a value to a namespace created by `quo_state_register_namespace()`.
bool quo_state_namespace_add(QuoState *s, QuoDict *ns, QuoStr *name, QuoVar value);
// Convenience function for adding a C function to a namespace.
// Add a C function to a namespace created by `quo_state_register_namespace()`.
void quo_state_namespace_add_cfn(QuoState *s, QuoDict *ns, const char *fn_name, QuoCFunctionPtr fn);
// Free the state and all associated resources.
void quo_state_free(QuoState *s);
// Parse
QuoParser *quo_parser_new(QuoState *s, const char *path);
bool quo_parser_parse(QuoParser *p);
void quo_parser_free(QuoParser *p);
// Compile
QuoCompiler *quo_compiler_new(QuoState *s, const char *name, uint64_t name_len);
QuoFn *quo_compiler_compile(QuoCompiler *c, QuoAST ast);
void quo_compiler_free(QuoCompiler *c);
// Execute
QuoVM *quo_vm_new(QuoState *s);
QuoVar quo_vm_run(QuoVM *vm, QuoFn *fn);
void quo_vm_free(QuoVM *vm);

// --- VARIABLE TYPES --- //

static inline const char *quo_var_as_err(QuoVar *v) { return v->val_err; }
static inline QuoObj *quo_var_as_obj(const QuoVar *v) { return v->val_obj; }
static inline QuoStr *quo_var_as_str(const QuoVar *v) { return quo_obj_as_str(v->val_obj); }
static inline QuoArr *quo_var_as_arr(const QuoVar *v) { return quo_obj_as_arr(v->val_obj); }
static inline QuoDict *quo_var_as_dict(const QuoVar *v) { return quo_obj_as_dict(v->val_obj); }
static inline QuoFn *quo_var_as_fn(const QuoVar *v) { return quo_obj_as_fn(v->val_obj); }
static inline QuoCFn *quo_var_as_cfn(const QuoVar *v) { return quo_obj_as_cfn(v->val_obj); }

static inline bool quo_var_is_nil(const QuoVar *v) { return v->type == QUO_VAR_TYPE_NIL; }
static inline bool quo_var_is_err(const QuoVar *v) { return v->type == QUO_VAR_TYPE_ERROR; }
static inline bool quo_var_is_str(const QuoVar *v) { return quo_var_is_obj(v) && quo_var_as_obj(v)->type == QUO_OBJ_TYPE_STR; }
static inline bool quo_var_is_arr(const QuoVar *v) { return quo_var_is_obj(v) && v->val_obj->type == QUO_OBJ_TYPE_ARR; }
static inline bool quo_var_is_dict(const QuoVar *v) { return quo_var_is_obj(v) && v->val_obj->type == QUO_OBJ_TYPE_DICT; }
static inline bool quo_var_is_fn(const QuoVar *v) { return quo_var_is_obj(v) && v->val_obj->type == QUO_OBJ_TYPE_FN; }
static inline bool quo_var_is_cfn(const QuoVar *v) { return quo_var_is_obj(v) && v->val_obj->type == QUO_OBJ_TYPE_CFN; }

static inline bool quo_var_is_true(const QuoVar *v) {
  return !quo_var_is_nil(v) &&
         ((quo_var_is_bool(v) && v->val_num) || (quo_var_is_num(v) && v->val_num > 0) ||
          (quo_var_is_str(v) && quo_var_as_str(v)->len > 0) || (quo_var_is_arr(v) && quo_arr_len(quo_obj_as_arr(v->val_obj)) > 0) ||
          (quo_var_is_dict(v) && quo_obj_as_dict(v->val_obj)->dict.count > 0));
}

QuoVar quo_var_to_bool(QuoVar *v);
QuoVar quo_var_to_num(QuoVar *v);
QuoVar quo_var_to_str(QuoState *s, QuoVar *v);

QuoVar quo_var_add(QuoState *s, QuoVar *a, QuoVar *b);
QuoVar quo_var_mul(QuoState *s, QuoVar *a, QuoVar *b);
QuoVar quo_var_sub(QuoVar *a, QuoVar *b);
QuoVar quo_var_div(QuoVar *a, QuoVar *b);

bool quo_var_eq(QuoState *s, QuoVar *a, QuoVar *b);
int64_t quo_var_cmp(QuoVar *a, QuoVar *b);
QuoVar quo_var_neg(QuoVar *v);
QuoVar quo_var_not(QuoVar *v);

uint64_t quo_var_len(QuoVar *v);

int64_t quo_var_print(QuoVar *v);

// ------------------------------------------------------------------------------------------ //
//                                     QUO IMPLEMENTATION                                     //
// ------------------------------------------------------------------------------------------ //

#ifdef QUO_IMPLEMENTATION

// Operations codes
typedef enum {
  QUO_OP_NOOP,
  QUO_OP_RETURN,
  QUO_OP_POP,
  QUO_OP_CONSTANT,
  QUO_OP_ARRAY,
  QUO_OP_DICT, // Create dictionary
  QUO_OP_MEMBER_ACCESS,
  QUO_OP_SET_MEMBER,
  QUO_OP_NEGATE,
  QUO_OP_NOT,
  QUO_OP_ADD,
  QUO_OP_SUB,
  QUO_OP_MUL,
  QUO_OP_DIV,
  QUO_OP_MOD,
  QUO_OP_EQ,
  QUO_OP_NEQ,
  QUO_OP_GT,
  QUO_OP_LT,
  QUO_OP_GTEQ,
  QUO_OP_LTEQ,
  QUO_OP_SET_LOCAL,
  QUO_OP_GET_LOCAL,
  QUO_OP_GET_GLOBAL,
  QUO_OP_SET_GLOBAL,
  QUO_OP_CALL,
  QUO_OP_METHOD_CALL,
  QUO_OP_JUMP,
  QUO_OP_JUMP_IF_FALSE,
  QUO_OP_LOOP,
} QuoOP;

// --- UTILS --- //

uint64_t quo_hash(const char *str, uint64_t len);
char *quo_strdup(const char *str);
char *quo_strdupf(const char *fmt, ...);
char *quo_strndup(const char *str, uint64_t len);
// Converts a string to an integer.
// Examples:
//    quo_strtoll("123", 3) -> 123
//    quo_strtoll("12_345_678", 10) -> 12345678
int64_t quo_strtoll(const char *s, uint64_t len);
// Converts a string to a double.
// Examples:
//    quo_strtod("123.45", 6) -> 123.45
//    quo_strtod("123_456.45", 9) -> 123456.45
double quo_strtod(const char *s, uint64_t len);
// Reads the contents of a file into a string.
char *quo_read_file(const char *path);
// Writes the contents of a string to a file.
bool quo_write_file(const char *path, const char *content);

// --- HASH TABLE --- //

void quo_ht_init(QuoHashTable *t);
bool quo_ht_set(QuoHashTable *t, QuoStr *key, QuoVar *value);
bool quo_ht_get(QuoHashTable *t, QuoStr *key, QuoVar *value);
bool quo_ht_del(QuoHashTable *t, QuoStr *key);
void quo_ht_free(QuoHashTable *t);

// --- LEXER --- //

// Format string for QuoToken, used with QUO_TOKEN_ARG macro:
// Example: printf(QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(token))
#define QUO_TOKEN_FMT    "%.*s"
#define QUO_TOKEN_ARG(t) (t).len, (t).start

bool quo_tokens_eq(QuoToken t1, QuoToken t2);

// --- DEBUG --- //

void quo_debug_expression_print(QuoExpr *expr, int indent);
void quo_debug_statement_print(QuoStmt *stmt, int indent);
void quo_debug_ast_print(QuoAST *ast);
void quo_debug_function_disassemble(QuoFn *fn);
static const char *quo__debug_op_str(QuoOP op);
int quo_debug_instruction_disassemble(QuoFn *fn, int offset);

// -------------------- MEMORY -------------------- //

void *quo_alloc(void *ptr, uint64_t size) {
  bool memset_needed = ptr == NULL;
  ptr = realloc(ptr, size);
  if (ptr == NULL) {
    fprintf(stderr, "quo_alloc: failed to allocate %lu bytes\n", size);
    exit(EXIT_FAILURE);
  }
  if (memset_needed) memset(ptr, 0, size);
  return ptr;
}

void quo_dealloc(void *ptr) {
  if (ptr == NULL) return;
  free(ptr);
  ptr = NULL;
}

// -------------------- UTILS -------------------- //

#define quo__static_array_size(arr) (sizeof(arr) / sizeof(arr[0]))

char *quo_strdup(const char *str) { return quo_strndup(str, strlen(str)); }

char *quo_strndup(const char *str, uint64_t len) {
  char *ptr = quo_alloc(NULL, len + 1);
  memcpy(ptr, str, len);
  ptr[len] = '\0';
  return ptr;
}

char *quo_strdupf(const char *fmt, ...) {
  if (fmt == NULL) return NULL;
  va_list ap;
  va_list ap2;
  // Determine required length (+1 for NULL)
  va_start(ap, fmt);
  va_copy(ap2, ap);
  int needed = vsnprintf(NULL, 0, fmt, ap2);
  va_end(ap2);
  if (needed < 0) {
    va_end(ap);
    return NULL;
  }
  char *buf = (char *)quo_alloc(NULL, (size_t)needed + 1);
  if (!buf) {
    va_end(ap);
    return NULL;
  }
  // Write into buffer
  if (vsnprintf(buf, (size_t)needed + 1, fmt, ap) < 0) {
    quo_dealloc(buf);
    buf = NULL;
  }
  va_end(ap);
  return buf;
}

int64_t quo_strtoll(const char *s, uint64_t len) {
  char buffer[len + 1];
  buffer[0] = '\0';
  for (int i = 0; i < len; i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') strncat(buffer, &c, 1);
  }
  return strtoll(buffer, NULL, 10);
}

double quo_strtod(const char *s, uint64_t len) {
  char buffer[len + 1];
  buffer[0] = '\0';
  bool has_dot = false;
  for (int i = 0; i < len; i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') strncat(buffer, &c, 1);
    else if (c == '.') {
      if (has_dot) break;
      strncat(buffer, &c, 1);
      has_dot = true;
    }
  }
  return strtod(buffer, NULL);
}

// Get UTF-8 character length
static uint64_t quo__utf8_char_len(unsigned char c) {
  if (c < 0x80) return 1; // 0xxxxxxx → 1 byte (ASCII)
  if (c < 0xC0) return 0; // 10xxxxxx → continuation byte (invalid as first byte)
  if (c < 0xE0) return 2; // 110xxxxx → 2 bytes
  if (c < 0xF0) return 3; // 1110xxxx → 3 bytes
  if (c < 0xF8) return 4; // 11110xxx → 4 bytes
  return 0;               // Invalid
}

// Get UTF-8 character count
static int64_t quo__utf8_strlen(const char *str, uint64_t byte_len) {
  uint64_t char_count = 0;
  const char *a = str;
  const char *end = a + byte_len;
  while (a < end) {
    uint64_t clen = quo__utf8_char_len((unsigned char)*a);
    if (clen == 0) clen = 1; // Skip invalid bytes
    a += clen;
    char_count++;
  }
  return char_count;
}

// Get the Nth UTF-8 character from a string
static const char *quo__utf8_index(const char *str, uint64_t len, int64_t index) {
  int64_t current = 0;
  const char *a = str;
  const char *end = str + len;
  while (a < end && current < index) {
    uint64_t char_len = quo__utf8_char_len((unsigned char)*a);
    if (char_len == 0) char_len = 1; // Skip invalid bytes
    a += char_len;
    current++;
  }
  if (current != index || a >= end) return NULL;
  return a;
}

char *quo_read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) return NULL;
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);
  char *buffer = (char *)quo_alloc(NULL, fileSize + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead != fileSize) {
    quo_dealloc(buffer);
    fclose(file);
    return NULL;
  }
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

bool quo_write_file(const char *path, const char *content) {
  if (!path || !content) return false;
  FILE *file = fopen(path, "wb");
  if (!file) return false;
  fwrite(content, sizeof(char), strlen(content), file);
  fclose(file);
  return true;
}

static char *quo_file_name(const char *path) {
  if (!path) return NULL;
  // Remove the directory path from the file name
  const char *last_slash = NULL;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\') last_slash = p;
  // If no slash found, use the entire path as filename
  const char *filename = last_slash ? last_slash + 1 : path;
  // Remove extension - only look for dots in the filename part
  const char *last_dot = NULL;
  for (const char *p = filename; *p; p++)
    if (*p == '.') last_dot = p;
  if (!last_dot) return quo_strdup(filename);
  return quo_strndup(filename, last_dot - filename);
}

// Extracts the directory path from a file path
// e.g., "path/to/script.quo" -> "path/to/"
static char *quo_dirname(const char *path) {
  if (!path) return quo_strdup("./");
  // Find the last '/' or '\'
  const char *last_slash = NULL;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\') last_slash = p;
  if (!last_slash) return quo_strdup("./"); // No directory separator, return current directory
  // Include the separator in the result
  return quo_strndup(path, last_slash - path + 1);
}

typedef enum { QUO_ERROR, QUO_WARNING } QuoErrorLevel;

static inline void quo__parser_error(QuoParser *p, QuoToken t, const char *fmt, ...) {
  p->s->had_compile_error = true;
  fprintf(stderr, "\033[0;31m"); // Red
  fprintf(stderr, "%s:%d:%d: Parse error: ", p->file_path ? p->file_path : "<input>", t.line, t.column);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\033[0m\n");
}

// ----------------- HASH TABLE ----------------- //

typedef struct QuoHashTableEntry {
  QuoStr *key;
  QuoVar value;
} QuoHashTableEntry;

static QuoHashTableEntry *quo__ht_find_entry(QuoHashTableEntry *entries, uint64_t capacity, QuoStr *key) {
  uint64_t index = key->hash & (capacity - 1);
  QuoHashTableEntry *tombstone = NULL;
  for (;;) {
    QuoHashTableEntry *entry = &entries[index];
    if (entry->key == NULL) {
      if (quo_var_is_nil(&entry->value)) {
        return tombstone != NULL ? tombstone : entry; // Empty entry
      } else {
        if (tombstone == NULL) tombstone = entry; // We found a tombstone
      }
    } else if (entry->key == key || entry->key->hash == key->hash || strcmp(entry->key->data, key->data) == 0) {
      return entry; // We found the key
    }
    index = (index + 1) & (capacity - 1);
  }
}

static void quo__ht_adjust_capacity(QuoHashTable *t, uint64_t capacity) {
  if (t->capacity == 0) capacity = 8;
  QuoHashTableEntry *entries = quo_alloc(NULL, sizeof(*entries) * capacity);
  t->count = 0;
  for (int i = 0; i < t->capacity; i++) {
    QuoHashTableEntry *entry = &t->items[i];
    if (entry->key == NULL) continue;
    QuoHashTableEntry *dest = quo__ht_find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    t->count++;
  }
  quo_dealloc(t->items);
  t->items = entries;
  t->capacity = capacity;
}

unsigned long quo_hash(const char *str, uint64_t len) {
  unsigned long hash = 5381;
  int c;
  while (len--) {
    c = *str++;
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

void quo_ht_init(QuoHashTable *t) {
  t->count = t->capacity = 0;
  t->items = NULL;
}

bool quo_ht_set(QuoHashTable *t, QuoStr *key, QuoVar *value) {
  if (t->count + 1 > t->capacity * 0.75) quo__ht_adjust_capacity(t, t->capacity * 2);
  QuoHashTableEntry *entry = quo__ht_find_entry(t->items, t->capacity, key);
  bool new = entry->key == NULL;
  if (new && quo_var_is_nil(&entry->value)) t->count++;
  entry->key = key;
  entry->value = *value;
  return new;
}

bool quo_ht_get(QuoHashTable *t, QuoStr *key, QuoVar *value) {
  if (t->count == 0) return false;
  QuoHashTableEntry *entry = quo__ht_find_entry(t->items, t->capacity, key);
  if (entry->key == NULL) return false;
  *value = entry->value;
  return true;
}

bool quo_ht_del(QuoHashTable *t, QuoStr *key) {
  if (t->count == 0) return false;
  QuoHashTableEntry *entry = quo__ht_find_entry(t->items, t->capacity, key);
  if (entry->key == NULL) return false;
  // Place a tombstone
  entry->key = NULL;
  entry->value = quo_var_new_bool(true);
  return true;
}

void quo_ht_free(QuoHashTable *t) {
  for (uint64_t i = 0; i < t->capacity; i++)
    if (t->items[i].key) quo_var_unref(&t->items[i].value);
  da_free(t);
}

// -------------------- LEXER -------------------- //

static enum QuoTokenType quo__keywords[] = {
    QUO_TT_VAR,  QUO_TT_FN,    QUO_TT_LOOP, QUO_TT_BREAK, QUO_TT_CONTINUE, QUO_TT_IF,     QUO_TT_ELSE,
    QUO_TT_TRUE, QUO_TT_FALSE, QUO_TT_NIL,  QUO_TT_AND,   QUO_TT_OR,       QUO_TT_RETURN, QUO_TT_IMPORT,
};
static enum QuoTokenType quo__single_char_symbols[] = {
    QUO_TT_DOT,   QUO_TT_OPAREN, QUO_TT_CPAREN, QUO_TT_OBRACE, QUO_TT_CBRACE,   QUO_TT_OBRACKET, QUO_TT_CBRACKET,
    QUO_TT_COMMA, QUO_TT_COLON,  QUO_TT_EQ,     QUO_TT_LT,     QUO_TT_GT,       QUO_TT_PLUS,     QUO_TT_MINUS,
    QUO_TT_STAR,  QUO_TT_SLASH,  QUO_TT_BANG,   QUO_TT_MOD,    QUO_TT_QUESTION,
};
static enum QuoTokenType quo__compound_symbols[] = {
    QUO_TT_BANGEQ, QUO_TT_DOUBLEEQ, QUO_TT_GTEQ, QUO_TT_LTEQ, QUO_TT_PLUSEQ, QUO_TT_MINUSEQ, QUO_TT_MULEQ, QUO_TT_DIVEQ,
};

static_assert(quo__static_array_size(quo__keywords) == 14, "quo__keywords size mismatch");
static_assert(quo__static_array_size(quo__single_char_symbols) == 19, "quo__single_char_symbols size mismatch");
static_assert(quo__static_array_size(quo__compound_symbols) == 8, "quo__compound_symbols size mismatch");

bool quo__is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
bool lexer__is_digit(char c) { return c >= '0' && c <= '9'; }
bool lexer__is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool lexer__is_alphanumeric(char c) { return lexer__is_alpha(c) || lexer__is_digit(c); }
char lexer__peek(QuoParser *p, int offset) { return p->source[p->pos + offset]; }
void lexer__advance(QuoParser *p) {
  p->pos++;
  if (lexer__peek(p, 0) == '\n') p->line++, p->column = 1;
  else p->column++;
}

QuoToken quo_parser_next_token(QuoParser *p) {
  QuoToken t = {.type = QUO_TT_EOF, .line = p->line, .column = p->column};
  while (lexer__peek(p, 0) != '\0') {
    // Skip whitespace
    if (quo__is_space(lexer__peek(p, 0))) lexer__advance(p);
    // Line comments
    else if (!strncmp(p->source + p->pos, quo_token_type_str(QUO_TT_COMMENT), strlen(quo_token_type_str(QUO_TT_COMMENT)))) {
      while (lexer__peek(p, 0) != '\n' && lexer__peek(p, 0) != '\0') lexer__advance(p);
      continue;
    }
    // Identifier or keyword
    else if (lexer__is_alpha(lexer__peek(p, 0))) {
      t.type = QUO_TT_ID;
      size_t start = p->pos;
      while (lexer__is_alphanumeric(lexer__peek(p, 0)) || lexer__peek(p, 0) == '_') lexer__advance(p);
      t.start = p->source + start;
      t.len = p->pos - start;
      // Find keywords
      for (int i = 0; i < quo__static_array_size(quo__keywords); ++i) {
        size_t len = strlen(quo_token_type_str(quo__keywords[i]));
        if (len == t.len && memcmp(t.start, quo_token_type_str(quo__keywords[i]), len) == 0) {
          t.type = quo__keywords[i];
          break;
        }
      }
      break;
    }
    // Number
    else if (lexer__is_digit(lexer__peek(p, 0))) {
      t.type = QUO_TT_LITERAL_NUM;
      size_t start = p->pos;
      t.start = p->source + start;
      // Integer part
      while (lexer__is_digit(lexer__peek(p, 0)) || lexer__peek(p, 0) == '_') lexer__advance(p);
      // Float part
      if (lexer__peek(p, 0) == '.' && lexer__peek(p, 1) != '\0' && lexer__is_digit(lexer__peek(p, 1))) {
        lexer__advance(p); // Skip '.'
        while (lexer__is_digit(lexer__peek(p, 0)) || lexer__peek(p, 0) == '_') lexer__advance(p);
      }
      t.len = p->pos - start;
      break;
    }
    // String
    else if (lexer__peek(p, 0) == '"') {
      t.type = QUO_TT_LITERAL_STR;
      lexer__advance(p); // Skip '"'
      size_t start = p->pos;
      t.start = p->source + start;
      while (lexer__peek(p, 0) != '\0') {
        if (lexer__peek(p, 0) == '"') {
          if (lexer__peek(p, -1) == '\\') {
            lexer__advance(p);
            continue;
          }
          t.len = p->pos - start;
          break;
        }
        lexer__advance(p);
      }
      if (lexer__peek(p, 0) == '"') lexer__advance(p);
      else t.type = QUO_TT_ERROR, t.error_msg = "Unterminated string";
      break;
    }
    // Symbol
    else if (!lexer__is_alphanumeric(lexer__peek(p, 0))) {
      // Check for compound symbols first
      if (lexer__peek(p, 1) != '\0') {
        char two_char[3] = {lexer__peek(p, 0), lexer__peek(p, 1), '\0'};
        for (int64_t i = 0; i < quo__static_array_size(quo__compound_symbols); ++i)
          if (!strcmp(two_char, quo_token_type_str(quo__compound_symbols[i]))) {
            t.type = quo__compound_symbols[i];
            t.start = p->source + p->pos;
            t.len = 2;
            lexer__advance(p);
            lexer__advance(p);
            break;
          }
      }
      // If not a two-char symbol, check single-char symbols
      if (t.type == QUO_TT_EOF) { // Only if we haven't matched a compound symbol
        char single_char[2] = {lexer__peek(p, 0), '\0'};
        for (int64_t i = 0; i < quo__static_array_size(quo__single_char_symbols); ++i)
          if (!strcmp(single_char, quo_token_type_str(quo__single_char_symbols[i]))) {
            t.type = quo__single_char_symbols[i];
            t.start = p->source + p->pos;
            t.len = 1;
            lexer__advance(p);
            break;
          }
      }
      break;
    }
    // Unknown token
    else {
      t.type = QUO_TT_ERROR;
      t.error_msg = "Unknown token";
    }
  }
  return t;
}

bool quo_tokens_eq(QuoToken t1, QuoToken t2) { return t1.len != t2.len ? false : memcmp(t1.start, t2.start, t1.len) == 0; }

const char *quo_token_type_str(enum QuoTokenType t) {
  const char *str = NULL;
  switch (t) {
  case QUO_TT_NONE:
  case QUO_TT_EOF:
  case QUO_TT_ERROR:
  case QUO_TT_ID:
  case QUO_TT_LITERAL_NUM:
  case QUO_TT_LITERAL_STR: break;
  case QUO_TT_COMMENT: str = "#"; break;
  case QUO_TT_VAR: str = "var"; break;
  case QUO_TT_FN: str = "fn"; break;
  case QUO_TT_LOOP: str = "loop"; break;
  case QUO_TT_BREAK: str = "break"; break;
  case QUO_TT_CONTINUE: str = "continue"; break;
  case QUO_TT_IF: str = "if"; break;
  case QUO_TT_ELSE: str = "else"; break;
  case QUO_TT_TRUE: str = "true"; break;
  case QUO_TT_FALSE: str = "false"; break;
  case QUO_TT_NIL: str = "nil"; break;
  case QUO_TT_AND: str = "and"; break;
  case QUO_TT_OR: str = "or"; break;
  case QUO_TT_RETURN: str = "return"; break;
  case QUO_TT_IMPORT: str = "import"; break;
  case QUO_TT_DOT: str = "."; break;
  case QUO_TT_OPAREN: str = "("; break;
  case QUO_TT_CPAREN: str = ")"; break;
  case QUO_TT_OBRACE: str = "{"; break;
  case QUO_TT_CBRACE: str = "}"; break;
  case QUO_TT_OBRACKET: str = "["; break;
  case QUO_TT_CBRACKET: str = "]"; break;
  case QUO_TT_COMMA: str = ","; break;
  case QUO_TT_COLON: str = ":"; break;
  case QUO_TT_EQ: str = "="; break;
  case QUO_TT_LT: str = "<"; break;
  case QUO_TT_GT: str = ">"; break;
  case QUO_TT_PLUS: str = "+"; break;
  case QUO_TT_MINUS: str = "-"; break;
  case QUO_TT_STAR: str = "*"; break;
  case QUO_TT_SLASH: str = "/"; break;
  case QUO_TT_BANG: str = "!"; break;
  case QUO_TT_MOD: str = "%"; break;
  case QUO_TT_QUESTION: str = "?"; break;
  case QUO_TT_BANGEQ: str = "!="; break;
  case QUO_TT_DIVEQ: str = "/="; break;
  case QUO_TT_DOUBLEEQ: str = "=="; break;
  case QUO_TT_GTEQ: str = ">="; break;
  case QUO_TT_LTEQ: str = "<="; break;
  case QUO_TT_MINUSEQ: str = "-="; break;
  case QUO_TT_MULEQ: str = "*="; break;
  case QUO_TT_PLUSEQ: str = "+="; break;
  }
  return str;
}

// ---------- VAR FUNCTIONS ---------- //

void quo_var_ref(QuoVar *v) {
  if (quo_var_is_obj(v)) quo_obj_ref(v->val_obj);
}

void quo_var_unref(QuoVar *v) {
  if (!quo_var_is_obj(v)) return;
  quo_obj_unref(v->val_obj);
}

// --- OBJECT FUNCTIONS --- //

QuoObj *quo_obj_new(size_t size) {
  QuoObj *obj = (QuoObj *)quo_alloc(NULL, size);
  obj->size = size;
  obj->ref_count = 1;
  return obj;
}

void quo_obj_ref(QuoObj *obj) {
  if (obj) obj->ref_count++;
}

void quo_obj_unref(QuoObj *obj) {
  obj->ref_count--;
  if (obj->ref_count > 0) return;
  switch (obj->type) {
  case QUO_OBJ_TYPE_ARR: {
    QuoArr *arr = quo_obj_as_arr(obj);
    for (uint64_t i = 0; i < da_count(&arr->arr); i++) quo_var_unref(&da_at(&arr->arr, i));
    da_free(&arr->arr);
    break;
  }
  case QUO_OBJ_TYPE_DICT: {
    QuoDict *dict = quo_obj_as_dict(obj);
    for (uint64_t i = 0; i < dict->dict.capacity; i++)
      if (dict->dict.items[i].key) quo_var_unref(&dict->dict.items[i].value);
    quo_ht_free(&dict->dict);
    break;
  }
  case QUO_OBJ_TYPE_FN: {
    QuoFn *fn = quo_obj_as_fn(obj);
    for (int i = 0; i < da_count(&fn->constants); i++) quo_var_unref(&da_at(&fn->constants, i));
    da_free(&fn->instructions);
    da_free(&fn->constants);
    break;
  }
  case QUO_OBJ_TYPE_MODULE: {
    QuoModule *module = quo_obj_as_module(obj);
    if (module->state) quo_state_free(module->state);
    break;
  }
  case QUO_OBJ_TYPE_CFN: break;
  case QUO_OBJ_TYPE_STR: break;
  case QUO_OBJ_TYPE_USER: break;
  }
  quo_dealloc(obj);
}

// --- STRING FUNCTIONS --- //

static inline QuoStr *quo__find_string(QuoHashTable *table, const char *chars, int length, unsigned int hash) {
  if (table->count == 0) return NULL;
  unsigned int index = hash & (table->capacity - 1);
  for (;;) {
    QuoHashTableEntry *entry = &table->items[index];
    if (entry->key == NULL) {
      if (quo_var_is_nil(&entry->value)) return NULL; // Stop if we find an empty non-tombstone entry
    } else if (entry->key->len == length && entry->key->hash == hash && memcmp(entry->key->data, chars, length) == 0) {
      return entry->key;
    }
    index = (index + 1) & (table->capacity - 1);
  }
}

QuoStr *quo_str_new(QuoState *s, const char *str, int64_t len) {
  if (len < 0) len = strlen(str);

  // Process escape sequences
  char *processed = quo_alloc(NULL, len + 1);
  uint64_t out_len = 0;
  for (uint64_t i = 0; i < len; i++) {
    if (str[i] == '\\' && i + 1 < len) {
      i++;
      switch (str[i]) {
      case 'n': processed[out_len++] = '\n'; break;
      case 't': processed[out_len++] = '\t'; break;
      case 'r': processed[out_len++] = '\r'; break;
      case '\\': processed[out_len++] = '\\'; break;
      case '"': processed[out_len++] = '"'; break;
      case '0': processed[out_len++] = '\0'; break;
      default:
        processed[out_len++] = '\\';
        processed[out_len++] = str[i];
        break;
      }
    } else processed[out_len++] = str[i];
  }
  processed[out_len] = '\0';

  uint64_t hash = quo_hash(processed, out_len);
  QuoStr *existing = quo__find_string(&s->string_table, processed, out_len, hash);
  if (existing) {
    quo_dealloc(processed);
    return existing;
  }

  // Create new string
  QuoStr *string = (QuoStr *)quo_obj_new(sizeof(QuoStr));
  string->obj.type = QUO_OBJ_TYPE_STR;
  string->data = processed;
  string->len = out_len;
  string->char_len = quo__utf8_strlen(processed, out_len);
  string->hash = hash;

  quo_ht_set(&s->string_table, string, &quo_var_new_nil());
  return string;
}

// Create string from raw data (no escape processing)
QuoStr *quo_str_new_raw(QuoState *s, const char *str, int64_t len) {
  if (len < 0) len = strlen(str);
  uint64_t hash = quo_hash(str, len);
  QuoStr *existing = quo__find_string(&s->string_table, str, len, hash);
  if (existing) return existing;
  // Create new string
  QuoStr *string = (QuoStr *)quo_obj_new(sizeof(QuoStr));
  string->obj.type = QUO_OBJ_TYPE_STR;
  string->data = quo_strndup(str, len);
  string->len = len;
  string->char_len = quo__utf8_strlen(str, len);
  string->hash = hash;
  quo_ht_set(&s->string_table, string, &quo_var_new_nil());
  return string;
}

// --- DICTIONARY FUNCTIONS --- //

QuoDict *quo_dict_new() {
  QuoDict *dict = (QuoDict *)quo_obj_new(sizeof(QuoDict));
  dict->obj.type = QUO_OBJ_TYPE_DICT;
  return dict;
}
QuoVar quo_dict_get(QuoDict *dict, QuoStr *key) {
  QuoVar value;
  if (!quo_ht_get(&dict->dict, key, &value)) return quo_var_new_nil();
  return value;
}
bool quo_dict_set(QuoDict *dict, QuoStr *key, QuoVar *value) {
  quo_var_ref(value);
  return quo_ht_set(&dict->dict, key, value);
}

// --- FN FUNCTIONS --- //

QuoFn *quo_function_new(QuoState *s, const char *name, uint64_t name_len) {
  QuoFn *fn = (QuoFn *)quo_obj_new(sizeof(QuoFn));
  fn->obj.type = QUO_OBJ_TYPE_FN;
  fn->name = quo_str_new(s, name, name_len);
  fn->arity = -1;
  fn->state = s;
  return fn;
}
static inline void quo__function_push_instruction(QuoFn *f, uint64_t instruction) { da_add(&f->instructions, instruction); }
static inline uint64_t quo__function_push_constant(QuoFn *f, QuoVar *constant) {
  // Check if constant already exists (for strings only)
  if (quo_var_is_str(constant)) {
    for (uint64_t i = 0; i < da_count(&f->constants); i++) {
      QuoVar *existing = &da_at(&f->constants, i);
      if (quo_var_is_str(existing) && existing->val_obj == constant->val_obj) {
        return i; // Return existing index
      }
    }
  }
  // Also check for function constants
  if (quo_var_is_fn(constant)) {
    for (uint64_t i = 0; i < da_count(&f->constants); i++) {
      QuoVar *existing = &da_at(&f->constants, i);
      if (quo_var_is_fn(existing) && existing->val_obj == constant->val_obj) return i; // Return existing index
    }
  }
  da_add(&f->constants, *constant);
  return da_count(&f->constants) - 1;
}
// Emit a jump instruction and return its position for later patching
static inline uint64_t quo__function_emit_jump(QuoFn *f, QuoOP op) {
  quo__function_push_instruction(f, op);
  quo__function_push_instruction(f, QUO_OP_NOOP); // Placeholder
  return da_count(&f->instructions) - 1;
}
// Patch a jump instruction to point to the current position
static inline void quo__function_patch_jump(QuoFn *f, uint64_t jump_pos) {
  uint64_t current_pos = da_count(&f->instructions);
  uint64_t offset = current_pos - jump_pos - 1;
  da_at(&f->instructions, jump_pos) = offset;
}
// Emit a loop jump instruction (backwards jump)
static inline void quo__function_emit_loop(QuoFn *f, uint64_t loop_start) {
  quo__function_push_instruction(f, QUO_OP_LOOP);
  uint64_t offset = da_count(&f->instructions) - loop_start + 1;
  quo__function_push_instruction(f, offset);
}

// Array functions
QuoArr *quo_arr_new(void) {
  QuoArr *arr = (QuoArr *)quo_obj_new(sizeof(QuoArr));
  arr->obj.type = QUO_OBJ_TYPE_ARR;
  return arr;
}
void quo_arr_push(QuoArr *arr, QuoVar value) {
  quo_var_ref(&value);
  da_add(&arr->arr, value);
}
QuoVar quo_arr_get(QuoArr *arr, int64_t index) {
  if (index < 0 || index >= da_count(&arr->arr)) return quo_var_new_nil();
  return da_at(&arr->arr, index);
}
QuoVar quo_arr_pop(QuoArr *arr) {
  if (da_count(&arr->arr) == 0) return quo_var_new_nil();
  return da_pop(&arr->arr);
}
void quo_arr_set(QuoArr *arr, int64_t index, QuoVar value) {
  if (index < 0) {
    if (da_count(&arr->arr) == 0) index = 0;
    else
      while (index < 0) index = da_count(&arr->arr) + index;
  }
  if (index >= da_count(&arr->arr)) da_grow(&arr->arr, index + 1);
  QuoVar *slot = &da_at(&arr->arr, index);
  quo_var_unref(slot); // Unref old value
  quo_var_ref(&value); // Ref new value
  *slot = value;
}
int64_t quo_arr_len(QuoArr *arr) { return da_count(&arr->arr); }

QuoCFn *quo_cfunction_new(QuoState *s, const char *name, int64_t name_len, QuoCFunctionPtr ptr) {
  QuoCFn *fn = (QuoCFn *)quo_obj_new(sizeof(QuoCFn));
  fn->obj.type = QUO_OBJ_TYPE_CFN;
  fn->obj.name = quo_str_new(s, "cfn", -1);
  fn->name = quo_str_new(s, name, name_len);
  fn->fn = ptr;
  return fn;
}

QuoModule *quo_module_new(QuoState *s, const char *name, int64_t name_len) {
  QuoModule *module = (QuoModule *)quo_obj_new(sizeof(QuoModule));
  module->obj.type = QUO_OBJ_TYPE_MODULE;
  module->name = quo_str_new(s, name, name_len);
  module->state = quo_state_new(s->cwd);
  module->initialized = false;
  return module;
}

// --- CONVERSION FUNCTIONS --- //

QuoVar quo_var_to_bool(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_ERROR:
  case QUO_VAR_TYPE_NIL: return quo_var_new_bool(false);
  case QUO_VAR_TYPE_BOOL: return *v;
  case QUO_VAR_TYPE_NUM: return quo_var_new_bool(v->val_num != 0);
  case QUO_VAR_TYPE_OBJ:
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return quo_var_new_bool(quo_obj_as_str(v->val_obj)->len > 0);
    case QUO_OBJ_TYPE_ARR: return quo_var_new_bool(quo_obj_as_arr(v->val_obj)->arr.count > 0);
    case QUO_OBJ_TYPE_DICT: return quo_var_new_bool(quo_obj_as_dict(v->val_obj)->dict.count > 0);
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_CFN: return quo_var_new_bool(false);
    }
  }
}

QuoVar quo_var_to_num(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_ERROR:
  case QUO_VAR_TYPE_NIL: return quo_var_new_num(0.0);
  case QUO_VAR_TYPE_BOOL:
  case QUO_VAR_TYPE_NUM: return *v;
  case QUO_VAR_TYPE_OBJ:
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return quo_var_new_num(quo_strtod(quo_obj_as_str(v->val_obj)->data, quo_obj_as_str(v->val_obj)->len));
    case QUO_OBJ_TYPE_ARR:
    case QUO_OBJ_TYPE_DICT:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_CFN: return quo_var_new_num(0.0);
    }
  }
}

QuoVar quo_var_to_str(QuoState *s, QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_ERROR:
  case QUO_VAR_TYPE_NIL: return quo_var_new_obj(quo_str_new(s, "", 0));
  case QUO_VAR_TYPE_BOOL: return quo_var_new_obj(quo_str_new(s, v->val_num ? "true" : "false", -1));
  case QUO_VAR_TYPE_NUM: {
    char buf[318];
    snprintf(buf, sizeof(buf), "%g", v->val_num);
    return quo_var_new_obj(quo_str_new(s, buf, -1));
  }
  case QUO_VAR_TYPE_OBJ:
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return *v;
    case QUO_OBJ_TYPE_ARR:
    case QUO_OBJ_TYPE_DICT:
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_CFN: return quo_var_new_obj(quo_str_new(s, "", 0));
    }
  }
}

// --- MATH FUNCTIONS --- //

QuoVar quo_var_add(QuoState *s, QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) { return quo_var_new_num(a->val_num + b->val_num); }
  if (quo_var_is_str(a) || quo_var_is_str(b)) {
    const QuoVar str_a = quo_var_to_str(s, a);
    const QuoVar str_b = quo_var_to_str(s, b);
    char *str = quo_strdupf("%s%s", quo_obj_as_str(str_a.val_obj)->data, quo_obj_as_str(str_b.val_obj)->data);
    QuoStr *res = quo_str_new(s, str, -1);
    quo_dealloc(str);
    return quo_var_new_obj(res);
  }
  if (quo_var_is_arr(a) && quo_var_is_arr(b)) {
    QuoArr *a_arr = quo_obj_as_arr(a->val_obj);
    QuoArr *b_arr = quo_obj_as_arr(b->val_obj);
    for (uint64_t i = 0; i < quo_arr_len(b_arr); i++) quo_arr_push(a_arr, quo_arr_get(b_arr, i));
    return *a;
  }
  return quo_var_new_err("Types don't support addition");
}

QuoVar quo_var_sub(QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) return quo_var_new_num(a->val_num - b->val_num);
  return quo_var_new_err("Types don't support subtraction");
}

QuoVar quo_var_mul(QuoState *s, QuoVar *a, QuoVar *b) {
  // Numeric multiplication
  if (quo_var_is_num(a) && quo_var_is_num(b)) return quo_var_new_num(a->val_num * b->val_num);
  // String repetition: "foo" * 3 -> "foofoofoo"
  if ((quo_var_is_str(a) && quo_var_is_num(b)) || (quo_var_is_num(a) && quo_var_is_str(b))) {
    QuoVar *num_var = quo_var_is_num(a) ? a : b;
    QuoVar *str_var = quo_var_is_str(a) ? a : b;
    if (num_var->val_num <= 0) return quo_var_new_obj(quo_str_new(s, "", 0));
    QuoStr *str = quo_var_as_str(str_var);
    uint64_t len = str->len * num_var->val_num;
    char *data = quo_alloc(NULL, len + 1);
    for (int64_t i = 0; i < (int64_t)num_var->val_num; i++) memcpy(data + (i * str->len), str->data, str->len);
    data[len] = '\0';
    QuoStr *string = quo_str_new(s, data, -1);
    quo_dealloc(data);
    return quo_var_new_obj(string);
  }
  // Invalid operation
  return quo_var_new_err("Types don't support multiplication");
}

QuoVar quo_var_div(QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) {
    if (b->val_num == 0.0) return quo_var_new_err("Division by zero");
    return quo_var_new_num(a->val_num / b->val_num);
  }
  return quo_var_new_err("Types don't support division");
}

QuoVar quo_var_mod(QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) {
    int64_t a_val = (int64_t)a->val_num;
    int64_t b_val = (int64_t)b->val_num;
    if (b_val == 0) return quo_var_new_err("Modulo by zero");
    return quo_var_new_num(a_val % b_val);
  }
  return quo_var_new_err("Types don't support modulo");
}

QuoVar quo_var_neg(QuoVar *a) {
  if (quo_var_is_num(a)) return quo_var_new_num(-a->val_num);
  return quo_var_new_err("Type don't support negation");
}

QuoVar quo_var_not(QuoVar *a) {
  if (quo_var_is_bool(a)) return quo_var_new_bool(a->val_num != 0 ? false : true);
  if (quo_var_is_nil(a)) return quo_var_new_bool(true);
  return quo_var_new_err("Type don't support logical negation");
}

bool quo_var_eq(QuoState *s, QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) return a->val_num == b->val_num;
  if (quo_var_is_str(a) || quo_var_is_str(b)) {
    if (quo_var_is_str(a) && quo_var_is_str(b)) return a->val_obj == b->val_obj;
    return quo_var_to_bool(a).val_num == quo_var_to_bool(b).val_num;
  }
  if ((quo_var_is_bool(a) || quo_var_is_bool(b)) || (quo_var_is_nil(a) || quo_var_is_nil(b)))
    return quo_var_to_bool(a).val_num == quo_var_to_bool(b).val_num;
  if (a->type != b->type) return false;
  switch (a->type) {
  case QUO_VAR_TYPE_ERROR: return a->val_err == b->val_err;
  case QUO_VAR_TYPE_OBJ:
    switch (a->val_obj->type) {
    case QUO_OBJ_TYPE_CFN: return quo_obj_as_cfn(a->val_obj)->name == quo_obj_as_cfn(b->val_obj)->name;
    case QUO_OBJ_TYPE_FN: return quo_obj_as_cfn(a->val_obj)->name == quo_obj_as_cfn(b->val_obj)->name;
    case QUO_OBJ_TYPE_STR:
    case QUO_OBJ_TYPE_ARR:
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_DICT: break;
    }
  default: return false;
  }
}

int64_t quo_var_cmp(QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) return a->val_num - b->val_num;
  if (quo_var_is_str(a) || quo_var_is_str(b)) {
    if (quo_var_is_str(a) && quo_var_is_str(b)) return a->val_obj == b->val_obj;
    const double a_val = quo_var_is_num(a) ? a->val_num : quo_obj_as_str(a->val_obj)->len;
    const double b_val = quo_var_is_num(b) ? b->val_num : quo_obj_as_str(b->val_obj)->len;
    return a_val - b_val;
  }
  if (quo_var_is_str(a) && quo_var_is_str(b)) return quo_obj_as_str(a->val_obj)->len - quo_obj_as_str(b->val_obj)->len;
  return 0;
}

uint64_t quo_var_len(QuoVar *v) {
  if (quo_var_is_str(v)) return quo_obj_as_str(v->val_obj)->char_len;
  if (quo_var_is_arr(v)) return quo_arr_len(quo_obj_as_arr(v->val_obj));
  if (quo_var_is_dict(v)) return quo_obj_as_dict(v->val_obj)->dict.count;
  return 0;
}

int64_t quo_var_print(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_NIL: return printf("nil");
  case QUO_VAR_TYPE_BOOL: return printf(v->val_num ? "true" : "false");
  case QUO_VAR_TYPE_NUM: return printf("%g", v->val_num);
  case QUO_VAR_TYPE_ERROR: return printf("<error %s>", v->val_err);
  case QUO_VAR_TYPE_OBJ: {
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return printf("%s", quo_obj_as_str(v->val_obj)->data);
    case QUO_OBJ_TYPE_ARR: {
      int64_t len = 0;
      len += printf("[");
      for (int64_t i = 0; i < quo_arr_len(quo_obj_as_arr(v->val_obj)); i++) {
        if (i > 0) len += printf(", ");
        QuoVar item = quo_arr_get(quo_obj_as_arr(v->val_obj), i);
        if (quo_var_is_str(&item)) len += printf("\"%s\"", quo_obj_as_str(item.val_obj)->data);
        else len += quo_var_print(&item);
      }
      len += printf("]");
      return len;
    }
    case QUO_OBJ_TYPE_DICT: {
      int64_t len = 0;
      len += printf("{");
      bool first = true;
      for (uint64_t i = 0; i < da_capacity(&quo_obj_as_dict(v->val_obj)->dict); i++) {
        QuoHashTableEntry *entry = &da_at(&quo_obj_as_dict(v->val_obj)->dict, i);
        if (entry->key != NULL) {
          if (!first) len += printf(", ");
          printf("\"%s\": ", entry->key->data);
          first = false;
          if (quo_var_is_str(&entry->value)) len += printf("\"%s\"", quo_obj_as_str(entry->value.val_obj)->data);
          else len += quo_var_print(&entry->value);
        }
      }
      len += printf("}");
      return len;
    }
    case QUO_OBJ_TYPE_MODULE: return printf("<module %s>", quo_obj_as_module(v->val_obj)->name->data);
    case QUO_OBJ_TYPE_FN: return printf("<fn %s(%d)>", quo_obj_as_fn(v->val_obj)->name->data, quo_obj_as_fn(v->val_obj)->arity);
    case QUO_OBJ_TYPE_CFN: return printf("<cfn %s>", quo_obj_as_cfn(v->val_obj)->name->data);
    case QUO_OBJ_TYPE_USER: return printf("<%s>", v->val_obj->name->data);
    }
  }
  }
}

// --- STANDARD LIBRARY --- //

static QuoVar quo__cfn_type(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("type() takes 1 argument");
  const char *type_str = "unknown";
  switch (argv[0].type) {
  case QUO_VAR_TYPE_ERROR: type_str = "error"; break;
  case QUO_VAR_TYPE_NIL: type_str = "nil"; break;
  case QUO_VAR_TYPE_BOOL: type_str = "bool"; break;
  case QUO_VAR_TYPE_NUM: type_str = "number"; break;
  case QUO_VAR_TYPE_OBJ:
    switch (argv[0].val_obj->type) {
    case QUO_OBJ_TYPE_STR: type_str = "str"; break;
    case QUO_OBJ_TYPE_ARR: type_str = "arr"; break;
    case QUO_OBJ_TYPE_DICT: type_str = "dict"; break;
    case QUO_OBJ_TYPE_MODULE: type_str = "module"; break;
    case QUO_OBJ_TYPE_FN: type_str = "fn"; break;
    case QUO_OBJ_TYPE_CFN: type_str = "cfn"; break;
    case QUO_OBJ_TYPE_USER: type_str = argv[0].val_obj->name ? argv[0].val_obj->name->data : "USER"; break;
    }
  }
  return quo_var_new_obj(quo_str_new(s, type_str, -1));
}

// Print the values of the given arguments. Separate values with spaces and print a newline at the end.
static QuoVar quo__cfn_print(QuoState *s, int64_t argc, QuoVar *argv) {
  for (uint64_t i = 0; i < argc; i++) {
    quo_var_print(&argv[i]);
    if (i < argc - 1) printf(" ");
  }
  printf("\n");
  return quo_var_new_nil();
}

// Read a line from stdin and return it as a string
static QuoVar quo__cfn_input(QuoState *s, int64_t argc, QuoVar *argv) {
  // Optional prompt arguments
  for (uint64_t i = 0; i < argc; i++) quo_var_print(&argv[i]);
  // Read a line from stdin
  char *line = NULL;
  size_t len = 0;
  ssize_t read = getline(&line, &len, stdin);
  if (read == -1) {
    // EOF or error - return nil
    free(line);
    return quo_var_new_nil();
  }
  // Remove trailing newline if present
  if (read > 0 && line[read - 1] == '\n') line[--read] = '\0';
  QuoStr *str = quo_str_new(s, line, read);
  free(line);
  return quo_var_new_obj(str);
}

// Exit the program with an optional exit code
static QuoVar quo__cfn_exit(QuoState *s, int64_t argc, QuoVar *argv) {
  int64_t code = 0;
  if (argc > 0 && quo_var_is_num(&argv[0])) code = (int64_t)argv[1].val_num;
  exit(code);
  return quo_var_new_nil();
}

// --- BUILT-IN TYPES METHODS --- //

static QuoVar quo__method_bool(QuoState *s, int64_t argc, QuoVar *argv) { return quo_var_to_bool(&argv[0]); }
static QuoVar quo__method_num(QuoState *s, int64_t argc, QuoVar *argv) { return quo_var_to_num(&argv[0]); }
static QuoVar quo__method_str(QuoState *s, int64_t argc, QuoVar *argv) { return quo_var_to_str(s, &argv[0]); }
static QuoVar quo__method_len(QuoState *s, int64_t argc, QuoVar *argv) { return quo_var_new_num(quo_var_len(&argv[0])); }

// - STRING METHODS - //

static QuoVar quo__str_method_get(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2) return quo_var_new_err("get() requires index argument");
  if (!quo_var_is_num(&argv[1])) return quo_var_new_err("Index must be a number");
  QuoStr *str = quo_var_as_str(&argv[0]);
  if (str->char_len == 0) return quo_var_new_obj(quo_str_new(s, "", 0));
  int64_t index = (int64_t)argv[1].val_num;
  if (index > str->char_len) return quo_var_new_err("Index out of range");
  if (index < 0) {
    if (str->char_len == 0) index = 0;
    else
      while (index < 0) index = str->char_len + index;
  }
  const char *pos = quo__utf8_index(str->data, str->len, index);
  uint64_t char_len = quo__utf8_char_len((unsigned char)*pos);
  return quo_var_new_obj(quo_str_new(s, pos, char_len));
}
// Strip whitespace
static QuoVar quo__str_method_strip(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("strip() takes no arguments");
  QuoStr *str = quo_var_as_str(&argv[0]);
  const char *start = str->data;
  const char *end = str->data + str->len;
  while (start < end && quo__is_space(*start)) start++;
  while (end > start && quo__is_space(*(end - 1))) end--;
  return quo_var_new_obj(quo_str_new(s, start, end - start));
}
// Check if string starts with prefix
static QuoVar quo__str_method_startswith(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("startswith() requires a string argument");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *prefix = quo_var_as_str(&argv[1]);
  if (prefix->len > str->len) return quo_var_new_bool(false);
  return quo_var_new_bool(memcmp(str->data, prefix->data, prefix->len) == 0);
}
// Check if string ends with suffix
static QuoVar quo__str_method_endswith(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("endswith() requires a string argument");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *suffix = quo_var_as_str(&argv[1]);
  if (suffix->len > str->len) return quo_var_new_bool(false);
  return quo_var_new_bool(memcmp(str->data + str->len - suffix->len, suffix->data, suffix->len) == 0);
}
// Check if string contains substring
static QuoVar quo__str_method_contains(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("contains() requires a string argument");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *substr = quo_var_as_str(&argv[1]);
  return quo_var_new_bool(strstr(str->data, substr->data));
}
// Split string into array
static QuoVar quo__str_method_split(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("split() requires a delimiter string");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *delim = quo_var_as_str(&argv[1]);
  QuoArr *arr = quo_arr_new();
  if (delim->len == 0) {
    // Split by character if delimiter is empty
    for (uint64_t i = 0; i < str->char_len; i++) {
      const char *ch = quo__utf8_index(str->data, str->len, i);
      uint64_t ch_len = quo__utf8_char_len((unsigned char)*ch);
      quo_arr_push(arr, quo_var_new_obj(quo_str_new(s, ch, ch_len)));
    }
  } else {
    const char *start = str->data;
    for (uint64_t i = 0; i <= str->len - delim->len; i++) {
      if (memcmp(str->data + i, delim->data, delim->len) == 0) {
        quo_arr_push(arr, quo_var_new_obj(quo_str_new(s, start, str->data + i - start)));
        start = str->data + i + delim->len;
        i += delim->len - 1;
      }
    }
    quo_arr_push(arr, quo_var_new_obj(quo_str_new(s, start, str->data + str->len - start)));
  }
  return quo_var_new_obj(arr);
}
// Replace substrings
static QuoVar quo__str_method_replace(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_str(&argv[1]) || !quo_var_is_str(&argv[2]))
    return quo_var_new_err("replace() requires two string arguments");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *from = quo_var_as_str(&argv[1]);
  QuoStr *to = quo_var_as_str(&argv[2]);
  if (from->len == 0) return quo_var_new_obj(str); // No empty pattern
  QuoStringBuilder sb = quo_sb_new();
  uint64_t i = 0;
  while (i < str->len) {
    if (i <= str->len - from->len && memcmp(str->data + i, from->data, from->len) == 0) {
      quo_sb_append(&sb, to->data, to->len);
      i += from->len;
    } else {
      da_add(&sb, str->data[i]);
      i++;
    }
  }
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(s, quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

// - ARRAY METHODS - //

static QuoVar quo__arr_method_get(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2) return quo_var_new_err("get() requires index argument");
  if (!quo_var_is_num(&argv[1])) return quo_var_new_err("Index must be a number");
  return quo_arr_get(quo_obj_as_arr(argv[0].val_obj), (int64_t)argv[1].val_num);
}
static QuoVar quo__arr_method_set(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3) return quo_var_new_err("set() requires index and value arguments");
  if (!quo_var_is_num(&argv[1])) return quo_var_new_err("Index must be a number");
  quo_arr_set(quo_obj_as_arr(argv[0].val_obj), (int64_t)argv[1].val_num, argv[2]);
  return argv[0];
}
static QuoVar quo__arr_method_push(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2) return quo_var_new_err("push() requires value argument");
  quo_arr_push(quo_obj_as_arr(argv[0].val_obj), argv[1]);
  return argv[0];
}
static QuoVar quo__arr_method_pop(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("pop() requires no arguments");
  if (quo_arr_len(quo_obj_as_arr(argv[0].val_obj)) == 0) return quo_var_new_nil();
  return quo_arr_pop(quo_obj_as_arr(argv[0].val_obj));
}

// - DICTIONARY METHODS - //

static QuoVar quo__dict_method_get(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2) return quo_var_new_err("get() requires key argument");
  if (!quo_var_is_str(&argv[1])) return quo_var_new_err("Key must be a string");
  return quo_dict_get(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1]));
}
static QuoVar quo__dict_method_set(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3) return quo_var_new_err("set() requires key and value arguments");
  if (!quo_var_is_str(&argv[1])) return quo_var_new_err("Key must be a string");
  quo_dict_set(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1]), &argv[2]);
  return quo_var_new_nil();
}
static QuoVar quo__dict_method_has(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 && !quo_var_is_str(&argv[1])) return quo_var_new_err("has() requires key string argument");
  QuoVar val = quo_dict_get(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1]));
  return quo_var_new_bool(!quo_var_is_nil(&val));
}
static QuoVar quo__dict_method_keys(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("keys() requires no arguments");
  QuoArr *keys = quo_arr_new();
  QuoDict *dict = quo_var_as_dict(&argv[0]);
  for (int i = 0; i < dict->dict.capacity; ++i) {
    QuoHashTableEntry *entry = &dict->dict.items[i];
    if (entry->key) quo_arr_push(keys, quo_var_new_obj(entry->key));
  }
  return quo_var_new_obj(keys);
}
static QuoVar quo__dict_method_values(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("values() requires no arguments");
  QuoArr *values = quo_arr_new();
  QuoDict *dict = quo_var_as_dict(&argv[0]);
  for (int i = 0; i < dict->dict.capacity; ++i) {
    QuoHashTableEntry *entry = &dict->dict.items[i];
    if (entry->key) quo_arr_push(values, entry->value);
  }
  return quo_var_new_obj(values);
}

// --- REGISTER AND DISPATCH METHODS --- //

static inline void quo__register_builtin_method(QuoState *s, QuoHashTable *methods, const char *name, QuoCFunctionPtr fn) {
  QuoCFn *cfn = quo_cfunction_new(s, name, -1, fn);
  QuoVar cfn_var = quo_var_new_obj(cfn);
  quo_ht_set(methods, cfn->name, &cfn_var);
}

static QuoObj *quo__method_lookup(QuoState *s, QuoVar *val, QuoStr *name) {
  QuoHashTable *methods = NULL;
  switch (val->type) {
  case QUO_VAR_TYPE_BOOL: methods = &s->bool_methods; break;
  case QUO_VAR_TYPE_NUM: methods = &s->num_methods; break;
  case QUO_VAR_TYPE_NIL:
  case QUO_VAR_TYPE_ERROR: break;
  case QUO_VAR_TYPE_OBJ: {
    switch (val->val_obj->type) {
    case QUO_OBJ_TYPE_STR: methods = &s->str_methods; break;
    case QUO_OBJ_TYPE_ARR: methods = &s->arr_methods; break;
    case QUO_OBJ_TYPE_DICT: methods = &s->dict_methods; break;
    case QUO_OBJ_TYPE_USER: methods = &val->val_obj->dict->dict; break;
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_CFN: break;
    }
  }
  }
  if (methods == NULL) return NULL;
  QuoVar value = {0};
  if (!quo_ht_get(methods, name, &value)) return NULL;
  return value.val_obj;
}

// ------------------------------ STATE ------------------------------ //

QuoState *quo_state_new(const char *cwd) {
  srand((unsigned int)time(NULL));

  QuoState *s = quo_alloc(NULL, sizeof(QuoState));

  s->cwd = cwd ? quo_strdup(cwd) : NULL;

  quo_ht_init(&s->string_table);
  quo_ht_init(&s->bool_methods);
  quo_ht_init(&s->num_methods);
  quo_ht_init(&s->str_methods);
  quo_ht_init(&s->arr_methods);
  quo_ht_init(&s->dict_methods);
  quo_ht_init(&s->globals);

  // Register stdlib

  // Register global functions
  quo_state_register_cfn(s, "type", -1, quo__cfn_type);
  quo_state_register_cfn(s, "print", -1, quo__cfn_print);
  quo_state_register_cfn(s, "input", -1, quo__cfn_input);
  quo_state_register_cfn(s, "exit", -1, quo__cfn_exit);

  // Register built-in methods functions
  // Bool
  quo__register_builtin_method(s, &s->bool_methods, "bool", quo__method_bool);
  quo__register_builtin_method(s, &s->bool_methods, "num", quo__method_num);
  quo__register_builtin_method(s, &s->bool_methods, "str", quo__method_str);
  // Number
  quo__register_builtin_method(s, &s->num_methods, "bool", quo__method_bool);
  quo__register_builtin_method(s, &s->num_methods, "num", quo__method_num);
  quo__register_builtin_method(s, &s->num_methods, "str", quo__method_str);
  // String
  quo__register_builtin_method(s, &s->str_methods, "bool", quo__method_bool);
  quo__register_builtin_method(s, &s->str_methods, "num", quo__method_num);
  quo__register_builtin_method(s, &s->str_methods, "str", quo__method_str);
  quo__register_builtin_method(s, &s->str_methods, "len", quo__method_len);
  quo__register_builtin_method(s, &s->str_methods, "get", quo__str_method_get);
  quo__register_builtin_method(s, &s->str_methods, "strip", quo__str_method_strip);
  quo__register_builtin_method(s, &s->str_methods, "startswith", quo__str_method_startswith);
  quo__register_builtin_method(s, &s->str_methods, "endswith", quo__str_method_endswith);
  quo__register_builtin_method(s, &s->str_methods, "contains", quo__str_method_contains);
  quo__register_builtin_method(s, &s->str_methods, "split", quo__str_method_split);
  quo__register_builtin_method(s, &s->str_methods, "replace", quo__str_method_replace);
  // Array
  quo__register_builtin_method(s, &s->arr_methods, "len", quo__method_len);
  quo__register_builtin_method(s, &s->arr_methods, "get", quo__arr_method_get);
  quo__register_builtin_method(s, &s->arr_methods, "set", quo__arr_method_set);
  quo__register_builtin_method(s, &s->arr_methods, "push", quo__arr_method_push);
  quo__register_builtin_method(s, &s->arr_methods, "pop", quo__arr_method_pop);
  // Dictionary
  quo__register_builtin_method(s, &s->dict_methods, "len", quo__method_len);
  quo__register_builtin_method(s, &s->dict_methods, "get", quo__dict_method_get);
  quo__register_builtin_method(s, &s->dict_methods, "set", quo__dict_method_set);
  quo__register_builtin_method(s, &s->dict_methods, "has", quo__dict_method_has);
  quo__register_builtin_method(s, &s->dict_methods, "keys", quo__dict_method_keys);
  quo__register_builtin_method(s, &s->dict_methods, "values", quo__dict_method_values);

  return s;
}

void quo_state_register_module(QuoState *s, QuoModuleInitFn init_fn, QuoModuleCleanupFn cleanup_fn) {
  if (init_fn) init_fn(s);
  if (cleanup_fn) da_add(&s->modules_cleanup_fns, cleanup_fn);
}

void quo_state_register_cfn(QuoState *s, const char *name, int64_t name_len, QuoCFunctionPtr fn) {
  QuoCFn *cfn = quo_cfunction_new(s, name, name_len, fn);
  QuoVar var = quo_var_new_obj(cfn);
  quo_ht_set(&s->globals, cfn->name, &var);
}

QuoObj *quo_state_register_type(QuoState *s, const char *name, int64_t name_len, size_t size) {
  QuoObj *obj = quo_obj_new(size);
  obj->type = QUO_OBJ_TYPE_USER;
  obj->name = quo_str_new(s, name, name_len);
  obj->dict = quo_dict_new();
  QuoVar obj_var = quo_var_new_obj(obj);
  quo_ht_set(&s->types, obj->name, &obj_var);
  return obj;
}

QuoObj *quo_state_get_type_instance(QuoState *s, const char *name, int64_t name_len) {
  QuoVar var;
  if (quo_ht_get(&s->types, quo_str_new(s, name, name_len), &var)) {
    QuoObj *obj = quo_var_as_obj(&var);
    QuoObj *instance = quo_obj_new(obj->size);
    memcpy(instance, obj, obj->size);
    return instance;
  }
  return NULL;
}

void quo_state_type_add(QuoObj *type, QuoStr *name, QuoVar value) { quo_dict_set(type->dict, name, &value); }

void quo_state_type_add_cfn(QuoState *s, QuoObj *type, const char *name, int64_t name_len, QuoCFunctionPtr fn) {
  QuoCFn *cfn = quo_cfunction_new(s, name, name_len, fn);
  quo_state_type_add(type, cfn->name, quo_var_new_obj(cfn));
}

QuoDict *quo_state_register_namespace(QuoState *s, const char *name) {
  QuoDict *ns = quo_dict_new();
  QuoStr *ns_key = quo_str_new(s, name, -1);
  QuoVar ns_value = quo_var_new_obj(ns);
  quo_ht_set(&s->globals, ns_key, &ns_value);
  return ns;
}

bool quo_state_namespace_add(QuoState *s, QuoDict *ns, QuoStr *name, QuoVar value) {
  quo_dict_set(ns, name, &value);
  return true;
}

void quo_state_namespace_add_cfn(QuoState *s, QuoDict *ns, const char *fn_name, QuoCFunctionPtr fn) {
  QuoCFn *cfn = quo_cfunction_new(s, fn_name, -1, fn);
  QuoVar var = quo_var_new_obj(cfn);
  quo_state_namespace_add(s, ns, cfn->name, var);
}

// Unref all values in a hash table
void quo_ht_unref_values(QuoHashTable *t) {
  for (uint64_t i = 0; i < t->capacity; i++)
    if (t->items[i].key) quo_var_unref(&t->items[i].value);
}

// Unref all keys in a hash table (for string tables or other ref-counted keys)
void quo_ht_unref_keys(QuoHashTable *t) {
  for (uint64_t i = 0; i < t->capacity; i++)
    if (t->items[i].key) {
      QuoVar key_var = quo_var_new_obj(t->items[i].key);
      quo_var_unref(&key_var);
    }
}

void quo_state_free(QuoState *s) {
  // Call modules cleanup functions
  for (uint64_t i = 0; i < da_count(&s->modules_cleanup_fns); i++) da_at(&s->modules_cleanup_fns, i)(s);
  // Free cwd
  quo_dealloc(s->cwd);
  // Free tables
  quo_ht_free(&s->globals);
  quo_ht_free(&s->bool_methods);
  quo_ht_free(&s->num_methods);
  quo_ht_free(&s->str_methods);
  quo_ht_free(&s->arr_methods);
  quo_ht_free(&s->dict_methods);
  // Free string table (strings are interned, not ref-counted)
  for (uint64_t i = 0; i < s->string_table.capacity; i++) {
    if (s->string_table.items[i].key) {
      quo_dealloc(s->string_table.items[i].key->data);
      quo_dealloc(s->string_table.items[i].key);
    }
  }
  quo_ht_free(&s->string_table);
  quo_dealloc(s);
}

// ------------------------------ PARSER ------------------------------ //

static void quo__parser_statement(QuoParser *p);
static QuoStmt *quo__stmt_new(enum QuoStmtType type);
static void quo__stmt_free(QuoStmt *stmt);

// Check if the current token matches the given type without consuming it.
static inline bool quo__parser_check(QuoParser *p, enum QuoTokenType type) { return p->current.type == type; }

// Consume the current token and advance to the next one.
static inline void quo__parser_advance(QuoParser *p) {
  p->previous = p->current;
  while (true) {
    p->current = quo_parser_next_token(p);
    if (quo__parser_check(p, QUO_TT_ERROR)) quo__parser_error(p, p->current, p->current.error_msg);
    else break;
  }
}

// Match a specific token type and consume it if it matches.
static inline bool quo__parser_match(QuoParser *p, enum QuoTokenType type) {
  if (!quo__parser_check(p, type)) return false;
  quo__parser_advance(p);
  return true;
}

// Expect a specific token type or error. Consumes the token if it matches.
static inline void quo__parser_expect(QuoParser *p, enum QuoTokenType type, const char *message) {
  if (quo__parser_check(p, type)) {
    quo__parser_advance(p);
    return;
  }
  quo__parser_error(p, p->current, message);
}

static void quo__parser_begin_scope(QuoParser *p) {
  QuoTokenList scope = {0};
  da_add(&p->scopes, scope);
}

static void quo__parser_end_scope(QuoParser *p) {
  if (da_count(&p->scopes) > 0) {
    da_free(&da_at(&p->scopes, da_count(&p->scopes) - 1));
    da_count(&p->scopes)--;
  }
}

static bool quo__parser_is_declared_in_current_scope(QuoParser *p, QuoToken name) {
  if (da_count(&p->scopes) == 0) return false;
  QuoTokenList *current_scope = &da_at(&p->scopes, da_count(&p->scopes) - 1);
  for (int i = 0; i < da_count(current_scope); i++)
    if (quo_tokens_eq(da_at(current_scope, i), name)) return true;
  return false;
}

// Search from innermost to outermost scope
static bool quo__parser_is_declared(QuoParser *p, QuoToken name) {
  for (int s = da_count(&p->scopes) - 1; s >= 0; s--) {
    QuoTokenList *scope = &da_at(&p->scopes, s);
    for (int i = 0; i < da_count(scope); i++)
      if (quo_tokens_eq(da_at(scope, i), name)) return true;
  }
  return false;
}

// Check if variable is declared in the global scope (first scope)
static bool quo__parser_is_global(QuoParser *p, QuoToken name) {
  if (da_count(&p->scopes) == 0) return false;
  QuoTokenList *global_scope = &da_at(&p->scopes, 0);
  for (int i = 0; i < da_count(global_scope); i++)
    if (quo_tokens_eq(da_at(global_scope, i), name)) return true;
  return false;
}

static void quo__parser_declare_variable(QuoParser *p, QuoToken name) {
  if (da_count(&p->scopes) == 0) return;
  QuoTokenList *current_scope = &da_at(&p->scopes, da_count(&p->scopes) - 1);
  da_add(current_scope, name);
}

// --- PARSER EXPRESSIONS --- //

static QuoExpr *quo__parser_literal(QuoParser *p);
static QuoExpr *quo__parser_array_literal(QuoParser *p);
static QuoExpr *quo__parser_dict_literal(QuoParser *p);
static QuoExpr *quo__parser_fn_expr(QuoParser *p);
static QuoExpr *quo__parser_id(QuoParser *p);
static QuoExpr *quo__parser_grouping(QuoParser *p);
static QuoExpr *quo__parser_unary(QuoParser *p);
static QuoExpr *quo__parser_binary(QuoParser *p, QuoExpr *left);
static QuoExpr *quo__parser_call(QuoParser *p, QuoExpr *callee);
static QuoExpr *quo__parser_assignment_expr(QuoParser *p, QuoExpr *target);
static QuoExpr *quo__parser_ternary_expr(QuoParser *p, QuoExpr *condition);
static QuoExpr *quo__parser_member_access(QuoParser *p, QuoExpr *object);

static QuoStmt *quo__parser_block_statement(QuoParser *p);

static QuoExpr *quo__expr_new(enum QuoExprType type, QuoToken token) {
  QuoExpr *expr = quo_alloc(NULL, sizeof(QuoExpr));
  expr->type = type;
  expr->token = token;
  return expr;
}

static void quo__expr_free(QuoExpr *expr) {
  if (!expr) return;
  switch (expr->type) {
  case QUO_EXPR_LITERAL:
  case QUO_EXPR_VARIABLE: break;
  case QUO_EXPR_ARRAY:
    for (int i = 0; i < da_count(&expr->array.elements); i++) quo__expr_free(da_at(&expr->array.elements, i));
    da_free(&expr->array.elements);
    break;
  case QUO_EXPR_BINARY:
    quo__expr_free(expr->binary.left);
    quo__expr_free(expr->binary.right);
    break;
  case QUO_EXPR_UNARY:
  case QUO_EXPR_GROUPING: quo__expr_free(expr->unary.expr); break;
  case QUO_EXPR_CALL:
    quo__expr_free(expr->call.callee);
    for (int i = 0; i < da_count(&expr->call.arguments); i++) quo__expr_free(da_at(&expr->call.arguments, i));
    da_free(&expr->call.arguments);
    break;
  case QUO_EXPR_ASSIGN:
    quo__expr_free(expr->assign.target);
    quo__expr_free(expr->assign.value);
    break;
  case QUO_EXPR_TERNARY:
    quo__expr_free(expr->ternary.condition);
    quo__expr_free(expr->ternary.then_expr);
    quo__expr_free(expr->ternary.else_expr);
    break;
  case QUO_EXPR_MEMBER_ACCESS: quo__expr_free(expr->member_access.object); break;
  case QUO_EXPR_DICT:
    for (int i = 0; i < da_count(&expr->dict.pairs); i++) {
      quo__expr_free(da_at(&expr->dict.pairs, i).key);
      quo__expr_free(da_at(&expr->dict.pairs, i).value);
    }
    da_free(&expr->dict.pairs);
    break;
  case QUO_EXPR_FUNCTION:
    da_free(&expr->function.parameters);
    quo__stmt_free(expr->function.body);
    break;
  }
  quo_dealloc(expr);
}

// Parse rules table
static struct {
  QuoExpr *(*prefix)(QuoParser *);
  QuoExpr *(*infix)(QuoParser *, QuoExpr *);
  // QuoPrecedence levels (lowest to highest)
  enum QuoPrecedence {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_TERNARY,    // ? :
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * / %
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
  } precedence;
} rules[] = {
    [QUO_TT_ERROR] = {NULL, NULL, PREC_NONE},
    [QUO_TT_EOF] = {NULL, NULL, PREC_NONE},

    // Identifiers
    [QUO_TT_ID] = {quo__parser_id, NULL, PREC_NONE},

    // Literals
    [QUO_TT_LITERAL_NUM] = {quo__parser_literal, NULL, PREC_NONE},
    [QUO_TT_LITERAL_STR] = {quo__parser_literal, NULL, PREC_NONE},
    [QUO_TT_TRUE] = {quo__parser_literal, NULL, PREC_NONE},
    [QUO_TT_FALSE] = {quo__parser_literal, NULL, PREC_NONE},
    [QUO_TT_NIL] = {quo__parser_literal, NULL, PREC_NONE},
    [QUO_TT_OBRACKET] = {quo__parser_array_literal, NULL, PREC_NONE},
    [QUO_TT_OBRACE] = {quo__parser_dict_literal, NULL, PREC_NONE},

    // Keywords (not expressions)
    [QUO_TT_FN] = {quo__parser_fn_expr, NULL, PREC_NONE},
    [QUO_TT_LOOP] = {NULL, NULL, PREC_NONE},
    [QUO_TT_BREAK] = {NULL, NULL, PREC_NONE},
    [QUO_TT_CONTINUE] = {NULL, NULL, PREC_NONE},
    [QUO_TT_IF] = {NULL, NULL, PREC_NONE},
    [QUO_TT_ELSE] = {NULL, NULL, PREC_NONE},
    [QUO_TT_RETURN] = {NULL, NULL, PREC_NONE},

    // Delimiters
    [QUO_TT_OPAREN] = {quo__parser_grouping, quo__parser_call, PREC_CALL},
    [QUO_TT_CPAREN] = {NULL, NULL, PREC_NONE},
    [QUO_TT_CBRACE] = {NULL, NULL, PREC_NONE},
    [QUO_TT_CBRACKET] = {NULL, NULL, PREC_NONE},
    [QUO_TT_COMMA] = {NULL, NULL, PREC_NONE},
    [QUO_TT_COLON] = {NULL, NULL, PREC_NONE},
    [QUO_TT_COMMENT] = {NULL, NULL, PREC_NONE},

    // Unary operators (prefix)
    [QUO_TT_MINUS] = {quo__parser_unary, quo__parser_binary, PREC_TERM},
    [QUO_TT_BANG] = {quo__parser_unary, NULL, PREC_NONE},

    // Arithmetic operators (infix)
    [QUO_TT_PLUS] = {NULL, quo__parser_binary, PREC_TERM},
    [QUO_TT_STAR] = {NULL, quo__parser_binary, PREC_FACTOR},
    [QUO_TT_SLASH] = {NULL, quo__parser_binary, PREC_FACTOR},
    [QUO_TT_MOD] = {NULL, quo__parser_binary, PREC_FACTOR},

    // Comparison operators (infix)
    [QUO_TT_LT] = {NULL, quo__parser_binary, PREC_COMPARISON},
    [QUO_TT_GT] = {NULL, quo__parser_binary, PREC_COMPARISON},
    [QUO_TT_LTEQ] = {NULL, quo__parser_binary, PREC_COMPARISON},
    [QUO_TT_GTEQ] = {NULL, quo__parser_binary, PREC_COMPARISON},

    // Equality operators (infix)
    [QUO_TT_DOUBLEEQ] = {NULL, quo__parser_binary, PREC_EQUALITY},
    [QUO_TT_BANGEQ] = {NULL, quo__parser_binary, PREC_EQUALITY},

    // Logical operators (infix)
    [QUO_TT_AND] = {NULL, quo__parser_binary, PREC_AND},
    [QUO_TT_OR] = {NULL, quo__parser_binary, PREC_OR},

    // Assignment operators (infix)
    [QUO_TT_EQ] = {NULL, quo__parser_assignment_expr, PREC_ASSIGNMENT},
    [QUO_TT_PLUSEQ] = {NULL, quo__parser_assignment_expr, PREC_ASSIGNMENT},
    [QUO_TT_MINUSEQ] = {NULL, quo__parser_assignment_expr, PREC_ASSIGNMENT},
    [QUO_TT_MULEQ] = {NULL, quo__parser_assignment_expr, PREC_ASSIGNMENT},
    [QUO_TT_DIVEQ] = {NULL, quo__parser_assignment_expr, PREC_ASSIGNMENT},

    [QUO_TT_QUESTION] = {NULL, quo__parser_ternary_expr, PREC_TERNARY},

    // Access operators
    [QUO_TT_DOT] = {NULL, quo__parser_member_access, PREC_CALL},
};

static QuoExpr *quo__parser_parse_precedence(QuoParser *p, enum QuoPrecedence precedence) {
  quo__parser_advance(p);
  QuoExpr *(*prefix)(QuoParser *) = rules[p->previous.type].prefix;
  if (!prefix) quo__parser_error(p, p->previous, "Expected expression");
  QuoExpr *left = prefix(p);
  while (precedence < rules[p->current.type].precedence) {
    quo__parser_advance(p);
    QuoExpr *(*infix)(QuoParser *, QuoExpr *) = rules[p->previous.type].infix;
    left = infix(p, left);
  }
  return left;
}

static QuoExpr *quo__parser_expression(QuoParser *p) { return quo__parser_parse_precedence(p, PREC_NONE); }

static QuoExpr *quo__parser_literal(QuoParser *p) { return quo__expr_new(QUO_EXPR_LITERAL, p->previous); }

static QuoExpr *quo__parser_array_literal(QuoParser *p) {
  QuoExpr *expr = quo__expr_new(QUO_EXPR_ARRAY, p->previous);
  if (!quo__parser_check(p, QUO_TT_CBRACKET)) {
    do {
      if (quo__parser_check(p, QUO_TT_CBRACKET)) {
        expr->array.trailing_comma = true;
        break;
      }
      QuoExpr *element = quo__parser_expression(p);
      da_add(&expr->array.elements, element);
    } while (quo__parser_match(p, QUO_TT_COMMA));
  }
  quo__parser_expect(p, QUO_TT_CBRACKET, "Expected ']' after array elements");
  return expr;
}

static QuoExpr *quo__parser_dict_literal(QuoParser *p) {
  QuoExpr *expr = quo__expr_new(QUO_EXPR_DICT, p->previous);
  if (!quo__parser_check(p, QUO_TT_CBRACE)) {
    do {
      if (quo__parser_check(p, QUO_TT_CBRACE)) {
        expr->dict.trailing_comma = true;
        break;
      }
      QuoExpr *key = quo__parser_expression(p);
      quo__parser_expect(p, QUO_TT_COLON, "Expected ':' after dictionary key");
      QuoExpr *value = quo__parser_expression(p);
      QuoExprDictPair pair = {key, value};
      // Auto-name function expressions using the dict key if it's a string literal
      if (value->type == QUO_EXPR_FUNCTION && value->function.name.len == 0 && key->type == QUO_EXPR_LITERAL &&
          key->token.type == QUO_TT_LITERAL_STR) {
        value->function.name = key->token;
      }
      da_add(&expr->dict.pairs, pair);
    } while (quo__parser_match(p, QUO_TT_COMMA));
  }
  quo__parser_expect(p, QUO_TT_CBRACE, "Expected '}' after dictionary literal");
  return expr;
}

static QuoExpr *quo__parser_fn_expr(QuoParser *p) {
  // Parse parameters (same as before)
  quo__parser_expect(p, QUO_TT_OPAREN, "Expected '(' after fn");
  quo__parser_begin_scope(p);
  QuoTokenList parameters = {0};
  if (!quo__parser_check(p, QUO_TT_CPAREN)) {
    do {
      quo__parser_expect(p, QUO_TT_ID, "Expected parameter name");
      QuoToken param = p->previous;
      da_add(&parameters, param);
      quo__parser_declare_variable(p, param);
    } while (quo__parser_match(p, QUO_TT_COMMA));
  }
  quo__parser_expect(p, QUO_TT_CPAREN, "Expected ')' after parameters");
  // Block body
  quo__parser_expect(p, QUO_TT_OBRACE, "Expected '{' before function body");
  QuoExpr *expr = quo__expr_new(QUO_EXPR_FUNCTION, p->previous);
  expr->function.parameters = parameters;
  expr->function.body = quo__parser_block_statement(p);
  return expr;
}

// Parse a variable reference
static QuoExpr *quo__parser_variable(QuoParser *p) {
  QuoToken name = p->previous;
  return quo__expr_new(QUO_EXPR_VARIABLE, name);
}

// Parse a grouping expression: ( expr )
static QuoExpr *quo__parser_grouping(QuoParser *p) {
  QuoExpr *expr = quo__parser_expression(p);
  quo__parser_expect(p, QUO_TT_CPAREN, "Expected ')' after expression");
  QuoExpr *group = quo__expr_new(QUO_EXPR_GROUPING, p->previous);
  group->unary.expr = expr;
  return group;
}

// Parse a unary expression: -expr, !expr
static QuoExpr *quo__parser_unary(QuoParser *p) {
  QuoToken op = p->previous;
  QuoExpr *right = quo__parser_parse_precedence(p, PREC_UNARY);
  QuoExpr *expr = quo__expr_new(QUO_EXPR_UNARY, op);
  expr->unary.op = op;
  expr->unary.expr = right;
  return expr;
}

// Parse a binary expression: expr op expr
static QuoExpr *quo__parser_binary(QuoParser *p, QuoExpr *left) {
  QuoToken op = p->previous;
  enum QuoPrecedence precedence = rules[op.type].precedence;
  QuoExpr *right = quo__parser_parse_precedence(p, precedence);
  QuoExpr *expr = quo__expr_new(QUO_EXPR_BINARY, op);
  expr->binary.left = left;
  expr->binary.op = op;
  expr->binary.right = right;
  return expr;
}

// Parse function call: callee(args)
static QuoExpr *quo__parser_call(QuoParser *p, QuoExpr *callee) {
  QuoExpr *expr = quo__expr_new(QUO_EXPR_CALL, p->previous);
  expr->call.callee = callee;
  if (!quo__parser_check(p, QUO_TT_CPAREN)) do {
      da_add(&expr->call.arguments, quo__parser_expression(p));
    } while (quo__parser_match(p, QUO_TT_COMMA));
  quo__parser_expect(p, QUO_TT_CPAREN, "Expected ')' after arguments");
  return expr;
}

// Parse assignment: target = value or target += value etc.
static QuoExpr *quo__parser_assignment_expr(QuoParser *p, QuoExpr *target) {
  QuoToken op = p->previous;
  QuoExpr *value = quo__parser_expression(p);

  // If assigning a function to a variable, auto-name it
  if (value && value->type == QUO_EXPR_FUNCTION) value->function.name = target->token;

  QuoExpr *expr = quo__expr_new(QUO_EXPR_ASSIGN, op);
  expr->assign.target = target;
  expr->assign.value = value;
  return expr;
}

static QuoExpr *quo__parser_ternary_expr(QuoParser *p, QuoExpr *condition) {
  // Parse the then branch
  QuoExpr *then_expr = quo__parser_expression(p);
  // Expect the colon
  quo__parser_expect(p, QUO_TT_COLON, "Expected ':' in ternary expression");
  // Parse the else branch
  QuoExpr *else_expr = quo__parser_parse_precedence(p, PREC_ASSIGNMENT);
  // Create ternary expression
  QuoExpr *expr = quo__expr_new(QUO_EXPR_TERNARY, p->previous);
  expr->ternary.condition = condition;
  expr->ternary.then_expr = then_expr;
  expr->ternary.else_expr = else_expr;
  return expr;
}

static QuoExpr *quo__parser_id(QuoParser *p) {
  if (!quo__parser_is_declared(p, p->previous)) {
    // Check if it's a C function or namespace registered in state
    QuoStr *key = quo_str_new(p->s, p->previous.start, p->previous.len);
    QuoVar value;
    if (quo_ht_get(&p->s->globals, key, &value)) return quo__parser_variable(p);
    quo__parser_error(p, p->previous, "Undefined variable '" QUO_TOKEN_FMT "'", QUO_TOKEN_ARG(p->previous));
  }
  return quo__parser_variable(p);
}

// Parse member access: object.member
static QuoExpr *quo__parser_member_access(QuoParser *p, QuoExpr *object) {
  if (!quo__parser_match(p, QUO_TT_ID)) {
    quo__parser_error(p, p->current, "Expected member name after '.'");
    return object;
  }
  QuoToken member = p->previous;
  // Create member access expression
  QuoExpr *expr = quo__expr_new(QUO_EXPR_MEMBER_ACCESS, p->previous);
  expr->member_access.object = object;
  expr->member_access.member = member;
  // If followed by '(', convert to method call
  if (quo__parser_match(p, QUO_TT_OPAREN)) {
    QuoExpr *call = quo__expr_new(QUO_EXPR_CALL, member);
    call->call.callee = expr; // The member access becomes the callee
    // Parse arguments
    if (!quo__parser_check(p, QUO_TT_CPAREN)) {
      do { da_add(&call->call.arguments, quo__parser_expression(p)); } while (quo__parser_match(p, QUO_TT_COMMA));
    }
    quo__parser_expect(p, QUO_TT_CPAREN, "Expected ')' after arguments");
    // Handle chaining
    if (quo__parser_match(p, QUO_TT_DOT)) return quo__parser_member_access(p, call);
    return call;
  }
  // Handle chained member access
  if (quo__parser_match(p, QUO_TT_DOT)) return quo__parser_member_access(p, expr);
  return expr;
}

// --- PARSER STATEMENTS --- //

static QuoStmt *quo__stmt_new(enum QuoStmtType type) {
  QuoStmt *stmt = quo_alloc(NULL, sizeof(QuoStmt));
  stmt->type = type;
  return stmt;
}

static void quo__stmt_free(QuoStmt *stmt) {
  if (!stmt) return;
  switch (stmt->type) {
  case QUO_STMT_EXPRESSION: quo__expr_free(stmt->expression); break;
  case QUO_STMT_IMPORT: break;
  case QUO_STMT_RETURN:
    if (stmt->expression) quo__expr_free(stmt->expression);
    break;
  case QUO_STMT_IF:
    quo__expr_free(stmt->if_stmt.condition);
    quo__stmt_free(stmt->if_stmt.then_branch);
    if (stmt->if_stmt.else_branch) quo__stmt_free(stmt->if_stmt.else_branch);
    break;
  case QUO_STMT_LOOP:
    if (stmt->loop.initializer) quo__stmt_free(stmt->loop.initializer);
    if (stmt->loop.condition) quo__expr_free(stmt->loop.condition);
    if (stmt->loop.increment) quo__stmt_free(stmt->loop.increment);
    if (stmt->loop.body) quo__stmt_free(stmt->loop.body);
    break;
  case QUO_STMT_BLOCK:
    for (int i = 0; i < da_count(&stmt->block); i++) quo__stmt_free(da_at(&stmt->block, i));
    da_free(&stmt->block);
    break;
  case QUO_STMT_VAR_DECL:
    if (stmt->var_decl.initializer) quo__expr_free(stmt->var_decl.initializer);
    break;
  case QUO_STMT_BREAK:
  case QUO_STMT_CONTINUE: break;
  }
  quo_dealloc(stmt);
}

static QuoStmt *quo__parser_expression_statement(QuoParser *p) {
  QuoExpr *expr = quo__parser_expression(p);
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_EXPRESSION);
  stmt->expression = expr;
  return stmt;
}

static QuoStmt *quo__parser_import_stmt(QuoParser *p) {
  // Check if we're at global scope (only 1 scope exists)
  if (da_count(&p->scopes) > 1) quo__parser_error(p, p->previous, "Import statements can only be used at global scope");
  quo__parser_expect(p, QUO_TT_LITERAL_STR, "Expected import path string");
  QuoToken path = p->previous;
  // Resolve the module path
  char *mod_path = quo_strdupf("%s/%.*s.quo", p->s->cwd, path.len, path.start);
  char *mod_name = quo_file_name(mod_path);
  // Create and compile the module
  QuoModule *module = quo_module_new(p->s, mod_name, -1);
  quo_dealloc(mod_name);
  // Parse the module
  QuoParser *mod_parser = quo_parser_new(module->state, mod_path);
  quo_dealloc(mod_path);
  if (!mod_parser) {
    quo__parser_error(p, p->current, "Module not found: %.*s", path.len, path.start);
    return NULL;
  }
  if (!quo_parser_parse(mod_parser)) {
    quo_parser_free(mod_parser);
    return NULL;
  }
  // Compile the module's AST into a function and run it
  QuoCompiler *mod_compiler = quo_compiler_new(module->state, mod_parser->file_name, -1);
  QuoFn *main_fn = quo_compiler_compile(mod_compiler, mod_parser->ast);
  quo_compiler_free(mod_compiler);
  quo_parser_free(mod_parser);
  QuoVM *vm = quo_vm_new(module->state);
  if (!vm) return NULL;
  QuoVar result = quo_vm_run(vm, main_fn);
  // TODO ref globals
  // quo_vm_free(vm);

  // Declare the module name in the current scope
  QuoToken module_name = {
      .start = module->name->data,
      .len = module->name->len,
      .type = QUO_TT_ID,
      .line = path.line,
      .column = path.column,
  };
  if (quo__parser_is_declared_in_current_scope(p, module_name)) {
    quo__parser_error(p, module_name, "Variable '" QUO_TOKEN_FMT "' already declared in this scope", QUO_TOKEN_ARG(module_name));
  }
  quo__parser_declare_variable(p, module_name);
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_IMPORT);
  stmt->import = module;
  return stmt;
}

static QuoStmt *quo__parser_var_statement(QuoParser *p) {
  quo__parser_expect(p, QUO_TT_ID, "Expected variable name");
  QuoToken name = p->previous;
  if (quo__parser_is_declared_in_current_scope(p, name))
    quo__parser_error(p, name, "Variable '" QUO_TOKEN_FMT "' already declared in this scope", QUO_TOKEN_ARG(name));
  quo__parser_declare_variable(p, name); // Declare the variable in current scope
  QuoExpr *initializer = NULL;
  if (quo__parser_match(p, QUO_TT_EQ)) {
    initializer = quo__parser_expression(p);
    // Auto-name function expressions
    if (initializer && initializer->type == QUO_EXPR_FUNCTION) initializer->function.name = name;
  }
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_VAR_DECL);
  stmt->var_decl.name = name;
  stmt->var_decl.initializer = initializer;
  return stmt;
}

static QuoStmt *quo__parser_fn_call_stmt(QuoParser *p) {
  QuoToken func_name = p->previous;
  if (!quo__parser_is_declared(p, func_name))
    quo__parser_error(p, func_name, "Undefined variable '" QUO_TOKEN_FMT "'", QUO_TOKEN_ARG(func_name));
  quo__parser_expect(p, QUO_TT_OPAREN, "Expected '(' after function name");
  QuoExpr *callee = quo__expr_new(QUO_EXPR_VARIABLE, func_name);
  QuoExpr *call = quo__parser_call(p, callee);
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_EXPRESSION);
  stmt->expression = call;
  return stmt;
}

static QuoStmt *quo__parser_block_statement(QuoParser *p) {
  quo__parser_begin_scope(p); // New scope for block
  QuoStmtList old_stmts = p->ast;
  QuoStmtList block_stmts = {0};
  p->ast = block_stmts;
  while (!quo__parser_check(p, QUO_TT_CBRACE) && !quo__parser_check(p, QUO_TT_EOF)) quo__parser_statement(p);
  quo__parser_expect(p, QUO_TT_CBRACE, "Expected '}' after block");
  block_stmts = p->ast;
  p->ast = old_stmts;
  quo__parser_end_scope(p); // End scope
  QuoStmt *block = quo__stmt_new(QUO_STMT_BLOCK);
  block->block = block_stmts;
  return block;
}

static QuoStmt *quo__parser_return_statement(QuoParser *p) {
  QuoExpr *value = NULL;
  if (!quo__parser_check(p, QUO_TT_EOF) && !quo__parser_check(p, QUO_TT_CBRACE)) value = quo__parser_expression(p);
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_RETURN);
  stmt->expression = value;
  return stmt;
}

static QuoStmt *quo__parser_if_statement(QuoParser *p) {
  QuoExpr *condition = quo__parser_expression(p);
  // Parse then branch - either block or single statement
  QuoStmt *then_branch;
  if (quo__parser_match(p, QUO_TT_OBRACE)) then_branch = quo__parser_block_statement(p);
  else {
    // Single statement - wrap in a block
    quo__parser_begin_scope(p);
    QuoStmtList old_ast = p->ast;
    p->ast = (QuoStmtList){0};
    quo__parser_statement(p);
    then_branch = quo__stmt_new(QUO_STMT_BLOCK);
    then_branch->block = p->ast;
    p->ast = old_ast;
    quo__parser_end_scope(p);
  }
  // Parse else branch (optional)
  QuoStmt *else_branch = NULL;
  if (quo__parser_match(p, QUO_TT_ELSE)) {
    if (quo__parser_match(p, QUO_TT_IF)) else_branch = quo__parser_if_statement(p);
    else if (quo__parser_match(p, QUO_TT_OBRACE)) else_branch = quo__parser_block_statement(p);
    else {
      // Single statement after else
      quo__parser_begin_scope(p);
      QuoStmtList old_ast = p->ast;
      p->ast = (QuoStmtList){0};
      quo__parser_statement(p);
      else_branch = quo__stmt_new(QUO_STMT_BLOCK);
      else_branch->block = p->ast;
      p->ast = old_ast;
      quo__parser_end_scope(p);
    }
  }
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_IF);
  stmt->if_stmt.condition = condition;
  stmt->if_stmt.then_branch = then_branch;
  stmt->if_stmt.else_branch = else_branch;
  return stmt;
}

static QuoStmt *quo__parser_loop_statement(QuoParser *p) {
  p->loop_count++;
  quo__parser_expect(p, QUO_TT_OPAREN, "Expected '(' after 'loop'");
  // Parse initializer (optional)
  QuoStmt *initializer = NULL;
  if (!quo__parser_check(p, QUO_TT_COMMA)) {
    if (quo__parser_match(p, QUO_TT_VAR)) initializer = quo__parser_var_statement(p);
    else initializer = quo__parser_expression_statement(p);
  }
  quo__parser_expect(p, QUO_TT_COMMA, "Expected ',' after loop initializer");
  // Parse condition (optional)
  QuoExpr *condition = NULL;
  if (!quo__parser_check(p, QUO_TT_COMMA)) condition = quo__parser_expression(p);
  quo__parser_expect(p, QUO_TT_COMMA, "Expected ',' after loop condition");
  // Parse increment (optional)
  QuoStmt *increment = NULL;
  if (!quo__parser_check(p, QUO_TT_CPAREN)) increment = quo__parser_expression_statement(p);
  quo__parser_expect(p, QUO_TT_CPAREN, "Expected ')' after loop clauses");
  // Parse body - either block or single statement
  QuoStmt *body;
  if (quo__parser_match(p, QUO_TT_OBRACE)) body = quo__parser_block_statement(p);
  else {
    // Single statement - wrap in a block
    quo__parser_begin_scope(p);
    QuoStmtList old_ast = p->ast;
    p->ast = (QuoStmtList){0};
    quo__parser_statement(p);
    body = quo__stmt_new(QUO_STMT_BLOCK);
    body->block = p->ast;
    p->ast = old_ast;
    quo__parser_end_scope(p);
  }
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_LOOP);
  stmt->loop.initializer = initializer;
  stmt->loop.condition = condition;
  stmt->loop.increment = increment;
  stmt->loop.body = body;
  p->loop_count--;
  return stmt;
}

static QuoStmt *quo__parser_break_statement(QuoParser *p) {
  if (p->loop_count == 0) quo__parser_error(p, p->current, "'break' statement outside of loop");
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_BREAK);
  return stmt;
}

static QuoStmt *quo__parser_continue_statement(QuoParser *p) {
  if (p->loop_count == 0) quo__parser_error(p, p->current, "'continue' statement outside of loop");
  QuoStmt *stmt = quo__stmt_new(QUO_STMT_CONTINUE);
  return stmt;
}

static void quo__parser_statement(QuoParser *p) {
  QuoStmt *stmt = NULL;
  if (quo__parser_match(p, QUO_TT_VAR)) stmt = quo__parser_var_statement(p);
  else if (quo__parser_match(p, QUO_TT_OBRACE)) stmt = quo__parser_block_statement(p);
  else if (quo__parser_match(p, QUO_TT_RETURN)) stmt = quo__parser_return_statement(p);
  else if (quo__parser_match(p, QUO_TT_IF)) stmt = quo__parser_if_statement(p);
  else if (quo__parser_match(p, QUO_TT_LOOP)) stmt = quo__parser_loop_statement(p);
  else if (quo__parser_match(p, QUO_TT_BREAK)) stmt = quo__parser_break_statement(p);
  else if (quo__parser_match(p, QUO_TT_CONTINUE)) stmt = quo__parser_continue_statement(p);
  else if (quo__parser_match(p, QUO_TT_IMPORT)) stmt = quo__parser_import_stmt(p);
  else stmt = quo__parser_expression_statement(p);
  if (stmt) da_add(&p->ast, stmt);
}

QuoParser *quo_parser_new(QuoState *s, const char *path) {
  QuoParser *p = quo_alloc(NULL, sizeof(QuoParser));
  p->s = s;
  p->file_path = quo_strdup(path);
  p->file_name = quo_file_name(path);
  char *source = quo_read_file(path);
  if (!source) {
    quo_dealloc(p->file_path);
    quo_dealloc(p);
    return NULL;
  }
  p->source = source;
  p->pos = 0;
  p->line = 1;
  p->column = 1;
  // Start global scope
  QuoTokenList scope = {0};
  da_add(&p->scopes, scope);
  return p;
}

bool quo_parser_parse(QuoParser *p) {
  quo__parser_advance(p);
  while (!quo__parser_check(p, QUO_TT_EOF)) quo__parser_statement(p);
  return !p->s->had_compile_error;
}

void quo_parser_free(QuoParser *p) {
  if (!p) return;
  for (int i = 0; i < da_count(&p->ast); i++) quo__stmt_free(da_at(&p->ast, i));
  da_free(&p->ast);
  while (da_count(&p->scopes) > 0) {
    da_free(&da_at(&p->scopes, da_count(&p->scopes) - 1));
    da_count(&p->scopes)--;
  }
  da_free(&p->scopes);
  quo_dealloc(p->file_name);
  quo_dealloc(p->file_path);
  quo_dealloc(p->source);
  quo_dealloc(p);
}

// ------------------------------ COMPILER ------------------------------ //

static void quo__compiler_stmt(QuoCompiler *c, QuoStmt *stmt);

static bool quo__compiler_is_global_declared(QuoCompiler *c, QuoToken name) {
  for (int i = 0; i < da_count(&c->declared_globals); i++)
    if (quo_tokens_eq(da_at(&c->declared_globals, i), name)) return true;
  return false;
}

static void quo__compiler_begin_scope(QuoCompiler *c) { c->scope_depth++; }

static void quo__compiler_end_scope(QuoCompiler *c) {
  c->scope_depth--;
  while (da_count(&c->locals) > 0 && da_at(&c->locals, da_count(&c->locals) - 1).depth > c->scope_depth) {
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    da_count(&c->locals)--;
  }
}

static void quo__compiler_add_local_variable(QuoCompiler *c, QuoToken name) {
  QuoLocalVariable local = {name, -1}; // -1 means "not initialized yet"
  da_add(&c->locals, local);
}

static int64_t quo__compiler_resolve_local(QuoCompiler *c, QuoToken name) {
  for (int64_t i = da_count(&c->locals) - 1; i >= 0; i--) {
    QuoLocalVariable local = da_at(&c->locals, i);
    if (quo_tokens_eq(name, local.name)) {
      if (local.depth == -1) {
        // quo__parser_error(a, QUO_ERROR, name, "Can't read local variable in its own initializer");
        return -1;
      }
      return i;
    }
  }
  return -1;
}

static void quo__compiler_expr(QuoCompiler *c, QuoExpr *e) {
  switch (e->type) {
  case QUO_EXPR_LITERAL: {
    QuoVar constant = {0};
    switch (e->token.type) {
    case QUO_TT_TRUE:
      constant.type = QUO_VAR_TYPE_BOOL;
      constant.val_num = 1.0;
      break;
    case QUO_TT_FALSE:
      constant.type = QUO_VAR_TYPE_BOOL;
      constant.val_num = 0.0;
      break;
    case QUO_TT_NIL: constant.type = QUO_VAR_TYPE_NIL; break;
    case QUO_TT_LITERAL_NUM:
      constant.type = QUO_VAR_TYPE_NUM;
      constant.val_num = quo_strtod(e->token.start, e->token.len);
      break;
    case QUO_TT_LITERAL_STR:
      constant.type = QUO_VAR_TYPE_OBJ;
      constant.val_obj = (QuoObj *)quo_str_new(c->s, e->token.start, e->token.len);
      break;
    default: break;
    }
    uint64_t index = quo__function_push_constant(c->fn, &constant);
    quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
    quo__function_push_instruction(c->fn, index);
    break;
  }
  case QUO_EXPR_ARRAY: {
    for (int i = 0; i < da_count(&e->array.elements); i++) quo__compiler_expr(c, da_at(&e->array.elements, i));
    quo__function_push_instruction(c->fn, QUO_OP_ARRAY);
    quo__function_push_instruction(c->fn, da_count(&e->array.elements));
    break;
  }
  case QUO_EXPR_DICT: {
    // Push the number of pairs
    uint64_t pair_count = da_count(&e->dict.pairs);
    // Compile each key-value pair
    for (int i = 0; i < pair_count; i++) {
      QuoExprDictPair pair = da_at(&e->dict.pairs, i);
      quo__compiler_expr(c, pair.key);   // Compile key expression (should evaluate to a string)
      quo__compiler_expr(c, pair.value); // Compile value expression
    }
    // Create dictionary
    quo__function_push_instruction(c->fn, QUO_OP_DICT);
    quo__function_push_instruction(c->fn, pair_count);
    break;
  }
  case QUO_EXPR_VARIABLE: {
    int64_t idx = quo__compiler_resolve_local(c, e->token);
    if (idx != -1) {
      // Local variable
      quo__function_push_instruction(c->fn, QUO_OP_GET_LOCAL);
      quo__function_push_instruction(c->fn, idx);
    } else {
      // Treat as global - VM will error if undefined
      QuoVar name_var = quo_var_new_obj(quo_str_new(c->s, e->token.start, e->token.len));
      uint64_t name_idx = quo__function_push_constant(c->fn, &name_var);
      quo__function_push_instruction(c->fn, QUO_OP_GET_GLOBAL);
      quo__function_push_instruction(c->fn, name_idx);
    }
    break;
  }
  case QUO_EXPR_FUNCTION: {
    QuoCompiler *fn_compiler = quo_compiler_new(c->s, e->function.name.start, e->function.name.len);
    fn_compiler->fn->arity = da_count(&e->function.parameters);
    quo__compiler_begin_scope(fn_compiler);
    for (int i = 0; i < da_count(&e->function.parameters); i++) {
      QuoToken param = da_at(&e->function.parameters, i);
      quo__compiler_add_local_variable(fn_compiler, param);
      da_at(&fn_compiler->locals, da_count(&fn_compiler->locals) - 1).depth = fn_compiler->scope_depth;
    }
    QuoFn *fn = quo_compiler_compile(fn_compiler, e->function.body->block);
    quo_compiler_free(fn_compiler);
    QuoVar fn_var = quo_var_new_obj(fn);
    uint64_t fn_idx = quo__function_push_constant(c->fn, &fn_var);
    quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
    quo__function_push_instruction(c->fn, fn_idx);
    break;
  }
  case QUO_EXPR_BINARY: {
    // Special handling for 'and' and 'or' (short-circuit operators)
    if (e->binary.op.type == QUO_TT_AND) {
      // AND: evaluate left, if false jump to end with false value, else pop and evaluate right
      quo__compiler_expr(c, e->binary.left);
      uint64_t jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
      quo__function_push_instruction(c->fn, QUO_OP_POP); // Pop the true value
      quo__compiler_expr(c, e->binary.right);
      quo__function_patch_jump(c->fn, jump_to_end);
      // If left was false, it stays on stack. If true, right value is on stack.
    } else if (e->binary.op.type == QUO_TT_OR) {
      quo__compiler_expr(c, e->binary.left);
      // If left is FALSE, jump to evaluate right side
      uint64_t jump_to_right = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
      // Left was TRUE, we're done - return left (still on stack)
      // But we need to skip the right side evaluation
      uint64_t jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
      // Right side: pop the false left value, evaluate right
      quo__function_patch_jump(c->fn, jump_to_right);
      quo__function_push_instruction(c->fn, QUO_OP_POP); // Pop false left value
      quo__compiler_expr(c, e->binary.right);
      quo__function_patch_jump(c->fn, jump_to_end);
    } else {
      quo__compiler_expr(c, e->binary.left);
      quo__compiler_expr(c, e->binary.right);
      switch (e->binary.op.type) {
      case QUO_TT_PLUS: quo__function_push_instruction(c->fn, QUO_OP_ADD); break;
      case QUO_TT_MINUS: quo__function_push_instruction(c->fn, QUO_OP_SUB); break;
      case QUO_TT_STAR: quo__function_push_instruction(c->fn, QUO_OP_MUL); break;
      case QUO_TT_SLASH: quo__function_push_instruction(c->fn, QUO_OP_DIV); break;
      case QUO_TT_MOD: quo__function_push_instruction(c->fn, QUO_OP_MOD); break;
      case QUO_TT_DOUBLEEQ: quo__function_push_instruction(c->fn, QUO_OP_EQ); break;
      case QUO_TT_BANGEQ: quo__function_push_instruction(c->fn, QUO_OP_NEQ); break;
      case QUO_TT_GT: quo__function_push_instruction(c->fn, QUO_OP_GT); break;
      case QUO_TT_LT: quo__function_push_instruction(c->fn, QUO_OP_LT); break;
      case QUO_TT_GTEQ: quo__function_push_instruction(c->fn, QUO_OP_GTEQ); break;
      case QUO_TT_LTEQ: quo__function_push_instruction(c->fn, QUO_OP_LTEQ); break;
      default: break;
      }
    }
    break;
  }
  case QUO_EXPR_UNARY: {
    quo__compiler_expr(c, e->unary.expr);
    switch (e->unary.op.type) {
    case QUO_TT_BANG: quo__function_push_instruction(c->fn, QUO_OP_NOT); break;
    case QUO_TT_MINUS: quo__function_push_instruction(c->fn, QUO_OP_NEGATE); break;
    default: break;
    }
    break;
  }
  case QUO_EXPR_GROUPING: {
    quo__compiler_expr(c, e->unary.expr);
    break;
  }
  case QUO_EXPR_CALL: {
    if (e->call.callee->type == QUO_EXPR_MEMBER_ACCESS) {
      // Method call: obj.method(args)
      quo__compiler_expr(c, e->call.callee->member_access.object);
      QuoVar method_name =
          quo_var_new_obj(quo_str_new(c->s, e->call.callee->member_access.member.start, e->call.callee->member_access.member.len));
      uint64_t name_idx = quo__function_push_constant(c->fn, &method_name);
      quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
      quo__function_push_instruction(c->fn, name_idx);
      int64_t arity = da_count(&e->call.arguments);
      for (int64_t i = 0; i < arity; i++) quo__compiler_expr(c, da_at(&e->call.arguments, i));
      quo__function_push_instruction(c->fn, QUO_OP_METHOD_CALL);
      quo__function_push_instruction(c->fn, arity);
    } else {
      // Regular call: func(args)
      quo__compiler_expr(c, e->call.callee);
      int64_t arity = da_count(&e->call.arguments);
      for (int64_t i = 0; i < arity; i++) quo__compiler_expr(c, da_at(&e->call.arguments, i));
      quo__function_push_instruction(c->fn, QUO_OP_CALL);
      quo__function_push_instruction(c->fn, arity);
    }
    break;
  }
  case QUO_EXPR_ASSIGN: {
    // Check if target is a member access (dict.field = value)
    if (e->assign.target->type == QUO_EXPR_MEMBER_ACCESS) {
      // Compile: push dict, push field name, push value
      quo__compiler_expr(c, e->assign.target->member_access.object);

      QuoVar field_name =
          quo_var_new_obj(quo_str_new(c->s, e->assign.target->member_access.member.start, e->assign.target->member_access.member.len));
      uint64_t name_idx = quo__function_push_constant(c->fn, &field_name);
      quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
      quo__function_push_instruction(c->fn, name_idx);

      quo__compiler_expr(c, e->assign.value);

      quo__function_push_instruction(c->fn, QUO_OP_SET_MEMBER);
    } else {
      bool is_compound = e->token.type != QUO_TT_EQ;

      if (is_compound) {
        // For compound assignment: a -= 1 → a = a - 1
        // First push the current value of the variable
        quo__compiler_expr(c, e->assign.target); // Push current value
        quo__compiler_expr(c, e->assign.value);  // Push new value

        // Apply the operation
        switch (e->token.type) {
        case QUO_TT_PLUSEQ: quo__function_push_instruction(c->fn, QUO_OP_ADD); break;
        case QUO_TT_MINUSEQ: quo__function_push_instruction(c->fn, QUO_OP_SUB); break;
        case QUO_TT_MULEQ: quo__function_push_instruction(c->fn, QUO_OP_MUL); break;
        case QUO_TT_DIVEQ: quo__function_push_instruction(c->fn, QUO_OP_DIV); break;
        default: break;
        }
        // Result of (a - 1) is now on stack
      } else {
        quo__compiler_expr(c, e->assign.value);
      }

      // Store the result back
      int64_t idx = quo__compiler_resolve_local(c, e->assign.target->token);
      if (idx != -1) {
        quo__function_push_instruction(c->fn, QUO_OP_SET_LOCAL);
        quo__function_push_instruction(c->fn, idx);
      } else {
        QuoStr *name = quo_str_new(c->s, e->assign.target->token.start, e->assign.target->token.len);
        QuoVar name_var = quo_var_new_obj(name);
        uint64_t name_idx = quo__function_push_constant(c->fn, &name_var);
        quo__function_push_instruction(c->fn, QUO_OP_SET_GLOBAL);
        quo__function_push_instruction(c->fn, name_idx);
      }
    }
    break;
  }
  case QUO_EXPR_TERNARY: {
    // Compile condition
    quo__compiler_expr(c, e->ternary.condition);
    // Jump to else if condition is false
    uint64_t jump_to_else = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
    // Pop condition, compile then branch
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    quo__compiler_expr(c, e->ternary.then_expr);
    // Jump over else branch
    uint64_t jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
    // Patch else jump, pop condition, compile else
    quo__function_patch_jump(c->fn, jump_to_else);
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    quo__compiler_expr(c, e->ternary.else_expr);
    // Patch end jump
    quo__function_patch_jump(c->fn, jump_to_end);
    break;
  }
  case QUO_EXPR_MEMBER_ACCESS: {
    // Compile the object
    quo__compiler_expr(c, e->member_access.object);
    // Push method name as string constant
    QuoVar field_name = quo_var_new_obj(quo_str_new(c->s, e->member_access.member.start, e->member_access.member.len));
    uint64_t name_idx = quo__function_push_constant(c->fn, &field_name);
    quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
    quo__function_push_instruction(c->fn, name_idx);
    quo__function_push_instruction(c->fn, QUO_OP_MEMBER_ACCESS);
    break;
  }
  }
}

static void quo__compiler_stmt(QuoCompiler *c, QuoStmt *s) {
  switch (s->type) {

  case QUO_STMT_EXPRESSION: {
    quo__compiler_expr(c, s->expression);
    // Only pop if the expression leaves a value (not an assignment)
    if (s->expression->type != QUO_EXPR_ASSIGN) quo__function_push_instruction(c->fn, QUO_OP_POP);
    break;
  }
  case QUO_STMT_IMPORT: {
    QuoModule *module = s->import;
    // Store the module as a global variable
    QuoVar module_var = quo_var_new_obj((QuoObj *)module);
    // Set as global
    // da_add(&c->declared_globals, s->import->name);
    uint64_t ns_idx = quo__function_push_constant(c->fn, &module_var);
    quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
    quo__function_push_instruction(c->fn, ns_idx);
    QuoStr *mod_name = quo_str_new(c->s, module->name->data, module->name->len);
    QuoVar name_var = quo_var_new_obj(mod_name);
    uint64_t name_idx = quo__function_push_constant(c->fn, &name_var);
    quo__function_push_instruction(c->fn, QUO_OP_SET_GLOBAL);
    quo__function_push_instruction(c->fn, name_idx);
    break;
  }
  case QUO_STMT_VAR_DECL: {
    // Compile initializer if present, otherwise nil
    if (s->var_decl.initializer) {
      quo__compiler_expr(c, s->var_decl.initializer);
    } else {
      uint64_t nil_idx = quo__function_push_constant(c->fn, &quo_var_new_nil());
      quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
      quo__function_push_instruction(c->fn, nil_idx);
    }
    if (c->scope_depth == 0) {
      // Global variable declaration
      da_add(&c->declared_globals, s->var_decl.name);
      QuoVar name_var = quo_var_new_obj(quo_str_new(c->s, s->var_decl.name.start, s->var_decl.name.len));
      uint64_t name_idx = quo__function_push_constant(c->fn, &name_var);
      quo__function_push_instruction(c->fn, QUO_OP_SET_GLOBAL);
      quo__function_push_instruction(c->fn, name_idx);
    } else {
      // Local variable declaration
      quo__compiler_add_local_variable(c, s->var_decl.name);
      quo__function_push_instruction(c->fn, QUO_OP_SET_LOCAL);
      quo__function_push_instruction(c->fn, da_count(&c->locals) - 1);
      da_at(&c->locals, da_count(&c->locals) - 1).depth = c->scope_depth;
    }
    break;
  }
  case QUO_STMT_RETURN: {
    if (s->expression) quo__compiler_expr(c, s->expression);
    else {
      // Return nil by default
      uint64_t nil_idx = quo__function_push_constant(c->fn, &quo_var_new_nil());
      quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
      quo__function_push_instruction(c->fn, nil_idx);
    }
    quo__function_push_instruction(c->fn, QUO_OP_RETURN);
    break;
  }
  case QUO_STMT_BLOCK: {
    quo__compiler_begin_scope(c);
    for (int i = 0; i < da_count(&s->block); i++) quo__compiler_stmt(c, da_at(&s->block, i));
    quo__compiler_end_scope(c);
    break;
  }
  case QUO_STMT_IF: {
    // Compile condition
    quo__compiler_expr(c, s->if_stmt.condition);
    // Emit conditional jump to else branch (or end)
    uint64_t jump_to_else = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    // Compile then branch
    quo__compiler_stmt(c, s->if_stmt.then_branch);
    // If there's an else branch, we need to jump over it
    uint64_t jump_to_end = 0;
    if (s->if_stmt.else_branch) jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
    // Patch the conditional jump to point to the else branch (or end)
    quo__function_patch_jump(c->fn, jump_to_else);
    // Compile else branch if it exists
    if (s->if_stmt.else_branch) {
      quo__compiler_stmt(c, s->if_stmt.else_branch);
      // Patch the jump over else
      quo__function_patch_jump(c->fn, jump_to_end);
    }
    break;
  }
  case QUO_STMT_LOOP: {
    // Create loop context
    struct QuoLoopContext loop_ctx = {0};
    struct QuoLoopContext *prev_loop = c->loop;
    c->loop = &loop_ctx;
    quo__compiler_begin_scope(c);
    // Compile initializer if it exists (runs once before the loop)
    if (s->loop.initializer) quo__compiler_stmt(c, s->loop.initializer);
    // Mark the start of the condition check
    uint64_t condition_start = da_count(&c->fn->instructions);
    loop_ctx.start = condition_start;
    // Compile condition if it exists, otherwise default to true
    uint64_t jump_to_end = 0;
    if (s->loop.condition) {
      quo__compiler_expr(c, s->loop.condition);
      jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
    }
    // Compile the body
    quo__compiler_stmt(c, s->loop.body);
    // Mark the start of increment section (for continue patching)
    uint64_t increment_start = da_count(&c->fn->instructions);
    loop_ctx.increment_start = increment_start;
    // Compile increment if it exists
    if (s->loop.increment) {
      quo__compiler_stmt(c, s->loop.increment);
      quo__function_push_instruction(c->fn, QUO_OP_POP); // Discard increment result
    }
    // Jump back to condition check
    quo__function_emit_loop(c->fn, condition_start);
    // Patch the conditional jump to exit the loop
    if (s->loop.condition) quo__function_patch_jump(c->fn, jump_to_end);
    // Store the end position for break patching
    uint64_t end_pos = da_count(&c->fn->instructions);
    // Patch all break jumps to this position (after the loop)
    for (int i = 0; i < da_count(&loop_ctx.breaks); i++) {
      uint64_t break_jump = da_at(&loop_ctx.breaks, i);
      uint64_t offset = end_pos - break_jump - 1;
      da_at(&c->fn->instructions, break_jump) = offset;
    }
    // Patch all continue jumps to the increment section
    for (int i = 0; i < da_count(&loop_ctx.continues); i++) {
      uint64_t continue_jump = da_at(&loop_ctx.continues, i);
      uint64_t offset = increment_start - continue_jump - 1;
      da_at(&c->fn->instructions, continue_jump) = offset;
    }
    da_free(&loop_ctx.breaks);
    da_free(&loop_ctx.continues);
    // Restore previous loop context
    c->loop = prev_loop;
    quo__compiler_end_scope(c);
    break;
  }
  case QUO_STMT_BREAK: {
    // Emit a jump to the end of the loop (to be patched later)
    uint64_t jump = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
    da_add(&c->loop->breaks, jump);
    break;
  }
  case QUO_STMT_CONTINUE: {
    // Emit a jump to the increment section (to be patched later)
    uint64_t jump = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
    da_add(&c->loop->continues, jump);
    break;
  }
  }
}

QuoCompiler *quo_compiler_new(QuoState *s, const char *name, uint64_t name_len) {
  QuoCompiler *c = quo_alloc(NULL, sizeof(QuoCompiler));
  c->s = s;
  c->fn = quo_function_new(s, name, name_len);
  c->scope_depth = 0;
  c->loop = NULL;

  QuoLocalVariable local = {.depth = 0, .name = {.start = "", .len = 0}};
  da_add(&c->locals, local);

  return c;
}

QuoFn *quo_compiler_compile(QuoCompiler *c, QuoAST ast) {
  for (uint64_t i = 0; i < da_count(&ast); i++) quo__compiler_stmt(c, da_at(&ast, i));
  // Add explicit nil return
  uint64_t nil_idx = quo__function_push_constant(c->fn, &quo_var_new_nil());
  quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
  quo__function_push_instruction(c->fn, nil_idx);
  quo__function_push_instruction(c->fn, QUO_OP_RETURN);
  // Return the compiled function
  QuoFn *function = c->fn;
#ifdef QUO_DEBUG
  quo_debug_function_disassemble(function);
#endif
  return function;
}

void quo_compiler_free(QuoCompiler *c) {
  if (!c) return;
  da_free(&c->locals);
  da_free(&c->declared_globals);
  if (c->loop) {
    da_free(&c->loop->breaks);
    da_free(&c->loop->continues);
  }
  quo_dealloc(c);
}

// ------------------------------ VM ------------------------------ //

static inline QuoVar *quo__vm_stack_top(QuoVM *vm) { return &da_at(&vm->stack, da_count(&vm->stack)); }
static inline void quo__vm_push(QuoVM *vm, QuoVar value) {
  quo_var_ref(&value);
  da_add(&vm->stack, value);
}
static inline QuoVar quo__vm_pop(QuoVM *vm) {
  assert(da_count(&vm->stack) > 0);
  return da_pop(&vm->stack);
}
static inline QuoVar *quo__vm_peek(QuoVM *vm, int distance) { return quo__vm_stack_top(vm) - 1 - distance; }
// Creates a new call frame for a user-defined function
static bool quo__vm_call_fn(QuoVM *vm, QuoFn *fn, uint64_t argc) {
  if (fn->arity != -1 && fn->arity != argc) return false;
  struct QuoCallFrame frame;
  frame.function = fn;
  frame.ip = da_items(&fn->instructions);
  frame.slots_start = da_count(&vm->stack) - argc - 1;
  da_add(&vm->frames, frame);
  return true;
}
// Calls any function (C or user) that's at the correct stack position
// Returns an error QuoVar on failure, nil on success
static QuoVar quo__vm_dispatch_call(QuoVM *vm, QuoObj *func, uint64_t argc) {
#define CLEANUP_STACK()                                                                                                                    \
  for (uint64_t i = 0; i < argc + 1; i++) quo_var_unref(quo__vm_peek(vm, i));                                                              \
  da_count(&vm->stack) -= argc + 1;

  if (func->type == QUO_OBJ_TYPE_CFN) {
    QuoVar *args = quo__vm_peek(vm, argc - 1);
    QuoCFn *cfn = quo_obj_as_cfn(func);
    QuoVar result = cfn->fn(vm->s, argc, args);
    if (result.type == QUO_VAR_TYPE_ERROR) {
      CLEANUP_STACK();
      return quo_var_new_err(result.val_err);
    }
    CLEANUP_STACK();
    quo__vm_push(vm, result);
    return quo_var_new_nil();
  }
  if (func->type == QUO_OBJ_TYPE_FN) {
    if (!quo__vm_call_fn(vm, quo_obj_as_fn(func), argc)) {
      CLEANUP_STACK();
      return quo_var_new_err("Function arity mismatch");
    }
    return quo_var_new_nil();
  }
  CLEANUP_STACK();
  return quo_var_new_err("Attempt to call non-function");

#undef CLEANUP_STACK
}

QuoVM *quo_vm_new(QuoState *s) {
  QuoVM *vm = quo_alloc(NULL, sizeof(QuoVM));
  vm->s = s;
  return vm;
}

QuoVar quo_vm_run(QuoVM *vm, QuoFn *fn) {
#define READ_INST()  (*frame->ip++)
#define READ_CONST() (da_at(&frame->function->constants, READ_INST()))
#define LAST_FRAME() (&da_at(&vm->frames, da_count(&vm->frames) - 1))

  if (vm->s->had_compile_error) return quo_var_new_nil();
  quo__vm_push(vm, quo_var_new_obj(fn));
  quo__vm_call_fn(vm, fn, 0);
  struct QuoCallFrame *frame = LAST_FRAME();

  QuoOP instruction;
  while (true) {
    instruction = READ_INST();

#ifdef QUO_DEBUG
    printf("%-17s", quo__debug_op_str(instruction));
    printf("[ ");
    for (int i = 0; i < da_count(&vm->stack); i++) {
      QuoVar *v = &da_at(&vm->stack, i);
      quo_var_print(v);
      if (i != da_count(&vm->stack) - 1) printf(" | ");
    }
    printf(" ]\n");
#endif

    switch (instruction) {
    case QUO_OP_NOOP: break;
    case QUO_OP_RETURN: {
      QuoVar result = quo__vm_pop(vm);
      // Pop locals (everything above the function at slots_start)
      while (da_count(&vm->stack) > frame->slots_start + 1) {
        QuoVar v = quo__vm_pop(vm);
        quo_var_unref(&v);
      }
      // Pop the function itself
      QuoVar fn = quo__vm_pop(vm);
      quo_var_unref(&fn);
      // Pop this call frame
      da_count(&vm->frames)--;
      if (da_count(&vm->frames) == 0) return result;
      // Push result for parent frame
      quo__vm_push(vm, result);
      frame = LAST_FRAME();
      break;
    }
    case QUO_OP_POP: {
      QuoVar v = quo__vm_pop(vm);
      quo_var_unref(&v);
      break;
    }
    case QUO_OP_CONSTANT: quo__vm_push(vm, READ_CONST()); break;
    case QUO_OP_ARRAY: {
      uint64_t count = READ_INST();
      QuoArr *arr = quo_arr_new();
      QuoVar *base = quo__vm_peek(vm, count - 1); // Pointer to first element
      for (uint64_t i = 0; i < count; i++) quo_arr_push(arr, base[i]);
      // Remove elements from stack
      da_count(&vm->stack) -= count;
      quo__vm_push(vm, quo_var_new_obj(arr));
      break;
    }
    case QUO_OP_DICT: {
      uint64_t pair_count = READ_INST();
      QuoDict *dict = quo_dict_new(); // Create new dictionary
      // Pop key-value pairs in reverse order
      for (int64_t i = pair_count - 1; i >= 0; i--) {
        QuoVar value = quo__vm_pop(vm);
        QuoVar key_var = quo__vm_pop(vm);
        if (quo_var_is_str(&key_var)) quo_dict_set(dict, quo_var_as_str(&key_var), &value);
        else return quo_var_new_err("Dictionary keys must be strings");
      }
      quo__vm_push(vm, quo_var_new_obj(dict));
      break;
    }
    case QUO_OP_ADD: {
      QuoVar result = quo_var_add(vm->s, quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_SUB: {
      QuoVar result = quo_var_sub(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_MUL: {
      QuoVar result = quo_var_mul(vm->s, quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_DIV: {
      QuoVar result = quo_var_div(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_MOD: {
      QuoVar result = quo_var_mod(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_NEGATE: {
      QuoVar *value = quo__vm_peek(vm, 0);
      QuoVar result = quo_var_neg(value);
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      *value = result;
      break;
    }
    case QUO_OP_NOT: {
      QuoVar *value = quo__vm_peek(vm, 0);
      QuoVar result = quo_var_not(value);
      if (quo_var_is_err(&result)) return quo_var_new_err(result.val_err);
      *value = result;
      break;
    }
    case QUO_OP_EQ: {
      bool res = quo_var_eq(vm->s, quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_NEQ: {
      bool res = !quo_var_eq(vm->s, quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_GT: {
      bool res = quo_var_cmp(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0)) > 0;
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_LT: {
      bool res = quo_var_cmp(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0)) < 0;
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_GTEQ: {
      bool res = quo_var_cmp(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0)) >= 0;
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_LTEQ: {
      bool res = quo_var_cmp(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0)) <= 0;
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_GET_LOCAL: {
      uint64_t slot = READ_INST();
      quo__vm_push(vm, da_at(&vm->stack, frame->slots_start + slot));
      break;
    }
    case QUO_OP_SET_LOCAL: {
      uint64_t slot = READ_INST();
      QuoVar *slot_ptr = &da_at(&vm->stack, frame->slots_start + slot);
      quo_var_unref(slot_ptr);
      *slot_ptr = *quo__vm_peek(vm, 0);
      quo_var_ref(slot_ptr);
      break;
    }
    case QUO_OP_GET_GLOBAL: {
      QuoVar name_var = READ_CONST();
      QuoVar value;
      if (quo_ht_get(&frame->function->state->globals, quo_var_as_str(&name_var), &value)) quo__vm_push(vm, value);
      else return quo_var_new_err("Undefined variable");
      break;
    }
    case QUO_OP_SET_GLOBAL: {
      QuoVar name_var = READ_CONST();
      QuoVar old_value;
      if (quo_ht_get(&frame->function->state->globals, quo_var_as_str(&name_var), &old_value)) quo_var_unref(&old_value);
      QuoVar *value = quo__vm_peek(vm, 0);
      quo_var_ref(value);
      quo_ht_set(&frame->function->state->globals, quo_var_as_str(&name_var), value);
      da_pop(&vm->stack);
      break;
    }
    case QUO_OP_MEMBER_ACCESS: {
      // Stack: [object] [field_name]
      QuoVar field_name = quo__vm_pop(vm);
      // Stack: [object]
      QuoVar object = *quo__vm_peek(vm, 0);
      if (quo_var_is_module(&object)) {
        // Look up in module exports
        QuoModule *module = quo_var_as_module(&object);
        QuoStr *field_str = quo_var_as_str(&field_name);
        QuoVar value;
        if (quo_ht_get(&module->state->globals, field_str, &value)) {
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = value;
          quo_var_ref(quo__vm_peek(vm, 0));
        } else {
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = quo_var_new_nil();
        }
      } else if (quo_var_is_dict(&object)) {
        // Look up field in dict
        QuoVar value;
        if (quo_ht_get(&quo_obj_as_dict(object.val_obj)->dict, quo_var_as_str(&field_name), &value)) {
          // Replace object with value
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = value;
          quo_var_ref(quo__vm_peek(vm, 0));
        } else {
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = quo_var_new_nil();
        }
      } else {
        // TODO fix check first
        // Look up method in array method table
        QuoObj *method = quo__method_lookup(vm->s, &object, quo_var_as_str(&field_name));
        if (method) {
          // Return the method (as a function value)
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = quo_var_new_obj(method);
          quo_var_ref(quo__vm_peek(vm, 0));
        }
      }
      quo_var_unref(&field_name);
      break;
    }
    case QUO_OP_SET_MEMBER: {
      // Stack: [dict] [field_name] [value]
      QuoVar value = quo__vm_pop(vm);
      QuoVar field_name = quo__vm_pop(vm);
      QuoVar dict = *quo__vm_peek(vm, 0);
      if (!quo_var_is_dict(&dict)) return quo_var_new_err("Setting field on non-dictionary");
      quo_dict_set(quo_obj_as_dict(dict.val_obj), quo_var_as_str(&field_name), &value);
      quo_var_unref(&field_name);
      break;
    }
    case QUO_OP_JUMP: {
      uint64_t offset = READ_INST();
      frame->ip += offset;
      break;
    }
    case QUO_OP_JUMP_IF_FALSE: {
      uint64_t offset = READ_INST();
      QuoVar *condition = quo__vm_peek(vm, 0);
      if (!quo_var_is_true(condition)) frame->ip += offset;
      break;
    }
    case QUO_OP_LOOP: {
      uint64_t offset = READ_INST();
      frame->ip -= offset;
      break;
    }
    case QUO_OP_CALL: {
      uint64_t argc = READ_INST();
      QuoVar *callee = quo__vm_peek(vm, argc);
      if (callee->type != QUO_VAR_TYPE_OBJ) return quo_var_new_err("Attempt to call non-function");
      // Save the function type before dispatch (which may clean up the stack)
      QuoObjType func_type = callee->val_obj->type;
      QuoVar result = quo__vm_dispatch_call(vm, callee->val_obj, argc);
      if (result.type == QUO_VAR_TYPE_ERROR) return result;
      // Use the saved type instead of the now-invalid callee pointer
      if (func_type == QUO_OBJ_TYPE_FN) frame = LAST_FRAME();
      break;
    }
    case QUO_OP_METHOD_CALL: {
      uint64_t argc = READ_INST();
      // Stack: [object, method_name, arg1, arg2, ...]
      QuoVar method_name = *quo__vm_peek(vm, argc);
      QuoVar *object = quo__vm_peek(vm, argc + 1);
      QuoObj *method = NULL;
      bool is_method = false; // Whether method takes 'self' as first arg

      // First lookup for type methods
      method = quo__method_lookup(vm->s, object, quo_var_as_str(&method_name));
      if (method) {
        is_method = true; // Type methods take 'self'
      }

      // Look up in module globals (these are regular functions)
      if (!method && quo_var_is_module(object)) {
        QuoModule *mod = quo_var_as_module(object);
        QuoVar method_var;
        bool found = quo_ht_get(&mod->state->globals, quo_var_as_str(&method_name), &method_var);
        if (found && method_var.type == QUO_VAR_TYPE_OBJ && method_var.val_obj) {
          method = method_var.val_obj;
          is_method = false; // Module functions don't take 'self'
        }
      }

      // Look up in object's own dict (for namespaced functions like io.println)
      if (!method && quo_var_is_dict(object)) {
        QuoDict *dict = quo_obj_as_dict(object->val_obj);
        QuoVar method_var;
        bool found = quo_ht_get(&dict->dict, quo_var_as_str(&method_name), &method_var);
        if (found && method_var.type == QUO_VAR_TYPE_OBJ && method_var.val_obj) {
          method = method_var.val_obj;
          is_method = true; // Dict functions take 'self'
        }
      }

      if (!method) {
        for (uint64_t i = 0; i < argc + 2; i++) quo_var_unref(quo__vm_peek(vm, i));
        da_count(&vm->stack) -= argc + 2;
        return quo_var_new_err("Method not found");
      }

      // Save the method type before manipulating the stack
      QuoObjType method_type = method->type;

      // Replace method_name with function object
      quo_var_unref(quo__vm_peek(vm, argc));             // Unref method_name string
      *quo__vm_peek(vm, argc) = quo_var_new_obj(method); // Replace with function
      quo_var_ref(quo__vm_peek(vm, argc));

      if (is_method) {
        // Stack: [object, function, arg1, arg2, ...]
        // Swap object and function to get: [function, object, arg1, arg2, ...]
        QuoVar tmp = *quo__vm_peek(vm, argc);
        *quo__vm_peek(vm, argc) = *quo__vm_peek(vm, argc + 1);
        *quo__vm_peek(vm, argc + 1) = tmp;
        // Call with argc+1 to include 'self' as first argument
        QuoVar result = quo__vm_dispatch_call(vm, method, argc + 1);
        if (result.type == QUO_VAR_TYPE_ERROR) return result;
        if (method_type == QUO_OBJ_TYPE_FN) frame = LAST_FRAME();
      } else {
        // For module/dict functions, just remove the object and call normally
        // Stack: [object, function, arg1, arg2, ...]
        // Remove object (shift everything down)
        QuoVar object_var = *quo__vm_peek(vm, argc + 1);
        quo_var_unref(&object_var);
        // Move function and args down one slot
        for (uint64_t i = argc + 1; i > 0; i--) { *quo__vm_peek(vm, i) = *quo__vm_peek(vm, i - 1); }
        da_count(&vm->stack)--; // Remove one slot
        // Stack: [function, arg1, arg2, ...]
        QuoVar result = quo__vm_dispatch_call(vm, method, argc);
        if (result.type == QUO_VAR_TYPE_ERROR) return result;
        if (method_type == QUO_OBJ_TYPE_FN) frame = LAST_FRAME();
      }
      break;
    }
    }
  }

  return quo_var_new_num(0);

#undef LAST_FRAME
#undef READ_INST
#undef READ_CONST
}

void quo_vm_free(QuoVM *vm) {
  if (!vm) return;
  for (uint64_t i = 0; i < da_count(&vm->stack); i++) quo_var_unref(&da_at(&vm->stack, i));
  da_free(&vm->stack);
  da_free(&vm->frames);
  quo_dealloc(vm);
}

// -------------------- DEBUG -------------------- //

void quo_debug_expression_print(QuoExpr *expr, int indent) {
  if (!expr) return;
  for (int i = 0; i < indent; i++) printf("  ");
  switch (expr->type) {
  case QUO_EXPR_LITERAL: printf("LITERAL: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->token)); break;
  case QUO_EXPR_ARRAY:
    printf("ARRAY:\n");
    for (int i = 0; i < expr->array.elements.count; i++) quo_debug_expression_print(da_at(&expr->array.elements, i), indent + 1);
    break;
  case QUO_EXPR_DICT:
    printf("DICT:\n");
    for (int i = 0; i < expr->dict.pairs.count; i++) {
      QuoExprDictPair pair = da_at(&expr->dict.pairs, i);
      for (int j = 0; j < indent + 1; j++) printf("  ");
      printf("KEY:\n");
      quo_debug_expression_print(pair.key, indent + 2);
      for (int j = 0; j < indent + 1; j++) printf("  ");
      printf("VALUE:\n");
      quo_debug_expression_print(pair.value, indent + 2);
    }
    break;
  case QUO_EXPR_VARIABLE: printf("VAR: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->token)); break;
  case QUO_EXPR_UNARY:
    printf("UNARY: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->unary.op));
    quo_debug_expression_print(expr->unary.expr, indent + 1);
    break;
  case QUO_EXPR_BINARY:
    printf("BINARY: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->binary.op));
    quo_debug_expression_print(expr->binary.left, indent + 1);
    quo_debug_expression_print(expr->binary.right, indent + 1);
    break;
  case QUO_EXPR_GROUPING:
    printf("GROUP:\n");
    quo_debug_expression_print(expr->unary.expr, indent + 1);
    break;
  case QUO_EXPR_CALL:
    printf("CALL:\n");
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("CALLEE:\n");
    quo_debug_expression_print(expr->call.callee, indent + 2);
    if (da_count(&expr->call.arguments) > 0) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("ARGUMENTS:\n");
      for (int i = 0; i < da_count(&expr->call.arguments); i++) {
        for (int j = 0; j < indent + 2; j++) printf("  ");
        printf("ARG %d:\n", i);
        quo_debug_expression_print(da_at(&expr->call.arguments, i), indent + 3);
      }
    }
    break;
  case QUO_EXPR_ASSIGN:
    printf("ASSIGN: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->token)); // Print the actual operator
    quo_debug_expression_print(expr->assign.target, indent + 1);
    quo_debug_expression_print(expr->assign.value, indent + 1);
    break;
  case QUO_EXPR_TERNARY:
    printf("TERNARY:\n");
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("COND:\n");
    quo_debug_expression_print(expr->ternary.condition, indent + 2);
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("THEN:\n");
    quo_debug_expression_print(expr->ternary.then_expr, indent + 2);
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("ELSE:\n");
    quo_debug_expression_print(expr->ternary.else_expr, indent + 2);
    break;
  case QUO_EXPR_MEMBER_ACCESS:
    printf("MEMBER ACCESS:\n");
    quo_debug_expression_print(expr->member_access.object, indent + 1);
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("MEMBER: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->member_access.member));
    break;
  case QUO_EXPR_FUNCTION:
    printf("FUNCTION:\n");
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("NAME: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(expr->function.name));
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("PARAMS:\n");
    for (int i = 0; i < da_count(&expr->function.parameters); i++) {
      QuoToken param = da_at(&expr->function.parameters, i);
      for (int j = 0; j < indent + 2; j++) printf("  ");
      printf(QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(param));
    }
    break;
  }
}

void quo_debug_statement_print(QuoStmt *stmt, int indent) {
  if (!stmt) return;
  for (int i = 0; i < indent; i++) printf("  ");
  switch (stmt->type) {
  case QUO_STMT_IMPORT: printf("IMPORT: %s\n", stmt->import->state->cwd); break;
  case QUO_STMT_VAR_DECL:
    printf("VAR_DECL: " QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(stmt->var_decl.name));
    if (stmt->var_decl.initializer) quo_debug_expression_print(stmt->var_decl.initializer, indent + 1);
    break;
  case QUO_STMT_EXPRESSION:
    printf("EXPR_STMT:\n");
    quo_debug_expression_print(stmt->expression, indent + 1);
    break;
  case QUO_STMT_RETURN:
    printf("RETURN:\n");
    if (stmt->expression) {
      quo_debug_expression_print(stmt->expression, indent + 1);
    } else {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("void\n");
    }
    break;
  case QUO_STMT_IF:
    printf("IF:\n");
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("COND:\n");
    quo_debug_expression_print(stmt->if_stmt.condition, indent + 2);
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("THEN:\n");
    quo_debug_statement_print(stmt->if_stmt.then_branch, indent + 2);
    if (stmt->if_stmt.else_branch) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("ELSE:\n");
      quo_debug_statement_print(stmt->if_stmt.else_branch, indent + 2);
    }
    break;
  case QUO_STMT_LOOP:
    printf("LOOP:\n");
    if (stmt->loop.initializer) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("INIT:\n");
      quo_debug_statement_print(stmt->loop.initializer, indent + 2);
    }
    if (stmt->loop.condition) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("COND:\n");
      quo_debug_expression_print(stmt->loop.condition, indent + 2);
    }
    if (stmt->loop.increment) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("INCR:\n");
      quo_debug_statement_print(stmt->loop.increment, indent + 2);
    }
    for (int i = 0; i < indent + 1; i++) printf("  ");
    if (da_count(&stmt->loop.body->block) == 0) break;
    printf("BODY:\n");
    quo_debug_statement_print(stmt->loop.body, indent + 2);
    break;
  case QUO_STMT_BREAK: printf("BREAK\n"); break;
  case QUO_STMT_CONTINUE: printf("CONTINUE\n"); break;
  case QUO_STMT_BLOCK:
    if (da_count(&stmt->block) == 0) break;
    printf("BLOCK:\n");
    for (int i = 0; i < da_count(&stmt->block); i++) quo_debug_statement_print(da_at(&stmt->block, i), indent + 1);
    break;
  }
}

void quo_debug_ast_print(QuoAST *ast) {
  printf("\n============= AST: ===============\n\n");
  for (int i = 0; i < da_count(ast); i++) {
    quo_debug_statement_print(da_at(ast, i), 0);
    printf("\n");
  }
  printf("==================================\n");
}

void quo_debug_function_disassemble(QuoFn *fn) {
  int len = printf("========== FUNCTION '%s' DISSASSEMBLY ==========\n", fn->name->data);
  printf("CONSTANTS (%d):\n", da_count(&fn->constants));
  for (int i = 0; i < da_count(&fn->constants); i++) {
    printf("%d: ", i);
    quo_var_print(&da_at(&fn->constants, i));
    printf("\n");
  }
  printf("\n");
  printf("INSTRUCTIONS (%d):\n", da_count(&fn->instructions));
  for (int i = 0; i < da_count(&fn->instructions);) i = quo_debug_instruction_disassemble(fn, i);
  for (int i = 0; i < len; i++) printf("=");
  printf("\n");
}

static const char *quo__debug_op_str(QuoOP op) {
  switch (op) {
  case QUO_OP_NOOP: return "NOOP";
  case QUO_OP_RETURN: return "RETURN";
  case QUO_OP_POP: return "POP";
  case QUO_OP_CONSTANT: return "CONSTANT";
  case QUO_OP_ARRAY: return "ARRAY";
  case QUO_OP_DICT: return "DICT";
  case QUO_OP_NEGATE: return "NEGATE";
  case QUO_OP_NOT: return "NOT";
  case QUO_OP_ADD: return "ADD";
  case QUO_OP_SUB: return "SUB";
  case QUO_OP_MUL: return "MUL";
  case QUO_OP_DIV: return "DIV";
  case QUO_OP_MOD: return "MOD";
  case QUO_OP_EQ: return "EQ";
  case QUO_OP_NEQ: return "NEQ";
  case QUO_OP_GT: return "GT";
  case QUO_OP_LT: return "LT";
  case QUO_OP_GTEQ: return "GTEQ";
  case QUO_OP_LTEQ: return "LTEQ";
  case QUO_OP_GET_LOCAL: return "GET_LOCAL";
  case QUO_OP_SET_LOCAL: return "SET_LOCAL";
  case QUO_OP_GET_GLOBAL: return "GET_GLOBAL";
  case QUO_OP_SET_GLOBAL: return "SET_GLOBAL";
  case QUO_OP_CALL: return "CALL";
  case QUO_OP_METHOD_CALL: return "METHOD_CALL";
  case QUO_OP_JUMP: return "JUMP";
  case QUO_OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
  case QUO_OP_LOOP: return "LOOP";
  case QUO_OP_MEMBER_ACCESS: return "OP_MEMBER_ACCESS";
  case QUO_OP_SET_MEMBER: return "OP_SET_MEMBER";
  }
}

int quo_debug_instruction_disassemble(QuoFn *fn, int offset) {
  printf("%04d: ", offset);
  QuoOP instruction = da_at(&fn->instructions, offset);
  offset++;
  switch (instruction) {
  case QUO_OP_CONSTANT: {
    uint64_t index = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld (", quo__debug_op_str(instruction), index);
    quo_var_print(&da_at(&fn->constants, index));
    printf(")\n");
    break;
  }
  case QUO_OP_ARRAY: {
    uint64_t count = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld\n", quo__debug_op_str(instruction), count);
    break;
  }
  case QUO_OP_DICT: {
    uint64_t pair_count = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld\n", quo__debug_op_str(instruction), pair_count);
    break;
  }
  case QUO_OP_GET_LOCAL:
  case QUO_OP_SET_LOCAL: {
    uint64_t index = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld\n", quo__debug_op_str(instruction), index);
    break;
  }
  case QUO_OP_GET_GLOBAL:
  case QUO_OP_SET_GLOBAL: {
    uint64_t index = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld (", quo__debug_op_str(instruction), index);
    quo_var_print(&da_at(&fn->constants, index));
    printf(")\n");
    break;
  }
  case QUO_OP_CALL: {
    uint64_t arity = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld\n", quo__debug_op_str(instruction), arity);
    break;
  }
  case QUO_OP_METHOD_CALL: {
    uint64_t arity = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld\n", quo__debug_op_str(instruction), arity);
    break;
  }
  case QUO_OP_JUMP:
  case QUO_OP_JUMP_IF_FALSE: {
    uint64_t jump_offset = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld -> %ld\n", quo__debug_op_str(instruction), jump_offset, offset + jump_offset - 1);
    break;
  }
  case QUO_OP_LOOP: {
    uint64_t jump_offset = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %ld -> %ld\n", quo__debug_op_str(instruction), jump_offset, offset - jump_offset - 1);
    break;
  }
  default: printf("%s\n", quo__debug_op_str(instruction)); break;
  }
  return offset;
}

#endif // QUO_IMPLEMENTATION
#endif // QUO_H
