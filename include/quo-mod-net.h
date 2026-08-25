/*

DESCRIPTION:
    Quo module for HTTP/networking using libcurl.
    It will load libcurl dynamically using the system's dynamic linker.

QUO API:
    var net = import("net")

    # GET request
    var response = net.get("https://api.example.com/data")

    # POST request with data
    var response = net.post("https://api.example.com/data", "{\"key\": \"value\"}")

    # PUT request with data
    var response = net.put("https://api.example.com/data/1", "{\"key\": \"updated\"}")

    # PATCH request with data
    var response = net.patch("https://api.example.com/data/1", "{\"key\": \"patched\"}")

    # DELETE request
    var response = net.delete("https://api.example.com/data/1")

    # Custom request with headers
    var headers = { "Content-Type": "application/json", "Authorization": "Bearer token123" }
    var response = net.request("https://api.example.com/data", "PUT", "{\"key\": \"value\"}", headers)

    # URL encode a string
    var encoded = net.encode("hello world") # Returns "hello%20world"

    # URL decode a string
    var decoded = net.decode("hello%20world") # Returns "hello world"

*/

#ifndef QUO_MOD_NET_H
#define QUO_MOD_NET_H

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- LIBCURL FUNCTION POINTERS ---------- //

enum {
  CURLOPT_URL = 10002,
  CURLOPT_CUSTOMREQUEST = 10036,
  CURLOPT_POSTFIELDS = 10015,
  CURLOPT_WRITEFUNCTION = 20011,
  CURLOPT_WRITEDATA = 10001,
  CURLOPT_HTTPHEADER = 10023,
  CURLOPT_FOLLOWLOCATION = 52,
  CURLE_OK = 0,
  CURL_GLOBAL_ALL = 3,
};

typedef void *(*curl_easy_init_t)(void);
typedef void (*curl_easy_cleanup_t)(void *);
typedef void *(*curl_easy_setopt_t)(void *, int, ...);
typedef int (*curl_easy_perform_t)(void *);
typedef char *(*curl_easy_escape_t)(void *, const char *, int);
typedef char *(*curl_easy_unescape_t)(void *, const char *, int, int *);
typedef const char *(*curl_easy_strerror_t)(int);
typedef void *(*curl_slist_append_t)(void *, const char *);
typedef void (*curl_slist_free_all_t)(void *);
typedef int (*curl_global_init_t)(long);
typedef size_t (*curl_write_callback_t)(char *, size_t, size_t, void *);
typedef int CURLoption;
typedef int CURLcode;

// Static function pointers
static struct {
  void *handle;
  curl_easy_init_t easy_init;
  curl_easy_cleanup_t easy_cleanup;
  curl_easy_setopt_t easy_setopt;
  curl_easy_perform_t easy_perform;
  curl_easy_escape_t easy_escape;
  curl_easy_unescape_t easy_unescape;
  curl_easy_strerror_t easy_strerror;
  curl_slist_append_t slist_append;
  curl_slist_free_all_t slist_free_all;
  curl_global_init_t global_init;
} quo__curl = {0};

// ---------- PRIVATE HELPERS ---------- //

// Try to load libcurl dynamically. Returns false if libcurl is not available.
static inline bool quo__mod_net_load(void) {
  if (quo__curl.handle) return true; // Already loaded

  // Try different library names based on platform
#ifdef _WIN32
  const char *lib_names[] = {"libcurl.dll", "libcurl-4.dll"};
#elif defined(__APPLE__)
  const char *lib_names[] = {"libcurl.dylib", "libcurl.4.dylib"};
#else
  const char *lib_names[] = {"libcurl.so", "libcurl.so.4", "libcurl.so.4.8.0"};
#endif

  for (size_t i = 0; i < sizeof(lib_names) / sizeof(lib_names[0]); i++) {
    quo__curl.handle = dlopen(lib_names[i], RTLD_LAZY);
    if (quo__curl.handle) break;
  }
  if (!quo__curl.handle) return false;

  // Load function pointers
  quo__curl.easy_init = (curl_easy_init_t)dlsym(quo__curl.handle, "curl_easy_init");
  quo__curl.easy_cleanup = (curl_easy_cleanup_t)dlsym(quo__curl.handle, "curl_easy_cleanup");
  quo__curl.easy_setopt = (curl_easy_setopt_t)dlsym(quo__curl.handle, "curl_easy_setopt");
  quo__curl.easy_perform = (curl_easy_perform_t)dlsym(quo__curl.handle, "curl_easy_perform");
  quo__curl.easy_escape = (curl_easy_escape_t)dlsym(quo__curl.handle, "curl_easy_escape");
  quo__curl.easy_unescape = (curl_easy_unescape_t)dlsym(quo__curl.handle, "curl_easy_unescape");
  quo__curl.easy_strerror = (curl_easy_strerror_t)dlsym(quo__curl.handle, "curl_easy_strerror");
  quo__curl.slist_append = (curl_slist_append_t)dlsym(quo__curl.handle, "curl_slist_append");
  quo__curl.slist_free_all = (curl_slist_free_all_t)dlsym(quo__curl.handle, "curl_slist_free_all");
  quo__curl.global_init = (curl_global_init_t)dlsym(quo__curl.handle, "curl_global_init");

  // Check all functions loaded
  if (!quo__curl.easy_init || !quo__curl.easy_cleanup || !quo__curl.easy_setopt || !quo__curl.easy_perform || !quo__curl.easy_escape ||
      !quo__curl.easy_unescape || !quo__curl.easy_strerror || !quo__curl.slist_append || !quo__curl.slist_free_all ||
      !quo__curl.global_init) {
    dlclose(quo__curl.handle);
    quo__curl.handle = NULL;
    return false;
  }
  // Initialize curl globally
  quo__curl.global_init(CURL_GLOBAL_ALL);
  return true;
}

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
  if (!quo__curl.handle) return quo_var_new_err("libcurl not loaded");
  void *curl = quo__curl.easy_init();
  if (!curl) return quo_var_new_err("Failed to initialize curl");
  // Response buffer
  QuoStringBuilder response = quo_sb_new();
  // Set URL
  quo__curl.easy_setopt(curl, CURLOPT_URL, url);
  // Set method
  if (method) quo__curl.easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
  // Set POST data (Request body)
  if (data) quo__curl.easy_setopt(curl, CURLOPT_POSTFIELDS, data);
  // Set write callback
  quo__curl.easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback_t)quo__mod_net_write_callback);
  quo__curl.easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  // Set headers
  void *header_list = NULL;
  if (headers_dict) {
    for (int i = 0; i < headers_dict->dict.capacity; i++) {
      QuoHashTableEntry *entry = &headers_dict->dict.items[i];
      if (entry->key) {
        char *header = quo_strdupf("%s: %s", entry->key->data, quo_var_is_str(&entry->value) ? quo_var_as_str(&entry->value)->data : "");
        header_list = quo__curl.slist_append(header_list, header);
        quo_dealloc(header);
      }
    }
    if (header_list) quo__curl.easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  }
  // Follow redirects
  quo__curl.easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long)1);
  // Perform request
  CURLcode res = quo__curl.easy_perform(curl);
  // Cleanup
  if (header_list) quo__curl.slist_free_all(header_list);
  quo__curl.easy_cleanup(curl);
  if (res != CURLE_OK) {
    quo_sb_free(&response);
    const char *error = quo__curl.easy_strerror(res);
    return quo_var_new_err(error ? error : "Unknown curl error");
  }
  quo_sb_null_terminate(&response);
  QuoStr *result = quo_str_new_raw(quo_sb_string(&response), response.count);
  quo_sb_free(&response);
  return quo_var_new_obj(result);
}

// ---------- PRIVATE API ---------- //

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
  if (!quo__curl.handle) return quo_var_new_err("libcurl not loaded");
  void *curl = quo__curl.easy_init();
  if (!curl) return quo_var_new_err("Failed to initialize curl");
  char *encoded = quo__curl.easy_escape(curl, quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[0])->len);
  quo__curl.easy_cleanup(curl);
  if (!encoded) return quo_var_new_err("URL encoding failed");
  QuoStr *result = quo_str_new(encoded, -1);
  free(encoded); // curl_easy_escape returns malloc'd memory
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_net_decode(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("net.url_decode() requires a string argument");
  if (!quo__curl.handle) return quo_var_new_err("libcurl not loaded");
  void *curl = quo__curl.easy_init();
  if (!curl) return quo_var_new_err("Failed to initialize curl");
  int out_len;
  char *decoded = quo__curl.easy_unescape(curl, quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[0])->len, &out_len);
  quo__curl.easy_cleanup(curl);
  if (!decoded) return quo_var_new_err("URL decoding failed");
  QuoStr *result = quo_str_new(decoded, out_len);
  free(decoded); // curl_easy_unescape returns malloc'd memory
  return quo_var_new_obj(result);
}

// Cleanup - unload libcurl
static inline void quo__mod_net_cleanup(QuoModule *m) {
  QUO_UNUSED(m);
  if (quo__curl.handle) {
    dlclose(quo__curl.handle);
    quo__curl.handle = NULL;
  }
}

// ---------- PUBLIC API ---------- //

// Register the net namespace. Returns false if libcurl is not available.
static inline void quo_mod_net_init(QuoModule *parent) {
  if (!quo__mod_net_load()) {
    fprintf(stderr, "Error: quo-mod-net: libcurl not found, 'net' module not available\n");
    return;
  }
  QuoModule *m = quo_module_new(parent->cwd, "net", NULL, quo__mod_net_cleanup);
  quo_module_register_cfn(m, "get", -1, quo__mod_net_get);
  quo_module_register_cfn(m, "post", -1, quo__mod_net_post);
  quo_module_register_cfn(m, "put", -1, quo__mod_net_put);
  quo_module_register_cfn(m, "patch", -1, quo__mod_net_patch);
  quo_module_register_cfn(m, "delete", -1, quo__mod_net_delete);
  quo_module_register_cfn(m, "encode", -1, quo__mod_net_encode);
  quo_module_register_cfn(m, "decode", -1, quo__mod_net_decode);
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_NET_H
