/*

DESCRIPTION:
    Quo module for Base64 encoding/decoding

C API:
    #include "quo-mod-base64.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_base64_init, NULL);
    ...

QUO API:
    # Encode string to base64
    var encoded = base64.encode("Hello, World!") # Returns "SGVsbG8sIFdvcmxkIQ=="

    # Decode base64 string
    var decoded = base64.decode("SGVsbG8sIFdvcmxkIQ==") # Returns "Hello, World!"

    # Encode with URL-safe alphabet
    var url_safe = base64.encode_url("Hello, World!") # Uses - and _ instead of + and /

    # Decode URL-safe base64
    var decoded = base64.decode_url("SGVsbG8sIFdvcmxkIQ==") # Handles both standard and URL-safe

*/

#ifndef QUO_MOD_BASE64_H
#define QUO_MOD_BASE64_H

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- PRIVATE API ---------- //

static const char quo__base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char quo__base64_url_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int quo__base64_decode_char(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+' || c == '-') return 62;
  if (c == '/' || c == '_') return 63;
  return -1;
}

static QuoVar quo__mod_base64_encode_impl(QuoState *s, const char *input, int len, const char *table) {
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
  QuoStr *result = quo_str_new(s, quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

static QuoVar quo__mod_base64_decode_impl(QuoState *s, const char *input, int len) {
  QuoStringBuilder sb = quo_sb_new();
  int padding = 0;
  if (len > 0 && input[len - 1] == '=') padding++;
  if (len > 1 && input[len - 2] == '=') padding++;

  for (int i = 0; i < len; i += 4) {
    int remaining = len - i;
    unsigned char sextet[4] = {0};
    int valid_sextets = 0;
    for (int j = 0; j < 4 && (i + j) < len; j++) {
      char c = input[i + j];
      if (c == '=') sextet[j] = 0;
      else {
        int val = quo__base64_decode_char(c);
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
  QuoStr *result = quo_str_new(s, quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_base64_encode(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("base64.encode() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[1].val_obj);
  return quo__mod_base64_encode_impl(s, str->data, str->len, quo__base64_table);
}

static inline QuoVar quo__mod_base64_decode(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("base64.decode() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[1].val_obj);
  return quo__mod_base64_decode_impl(s, str->data, str->len);
}

static inline QuoVar quo__mod_base64_encode_url(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("base64.encode_url() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[1].val_obj);
  return quo__mod_base64_encode_impl(s, str->data, str->len, quo__base64_url_table);
}

static inline QuoVar quo__mod_base64_decode_url(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("base64.decode_url() requires a string argument");
  QuoStr *str = quo_obj_as_str(argv[1].val_obj);
  return quo__mod_base64_decode_impl(s, str->data, str->len);
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_base64_init(QuoState *s) {
  QuoDict *ns = quo_state_register_namespace(s, "base64");
  quo_state_namespace_add_cfn(s, ns, "encode", quo__mod_base64_encode);
  quo_state_namespace_add_cfn(s, ns, "decode", quo__mod_base64_decode);
  quo_state_namespace_add_cfn(s, ns, "encode_url", quo__mod_base64_encode_url);
  quo_state_namespace_add_cfn(s, ns, "decode_url", quo__mod_base64_decode_url);
  return true;
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_BASE64_H
