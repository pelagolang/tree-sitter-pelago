// External scanner for the Pelago grammar.
//
// It handles the parts of the language that depend on spacing and on what comes next:
//   * newline-terminated statements (with the continuation rules of docs/design/parser.md),
//   * `?`, `<`, `[`, `!` / `!!` that touch the token before them,
//   * soft keywords that are keywords only in some positions (`map {`, `type x =`, `open fn`, ...),
//   * nested block comments and `///* ... *///` doc blocks.
// The scanner keeps no state, so it needs no serialization.

#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// The order must match `externals` in grammar.js.
enum TokenType {
  NEWLINE,
  GENERIC_LT,
  GENERIC_LT_EXPR,
  INDEX_LBRACKET,
  QUESTION_ADJACENT,
  CALL_BANG,
  MAP_KW,
  SET_KW,
  SELECT_KW,
  SCOPE_KW,
  SOA_KW,
  TYPE_KW,
  TEST_KW,
  MODIFIER_KW,
  EXT_KW,
  INIT_KW,
  DEFAULT_SWITCH,
  DEFAULT_SELECT,
  BUILDER_LBRACKET,
  BUILDER_LPAREN,
  BUILDER_LBRACE,
  CLOSURE_LPAREN,
  CLOSURE_LBRACKET,
  BLOCK_LBRACE,
  STRUCT_LBRACE,
  BLOCK_COMMENT,
  DOC_BLOCK_COMMENT,
};

#define WORD_MAX 16

void *tree_sitter_pelago_external_scanner_create(void) { return NULL; }
void tree_sitter_pelago_external_scanner_destroy(void *payload) { (void)payload; }
unsigned tree_sitter_pelago_external_scanner_serialize(void *payload, char *buffer) {
  (void)payload;
  (void)buffer;
  return 0;
}
void tree_sitter_pelago_external_scanner_deserialize(void *payload, const char *buffer,
                                                      unsigned length) {
  (void)payload;
  (void)buffer;
  (void)length;
}

static inline void adv(TSLexer *l) { l->advance(l, false); }
static inline void skip(TSLexer *l) { l->advance(l, true); }

static bool is_ident_start(int32_t c) {
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
}
static bool is_ident_char(int32_t c) { return is_ident_start(c) || (c >= '0' && c <= '9'); }
static bool is_blank(int32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f'; }

static void skip_blanks(TSLexer *l) {
  while (is_blank(l->lookahead)) adv(l);
}
static void skip_space_and_newlines(TSLexer *l) {
  while (is_blank(l->lookahead) || l->lookahead == '\n') adv(l);
}

// Reads an identifier-like word into `buf` (at most WORD_MAX chars, `*len` is its true length).
static void read_word(TSLexer *l, char *buf, unsigned *len) {
  unsigned n = 0;
  while (is_ident_char(l->lookahead)) {
    if (n < WORD_MAX) buf[n] = (char)l->lookahead;
    n++;
    adv(l);
  }
  *len = n;
  buf[n < WORD_MAX ? n : WORD_MAX - 1] = 0;
  if (n < WORD_MAX) buf[n] = 0;
}

static bool word_is(const char *buf, unsigned len, const char *w) {
  return len == strlen(w) && len < WORD_MAX && strcmp(buf, w) == 0;
}

// ---- Comments ----------------------------------------------------------------------------------

// After `/*` was consumed: consumes up to and including the matching `*/` (they nest).
static bool finish_block_comment(TSLexer *l) {
  unsigned depth = 1;
  while (!l->eof(l)) {
    if (l->lookahead == '/') {
      adv(l);
      if (l->lookahead == '*') {
        adv(l);
        depth++;
      }
    } else if (l->lookahead == '*') {
      adv(l);
      if (l->lookahead == '/') {
        adv(l);
        if (--depth == 0) return true;
      }
    } else {
      adv(l);
    }
  }
  return false;
}

// After `///*` was consumed: consumes up to and including `*///`.
static bool finish_doc_block_comment(TSLexer *l) {
  unsigned state = 0;  // how many characters of `*///` were seen
  while (!l->eof(l)) {
    int32_t c = l->lookahead;
    adv(l);
    if (c == '*') {
      state = 1;
    } else if (c == '/' && state >= 1) {
      state++;
      if (state == 4) return true;
    } else {
      state = 0;
    }
  }
  return false;
}

// For look-ahead only: the first `/` was consumed. Skips a comment when one follows.
// Returns true if a comment was skipped, false if the `/` stands alone.
static bool skip_comment_after_slash(TSLexer *l) {
  if (l->lookahead == '*') {
    adv(l);
    finish_block_comment(l);
    return true;
  }
  if (l->lookahead == '/') {
    adv(l);
    if (l->lookahead == '/') {
      adv(l);
      if (l->lookahead == '*') {
        adv(l);
        finish_doc_block_comment(l);
        return true;
      }
    }
    while (!l->eof(l) && l->lookahead != '\n') adv(l);
    return true;
  }
  return false;
}


// ---- Look-ahead over bracketed groups (closures and builders) --------------------------------------

#define GROUP_BUDGET 200000

static bool skip_group(TSLexer *l, int32_t close, bool *has_of, unsigned *budget);

// After the opening `"` of a string: consumes up to and including the closing `"`.
static void skip_string(TSLexer *l, unsigned *budget) {
  while (!l->eof(l) && *budget > 0) {
    int32_t c = l->lookahead;
    (*budget)--;
    if (c == '\\') {
      adv(l);
      if (!l->eof(l)) adv(l);
    } else if (c == '"') {
      adv(l);
      return;
    } else if (c == '#') {
      adv(l);
      if (l->lookahead == '(') {
        adv(l);
        skip_group(l, ')', NULL, budget);
      }
    } else {
      adv(l);
    }
  }
}

// After the opening `'` of a character literal.
static void skip_char(TSLexer *l, unsigned *budget) {
  while (!l->eof(l) && *budget > 0 && l->lookahead != '\n') {
    int32_t c = l->lookahead;
    (*budget)--;
    adv(l);
    if (c == '\\') {
      if (!l->eof(l)) adv(l);
    } else if (c == '\'') {
      return;
    }
  }
}

// The opening bracket was consumed. Consumes up to and including the matching `close`.
// Sets `*has_of` when the word `of` stands directly inside this group (a builder).
// Returns false when the group is not closed properly.
static bool skip_group(TSLexer *l, int32_t close, bool *has_of, unsigned *budget) {
  int32_t stack[96];
  unsigned sp = 0;
  stack[sp++] = close;
  while (!l->eof(l) && *budget > 0) {
    int32_t c = l->lookahead;
    (*budget)--;
    switch (c) {
      case '(':
      case '[':
      case '{':
        if (sp >= sizeof(stack) / sizeof(stack[0])) return false;
        stack[sp++] = c == '(' ? ')' : c == '[' ? ']' : '}';
        adv(l);
        break;
      case ')':
      case ']':
      case '}':
        if (c != stack[sp - 1]) return false;
        adv(l);
        if (--sp == 0) return true;
        break;
      case '"':
        adv(l);
        skip_string(l, budget);
        break;
      case '\'':
        adv(l);
        skip_char(l, budget);
        break;
      case '/':
        adv(l);
        if (l->lookahead == '*') {
          adv(l);
          finish_block_comment(l);
        } else if (l->lookahead == '/') {
          while (!l->eof(l) && l->lookahead != '\n') adv(l);
        }
        break;
      default:
        if (is_ident_char(c)) {
          char buf[WORD_MAX];
          unsigned len;
          read_word(l, buf, &len);
          if (sp == 1 && has_of != NULL && word_is(buf, len, "of")) *has_of = true;
        } else {
          adv(l);
        }
    }
  }
  return false;
}

// After `)`: is a `~>` next, or `-> Type ~>`? Blanks are skipped, newlines are not.
static bool closure_tail(TSLexer *l, unsigned *budget) {
  skip_blanks(l);
  if (l->lookahead == '~') {
    adv(l);
    return l->lookahead == '>';
  }
  if (l->lookahead != '-') return false;
  adv(l);
  if (l->lookahead != '>') return false;
  adv(l);
  unsigned angle = 0;
  for (unsigned steps = 0; steps < 256 && !l->eof(l); steps++) {
    int32_t c = l->lookahead;
    switch (c) {
      case '~':
        adv(l);
        return l->lookahead == '>';
      case '<':
        angle++;
        adv(l);
        break;
      case '>':
        if (angle > 0) angle--;
        adv(l);
        break;
      case ',':
        if (angle == 0) return false;
        adv(l);
        break;
      case '\n':
      case '{':
      case '}':
      case ')':
      case ']':
      case ';':
      case '=':
        return false;
      case '(':
        adv(l);
        if (!skip_group(l, ')', NULL, budget)) return false;
        break;
      case '[':
        adv(l);
        if (!skip_group(l, ']', NULL, budget)) return false;
        break;
      default:
        adv(l);
    }
  }
  return false;
}

// After `]`: closure parameters and `~>`: `~>`, `x ~>`, or `(params) ~>`.
static bool capture_tail(TSLexer *l, unsigned *budget) {
  skip_blanks(l);
  int32_t c = l->lookahead;
  if (c == '~') return closure_tail(l, budget);
  if (is_ident_start(c)) {
    char buf[WORD_MAX];
    unsigned len;
    read_word(l, buf, &len);
    skip_blanks(l);
    if (l->lookahead != '~') return false;
    adv(l);
    return l->lookahead == '>';
  }
  if (c == '(') {
    adv(l);
    if (!skip_group(l, ')', NULL, budget)) return false;
    return closure_tail(l, budget);
  }
  return false;
}

// The lookahead is `(`, `[` or `{` (blanks already skipped). Decides which of the bracket tokens it is.
static bool scan_open_bracket(TSLexer *l, const bool *valid) {
  int32_t c = l->lookahead;
  unsigned budget = GROUP_BUDGET;
  bool has_of = false;
  if (c == '(') {
    bool want_closure = valid[CLOSURE_LPAREN];
    bool want_builder = valid[BUILDER_LPAREN];
    if (!want_closure && !want_builder) return false;
    adv(l);
    l->mark_end(l);
    if (!skip_group(l, ')', &has_of, &budget)) return false;
    if (want_builder && has_of) {
      l->result_symbol = BUILDER_LPAREN;
      return true;
    }
    if (want_closure && closure_tail(l, &budget)) {
      l->result_symbol = CLOSURE_LPAREN;
      return true;
    }
    return false;
  }
  if (c == '[') {
    bool want_closure = valid[CLOSURE_LBRACKET];
    bool want_builder = valid[BUILDER_LBRACKET];
    if (!want_closure && !want_builder) return false;
    adv(l);
    l->mark_end(l);
    if (!skip_group(l, ']', &has_of, &budget)) return false;
    if (want_closure && capture_tail(l, &budget)) {
      l->result_symbol = CLOSURE_LBRACKET;
      return true;
    }
    if (want_builder && has_of) {
      l->result_symbol = BUILDER_LBRACKET;
      return true;
    }
    return false;
  }
  if (c == '{') {
    if (valid[BUILDER_LBRACE]) {
      adv(l);
      l->mark_end(l);
      if (!skip_group(l, '}', &has_of, &budget) || !has_of) return false;
      l->result_symbol = BUILDER_LBRACE;
      return true;
    }
    // A `{` after a condition is a block; after anything else that can end a name it may be a
    // struct literal. When both are possible the block wins (struct literals are not allowed
    // in conditions).
    if (valid[BLOCK_LBRACE]) {
      adv(l);
      l->mark_end(l);
      l->result_symbol = BLOCK_LBRACE;
      return true;
    }
    if (valid[STRUCT_LBRACE]) {
      adv(l);
      l->mark_end(l);
      l->result_symbol = STRUCT_LBRACE;
      return true;
    }
  }
  return false;
}

// ---- Newlines ---------------------------------------------------------------------------------

// The lookahead is a '\n'. Decides whether it ends a statement. Returns true to emit NEWLINE.
static bool scan_newline(TSLexer *l) {
  l->result_symbol = NEWLINE;
  skip_space_and_newlines(l);
  l->mark_end(l);
  for (;;) {
    int32_t c = l->lookahead;
    if (l->eof(l)) return true;
    switch (c) {
      case '/':
        adv(l);
        if (skip_comment_after_slash(l)) {
          skip_space_and_newlines(l);
          continue;
        }
        return true;
      case '.':  // `.field` continues the line; `..` and `...` do not
        adv(l);
        return l->lookahead == '.';
      case '?':  // `?.`, `??` and a ternary `?` continue; `??=` does not
        adv(l);
        if (l->lookahead == '?') {
          adv(l);
          return l->lookahead == '=';
        }
        return false;
      case '|':  // `|>` and `||`
        adv(l);
        return !(l->lookahead == '>' || l->lookahead == '|');
      case '&':  // `&&`
        adv(l);
        return l->lookahead != '&';
      case ':':  // the `:` of a ternary written on its own line
        return false;
      case 'e': {  // `else`
        char buf[WORD_MAX];
        unsigned len;
        read_word(l, buf, &len);
        return !word_is(buf, len, "else");
      }
      default:
        return true;
    }
  }
}

// ---- Look-ahead helpers for contextual keywords ----------------------------------------------------

// Is the word at the lookahead a word that can start a declaration (after a modifier)?
// `allow_quote`: a string literal also counts (`ext "C" fn`).
static bool declaration_follows(TSLexer *l, bool allow_quote) {
  skip_space_and_newlines(l);
  if (l->lookahead == '"') return allow_quote;
  if (!is_ident_start(l->lookahead)) return false;
  char buf[WORD_MAX];
  unsigned len;
  read_word(l, buf, &len);
  static const char *const words[] = {"fn",     "struct",   "enum",  "interface", "impl",     "obj",
                                      "const",  "var",      "meta",  "unsafe",    "async",    "pub",
                                      "priv",   "deinit",   "init",  "open",      "abstract", "final",
                                      "override", "linear", "static", "ext"};
  for (unsigned i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
    if (word_is(buf, len, words[i])) return true;
  }
  return false;
}

// Called after a contextual word was read and `mark_end` set. Returns true when the keyword
// reading applies. `valid` tells which tokens the parser can use here.
static bool scan_word(TSLexer *l, const bool *valid) {
  char buf[WORD_MAX];
  unsigned len;
  read_word(l, buf, &len);
  if (len == 0 || len >= WORD_MAX) return false;
  l->mark_end(l);

  if (word_is(buf, len, "map") || word_is(buf, len, "set") || word_is(buf, len, "select")) {
    enum TokenType t = word_is(buf, len, "map")   ? MAP_KW
                       : word_is(buf, len, "set") ? SET_KW
                                                  : SELECT_KW;
    if (!valid[t]) return false;
    skip_blanks(l);
    if (l->lookahead != '{') return false;
    l->result_symbol = t;
    return true;
  }
  if (word_is(buf, len, "soa")) {
    if (!valid[SOA_KW]) return false;
    skip_blanks(l);
    if (l->lookahead != '[') return false;
    l->result_symbol = SOA_KW;
    return true;
  }
  if (word_is(buf, len, "scope")) {
    if (!valid[SCOPE_KW]) return false;
    skip_blanks(l);
    if (!is_ident_start(l->lookahead)) return false;
    char b2[WORD_MAX];
    unsigned n2;
    read_word(l, b2, &n2);
    skip_blanks(l);
    if (l->lookahead != '{') return false;
    l->result_symbol = SCOPE_KW;
    return true;
  }
  if (word_is(buf, len, "type")) {
    if (!valid[TYPE_KW]) return false;
    if (!is_blank(l->lookahead)) return false;
    skip_blanks(l);
    if (!is_ident_start(l->lookahead)) return false;
    char b2[WORD_MAX];
    unsigned n2;
    read_word(l, b2, &n2);
    skip_blanks(l);
    if (l->lookahead != '=' && l->lookahead != '<') return false;
    l->result_symbol = TYPE_KW;
    return true;
  }
  if (word_is(buf, len, "test")) {
    if (!valid[TEST_KW]) return false;
    skip_blanks(l);
    if (l->lookahead != '"') return false;
    l->result_symbol = TEST_KW;
    return true;
  }
  if (word_is(buf, len, "init")) {
    if (!valid[INIT_KW]) return false;
    skip_blanks(l);
    if (l->lookahead != '(') return false;
    l->result_symbol = INIT_KW;
    return true;
  }
  if (word_is(buf, len, "default")) {
    if (!valid[DEFAULT_SWITCH] && !valid[DEFAULT_SELECT]) return false;
    skip_blanks(l);
    if (l->lookahead == ':' && valid[DEFAULT_SWITCH]) {
      l->result_symbol = DEFAULT_SWITCH;
      return true;
    }
    if (l->lookahead == '{' && valid[DEFAULT_SELECT]) {
      l->result_symbol = DEFAULT_SELECT;
      return true;
    }
    return false;
  }
  if (word_is(buf, len, "ext")) {
    if (!valid[EXT_KW]) return false;
    if (!declaration_follows(l, true)) return false;
    l->result_symbol = EXT_KW;
    return true;
  }
  if (word_is(buf, len, "open") || word_is(buf, len, "abstract") || word_is(buf, len, "final") ||
      word_is(buf, len, "override") || word_is(buf, len, "linear") || word_is(buf, len, "static")) {
    if (!valid[MODIFIER_KW]) return false;
    if (!declaration_follows(l, false)) return false;
    l->result_symbol = MODIFIER_KW;
    return true;
  }
  return false;
}

// `<` right after a name in an expression: only generic arguments when `>` closes them soon and
// a `(`, `{` or `.` follows, as in `max<int>(3, 7)`. The `<` was consumed.
static bool generic_args_follow(TSLexer *l) {
  unsigned depth = 1;
  for (;;) {
    int32_t c = l->lookahead;
    if (l->eof(l)) return false;
    if (c == '<') {
      depth++;
    } else if (c == '>') {
      depth--;
      if (depth == 0) {
        adv(l);
        skip_blanks(l);
        return l->lookahead == '(' || l->lookahead == '{' || l->lookahead == '.';
      }
    } else if (!(is_ident_char(c) || c == ' ' || c == '\t' || c == ',' || c == '.' || c == '[' ||
                 c == ']' || c == '(' || c == ')' || c == ';' || c == '?' || c == '!' || c == '-')) {
      return false;
    }
    adv(l);
  }
}

bool tree_sitter_pelago_external_scanner_scan(void *payload, TSLexer *l, const bool *valid) {
  (void)payload;
  int32_t c = l->lookahead;

  // Tokens that must touch the token before them: nothing has been skipped yet.
  if (c == '<' && (valid[GENERIC_LT] || valid[GENERIC_LT_EXPR])) {
    adv(l);
    if (l->lookahead == '=' || l->lookahead == '<') return false;
    l->mark_end(l);
    if (valid[GENERIC_LT]) {
      l->result_symbol = GENERIC_LT;
      return true;
    }
    if (generic_args_follow(l)) {
      l->result_symbol = GENERIC_LT_EXPR;
      return true;
    }
    return false;
  }
  if (c == '[' && valid[INDEX_LBRACKET]) {
    adv(l);
    l->mark_end(l);
    l->result_symbol = INDEX_LBRACKET;
    return true;
  }
  if (c == '?' && valid[QUESTION_ADJACENT]) {
    adv(l);
    if (l->lookahead == '?' || l->lookahead == '.') return false;
    l->mark_end(l);
    l->result_symbol = QUESTION_ADJACENT;
    return true;
  }
  if (c == '!' && valid[CALL_BANG]) {
    adv(l);
    if (l->lookahead == '!') adv(l);
    l->mark_end(l);
    if (l->lookahead != '(') return false;
    l->result_symbol = CALL_BANG;
    return true;
  }

  // Everything below may skip blanks first (they do not belong to the token).
  while (is_blank(l->lookahead)) skip(l);
  c = l->lookahead;

  if (c == '\n') {
    if (valid[NEWLINE]) return scan_newline(l);
    while (is_blank(l->lookahead) || l->lookahead == '\n') skip(l);
    c = l->lookahead;
  }

  if (c == '(' || c == '[' || c == '{') return scan_open_bracket(l, valid);

  if (c == '/') {
    adv(l);
    if (l->lookahead == '*') {
      adv(l);
      if (!finish_block_comment(l)) return false;
      l->mark_end(l);
      l->result_symbol = BLOCK_COMMENT;
      return true;
    }
    if (l->lookahead == '/') {
      adv(l);
      if (l->lookahead == '/') {
        adv(l);
        if (l->lookahead == '*') {
          adv(l);
          if (!finish_doc_block_comment(l)) return false;
          l->mark_end(l);
          l->result_symbol = DOC_BLOCK_COMMENT;
          return true;
        }
      }
    }
    return false;
  }

  if (is_ident_start(c)) return scan_word(l, valid);
  return false;
}
