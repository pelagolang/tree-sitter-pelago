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
