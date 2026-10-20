# tree-sitter-pelago

A [Tree-sitter](https://tree-sitter.github.io/) grammar for [Pelago](https://github.com/pelagolang/pelago)
source files (`.pel`), for syntax highlighting, code folding, outlines and structural editing in editors
such as Zed and Neovim.

The compiler's own recursive-descent parser (`compiler/src/parser/` in the Pelago repository) is the
oracle for what is valid. This grammar follows it construct by construct and is checked against the
compiler's examples (see [How it is checked](#how-it-is-checked)).

## What is here

| Path | What |
|---|---|
| `grammar.js` | the grammar |
| `src/scanner.c` | the external scanner (newlines, spacing rules, contextual keywords, comments) |
| `src/parser.c`, `src/grammar.json`, `src/node-types.json`, `src/tree_sitter/` | generated, **committed on purpose** so editors can build the grammar from a commit without running the generator |
| `queries/` | `highlights.scm`, `brackets.scm`, `indents.scm`, `outline.scm` |
| `test/corpus/` | one test per construct, run by `tree-sitter test` |
| `script/check-corpus.sh` | parses every file of a Pelago checkout and fails on any `ERROR` or `MISSING` node |
| `tree-sitter.json` | grammar metadata (name `pelago`, scope `source.pelago`, file type `pel`) |

## Build and test

You need the Tree-sitter CLI (0.27) and a C compiler. `npm install` fetches the CLI as a dev dependency.

```
npm ci                           # tree-sitter-cli 0.27.1 (from package-lock.json)
npx tree-sitter generate --abi 14   # after changing grammar.js or src/scanner.c; also `npm run generate`
npx tree-sitter test             # the corpus in test/corpus
npx tree-sitter parse file.pel   # print a syntax tree
npx tree-sitter highlight file.pel   # needs a configured parser directory
script/check-corpus.sh /path/to/pelago   # every .pel file of the checkout
```

The generated parser uses language ABI 14, which every current editor can load (Neovim 0.10 and later,
Zed, Helix). It is large (about 16 MB of C, roughly 9000 parse states) because a precedence-based
expression grammar with this many forms multiplies states per context; the compiled parser is a few MB.
After editing the grammar, regenerate and commit the files in `src/` together with the change.

## What the external scanner does

Parts of Pelago depend on spacing or on what comes next, which an LR grammar cannot see. They live in
`src/scanner.c`, which keeps no state. The rules are the ones in `docs/design/parser.md` of the Pelago repository.

**Newlines.** The scanner emits `_newline` only where the grammar can end a statement (a list of statements
in a block, a `case` arm, the file, a type body). A newline does *not* end the statement when the next
non-blank, non-comment line starts with `.` (but not `..`), `?.`, `|>`, `&&`, `||`, `??`, a ternary `?` or
`:`, or with `else`. Newlines inside `( )`, `[ ]` and the braces of struct, map and set literals never reach
the grammar, and after a binary operator, `=`, `,` or `->` the grammar expects an operand, so those
newlines are plain whitespace. `;` also ends a statement.

**Spacing (a token that touches the one before it).**

| Written | Meaning |
|---|---|
| `f()?`, `int?` | error propagation, option type (a `?` touching the previous token) |
| `a ? b : c` | ternary (a `?` that does not touch). `?.` and `??` are their own tokens |
| `max<int>(3, 7)`, `map<string, int>` | generic arguments (a `<` touching a name) |
| `a < b`, `a <= b`, `a << b` | operators. In an expression a touching `<` is generic only if a matching `>` follows soon, followed by `(`, `{` or `.` |
| `xs[0]` | indexing (a `[` touching an expression). `return [0]` is a list |
| `c.bump!()`, `c.close!!()` | call marks (`!` or `!!` touching a name, followed by `(`). `a != b` is unaffected |

**Contextual keywords.** Soft keywords stay ordinary identifiers except where they act as keywords. Tree-sitter
already does this when the identifier is not valid in the same place (`to` after an expression, `step` after
a range, `where` after a signature). Where both readings are possible the scanner looks ahead, as the compiler does:
`map {`, `set {`, `select {`, `scope name {`, `soa [`, `type name =`, `test "name"`, `init(`, `default:` / `default {`,
and the modifiers `open abstract final override linear static ext` (only in front of a declaration).

**Brackets that open closures and builders.** Like the compiler, the scanner pairs brackets to decide what
`(` and `[` start: `(a, b) ~>` and `(x: int) -> T ~>` open closure parameters, `[inout n] x ~>` opens a capture
list, and a bracket with `of` directly inside (`[x of xs ~> y]`, `map { u of users ~> k : v }`,
`sum(x of xs ~> x)`) opens a builder. This keeps the grammar free of expression/pattern ambiguity.

**Struct literals are not allowed in conditions.** After an expression, a `{` is a block when a block is
possible (after `if`, `while`, `for ... of`, `switch`, `guard`, `unless` and `select` conditions) and a struct
literal otherwise, so `if x { y }` is a block and `point { x: 1 }` is a struct literal. Inside parentheses a
struct literal is fine again: `if (point { x: 1 }) == p { }`.

**Comments.** `//` and `///` (doc) comments are in the grammar; nested `/* */` and `///* ... *///` doc blocks are
scanned, because they cannot be written as a regular expression.

## Things to know about the trees

* Names in type positions are `type_identifier`; `_` is an ordinary `identifier`.
* `a..b step c` is a `range_expression` whose first child is the range `a..b`.
* `break name` is read as a label. When no loop has that label the compiler reads `name` as a value; the grammar
  cannot know, so a lone name after `break` is always a `label`.
* Strings are `string_literal` nodes with `string_content`, `escape_sequence` and `interpolation` children;
  `#name` and `#(expr)` contain a real `identifier` or expression.
* `modifiers` hold the modifier keywords of a declaration; `attribute` nodes (`@name(args)`) come before them.
* `super` is accepted as an expression (the design docs use it) although the compiler does not parse it yet.

## How it is checked

* `tree-sitter test` runs `test/corpus/*.txt`, small snippets per construct with the expected trees.
* `script/check-corpus.sh <pelago checkout>` parses every `.pel` file (examples, benches, tests, `compiler/core/core.pel`) (about 2000
  lines of real library code) and exits non-zero on any `ERROR` or `MISSING` node.
* The snippets in the compiler's parser tests (`compiler/src/parser/tests.rs`: the arguments of `one(...)`, `all(...)`,
  `assert_clean(...)` and `assert_flagged(...)`) and the code blocks of its design documents were also parsed with
  this grammar and compared with the compiler: nothing the compiler's parser accepts is rejected here, except for the
  differences below.

## Differences from the compiler's parser

The grammar is meant for editors, so it is a little more forgiving in some places and stricter in a few corners:

* It does not run the compiler's syntax checks (`check.rs`): `break` outside a loop, a function without a body,
  repeated parameter names, `init` outside a type and so on are not errors here.
* Not accepted although the compiler's parser takes them: `deinit() -> T { }` (a `deinit` takes nothing) and fields
  inside an `impl` body. Both are rejected by the compiler's own syntax checks anyway.
* More forgiving: members of a type body need no separator between them, struct literal fields may omit commas,
  and a hard keyword can act as a plain name where only a name makes sense (`x.in`). Contextual keywords are
  detected with bounded look-ahead, so unusual layouts (a modifier and the declaration it applies to many lines apart,
  `soa` and its `[` on different lines) can read differently from the compiler.
* A touching `<` in an expression is generic only when `>` and then `(`, `{` or `.` follow on the same line; the compiler
  tries the generic reading and backs out. A few odd spellings such as `a<b, c>(d)` read as generics here.
* The identifier pattern `\p{L}_` covers letters and digits of any script; the compiler's `is_alphabetic` / `is_alphanumeric`
  can differ on rare characters.

## Using the grammar

**Zed.** In an extension's `extension.toml`:

```toml
[grammars.pelago]
repository = "https://github.com/pelagolang/tree-sitter-pelago"
commit = "<commit>"
```

and copy `queries/*.scm` into the language directory of the extension (`languages/pelago/`).

**Neovim (nvim-treesitter).**

```lua
vim.filetype.add({ extension = { pel = "pelago" } })
require("nvim-treesitter.parsers").get_parser_configs().pelago = {
  install_info = {
    url = "https://github.com/pelagolang/tree-sitter-pelago",
    files = { "src/parser.c", "src/scanner.c" },
  },
  filetype = "pelago",
}
```

Then run `:TSInstall pelago` and copy `queries/*.scm` to `queries/pelago/` on your runtime path.
(`indents.scm` and `outline.scm` use Zed's capture names; Neovim ignores what it does not know.)
