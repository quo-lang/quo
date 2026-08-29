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
#include <threads.h>
#include <time.h>

#ifdef _WIN32
#include <wincrypt.h>
#include <windows.h>
#define RTLD_LAZY             0
#define dlopen(path, flags)   LoadLibraryA(path)
#define dlsym(handle, symbol) GetProcAddress((HMODULE)(handle), (symbol))
#define dlclose(handle)       (FreeLibrary((HMODULE)(handle)) ? 0 : -1)
#define dlerror()             "Windows error"
#else
#include <dlfcn.h>
#include <sys/time.h>
#include <unistd.h>
#endif

// ------------------------------------------------------------------------------------------ //
//                                           MACROS                                           //
// ------------------------------------------------------------------------------------------ //

#define QUO_UNUSED(x) ((void)(x))

// ------------------------------------------------------------------------------------------ //
//                                         DEFINITIONS                                        //
// ------------------------------------------------------------------------------------------ //

// --- MEMORY --- //

void *quo_alloc(void *ptr, size_t size);
void quo_dealloc(void *ptr);

// Memory arena
typedef struct {
  size_t size;     // Bytes currently allocated
  size_t capacity; // Total bytes available
  size_t offset;   // Current allocation pointer
  char *data;
} QuoArena;

static inline QuoArena quo_arena_create(size_t capacity) {
  QuoArena arena = {0};
  arena.capacity = capacity;
  arena.data = calloc(1, capacity);
  assert(arena.data != NULL);
  return arena;
}

static inline void *quo_arena_alloc(QuoArena *arena, size_t size) {
  assert(arena != NULL && size > 0);
  if (arena->offset > SIZE_MAX - size) return NULL; // Check for overflow in offset + size
  // Check if we need more space
  if (arena->offset + size > arena->capacity) {
    size_t new_capacity = arena->capacity * 2;
    // Handle overflow and minimum size
    if (new_capacity < arena->capacity) new_capacity = arena->capacity + size;
    if (new_capacity < arena->offset + size) new_capacity = arena->offset + size;
    // Ensure we grow by at least 1
    if (new_capacity <= arena->capacity) new_capacity = arena->capacity + 1;
    void *new_data = realloc(arena->data, new_capacity);
    if (!new_data) return NULL;
    arena->data = new_data;
    arena->capacity = new_capacity;
  }
  void *ptr = arena->data + arena->offset;
  arena->offset += size;
  arena->size += size;
  return ptr;
}

static inline void quo_arena_reset(QuoArena *arena) {
  assert(arena != NULL);
  arena->size = arena->offset = 0;
}

static inline void quo_arena_destroy(QuoArena *arena) {
  assert(arena != NULL);
  if (arena->data) {
    memset(arena->data, 0, arena->capacity);
    free(arena->data);
    arena->size = arena->capacity = arena->offset = 0;
    arena->data = NULL;
  }
}

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
  for (size_t i = 0; i < (size_t)(len); ++i) da_add(sb, (str)[i])
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
    QUO_TT_WHILE,    // while
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
typedef struct QuoModule QuoModule;

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
      QuoExpr *condition;
      QuoStmt *body;
    } loop;              // Loop
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
  char *data;   // NULL-terminated string
  int len;      // Byte length
  int char_len; // Character count (UTF-8)
  int hash;     // Hash
  bool interned;
} QuoStr;

// Creates a new interned QuoStr from a C string. Process escape sequences.
// Pass `len` as `-1` for NULL-terminated strings.
QuoStr *quo_str_new_interned(const char *str, int len);
// Create not interned string
QuoStr *quo_str_new(const char *str, int len);
// Creates a new QuoStr from raw data (no escape processing).
QuoStr *quo_str_new_raw(const char *str, int len);

static inline bool quo_str_eq(QuoStr *a, QuoStr *b) {
  if (a == b) return true;
  if (a->hash != b->hash) return false;
  if (a->len != b->len) return false;
  return memcmp(a->data, b->data, a->len) == 0;
}

static inline QuoStr *quo_obj_as_str(const QuoObj *o) { return (QuoStr *)o; }

typedef struct {
  QuoObj obj;
  da(QuoVar) arr;
} QuoArr;

QuoArr *quo_arr_new(void);
void quo_arr_push(QuoArr *arr, QuoVar value);
QuoVar quo_arr_pop(QuoArr *arr);
QuoVar quo_arr_get(QuoArr *arr, int index);
void quo_arr_set(QuoArr *arr, int index, QuoVar value);
int quo_arr_len(QuoArr *arr);

static inline bool quo_obj_is_arr(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_ARR; }
static inline QuoArr *quo_obj_as_arr(const QuoObj *o) { return (QuoArr *)o; }

typedef struct QuoDict {
  QuoObj obj;
  QuoHashTable dict;
} QuoDict;

QuoDict *quo_dict_new();
bool quo_dict_has(QuoDict *dict, QuoStr *key);
QuoVar quo_dict_get(QuoDict *dict, QuoStr *key);
bool quo_dict_set(QuoDict *dict, QuoStr *key, QuoVar *value);
bool quo_dict_del(QuoDict *dict, QuoStr *key);

static inline QuoDict *quo_obj_as_dict(const QuoObj *o) { return (QuoDict *)o; }

typedef struct {
  QuoObj obj;
  QuoStr *name;
  int arity; // -1 for variadic
  da(int) instructions;
  da(QuoVar) constants;
  QuoModule *m;
} QuoFn;

QuoFn *quo_function_new(QuoModule *m, const char *name, int name_len);

static inline QuoFn *quo_obj_as_fn(const QuoObj *o) { return (QuoFn *)o; }

typedef QuoVar (*QuoCFunctionPtr)(QuoModule *m, int argc, QuoVar *argv);
typedef struct {
  QuoObj obj;
  QuoStr *name;
  QuoCFunctionPtr fn;
} QuoCFn;

QuoCFn *quo_cfunction_new(const char *name, int name_len, QuoCFunctionPtr ptr);

static inline QuoCFn *quo_obj_as_cfn(const QuoObj *o) { return (QuoCFn *)o; }

typedef void (*QuoModuleCleanupFn)(QuoModule *);
typedef bool (*QuoModuleInitFn)(QuoModule *);

typedef struct QuoModule {
  QuoObj obj;   // Base class
  QuoStr *name; // Module name

  QuoHashTable globals;

  // --- PARSER --- //
  int pos, line, column;      // Lexer position
  char *cwd;                  // Working directory
  char *file_path;            // File path
  char *source;               // Source code
  QuoToken current, previous; // Parser tokens
  da(QuoTokenList) scopes;    // Scope tracking
  int loop_count;             // Loop tracking
  bool had_compile_error;
  QuoStmtList ast; // Abstract syntax tree
  QuoFn *fn;       // Compiled main function

  QuoModuleCleanupFn cleanup_fn;
} QuoModule;

// Create new QuoModule.
// Arguments:
//  - `cwd`: Current working directory.
//  - `file_path`: Path of the file relative to the `cwd`.
//  - `source`: Source code of the module.
//  - `cleanup_fn`: Function to call when the module is freed.
QuoModule *quo_module_new(const char *cwd, const char *file_path, const char *source, QuoModuleCleanupFn cleanup_fn);
void quo_module_register_var(QuoModule *m, QuoStr *name, QuoVar value);
// Register a global C function for accessing in quo.
// `name_len` is the length of `name`. Pass `-1` for NULL-terminated strings.
void quo_module_register_cfn(QuoModule *m, const char *name, int name_len, QuoCFunctionPtr fn);

// Register a type for use in quo.
// `name_len` is the length of `name`. Pass `-1` for NULL-terminated strings.
QuoObj *quo_type_register(const char *name, size_t size);
// Get an instance of a type registered by `quo_type_register()`.
QuoObj *quo_type_get_instance(const char *name);
// Add a method/field to a type registered by `quo_module_register_type()`.
void quo_type_add(QuoObj *type, QuoStr *name, QuoVar value);
// Convenience function for adding a C function to a type.
void quo_type_add_cfn(QuoObj *type, const char *name, int name_len, QuoCFunctionPtr fn);

QuoVar quo_module_run(QuoModule *m);

static inline bool quo_obj_is_module(const QuoObj *o) { return o->type == QUO_OBJ_TYPE_MODULE; }
static inline QuoModule *quo_obj_as_module(const QuoObj *o) { return (QuoModule *)o; }
static inline bool quo_var_is_module(const QuoVar *v) { return quo_var_is_obj(v) && v->val_obj->type == QUO_OBJ_TYPE_MODULE; }
static inline QuoModule *quo_var_as_module(const QuoVar *v) { return quo_obj_as_module(v->val_obj); }

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
QuoVar quo_var_to_str(QuoVar *v);
int quo_var_len(QuoVar *v);
int quo_var_print(QuoVar *v);

void quo_init(const char *cwd);
void quo_cleanup();

// ------------------------------------------------------------------------------------------ //
//                                     QUO IMPLEMENTATION                                     //
// ------------------------------------------------------------------------------------------ //

#ifdef QUO_IMPLEMENTATION

static QuoStmt *quo__stmt_new(enum QuoStmtType type);
static void quo__stmt_free(QuoStmt *stmt);
static inline bool quo__parser_check(QuoModule *m, enum QuoTokenType type);
static inline void quo__parser_advance(QuoModule *m);
static QuoStmt *quo__parser_stmt(QuoModule *m);

// --- GLOBALS --- //

static QuoHashTable quo__types = {0};
static QuoHashTable quo__interned_strings = {0};
static QuoHashTable quo__imported_modules = {0};

static QuoHashTable quo__builtin_methods = {0};
static QuoHashTable quo__str_methods = {0};
static QuoHashTable quo__arr_methods = {0};
static QuoHashTable quo__dict_methods = {0};

// Operations codes
typedef enum {
  QUO_OP_NOOP,
  QUO_OP_RETURN,
  QUO_OP_POP,
  QUO_OP_CONSTANT,
  QUO_OP_ARRAY,
  QUO_OP_DICT,
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

typedef struct QuoCompiler {
  QuoModule *m;
  // Output function
  QuoFn *fn;
  // Locals tracking
  da(struct QuoLocalVariable {
    QuoToken name;
    int depth;
  }) locals;
  QuoTokenList declared_globals;
  int scope_depth;
  // Loop context for break/continue
  struct QuoLoopContext {
    int start;
    da(int) breaks;
    da(int) continues;
  } *loop;
} QuoCompiler;

QuoCompiler *quo_compiler_new(QuoModule *m, const char *name, int name_len);
QuoFn *quo_compiler_compile(QuoCompiler *c, QuoStmtList ast);
void quo_compiler_free(QuoCompiler *c);

typedef struct QuoVM {
  QuoModule *m;
  da(QuoVar) stack;
  da(struct QuoCallFrame {
    QuoFn *function;
    int *ip;
    int slots_start;
  }) frames;
} QuoVM;

QuoVM *quo_vm_new(QuoModule *m);
QuoVar quo_vm_run(QuoVM *vm, QuoFn *fn);
void quo_vm_free(QuoVM *vm);

// --- UTILS --- //

#define quo__static_array_size(arr) ((int)(sizeof(arr) / sizeof(arr[0])))

// --- LEXER --- //

// Format string for QuoToken, used with QUO_TOKEN_ARG macro:
// Example: printf(QUO_TOKEN_FMT "\n", QUO_TOKEN_ARG(token))
#define QUO_TOKEN_FMT    "%.*s"
#define QUO_TOKEN_ARG(t) (t).len, (t).start

bool quo_tokens_eq(QuoToken t1, QuoToken t2);

// --- DEBUG --- //

void quo_debug_expression_print(QuoExpr *expr, int indent);
void quo_debug_statement_print(QuoStmt *stmt, int indent);
void quo_debug_ast_print(QuoStmtList *ast);
void quo_debug_function_disassemble(QuoFn *fn);
static const char *quo__debug_op_str(QuoOP op);
int quo_debug_instruction_disassemble(QuoFn *fn, int offset);

// -------------------- MEMORY -------------------- //

void *quo_alloc(void *ptr, size_t size) {
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

static inline bool quo_strsuffix(const char *str, const char *suffix) {
  if (str == NULL) return false;
  if (suffix == NULL) return true;
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (str_len < suffix_len) return false;
  return memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

static inline bool quo_strprefix(const char *str, const char *prefix) {
  if (str == NULL) return false;
  if (prefix == NULL) return true;
  size_t str_len = strlen(str);
  size_t prefix_len = strlen(prefix);
  if (str_len < prefix_len) return false;
  return memcmp(str, prefix, prefix_len) == 0;
}

char *quo_strndup(const char *str, int len) {
  char *ptr = quo_alloc(NULL, len + 1);
  memcpy(ptr, str, len);
  ptr[len] = '\0';
  return ptr;
}

char *quo_strdup(const char *str) { return quo_strndup(str, strlen(str)); }

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

double quo_strtod(const char *s, int len) {
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
static int quo__utf8_char_len(unsigned char c) {
  if (c < 0x80) return 1; // 0xxxxxxx → 1 byte (ASCII)
  if (c < 0xC0) return 0; // 10xxxxxx → continuation byte (invalid as first byte)
  if (c < 0xE0) return 2; // 110xxxxx → 2 bytes
  if (c < 0xF0) return 3; // 1110xxxx → 3 bytes
  if (c < 0xF8) return 4; // 11110xxx → 4 bytes
  return 0;               // Invalid
}

// Get UTF-8 character count
static int quo__utf8_strlen(const char *str, int byte_len) {
  int char_count = 0;
  const char *a = str;
  const char *end = a + byte_len;
  while (a < end) {
    int clen = quo__utf8_char_len((unsigned char)*a);
    if (clen == 0) clen = 1; // Skip invalid bytes
    a += clen;
    char_count++;
  }
  return char_count;
}

// Get the Nth UTF-8 character from a string
static const char *quo__utf8_index(const char *str, int len, int index) {
  int current = 0;
  const char *a = str;
  const char *end = str + len;
  while (a < end && current < index) {
    int char_len = quo__utf8_char_len((unsigned char)*a);
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

// static char *quo_file_name(const char *path) {
//   if (!path) return NULL;
//   // Remove the directory path from the file name
//   const char *last_slash = NULL;
//   for (const char *p = path; *p; p++)
//     if (*p == '/' || *p == '\\') last_slash = p;
//   // If no slash found, use the entire path as filename
//   const char *filename = last_slash ? last_slash + 1 : path;
//   // Remove extension - only look for dots in the filename part
//   const char *last_dot = NULL;
//   for (const char *p = filename; *p; p++)
//     if (*p == '.') last_dot = p;
//   if (!last_dot) return quo_strdup(filename);
//   return quo_strndup(filename, last_dot - filename);
// }

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

bool quo__is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
bool quo__is_digit(char c) { return c >= '0' && c <= '9'; }
bool quo__is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool quo__is_alphanumeric(char c) { return quo__is_alpha(c) || quo__is_digit(c); }

// ----------------- HASH TABLE ----------------- //

typedef struct QuoHashTableEntry {
  QuoStr *key;
  QuoVar value;
} QuoHashTableEntry;

static QuoHashTableEntry *quo__ht_find_entry(QuoHashTableEntry *entries, int capacity, QuoStr *key) {
  int index = key->hash & (capacity - 1);
  QuoHashTableEntry *tombstone = NULL;
  for (;;) {
    QuoHashTableEntry *entry = &entries[index];
    if (entry->key == NULL) {
      if (quo_var_is_nil(&entry->value)) {
        return tombstone != NULL ? tombstone : entry; // Empty entry
      } else {
        if (tombstone == NULL) tombstone = entry; // We found a tombstone
      }
    } else if (quo_str_eq(entry->key, key)) return entry; // We found the key
    index = (index + 1) & (capacity - 1);
  }
}

static void quo__ht_adjust_capacity(QuoHashTable *t, int capacity) {
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

int quo_hash(const char *str, int len) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < len; i++) {
    hash ^= (unsigned char)str[i];
    hash *= 16777619u;
  }
  return (int)hash;
}

void quo_ht_init(QuoHashTable *t) {
  t->count = t->capacity = 0;
  t->items = NULL;
}

bool quo_ht_set(QuoHashTable *t, QuoStr *key, QuoVar value) {
  if (t->count + 1 > t->capacity * 0.75) quo__ht_adjust_capacity(t, t->capacity * 2);
  QuoHashTableEntry *entry = quo__ht_find_entry(t->items, t->capacity, key);
  bool new = entry->key == NULL;
  if (new && quo_var_is_nil(&entry->value)) t->count++;
  entry->key = key;
  entry->value = value;
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
  for (int i = 0; i < t->capacity; i++)
    if (t->items[i].key) quo_var_unref(&t->items[i].value);
  da_free(t);
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
  if (!obj) return;
  if (quo_obj_is_str(obj) && quo_obj_as_str(obj)->interned) return; // Interned strings are managed by the module
  obj->ref_count--;
  if (obj->ref_count > 0) return;
  switch (obj->type) {
  case QUO_OBJ_TYPE_ARR: {
    QuoArr *arr = quo_obj_as_arr(obj);
    for (int i = 0; i < da_count(&arr->arr); i++) quo_var_unref(&da_at(&arr->arr, i));
    da_free(&arr->arr);
    break;
  }
  case QUO_OBJ_TYPE_DICT: {
    QuoDict *dict = quo_obj_as_dict(obj);
    for (int i = 0; i < dict->dict.capacity; i++)
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
    QuoModule *m = quo_obj_as_module(obj);
    if (m->cleanup_fn) m->cleanup_fn(m);

    // Free main function first
    if (m->fn) quo_obj_unref((QuoObj *)m->fn);

    // Free globals and method tables
    quo_ht_free(&m->globals);

    // Free parser structures
    for (int i = 0; i < da_count(&m->ast); i++) quo__stmt_free(da_at(&m->ast, i));
    da_free(&m->ast);
    while (da_count(&m->scopes) > 0) {
      da_free(&da_at(&m->scopes, da_count(&m->scopes) - 1));
      da_count(&m->scopes)--;
    }
    da_free(&m->scopes);

    // Free other allocated memory
    quo_dealloc(m->cwd);
    quo_dealloc(m->source);
    quo_dealloc(m->file_path);
    break;
  }
  case QUO_OBJ_TYPE_STR: quo_dealloc(quo_obj_as_str(obj)->data); break;
  case QUO_OBJ_TYPE_CFN:
  case QUO_OBJ_TYPE_USER: break;
  }
  quo_dealloc(obj);
}

// --- STRING FUNCTIONS --- //

static inline QuoStr *quo__find_string(const char *chars, int length, int hash) {
  if (quo__interned_strings.count == 0) return NULL;
  int index = hash & (quo__interned_strings.capacity - 1);
  while (true) {
    QuoHashTableEntry *entry = &quo__interned_strings.items[index];
    if (entry->key == NULL) {
      if (quo_var_is_nil(&entry->value)) return NULL; // Stop if we find an empty non-tombstone entry
    } else if (entry->key->len == length && entry->key->hash == hash && memcmp(entry->key->data, chars, length) == 0) {
      return entry->key;
    }
    index = (index + 1) & (quo__interned_strings.capacity - 1);
  }
}

QuoStr *quo_str_new_interned(const char *str, int len) {
  if (len < 0) len = strlen(str);
  // Process escape sequences
  char *processed = quo_alloc(NULL, len + 1);
  int out_len = 0;
  for (int i = 0; i < len; i++) {
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
  int hash = quo_hash(processed, out_len);
  QuoStr *existing = quo__find_string(processed, out_len, hash);
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
  string->interned = true;
  quo_ht_set(&quo__interned_strings, string, quo_var_new_nil());
  return string;
}

QuoStr *quo_str_new(const char *str, int len) {
  if (len < 0) len = strlen(str);
  // Process escape sequences
  char *processed = quo_alloc(NULL, len + 1);
  int out_len = 0;
  for (int i = 0; i < len; i++) {
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
  int hash = quo_hash(processed, out_len);

  QuoStr *string = (QuoStr *)quo_obj_new(sizeof(QuoStr));
  string->obj.type = QUO_OBJ_TYPE_STR;
  string->data = quo_strndup(str, len);
  string->len = len;
  string->char_len = quo__utf8_strlen(str, len);
  string->hash = hash;
  string->interned = false;
  return string;
}

QuoStr *quo_str_new_raw(const char *str, int len) {
  if (len < 0) len = strlen(str);
  QuoStr *string = (QuoStr *)quo_obj_new(sizeof(QuoStr));
  string->obj.type = QUO_OBJ_TYPE_STR;
  string->data = quo_strndup(str, len);
  string->len = len;
  string->char_len = quo__utf8_strlen(str, len);
  string->hash = quo_hash(str, len);
  string->interned = false;
  return string;
}

// --- DICTIONARY FUNCTIONS --- //

QuoDict *quo_dict_new() {
  QuoDict *dict = (QuoDict *)quo_obj_new(sizeof(QuoDict));
  dict->obj.type = QUO_OBJ_TYPE_DICT;
  return dict;
}
bool quo_dict_has(QuoDict *dict, QuoStr *key) {
  QuoVar value;
  return quo_ht_get(&dict->dict, key, &value);
}
QuoVar quo_dict_get(QuoDict *dict, QuoStr *key) {
  QuoVar value;
  if (!quo_ht_get(&dict->dict, key, &value)) return quo_var_new_nil();
  return value;
}
bool quo_dict_set(QuoDict *dict, QuoStr *key, QuoVar *value) {
  quo_var_ref(value);
  return quo_ht_set(&dict->dict, key, *value);
}
bool quo_dict_del(QuoDict *dict, QuoStr *key) {
  QuoVar value;
  if (!quo_ht_get(&dict->dict, key, &value)) return false;
  quo_var_unref(&value);
  return quo_ht_del(&dict->dict, key);
}

// --- FN FUNCTIONS --- //

QuoFn *quo_function_new(QuoModule *m, const char *name, int name_len) {
  QuoFn *fn = (QuoFn *)quo_obj_new(sizeof(QuoFn));
  fn->obj.type = QUO_OBJ_TYPE_FN;
  fn->name = quo_str_new_interned(name, name_len);
  fn->arity = -1;
  fn->m = m;
  return fn;
}
static inline void quo__function_push_instruction(QuoFn *f, int instruction) { da_add(&f->instructions, instruction); }
static inline int quo__function_push_constant(QuoFn *f, QuoVar *constant) {
  // Check if constant already exists (for strings only)
  if (quo_var_is_str(constant)) {
    for (int i = 0; i < da_count(&f->constants); i++) {
      QuoVar *existing = &da_at(&f->constants, i);
      if (quo_var_is_str(existing) && existing->val_obj == constant->val_obj) {
        return i; // Return existing index
      }
    }
  }
  // Also check for function constants
  if (quo_var_is_fn(constant)) {
    for (int i = 0; i < da_count(&f->constants); i++) {
      QuoVar *existing = &da_at(&f->constants, i);
      if (quo_var_is_fn(existing) && existing->val_obj == constant->val_obj) return i; // Return existing index
    }
  }
  da_add(&f->constants, *constant);
  return da_count(&f->constants) - 1;
}
// Emit a jump instruction and return its position for later patching
static inline int quo__function_emit_jump(QuoFn *f, QuoOP op) {
  quo__function_push_instruction(f, op);
  quo__function_push_instruction(f, QUO_OP_NOOP); // Placeholder
  return da_count(&f->instructions) - 1;
}
// Patch a jump instruction to point to the current position
static inline void quo__function_patch_jump(QuoFn *f, int jump_pos) {
  int current_pos = da_count(&f->instructions);
  int offset = current_pos - jump_pos - 1;
  da_at(&f->instructions, jump_pos) = offset;
}
// Emit a loop jump instruction (backwards jump)
static inline void quo__function_emit_loop(QuoFn *f, int loop_start) {
  quo__function_push_instruction(f, QUO_OP_LOOP);
  int offset = da_count(&f->instructions) - loop_start + 1;
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
QuoVar quo_arr_get(QuoArr *arr, int index) {
  if (index < 0 || index >= da_count(&arr->arr)) return quo_var_new_nil();
  return da_at(&arr->arr, index);
}
QuoVar quo_arr_pop(QuoArr *arr) {
  if (da_count(&arr->arr) == 0) return quo_var_new_nil();
  return da_pop(&arr->arr);
}
void quo_arr_set(QuoArr *arr, int index, QuoVar value) {
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
int quo_arr_len(QuoArr *arr) { return da_count(&arr->arr); }

// --- CFN FUNCTIONS --- //

QuoCFn *quo_cfunction_new(const char *name, int name_len, QuoCFunctionPtr ptr) {
  QuoCFn *fn = (QuoCFn *)quo_obj_new(sizeof(QuoCFn));
  fn->obj.type = QUO_OBJ_TYPE_CFN;
  fn->obj.name = quo_str_new_interned("cfn", -1);
  fn->name = quo_str_new_interned(name, name_len);
  fn->fn = ptr;
  return fn;
}

// ---------- BUILT-IN MODULES ---------- //

// --- MODULE OS --- //

// Run a system command and return the result
static QuoVar quo__mod_os_system(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("os.system() takes command string");
  return quo_var_new_num(system(quo_var_as_str(&argv[0])->data));
}

static inline void quo__mod_os_init(const char *cwd) {
  QuoModule *m = quo_module_new(cwd, "os", NULL, NULL);
  quo_module_register_cfn(m, "system", -1, quo__mod_os_system);

  const char *name = NULL;
#if defined(__linux__)
  name = "linux";
#elif defined(_WIN32)
  name = "windows";
#elif defined(__APPLE__) && defined(__MACH__)
  name = "macos";
#else
  name = "unknown";
#endif
  quo_module_register_var(m, quo_str_new_interned("name", -1), quo_var_new_obj(quo_str_new_interned(name, -1)));
}

// --- MODULE TIME --- //

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

static inline void quo__mod_time_init(const char *cwd) {
  QuoModule *m = quo_module_new(cwd, "time", NULL, NULL);
  quo_module_register_cfn(m, "sleep", -1, quo__mod_time_sleep);
  quo_module_register_cfn(m, "now", -1, quo__mod_time_now);
  quo_module_register_cfn(m, "clock", -1, quo__mod_time_clock);
}

// --- MODULE BASE64 --- //

static int quo__mod_base64_decode_char(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+' || c == '-') return 62;
  if (c == '/' || c == '_') return 63;
  return -1;
}

static QuoVar quo__mod_base64_encode_impl(const char *input, int len, const char *table) {
  QuoStringBuilder sb = quo_sb_new();
  for (int i = 0; i < len; i += 3) {
    int remaining = len - i;
    unsigned char a = input[i];
    unsigned char b = (remaining > 1) ? input[i + 1] : 0;
    unsigned char c = (remaining > 2) ? input[i + 2] : 0;
    unsigned int triple = (a << 16) | (b << 8) | c;
    da_add(&sb, table[(triple >> 18) & 0x3F]);
    da_add(&sb, table[(triple >> 12) & 0x3F]);
    if (remaining >= 2) da_add(&sb, table[(triple >> 6) & 0x3F]);
    else da_add(&sb, '=');
    if (remaining >= 3) da_add(&sb, table[triple & 0x3F]);
    else da_add(&sb, '=');
  }
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

static QuoVar quo__mod_base64_decode_impl(const char *input, int len) {
  QuoStringBuilder sb = quo_sb_new();
  int padding = 0;
  if (len > 0 && input[len - 1] == '=') padding++;
  if (len > 1 && input[len - 2] == '=') padding++;

  for (int i = 0; i < len; i += 4) {
    unsigned char sextet[4] = {0};
    int valid_sextets = 0;
    for (int j = 0; j < 4 && (i + j) < len; j++) {
      char c = input[i + j];
      if (c == '=') sextet[j] = 0;
      else {
        int val = quo__mod_base64_decode_char(c);
        if (val < 0) {
          quo_sb_free(&sb);
          return quo_var_new_err("Invalid base64 character");
        }
        sextet[j] = val;
        valid_sextets++;
      }
    }
    unsigned int triple = (sextet[0] << 18) | (sextet[1] << 12) | (sextet[2] << 6) | sextet[3];
    da_add(&sb, (triple >> 16) & 0xFF);
    if (valid_sextets >= 3) da_add(&sb, (triple >> 8) & 0xFF);
    if (valid_sextets >= 4) da_add(&sb, triple & 0xFF);
  }
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_base64_encode(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("base64.encode() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[0].val_obj);
  return quo__mod_base64_encode_impl(str->data, str->len, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
}

static inline QuoVar quo__mod_base64_decode(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("base64.decode() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[0].val_obj);
  return quo__mod_base64_decode_impl(str->data, str->len);
}

static inline QuoVar quo__mod_base64_encode_url(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("base64.encode_url() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[0].val_obj);
  return quo__mod_base64_encode_impl(str->data, str->len, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
}

static inline QuoVar quo__mod_base64_decode_url(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("base64.decode_url() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[0].val_obj);
  return quo__mod_base64_decode_impl(str->data, str->len);
}

static inline void quo__mod_base64_init(const char *cwd) {
  QuoModule *m = quo_module_new(cwd, "base64", NULL, NULL);
  quo_module_register_cfn(m, "encode", -1, quo__mod_base64_encode);
  quo_module_register_cfn(m, "decode", -1, quo__mod_base64_decode);
  quo_module_register_cfn(m, "encode_url", -1, quo__mod_base64_encode_url);
  quo_module_register_cfn(m, "decode_url", -1, quo__mod_base64_decode_url);
}

// --- MODULE UUID --- //

static void quo__mod_uuid_random_bytes(unsigned char *buf, int len) {
#ifdef _WIN32
  HCRYPTPROV prov;
  if (CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    CryptGenRandom(prov, len, buf);
    CryptReleaseContext(prov, 0);
  } else
    for (int i = 0; i < len; i++) buf[i] = rand() & 0xFF;
#else
  FILE *f = fopen("/dev/urandom", "rb");
  if (f) {
    fread(buf, 1, len, f);
    fclose(f);
  } else
    for (int i = 0; i < len; i++) buf[i] = rand() & 0xFF;
#endif
}

static uint64_t quo__mod_uuid_timestamp_ms(void) {
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  return (t / 10000) - 11644473600000ULL;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void quo__mod_uuid_format(char *buf, unsigned char *bytes) {
  snprintf(buf, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0], bytes[1], bytes[2], bytes[3],
           bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

static bool quo__mod_uuid_parse_bytes(const char *str, unsigned char *bytes) {
  if (strlen(str) != 36) return false;
  if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') return false;
  int byte_idx = 0;
  for (int i = 0; i < 36; i++) {
    if (str[i] == '-') continue;
    char hex[3] = {str[i], str[i + 1], '\0'};
    char *end;
    bytes[byte_idx++] = (unsigned char)strtoul(hex, &end, 16);
    if (*end != '\0') return false;
    i++;
  }
  return byte_idx == 16;
}

static int quo__mod_uuid_version(unsigned char *bytes) { return (bytes[6] >> 4) & 0x0F; }

static const char *quo__mod_uuid_variant(unsigned char *bytes) {
  int variant = (bytes[8] >> 6) & 0x03;
  switch (variant) {
  case 0: return "NCS";
  case 1: return "RFC4122";
  case 2: return "Microsoft";
  case 3: return "Future";
  }
  return "Unknown";
}

static inline QuoVar quo__mod_uuid_v4(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  unsigned char bytes[16];
  quo__mod_uuid_random_bytes(bytes, 16);
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  char str[37];
  quo__mod_uuid_format(str, bytes);
  return quo_var_new_obj(quo_str_new(str, -1));
}

static inline QuoVar quo__mod_uuid_v7(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  unsigned char bytes[16];
  uint64_t timestamp = quo__mod_uuid_timestamp_ms();
  bytes[0] = (timestamp >> 40) & 0xFF;
  bytes[1] = (timestamp >> 32) & 0xFF;
  bytes[2] = (timestamp >> 24) & 0xFF;
  bytes[3] = (timestamp >> 16) & 0xFF;
  bytes[4] = (timestamp >> 8) & 0xFF;
  bytes[5] = timestamp & 0xFF;
  quo__mod_uuid_random_bytes(&bytes[6], 10);
  bytes[6] = (bytes[6] & 0x0F) | 0x70;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  char str[37];
  quo__mod_uuid_format(str, bytes);
  return quo_var_new_obj(quo_str_new(str, -1));
}

static inline QuoVar quo__mod_uuid_parse(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("uuid.parse() requires a string argument");
  unsigned char bytes[16];
  if (!quo__mod_uuid_parse_bytes(quo_var_as_str(&argv[0])->data, bytes)) {
    QuoDict *result = quo_dict_new();
    QuoStr *key = quo_str_new("valid", -1);
    QuoVar val = quo_var_new_bool(false);
    quo_dict_set(result, key, &val);
    return quo_var_new_obj(result);
  }
  QuoDict *result = quo_dict_new();
  QuoStr *key_valid = quo_str_new("valid", -1);
  QuoVar val_valid = quo_var_new_bool(true);
  quo_dict_set(result, key_valid, &val_valid);
  QuoStr *key_version = quo_str_new("version", -1);
  QuoVar val_version = quo_var_new_num(quo__mod_uuid_version(bytes));
  quo_dict_set(result, key_version, &val_version);
  QuoStr *key_variant = quo_str_new("variant", -1);
  QuoVar val_variant = quo_var_new_obj(quo_str_new(quo__mod_uuid_variant(bytes), -1));
  quo_dict_set(result, key_variant, &val_variant);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_uuid_is_valid(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_bool(false);
  unsigned char bytes[16];
  return quo_var_new_bool(quo__mod_uuid_parse_bytes(quo_var_as_str(&argv[0])->data, bytes));
}

static inline void quo__mod_uuid_init(const char *cwd) {
  QuoModule *m = quo_module_new(cwd, "uuid", NULL, NULL);
  quo_module_register_cfn(m, "v4", -1, quo__mod_uuid_v4);
  quo_module_register_cfn(m, "v7", -1, quo__mod_uuid_v7);
  quo_module_register_cfn(m, "parse", -1, quo__mod_uuid_parse);
  quo_module_register_cfn(m, "is_valid", -1, quo__mod_uuid_is_valid);
}

// --- MODULE ENV --- //

static inline QuoVar quo__mod_env_get(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[1])) return quo_var_new_err("env.get() requires a string argument");
  const char *value = getenv(quo_var_as_str(&argv[1])->data);
  return value ? quo_var_new_obj(quo_str_new(value, -1)) : quo_var_new_nil();
}

static inline QuoVar quo__mod_env_set(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("env.set() requires key and value strings");
#ifdef _WIN32
  char *str = quo_strdupf("%s=%s", quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  setenv(quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data, 1);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_env_unset(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("env.unset() requires a string argument");
#ifdef _WIN32
  char *str = quo_strdupf("%s=", quo_var_as_str(&argv[0])->data);
  _putenv(str);
  quo_dealloc(str);
#else
  unsetenv(quo_var_as_str(&argv[0])->data);
#endif
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_env_has(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("env.has() requires a string argument");
  return quo_var_new_bool(getenv(quo_var_as_str(&argv[0])->data) != NULL);
}

static inline QuoVar quo__mod_env_all(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  QuoDict *dict = quo_dict_new();
#ifdef _WIN32
  char *env_block = GetEnvironmentStrings();
  if (env_block) {
    char *p = env_block;
    while (*p) {
      char *eq = strchr(p, '=');
      if (eq) {
        *eq = '\0';
        QuoStr *key = quo_str_new(p, -1);
        QuoVar val = quo_var_new_obj(quo_str_new(eq + 1, -1));
        quo_dict_set(dict, key, &val);
        *eq = '=';
      }
      p += strlen(p) + 1;
    }
    FreeEnvironmentStrings(env_block);
  }
#else
  extern char **environ;
  for (char **env = environ; *env; env++) {
    char *eq = strchr(*env, '=');
    if (eq) {
      QuoStr *key = quo_str_new(*env, eq - *env);
      QuoVar val = quo_var_new_obj(quo_str_new(eq + 1, -1));
      quo_dict_set(dict, key, &val);
    }
  }
#endif
  return quo_var_new_obj(dict);
}

static inline void quo__mod_env_init(const char *cwd) {
  QuoModule *m = quo_module_new(cwd, "env", NULL, NULL);
  quo_module_register_cfn(m, "get", -1, quo__mod_env_get);
  quo_module_register_cfn(m, "set", -1, quo__mod_env_set);
  quo_module_register_cfn(m, "unset", -1, quo__mod_env_unset);
  quo_module_register_cfn(m, "has", -1, quo__mod_env_has);
  quo_module_register_cfn(m, "all", -1, quo__mod_env_all);
}

// --- BUILT-IN GLOBAL FUNCTIONS --- //

static QuoVar quo__builtin_import(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 && !quo_var_is_str(&argv[0])) return quo_var_new_err("import() takes path string argument");
  QuoStr *path = quo_var_as_str(&argv[0]);
  if (!quo_strsuffix(path->data, ".quo")) {
    QuoVar value;
    if (quo_ht_get(&quo__imported_modules, quo_str_new_interned(path->data, path->len), &value)) return value;
  }
  char *mod_path = mod_path = quo_strdupf("%s/%.*s", m->cwd, path->len, path->data);
  char *mod_source = quo_read_file(mod_path);
  if (!mod_source) {
    quo_dealloc(mod_path);
    return quo_var_new_err("Module not found");
  }
  char *mod_cwd = quo_dirname(mod_path);
  QuoModule *mod = quo_module_new(mod_cwd, mod_path, mod_source, NULL);
  quo_dealloc(mod_source);
  quo_dealloc(mod_cwd);
  quo_dealloc(mod_path);
  if (!mod) return quo_var_new_err("Failed to compile module");
  QuoVM *vm = quo_vm_new(mod);
  QuoVar result = quo_vm_run(vm, mod->fn);
  if (quo_var_is_err(&result)) return quo_var_new_err("Failed to import module");
  quo_vm_free(vm);
  return quo_var_new_obj(mod);
}

static QuoVar quo__builtin_type(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
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
  return quo_var_new_obj(quo_str_new_interned(type_str, -1));
}
// Print the values of the given arguments. Separate values with spaces and print a newline at the end.
static QuoVar quo__builtin_print(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  for (int i = 0; i < argc; i++) {
    quo_var_print(&argv[i]);
    if (i < argc - 1) printf(" ");
  }
  printf("\n");
  return quo_var_new_nil();
}
// Read a line from stdin and return it as a string
static QuoVar quo__builtin_input(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  // Optional prompt arguments
  for (int i = 0; i < argc; i++) quo_var_print(&argv[i]);
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
  QuoStr *str = quo_str_new(line, read);
  free(line);
  return quo_var_new_obj(str);
}
// Exit the program with an optional exit code
static QuoVar quo__builtin_exit(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  int code = 0;
  if (argc > 0 && quo_var_is_num(&argv[0])) code = (int)argv[0].val_num;
  exit(code);
  return quo_var_new_nil();
}
static QuoVar quo__builtin_bool(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  return quo_var_to_bool(&argv[0]);
}
static QuoVar quo__builtin_num(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  return quo_var_to_num(&argv[0]);
}
static QuoVar quo__builtin_str(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  return quo_var_to_str(&argv[0]);
}
static QuoVar quo__builtin_len(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  return quo_var_new_num(quo_var_len(&argv[0]));
}

// - STRING METHODS - //

static QuoVar quo__str_method_get(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2) return quo_var_new_err("get() requires index argument");
  if (!quo_var_is_num(&argv[1])) return quo_var_new_err("Index must be a number");
  QuoStr *str = quo_var_as_str(&argv[0]);
  if (str->char_len == 0) return quo_var_new_obj(quo_str_new_interned("", 0));
  int index = (int)argv[1].val_num;
  if (index > str->char_len) return quo_var_new_err("Index out of range");
  if (index < 0) {
    if (str->char_len == 0) index = 0;
    else
      while (index < 0) index = str->char_len + index;
  }
  const char *pos = quo__utf8_index(str->data, str->len, index);
  int char_len = quo__utf8_char_len((unsigned char)*pos);
  return quo_var_new_obj(quo_str_new(pos, char_len));
}
// Strip whitespace
static QuoVar quo__str_method_strip(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("strip() takes no arguments");
  QuoStr *str = quo_var_as_str(&argv[0]);
  const char *start = str->data;
  const char *end = str->data + str->len;
  while (start < end && quo__is_space(*start)) start++;
  while (end > start && quo__is_space(*(end - 1))) end--;
  return quo_var_new_obj(quo_str_new(start, end - start));
}
// Check if string starts with prefix
static QuoVar quo__str_method_startswith(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("startswith() requires a string argument");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *prefix = quo_var_as_str(&argv[1]);
  return quo_var_new_bool(quo_strprefix(str->data, prefix->data));
}
// Check if string ends with suffix
static QuoVar quo__str_method_endswith(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("endswith() requires a string argument");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *suffix = quo_var_as_str(&argv[1]);
  return quo_var_new_bool(quo_strsuffix(str->data, suffix->data));
}
// Check if string contains substring
static QuoVar quo__str_method_contains(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("contains() requires a string argument");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *substr = quo_var_as_str(&argv[1]);
  return quo_var_new_bool(strstr(str->data, substr->data));
}
// Split string into array
static QuoVar quo__str_method_split(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("split() requires a delimiter string");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *delim = quo_var_as_str(&argv[1]);
  QuoArr *arr = quo_arr_new();
  if (delim->len == 0) {
    // Split by character if delimiter is empty
    for (int i = 0; i < str->char_len; i++) {
      const char *ch = quo__utf8_index(str->data, str->len, i);
      int ch_len = quo__utf8_char_len((unsigned char)*ch);
      quo_arr_push(arr, quo_var_new_obj(quo_str_new(ch, ch_len)));
    }
  } else {
    const char *start = str->data;
    for (int i = 0; i <= str->len - delim->len; i++) {
      if (memcmp(str->data + i, delim->data, delim->len) == 0) {
        quo_arr_push(arr, quo_var_new_obj(quo_str_new(start, str->data + i - start)));
        start = str->data + i + delim->len;
        i += delim->len - 1;
      }
    }
    quo_arr_push(arr, quo_var_new_obj(quo_str_new(start, str->data + str->len - start)));
  }
  return quo_var_new_obj(arr);
}
// Replace substrings
static QuoVar quo__str_method_replace(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 3 || !quo_var_is_str(&argv[1]) || !quo_var_is_str(&argv[2]))
    return quo_var_new_err("replace() requires two string arguments");
  QuoStr *str = quo_var_as_str(&argv[0]);
  QuoStr *from = quo_var_as_str(&argv[1]);
  QuoStr *to = quo_var_as_str(&argv[2]);
  if (from->len == 0) return quo_var_new_obj(str); // No empty pattern
  QuoStringBuilder sb = quo_sb_new();
  int i = 0;
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
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

// - ARRAY METHODS - //

static QuoVar quo__arr_method_get(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2) return quo_var_new_err("get() requires index argument");
  if (!quo_var_is_num(&argv[1])) return quo_var_new_err("Index must be a number");
  return quo_arr_get(quo_obj_as_arr(argv[0].val_obj), (int)argv[1].val_num);
}
static QuoVar quo__arr_method_set(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 3) return quo_var_new_err("set() requires index and value arguments");
  if (!quo_var_is_num(&argv[1])) return quo_var_new_err("Index must be a number");
  quo_arr_set(quo_obj_as_arr(argv[0].val_obj), (int)argv[1].val_num, argv[2]);
  return argv[0];
}
static QuoVar quo__arr_method_push(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2) return quo_var_new_err("push() requires value argument");
  quo_arr_push(quo_obj_as_arr(argv[0].val_obj), argv[1]);
  return argv[0];
}
static QuoVar quo__arr_method_pop(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("pop() requires no arguments");
  if (quo_arr_len(quo_obj_as_arr(argv[0].val_obj)) == 0) return quo_var_new_nil();
  return quo_arr_pop(quo_obj_as_arr(argv[0].val_obj));
}

// - DICTIONARY METHODS - //

static QuoVar quo__dict_method_get(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2) return quo_var_new_err("get() requires key argument");
  if (!quo_var_is_str(&argv[1])) return quo_var_new_err("Key must be a string");
  return quo_dict_get(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1]));
}
static QuoVar quo__dict_method_set(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 3) return quo_var_new_err("set() requires key and value arguments");
  if (!quo_var_is_str(&argv[1])) return quo_var_new_err("Key must be a string");
  quo_dict_set(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1]), &argv[2]);
  return quo_var_new_nil();
}
static QuoVar quo__dict_method_has(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 && !quo_var_is_str(&argv[1])) return quo_var_new_err("has() requires key string argument");
  return quo_var_new_bool(quo_dict_has(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1])));
}
static QuoVar quo__dict_method_del(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 && !quo_var_is_str(&argv[1])) return quo_var_new_err("del() requires key string argument");
  quo_dict_del(quo_var_as_dict(&argv[0]), quo_var_as_str(&argv[1]));
  return quo_var_new_nil();
}
static QuoVar quo__dict_method_keys(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("keys() requires no arguments");
  QuoArr *keys = quo_arr_new();
  QuoDict *dict = quo_var_as_dict(&argv[0]);
  for (int i = 0; i < dict->dict.capacity; ++i) {
    QuoHashTableEntry *entry = &dict->dict.items[i];
    if (entry->key) quo_arr_push(keys, quo_var_new_obj(entry->key));
  }
  return quo_var_new_obj(keys);
}
static QuoVar quo__dict_method_values(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("values() requires no arguments");
  QuoArr *values = quo_arr_new();
  QuoDict *dict = quo_var_as_dict(&argv[0]);
  for (int i = 0; i < dict->dict.capacity; ++i) {
    QuoHashTableEntry *entry = &dict->dict.items[i];
    if (entry->key) quo_arr_push(values, entry->value);
  }
  return quo_var_new_obj(values);
}

static QuoObj *quo__method_lookup(QuoVar *val, QuoStr *name) {
  QuoHashTable *methods = NULL;
  switch (val->type) {
  case QUO_VAR_TYPE_NIL:
  case QUO_VAR_TYPE_BOOL:
  case QUO_VAR_TYPE_NUM:
  case QUO_VAR_TYPE_ERROR: break;
  case QUO_VAR_TYPE_OBJ: {
    switch (val->val_obj->type) {
    case QUO_OBJ_TYPE_STR: methods = &quo__str_methods; break;
    case QUO_OBJ_TYPE_ARR: methods = &quo__arr_methods; break;
    case QUO_OBJ_TYPE_DICT: methods = &quo__dict_methods; break;
    case QUO_OBJ_TYPE_USER: methods = &val->val_obj->dict->dict; break;
    case QUO_OBJ_TYPE_MODULE:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_CFN: break;
    }
  }
  }
  if (methods == NULL) return NULL;
  QuoVar value;
  if (!quo_ht_get(methods, name, &value)) return NULL;
  return value.val_obj;
}

// --- MODULE --- //

QuoModule *quo_module_new(const char *cwd, const char *file_path, const char *source, QuoModuleCleanupFn cleanup_fn) {
  assert(cwd != NULL && file_path != NULL);

  QuoModule *m = (QuoModule *)quo_obj_new(sizeof(QuoModule));
  m->obj.type = QUO_OBJ_TYPE_MODULE;
  m->cleanup_fn = cleanup_fn;

  quo_ht_init(&m->globals);

  m->name = quo_str_new_interned(file_path, -1);
  quo_ht_set(&quo__imported_modules, m->name, quo_var_new_obj(m));
  m->cwd = quo_strdup(cwd);
  m->file_path = quo_strdup(file_path);
  if (source) {
    m->source = quo_strdup(source);
    m->pos = 0;
    m->line = m->column = 1;
    da_add(&m->scopes, (QuoTokenList){0}); // Start global parser scope
    quo__parser_advance(m);
    while (!quo__parser_check(m, QUO_TT_EOF)) {
      QuoStmt *stmt = quo__parser_stmt(m);
      if (stmt) da_add(&m->ast, stmt);
    }
    if (m->had_compile_error) {
      quo_obj_unref((QuoObj *)m);
      return NULL;
    }
    QuoCompiler *compiler = quo_compiler_new(m, "__main_fn__", -1);
    m->fn = quo_compiler_compile(compiler, m->ast);
    quo_compiler_free(compiler);
  }
  return m;
}

void quo_module_register_var(QuoModule *m, QuoStr *name, QuoVar value) { quo_ht_set(&m->globals, name, value); }

void quo_module_register_cfn(QuoModule *m, const char *name, int name_len, QuoCFunctionPtr fn) {
  QuoCFn *cfn = quo_cfunction_new(name, name_len, fn);
  QuoVar var = quo_var_new_obj(cfn);
  quo_module_register_var(m, cfn->name, var);
}

QuoObj *quo_type_register(const char *name, size_t size) {
  QuoObj *obj = quo_obj_new(size);
  obj->type = QUO_OBJ_TYPE_USER;
  obj->name = quo_str_new_interned(name, -1);
  obj->dict = quo_dict_new();
  quo_ht_set(&quo__types, obj->name, quo_var_new_obj(obj));
  return obj;
}

QuoObj *quo_type_get_instance(const char *name) {
  QuoStr *name_str = quo_str_new(name, -1);
  QuoVar var;
  if (quo_ht_get(&quo__types, name_str, &var)) {
    QuoObj *obj = quo_var_as_obj(&var);
    QuoObj *instance = quo_obj_new(obj->size);
    memcpy(instance, obj, obj->size);
    quo_obj_unref((QuoObj *)name_str);
    return instance;
  }
  quo_obj_unref((QuoObj *)name_str);
  return NULL;
}

void quo_type_add(QuoObj *type, QuoStr *name, QuoVar value) { quo_dict_set(type->dict, name, &value); }

void quo_type_add_cfn(QuoObj *type, const char *name, int name_len, QuoCFunctionPtr fn) {
  QuoCFn *cfn = quo_cfunction_new(name, name_len, fn);
  quo_type_add(type, cfn->name, quo_var_new_obj(cfn));
}

QuoVar quo_module_run(QuoModule *m) {
  if (m->had_compile_error) return quo_var_new_nil();
  QuoVM *vm = quo_vm_new(m);
  QuoVar result = quo_vm_run(vm, m->fn);
  quo_vm_free(vm);
  return result;
}

// --- CONVERSION FUNCTIONS --- //

QuoVar quo_var_to_bool(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_ERROR:
  case QUO_VAR_TYPE_NIL: break;
  case QUO_VAR_TYPE_BOOL: return *v;
  case QUO_VAR_TYPE_NUM: return quo_var_new_bool(v->val_num > 0);
  case QUO_VAR_TYPE_OBJ:
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return quo_var_new_bool(quo_obj_as_str(v->val_obj)->len > 0);
    case QUO_OBJ_TYPE_ARR: return quo_var_new_bool(quo_obj_as_arr(v->val_obj)->arr.count > 0);
    case QUO_OBJ_TYPE_DICT: return quo_var_new_bool(quo_obj_as_dict(v->val_obj)->dict.count > 0);
    case QUO_OBJ_TYPE_MODULE:
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_CFN: break;
    }
  }
  return quo_var_new_bool(false);
}

QuoVar quo_var_to_num(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_ERROR:
  case QUO_VAR_TYPE_NIL: break;
  case QUO_VAR_TYPE_BOOL:
  case QUO_VAR_TYPE_NUM: return *v;
  case QUO_VAR_TYPE_OBJ:
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return quo_var_new_num(quo_strtod(quo_obj_as_str(v->val_obj)->data, quo_obj_as_str(v->val_obj)->len));
    case QUO_OBJ_TYPE_ARR:
    case QUO_OBJ_TYPE_DICT:
    case QUO_OBJ_TYPE_MODULE:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_CFN: break;
    }
  }
  return quo_var_new_num(0.0);
}

QuoVar quo_var_to_str(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_ERROR:
  case QUO_VAR_TYPE_NIL: break;
  case QUO_VAR_TYPE_BOOL: return quo_var_new_obj(quo_str_new(v->val_num ? "true" : "false", -1));
  case QUO_VAR_TYPE_NUM: {
    char buf[318];
    snprintf(buf, sizeof(buf), "%g", v->val_num);
    return quo_var_new_obj(quo_str_new(buf, -1));
  }
  case QUO_VAR_TYPE_OBJ:
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return *v;
    case QUO_OBJ_TYPE_ARR:
    case QUO_OBJ_TYPE_MODULE:
    case QUO_OBJ_TYPE_DICT:
    case QUO_OBJ_TYPE_USER:
    case QUO_OBJ_TYPE_FN:
    case QUO_OBJ_TYPE_CFN: return quo_var_new_obj(quo_str_new("", 0));
    }
  }
  return quo_var_new_obj(quo_str_new("", 0));
}

static inline bool quo_var_eq(QuoVar *a, QuoVar *b) {
  // Fast path: both numbers
  if (quo_var_is_num(a) && quo_var_is_num(b)) return a->val_num == b->val_num;
  // Fast path: both strings
  if (quo_var_is_str(a) && quo_var_is_str(b)) {
    QuoStr *str_a = quo_var_as_str(a);
    QuoStr *str_b = quo_var_as_str(b);
    // Quick checks first
    if (str_a == str_b) return true;              // Same pointer (interned)
    if (str_a->hash != str_b->hash) return false; // Different hash = different strings
    if (str_a->len != str_b->len) return false;   // Different length = different strings
    // Same hash and length, must compare content
    return memcmp(str_a->data, str_b->data, str_a->len) == 0;
  }
  // Mixed string comparison (string vs non-string)
  if (quo_var_is_str(a) || quo_var_is_str(b)) return quo_var_to_bool(a).val_num == quo_var_to_bool(b).val_num;
  // Boolean/nil comparison
  if ((quo_var_is_bool(a) || quo_var_is_bool(b)) || (quo_var_is_nil(a) || quo_var_is_nil(b)))
    return quo_var_to_bool(a).val_num == quo_var_to_bool(b).val_num;
  // Different types
  if (a->type != b->type) return false;
  // Same type comparison
  switch (a->type) {
  case QUO_VAR_TYPE_ERROR: return a->val_err == b->val_err;
  case QUO_VAR_TYPE_NIL: return true;
  case QUO_VAR_TYPE_BOOL: return a->val_num == b->val_num;
  case QUO_VAR_TYPE_NUM: return a->val_num == b->val_num;
  case QUO_VAR_TYPE_OBJ: return a->val_obj == b->val_obj;
  }
  return false;
}

static inline int quo_var_cmp(QuoVar *a, QuoVar *b) {
  if (quo_var_is_num(a) && quo_var_is_num(b)) return a->val_num - b->val_num;
  if (quo_var_is_str(a) && quo_var_is_str(b)) return quo_obj_as_str(a->val_obj)->len - quo_obj_as_str(b->val_obj)->len;
  return 0;
}

int quo_var_len(QuoVar *v) {
  if (quo_var_is_str(v)) return quo_obj_as_str(v->val_obj)->char_len;
  if (quo_var_is_arr(v)) return quo_arr_len(quo_obj_as_arr(v->val_obj));
  if (quo_var_is_dict(v)) return quo_obj_as_dict(v->val_obj)->dict.count;
  return 0;
}

int quo_var_print(QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_NIL: return printf("nil");
  case QUO_VAR_TYPE_BOOL: return printf(v->val_num ? "true" : "false");
  case QUO_VAR_TYPE_NUM: return printf("%.15g", v->val_num);
  case QUO_VAR_TYPE_ERROR: return printf("<error %s>", v->val_err);
  case QUO_VAR_TYPE_OBJ: {
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: return printf("%s", quo_obj_as_str(v->val_obj)->data);
    case QUO_OBJ_TYPE_ARR: {
      int len = 0;
      len += printf("[");
      for (int i = 0; i < quo_arr_len(quo_obj_as_arr(v->val_obj)); i++) {
        if (i > 0) len += printf(", ");
        QuoVar item = quo_arr_get(quo_obj_as_arr(v->val_obj), i);
        if (quo_var_is_str(&item)) len += printf("\"%s\"", quo_obj_as_str(item.val_obj)->data);
        else len += quo_var_print(&item);
      }
      len += printf("]");
      return len;
    }
    case QUO_OBJ_TYPE_DICT: {
      int len = 0;
      len += printf("{");
      bool first = true;
      for (int i = 0; i < da_capacity(&quo_obj_as_dict(v->val_obj)->dict); i++) {
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
    case QUO_OBJ_TYPE_FN: return printf("<fn %s>", quo_obj_as_fn(v->val_obj)->name->data);
    case QUO_OBJ_TYPE_CFN: return printf("<cfn %s>", quo_obj_as_cfn(v->val_obj)->name->data);
    case QUO_OBJ_TYPE_USER: return printf("<%s>", v->val_obj->name->data);
    }
  }
  }
  return 0;
}

// -------------------- LEXER -------------------- //

static enum QuoTokenType quo__keywords[] = {
    QUO_TT_VAR,  QUO_TT_FN,    QUO_TT_WHILE, QUO_TT_BREAK, QUO_TT_CONTINUE, QUO_TT_IF,     QUO_TT_ELSE,
    QUO_TT_TRUE, QUO_TT_FALSE, QUO_TT_NIL,   QUO_TT_AND,   QUO_TT_OR,       QUO_TT_RETURN,
};
static enum QuoTokenType quo__single_char_symbols[] = {
    QUO_TT_DOT,   QUO_TT_OPAREN, QUO_TT_CPAREN, QUO_TT_OBRACE, QUO_TT_CBRACE,   QUO_TT_OBRACKET, QUO_TT_CBRACKET,
    QUO_TT_COMMA, QUO_TT_COLON,  QUO_TT_EQ,     QUO_TT_LT,     QUO_TT_GT,       QUO_TT_PLUS,     QUO_TT_MINUS,
    QUO_TT_STAR,  QUO_TT_SLASH,  QUO_TT_BANG,   QUO_TT_MOD,    QUO_TT_QUESTION,
};
static enum QuoTokenType quo__compound_symbols[] = {
    QUO_TT_BANGEQ, QUO_TT_DOUBLEEQ, QUO_TT_GTEQ, QUO_TT_LTEQ, QUO_TT_PLUSEQ, QUO_TT_MINUSEQ, QUO_TT_MULEQ, QUO_TT_DIVEQ,
};

static inline char quo__lexer_peek(QuoModule *m, int offset) { return m->source[m->pos + offset]; }
static inline void quo__lexer_advance(QuoModule *m) {
  m->pos++;
  if (quo__lexer_peek(m, 0) == '\n') m->line++, m->column = 1;
  else m->column++;
}
static QuoToken quo__lexer_next_token(QuoModule *m) {
  QuoToken t = {.type = QUO_TT_EOF, .line = m->line, .column = m->column};
  while (quo__lexer_peek(m, 0) != '\0') {
    // Skip whitespace
    if (quo__is_space(quo__lexer_peek(m, 0))) quo__lexer_advance(m);
    // Line comments
    else if (!strncmp(m->source + m->pos, quo_token_type_str(QUO_TT_COMMENT), strlen(quo_token_type_str(QUO_TT_COMMENT)))) {
      while (quo__lexer_peek(m, 0) != '\n' && quo__lexer_peek(m, 0) != '\0') quo__lexer_advance(m);
      continue;
    }
    // Identifier or keyword
    else if (quo__is_alpha(quo__lexer_peek(m, 0))) {
      t.type = QUO_TT_ID;
      size_t start = m->pos;
      while (quo__is_alphanumeric(quo__lexer_peek(m, 0)) || quo__lexer_peek(m, 0) == '_') quo__lexer_advance(m);
      t.start = m->source + start;
      t.len = m->pos - start;
      // Find keywords
      for (int i = 0; i < quo__static_array_size(quo__keywords); ++i) {
        int len = (int)strlen(quo_token_type_str(quo__keywords[i]));
        if (len == t.len && memcmp(t.start, quo_token_type_str(quo__keywords[i]), len) == 0) {
          t.type = quo__keywords[i];
          break;
        }
      }
      break;
    }
    // Number
    else if (quo__is_digit(quo__lexer_peek(m, 0))) {
      t.type = QUO_TT_LITERAL_NUM;
      size_t start = m->pos;
      t.start = m->source + start;
      // Integer part
      while (quo__is_digit(quo__lexer_peek(m, 0)) || quo__lexer_peek(m, 0) == '_') quo__lexer_advance(m);
      // Float part
      if (quo__lexer_peek(m, 0) == '.' && quo__lexer_peek(m, 1) != '\0' && quo__is_digit(quo__lexer_peek(m, 1))) {
        quo__lexer_advance(m); // Skip '.'
        while (quo__is_digit(quo__lexer_peek(m, 0)) || quo__lexer_peek(m, 0) == '_') quo__lexer_advance(m);
      }
      t.len = m->pos - start;
      break;
    }
    // String
    else if (quo__lexer_peek(m, 0) == '"') {
      t.type = QUO_TT_LITERAL_STR;
      quo__lexer_advance(m); // Skip '"'
      size_t start = m->pos;
      t.start = m->source + start;
      while (quo__lexer_peek(m, 0) != '\0') {
        if (quo__lexer_peek(m, 0) == '"') {
          if (quo__lexer_peek(m, -1) == '\\') {
            quo__lexer_advance(m);
            continue;
          }
          t.len = m->pos - start;
          break;
        }
        quo__lexer_advance(m);
      }
      if (quo__lexer_peek(m, 0) == '"') quo__lexer_advance(m);
      else t.type = QUO_TT_ERROR, t.error_msg = "Unterminated string";
      break;
    }
    // Symbol
    else if (!quo__is_alphanumeric(quo__lexer_peek(m, 0))) {
      // Check for compound symbols first
      if (quo__lexer_peek(m, 1) != '\0') {
        char two_char[3] = {quo__lexer_peek(m, 0), quo__lexer_peek(m, 1), '\0'};
        for (int i = 0; i < quo__static_array_size(quo__compound_symbols); ++i)
          if (!strcmp(two_char, quo_token_type_str(quo__compound_symbols[i]))) {
            t.type = quo__compound_symbols[i];
            t.start = m->source + m->pos;
            t.len = 2;
            quo__lexer_advance(m);
            quo__lexer_advance(m);
            break;
          }
      }
      // If not a two-char symbol, check single-char symbols
      if (t.type == QUO_TT_EOF) { // Only if we haven't matched a compound symbol
        char single_char[2] = {quo__lexer_peek(m, 0), '\0'};
        for (int i = 0; i < quo__static_array_size(quo__single_char_symbols); ++i)
          if (!strcmp(single_char, quo_token_type_str(quo__single_char_symbols[i]))) {
            t.type = quo__single_char_symbols[i];
            t.start = m->source + m->pos;
            t.len = 1;
            quo__lexer_advance(m);
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
  case QUO_TT_WHILE: str = "while"; break;
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

// ------------------------------ PARSER ------------------------------ //

static inline void quo__parser_error(QuoModule *m, QuoToken t, const char *fmt, ...) {
  m->had_compile_error = true;
  fprintf(stderr, "\033[0;31m%s:%d:%d: Parse error: ", m->file_path ? m->file_path : "<input>", t.line, t.column);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\033[0m\n");
}

// Check if the current token matches the given type without consuming it.
static inline bool quo__parser_check(QuoModule *m, enum QuoTokenType type) { return m->current.type == type; }

// Consume the current token and advance to the next one.
static inline void quo__parser_advance(QuoModule *m) {
  m->previous = m->current;
  while (true) {
    m->current = quo__lexer_next_token(m);
    if (quo__parser_check(m, QUO_TT_ERROR)) quo__parser_error(m, m->current, m->current.error_msg);
    else break;
  }
}

// Match a specific token type and consume it if it matches.
static inline bool quo__parser_match(QuoModule *m, enum QuoTokenType type) {
  if (!quo__parser_check(m, type)) return false;
  quo__parser_advance(m);
  return true;
}

// Expect a specific token type or error. Consumes the token if it matches.
static inline void quo__parser_expect(QuoModule *m, enum QuoTokenType type, const char *message) {
  if (quo__parser_check(m, type)) {
    quo__parser_advance(m);
    return;
  }
  quo__parser_error(m, m->current, message);
}

static void quo__parser_begin_scope(QuoModule *m) {
  QuoTokenList scope = {0};
  da_add(&m->scopes, scope);
}

static void quo__parser_end_scope(QuoModule *m) {
  if (da_count(&m->scopes) > 0) {
    da_free(&da_at(&m->scopes, da_count(&m->scopes) - 1));
    da_count(&m->scopes)--;
  }
}

static bool quo__parser_is_declared_in_current_scope(QuoModule *m, QuoToken name) {
  if (da_count(&m->scopes) == 0) return false;
  QuoTokenList *current_scope = &da_at(&m->scopes, da_count(&m->scopes) - 1);
  for (int i = 0; i < da_count(current_scope); i++)
    if (quo_tokens_eq(da_at(current_scope, i), name)) return true;
  return false;
}

// Search from innermost to outermost scope
static bool quo__parser_is_declared(QuoModule *m, QuoToken name) {
  for (int s = da_count(&m->scopes) - 1; s >= 0; s--) {
    QuoTokenList *scope = &da_at(&m->scopes, s);
    for (int i = 0; i < da_count(scope); i++)
      if (quo_tokens_eq(da_at(scope, i), name)) return true;
  }
  return false;
}

static void quo__parser_declare_variable(QuoModule *m, QuoToken name) {
  if (da_count(&m->scopes) == 0) return;
  QuoTokenList *current_scope = &da_at(&m->scopes, da_count(&m->scopes) - 1);
  da_add(current_scope, name);
}

static inline QuoStmt *quo__parser_wrap_stmt_in_block(QuoModule *m, QuoStmt *stmt) {
  quo__parser_begin_scope(m);
  QuoStmt *block_stmt = quo__stmt_new(QUO_STMT_BLOCK);
  da_add(&block_stmt->block, stmt);
  quo__parser_end_scope(m);
  return block_stmt;
}

// --- PARSER EXPRESSIONS --- //

static QuoExpr *quo__parser_literal(QuoModule *m);
static QuoExpr *quo__parser_fn_expr(QuoModule *m);
static QuoExpr *quo__parser_id(QuoModule *m);
static QuoExpr *quo__parser_grouping(QuoModule *m);
static QuoExpr *quo__parser_unary(QuoModule *m);
static QuoExpr *quo__parser_binary(QuoModule *m, QuoExpr *left);
static QuoExpr *quo__parser_call(QuoModule *m, QuoExpr *callee);
static QuoExpr *quo__parser_assignment_expr(QuoModule *m, QuoExpr *target);
static QuoExpr *quo__parser_ternary_expr(QuoModule *m, QuoExpr *condition);
static QuoExpr *quo__parser_member_access(QuoModule *m, QuoExpr *object);

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

// Parse rules
typedef struct {
  QuoExpr *(*prefix)(QuoModule *);
  QuoExpr *(*infix)(QuoModule *, QuoExpr *);
  // Precedence levels (lowest to highest)
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
} QuoParseRule;

static QuoParseRule quo__get_parse_rule(QuoToken t) {
  switch (t.type) {
  case QUO_TT_NONE:
  case QUO_TT_EOF:
  case QUO_TT_ERROR:
  case QUO_TT_COMMENT:
  case QUO_TT_VAR:
  case QUO_TT_WHILE:
  case QUO_TT_BREAK:
  case QUO_TT_CONTINUE:
  case QUO_TT_IF:
  case QUO_TT_ELSE:
  case QUO_TT_RETURN:
  case QUO_TT_CPAREN:
  case QUO_TT_CBRACE:
  case QUO_TT_CBRACKET:
  case QUO_TT_COMMA:
  case QUO_TT_COLON: return (QuoParseRule){0};

  case QUO_TT_ID: return (QuoParseRule){quo__parser_id, NULL, PREC_NONE};
  case QUO_TT_LITERAL_NUM:
  case QUO_TT_LITERAL_STR:
  case QUO_TT_TRUE:
  case QUO_TT_FALSE:
  case QUO_TT_NIL:
  case QUO_TT_OBRACE:
  case QUO_TT_OBRACKET: return (QuoParseRule){quo__parser_literal, NULL, PREC_NONE};
  case QUO_TT_FN: return (QuoParseRule){quo__parser_fn_expr, NULL, PREC_NONE};
  case QUO_TT_AND: return (QuoParseRule){NULL, quo__parser_binary, PREC_AND};
  case QUO_TT_OR: return (QuoParseRule){NULL, quo__parser_binary, PREC_OR};
  case QUO_TT_BANGEQ:
  case QUO_TT_DOUBLEEQ: return (QuoParseRule){NULL, quo__parser_binary, PREC_EQUALITY};
  case QUO_TT_DOT: return (QuoParseRule){NULL, quo__parser_member_access, PREC_CALL};
  case QUO_TT_OPAREN: return (QuoParseRule){quo__parser_grouping, quo__parser_call, PREC_CALL};
  case QUO_TT_EQ:
  case QUO_TT_DIVEQ:
  case QUO_TT_MULEQ:
  case QUO_TT_MINUSEQ:
  case QUO_TT_PLUSEQ: return (QuoParseRule){NULL, quo__parser_assignment_expr, PREC_ASSIGNMENT};
  case QUO_TT_GTEQ:
  case QUO_TT_LTEQ:
  case QUO_TT_LT:
  case QUO_TT_GT: return (QuoParseRule){NULL, quo__parser_binary, PREC_COMPARISON};
  case QUO_TT_PLUS: return (QuoParseRule){NULL, quo__parser_binary, PREC_TERM};
  case QUO_TT_MINUS: return (QuoParseRule){quo__parser_unary, quo__parser_binary, PREC_TERM};
  case QUO_TT_STAR:
  case QUO_TT_SLASH:
  case QUO_TT_MOD: return (QuoParseRule){NULL, quo__parser_binary, PREC_FACTOR};
  case QUO_TT_BANG: return (QuoParseRule){quo__parser_unary, NULL, PREC_NONE};
  case QUO_TT_QUESTION: return (QuoParseRule){NULL, quo__parser_ternary_expr, PREC_TERNARY};
  }
  return (QuoParseRule){NULL, NULL, PREC_NONE};
}

static QuoExpr *quo__parser_parse_precedence(QuoModule *m, enum QuoPrecedence precedence) {
  quo__parser_advance(m);
  QuoExpr *(*prefix)(QuoModule *) = quo__get_parse_rule(m->previous).prefix;
  if (!prefix) return NULL;
  QuoExpr *left = prefix(m);
  while (precedence < quo__get_parse_rule(m->current).precedence) {
    quo__parser_advance(m);
    QuoExpr *(*infix)(QuoModule *, QuoExpr *) = quo__get_parse_rule(m->previous).infix;
    left = infix(m, left);
  }
  return left;
}

static QuoExpr *quo__parser_expression(QuoModule *m) { return quo__parser_parse_precedence(m, PREC_NONE); }

static QuoExpr *quo__parser_literal(QuoModule *m) {
  // Array literal
  if (m->previous.type == QUO_TT_OBRACKET) {
    QuoExpr *expr = quo__expr_new(QUO_EXPR_ARRAY, m->previous);
    if (!quo__parser_check(m, QUO_TT_CBRACKET)) {
      do {
        if (quo__parser_check(m, QUO_TT_CBRACKET)) {
          expr->array.trailing_comma = true;
          break;
        }
        QuoExpr *element = quo__parser_expression(m);
        da_add(&expr->array.elements, element);
      } while (quo__parser_match(m, QUO_TT_COMMA));
    }
    quo__parser_expect(m, QUO_TT_CBRACKET, "Expected ']' after array elements");
    return expr;
  }
  // Dictionary literal
  else if (m->previous.type == QUO_TT_OBRACE) {
    QuoExpr *expr = quo__expr_new(QUO_EXPR_DICT, m->previous);
    if (!quo__parser_check(m, QUO_TT_CBRACE)) {
      do {
        if (quo__parser_check(m, QUO_TT_CBRACE)) {
          expr->dict.trailing_comma = true;
          break;
        }
        QuoExpr *key = quo__parser_expression(m);
        quo__parser_expect(m, QUO_TT_COLON, "Expected ':' after dictionary key");
        QuoExpr *value = quo__parser_expression(m);
        QuoExprDictPair pair = {key, value};
        // Auto-name function expressions using the dict key if it's a string literal
        if (value->type == QUO_EXPR_FUNCTION && value->function.name.len == 0 && key->type == QUO_EXPR_LITERAL &&
            key->token.type == QUO_TT_LITERAL_STR) {
          value->function.name = key->token;
        }
        da_add(&expr->dict.pairs, pair);
      } while (quo__parser_match(m, QUO_TT_COMMA));
    }
    quo__parser_expect(m, QUO_TT_CBRACE, "Expected '}' after dictionary literal");
    return expr;
  }
  // Number or string
  else {
    return quo__expr_new(QUO_EXPR_LITERAL, m->previous);
  }
}

static QuoExpr *quo__parser_fn_expr(QuoModule *m) {
  quo__parser_expect(m, QUO_TT_OPAREN, "Expected '(' after fn");
  quo__parser_begin_scope(m);
  QuoTokenList parameters = {0};
  if (!quo__parser_check(m, QUO_TT_CPAREN)) {
    do {
      quo__parser_expect(m, QUO_TT_ID, "Expected parameter name");
      QuoToken param = m->previous;
      da_add(&parameters, param);
      quo__parser_declare_variable(m, param);
    } while (quo__parser_match(m, QUO_TT_COMMA));
  }
  quo__parser_expect(m, QUO_TT_CPAREN, "Expected ')' after parameters");
  // Body
  QuoStmt *stmt = quo__parser_stmt(m);
  QuoExpr *expr = quo__expr_new(QUO_EXPR_FUNCTION, m->previous);
  expr->function.parameters = parameters;
  expr->function.body = stmt->type != QUO_STMT_BLOCK ? quo__parser_wrap_stmt_in_block(m, stmt) : stmt;
  return expr;
}

// Parse a variable reference
static QuoExpr *quo__parser_variable(QuoModule *m) {
  QuoToken name = m->previous;
  return quo__expr_new(QUO_EXPR_VARIABLE, name);
}

// Parse a grouping expression: ( expr )
static QuoExpr *quo__parser_grouping(QuoModule *m) {
  QuoExpr *expr = quo__parser_expression(m);
  quo__parser_expect(m, QUO_TT_CPAREN, "Expected ')' after expression");
  QuoExpr *group = quo__expr_new(QUO_EXPR_GROUPING, m->previous);
  group->unary.expr = expr;
  return group;
}

// Parse a unary expression: -expr, !expr
static QuoExpr *quo__parser_unary(QuoModule *m) {
  QuoToken op = m->previous;
  QuoExpr *right = quo__parser_parse_precedence(m, PREC_UNARY);
  QuoExpr *expr = quo__expr_new(QUO_EXPR_UNARY, op);
  expr->unary.op = op;
  expr->unary.expr = right;
  return expr;
}

// Parse a binary expression: expr op expr
static QuoExpr *quo__parser_binary(QuoModule *m, QuoExpr *left) {
  QuoToken op = m->previous;
  QuoExpr *right = quo__parser_parse_precedence(m, quo__get_parse_rule(op).precedence);
  QuoExpr *expr = quo__expr_new(QUO_EXPR_BINARY, op);
  expr->binary.left = left;
  expr->binary.op = op;
  expr->binary.right = right;
  return expr;
}

// Parse function call: callee(args)
static QuoExpr *quo__parser_call(QuoModule *m, QuoExpr *callee) {
  QuoExpr *expr = quo__expr_new(QUO_EXPR_CALL, m->previous);
  expr->call.callee = callee;
  if (!quo__parser_check(m, QUO_TT_CPAREN)) do {
      da_add(&expr->call.arguments, quo__parser_expression(m));
    } while (quo__parser_match(m, QUO_TT_COMMA));
  quo__parser_expect(m, QUO_TT_CPAREN, "Expected ')' after arguments");
  return expr;
}

// Parse assignment: target = value or target += value etc.
static QuoExpr *quo__parser_assignment_expr(QuoModule *m, QuoExpr *target) {
  QuoToken op = m->previous;
  QuoExpr *value = quo__parser_expression(m);
  // If assigning a function to a variable, auto-name it
  if (value && value->type == QUO_EXPR_FUNCTION) value->function.name = target->token;
  QuoExpr *expr = quo__expr_new(QUO_EXPR_ASSIGN, op);
  expr->assign.target = target;
  expr->assign.value = value;
  return expr;
}

static QuoExpr *quo__parser_ternary_expr(QuoModule *m, QuoExpr *condition) {
  // Parse the then branch
  QuoExpr *then_expr = quo__parser_expression(m);
  // Expect the colon
  quo__parser_expect(m, QUO_TT_COLON, "Expected ':' in ternary expression");
  // Parse the else branch
  QuoExpr *else_expr = quo__parser_parse_precedence(m, PREC_ASSIGNMENT);
  // Create ternary expression
  QuoExpr *expr = quo__expr_new(QUO_EXPR_TERNARY, m->previous);
  expr->ternary.condition = condition;
  expr->ternary.then_expr = then_expr;
  expr->ternary.else_expr = else_expr;
  return expr;
}

static QuoExpr *quo__parser_id(QuoModule *m) {
  if (!quo__parser_is_declared(m, m->previous)) {
    QuoStr *key = quo_str_new_interned(m->previous.start, m->previous.len);
    QuoVar value;
    if (quo_ht_get(&quo__builtin_methods, key, &value)) return quo__parser_variable(m);
    else if (quo_ht_get(&m->globals, key, &value)) return quo__parser_variable(m);
    quo__parser_error(m, m->previous, "Undefined variable '" QUO_TOKEN_FMT "'", QUO_TOKEN_ARG(m->previous));
  }
  return quo__parser_variable(m);
}

// Parse member access: object.member
static QuoExpr *quo__parser_member_access(QuoModule *m, QuoExpr *object) {
  if (!quo__parser_match(m, QUO_TT_ID)) {
    quo__parser_error(m, m->current, "Expected member name after '.'");
    return object;
  }
  QuoToken member = m->previous;
  // Create member access expression
  QuoExpr *expr = quo__expr_new(QUO_EXPR_MEMBER_ACCESS, m->previous);
  expr->member_access.object = object;
  expr->member_access.member = member;
  // If followed by '(', convert to method call
  if (quo__parser_match(m, QUO_TT_OPAREN)) {
    QuoExpr *call = quo__expr_new(QUO_EXPR_CALL, member);
    call->call.callee = expr; // The member access becomes the callee
    // Parse arguments
    if (!quo__parser_check(m, QUO_TT_CPAREN)) {
      do { da_add(&call->call.arguments, quo__parser_expression(m)); } while (quo__parser_match(m, QUO_TT_COMMA));
    }
    quo__parser_expect(m, QUO_TT_CPAREN, "Expected ')' after arguments");
    // Handle chaining
    if (quo__parser_match(m, QUO_TT_DOT)) return quo__parser_member_access(m, call);
    return call;
  }
  // Handle chained member access
  if (quo__parser_match(m, QUO_TT_DOT)) return quo__parser_member_access(m, expr);
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
  case QUO_STMT_RETURN:
    if (stmt->expression) quo__expr_free(stmt->expression);
    break;
  case QUO_STMT_IF:
    quo__expr_free(stmt->if_stmt.condition);
    quo__stmt_free(stmt->if_stmt.then_branch);
    if (stmt->if_stmt.else_branch) quo__stmt_free(stmt->if_stmt.else_branch);
    break;
  case QUO_STMT_LOOP:
    if (stmt->loop.condition) quo__expr_free(stmt->loop.condition);
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

static QuoStmt *quo__parser_stmt(QuoModule *m) {
  QuoStmt *stmt = NULL;

  // Variable
  if (quo__parser_match(m, QUO_TT_VAR)) {
    quo__parser_expect(m, QUO_TT_ID, "Expected variable name");
    QuoToken name = m->previous;
    if (quo__parser_is_declared_in_current_scope(m, name))
      quo__parser_error(m, name, "Variable '" QUO_TOKEN_FMT "' already declared in this scope", QUO_TOKEN_ARG(name));
    quo__parser_declare_variable(m, name); // Declare the variable in current scope
    QuoExpr *initializer = NULL;
    if (quo__parser_match(m, QUO_TT_EQ)) {
      initializer = quo__parser_expression(m);
      // Auto-name function expressions
      if (initializer && initializer->type == QUO_EXPR_FUNCTION) initializer->function.name = name;
    }
    stmt = quo__stmt_new(QUO_STMT_VAR_DECL);
    stmt->var_decl.name = name;
    stmt->var_decl.initializer = initializer;
  }

  // Block {}
  else if (quo__parser_match(m, QUO_TT_OBRACE)) {
    quo__parser_begin_scope(m); // New scope for block
    QuoStmtList block_stmts = {0};
    while (!quo__parser_check(m, QUO_TT_CBRACE) && !quo__parser_check(m, QUO_TT_EOF)) {
      QuoStmt *stmt = quo__parser_stmt(m);
      if (stmt) da_add(&block_stmts, stmt);
    }
    quo__parser_expect(m, QUO_TT_CBRACE, "Expected '}' after block");
    quo__parser_end_scope(m); // End scope
    stmt = quo__stmt_new(QUO_STMT_BLOCK);
    stmt->block = block_stmts;
  }

  // Return
  else if (quo__parser_match(m, QUO_TT_RETURN)) {
    QuoExpr *value = NULL;
    if (!quo__parser_check(m, QUO_TT_EOF) && !quo__parser_check(m, QUO_TT_CBRACE)) value = quo__parser_expression(m);
    stmt = quo__stmt_new(QUO_STMT_RETURN);
    stmt->expression = value;
  }

  // If / Else
  else if (quo__parser_match(m, QUO_TT_IF)) {
    QuoExpr *condition = quo__parser_expression(m);
    // Parse then branch - either block or single statement
    QuoStmt *then_stmt = quo__parser_stmt(m);
    QuoStmt *then_branch = then_stmt->type != QUO_STMT_BLOCK ? quo__parser_wrap_stmt_in_block(m, then_stmt) : then_stmt;
    // Parse else branch (optional)
    QuoStmt *else_branch = NULL;
    if (quo__parser_match(m, QUO_TT_ELSE)) {
      if (quo__parser_check(m, QUO_TT_IF)) else_branch = quo__parser_stmt(m);
      else {
        QuoStmt *else_stmt = quo__parser_stmt(m);
        else_branch = else_stmt->type != QUO_STMT_BLOCK ? quo__parser_wrap_stmt_in_block(m, else_stmt) : else_stmt;
      }
    }
    stmt = quo__stmt_new(QUO_STMT_IF);
    stmt->if_stmt.condition = condition;
    stmt->if_stmt.then_branch = then_branch;
    stmt->if_stmt.else_branch = else_branch;
  }

  // Loop
  else if (quo__parser_match(m, QUO_TT_WHILE)) {
    m->loop_count++;
    if (!quo__parser_match(m, QUO_TT_OPAREN)) {
      quo__parser_error(m, m->current, "Expected '(' before condition");
      m->loop_count--;
      return NULL;
    }
    QuoExpr *condition = quo__parser_expression(m);
    if (!condition) {
      quo__parser_error(m, m->current, "Expected expression");
      m->loop_count--;
      return NULL;
    }
    if (!quo__parser_match(m, QUO_TT_CPAREN)) {
      quo__parser_error(m, m->current, "Expected ')' after condition");
      m->loop_count--;
      return NULL;
    }
    QuoStmt *body_stmt = quo__parser_stmt(m);
    if (!body_stmt) {
      quo__expr_free(condition);
      m->loop_count--;
      return NULL;
    }
    QuoStmt *body = body_stmt->type != QUO_STMT_BLOCK ? quo__parser_wrap_stmt_in_block(m, body_stmt) : body_stmt;
    stmt = quo__stmt_new(QUO_STMT_LOOP);
    stmt->loop.condition = condition;
    stmt->loop.body = body;
    m->loop_count--;
  }

  // Break
  else if (quo__parser_match(m, QUO_TT_BREAK)) {
    if (m->loop_count == 0) quo__parser_error(m, m->current, "'break' statement outside of loop");
    stmt = quo__stmt_new(QUO_STMT_BREAK);
  }

  // Continue
  else if (quo__parser_match(m, QUO_TT_CONTINUE)) {
    if (m->loop_count == 0) quo__parser_error(m, m->current, "'continue' statement outside of loop");
    stmt = quo__stmt_new(QUO_STMT_CONTINUE);
  }

  // Expression
  else {
    stmt = quo__stmt_new(QUO_STMT_EXPRESSION);
    stmt->expression = quo__parser_expression(m);
  }

  return stmt;
}

// ------------------------------ COMPILER ------------------------------ //

static void quo__compiler_stmt(QuoCompiler *c, QuoStmt *stmt);

static void quo__compiler_begin_scope(QuoCompiler *c) { c->scope_depth++; }

static void quo__compiler_end_scope(QuoCompiler *c) {
  c->scope_depth--;
  while (da_count(&c->locals) > 0 && da_at(&c->locals, da_count(&c->locals) - 1).depth > c->scope_depth) {
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    da_count(&c->locals)--;
  }
}

static void quo__compiler_add_local_variable(QuoCompiler *c, QuoToken name) {
  struct QuoLocalVariable local = {name, -1}; // -1 means "not initialized yet"
  da_add(&c->locals, local);
}

static int quo__compiler_resolve_local(QuoCompiler *c, QuoToken name) {
  for (int i = da_count(&c->locals) - 1; i >= 0; i--) {
    struct QuoLocalVariable local = da_at(&c->locals, i);
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
    case QUO_TT_TRUE: constant = quo_var_new_bool(true); break;
    case QUO_TT_FALSE: constant = quo_var_new_bool(false); break;
    case QUO_TT_NIL: constant = quo_var_new_nil(); break;
    case QUO_TT_LITERAL_NUM: constant = quo_var_new_num(quo_strtod(e->token.start, e->token.len)); break;
    case QUO_TT_LITERAL_STR: constant = quo_var_new_obj(quo_str_new_interned(e->token.start, e->token.len)); break;
    default: break;
    }
    int index = quo__function_push_constant(c->fn, &constant);
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
    int pair_count = da_count(&e->dict.pairs);
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
    int idx = quo__compiler_resolve_local(c, e->token);
    if (idx != -1) {
      // Local variable
      quo__function_push_instruction(c->fn, QUO_OP_GET_LOCAL);
      quo__function_push_instruction(c->fn, idx);
    } else {
      // Treat as global - VM will error if undefined
      QuoVar name_var = quo_var_new_obj(quo_str_new_interned(e->token.start, e->token.len));
      int name_idx = quo__function_push_constant(c->fn, &name_var);
      quo__function_push_instruction(c->fn, QUO_OP_GET_GLOBAL);
      quo__function_push_instruction(c->fn, name_idx);
    }
    break;
  }
  case QUO_EXPR_FUNCTION: {
    QuoCompiler *fn_compiler = quo_compiler_new(c->m, e->function.name.start, e->function.name.len);
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
    int fn_idx = quo__function_push_constant(c->fn, &fn_var);
    quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
    quo__function_push_instruction(c->fn, fn_idx);
    break;
  }
  case QUO_EXPR_BINARY: {
    // Special handling for 'and' and 'or' (short-circuit operators)
    if (e->binary.op.type == QUO_TT_AND) {
      // AND: evaluate left, if false jump to end with false value, else pop and evaluate right
      quo__compiler_expr(c, e->binary.left);
      int jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
      quo__function_push_instruction(c->fn, QUO_OP_POP); // Pop the true value
      quo__compiler_expr(c, e->binary.right);
      quo__function_patch_jump(c->fn, jump_to_end);
      // If left was false, it stays on stack. If true, right value is on stack.
    } else if (e->binary.op.type == QUO_TT_OR) {
      quo__compiler_expr(c, e->binary.left);
      // If left is FALSE, jump to evaluate right side
      int jump_to_right = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
      // Left was TRUE, we're done - return left (still on stack)
      // But we need to skip the right side evaluation
      int jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
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
          quo_var_new_obj(quo_str_new_interned(e->call.callee->member_access.member.start, e->call.callee->member_access.member.len));
      int name_idx = quo__function_push_constant(c->fn, &method_name);
      quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
      quo__function_push_instruction(c->fn, name_idx);
      int arity = da_count(&e->call.arguments);
      for (int i = 0; i < arity; i++) quo__compiler_expr(c, da_at(&e->call.arguments, i));
      quo__function_push_instruction(c->fn, QUO_OP_METHOD_CALL);
      quo__function_push_instruction(c->fn, arity);
    } else {
      // Regular call: func(args)
      quo__compiler_expr(c, e->call.callee);
      int arity = da_count(&e->call.arguments);
      for (int i = 0; i < arity; i++) quo__compiler_expr(c, da_at(&e->call.arguments, i));
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
          quo_var_new_obj(quo_str_new_interned(e->assign.target->member_access.member.start, e->assign.target->member_access.member.len));
      int name_idx = quo__function_push_constant(c->fn, &field_name);
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
      int idx = quo__compiler_resolve_local(c, e->assign.target->token);
      if (idx != -1) {
        quo__function_push_instruction(c->fn, QUO_OP_SET_LOCAL);
        quo__function_push_instruction(c->fn, idx);
      } else {
        QuoStr *name = quo_str_new_interned(e->assign.target->token.start, e->assign.target->token.len);
        QuoVar name_var = quo_var_new_obj(name);
        int name_idx = quo__function_push_constant(c->fn, &name_var);
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
    int jump_to_else = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
    // Pop condition, compile then branch
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    quo__compiler_expr(c, e->ternary.then_expr);
    // Jump over else branch
    int jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
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
    QuoVar field_name = quo_var_new_obj(quo_str_new_interned(e->member_access.member.start, e->member_access.member.len));
    int name_idx = quo__function_push_constant(c->fn, &field_name);
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
  case QUO_STMT_VAR_DECL: {
    // Compile initializer if present, otherwise nil
    if (s->var_decl.initializer) {
      quo__compiler_expr(c, s->var_decl.initializer);
    } else {
      int nil_idx = quo__function_push_constant(c->fn, &quo_var_new_nil());
      quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
      quo__function_push_instruction(c->fn, nil_idx);
    }
    if (c->scope_depth == 0) {
      // Global variable declaration
      da_add(&c->declared_globals, s->var_decl.name);
      QuoVar name_var = quo_var_new_obj(quo_str_new_interned(s->var_decl.name.start, s->var_decl.name.len));
      int name_idx = quo__function_push_constant(c->fn, &name_var);
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
      int nil_idx = quo__function_push_constant(c->fn, &quo_var_new_nil());
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
    int jump_to_else = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    // Compile then branch
    quo__compiler_stmt(c, s->if_stmt.then_branch);
    // If there's an else branch, we need to jump over it
    int jump_to_end = 0;
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
    // Mark the start of the condition check
    int condition_start = da_count(&c->fn->instructions);
    loop_ctx.start = condition_start;
    // Compile condition
    int jump_to_end = 0;
    quo__compiler_expr(c, s->loop.condition);
    jump_to_end = quo__function_emit_jump(c->fn, QUO_OP_JUMP_IF_FALSE);
    quo__function_push_instruction(c->fn, QUO_OP_POP);
    quo__compiler_stmt(c, s->loop.body);             // Compile the body
    quo__function_emit_loop(c->fn, condition_start); // Jump back to condition check
    quo__function_patch_jump(c->fn, jump_to_end);    // Patch the conditional jump to exit the loop
    int end_pos = da_count(&c->fn->instructions);    // Store the end position for break patching
    // Patch all break jumps to this position (after the loop)
    for (int i = 0; i < da_count(&loop_ctx.breaks); i++) {
      int break_jump = da_at(&loop_ctx.breaks, i);
      int offset = end_pos - break_jump - 1;
      da_at(&c->fn->instructions, break_jump) = offset;
    }
    // Patch all continue jumps to the condition section
    for (int i = 0; i < da_count(&loop_ctx.continues); i++) {
      int continue_jump = da_at(&loop_ctx.continues, i);
      int offset = condition_start - continue_jump - 1;
      da_at(&c->fn->instructions, continue_jump) = offset;
    }
    da_free(&loop_ctx.breaks);
    da_free(&loop_ctx.continues);
    c->loop = prev_loop; // Restore previous loop context
    quo__compiler_end_scope(c);
    break;
  }
  case QUO_STMT_BREAK: {
    // Emit a jump to the end of the loop (to be patched later)
    int jump = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
    da_add(&c->loop->breaks, jump);
    break;
  }
  case QUO_STMT_CONTINUE: {
    // Emit a jump to the increment section (to be patched later)
    int jump = quo__function_emit_jump(c->fn, QUO_OP_JUMP);
    da_add(&c->loop->continues, jump);
    break;
  }
  }
}

QuoCompiler *quo_compiler_new(QuoModule *m, const char *name, int name_len) {
  QuoCompiler *c = quo_alloc(NULL, sizeof(QuoCompiler));
  c->m = m;
  c->fn = quo_function_new(m, name, name_len);
  c->scope_depth = 0;
  c->loop = NULL;

  struct QuoLocalVariable local = {.depth = 0, .name = {.start = "", .len = 0}};
  da_add(&c->locals, local);

  return c;
}

QuoFn *quo_compiler_compile(QuoCompiler *c, QuoStmtList ast) {
  for (int i = 0; i < da_count(&ast); i++) quo__compiler_stmt(c, da_at(&ast, i));
  // Add explicit nil return
  int nil_idx = quo__function_push_constant(c->fn, &quo_var_new_nil());
  quo__function_push_instruction(c->fn, QUO_OP_CONSTANT);
  quo__function_push_instruction(c->fn, nil_idx);
  quo__function_push_instruction(c->fn, QUO_OP_RETURN);
  // Return the compiled function
#ifdef QUO_DEBUG
  quo_debug_function_disassemble(c->fn);
#endif
  return c->fn;
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
static bool quo__vm_call_fn(QuoVM *vm, QuoFn *fn, int argc) {
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
static QuoVar quo__vm_dispatch_call(QuoVM *vm, QuoObj *func, int argc) {
#define CLEANUP_STACK()                                                                                                                    \
  for (int i = 0; i < argc + 1; i++) quo_var_unref(quo__vm_peek(vm, i));                                                                   \
  da_count(&vm->stack) -= argc + 1;
  if (func->type == QUO_OBJ_TYPE_CFN) {
    QuoVar *args = quo__vm_peek(vm, argc - 1);
    QuoCFn *cfn = quo_obj_as_cfn(func);
    QuoVar result = cfn->fn(vm->m, argc, args);
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

QuoVM *quo_vm_new(QuoModule *m) {
  QuoVM *vm = quo_alloc(NULL, sizeof(QuoVM));
  vm->m = m;
  return vm;
}

QuoVar quo_vm_run(QuoVM *vm, QuoFn *fn) {
#define READ_INST()  (*frame->ip++)
#define READ_CONST() (da_at(&frame->function->constants, READ_INST()))
#define LAST_FRAME() (&da_at(&vm->frames, da_count(&vm->frames) - 1))

  if (vm->m->had_compile_error) return quo_var_new_nil();
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
      int count = READ_INST();
      QuoArr *arr = quo_arr_new();
      QuoVar *base = quo__vm_peek(vm, count - 1); // Pointer to first element
      for (int i = 0; i < count; i++) quo_arr_push(arr, base[i]);
      // Remove elements from stack
      da_count(&vm->stack) -= count;
      quo__vm_push(vm, quo_var_new_obj(arr));
      break;
    }
    case QUO_OP_DICT: {
      int pair_count = READ_INST();
      QuoDict *dict = quo_dict_new(); // Create new dictionary
      // Pop key-value pairs in reverse order
      for (int i = pair_count - 1; i >= 0; i--) {
        QuoVar value = quo__vm_pop(vm);
        QuoVar key_var = quo__vm_pop(vm);
        if (quo_var_is_str(&key_var)) quo_dict_set(dict, quo_var_as_str(&key_var), &value);
        else return quo_var_new_err("Dictionary keys must be strings");
      }
      quo__vm_push(vm, quo_var_new_obj(dict));
      break;
    }
    case QUO_OP_ADD: {
      QuoVar *b = quo__vm_peek(vm, 0);
      QuoVar *a = quo__vm_peek(vm, 1);
      QuoVar result;
      if (quo_var_is_num(a) && quo_var_is_num(b)) result = quo_var_new_num(a->val_num + b->val_num);
      else if (quo_var_is_str(a) || quo_var_is_str(b)) {
        const QuoVar str_a = quo_var_to_str(a);
        const QuoVar str_b = quo_var_to_str(b);
        char *str = quo_strdupf("%s%s", quo_obj_as_str(str_a.val_obj)->data, quo_obj_as_str(str_b.val_obj)->data);
        QuoStr *res = quo_str_new(str, -1);
        quo_dealloc(str);
        result = quo_var_new_obj(res);
      } else if (quo_var_is_arr(a) && quo_var_is_arr(b)) {
        QuoArr *a_arr = quo_obj_as_arr(a->val_obj);
        QuoArr *b_arr = quo_obj_as_arr(b->val_obj);
        for (int i = 0; i < quo_arr_len(b_arr); i++) quo_arr_push(a_arr, quo_arr_get(b_arr, i));
        result = *a;
      } else return quo_var_new_err("Types don't support addition");
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_SUB: {
      QuoVar *b = quo__vm_peek(vm, 0);
      QuoVar *a = quo__vm_peek(vm, 1);
      QuoVar result;
      if (quo_var_is_num(a) && quo_var_is_num(b)) result = quo_var_new_num(a->val_num - b->val_num);
      else return quo_var_new_err("Types don't support subtraction");
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_MUL: {
      QuoVar *b = quo__vm_peek(vm, 0);
      QuoVar *a = quo__vm_peek(vm, 1);
      QuoVar result;
      // Numeric multiplication
      if (quo_var_is_num(a) && quo_var_is_num(b)) result = quo_var_new_num(a->val_num * b->val_num);
      // String repetition: "foo" * 3 -> "foofoofoo"
      else if ((quo_var_is_str(a) && quo_var_is_num(b)) || (quo_var_is_num(a) && quo_var_is_str(b))) {
        QuoVar *num_var = quo_var_is_num(a) ? a : b;
        QuoVar *str_var = quo_var_is_str(a) ? a : b;
        if (num_var->val_num <= 0) result = quo_var_new_obj(quo_str_new("", 0));
        else {
          QuoStr *str = quo_var_as_str(str_var);
          int len = str->len * num_var->val_num;
          char *data = quo_alloc(NULL, len + 1);
          for (int i = 0; i < (int)num_var->val_num; i++) memcpy(data + (i * str->len), str->data, str->len);
          data[len] = '\0';
          QuoStr *string = quo_str_new(data, -1);
          quo_dealloc(data);
          result = quo_var_new_obj(string);
        }
      } else return quo_var_new_err("Types don't support multiplication");
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_DIV: {
      QuoVar *b = quo__vm_peek(vm, 0);
      QuoVar *a = quo__vm_peek(vm, 1);
      QuoVar result;
      if (quo_var_is_num(a) && quo_var_is_num(b)) {
        if (b->val_num == 0) return quo_var_new_err("Division by zero");
        result = quo_var_new_num(a->val_num / b->val_num);
      } else return quo_var_new_err("Types don't support division");
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_MOD: {
      QuoVar *b = quo__vm_peek(vm, 0);
      QuoVar *a = quo__vm_peek(vm, 1);
      QuoVar result;
      if (quo_var_is_num(a) && quo_var_is_num(b)) {
        int a_val = (int)a->val_num;
        int b_val = (int)b->val_num;
        if (b_val == 0) return quo_var_new_err("Modulo by zero");
        result = quo_var_new_num(a_val % b_val);
      } else return quo_var_new_err("Types don't support modulo");
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, result);
      break;
    }
    case QUO_OP_NEGATE: {
      QuoVar *value = quo__vm_peek(vm, 0);
      if (quo_var_is_num(value)) value->val_num = -(value->val_num);
      else return quo_var_new_err("Type don't support negation");
      break;
    }
    case QUO_OP_NOT: {
      QuoVar *value = quo__vm_peek(vm, 0);
      if (quo_var_is_bool(value) || quo_var_is_num(value)) value->val_num = value->val_num > 0 ? false : true;
      else if (quo_var_is_nil(value)) value->val_num = true;
      else return quo_var_new_err("Type don't support logical negation");
      break;
    }
    case QUO_OP_EQ: {
      bool res = quo_var_eq(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
      da_count(&vm->stack) -= 2;
      quo__vm_push(vm, quo_var_new_bool(res));
      break;
    }
    case QUO_OP_NEQ: {
      bool res = !quo_var_eq(quo__vm_peek(vm, 1), quo__vm_peek(vm, 0));
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
      int slot = READ_INST();
      quo__vm_push(vm, da_at(&vm->stack, frame->slots_start + slot));
      break;
    }
    case QUO_OP_SET_LOCAL: {
      int slot = READ_INST();
      QuoVar *slot_ptr = &da_at(&vm->stack, frame->slots_start + slot);
      quo_var_unref(slot_ptr);
      *slot_ptr = *quo__vm_peek(vm, 0);
      quo_var_ref(slot_ptr);
      break;
    }
    case QUO_OP_GET_GLOBAL: {
      QuoVar name_var = READ_CONST();
      QuoVar value;
      if (quo_ht_get(&quo__builtin_methods, quo_var_as_str(&name_var), &value)) quo__vm_push(vm, value);
      else if (quo_ht_get(&frame->function->m->globals, quo_var_as_str(&name_var), &value)) quo__vm_push(vm, value);
      else return quo_var_new_err("Undefined variable");
      break;
    }
    case QUO_OP_SET_GLOBAL: {
      QuoVar name_var = READ_CONST();
      QuoVar old_value;
      if (quo_ht_get(&frame->function->m->globals, quo_var_as_str(&name_var), &old_value)) quo_var_unref(&old_value);
      QuoVar *value = quo__vm_peek(vm, 0);
      quo_var_ref(value);
      quo_ht_set(&frame->function->m->globals, quo_var_as_str(&name_var), *value);
      vm->stack.count--;
      break;
    }
    case QUO_OP_MEMBER_ACCESS: {
      // Stack: [object] [field_name]
      QuoVar field_name = quo__vm_pop(vm);
      // Stack: [object]
      QuoVar object = *quo__vm_peek(vm, 0);
      // Look up method in array method table
      QuoObj *method = quo__method_lookup(&object, quo_var_as_str(&field_name));
      if (method) {
        // Return the method (as a function value)
        quo_var_unref(quo__vm_peek(vm, 0));
        *quo__vm_peek(vm, 0) = quo_var_new_obj(method);
        quo_var_ref(quo__vm_peek(vm, 0));
      }
      if (!method && quo_var_is_module(&object)) {
        // Look up in module exports
        QuoModule *module = quo_var_as_module(&object);
        QuoStr *field_str = quo_var_as_str(&field_name);
        QuoVar value;
        if (quo_ht_get(&module->globals, field_str, &value)) {
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = value;
          quo_var_ref(quo__vm_peek(vm, 0));
        } else {
          quo_var_unref(quo__vm_peek(vm, 0));
          *quo__vm_peek(vm, 0) = quo_var_new_nil();
        }
      }
      if (!method && quo_var_is_dict(&object)) {
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
      int offset = READ_INST();
      frame->ip += offset;
      break;
    }
    case QUO_OP_JUMP_IF_FALSE: {
      int offset = READ_INST();
      QuoVar *condition = quo__vm_peek(vm, 0);
      if (!quo_var_is_true(condition)) frame->ip += offset;
      break;
    }
    case QUO_OP_LOOP: {
      int offset = READ_INST();
      frame->ip -= offset;
      break;
    }
    case QUO_OP_CALL: {
      int argc = READ_INST();
      QuoVar *callee = quo__vm_peek(vm, argc);
      if (callee->type != QUO_VAR_TYPE_OBJ) return quo_var_new_err("Attempt to call non-function");
      // Save the function type before dispatch (which may clean up the stack)
      QuoObjType func_type = callee->val_obj->type;
      QuoVar result = quo__vm_dispatch_call(vm, callee->val_obj, argc);
      if (quo_var_is_err(&result)) return result;
      // Use the saved type instead of the now-invalid callee pointer
      if (func_type == QUO_OBJ_TYPE_FN) frame = LAST_FRAME();
      break;
    }
    case QUO_OP_METHOD_CALL: {
      int argc = READ_INST();
      // Stack: [object, method_name, arg1, arg2, ...]
      QuoVar method_name = *quo__vm_peek(vm, argc);
      QuoVar *object = quo__vm_peek(vm, argc + 1);
      QuoObj *method = NULL;
      bool is_method = false; // Whether method takes 'self' as first arg

      // First lookup for type methods
      method = quo__method_lookup(object, quo_var_as_str(&method_name));
      if (method) {
        is_method = true; // Type methods take 'self'
      }

      // Look up in module globals (these are regular functions)
      if (!method && quo_var_is_module(object)) {
        QuoModule *mod = quo_var_as_module(object);
        QuoVar method_var;
        bool found = quo_ht_get(&mod->globals, quo_var_as_str(&method_name), &method_var);
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
        for (int i = 0; i < argc + 2; i++) quo_var_unref(quo__vm_peek(vm, i));
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
        for (int i = argc + 1; i > 0; i--) *quo__vm_peek(vm, i) = *quo__vm_peek(vm, i - 1);
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
  for (int i = 0; i < da_count(&vm->stack); i++) quo_var_unref(&da_at(&vm->stack, i));
  da_free(&vm->stack);
  da_free(&vm->frames);
  quo_dealloc(vm);
}

// -------------------- INIT / CLEANUP -------------------- //

void quo_init(const char *cwd) {
#define REGISTER_BUILTIN_METHOD(methods_table, method_name, fn)                                                                            \
  do {                                                                                                                                     \
    QuoCFn *cfn = quo_cfunction_new(method_name, -1, fn);                                                                                  \
    quo_ht_set(methods_table, cfn->name, quo_var_new_obj(cfn));                                                                            \
  } while (0)

  // Register global built-in functions
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "import", quo__builtin_import);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "type", quo__builtin_type);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "print", quo__builtin_print);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "input", quo__builtin_input);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "exit", quo__builtin_exit);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "bool", quo__builtin_bool);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "num", quo__builtin_num);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "str", quo__builtin_str);
  REGISTER_BUILTIN_METHOD(&quo__builtin_methods, "len", quo__builtin_len);

  // Register built-in methods for types
  // String
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "get", quo__str_method_get);
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "strip", quo__str_method_strip);
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "startswith", quo__str_method_startswith);
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "endswith", quo__str_method_endswith);
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "contains", quo__str_method_contains);
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "split", quo__str_method_split);
  REGISTER_BUILTIN_METHOD(&quo__str_methods, "replace", quo__str_method_replace);
  // Array
  REGISTER_BUILTIN_METHOD(&quo__arr_methods, "get", quo__arr_method_get);
  REGISTER_BUILTIN_METHOD(&quo__arr_methods, "set", quo__arr_method_set);
  REGISTER_BUILTIN_METHOD(&quo__arr_methods, "push", quo__arr_method_push);
  REGISTER_BUILTIN_METHOD(&quo__arr_methods, "pop", quo__arr_method_pop);
  // Dictionary
  REGISTER_BUILTIN_METHOD(&quo__dict_methods, "get", quo__dict_method_get);
  REGISTER_BUILTIN_METHOD(&quo__dict_methods, "set", quo__dict_method_set);
  REGISTER_BUILTIN_METHOD(&quo__dict_methods, "has", quo__dict_method_has);
  REGISTER_BUILTIN_METHOD(&quo__dict_methods, "del", quo__dict_method_del);
  REGISTER_BUILTIN_METHOD(&quo__dict_methods, "keys", quo__dict_method_keys);
  REGISTER_BUILTIN_METHOD(&quo__dict_methods, "values", quo__dict_method_values);

  // Register stdlib modules
  quo__mod_os_init(cwd);
  quo__mod_time_init(cwd);
  quo__mod_base64_init(cwd);
  quo__mod_uuid_init(cwd);
  quo__mod_env_init(cwd);

#undef REGISTER_BUILTIN_METHOD
}

void quo_cleanup() {
  quo_ht_free(&quo__builtin_methods);
  quo_ht_free(&quo__str_methods);
  quo_ht_free(&quo__arr_methods);
  quo_ht_free(&quo__dict_methods);
  quo_ht_free(&quo__imported_modules);
  quo_ht_free(&quo__types);

  // Free all interned strings
  for (int i = 0; i < quo__interned_strings.capacity; i++) {
    if (quo__interned_strings.items[i].key) {
      QuoStr *str = quo__interned_strings.items[i].key;
      quo_dealloc(str->data);
      quo_dealloc(str);
    }
  }
  da_free(&quo__interned_strings);
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
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("COND:\n");
    quo_debug_expression_print(stmt->loop.condition, indent + 2);
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

void quo_debug_ast_print(QuoStmtList *ast) {
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
  return "UNKNOWN";
}

int quo_debug_instruction_disassemble(QuoFn *fn, int offset) {
  printf("%04d: ", offset);
  QuoOP instruction = da_at(&fn->instructions, offset);
  offset++;
  switch (instruction) {
  case QUO_OP_CONSTANT: {
    int index = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d (", quo__debug_op_str(instruction), index);
    quo_var_print(&da_at(&fn->constants, index));
    printf(")\n");
    break;
  }
  case QUO_OP_ARRAY: {
    int count = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d\n", quo__debug_op_str(instruction), count);
    break;
  }
  case QUO_OP_DICT: {
    int pair_count = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d\n", quo__debug_op_str(instruction), pair_count);
    break;
  }
  case QUO_OP_GET_LOCAL:
  case QUO_OP_SET_LOCAL: {
    int index = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d\n", quo__debug_op_str(instruction), index);
    break;
  }
  case QUO_OP_GET_GLOBAL:
  case QUO_OP_SET_GLOBAL: {
    int index = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d (", quo__debug_op_str(instruction), index);
    quo_var_print(&da_at(&fn->constants, index));
    printf(")\n");
    break;
  }
  case QUO_OP_CALL: {
    int arity = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d\n", quo__debug_op_str(instruction), arity);
    break;
  }
  case QUO_OP_METHOD_CALL: {
    int arity = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d\n", quo__debug_op_str(instruction), arity);
    break;
  }
  case QUO_OP_JUMP:
  case QUO_OP_JUMP_IF_FALSE: {
    int jump_offset = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d -> %d\n", quo__debug_op_str(instruction), jump_offset, offset + jump_offset - 1);
    break;
  }
  case QUO_OP_LOOP: {
    int jump_offset = da_at(&fn->instructions, offset);
    offset++;
    printf("%s %d -> %d\n", quo__debug_op_str(instruction), jump_offset, offset - jump_offset - 1);
    break;
  }
  default: printf("%s\n", quo__debug_op_str(instruction)); break;
  }
  return offset;
}

#endif // QUO_IMPLEMENTATION
#endif // QUO_H
