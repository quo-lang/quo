#ifndef QUO_FMT_H
#define QUO_FMT_H

#include "../quo.h"

// Formatter for Quo modules
typedef struct {
  QuoStringBuilder sb;
  bool indent;
  bool lock_indent;
} QuoFormatter;

// ---------- PRIVATE FUNCTIONS ---------- //

static inline void quo__fmt_indent(QuoFormatter *f) {
  for (int i = 0; i < f->indent; i++) quo_sb_append_cstr(&f->sb, "    ");
}

static void quo__fmt_expr(QuoFormatter *f, QuoExpr *expr) {
  switch (expr->type) {
  case QUO_EXPR_LITERAL: {
    QuoToken value = expr->token;
    if (value.type == QUO_TT_LITERAL_STR) {
      quo_sb_append_cstr(&f->sb, "\"");
      quo_sb_append(&f->sb, value.start, value.len);
      quo_sb_append_cstr(&f->sb, "\"");
    } else quo_sb_append(&f->sb, value.start, value.len);
  } break;
  case QUO_EXPR_VARIABLE: {
    QuoToken name = expr->token;
    quo_sb_append(&f->sb, name.start, name.len);
    break;
  }
  case QUO_EXPR_BINARY: {
    quo__fmt_expr(f, expr->binary.left);
    quo_sb_append_cstr(&f->sb, " ");
    quo_sb_append_cstr(&f->sb, quo_token_type_str(expr->binary.op.type));
    quo_sb_append_cstr(&f->sb, " ");
    quo__fmt_expr(f, expr->binary.right);
    break;
  }
  case QUO_EXPR_UNARY: {
    quo_sb_append_cstr(&f->sb, quo_token_type_str(expr->unary.op.type));
    quo__fmt_expr(f, expr->unary.expr);
    break;
  }
  case QUO_EXPR_GROUPING: {
    quo_sb_append_cstr(&f->sb, "(");
    quo__fmt_expr(f, expr->unary.expr);
    quo_sb_append_cstr(&f->sb, ")");
    break;
  }
  case QUO_EXPR_CALL: {
    quo__fmt_expr(f, expr->call.callee);
    quo_sb_append_cstr(&f->sb, "(");
    for (int i = 0; i < da_count(&expr->call.arguments); i++) {
      QuoExpr *arg = da_at(&expr->call.arguments, i);
      quo__fmt_expr(f, arg);
      if (i < da_count(&expr->call.arguments) - 1) quo_sb_append_cstr(&f->sb, ", ");
    }
    quo_sb_append_cstr(&f->sb, ")");
    break;
  }
  case QUO_EXPR_ASSIGN: {
    quo__fmt_expr(f, expr->assign.target);
    quo_sb_append_cstr(&f->sb, " ");
    quo_sb_append_cstr(&f->sb, quo_token_type_str(expr->token.type));
    quo_sb_append_cstr(&f->sb, " ");
    quo__fmt_expr(f, expr->assign.value);
    break;
  }
  case QUO_EXPR_TERNARY: {
    quo__fmt_expr(f, expr->ternary.condition);
    quo_sb_append_cstr(&f->sb, " ? ");
    quo__fmt_expr(f, expr->ternary.then_expr);
    quo_sb_append_cstr(&f->sb, " : ");
    quo__fmt_expr(f, expr->ternary.else_expr);
    break;
  }
  case QUO_EXPR_MEMBER_ACCESS: {
    quo__fmt_expr(f, expr->member_access.object);
    quo_sb_append_cstr(&f->sb, ".");
    quo_sb_append(&f->sb, expr->member_access.member.start, expr->member_access.member.len);
    break;
  }
  case QUO_EXPR_ARRAY:
  case QUO_EXPR_DICT: break;
  case QUO_EXPR_FUNCTION: break;
  }
}

static void quo__fmt_stmt(QuoFormatter *f, QuoStmt *stmt) {
  switch (stmt->type) {
  case QUO_STMT_BLOCK: {
    if (!f->lock_indent) quo__fmt_indent(f);
    f->lock_indent = false;
    quo_sb_append_cstr(&f->sb, "{\n");
    f->indent++;
    for (int i = 0; i < da_count(&stmt->block); i++) quo__fmt_stmt(f, da_at(&stmt->block, i));
    f->indent--;
    quo__fmt_indent(f);
    quo_sb_append_cstr(&f->sb, "}\n");
  } break;
  case QUO_STMT_BREAK: {
    quo__fmt_indent(f);
    quo_sb_append_cstr(&f->sb, "break\n");
  } break;
  case QUO_STMT_CONTINUE: {
    quo__fmt_indent(f);
    quo_sb_append_cstr(&f->sb, "continue\n");
  } break;
  case QUO_STMT_RETURN: {
    quo__fmt_indent(f);
    quo_sb_append_cstr(&f->sb, "return");
    QuoExpr *expr = stmt->expression;
    if (expr) {
      quo_sb_append_cstr(&f->sb, " ");
      quo__fmt_expr(f, expr);
    }
    quo_sb_append_cstr(&f->sb, "\n");
  } break;
  case QUO_STMT_IF: {
    if (!f->lock_indent) quo__fmt_indent(f);
    quo_sb_append_cstr(&f->sb, "if ");
    quo__fmt_expr(f, stmt->if_stmt.condition);
    quo_sb_append_cstr(&f->sb, " ");
    f->lock_indent = true;
    quo__fmt_stmt(f, stmt->if_stmt.then_branch);
    QuoStmt *else_branch = stmt->if_stmt.else_branch;
    if (else_branch) {
      quo__fmt_indent(f);
      quo_sb_append_cstr(&f->sb, "else ");
      f->lock_indent = true;
      quo__fmt_stmt(f, else_branch);
    }
  } break;
  case QUO_STMT_LOOP: {
    quo__fmt_indent(f);
    quo_sb_append_cstr(&f->sb, "loop (");
    // Initializer
    if (stmt->loop.initializer) {
      f->lock_indent = true;
      quo__fmt_stmt(f, stmt->loop.initializer);
      f->lock_indent = false;
      // Remove trailing newline if present
      if (quo_sb_string(&f->sb)[f->sb.count - 1] == '\n') f->sb.count--;
    }
    quo_sb_append_cstr(&f->sb, ", ");
    // Condition
    if (stmt->loop.condition) quo__fmt_expr(f, stmt->loop.condition);
    quo_sb_append_cstr(&f->sb, ", ");
    // Increment
    if (stmt->loop.increment) {
      f->lock_indent = true;
      quo__fmt_stmt(f, stmt->loop.increment);
      f->lock_indent = false;
      // Remove trailing newline if present
      if (quo_sb_string(&f->sb)[f->sb.count - 1] == '\n') f->sb.count--;
    }
    quo_sb_append_cstr(&f->sb, ") ");
    f->lock_indent = true;
    quo__fmt_stmt(f, stmt->loop.body);
  } break;
  case QUO_STMT_EXPRESSION:
    if (!f->lock_indent) quo__fmt_indent(f);
    quo__fmt_expr(f, stmt->expression);
    quo_sb_append_cstr(&f->sb, "\n");
    break;
  case QUO_STMT_VAR_DECL: break;
  }
}

// ---------- PUBLIC API ---------- //

// Creates a new formatter
static inline QuoFormatter quo_fmt_new() { return (QuoFormatter){0}; }

// Formats a module
void quo_fmt_format(QuoFormatter *f, QuoAST ast);

// Returns the formatted string
const char *quo_fmt_get_string(QuoFormatter *f) {
  if (!f) return NULL;
  return quo_sb_string(&f->sb);
}

// Frees the formatter
void quo_fmt_free(QuoFormatter *f) {
  if (!f) return;
  quo_sb_free(&f->sb);
}

#endif // QUO_FMT_H
