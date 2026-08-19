/*

DESCRIPTION:
    Quo module for CSV parsing/generation

C API:
    #include "quo-mod-csv.h"
    ...
    QuoModule *m = quo_module_new(...);
    quo_mod_csv_init(m);
    ...

QUO API:
    var csv = import("csv")

    # Parse CSV string to array of arrays
    var rows = csv.parse("name,age,city\nJohn,30,NYC\nJane,25,LA")
    # Returns: [["name", "age", "city"], ["John", "30", "NYC"], ["Jane", "25", "LA"]]

    # Parse CSV with custom delimiter
    var rows = csv.parse("name;age;city\nJohn;30;NYC", ";")

    # Parse CSV with headers (returns array of dicts)
    var records = csv.parse_dict("name,age,city\nJohn,30,NYC\nJane,25,LA")
    # Returns: [{ "name": "John", "age": "30", "city": "NYC" }, { "name": "Jane", "age": "25", "city": "LA" }]

    # Stringify array to CSV
    var csv_str = csv.stringify([["name", "age"], ["John", "30"], ["Jane", "25"]])
    # Returns: "name,age\nJohn,30\nJane,25"

    # Stringify array of dicts to CSV
    var csv_str = csv.stringify_dict([{ "name": "John", "age": "30" }, { "name": "Jane", "age": "25" }])
    # Returns: "name,age\nJohn,30\nJane,25"

*/

#ifndef QUO_MOD_CSV_H
#define QUO_MOD_CSV_H

#include "quo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- PRIVATE API ---------- //

// Parse a single CSV field, handling quoted fields
static QuoVar quo__csv_parse_field(const char *str, int *pos, int len, char delimiter) {
  QuoStringBuilder sb = quo_sb_new();
  int i = *pos;
  // Check for quoted field
  if (i < len && str[i] == '"') {
    i++; // Skip opening quote
    while (i < len) {
      if (str[i] == '"') {
        if (i + 1 < len && str[i + 1] == '"') {
          da_add(&sb, '"'); // Escaped quote
          i += 2;
        } else {
          i++; // Closing quote
          break;
        }
      } else {
        da_add(&sb, str[i]);
        i++;
      }
    }
    // Skip delimiter or newline after closing quote
    if (i < len && str[i] != delimiter && str[i] != '\n' && str[i] != '\r') {
      quo_sb_free(&sb);
      *pos = i;
      return quo_var_new_err("Unexpected character after quoted field");
    }
  } else {
    // Unquoted field
    while (i < len && str[i] != delimiter && str[i] != '\n' && str[i] != '\r') {
      da_add(&sb, str[i]);
      i++;
    }
  }
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  *pos = i;
  return quo_var_new_obj(result);
}

// Parse a single CSV line
static QuoVar quo__csv_parse_line(const char *str, int *pos, int len, char delimiter) {
  QuoArr *arr = quo_arr_new();
  int i = *pos;
  if (i >= len || (str[i] == '\n' || str[i] == '\r')) {
    *pos = i;
    return quo_var_new_obj(arr);
  }
  while (i < len && str[i] != '\n' && str[i] != '\r') {
    QuoVar field = quo__csv_parse_field(str, &i, len, delimiter);
    if (quo_var_is_err(&field)) {
      QuoVar obj = quo_var_new_obj(arr);
      quo_var_unref(&obj);
      return field;
    }
    quo_arr_push(arr, field);
    // Skip delimiter
    if (i < len && str[i] == delimiter) i++;
  }
  // Skip newline
  if (i < len && str[i] == '\r') i++;
  if (i < len && str[i] == '\n') i++;
  *pos = i;
  return quo_var_new_obj(arr);
}

// Escape a field for CSV output
static QuoVar quo__csv_escape_field(QuoVar *value) {
  const char *str;
  int len;
  if (quo_var_is_str(value)) {
    str = quo_var_as_str(value)->data;
    len = quo_var_as_str(value)->len;
  } else if (quo_var_is_nil(value)) return quo_var_new_obj(quo_str_new("", -1));
  else {
    // Convert to string
    QuoVar str_val = quo_var_to_str(value);
    str = quo_var_as_str(&str_val)->data;
    len = quo_var_as_str(&str_val)->len;
  }
  QuoStringBuilder sb = quo_sb_new();
  bool needs_quoting = false;
  for (int i = 0; i < len; i++) {
    char c = str[i];
    if (c == ',' || c == '\n' || c == '\r' || c == '"') needs_quoting = true;
  }
  if (needs_quoting) {
    da_add(&sb, '"');
    for (int i = 0; i < len; i++) {
      if (str[i] == '"') da_add(&sb, '"'); // Double quote
      da_add(&sb, str[i]);
    }
    da_add(&sb, '"');
  } else quo_sb_append(&sb, str, len);
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_csv_parse(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("csv.parse() requires a string argument");
  char delimiter = ',';
  if (argc >= 2 && quo_var_is_str(&argv[1]) && quo_var_as_str(&argv[1])->len > 0) delimiter = quo_var_as_str(&argv[1])->data[0];
  const char *str = quo_var_as_str(&argv[0])->data;
  int len = quo_var_as_str(&argv[0])->len;
  int pos = 0;
  QuoArr *result = quo_arr_new();
  while (pos < len) {
    // Skip empty lines
    if (str[pos] == '\n' || str[pos] == '\r') {
      pos++;
      continue;
    }
    QuoVar line = quo__csv_parse_line(str, &pos, len, delimiter);
    if (quo_var_is_err(&line)) {
      QuoVar obj = quo_var_new_obj(result);
      quo_var_unref(&obj);
      return line;
    }
    // Only add non-empty lines
    if (quo_arr_len(quo_obj_as_arr(line.val_obj)) > 0) quo_arr_push(result, line);
  }
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_csv_parse_dict(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("csv.parse_dict() requires a string argument");
  char delimiter = ',';
  if (argc >= 2 && quo_var_is_str(&argv[1]) && quo_var_as_str(&argv[1])->len > 0) delimiter = quo_var_as_str(&argv[1])->data[0];
  const char *str = quo_var_as_str(&argv[0])->data;
  int len = quo_var_as_str(&argv[0])->len;
  int pos = 0;
  // Parse header
  QuoVar header_line = quo__csv_parse_line(str, &pos, len, delimiter);
  if (quo_var_is_err(&header_line)) return header_line;
  if (!quo_var_is_arr(&header_line) || quo_arr_len(quo_var_as_arr(&header_line)) == 0) return quo_var_new_err("CSV must have a header row");
  QuoArr *result = quo_arr_new();
  while (pos < len) {
    if (str[pos] == '\n' || str[pos] == '\r') {
      pos++;
      continue;
    }
    QuoVar line = quo__csv_parse_line(str, &pos, len, delimiter);
    if (quo_var_is_err(&line)) {
      QuoVar obj = quo_var_new_obj(result);
      quo_var_unref(&obj);
      return line;
    }
    if (quo_arr_len(quo_var_as_arr(&line)) == 0) continue;
    QuoDict *record = quo_dict_new();
    int64_t field_count = quo_arr_len(quo_var_as_arr(&line));
    int64_t header_count = quo_arr_len(quo_var_as_arr(&header_line));
    for (int64_t i = 0; i < header_count && i < field_count; i++) {
      QuoVar key = quo_arr_get(quo_var_as_arr(&header_line), i);
      QuoVar val = quo_arr_get(quo_var_as_arr(&line), i);
      if (quo_var_is_str(&key)) quo_dict_set(record, quo_var_as_str(&key), &val);
    }
    quo_arr_push(result, quo_var_new_obj(record));
  }
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_csv_stringify(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_arr(&argv[0])) return quo_var_new_err("csv.stringify() requires an array argument");
  char delimiter = ',';
  if (argc >= 2 && quo_var_is_str(&argv[1]) && quo_var_as_str(&argv[1])->len > 0) delimiter = quo_var_as_str(&argv[1])->data[0];
  QuoArr *arr = quo_var_as_arr(&argv[0]);
  QuoStringBuilder sb = quo_sb_new();
  for (int i = 0; i < quo_arr_len(arr); i++) {
    QuoVar row = quo_arr_get(arr, i);
    if (quo_var_is_arr(&row)) {
      QuoArr *row_arr = quo_var_as_arr(&row);
      for (int j = 0; j < quo_arr_len(row_arr); j++) {
        if (j > 0) da_add(&sb, delimiter);
        QuoVar field = quo_arr_get(row_arr, j);
        QuoVar escaped = quo__csv_escape_field(&field);
        quo_sb_append(&sb, quo_var_as_str(&escaped)->data, quo_var_as_str(&escaped)->len);
      }
      da_add(&sb, '\n');
    }
  }
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_csv_stringify_dict(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc < 1 || !quo_var_is_arr(&argv[0])) return quo_var_new_err("csv.stringify_dict() requires an array of dicts argument");
  char delimiter = ',';
  if (argc >= 2 && quo_var_is_str(&argv[1]) && quo_var_as_str(&argv[1])->len > 0) delimiter = quo_var_as_str(&argv[1])->data[0];
  QuoArr *arr = quo_var_as_arr(&argv[0]);
  if (quo_arr_len(arr) == 0) return quo_var_new_obj(quo_str_new("", -1));
  QuoStringBuilder sb = quo_sb_new();
  // Get headers from first record
  QuoVar first = quo_arr_get(arr, 0);
  if (!quo_var_is_dict(&first)) return quo_var_new_err("Array elements must be dicts");
  da(QuoStr *) headers = {0};
  for (int i = 0; i < quo_var_as_dict(&first)->dict.capacity; i++) {
    QuoHashTableEntry *entry = &quo_var_as_dict(&first)->dict.items[i];
    if (entry->key) da_add(&headers, entry->key);
  }
  // Write headers
  for (int i = 0; i < da_count(&headers); i++) {
    if (i > 0) da_add(&sb, delimiter);
    QuoVar header = quo_var_new_obj(headers.items[i]);
    QuoVar escaped = quo__csv_escape_field(&header);
    quo_sb_append(&sb, quo_var_as_str(&escaped)->data, quo_var_as_str(&escaped)->len);
  }
  da_add(&sb, '\n');
  // Write data rows
  for (int i = 0; i < quo_arr_len(arr); i++) {
    QuoVar row = quo_arr_get(arr, i);
    if (!quo_var_is_dict(&row)) continue;
    for (int j = 0; j < da_count(&headers); j++) {
      if (j > 0) da_add(&sb, delimiter);
      QuoVar value = quo_dict_get(quo_var_as_dict(&row), headers.items[j]);
      QuoVar escaped = quo__csv_escape_field(&value);
      quo_sb_append(&sb, quo_var_as_str(&escaped)->data, quo_var_as_str(&escaped)->len);
    }
    da_add(&sb, '\n');
  }
  da_free(&headers);
  quo_sb_null_terminate(&sb);
  QuoStr *result = quo_str_new(quo_sb_string(&sb), da_count(&sb) - 1);
  quo_sb_free(&sb);
  return quo_var_new_obj(result);
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_csv_init(QuoModule *parent) {
  QuoModule *m = quo_module_new(parent, parent->cwd, "csv", NULL, NULL);
  quo_module_register_cfn(m, "parse", -1, quo__mod_csv_parse);
  quo_module_register_cfn(m, "parse_dict", -1, quo__mod_csv_parse_dict);
  quo_module_register_cfn(m, "stringify", -1, quo__mod_csv_stringify);
  quo_module_register_cfn(m, "stringify_dict", -1, quo__mod_csv_stringify_dict);
  return true;
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_CSV_H
