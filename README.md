<div align="center">
    <img src="assets/logo.png" alt="Quo Logo">
    <h1>Quo</h1>
</div>

**Quo** is a small, interpreted, embeddable, dynamically typed programming language with a tiny **header-only** C implementation.

> [!WARNING]
> Quo is Work-in-Progress. API is subject to change.

# Building

Because Quo is tiny, it builds very fast. About `0.5` seconds for standard build and `3` seconds for release build if included all standard library modules.

To build `quo` CLI run:

```sh
# Standard build
./build.sh -b
# O3 optimization release build
./build.sh -b release
# Debug build with logs enabled
./build.sh -b debug
```

# Documentation

- [Getting Started](#getting-started)
  - [File Format](#file-format)
  - [Command-line Tool](#command-line-tool)
  - [Hello, World](#hello-world)
- [Language Reference](#language-reference)
  - [Comments](#comments)
  - [Keywords](#keywords)
  - [Basic Types](#basic-types)
  - [Variables](#variables)
  - [Functions](#functions)
  - [Scoping](#scoping)
  - [Control Flow](#control-flow)
  - [Loops](#loops)
  - [Operators](#operators)
- [Standard Library](#standard-library)
  - [Built-in Functions](#built-in-functions)
  - [Built-in Types Methods](#types-methods)
    - [Basic Types Methods](#basic-types-methods)
  - [Built-in Modules](#built-in-modules)
- [Embedding](#embedding)

## Getting Started

### File Format

Quo files use the `.quo` extension.

### Command-line Tool

To run `.quo` scripts, use the `quo` command-line tool.

Run script:

```bash
$ quo script.quo
```

### Hello, World

Here's a simple `"Hello, World!"` program in Quo:

```go
print("Hello, World!")
```

## Language Reference

### Comments

```python
# This is a line comment
```

### Keywords

| Keyword    | Description               |
| ---------- | ------------------------- |
| `var`      | Variable declaration      |
| `fn`       | Function declaration      |
| `if`       | Conditional statement     |
| `else`     | Conditional statement     |
| `loop`     | Loop statement            |
| `break`    | Loop control statement    |
| `continue` | Loop control statement    |
| `return`   | Function return statement |
| `nil`      | Nil literal               |
| `true`     | Boolean literal           |
| `false`    | Boolean literal           |

### Basic Types

| Type   | Description                      | Literal                       |
| ------ | -------------------------------- | ----------------------------- |
| `nil`  | No value                         | `nil`                         |
| `bool` | Boolean                          | `true`, `false`               |
| `num`  | Integer or floating-point number | `42`, `3.14`, `10_000_000`    |
| `str`  | String                           | `"hello"`                     |
| `arr`  | Dynamic array                    | `[1, 2.3, "4"]`               |
| `dict` | Key-value dictionary             | `{"key": 69, "foo": "hello"}` |

### Variables

Variables are declared using the `var` keyword.

```go
var foo = 69
```

Because Quo is dynamically typed, the type of a variable can change.

```go
var foo = 69
foo = "hello"
```

### Functions

Function definitions are expressions that are assigned to variables using the `fn` keyword.

```go
var add = fn(a, b) {
	return a + b
}
```

### Scoping

Variables are scoped to the block they are declared in, surrounded by `{}`.

```go
var foo = 69 # Global variable
{
	var bar = 42  # Local variable
	var foo = 420 # Shadowing of global variable `foo`
}
```

### Control Flow

Quo has `if`, `else`, `else if` keywords for control flow.

```go
var foo = 69

if foo < 42 {
	println("foo is less than 42")
} else if foo > 50 {
	println("foo is greater than 50")
} else {
	println("foo is between 42 and 50")
}
```

### Loop

Quo has only one loop construct `loop`.

It works like classic **C** `for` loop.

```go
loop (var i = 0, i < 10, i += 1) {
	println(i)
}
```

Infinite loop example:

```go
loop (,true,) {
	println("infinite loop")
}
```

Loop also has `break` and `continue` keywords for early termination and skipping iterations.

```go
loop (var i = 0, i < 10, i += 1) {
	if i == 5 continue
	if i == 7 break
	println(i) # 0 1 2 3 4 6 8 9
}
```

### Operators

Quo supports the following operators:

- Arithmetic: `+`, `-`, `*`, `/`, `%`
  ```go
  var a = 69 + 42 - 10 * 2 / 3 % 2
  var s = "Hello" + "World" # "HelloWorld"
  var t = "foo" * 3         # "foofoofoo"
  ```
- Grouping: `()`
  ```go
  var a = (69 + 42) - 10 * 2 / 3 % 2
  ```
- Assignment: `+=`, `-=`, `*=`, `/=`, `%=`
  ```go
  var a = 69
  a += 42 # a = a + 42
  a -= 10 # a = a - 10
  a *= 2  # a = a * 2
  a /= 3  # a = a / 3
  a %= 2  # a = a % 2
  ```
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
  ```go
  var a = 69
  var b = 42
  if a == b {
  	println("a is equal to b")
  } else if a > b {
  	println("a is greater than b")
  } else {
  	println("a is less than b")
  }
  ```
- Logical and: `and`
  ```go
  var a = true
  var b = false
  if a and b {
  	println("a and b are both true")
  } else {
  	println("a and b are not both true")
  }
  ```
- Logical or: `or`
  ```go
  var a = true
  var b = false
  if a or b {
  	println("a or b are true")
  } else {
  	println("a and b are not both true")
  }
  ```
- Logical not: `!`
  ```go
  var a = true
  if !a {
  	println("a is false")
  } else {
  	println("a is true")
  }
  ```

## Standard Library

### Built-in Functions

- `print(value, ...)`: Prints the values to the console, separated by spaces, adding new line at the end.
- `type(value)`: Get the type of the variable as string.
- `input(value, ...)`: Get the user input as string, printing optional prompt.

### Built-in Types Methods

Methods are functions that are associated with a type and can be called on a value of that type.

They are accessed using the dot notation: `value.method()`.

#### Basic types methods

- `str`:
  - `len()`: Returns the length of the UTF-8 string.
  - `get(n)`: Get the n-th character of the string.
  - `contains(s)`: Returns `true` if the string contains the substring `s`.
  - `strip()`: Returns a new string with leading and trailing whitespace removed.
  - `replace(old, new)`: Returns a new string with all occurrences of `old` replaced by `new`.
  - `split(sep)`: Returns an array of strings split by the separator `sep`.
  - `startswith(s)`: Returns `true` if the string starts with `s`.
  - `endswith(s)`: Returns `true` if the string ends with `s`.
- `dict`:
  - `len()`: Returns the number of key-value pairs in the dictionary.
  - `get(key)`: Returns the value associated with `key` in the dictionary.
  - `set(key, value)`: Sets the value associated with `key` in the dictionary.
- `arr`:
  - `len()`: Returns the length of the array.
  - `get(n)`: Get the n-th element of the array.
  - `set(n, value)`: Sets the n-th element of the array to `value`.
  - `push(value)`: Appends `value` to the end of the array.
  - `pop()`: Removes and returns the last element of the array.

### Built-in Modules

`quo` CLI has multiple built-in modules.

They're source is in the `include` directory with `quo-mod-*.h` names.
They are can be selectively disabled when [Embedding](#embedding) in your own code.

Modules are implemented as **global dictionary variables** (e. g `namespaces`) and available in any `.quo` script.

All functions are implemented in **C** so they are fast and efficient.

**Example usage:**

```
var encoded_string = base64.encode("Hello, World!")
print(encoded_string) # SGVsbG8sIFdvcmxkIQ==
```

**Modules and methods:**

- `base64`: Encodes and decodes strings using base64.
  - `encode(s)`: Returns the base64 encoding of `s`.
  - `decode(s)`: Returns the decoded string of `s`.

- `csv`: Parses and generates CSV files.
  - `parse(s)`: Returns an array of rows parsed from string `s`.
  - `parse_dict(s)`: Returns a dictionary with headers parsed from string `s`.
  - `stringify(rows)`: Stringify array of arrays to CSV
  - `stringify_dict(dict)`: Stringify array of dictionaries to CSV

- `env`: Access environment variables.
  - `get(key)`: Returns the value of the environment variable `key`.
  - `set(key, value)`: Sets the value of the environment variable `key` to `value`.
  - `unset(key)`: Removes the environment variable `key`.
  - `all()`: Returns a dictionary of all environment variables.
  - `has(key)`: Returns `true` if the environment variable `key` exists, `false` otherwise.

- `dl`: Dynamic loading of C libraries.
  - `open(libname)`: Loads the C library `libname` and returns a `QuoDLHandle`.
  - `QuoDLHandle`: A handle to a loaded C library.
    - `sym(name)`: Returns the `QuoDLSym` symbol from the library.
    - `call(sym, args)`: Calls the `QuoDLSym` symbol with `args` and returns the result.
    - `close()`: Closes the handle and unloads the library.

- `json`: Encodes and decodes JSON strings.
  - `decode(s)`: Decodes JSON string and returns `dict`.
  - `encode(obj)`: Returns the JSON string from `dict`.

- `time`: Time-related functions.
  - `now()`: Returns the current time as a `num`.
  - `clock()`: Returns the current clock time as a `num`.
  - `sleep(seconds)`: Sleeps for `seconds` seconds.

- `os`: Operating system functions.
  - `system(command)`: Executes the `command` in the operating system shell.
  - `name()`: Returns the name of the operating system.

- `net`: Network functions.
  - `get(url)`: Sends a GET request to `url` and returns the response.
  - `post(url, data)`: Sends a POST request to `url` with `data` and returns the response.
  - `put(url, data)`: Sends a PUT request to `url` with `data` and returns the response.
  - `patch(url, data)`: Sends a PATCH request to `url` with `data` and returns the response.
  - `delete(url)`: Sends a DELETE request to `url` and returns the response.
  - `request(url, method, data, headers)`: Sends a custom request to `url` with `method`, `data`, and `headers` dict and returns the response.
  - `encode(s)`: URL encodes the string `s` and returns the result.
  - `decode(s)`: URL decodes the string `s` and returns the result.

- `fs`: File system functions.
  - `open(path)`: Opens a file at `path` and returns a `QuoFSFile` object.
    - `QuoFSFile`: A file object that can be used to read and write to a file.
      - `read()`: Reads the contents of the file and returns it as a string.
      - `read_lines()`: Reads the contents of the file and returns it as a array of strings.
      - `write(data)`: Writes `data` string to the file.
  - `exists(path)`: Returns `true` if the file at `path` exists, `false` otherwise.
  - `stat(path)`: Returns the stat information of the file at `path`.
  - `ls(path)`: Returns a list of files in the directory at `path`.
  - `mkdir(path)`: Creates a directory at `path`.
  - `rm(path)`: Removes the file at `path`.
  - `rmdir(path)`: Removes the directory at `path`.
  - `rename(old_path, new_path)`: Renames the file at `old_path` to `new_path`.
  - `cp(src_path, dst_path)`: Copies the file at `src_path` to `dst_path`.
  - `cwd()`: Returns the current working directory.
  - `cd(path)`: Changes the current working directory to `path`.
  - `get_tmp_dir()`: Returns the path of the temporary directory.

- `math`: Math library
  - Constants:
    - `pi`: Pi (π)
    - `e`: Euler's number (e)
    - `tau`: Tau (2π)
  - Functions:
    - `floor(num)`: Returns the largest integer less than or equal to `num`.
    - `ceil(num)`: Returns the smallest integer greater than or equal to `num`.
    - `round(num)`: Returns the nearest integer to `num`.
    - `trunc(num)`: Returns the integer part of `num`.
    - `abs(num)`: Returns the absolute value of `num`.
    - `sqrt(num)`: Returns the square root of `num`.
    - `cbrt(num)`: Returns the cube root of `num`.
    - `pow(base, exp)`: Returns `base` raised to the power of `exp`.
    - `exp(num)`: Returns e raised to the power of `num`.
    - `log(num)`: Returns the natural logarithm of `num`.
    - `log2(num)`: Returns the base-2 logarithm of `num`.
    - `log10(num)`: Returns the base-10 logarithm of `num`.
    - `sin(num)`: Returns the sine of `num`.
    - `cos(num)`: Returns the cosine of `num`.
    - `tan(num)`: Returns the tangent of `num`.
    - `asin(num)`: Returns the arcsine of `num`.
    - `acos(num)`: Returns the arccosine of `num`.
    - `atan(num)`: Returns the arctangent of `num`.
    - `atan2(y, x)`: Returns the arctangent of `y`/`x`.
    - `sinh(num)`: Returns the hyperbolic sine of `num`.
    - `cosh(num)`: Returns the hyperbolic cosine of `num`.
    - `tanh(num)`: Returns the hyperbolic tangent of `num`.
    - `min(a, b)`: Returns the smaller of `a` and `b`.
    - `max(a, b)`: Returns the larger of `a` and `b`.
    - `clamp(num, min, max)`: Returns `num` clamped to the range `min` to `max`.
    - `random(max)`: Returns a random number between 0 and `max`.
    - `random_float(max)`: Returns a random floating-point number between 0 and `max`.
    - `deg_to_rad(num)`: Converts `num` from degrees to radians.
    - `rad_to_deg(num)`: Converts `num` from radians to degrees.

- `uuid`: UUID functions.
  - `v4()`: Generates a random UUID v4.
  - `v7()`: Generates a UUID v7 (time-ordered).
  - `parse(str)`: Parses a UUID string and returns a dictionary with `valid`, `version`, and `variant` fields.
  - `is_valid(str)`: Checks if a string is a valid UUID.

## Embedding

Quo is written in header-only C, so embedding it in your own code is easy.
Just copy the headers from the `include` directory to your project.

Quo can be used as scripting language for your game, configuration language for your program etc.

The main file is `quo.h`, it contains:

- Lexer/Parser
- Bytecode Compiler
- Virtual Machine
- All of the embedding functions

Quo modules are `quo-mod-*.h` files. Modules can be excluded if not needed.

It is very simple to embed Quo in your own code.

```c
#define QUO_IMPLEMENTATION // Define this before including quo.h in ONE of your source files
#include "../include/quo.h"

// Include modules that you need to be available in your quo code.
#include "../include/quo-mod-base64.h"
#include "../include/quo-mod-csv.h"
#include "../include/quo-mod-dl.h"
#include "../include/quo-mod-env.h"
#include "../include/quo-mod-fs.h"
#include "../include/quo-mod-json.h"
#include "../include/quo-mod-math.h"
#include "../include/quo-mod-net.h"
#include "../include/quo-mod-os.h"
#include "../include/quo-mod-time.h"
#include "../include/quo-mod-uuid.h"

 int main() {
  int64_t exit_code = 0;
  const char *path = "path/to/script.quo";
  // Get the directory name of the .quo script. It will be used as the current working directory.
  char *cwd = quo_dirname(path);
  // Create a Quo state with the script current working directory.
  QuoState *s = quo_state_new(cwd);

  // Load modules
  quo_state_register_module(s, quo_mod_base64_init, NULL);
  quo_state_register_module(s, quo_mod_csv_init, NULL);
  quo_state_register_module(s, quo_mod_dl_init, NULL);
  quo_state_register_module(s, quo_mod_env_init, NULL);
  quo_state_register_module(s, quo_mod_fs_init, NULL);
  quo_state_register_module(s, quo_mod_json_init, NULL);
  quo_state_register_module(s, quo_mod_math_init, NULL);
  quo_state_register_module(s, quo_mod_os_init, NULL);
  quo_state_register_module(s, quo_mod_time_init, NULL);
  quo_state_register_module(s, quo_mod_uuid_init, NULL);
  // Some modules require cleanup, so we register them with cleanup functions that will be called when the state is freed.
  quo_state_register_module(s, quo_mod_net_init, quo_mod_net_cleanup);

  // Create a parser for the script.
  QuoParser *p = quo_parser_new(s, path);
  // Parse the script and compile it.
  if (quo_parser_parse(p)) {
    // Create a compiler for the parsed AST script.
    QuoCompiler *c = quo_compiler_new(s, "main", -1);
    // Compile the AST into a main function.
    QuoFn *main = quo_compiler_compile(c, p->ast);
    // Create a VM to run the compiled function.
    QuoVM *vm = quo_vm_new(s);
    // Run the main function and get the result.
    QuoVar result = quo_vm_run(vm, main);
    // Check the result and handle any errors.
    if (quo_var_is_err(&result)) fprintf(stderr, "Runtime error: %s\n", result.val_err);
    // Here we're returning number from the main function so we can use it as the exit code.
    // You can return any value type from the main function.
    else if (quo_var_is_num(&result)) exit_code = (int64_t)result.val_num;
    // Unreference the result and free the compiler and VM.
    quo_var_unref(&result);
    quo_compiler_free(c);
    quo_vm_free(vm);
  }
  // Free the parser and state.
  quo_parser_free(p);
  quo_state_free(s);
  // Free the current working directory.
  quo_dealloc(cwd);
  // Return the exit code.
  return exit_code;
}
```

To see the example of embedding Quo in your own code, see the [main.c](cli/main.c) file.
