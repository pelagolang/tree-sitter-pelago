; Highlights for Pelago. Capture names follow the common set that Zed and Neovim understand.
; Later patterns win over earlier ones for the same node, so the general rules come first.

; ---- Names ------------------------------------------------------------------------------------

(identifier) @variable

(type_identifier) @type

((type_identifier) @type.builtin
 (#match? @type.builtin "^(int|int8|int16|int32|int64|uint|uint8|uint16|uint32|uint64|byte|float|float32|float64|bool|char|string|str|map|set|range|option|result|box|soa|rc|ptr|void|never|error|list|iter|task|chan|sender|receiver|shared|lock)$"))

; A name written in capitals is a constant.
((identifier) @constant
 (#match? @constant "^[A-Z][A-Z0-9_]*$"))

; `_` is the wildcard.
((identifier) @variable.builtin
 (#eq? @variable.builtin "_"))

(self) @variable.builtin
(super) @variable.builtin
(self_type) @type.builtin

(label) @label

(module_path (identifier) @module)
(import_path (module_path (identifier) @module))
(import_declaration alias: (identifier) @module)
(import_name name: (identifier) @variable alias: (identifier) @variable)

; ---- Functions ----------------------------------------------------------------------------------

(function_declaration name: (identifier) @function)

(field_declaration_list (function_declaration name: (identifier) @function.method))
(declaration_list (function_declaration name: (identifier) @function.method))
(enum_body (function_declaration name: (identifier) @function.method))

(call_expression function: (identifier) @function.call)
(call_expression function: (field_expression field: (identifier) @function.method))
(call_expression function: (type_application value: (identifier) @function.call))
(call_expression function: (type_application value: (field_expression field: (identifier) @function.method)))

((call_expression function: (identifier) @function.builtin)
 (#match? @function.builtin "^(print|assert|panic|todo|unreachable)$"))

; Variants written like calls: `ok(x)`, `err(e)`, `some(v)`.
((call_expression function: (identifier) @constructor)
 (#match? @constructor "^(ok|err|some)$"))

(call_mark) @operator

(constructor_declaration "init" @function.method)
(destructor_declaration "deinit" @function.method)

; ---- Parameters, fields and variants ------------------------------------------------------------

(parameter name: (identifier) @variable.parameter)
(parameter name: (self) @variable.builtin)
(closure_parameter pattern: (identifier) @variable.parameter)
(capture (identifier) @variable.parameter)
(argument label: (identifier) @variable.parameter)
(variant_field name: (identifier) @variable.parameter)
(pattern_argument label: (identifier) @variable.parameter)

(field_declaration name: (identifier) @property)
(field_initializer name: (identifier) @property)
(field_pattern name: (identifier) @property)
(field_expression field: (identifier) @property)
(field_expression field: (tuple_index) @number)

(enum_variant name: (identifier) @constant)
(dot_name name: (identifier) @constant)
(variant_pattern name: (identifier) @constant)

; ---- Literals -------------------------------------------------------------------------------------

(integer_literal) @number
(float_literal) @number
(boolean_literal) @boolean
(none) @constant.builtin

(character_literal) @character
(string_literal) @string
(string_content) @string
(escape_sequence) @string.escape
(interpolation ["#(" "#"] @punctuation.special)
(interpolation ")" @punctuation.special)
(interpolation (identifier) @variable)

; ---- Comments ---------------------------------------------------------------------------------------

(line_comment) @comment
(block_comment) @comment
(doc_comment) @comment.doc
(doc_block_comment) @comment.doc

; ---- Keywords ---------------------------------------------------------------------------------------

[
  "if" "else" "unless" "switch" "case" "default" "guard" "for" "in" "of" "while" "do" "loop"
  "break" "continue" "return" "defer" "become" "yield" "is"
] @keyword.control

[ "to" "step" "where" ] @keyword

"fn" @keyword.function

[
  "struct" "enum" "interface" "impl" "obj" "mod" "import" "use" "type" "test"
  "pub" "priv" "export" "pkg" "meta" "async" "unsafe" "await" "scope" "select"
] @keyword

[ "const" "var" ] @keyword

[ "inout" "sink" "let" "copy" ] @keyword.modifier
(mode) @keyword.modifier
(modifier) @keyword.modifier
(ext_modifier) @keyword.modifier

[ "map" "set" "soa" ] @type.builtin
"any" @keyword

; ---- Attributes ---------------------------------------------------------------------------------------

(attribute "@" @attribute)
(attribute name: (identifier) @attribute)

; ---- Operators and punctuation ---------------------------------------------------------------------------

[
  "+" "-" "*" "/" "%" "**" "==" "!=" "<" ">" "<=" ">=" "&&" "||" "!" "!!"
  "+%" "-%" "*%" "&" "|" "^" "~" "<<" ">>"
  "=" "+=" "-=" "*=" "/=" "%=" "&=" "|=" "^=" "<<=" ">>=" "+%=" "-%=" "*%=" "??="
  "++" "--" ".." "..=" "..." "?" "??" "?." "->" "~>" "|>"
] @operator

[ "," ";" ":" "." ] @punctuation.delimiter

[ "(" ")" "[" "]" "{" "}" ] @punctuation.bracket
(type_arguments ["<" ">"] @punctuation.bracket)
(generic_parameters ["<" ">"] @punctuation.bracket)

(import_path ["." ".." "..."] @punctuation.delimiter)
