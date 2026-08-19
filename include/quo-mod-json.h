/*

DESCRIPTION:
    Quo module for JSON encoding/decoding (no dependencies)

QUO API:
    var json = import("json")

    # Parse JSON string to dictionary
    var data = json.decode('{"name": "John", "age": 30, "items": [1, 2, 3]}')

    # Encode dictionary to JSON string
    var str = json.encode(data)

*/

#ifndef QUO_MOD_JSON_H
#define QUO_MOD_JSON_H

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- JSON PARSER ---------- //

typedef struct {
  const char *json;
  int pos;
  int len;
  QuoModule *m;
} quo__json_parser;

static QuoVar quo__json_parse_value(quo__json_parser *p);

static void quo__json_skip_whitespace(quo__json_parser *p) {
  while (p->pos < p->len && (p->json[p->pos] == ' ' || p->json[p->pos] == '\t' || p->json[p->pos] == '\n' || p->json[p->pos] == '\r'))
    p->pos++;
}
static char quo__json_peek(quo__json_parser *p) { return p->pos < p->len ? p->json[p->pos] : '\0'; }
static char quo__json_next(quo__json_parser *p) { return p->pos < p->len ? p->json[p->pos++] : '\0'; }

// Parse string
static QuoVar quo__json_parse_string(quo__json_parser *p) {
  if (quo__json_next(p) != '"') return quo_var_new_err("Expected '\"'");

  QuoStringBuilder sb = quo_sb_new();

  while (p->pos < p->len && quo__json_peek(p) != '"') {
    char c = quo__json_next(p);
    if (c == '\\') {
      c = quo__json_next(p);
      switch (c) {
      case '"': da_add(&sb, '"'); break;
      case '\\': da_add(&sb, '\\'); break;
      case '/': da_add(&sb, '/'); break;
      case 'b': da_add(&sb, '\b'); break;
      case 'f': da_add(&sb, '\f'); break;
      case 'n': da_add(&sb, '\n'); break;
      case 'r': da_add(&sb, '\r'); break;
      case 't': da_add(&sb, '\t'); break;
      case 'u': {
        // Parse \uXXXX
        char hex[5] = {0};
        for (int i = 0; i < 4 && p->pos < p->len; i++) hex[i] = quo__json_next(p);
        unsigned int codepoint = (unsigned int)strtol(hex, NULL, 16);
        if (codepoint < 0x80) {
          da_add(&sb, (char)codepoint);
        } else if (codepoint < 0x800) {
          da_add(&sb, (char)(0xC0 | (codepoint >> 6)));
          da_add(&sb, (char)(0x80 | (codepoint & 0x3F)));
        } else {
          da_add(&sb, (char)(0xE0 | (codepoint >> 12)));
          da_add(&sb, (char)(0x80 | ((codepoint >> 6) & 0x3F)));
          da_add(&sb, (char)(0x80 | (codepoint & 0x3F)));
        }
        break;
      }
      default: da_add(&sb, c); break;
      }
    } else {
      da_add(&sb, c);
    }
  }
  if (quo__json_next(p) != '"') {
    quo_sb_free(&sb);
    return quo_var_new_err("Unterminated string");
  }
  quo_sb_null_terminate(&sb);
  QuoStr *str = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(str);
}
// Parse number
static QuoVar quo__json_parse_number(quo__json_parser *p) {
  int start = p->pos;
  if (quo__json_peek(p) == '-') p->pos++;
  while (p->pos < p->len && quo__json_peek(p) >= '0' && quo__json_peek(p) <= '9') p->pos++;
  if (p->pos < p->len && quo__json_peek(p) == '.') {
    p->pos++;
    while (p->pos < p->len && quo__json_peek(p) >= '0' && quo__json_peek(p) <= '9') p->pos++;
  }
  if (p->pos < p->len && (quo__json_peek(p) == 'e' || quo__json_peek(p) == 'E')) {
    p->pos++;
    if (quo__json_peek(p) == '+' || quo__json_peek(p) == '-') p->pos++;
    while (p->pos < p->len && quo__json_peek(p) >= '0' && quo__json_peek(p) <= '9') p->pos++;
  }
  int len = p->pos - start;
  QuoVar result = quo_var_new_num(quo_strtod(p->json + start, len));
  return result;
}

// Parse array
static QuoVar quo__json_parse_array(quo__json_parser *p) {
  quo__json_next(p); // Skip '['
  QuoArr *arr = quo_arr_new();
  quo__json_skip_whitespace(p);
  if (quo__json_peek(p) == ']') {
    quo__json_next(p);
    return quo_var_new_obj(arr);
  }
  while (true) {
    quo__json_skip_whitespace(p);
    QuoVar value = quo__json_parse_value(p);
    if (quo_var_is_err(&value)) {
      QuoVar obj = quo_var_new_obj(arr);
      quo_var_unref(&obj);
      return value;
    }
    quo_arr_push(arr, value);
    quo__json_skip_whitespace(p);
    if (quo__json_peek(p) == ']') {
      quo__json_next(p);
      break;
    }
    if (quo__json_peek(p) != ',') {
      QuoVar obj = quo_var_new_obj(arr);
      quo_var_unref(&obj);
      return quo_var_new_err("Expected ',' or ']'");
    }
    quo__json_next(p);
  }
  return quo_var_new_obj(arr);
}

// Parse object
static QuoVar quo__json_parse_object(quo__json_parser *p) {
  quo__json_next(p); // Skip '{'
  QuoDict *dict = quo_dict_new();
  quo__json_skip_whitespace(p);
  if (quo__json_peek(p) == '}') {
    quo__json_next(p);
    return quo_var_new_obj(dict);
  }
  while (true) {
    quo__json_skip_whitespace(p);
    QuoVar key = quo__json_parse_string(p);
    if (quo_var_is_err(&key)) {
      QuoVar obj = quo_var_new_obj(dict);
      quo_var_unref(&obj);
      return key;
    }
    quo__json_skip_whitespace(p);
    if (quo__json_next(p) != ':') {
      QuoVar obj = quo_var_new_obj(dict);
      quo_var_unref(&obj);
      return quo_var_new_err("Expected ':'");
    }
    quo__json_skip_whitespace(p);
    QuoVar value = quo__json_parse_value(p);
    if (quo_var_is_err(&value)) {
      QuoVar obj = quo_var_new_obj(dict);
      quo_var_unref(&obj);
      return value;
    }
    quo_dict_set(dict, quo_var_as_str(&key), &value);
    quo__json_skip_whitespace(p);
    if (quo__json_peek(p) == '}') {
      quo__json_next(p);
      break;
    }
    if (quo__json_peek(p) != ',') {
      QuoVar obj = quo_var_new_obj(dict);
      quo_var_unref(&obj);
      return quo_var_new_err("Expected ',' or '}'");
    }
    quo__json_next(p);
  }
  return quo_var_new_obj(dict);
}

// Parse value
static QuoVar quo__json_parse_value(quo__json_parser *p) {
  quo__json_skip_whitespace(p);
  char c = quo__json_peek(p);
  if (c == '"') return quo__json_parse_string(p);
  if (c == '{') return quo__json_parse_object(p);
  if (c == '[') return quo__json_parse_array(p);
  if (c == 't' || c == 'f') {
    if (strncmp(p->json + p->pos, "true", 4) == 0) {
      p->pos += 4;
      return quo_var_new_bool(true);
    }
    if (strncmp(p->json + p->pos, "false", 5) == 0) {
      p->pos += 5;
      return quo_var_new_bool(false);
    }
    return quo_var_new_err("Invalid value");
  }
  if (c == 'n') {
    if (strncmp(p->json + p->pos, "null", 4) == 0) {
      p->pos += 4;
      return quo_var_new_nil();
    }
    return quo_var_new_err("Invalid value");
  }
  if (c == '-' || (c >= '0' && c <= '9')) return quo__json_parse_number(p);
  return quo_var_new_err("Unexpected character");
}

// ---------- JSON STRINGIFY ---------- //

static void quo__json_stringify_value(QuoStringBuilder *sb, QuoVar *v);

static void quo__json_stringify_string(QuoStringBuilder *sb, const char *str) {
  da_add(sb, '"');
  for (const char *c = str; *c; c++) {
    switch (*c) {
    case '"': quo_sb_append_cstr(sb, "\\\""); break;
    case '\\': quo_sb_append_cstr(sb, "\\\\"); break;
    case '\b': quo_sb_append_cstr(sb, "\\b"); break;
    case '\f': quo_sb_append_cstr(sb, "\\f"); break;
    case '\n': quo_sb_append_cstr(sb, "\\n"); break;
    case '\r': quo_sb_append_cstr(sb, "\\r"); break;
    case '\t': quo_sb_append_cstr(sb, "\\t"); break;
    default:
      if ((unsigned char)*c < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*c);
        quo_sb_append_cstr(sb, buf);
      } else {
        da_add(sb, *c);
      }
    }
  }
  da_add(sb, '"');
}

static void quo__json_stringify_value(QuoStringBuilder *sb, QuoVar *v) {
  switch (v->type) {
  case QUO_VAR_TYPE_NIL: quo_sb_append_cstr(sb, "null"); break;
  case QUO_VAR_TYPE_BOOL: quo_sb_append_cstr(sb, v->val_num ? "true" : "false"); break;
  case QUO_VAR_TYPE_NUM: {
    char buf[318];
    snprintf(buf, sizeof(buf), "%g", v->val_num);
    quo_sb_append_cstr(sb, buf);
    break;
  }
  case QUO_VAR_TYPE_OBJ: {
    switch (v->val_obj->type) {
    case QUO_OBJ_TYPE_STR: quo__json_stringify_string(sb, quo_var_as_str(v)->data); break;
    case QUO_OBJ_TYPE_ARR: {
      da_add(sb, '[');
      QuoArr *arr = quo_var_as_arr(v);
      for (int i = 0; i < quo_arr_len(arr); i++) {
        if (i > 0) da_add(sb, ',');
        QuoVar val = quo_arr_get(arr, i);
        quo__json_stringify_value(sb, &val);
      }
      da_add(sb, ']');
      break;
    }
    case QUO_OBJ_TYPE_DICT: {
      da_add(sb, '{');
      bool first = true;
      for (int i = 0; i < quo_var_as_dict(v)->dict.capacity; i++) {
        QuoHashTableEntry *entry = &quo_var_as_dict(v)->dict.items[i];
        if (entry->key) {
          if (!first) da_add(sb, ',');
          quo__json_stringify_string(sb, entry->key->data);
          da_add(sb, ':');
          quo__json_stringify_value(sb, &entry->value);
          first = false;
        }
      }
      da_add(sb, '}');
      break;
    }
    default: quo_sb_append_cstr(sb, "null");
    }
    break;
  }
  default: quo_sb_append_cstr(sb, "null");
  }
}

// ---------- PRIVATE API ---------- //

static inline QuoVar quo__mod_json_decode(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("json.decode() requires a string argument");
  quo__json_parser parser;
  parser.m = m;
  parser.json = quo_var_as_str(&argv[0])->data;
  parser.len = quo_var_as_str(&argv[0])->len;
  parser.pos = 0;
  QuoVar result = quo__json_parse_value(&parser);
  return result;
}

static inline QuoVar quo__mod_json_encode(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1) return quo_var_new_err("json.encode() requires one argument");
  QuoStringBuilder sb = quo_sb_new();
  quo__json_stringify_value(&sb, &argv[0]);
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_json_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "json", NULL, NULL);
  quo_module_register_cfn(m, "encode", -1, quo__mod_json_encode);
  quo_module_register_cfn(m, "decode", -1, quo__mod_json_decode);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_JSON_H
