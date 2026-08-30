#pragma once

#include "quo.h"

static inline QuoVar quo__mod_fs_exists(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.exists() requires a path string");
  FILE *f = fopen(quo_var_as_str(&argv[0])->data, "rb");
  if (f) {
    fclose(f);
    return quo_var_new_bool(true);
  }
  // Check if it's a directory
#ifdef _WIN32
  DWORD attrs = GetFileAttributes(quo_var_as_str(&argv[0])->data);
  return quo_var_new_bool(attrs != INVALID_FILE_ATTRIBUTES);
#else
  struct stat st;
  return quo_var_new_bool(stat(quo_var_as_str(&argv[0])->data, &st) == 0);
#endif
}

static inline QuoVar quo__mod_fs_stat(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.stat() requires a path string");
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  if (!GetFileAttributesEx(quo_var_as_str(&argv[0])->data, GetFileExInfoStandard, &attrs)) {
    QuoObj *result = quo_dict_new();
    QuoVar key = quo_var_new_obj(quo_str_new("exists", -1));
    QuoVar val = quo_var_new_bool(false);
    quo_dict_set(result, quo_var_as_obj(&key), &val);
    return quo_var_new_obj(result);
  }
  QuoObj *result = quo_dict_new();
  // exists: true
  quo_dict_set(result, quo_str_new("exists", -1), &quo_var_new_bool(true));
  // is_dir
  bool is_dir = (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  quo_dict_set(result, quo_str_new("is_dir", -1), &quo_var_new_bool(is_dir));
  // is_file
  quo_dict_set(result, quo_str_new("is_file", -1), &quo_var_new_bool(!is_dir));
  // size
  uint64_t size = ((uint64_t)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
  quo_dict_set(result, quo_str_new("size", -1), &quo_var_new_int((int64_t)size));
  // modified
  uint64_t timestamp = ((uint64_t)attrs.ftLastWriteTime.dwHighDateTime << 32) | attrs.ftLastWriteTime.dwLowDateTime;
  timestamp = (timestamp / 10000000) - 11644473600ULL;
  quo_dict_set(result, quo_str_new("modified", -1), &quo_var_new_int((int64_t)timestamp));
  return quo_var_new_obj(result);
#else
  struct stat st;
  if (stat(quo_var_as_str(&argv[0])->data, &st) != 0) {
    QuoDict *result = quo_dict_new();
    quo_dict_set(result, quo_str_new("exists", -1), &quo_var_new_bool(false));
    return quo_var_new_obj(result);
  }
  QuoDict *result = quo_dict_new();
  quo_dict_set(result, quo_str_new("exists", -1), &quo_var_new_bool(true));
  quo_dict_set(result, quo_str_new("is_dir", -1), &quo_var_new_bool(S_ISDIR(st.st_mode)));
  quo_dict_set(result, quo_str_new("is_file", -1), &quo_var_new_bool(S_ISREG(st.st_mode)));
  quo_dict_set(result, quo_str_new("size", -1), &quo_var_new_num(st.st_size));
  quo_dict_set(result, quo_str_new("modified", -1), &quo_var_new_num(st.st_mtime));
  return quo_var_new_obj(result);
#endif
}

static inline QuoVar quo__mod_fs_ls(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.ls() requires a path string");
  QuoArr *arr = quo_arr_new();
#ifdef _WIN32
  char *pattern = quo_strdupf("%s\\*", argv[0].val_obj->str.data);
  WIN32_FIND_DATA fd;
  HANDLE hFind = FindFirstFile(pattern, &fd);
  quo_dealloc(pattern);
  if (hFind == INVALID_HANDLE_VALUE) {
    quo_var_unref(&quo_var_new_obj(arr));
    return quo_var_new_err("Failed to open directory");
  }
  do {
    if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
      quo_arr_push(arr, quo_var_new_obj(quo_str_new_raw(m, fd.cFileName, -1)));
  } while (FindNextFile(hFind, &fd));
  FindClose(hFind);
#else
  DIR *dir = opendir(quo_var_as_str(&argv[0])->data);
  if (!dir) {
    QuoVar o = quo_var_new_obj(arr);
    quo_var_unref(&o);
    return quo_var_new_err("Failed to open directory");
  }
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL)
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
      quo_arr_push(arr, quo_var_new_obj(quo_str_new_raw(entry->d_name, -1)));
  closedir(dir);
#endif
  return quo_var_new_obj(arr);
}

static inline QuoVar quo__mod_fs_mkdir(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.mkdir_all() requires a path string");
  char *path = quo_strdup(quo_var_as_str(&argv[0])->data);
  char *p = path;
#ifdef _WIN32
  // Skip drive letter on Windows
  if (strlen(p) >= 2 && p[1] == ':') p += 2;
#endif
  while (*p) {
    if (*p == '/' || *p == '\\') {
      char old = *p;
      *p = '\0';
      mkdir(path, 0755);
      *p = old;
    }
    p++;
  }
  // Create final directory
  mkdir(path, 0755);
  quo_dealloc(path);
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_rm(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.rm() requires a path string");
  if (remove(quo_var_as_str(&argv[0])->data) != 0) return quo_var_new_err("Failed to remove file");
  return quo_var_new_nil();
}

static bool quo__mod_fs_remove_recursive(const char *path) {
#ifdef _WIN32
  // Windows implementation using Win32 API
  char path_buf[MAX_PATH];
  snprintf(path_buf, sizeof(path_buf), "%s\0", path);
  // Double null-terminate for SHFileOperation
  size_t len = strlen(path_buf);
  path_buf[len + 1] = '\0';
  SHFILEOPSTRUCT shfo = {NULL, FO_DELETE, path_buf, NULL, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, FALSE, NULL, NULL};
  return SHFileOperation(&shfo) == 0;
#else
  // Unix implementation
  DIR *dir = opendir(path);
  if (!dir) return false;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    char *full_path = quo_strdupf("%s/%s", path, entry->d_name);
    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        quo__mod_fs_remove_recursive(full_path);
      } else {
        remove(full_path);
      }
    }
    quo_dealloc(full_path);
  }
  closedir(dir);
  return rmdir(path) == 0;
#endif
}

static inline QuoVar quo__mod_fs_rmdir(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.rm_all() requires a path string");
  if (!quo__mod_fs_remove_recursive(quo_var_as_str(&argv[0])->data)) return quo_var_new_err("Failed to remove directory recursively");
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_rename(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("fs.rename() requires source and destination path strings");
  if (rename(quo_var_as_str(&argv[0])->data, quo_var_as_str(&argv[1])->data) != 0) return quo_var_new_err("Failed to rename file");
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_cp(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 2 || !quo_var_is_str(&argv[0]) || !quo_var_is_str(&argv[1]))
    return quo_var_new_err("fs.cp() requires source and destination path strings");
  char *src_data = quo_read_file(quo_var_as_str(&argv[0])->data);
  if (!src_data) return quo_var_new_err("Failed to read source file");
  bool res = quo_write_file(quo_var_as_str(&argv[1])->data, src_data);
  if (!res) {
    quo_dealloc(src_data);
    return quo_var_new_err("Failed to write destination file");
  }
  quo_dealloc(src_data);
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_cwd(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
  char buf[4096];
  if (!getcwd(buf, sizeof(buf))) return quo_var_new_err("Failed to get current directory");
  return quo_var_new_obj(quo_str_new(buf, -1));
}

static inline QuoVar quo__mod_fs_cd(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  if (argc != 1 || !quo_var_is_str(&argv[0])) return quo_var_new_err("fs.chdir() requires a path string");
  if (chdir(quo_var_as_str(&argv[0])->data) != 0) return quo_var_new_err("Failed to change directory");
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_get_tmp_dir(QuoModule *m, int argc, QuoVar *argv) {
  QUO_UNUSED(m);
  QUO_UNUSED(argc);
  QUO_UNUSED(argv);
#ifdef _WIN32
  char buf[MAX_PATH];
  GetTempPath(MAX_PATH, buf);
#else
  const char *tmp = getenv("TMPDIR");
  if (!tmp) tmp = "/tmp";
  const char *buf = tmp;
#endif
  return quo_var_new_obj(quo_str_new(buf, -1));
}

static inline void quo_mod_fs_load() {
  QuoModule *m = quo_module_new(NULL, "fs", NULL, NULL);
  quo_module_register_cfn(m, "exists", -1, quo__mod_fs_exists);
  quo_module_register_cfn(m, "stat", -1, quo__mod_fs_stat);
  quo_module_register_cfn(m, "ls", -1, quo__mod_fs_ls);
  quo_module_register_cfn(m, "mkdir", -1, quo__mod_fs_mkdir);
  quo_module_register_cfn(m, "rm", -1, quo__mod_fs_rm);
  quo_module_register_cfn(m, "rmdir", -1, quo__mod_fs_rmdir);
  quo_module_register_cfn(m, "rename", -1, quo__mod_fs_rename);
  quo_module_register_cfn(m, "cp", -1, quo__mod_fs_cp);
  quo_module_register_cfn(m, "cwd", -1, quo__mod_fs_cwd);
  quo_module_register_cfn(m, "cd", -1, quo__mod_fs_cd);
  quo_module_register_cfn(m, "get_tmp_dir", -1, quo__mod_fs_get_tmp_dir);
}
