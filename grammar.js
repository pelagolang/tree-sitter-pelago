/**
 * Tree-sitter grammar for Pelago (`.pel`).
 *
 * The compiler's own parser (`compiler/src/parser/` in the pelago repository) is the oracle for what
 * is valid. Everything that depends on spacing or on newlines is done by the external scanner in
 * `src/scanner.c`: see README.md for the list.
 *
 * @file Pelago grammar for tree-sitter
 * @license see the repository
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const PREC = {
  closure: 0,
  assign: 1,
  ternary: 2,
  pipe: 3,
  coalesce: 4,
  or: 5,
  and: 6,
  compare: 7,
  range: 8,
  convert: 9,
  bit_or: 10,
  bit_xor: 11,
  bit_and: 12,
  shift: 13,
  additive: 14,
  multiplicative: 15,
  unary: 16,
  power: 17,
  postfix: 18,
};

const commaSep = (rule) => optional(commaSep1(rule));
const commaSep1 = (rule) => seq(rule, repeat(seq(',', rule)));
// A comma separated list that may end with a comma (and may be empty).
const list = (rule) => seq(commaSep(rule), optional(','));

const declPrefix = ($) => [repeat($.attribute), optional($.modifiers)];

// `struct`, `obj` and `interface` share one shape.
const typeDeclaration = (keyword) => ($) =>
  seq(
    ...declPrefix($),
    keyword,
    field('name', $._type_name),
    optional($.generic_parameters),
    optional($.base_types),
    optional($.where_clause),
    field('body', $.field_declaration_list),
  );

module.exports = grammar({
  name: 'pelago',

  externals: ($) => [
    $._newline,
    $._generic_lt,
    $._generic_lt_expr,
    $._index_lbracket,
    $._question_adjacent,
    $._call_bang,
    $._map_kw,
    $._set_kw,
    $._select_kw,
    $._scope_kw,
    $._soa_kw,
    $._type_kw,
    $._test_kw,
    $._modifier_kw,
    $._ext_kw,
    $._init_kw,
    $._default_switch,
    $._default_select,
    $.block_comment,
    $.doc_block_comment,
  ],

  extras: ($) => [/[ \t\r\f\n]/, $.line_comment, $.doc_comment, $.block_comment, $.doc_block_comment],

  word: ($) => $.identifier,

  supertypes: ($) => [$._expression, $._type, $._pattern],

  conflicts: ($) => [
    [$._expression, $.struct_expression, $.break_expression],
    [$._expression, $.struct_expression],
    [$.tuple_type, $.tuple_pattern],
    [$.tuple_type, $.tuple_pattern, $.tuple_expression],
    [$.where_clause],
    [$._type_name, $._pattern, $._expression, $.capture],
    [$._type_name, $._pattern, $._expression, $.closure_parameter],
    [$._type_name, $._expression, $.capture],
    [$._type_name, $.scoped_identifier, $._expression],
    [$._type_name, $._expression, $.closure_parameter],
    [$.tuple_type, $.tuple_expression],
    [$.tuple_type, $.closure_parameters],
    [$.labeled_expression, $._expression],
    [$._statements],
    [$._type_name, $._expression],
    [$._type_name, $.closure_parameter],
    [$._type, $._expression],
    [$.switch_arm],
    [$.list_pattern, $.builder_clause],
    [$.variant_pattern],
    [$.binding_pattern, $._expression],
    [$._expression, $.field_initializer],
    [$.block, $.struct_literal_body],
    [$.unless_expression],
    [$.if_expression],
    [$._pattern, $.struct_pattern],
    [$.field_pattern, $.field_initializer],
    [$.struct_pattern, $.struct_literal_body],
    [$.tuple_expression, $.closure_parameters],
    [$.rest_pattern, $._expression],
    [$.binding_pattern, $.closure_parameter],
    [$.if_expression, $.meta_if_expression],
    [$._expression, $.select_arm],
    [$._expression, $.closure_parameter, $.break_expression],
    [$._type, $.scoped_type_identifier],
    [$._pattern, $._pattern_literal],
    [$._pattern, $.as_pattern],
    [$._pattern, $.scoped_identifier],
    [$._pattern, $.variant_pattern],
    [$.range_pattern],
    [$.closure_parameter],
    [$.binding_pattern, $.capture],
    [$.continue_expression],
    [$.list_pattern, $.list_expression],
    [$.negated_literal, $._literal],
    [$.variant_pattern, $.dot_name],
    [$._pattern, $._expression, $.closure_parameter],
    [$.tuple_pattern, $.tuple_expression],
    [$.closure_parameter, $.break_expression],
    [$._expression, $.break_expression],
    [$._pattern_literal, $._expression],
    [$.as_pattern, $._expression],
    [$._expression, $.capture],
    [$.scoped_identifier, $._expression],
    [$._pattern, $._expression, $.capture],
    [$.variant_pattern, $._expression],
    [$._pattern, $._expression],
    [$.list_expression, $.closure_captures],
    [$._expression, $.closure_parameter],],

  rules: {
    source_file: ($) => optional($._statements),

    // ---- Statements ------------------------------------------------------------------------

    _terminator: ($) => choice($._newline, ';'),

    // Statements are separated by newlines or `;`. The last one may be followed by nothing.
    _statements: ($) =>
      choice(
        repeat1($._terminator),
        seq(
          repeat($._terminator),
          $._statement,
          repeat(seq(repeat1($._terminator), $._statement)),
          repeat($._terminator),
        ),
      ),

    _statement: ($) => choice($._declaration, $._expression, $.update_expression, $.labeled_expression),

    update_expression: ($) => prec(PREC.postfix, seq($._expression, choice('++', '--'))),

    labeled_expression: ($) =>
      seq(
        field('label', alias($.identifier, $.label)),
        ':',
        choice($.for_expression, $.while_expression, $.loop_expression, $.do_while_expression, $.block),
      ),

    block: ($) => seq('{', optional($._statements), '}'),

    // ---- Declarations ----------------------------------------------------------------------

    _declaration: ($) =>
      choice(
        $.function_declaration,
        $.constructor_declaration,
        $.destructor_declaration,
        $.struct_declaration,
        $.obj_declaration,
        $.interface_declaration,
        $.enum_declaration,
        $.impl_declaration,
        $.variable_declaration,
        $.mod_declaration,
        $.import_declaration,
        $.type_alias,
        $.test_declaration,
      ),

    attribute: ($) => seq('@', field('name', $.identifier), optional(field('arguments', $.argument_list))),

    visibility_modifier: ($) => choice(seq('pub', optional('pkg')), 'priv', 'export'),

    modifier: ($) => $._modifier_kw,
    ext_modifier: ($) => $._ext_kw,

    _modifier: ($) =>
      choice(
        $.visibility_modifier,
        'meta',
        'async',
        'unsafe',
        $.modifier,
        seq($.ext_modifier, optional(field('abi', $.string_literal))),
      ),

    modifiers: ($) => repeat1($._modifier),

    function_declaration: ($) =>
      seq(
        ...declPrefix($),
        'fn',
        field('name', $.identifier),
        optional($.generic_parameters),
        field('parameters', $.parameter_list),
        optional(seq('->', field('return_type', $._type))),
        optional($.where_clause),
        optional(field('body', $._function_body)),
      ),

    constructor_declaration: ($) =>
      seq(
        ...declPrefix($),
        alias($._init_kw, 'init'),
        field('parameters', $.parameter_list),
        optional(seq('->', field('return_type', $._type))),
        optional($.where_clause),
        field('body', $._function_body),
      ),

    destructor_declaration: ($) =>
      seq(...declPrefix($), 'deinit', optional(field('parameters', $.parameter_list)), field('body', $._function_body)),

    _function_body: ($) => choice($.block, $.expression_body),

    expression_body: ($) => seq('=', $._expression),

    parameter_list: ($) => seq('(', list($.parameter), ')'),

    mode: ($) => choice('let', 'inout', 'sink'),

    parameter: ($) =>
      seq(
        optional($.mode),
        optional('...'),
        field('name', choice($.self, $.identifier)),
        optional(seq(':', field('type', $._type))),
        optional(seq('=', field('default', $._expression))),
      ),

    generic_parameters: ($) => seq($._generic_lt, commaSep1($.generic_parameter), '>'),

    generic_parameter: ($) =>
      seq(
        optional('meta'),
        field('name', $._type_name),
        optional(seq(':', $._type, repeat(seq('+', $._type)))),
      ),

    where_clause: ($) => seq('where', commaSep1($.where_predicate)),

    where_predicate: ($) =>
      seq(field('type', $._type), ':', field('bounds', $._type), repeat(seq('+', field('bounds', $._type)))),

    // struct / obj / interface

    struct_declaration: typeDeclaration('struct'),
    obj_declaration: typeDeclaration('obj'),
    interface_declaration: typeDeclaration('interface'),

    base_types: ($) => seq(':', commaSep1($._type)),

    field_declaration_list: ($) =>
      seq('{', repeat(choice($.field_declaration, $._declaration, $._terminator, ',')), '}'),

    field_declaration: ($) =>
      seq(
        repeat($.attribute),
        optional($.modifiers),
        field('name', $.identifier),
        ':',
        field('type', $._type),
        optional(seq('=', field('default', $._expression))),
      ),

    enum_declaration: ($) =>
      seq(
        ...declPrefix($),
        'enum',
        field('name', $._type_name),
        optional($.generic_parameters),
        optional($.base_types),
        optional($.where_clause),
        field('body', $.enum_body),
      ),

    enum_body: ($) => seq('{', repeat(choice($.enum_variant, $._declaration, $._terminator, ',')), '}'),

    enum_variant: ($) =>
      seq(
        repeat($.attribute),
        field('name', $.identifier),
        optional($.variant_fields),
        optional(seq('=', field('value', $._expression))),
      ),

    variant_fields: ($) => seq('(', list($.variant_field), ')'),

    variant_field: ($) => seq(optional(seq(field('name', $.identifier), ':')), field('type', $._type)),

    impl_declaration: ($) =>
      seq(
        ...declPrefix($),
        'impl',
        optional($.generic_parameters),
        field('trait', $._type),
        optional(seq('for', field('type', $._type))),
        optional($.where_clause),
        field('body', $.declaration_list),
      ),

    declaration_list: ($) => seq('{', repeat(choice($._declaration, $._terminator, ',')), '}'),

    variable_declaration: ($) =>
      seq(
        ...declPrefix($),
        choice('const', 'var'),
        field('pattern', $._pattern),
        optional(seq(':', field('type', $._type))),
        optional(seq('=', field('value', $._expression))),
      ),

    mod_declaration: ($) => seq(...declPrefix($), 'mod', field('name', $.module_path), optional(field('body', $.block))),

    module_path: ($) => seq($.identifier, repeat(seq('.', $.identifier))),

    import_declaration: ($) =>
      seq(
        ...declPrefix($),
        choice('import', 'use'),
        optional(choice(seq($.import_names, '='), seq(field('alias', $.identifier), '='))),
        field('path', $.import_path),
      ),

    import_names: ($) => seq('{', list(choice($.import_name, '...')), '}'),

    import_name: ($) => seq(field('name', $.identifier), optional(seq(':', field('alias', $.identifier)))),

    import_path: ($) => seq(optional(choice('.', '..', '...')), $.module_path),

    type_alias: ($) =>
      seq(
        ...declPrefix($),
        alias($._type_kw, 'type'),
        field('name', $._type_name),
        optional($.generic_parameters),
        '=',
        field('type', $._type),
      ),

    test_declaration: ($) => seq(alias($._test_kw, 'test'), field('name', $.string_literal), field('body', $.block)),

    // ---- Types -----------------------------------------------------------------------------

    _type: ($) =>
      choice(
        $._type_name,
        $.scoped_type_identifier,
        $.generic_type,
        $.self_type,
        $.list_type,
        $.array_type,
        $.tuple_type,
        $.function_type,
        $.option_type,
        $.result_type,
        $.any_type,
      ),

    type_identifier: ($) => $.identifier,
    label: ($) => $.identifier,
    _type_name: ($) => alias($.identifier, $.type_identifier),

    scoped_type_identifier: ($) =>
      seq(field('path', choice($._type_name, $.scoped_type_identifier)), '.', field('name', $._type_name)),

    generic_type: ($) =>
      seq(field('type', choice($._type_name, $.scoped_type_identifier)), field('arguments', $.type_arguments)),

    type_arguments: ($) => seq($._generic_lt, commaSep1($._generic_argument), '>'),

    _expr_type_arguments: ($) =>
      seq(alias($._generic_lt_expr, '<'), commaSep1($._generic_argument), '>'),

    _generic_argument: ($) => choice($._type, $._const_argument),

    _const_argument: ($) =>
      choice($.integer_literal, $.float_literal, $.string_literal, $.character_literal, $.boolean_literal, $.negated_literal),

    negated_literal: ($) => seq('-', choice($.integer_literal, $.float_literal)),

    self_type: ($) => 'Self',

    list_type: ($) => seq('[', $._type, ']'),

    array_type: ($) => seq('[', field('element', $._type), ';', field('length', $._expression), ']'),

    tuple_type: ($) => seq('(', list($._type), optional(seq(';', field('length', $._expression))), ')'),

    function_type: ($) =>
      prec.right(
        seq('fn', '(', list(seq(optional($.mode), $._type)), ')', optional(seq('->', field('return_type', $._type)))),
      ),

    option_type: ($) => prec.left(1, seq($._type, alias($._question_adjacent, '?'))),

    result_type: ($) => prec.right(0, seq(field('ok', $._type), '!', field('error', $._type))),

    any_type: ($) => prec(2, seq('any', $._type)),

    // ---- Patterns --------------------------------------------------------------------------

    _pattern: ($) =>
      choice(
        $.identifier,
        $.binding_pattern,
        $.none,
        $._literal,
        $.negated_literal,
        $.range_pattern,
        $.tuple_pattern,
        $.list_pattern,
        $.variant_pattern,
        $.struct_pattern,
        $.as_pattern,
        $.scoped_identifier,
      ),

    binding_pattern: ($) => seq($.mode, $.identifier),

    _pattern_literal: ($) => choice($._literal, $.negated_literal),

    range_pattern: ($) =>
      choice(
        seq($._pattern_literal, choice('..', '..='), optional($._pattern_literal)),
        seq(choice('..', '..='), optional($._pattern_literal)),
      ),

    tuple_pattern: ($) => seq('(', list($._pattern), ')'),

    list_pattern: ($) => seq('[', list(choice($.rest_pattern, $._pattern)), ']'),

    rest_pattern: ($) => seq('...', optional($.mode), optional($.identifier)),

    scoped_identifier: ($) =>
      seq(field('path', choice($.identifier, $.scoped_identifier)), '.', field('name', $.identifier)),

    variant_pattern: ($) =>
      choice(
        seq('.', field('name', $.identifier), optional($.pattern_arguments)),
        seq(field('name', choice($.identifier, $.scoped_identifier)), $.pattern_arguments),
      ),

    pattern_arguments: ($) => seq('(', list($.pattern_argument), ')'),

    pattern_argument: ($) => seq(optional(seq(field('label', $.identifier), ':')), $._pattern),

    struct_pattern: ($) =>
      seq(
        field('type', choice($.identifier, $.scoped_identifier)),
        '{',
        list(choice($.field_pattern, '..')),
        '}',
      ),

    field_pattern: ($) => seq(optional($.mode), field('name', $.identifier), optional(seq(':', $._pattern))),

    as_pattern: ($) => seq(field('name', $.identifier), 'is', $._pattern),

    // ---- Expressions -----------------------------------------------------------------------

    _expression: ($) =>
      choice(
        $.binary_expression,
        $.unary_expression,
        $.copy_expression,
        $.ternary_expression,
        $.assignment_expression,
        $.range_expression,
        $.convert_expression,
        $.is_expression,
        $.try_expression,
        $.call_expression,
        $.field_expression,
        $.index_expression,
        $.type_application,
        $.struct_expression,
        $.closure_expression,
        $.parenthesized_expression,
        $.tuple_expression,
        $.list_expression,
        $.repeat_expression,
        $.list_builder,
        $.map_expression,
        $.set_expression,
        $.map_builder,
        $.set_builder,
        $.soa_expression,
        $.block,
        $.if_expression,
        $.unless_expression,
        $.switch_expression,
        $.select_expression,
        $.scope_expression,
        $.loop_expression,
        $.while_expression,
        $.do_while_expression,
        $.for_expression,
        $.guard_expression,
        $.break_expression,
        $.continue_expression,
        $.return_expression,
        $.yield_expression,
        $.become_expression,
        $.defer_expression,
        $.meta_expression,
        $.meta_if_expression,
        $.unsafe_block,
        $.await_expression,
        $.dot_name,
        $._literal,
        $.identifier,
        $.self,
        $.super,
        $.self_type,
        $.none,
      ),

    _literal: ($) => choice($.integer_literal, $.float_literal, $.character_literal, $.string_literal, $.boolean_literal),

    self: ($) => 'self',
    super: ($) => 'super',
    none: ($) => 'none',
    boolean_literal: ($) => choice('true', 'false'),

    dot_name: ($) => seq('.', field('name', $.identifier)),

    binary_expression: ($) => {
      const table = [
        [PREC.pipe, '|>'],
        [PREC.coalesce, '??'],
        [PREC.or, '||'],
        [PREC.and, '&&'],
        [PREC.compare, choice('==', '!=', '<', '>', '<=', '>=', 'in')],
        [PREC.bit_or, '|'],
        [PREC.bit_xor, '^'],
        [PREC.bit_and, '&'],
        [PREC.shift, choice('<<', '>>')],
        [PREC.additive, choice('+', '-', '+%', '-%')],
        [PREC.multiplicative, choice('*', '/', '%', '*%')],
      ];
      return choice(
        ...table.map(([p, op]) =>
          prec.left(p, seq(field('left', $._expression), field('operator', op), field('right', $._expression))),
        ),
        prec.right(
          PREC.power,
          seq(field('left', $._expression), field('operator', '**'), field('right', $._expression)),
        ),
      );
    },

    unary_expression: ($) =>
      prec(PREC.unary, seq(field('operator', choice('-', '!', '!!', '~')), field('operand', $._expression))),

    copy_expression: ($) => prec(PREC.unary, seq('copy', $._expression)),

    await_expression: ($) => prec(PREC.unary, seq('await', $._expression)),

    meta_expression: ($) => prec(PREC.unary, seq('meta', $._expression)),

    ternary_expression: ($) =>
      prec.right(
        PREC.ternary,
        seq(
          field('condition', $._expression),
          '?',
          field('then', $._expression),
          ':',
          field('else', $._expression),
        ),
      ),

    assignment_expression: ($) =>
      prec.right(
        PREC.assign,
        seq(
          field('left', $._expression),
          field(
            'operator',
            choice('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=', '+%=', '-%=', '*%=', '??='),
          ),
          field('right', $._expression),
        ),
      ),

    range_expression: ($) =>
      prec.left(
        PREC.range,
        choice(
          seq($._expression, choice('..', '..='), optional($._expression), optional(seq('step', $._expression))),
          seq(choice('..', '..='), optional($._expression), optional(seq('step', $._expression))),
        ),
      ),

    convert_expression: ($) =>
      prec.left(
        PREC.convert,
        seq(field('value', $._expression), 'to', optional(alias($._question_adjacent, '?')), field('type', $._type)),
      ),

    is_expression: ($) =>
      prec.left(PREC.compare, seq(field('value', $._expression), 'is', field('pattern', $._pattern))),

    try_expression: ($) => prec(PREC.postfix, seq($._expression, alias($._question_adjacent, '?'))),

    call_expression: ($) =>
      prec(
        PREC.postfix,
        seq(
          field('function', $._expression),
          optional(alias($._call_bang, $.call_mark)),
          field('arguments', $.argument_list),
        ),
      ),

    argument_list: ($) => seq('(', choice($.stream_builder, list($.argument)), ')'),

    argument: ($) =>
      seq(
        optional($.mode),
        optional(seq(field('label', $.identifier), ':')),
        optional('...'),
        field('value', $._expression),
      ),

    field_expression: ($) =>
      prec(
        PREC.postfix,
        seq(
          field('value', $._expression),
          choice('.', '?.'),
          field('field', choice($.identifier, $.tuple_index)),
        ),
      ),

    tuple_index: ($) => /[0-9]+/,

    index_expression: ($) =>
      prec(PREC.postfix, seq(field('value', $._expression), $._index_lbracket, field('index', $._expression), ']')),

    type_application: ($) =>
      prec(
        PREC.postfix,
        seq(
          field('value', choice($.identifier, $.field_expression, $.self_type)),
          field('arguments', alias($._expr_type_arguments, $.type_arguments)),
        ),
      ),

    struct_expression: ($) =>
      seq(
        field('type', choice($.identifier, $.field_expression, $.type_application, $.self_type)),
        field('body', $.struct_literal_body),
      ),

    struct_literal_body: ($) =>
      seq('{', optional(seq($._struct_init, repeat(seq(optional(','), $._struct_init)), optional(','))), '}'),

    _struct_init: ($) => choice($.field_initializer, $.spread_element),

    field_initializer: ($) => seq(field('name', $.identifier), optional(seq(':', field('value', $._expression)))),

    spread_element: ($) => seq('...', $._expression),

    parenthesized_expression: ($) => seq('(', $._expression, ')'),

    tuple_expression: ($) => choice(seq('(', ')'), seq('(', $._expression, ',', list($._expression), ')')),

    _list_item: ($) => choice($._expression, $.spread_element),

    list_expression: ($) => seq('[', list($._list_item), ']'),

    repeat_expression: ($) => seq('[', $._expression, ';', $._expression, ']'),

    map_expression: ($) =>
      choice(
        seq(alias($._map_kw, 'map'), '{', list(choice($.map_pair, $.spread_element)), '}'),
        seq('{', ':', '}'),
      ),

    map_pair: ($) => seq(field('key', $._expression), ':', field('value', $._expression)),

    set_expression: ($) => seq(alias($._set_kw, 'set'), '{', list($._list_item), '}'),

    soa_expression: ($) => seq(alias($._soa_kw, 'soa'), '[', list($._list_item), ']'),

    // Builders: `[x of xs if c ~> y]`, `set { ... }`, `map { ... }`, `f(x of xs ~> y)`

    _builder_core: ($) =>
      seq(
        $.builder_clause,
        repeat(choice(seq(',', $.builder_clause), $.builder_clause)),
        optional(seq('~>', field('result', $._expression), optional(seq(':', field('value', $._expression))))),
      ),

    builder_clause: ($) =>
      choice(seq(commaSep1($._pattern), 'of', field('source', $._expression)), seq('if', field('condition', $._expression))),

    list_builder: ($) => seq('[', $._builder_core, ']'),
    set_builder: ($) => seq(alias($._set_kw, 'set'), '{', $._builder_core, '}'),
    map_builder: ($) => seq(alias($._map_kw, 'map'), '{', $._builder_core, '}'),
    stream_builder: ($) => $._builder_core,

    // Closures: `x ~> e`, `(a, b) ~> e`, `[inout n] (a: int) -> int ~> e`, `~> it * 2`

    closure_expression: ($) =>
      prec.right(
        PREC.closure,
        seq(
          optional($.closure_captures),
          optional(choice($.closure_parameters, $.closure_parameter)),
          optional(seq('->', field('return_type', $._type))),
          '~>',
          field('body', choice($._expression, $.update_expression)),
        ),
      ),

    closure_captures: ($) => seq('[', list($.capture), ']'),

    capture: ($) => seq(optional($.mode), $.identifier),

    closure_parameters: ($) => seq('(', list($.closure_parameter), ')'),

    closure_parameter: ($) => seq(optional($.mode), field('name', $.identifier), optional(seq(':', field('type', $._type)))),

    // ---- Control flow ------------------------------------------------------------------------

    if_expression: ($) =>
      seq('if', field('condition', $._expression), field('consequence', $.block), optional($.else_clause)),

    unless_expression: ($) =>
      seq('unless', field('condition', $._expression), field('consequence', $.block), optional($.else_clause)),

    else_clause: ($) => seq('else', choice($.block, $.if_expression)),

    meta_if_expression: ($) =>
      seq('meta', 'if', field('condition', $._expression), field('consequence', $.block), optional($.else_clause)),

    switch_expression: ($) =>
      seq('switch', optional($.mode), field('value', $._expression), field('body', $.switch_body)),

    switch_body: ($) => seq('{', repeat(choice($.switch_arm, ';')), '}'),

    switch_arm: ($) =>
      choice(
        seq(
          'case',
          commaSep1(field('pattern', $._pattern)),
          optional(seq('if', field('guard', $._expression))),
          ':',
          optional($._statements),
        ),
        seq(alias($._default_switch, 'default'), ':', optional($._statements)),
      ),

    select_expression: ($) => seq(alias($._select_kw, 'select'), '{', repeat($.select_arm), '}'),

    select_arm: ($) =>
      choice(
        seq(
          'case',
          optional(seq(field('binding', $.identifier), '=')),
          field('operation', $._expression),
          field('body', $.block),
        ),
        seq(alias($._default_select, 'default'), field('body', $.block)),
      ),

    scope_expression: ($) => seq(alias($._scope_kw, 'scope'), field('name', $.identifier), field('body', $.block)),

    loop_expression: ($) => seq('loop', field('body', $.block)),

    while_expression: ($) => seq('while', field('condition', $._expression), field('body', $.block)),

    do_while_expression: ($) => seq('do', field('body', $.block), 'while', field('condition', $._expression)),

    for_expression: ($) =>
      choice(
        seq(
          'for',
          commaSep1(field('pattern', $._pattern)),
          'of',
          field('iterable', $._expression),
          field('body', $.block),
        ),
        seq('for', field('pattern', $._pattern), 'in', field('iterable', $._expression), field('body', $.block)),
        seq(
          'for',
          '(',
          optional(field('initializer', choice($.variable_declaration, $._expression, $.update_expression))),
          ';',
          optional(field('condition', $._expression)),
          ';',
          optional(field('update', choice($._expression, $.update_expression))),
          ')',
          field('body', $.block),
        ),
      ),

    guard_expression: ($) => seq('guard', field('condition', $._expression), 'else', field('else', $.block)),

    break_expression: ($) =>
      prec.right(seq('break', optional(field('label', alias($.identifier, $.label))), optional($._expression))),

    continue_expression: ($) => seq('continue', optional(field('label', alias($.identifier, $.label)))),

    return_expression: ($) => prec.right(seq('return', optional($._expression))),
    yield_expression: ($) => prec.right(seq('yield', optional($._expression))),
    become_expression: ($) => prec.right(seq('become', $._expression)),
    defer_expression: ($) => prec.right(seq('defer', $._expression)),

    unsafe_block: ($) => seq('unsafe', $.block),

    // ---- Tokens ------------------------------------------------------------------------------

    identifier: ($) => /[\p{L}_][\p{L}\p{N}_]*/,

    integer_literal: ($) => token(choice(/0[xX][0-9a-fA-F_]+/, /0[bB][01_]+/, /0[oO][0-7_]+/, /[0-9][0-9_]*/)),

    float_literal: ($) =>
      token(
        choice(
          /[0-9][0-9_]*\.[0-9][0-9_]*([eE][+-]?[0-9][0-9_]*)?/,
          /[0-9][0-9_]*[eE][+-]?[0-9][0-9_]*/,
        ),
      ),

    character_literal: ($) => token(seq("'", choice(/[^'\\\n]/, /\\[ntr0\\'"]/, /\\u\{[0-9a-fA-F]{1,6}\}/), "'")),

    string_literal: ($) =>
      seq(
        '"',
        repeat(choice($.string_content, $.escape_sequence, $.interpolation)),
        token.immediate('"'),
      ),

    string_content: ($) => token.immediate(prec(1, /[^"\\#]+/)),

    escape_sequence: ($) => token.immediate(/\\([ntr0\\'"#]|u\{[0-9a-fA-F]{1,6}\})/),

    interpolation: ($) =>
      choice(
        seq(token.immediate('#('), $._expression, ')'),
        seq(token.immediate('#'), alias($._interpolated_name, $.identifier)),
      ),

    _interpolated_name: ($) => token.immediate(/[\p{L}_][\p{L}\p{N}_]*/),

    line_comment: ($) => token(choice(/\/\/\/\/[^\n]*/, /\/\/([^\/\n][^\n]*)?/)),

    doc_comment: ($) => token(/\/\/\/([^\/*\n][^\n]*)?/),
  },
});
