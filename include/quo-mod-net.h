/*
DESCRIPTION:
    Network module for Quo. It's a wrapper around libcurl.

DEPENDENCIES:
    Add -lcurl to your linker flags if using this module.
*/

#pragma once

#include "quo.h"

#include <curl/curl.h>

// ---------- PRIVATE HELPERS ---------- //

// Callback for writing response data
static size_t quo__mod_net_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  QuoStringBuilder *sb = (QuoStringBuilder *)userp;
  const char *content = (const char *)contents;
  size_t total_size = size * nmemb;
  quo_sb_append(sb, content, total_size);
  return total_size;
}

// Helper to perform HTTP request
static QuoVar quo__mod_net_perform_request(const char *url, const char *method, const char *data, QuoDict *headers_dict) {
  void *curl = curl_easy_init();
  if (!curl) return quo_var_new_err("Failed to initialize curl");
  // Response buffer
  QuoStringBuilder response = quo_sb_new();
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, url);
  // Set method
  if (method) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
  // Set POST data (Request body)
  if (data) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
  // Set write callback
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, quo__mod_net_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  // Set headers
  void *header_list = NULL;
  if (headers_dict) {
    for (int i = 0; i < headers_dict->dict.capacity; i++) {
      QuoHashTableEntry *entry = &headers_dict->dict.items[i];
      if (entry->key) {
        char *header = quo_strdupf("%s: %s", entry->key->data, quo_var_is_str(&entry->value) ? quo_var_as_str(&entry->value)->data : "");
        header_list = curl_slist_append(header_list, header);
        quo_dealloc(header);
      }
    }
    if (header_list) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  }
  // Follow redirects
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long)1);
  // Perform request
  CURLcode res = curl_easy_perform(curl);
  // Cleanup
  if (header_list) curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    quo_sb_free(&response);
    const char *error = curl_easy_strerror(res);
    return quo_var_new_err(error ? error : "Unknown curl error");
  }
  quo_sb_null_terminate(&response);
  QuoStr *result = quo_str_new_raw(quo_sb_string(&response), response.count);
  quo_sb_free(&response);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_net_get(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.get() requires a URL string");
  return quo__mod_net_perform_request(quo_var_as_str(&argv[0])->data, "GET", NULL, NULL);
}

static inline QuoVar quo__mod_net_post(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.post() requires a URL string");
  const char *data = NULL;
  if (argc >= 2 && quo_var_is_str(&argv[1])) data = quo_var_as_str(&argv[1])->data;
  return quo__mod_net_perform_request(quo_var_as_str(&argv[0])->data, "POST", data, NULL);
}

static inline QuoVar quo__mod_net_put(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.put() requires a URL string");
  const char *data = NULL;
  if (argc >= 2 && quo_var_is_str(&argv[1])) data = quo_var_as_str(&argv[1])->data;
  return quo__mod_net_perform_request(quo_var_as_str(&argv[0])->data, "PUT", data, NULL);
}

static inline QuoVar quo__mod_net_patch(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.patch() requires a URL string");
  const char *data = NULL;
  if (argc >= 2 && quo_var_is_str(&argv[1])) data = quo_var_as_str(&argv[1])->data;
  return quo__mod_net_perform_request(quo_var_as_str(&argv[0])->data, "PATCH", data, NULL);
}

static inline QuoVar quo__mod_net_delete(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.delete() requires a URL string");
  return quo__mod_net_perform_request(quo_var_as_str(&argv[0])->data, "DELETE", NULL, NULL);
}

static inline QuoVar quo__mod_net_request(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("net.request() requires URL and method strings");
  const char *url = quo_var_as_str(&argv[0])->data;
  const char *method = quo_var_as_str(&argv[1])->data;
  // Optional body
  const char *data = NULL;
  if (argc >= 3 && quo_var_is_str(&argv[2])) data = quo_var_as_str(&argv[2])->data;
  // Optional headers
  QuoDict *headers = NULL;
  if (argc >= 4 && quo_var_is_dict(&argv[3])) headers = quo_var_as_dict(&argv[3]);
  return quo__mod_net_perform_request(url, method, data, headers);
}

static inline QuoVar quo__mod_net_encode(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.url_encode() requires a string argument");
  void *curl = curl_easy_init();
  if (!curl) return quo_var_new_err("Failed to initialize curl");
  char *encoded = curl_easy_escape(curl, quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[0])->len);
  curl_easy_cleanup(curl);
  if (!encoded) return quo_var_new_err("URL encoding failed");
  QuoStr *result = quo_str_new(encoded, -1);
  free(encoded); // curl_easy_escape returns malloc'd memory
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_net_decode(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.url_decode() requires a string argument");
  void *curl = curl_easy_init();
  if (!curl) return quo_var_new_err("Failed to initialize curl");
  int out_len;
  char *decoded = curl_easy_unescape(curl, quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[0])->len, &out_len);
  curl_easy_cleanup(curl);
  if (!decoded) return quo_var_new_err("URL decoding failed");
  QuoStr *result = quo_str_new(decoded, out_len);
  free(decoded); // curl_easy_unescape returns malloc'd memory
  return quo_var_new_obj(result);
}

static inline void quo_mod_net_load() {
  QuoModule *m = quo_module_new(NULL, "net", NULL, NULL);
  quo_module_register_cfn(m, "get", -1, quo__mod_net_get);
  quo_module_register_cfn(m, "post", -1, quo__mod_net_post);
  quo_module_register_cfn(m, "put", -1, quo__mod_net_put);
  quo_module_register_cfn(m, "patch", -1, quo__mod_net_patch);
  quo_module_register_cfn(m, "delete", -1, quo__mod_net_delete);
  quo_module_register_cfn(m, "encode", -1, quo__mod_net_encode);
  quo_module_register_cfn(m, "decode", -1, quo__mod_net_decode);
  // Initialize curl globally
  curl_global_init(CURL_GLOBAL_ALL);
}
