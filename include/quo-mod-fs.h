/*

DESCRIPTION:
    Quo module for filesystem operations

C API:
    #include "quo-mod-fs.h"
    ...
    QuoState *s = quo_new_state();
    quo_state_register_module(s, quo_mod_fs_init, NULL);
    ...

QUO API:
    # Read file contents
    var content = fs.read("file.txt")

    # Write string to file
    fs.write("file.txt", "Hello, World!")

    # Append string to file
    fs.append("file.txt", "More content\n")

    # Check if path exists
    var exists = fs.exists("file.txt") # Returns true or false

    # Get file info
    var info = fs.stat("file.txt")
    # Returns dict: { "size": 1234, "is_dir": false, "is_file": true, "modified": 1234567890 }

    # List directory contents
    var files = fs.ls("./") # Returns array of filenames

    # Create directories recursively
    fs.mkdir("path/to/new/folder")

    # Remove file
    fs.rm("file.txt")

    # Remove directory recursively
    fs.rmdir("folder_with_contents")

    # Rename/move file or directory
    fs.rename("old_name.txt", "new_name.txt")

    # Copy file
    fs.cp("source.txt", "dest.txt")

    # Get current working directory
    var cwd = fs.cwd()

    # Change current working directory
    fs.cd("/path/to/dir")

    # Get temp directory
    var tmp = fs.tmp_dir()

    # Read entire file as array of lines
    var lines = fs.read_lines("file.txt")

    # Write array of lines to file
    fs.write_lines("file.txt", ["line 1", "line 2", "line 3"])

*/

#ifndef QUO_MOD_FS_H
#define QUO_MOD_FS_H

#include "quo.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir(path)       _rmdir(path)
#define getcwd(buf, size) _getcwd(buf, size)
#define chdir(path)       _chdir(path)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  QuoObj obj;
  FILE *file;
  QuoStr *path;
} QuoFSFile;

QUO_DEFINE_USER_TYPE(QuoFSFile, fs_file);

// ---------- PRIVATE API ---------- //

static inline QuoVar quo__mod_fs_exists(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.exists() requires a path string");
  FILE *f = fopen(quo_var_as_str(&argv[1])->data, "rb");
  if (f) {
    fclose(f);
    return quo_var_new_bool(true);
  }
  // Check if it's a directory
#ifdef _WIN32
  DWORD attrs = GetFileAttributes(quo_var_as_str(&argv[1])->data);
  return quo_var_new_bool(attrs != INVALID_FILE_ATTRIBUTES);
#else
  struct stat st;
  return quo_var_new_bool(stat(quo_var_as_str(&argv[1])->data, &st) == 0);
#endif
}

static inline QuoVar quo__mod_fs_stat(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.stat() requires a path string");
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  if (!GetFileAttributesEx(quo_var_as_str(&argv[1])->data, GetFileExInfoStandard, &attrs)) {
    QuoObj *result = quo_dict_new();
    QuoVar key = quo_var_new_obj(quo_str_new(s, "exists", -1));
    QuoVar val = quo_var_new_bool(false);
    quo_dict_set(result, quo_var_as_obj(&key), &val);
    return quo_var_new_obj(result);
  }
  QuoObj *result = quo_dict_new();
  // exists: true
  quo_dict_set(result, quo_str_new(s, "exists", -1), &quo_var_new_bool(true));
  // is_dir
  bool is_dir = (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  quo_dict_set(result, quo_str_new(s, "is_dir", -1), &quo_var_new_bool(is_dir));
  // is_file
  quo_dict_set(result, quo_str_new(s, "is_file", -1), &quo_var_new_bool(!is_dir));
  // size
  uint64_t size = ((uint64_t)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
  quo_dict_set(result, quo_str_new(s, "size", -1), &quo_var_new_int((int64_t)size));
  // modified
  uint64_t timestamp = ((uint64_t)attrs.ftLastWriteTime.dwHighDateTime << 32) | attrs.ftLastWriteTime.dwLowDateTime;
  timestamp = (timestamp / 10000000) - 11644473600ULL;
  quo_dict_set(result, quo_str_new(s, "modified", -1), &quo_var_new_int((int64_t)timestamp));
  return quo_var_new_obj(result);
#else
  struct stat st;
  if (stat(quo_var_as_str(&argv[1])->data, &st) != 0) {
    QuoDict *result = quo_dict_new();
    quo_dict_set(result, quo_str_new(s, "exists", -1), &quo_var_new_bool(false));
    return quo_var_new_obj(result);
  }
  QuoDict *result = quo_dict_new();
  quo_dict_set(result, quo_str_new(s, "exists", -1), &quo_var_new_bool(true));
  quo_dict_set(result, quo_str_new(s, "is_dir", -1), &quo_var_new_bool(S_ISDIR(st.st_mode)));
  quo_dict_set(result, quo_str_new(s, "is_file", -1), &quo_var_new_bool(S_ISREG(st.st_mode)));
  quo_dict_set(result, quo_str_new(s, "size", -1), &quo_var_new_num(st.st_size));
  quo_dict_set(result, quo_str_new(s, "modified", -1), &quo_var_new_num(st.st_mtime));
  return quo_var_new_obj(result);
#endif
}

static inline QuoVar quo__mod_fs_ls(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.ls() requires a path string");
  QuoArr *arr = quo_arr_new();
#ifdef _WIN32
  char *pattern = quo_strdupf("%s\\*", argv[1].val_obj->str.data);
  WIN32_FIND_DATA fd;
  HANDLE hFind = FindFirstFile(pattern, &fd);
  quo_dealloc(pattern);
  if (hFind == INVALID_HANDLE_VALUE) {
    quo_var_unref(&quo_var_new_obj(arr));
    return quo_var_new_err("Failed to open directory");
  }
  do {
    if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
      quo_arr_push(arr, quo_var_new_obj(quo_str_new_raw(s, fd.cFileName, -1)));
  } while (FindNextFile(hFind, &fd));
  FindClose(hFind);
#else
  DIR *dir = opendir(quo_var_as_str(&argv[1])->data);
  if (!dir) {
    QuoVar o = quo_var_new_obj(arr);
    quo_var_unref(&o);
    return quo_var_new_err("Failed to open directory");
  }
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL)
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
      quo_arr_push(arr, quo_var_new_obj(quo_str_new_raw(s, entry->d_name, -1)));
  closedir(dir);
#endif
  return quo_var_new_obj(arr);
}

static inline QuoVar quo__mod_fs_mkdir(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.mkdir_all() requires a path string");
  char *path = quo_strdup(quo_var_as_str(&argv[1])->data);
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

static inline QuoVar quo__mod_fs_rm(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.rm() requires a path string");
  if (remove(quo_var_as_str(&argv[1])->data) != 0) return quo_var_new_err("Failed to remove file");
  return quo_var_new_nil();
}

static bool quo__fs_remove_recursive(const char *path) {
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
        quo__fs_remove_recursive(full_path);
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

static inline QuoVar quo__mod_fs_rmdir(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.rm_all() requires a path string");
  if (!quo__fs_remove_recursive(quo_var_as_str(&argv[1])->data)) return quo_var_new_err("Failed to remove directory recursively");
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_rename(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_str(&argv[1]) || !quo_var_is_str(&argv[2]))
    return quo_var_new_err("fs.rename() requires source and destination path strings");
  if (rename(quo_var_as_str(&argv[1])->data, quo_var_as_str(&argv[2])->data) != 0) return quo_var_new_err("Failed to rename file");
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_cp(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_str(&argv[1]) || !quo_var_is_str(&argv[2]))
    return quo_var_new_err("fs.cp() requires source and destination path strings");
  char *src_data = quo_read_file(quo_var_as_str(&argv[1])->data);
  if (!src_data) return quo_var_new_err("Failed to read source file");
  bool res = quo_write_file(quo_var_as_str(&argv[2])->data, src_data);
  if (!res) {
    quo_dealloc(src_data);
    return quo_var_new_err("Failed to write destination file");
  }
  quo_dealloc(src_data);
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_cwd(QuoState *s, int64_t argc, QuoVar *argv) {
  char buf[4096];
  if (!getcwd(buf, sizeof(buf))) return quo_var_new_err("Failed to get current directory");
  return quo_var_new_obj(quo_str_new(s, buf, -1));
}

static inline QuoVar quo__mod_fs_cd(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("fs.chdir() requires a path string");
  if (chdir(quo_var_as_str(&argv[1])->data) != 0) return quo_var_new_err("Failed to change directory");
  return quo_var_new_nil();
}

static inline QuoVar quo__mod_fs_get_tmp_dir(QuoState *s, int64_t argc, QuoVar *argv) {
#ifdef _WIN32
  char buf[MAX_PATH];
  GetTempPath(MAX_PATH, buf);
#else
  const char *tmp = getenv("TMPDIR");
  if (!tmp) tmp = "/tmp";
  const char *buf = tmp;
#endif
  return quo_var_new_obj(quo_str_new(s, buf, -1));
}

// --- QuoFSFile --- //

static inline QuoVar quo__mod_fs_open(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 3 || !quo_var_is_str(&argv[1]) || !quo_var_is_str(&argv[2]))
    return quo_var_new_err("fs.open() requires a path string and mode string");
  QuoFSFile *file = (QuoFSFile *)quo_state_get_type_instance(s, "QuoFSFile", -1);
  file->path = quo_var_as_str(&argv[1]);
  file->file = fopen(file->path->data, quo_var_as_str(&argv[2])->data);
  if (!file->file) {
    quo_obj_unref((QuoObj *)file);
    return quo_var_new_err("Failed to open file");
  }
  return quo_var_new_obj(file);
}

static inline QuoVar quo__mod_fs_get_path(QuoState *s, int64_t argc, QuoVar *argv) {
  return quo_var_new_obj(quo_var_as_fs_file(&argv[0])->path);
}

static inline QuoVar quo__mod_fs_read(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("read() has no arguments");
  QuoFSFile *file = quo_var_as_fs_file(&argv[0]);
  char *content = quo_read_file(file->path->data);
  if (!content) return quo_var_new_err("Failed to read file");
  QuoStr *result = quo_str_new_raw(s, content, -1);
  quo_dealloc(content);
  return quo_var_new_obj(result);
}

static inline QuoVar quo__mod_fs_write(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 2 || !quo_var_is_str(&argv[1])) return quo_var_new_err("write() requires a content string");
  QuoFSFile *file = quo_var_as_fs_file(&argv[0]);
  return quo_var_new_bool(quo_write_file(file->path->data, quo_var_as_str(&argv[1])->data));
}

static inline QuoVar quo__mod_fs_read_lines(QuoState *s, int64_t argc, QuoVar *argv) {
  if (argc != 1) return quo_var_new_err("read_lines() has no arguments");
  QuoFSFile *file = quo_var_as_fs_file(&argv[0]);
  char *content = quo_read_file(file->path->data);
  if (!content) return quo_var_new_err("Failed to read file");
  QuoArr *arr = quo_arr_new();
  char *line = strtok(content, "\n");
  while (line) {
    // Remove trailing \r if present
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
    quo_arr_push(arr, quo_var_new_obj(quo_str_new_raw(s, line, -1)));
    line = strtok(NULL, "\n");
  }
  quo_dealloc(content);
  return quo_var_new_obj(arr);
}

// ---------- PUBLIC API ---------- //

static inline bool quo_mod_fs_init(QuoState *s) {
  QuoObj *fs_file_type = quo_state_register_type(s, "QuoFSFile", -1, sizeof(QuoFSFile));
  quo_state_type_add_cfn(s, fs_file_type, "get_path", -1, quo__mod_fs_get_path);
  quo_state_type_add_cfn(s, fs_file_type, "read", -1, quo__mod_fs_read);
  quo_state_type_add_cfn(s, fs_file_type, "write", -1, quo__mod_fs_write);
  quo_state_type_add_cfn(s, fs_file_type, "read_lines", -1, quo__mod_fs_read_lines);

  QuoDict *ns = quo_state_register_namespace(s, "fs");
  quo_state_namespace_add_cfn(s, ns, "open", quo__mod_fs_open);
  quo_state_namespace_add_cfn(s, ns, "exists", quo__mod_fs_exists);
  quo_state_namespace_add_cfn(s, ns, "stat", quo__mod_fs_stat);
  quo_state_namespace_add_cfn(s, ns, "ls", quo__mod_fs_ls);
  quo_state_namespace_add_cfn(s, ns, "mkdir", quo__mod_fs_mkdir);
  quo_state_namespace_add_cfn(s, ns, "rm", quo__mod_fs_rm);
  quo_state_namespace_add_cfn(s, ns, "rmdir", quo__mod_fs_rmdir);
  quo_state_namespace_add_cfn(s, ns, "rename", quo__mod_fs_rename);
  quo_state_namespace_add_cfn(s, ns, "cp", quo__mod_fs_cp);
  quo_state_namespace_add_cfn(s, ns, "cwd", quo__mod_fs_cwd);
  quo_state_namespace_add_cfn(s, ns, "cd", quo__mod_fs_cd);
  quo_state_namespace_add_cfn(s, ns, "get_tmp_dir", quo__mod_fs_get_tmp_dir);
  return true;
}

#ifdef __cplusplus
}
#endif

#endif // QUO_MOD_FS_H
