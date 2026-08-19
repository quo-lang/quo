/*

DESCRIPTION:
    Quo module for Base64 encoding/decoding

QUO API:
    var base64 = import("base64")

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

static int quo__base64_decode_char(char c) {
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
  QuoStr *result = quo_str_new_tmp(quo_sb_string(&sb), da_count(&sb) - 1);
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
  QuoStr *result = quo_str_new_tmp(quo_sb_string(&sb), da_count(&sb) - 1);
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

// ---------- PUBLIC API ---------- //

static inline void quo_mod_base64_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "base64", NULL, NULL);
  quo_module_register_cfn(m, "encode", -1, quo__mod_base64_encode);
  quo_module_register_cfn(m, "decode", -1, quo__mod_base64_decode);
  quo_module_register_cfn(m, "encode_url", -1, quo__mod_base64_encode_url);
  quo_module_register_cfn(m, "decode_url", -1, quo__mod_base64_decode_url);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_BASE64_H
