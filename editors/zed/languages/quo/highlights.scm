; Comments
(line_comment) @comment

; Literals
(number) @number
(string) @string
(true) @constant.builtin
(false) @constant.builtin
(nil) @constant.builtin

; Variable declarations
(identifier) @variable

; Variable
(var_statement
    name: (identifier) @variable)

; Function calls - regular function calls
(call_expression
  function: (identifier) @function)

; Method calls - obj.method()
(call_expression
  function: (member_access_expression
    member: (identifier) @function))

; Member access
(member_access_expression
  object: (identifier) @variable
  member: (identifier) @property)

; Operators
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "=="
  "!="
  ">"
  "<"
  ">="
  "<="
  "="
  "+="
  "-="
  "*="
  "/="
] @operator

; Punctuation
[
    "["
    "]"
    "("
    ")"
    "{"
    "}"
] @punctuation.bracket

[
    "."
    ","
    ":"
] @punctuation.delimiters

; Keywords
[
    "var"
    "fn"
    "loop"
    "break"
    "continue"
    "return"
    "if"
    "else"
    "or"
    "and"
] @keyword
