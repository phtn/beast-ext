#include "tree_sitter/parser.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Beast external scanner: indentation-sensitive, Python/YAML-style.
// Manages indentation tokens and embedded JS/TS expression boundaries declared
// in grammar.js.
// Extras in grammar = /[ \t]/ (spaces/tabs are skipped). Newlines and
// indentation are *not* extras — we emit them here.
//
// Token types must match order in `externals` in grammar.js:
enum TokenType {
  NEWLINE,
  INDENT,
  DEDENT,
  ERROR_SENTINEL,
  EXPRESSION_CONTENT,
  EACH_ITERABLE,
};

#define MAX_INDENTS 64
#define TAB_WIDTH 2

typedef struct {
  int16_t indents[MAX_INDENTS];
  uint16_t len;
} Scanner;

void *tree_sitter_beast_external_scanner_create(void) {
  Scanner *scanner = (Scanner *)calloc(1, sizeof(Scanner));
  return scanner;
}

void tree_sitter_beast_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_beast_external_scanner_serialize(void *payload,
                                                      char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  size_t size = 0;
  buffer[size++] = (char)scanner->len;
  for (uint16_t i = 0; i < scanner->len; i++) {
    // store as big-endian 16-bit
    buffer[size++] = (char)((scanner->indents[i] >> 8) & 0xFF);
    buffer[size++] = (char)(scanner->indents[i] & 0xFF);
  }
  return size;
}

void tree_sitter_beast_external_scanner_deserialize(void *payload,
                                                    const char *buffer,
                                                    unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  scanner->len = 1;
  scanner->indents[0] = 0;
  if (length == 0)
    return;
  size_t size = 0;
  uint16_t serialized_len = (uint8_t)buffer[size++];
  if (serialized_len > MAX_INDENTS)
    serialized_len = MAX_INDENTS;
  scanner->len = 0;
  for (uint16_t i = 0; i < serialized_len && size + 1 < length; i++) {
    int16_t hi = (int16_t)((uint8_t)buffer[size++]);
    int16_t lo = (int16_t)((uint8_t)buffer[size++]);
    scanner->indents[scanner->len++] = (int16_t)((hi << 8) | lo);
  }
  if (scanner->len == 0 || scanner->indents[0] != 0) {
    scanner->len = 1;
    scanner->indents[0] = 0;
  }
}

static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }
static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

typedef enum {
  JS_NORMAL,
  JS_SINGLE_QUOTE,
  JS_DOUBLE_QUOTE,
  JS_TEMPLATE,
  JS_LINE_COMMENT,
  JS_BLOCK_COMMENT,
  JS_REGEX,
  JS_REGEX_CHARACTER_CLASS,
} JsMode;

static bool is_identifier_start(int32_t c) {
  return c == '_' || c == '$' || (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

static bool is_identifier_continue(int32_t c) {
  return is_identifier_start(c) || (c >= '0' && c <= '9');
}

static bool keyword_allows_regex(const char *word) {
  static const char *const keywords[] = {
      "await", "case",   "delete", "in",     "instanceof", "new",
      "of",    "return", "throw",  "typeof", "void",       "yield",
  };
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if (strcmp(word, keywords[i]) == 0)
      return true;
  }
  return false;
}

static bool scan_expression_content(TSLexer *lexer) {
  if (lexer->lookahead == '}' || lexer->eof(lexer))
    return false;

  JsMode mode = JS_NORMAL;
  int32_t brace_depth = 0;
  int32_t template_depths[MAX_INDENTS];
  uint16_t template_depth_count = 0;
  bool can_start_regex = true;

  while (!lexer->eof(lexer)) {
    int32_t c = lexer->lookahead;

    if (mode == JS_SINGLE_QUOTE || mode == JS_DOUBLE_QUOTE) {
      int32_t quote = mode == JS_SINGLE_QUOTE ? '\'' : '"';
      if (c == '\\') {
        advance(lexer);
        if (!lexer->eof(lexer))
          advance(lexer);
      } else {
        advance(lexer);
        if (c == quote) {
          mode = JS_NORMAL;
          can_start_regex = false;
        }
      }
      continue;
    }

    if (mode == JS_TEMPLATE) {
      if (c == '\\') {
        advance(lexer);
        if (!lexer->eof(lexer))
          advance(lexer);
      } else if (c == '`') {
        advance(lexer);
        mode = JS_NORMAL;
        can_start_regex = false;
      } else if (c == '$') {
        advance(lexer);
        if (lexer->lookahead == '{') {
          advance(lexer);
          if (template_depth_count < MAX_INDENTS) {
            template_depths[template_depth_count++] = brace_depth;
          }
          brace_depth++;
          mode = JS_NORMAL;
          can_start_regex = true;
        }
      } else {
        advance(lexer);
      }
      continue;
    }

    if (mode == JS_LINE_COMMENT) {
      advance(lexer);
      if (c == '\n' || c == '\r')
        mode = JS_NORMAL;
      continue;
    }

    if (mode == JS_BLOCK_COMMENT) {
      if (c == '*') {
        advance(lexer);
        if (lexer->lookahead == '/') {
          advance(lexer);
          mode = JS_NORMAL;
        }
      } else {
        advance(lexer);
      }
      continue;
    }

    if (mode == JS_REGEX || mode == JS_REGEX_CHARACTER_CLASS) {
      if (c == '\\') {
        advance(lexer);
        if (!lexer->eof(lexer))
          advance(lexer);
      } else if (mode == JS_REGEX && c == '[') {
        advance(lexer);
        mode = JS_REGEX_CHARACTER_CLASS;
      } else if (mode == JS_REGEX_CHARACTER_CLASS && c == ']') {
        advance(lexer);
        mode = JS_REGEX;
      } else if (mode == JS_REGEX && c == '/') {
        advance(lexer);
        while (is_identifier_continue(lexer->lookahead))
          advance(lexer);
        mode = JS_NORMAL;
        can_start_regex = false;
      } else {
        advance(lexer);
      }
      continue;
    }

    if (c == '}' && brace_depth == 0) {
      lexer->mark_end(lexer);
      lexer->result_symbol = EXPRESSION_CONTENT;
      return true;
    }

    if (c == '\'' || c == '"') {
      mode = c == '\'' ? JS_SINGLE_QUOTE : JS_DOUBLE_QUOTE;
      advance(lexer);
      continue;
    }

    if (c == '`') {
      mode = JS_TEMPLATE;
      advance(lexer);
      continue;
    }

    if (c == '/') {
      advance(lexer);
      if (lexer->lookahead == '/') {
        advance(lexer);
        mode = JS_LINE_COMMENT;
      } else if (lexer->lookahead == '*') {
        advance(lexer);
        mode = JS_BLOCK_COMMENT;
      } else if (can_start_regex) {
        mode = JS_REGEX;
      } else {
        can_start_regex = true;
      }
      continue;
    }

    if (is_identifier_start(c)) {
      char word[16];
      size_t word_len = 0;
      while (is_identifier_continue(lexer->lookahead)) {
        if (word_len + 1 < sizeof(word))
          word[word_len++] = (char)lexer->lookahead;
        advance(lexer);
      }
      word[word_len] = '\0';
      can_start_regex = keyword_allows_regex(word);
      continue;
    }

    if (c == '{') {
      brace_depth++;
      can_start_regex = true;
      advance(lexer);
      continue;
    }

    if (c == '}') {
      advance(lexer);
      brace_depth--;
      can_start_regex = false;
      if (template_depth_count > 0 &&
          brace_depth == template_depths[template_depth_count - 1]) {
        template_depth_count--;
        mode = JS_TEMPLATE;
      }
      continue;
    }

    if (c == ')' || c == ']' || c == '.') {
      can_start_regex = false;
    } else if (c == '(' || c == '[' || c == ',' || c == ':' || c == ';' ||
               c == '?' || c == '=' || c == '!' || c == '&' || c == '|' ||
               c == '+' || c == '-' || c == '*' || c == '%' || c == '^' ||
               c == '~' || c == '<' || c == '>') {
      can_start_regex = true;
    } else if (c >= '0' && c <= '9') {
      can_start_regex = false;
    }

    advance(lexer);
  }

  lexer->mark_end(lexer);
  lexer->result_symbol = EXPRESSION_CONTENT;
  return true;
}

// Scan the iterable portion of `each ... in iterable [key expression]`.
// Beast recognizes ` key ` only at the top level, outside quoted strings and
// balanced delimiters. Stop the token before that separator so `key` remains
// visible to the grammar and can receive its own expression injection.
static bool scan_each_iterable(TSLexer *lexer) {
  if (lexer->eof(lexer) || lexer->lookahead == '\n' ||
      lexer->lookahead == '\r')
    return false;

  uint32_t delimiter_depth = 0;
  int32_t quote = 0;
  bool escaped = false;
  bool has_content = false;

  while (!lexer->eof(lexer) && lexer->lookahead != '\n' &&
         lexer->lookahead != '\r') {
    int32_t c = lexer->lookahead;

    if (quote != 0) {
      advance(lexer);
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == quote) {
        quote = 0;
      }
      has_content = true;
      lexer->mark_end(lexer);
      continue;
    }

    if (c == '\'' || c == '"' || c == '`') {
      quote = c;
      has_content = true;
      advance(lexer);
      lexer->mark_end(lexer);
      continue;
    }

    if (c == '(' || c == '{' || c == '[') {
      delimiter_depth++;
    } else if (c == ')' || c == '}' || c == ']') {
      if (delimiter_depth > 0)
        delimiter_depth--;
    } else if (c == ' ' && delimiter_depth == 0) {
      uint8_t matched_key_characters = 0;
      lexer->mark_end(lexer);
      advance(lexer);
      if (lexer->lookahead == 'k') {
        matched_key_characters++;
        advance(lexer);
        if (lexer->lookahead == 'e') {
          matched_key_characters++;
          advance(lexer);
          if (lexer->lookahead == 'y') {
            matched_key_characters++;
            advance(lexer);
            if (lexer->lookahead == ' ' && has_content) {
              lexer->result_symbol = EACH_ITERABLE;
              return true;
            }
          }
        }
      }
      if (matched_key_characters > 0) {
        has_content = true;
        lexer->mark_end(lexer);
      }
      continue;
    }

    advance(lexer);
    if (c != ' ' && c != '\t') {
      has_content = true;
      lexer->mark_end(lexer);
    }
  }

  if (!has_content)
    return false;
  lexer->result_symbol = EACH_ITERABLE;
  return true;
}

// Count indentation columns using the same two-column tab width configured for
// Beast in Zed.
static bool scan(TSLexer *lexer, const bool *valid_symbols, Scanner *scanner) {
  lexer->mark_end(lexer);

  bool found_end_of_line = false;
  uint16_t indent = 0;

  for (;;) {
    if (lexer->lookahead == '\n') {
      found_end_of_line = true;
      indent = 0;
      skip(lexer);
    } else if (lexer->lookahead == '\r') {
      found_end_of_line = true;
      indent = 0;
      skip(lexer);
    } else if (lexer->lookahead == ' ') {
      indent++;
      skip(lexer);
    } else if (lexer->lookahead == '\t') {
      indent += (uint16_t)(TAB_WIDTH - (indent % TAB_WIDTH));
      skip(lexer);
    } else if (lexer->eof(lexer)) {
      found_end_of_line = true;
      indent = 0;
      break;
    } else {
      break;
    }
  }

  if (!found_end_of_line)
    return false;

  int16_t current_indent = scanner->indents[scanner->len - 1];

  if (valid_symbols[INDENT] && indent > (uint16_t)current_indent) {
    if (scanner->len == MAX_INDENTS)
      return false;
    scanner->indents[scanner->len++] = (int16_t)indent;
    lexer->mark_end(lexer);
    lexer->result_symbol = INDENT;
    return true;
  }

  if (valid_symbols[DEDENT] && indent < (uint16_t)current_indent) {
    scanner->len--;
    if (indent >= (uint16_t)scanner->indents[scanner->len - 1]) {
      lexer->mark_end(lexer);
    }
    lexer->result_symbol = DEDENT;
    return true;
  }

  if (valid_symbols[NEWLINE]) {
    if (indent == (uint16_t)current_indent)
      lexer->mark_end(lexer);
    lexer->result_symbol = NEWLINE;
    return true;
  }

  return false;
}

bool tree_sitter_beast_external_scanner_scan(void *payload, TSLexer *lexer,
                                             const bool *valid_symbols) {
  Scanner *scanner = (Scanner *)payload;
  // Initialize stack with 0 base if empty
  if (scanner->len == 0) {
    scanner->indents[0] = 0;
    scanner->len = 1;
  }
  if (valid_symbols[EACH_ITERABLE] && !valid_symbols[ERROR_SENTINEL] &&
      scan_each_iterable(lexer)) {
    return true;
  }
  if (valid_symbols[EXPRESSION_CONTENT] && !valid_symbols[ERROR_SENTINEL] &&
      scan_expression_content(lexer)) {
    return true;
  }
  return scan(lexer, valid_symbols, scanner);
}
