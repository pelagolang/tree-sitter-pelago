; The outline (symbol tree) in the form Zed reads: `@item` is the whole node, `@name` its label
; and `@context` the words shown before it.

(function_declaration
  "fn" @context
  name: (_) @name) @item

(constructor_declaration
  "init" @name) @item

(destructor_declaration
  "deinit" @name) @item

(struct_declaration
  "struct" @context
  name: (_) @name) @item

(obj_declaration
  "obj" @context
  name: (_) @name) @item

(interface_declaration
  "interface" @context
  name: (_) @name) @item

(enum_declaration
  "enum" @context
  name: (_) @name) @item

(enum_variant
  name: (_) @name) @item

(field_declaration
  name: (_) @name) @item

(impl_declaration
  "impl" @context
  trait: (_) @name
  ("for" @context
   type: (_) @name)?) @item

(type_alias
  "type" @context
  name: (_) @name) @item

(mod_declaration
  "mod" @context
  name: (_) @name) @item

(test_declaration
  "test" @context
  name: (_) @name) @item

(source_file
  (variable_declaration
    ["const" "var"] @context
    pattern: (identifier) @name) @item)
