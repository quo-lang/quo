/*

DESCRIPTION:
    Quo module for UUID generation/parsing

QUO API:
    var uuid = import("uuid")

    # Generate a random UUID v4
    var id = uuid.v4() # Returns "550e8400-e29b-41d4-a716-446655440000"

    # Generate a UUID v7 (time-ordered)
    var id = uuid.v7() # Returns "018f2e3a-1234-7abc-9876-543210fedcba"

    # Parse and validate UUID string
    var info = uuid.parse("550e8400-e29b-41d4-a716-446655440000")
    # Returns dict: { "valid": true, "version": 4, "variant": "RFC4122" }

    # Check if string is a valid UUID
    var ok = uuid.is_valid("550e8400-e29b-41d4-a716-446655440000") # Returns true

*/

#ifndef QUO_MOD_UUID_H
#define QUO_MOD_UUID_H

#include "quo.h"

#ifdef _WIN32
#include <wincrypt.h>
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---------- UUID IMPLEMENTATION ---------- //

static void quo__uuid_random_bytes(unsigned char *buf, int len) {
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

static uint64_t quo__uuid_timestamp_ms(void) {
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  return (t / 10000) - 11644473600000ULL;
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static void quo__uuid_format(char *buf, unsigned char *bytes) {
  snprintf(buf, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0], bytes[1], bytes[2], bytes[3],
           bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

static bool quo__uuid_parse_bytes(const char *str, unsigned char *bytes) {
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

static int quo__uuid_version(unsigned char *bytes) { return (bytes[6] >> 4) & 0x0F; }

static const char *quo__uuid_variant(unsigned char *bytes) {
  int variant = (bytes[8] >> 6) & 0x03;
  switch (variant) {
  case 0: return "NCS";
  case 1: return "RFC4122";
  case 2: return "Microsoft";
  case 3: return "Future";
  }
  return "Unknown";
}

// ---------- PRIVATE API ---------- //

static inline QuoVar quo__mod_uuid_v4(QuoModule *m, int argc, QuoVar *argv) {
  unsigned char bytes[16];
  quo__uuid_random_bytes(bytes, 16);
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  char str[37];
  quo__uuid_format(str, bytes);
  return quo_var_new_obj(quo_str_new(m, str, -1));
}

static inline QuoVar quo__mod_uuid_v7(QuoModule *m, int argc, QuoVar *argv) {
  unsigned char bytes[16];
  uint64_t timestamp = quo__uuid_timestamp_ms();
  bytes[0] = (timestamp >> 40) & 0xFF;
  bytes[1] = (timestamp >> 32) & 0xFF;
  bytes[2] = (timestamp >> 24) & 0xFF;
  bytes[3] = (timestamp >> 16) & 0xFF;
  bytes[4] = (timestamp >> 8) & 0xFF;
  bytes[5] = timestamp & 0xFF;
  quo__uuid_random_bytes(&bytes[6], 10);
  bytes[6] = (bytes[6] & 0x0F) | 0x70;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  char str[37];
  quo__uuid_format(str, bytes);
  return quo_var_new_obj(quo_str_new(m, str, -1));
}

static inline QuoVar quo__mod_uuid_parse(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("uuid.parse() requires a string argument");
  unsigned char bytes[16];
  if (!quo__uuid_parse_bytes(quo_var_as_str(&argv[0])->data, bytes)) {
    QuoDict *result = quo_dict_new();
    QuoStr *key = quo_str_new(m, "valid", -1);
    QuoVar val = quo_var_new_bool(false);
    quo_dict_set(result, key, &val);
    return quo_var_new_obj(result);
  }
  QuoDict *result = quo_dict_new();
  QuoStr *key_valid = quo_str_new(m, "valid", -1);
  QuoVar val_valid = quo_var_new_bool(true);
  quo_dict_set(result, key_valid, &val_valid);
  QuoStr *key_version = quo_str_new(m, "version", -1);
  QuoVar val_version = quo_var_new_num(quo__uuid_version(bytes));
  quo_dict_set(result, key_version, &val_version);
  QuoStr *key_variant = quo_str_new(m, "variant", -1);
  QuoVar val_variant = quo_var_new_obj(quo_str_new(m, quo__uuid_variant(bytes), -1));
  quo_dict_set(result, key_variant, &val_variant);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_uuid_is_valid(QuoModule *m, int argc, QuoVar *argv) {
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_bool(false);
  unsigned char bytes[16];
  return quo_var_new_bool(quo__uuid_parse_bytes(quo_var_as_str(&argv[0])->data, bytes));
}

// ---------- PUBLIC API ---------- //

static inline void quo_mod_uuid_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "uuid", NULL, NULL);
  quo_module_register_cfn(m, "v4", -1, quo__mod_uuid_v4);
  quo_module_register_cfn(m, "v7", -1, quo__mod_uuid_v7);
  quo_module_register_cfn(m, "parse", -1, quo__mod_uuid_parse);
  quo_module_register_cfn(m, "is_valid", -1, quo__mod_uuid_is_valid);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_UUID_H
