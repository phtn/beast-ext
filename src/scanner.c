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
  EACH_ITERABLE_CONTINUATION,
  CONTINUATION,
};

#define MAX_INDENTS 64
#define TAB_WIDTH 2

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

typedef struct {
  int16_t indents[MAX_INDENTS];
  uint16_t len;
  bool expression_active;
  JsMode expression_mode;
  int16_t expression_brace_depth;
  int16_t expression_template_depths[MAX_INDENTS];
  uint16_t expression_template_depth_count;
  bool expression_can_start_regex;
} Scanner;

static void reset_expression(Scanner *scanner) {
  scanner->expression_active = false;
  scanner->expression_mode = JS_NORMAL;
  scanner->expression_brace_depth = 0;
  scanner->expression_template_depth_count = 0;
  scanner->expression_can_start_regex = true;
}

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
  buffer[size++] = (char)scanner->expression_active;
  if (scanner->expression_active) {
    buffer[size++] = (char)scanner->expression_mode;
    buffer[size++] = (char)((scanner->expression_brace_depth >> 8) & 0xFF);
    buffer[size++] = (char)(scanner->expression_brace_depth & 0xFF);
    buffer[size++] = (char)scanner->expression_template_depth_count;
    for (uint16_t i = 0; i < scanner->expression_template_depth_count; i++) {
      buffer[size++] =
          (char)((scanner->expression_template_depths[i] >> 8) & 0xFF);
      buffer[size++] = (char)(scanner->expression_template_depths[i] & 0xFF);
    }
    buffer[size++] = (char)scanner->expression_can_start_regex;
  }
  return size;
}

void tree_sitter_beast_external_scanner_deserialize(void *payload,
                                                    const char *buffer,
                                                    unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  scanner->len = 1;
  scanner->indents[0] = 0;
  reset_expression(scanner);
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
  if (size >= length || buffer[size++] == 0)
    return;
  if (size + 3 >= length)
    return;
  scanner->expression_active = true;
  scanner->expression_mode = (JsMode)(uint8_t)buffer[size++];
  int16_t brace_hi = (int16_t)(uint8_t)buffer[size++];
  int16_t brace_lo = (int16_t)(uint8_t)buffer[size++];
  scanner->expression_brace_depth =
      (int16_t)((brace_hi << 8) | brace_lo);
  uint16_t depth_count = (uint8_t)buffer[size++];
  if (depth_count > MAX_INDENTS)
    depth_count = MAX_INDENTS;
  scanner->expression_template_depth_count = 0;
  for (uint16_t i = 0; i < depth_count && size + 1 < length; i++) {
    int16_t hi = (int16_t)(uint8_t)buffer[size++];
    int16_t lo = (int16_t)(uint8_t)buffer[size++];
    scanner->expression_template_depths
        [scanner->expression_template_depth_count++] =
        (int16_t)((hi << 8) | lo);
  }
  if (size < length)
    scanner->expression_can_start_regex = buffer[size] != 0;
}

static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }
static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

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

static bool scan_expression_content(TSLexer *lexer, Scanner *scanner) {
  if (!scanner->expression_active) {
    reset_expression(scanner);
    scanner->expression_active = true;
  }

  bool has_content = false;

  while (!lexer->eof(lexer)) {
    int32_t c = lexer->lookahead;

    // A continuation is a token boundary even inside strings, templates, and
    // comments because Beast joins physical lines before TypeScript sees them.
    if (c == '\n' || c == '\r') {
      lexer->mark_end(lexer);
      if (c == '\r') {
        advance(lexer);
        if (lexer->lookahead == '\n')
          advance(lexer);
      } else {
        advance(lexer);
      }
      while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
        advance(lexer);
      if (lexer->lookahead == '~') {
        if (!has_content)
          return false;
        lexer->result_symbol = EXPRESSION_CONTENT;
        return true;
      }
      if (scanner->expression_mode == JS_LINE_COMMENT)
        scanner->expression_mode = JS_NORMAL;
      has_content = true;
      continue;
    }

    if (scanner->expression_mode == JS_SINGLE_QUOTE ||
        scanner->expression_mode == JS_DOUBLE_QUOTE) {
      int32_t quote = scanner->expression_mode == JS_SINGLE_QUOTE ? '\'' : '"';
      if (c == '\\') {
        advance(lexer);
        if (!lexer->eof(lexer))
          advance(lexer);
      } else {
        advance(lexer);
        if (c == quote) {
          scanner->expression_mode = JS_NORMAL;
          scanner->expression_can_start_regex = false;
        }
      }
      has_content = true;
      continue;
    }

    if (scanner->expression_mode == JS_TEMPLATE) {
      if (c == '\\') {
        advance(lexer);
        if (!lexer->eof(lexer))
          advance(lexer);
      } else if (c == '`') {
        advance(lexer);
        scanner->expression_mode = JS_NORMAL;
        scanner->expression_can_start_regex = false;
      } else if (c == '$') {
        advance(lexer);
        if (lexer->lookahead == '{') {
          advance(lexer);
          if (scanner->expression_template_depth_count < MAX_INDENTS) {
            scanner->expression_template_depths
                [scanner->expression_template_depth_count++] =
                scanner->expression_brace_depth;
          }
          scanner->expression_brace_depth++;
          scanner->expression_mode = JS_NORMAL;
          scanner->expression_can_start_regex = true;
        }
      } else {
        advance(lexer);
      }
      has_content = true;
      continue;
    }

    if (scanner->expression_mode == JS_LINE_COMMENT) {
      advance(lexer);
      has_content = true;
      continue;
    }

    if (scanner->expression_mode == JS_BLOCK_COMMENT) {
      if (c == '*') {
        advance(lexer);
        if (lexer->lookahead == '/') {
          advance(lexer);
          scanner->expression_mode = JS_NORMAL;
        }
      } else {
        advance(lexer);
      }
      has_content = true;
      continue;
    }

    if (scanner->expression_mode == JS_REGEX ||
        scanner->expression_mode == JS_REGEX_CHARACTER_CLASS) {
      if (c == '\\') {
        advance(lexer);
        if (!lexer->eof(lexer))
          advance(lexer);
      } else if (scanner->expression_mode == JS_REGEX && c == '[') {
        advance(lexer);
        scanner->expression_mode = JS_REGEX_CHARACTER_CLASS;
      } else if (scanner->expression_mode == JS_REGEX_CHARACTER_CLASS &&
                 c == ']') {
        advance(lexer);
        scanner->expression_mode = JS_REGEX;
      } else if (scanner->expression_mode == JS_REGEX && c == '/') {
        advance(lexer);
        while (is_identifier_continue(lexer->lookahead))
          advance(lexer);
        scanner->expression_mode = JS_NORMAL;
        scanner->expression_can_start_regex = false;
      } else {
        advance(lexer);
      }
      has_content = true;
      continue;
    }

    if (c == '}' && scanner->expression_brace_depth == 0) {
      if (!has_content) {
        reset_expression(scanner);
        return false;
      }
      lexer->mark_end(lexer);
      lexer->result_symbol = EXPRESSION_CONTENT;
      reset_expression(scanner);
      return true;
    }

    if (c == '\'' || c == '"') {
      scanner->expression_mode =
          c == '\'' ? JS_SINGLE_QUOTE : JS_DOUBLE_QUOTE;
      advance(lexer);
      has_content = true;
      continue;
    }

    if (c == '`') {
      scanner->expression_mode = JS_TEMPLATE;
      advance(lexer);
      has_content = true;
      continue;
    }

    if (c == '/') {
      advance(lexer);
      if (lexer->lookahead == '/') {
        advance(lexer);
        scanner->expression_mode = JS_LINE_COMMENT;
      } else if (lexer->lookahead == '*') {
        advance(lexer);
        scanner->expression_mode = JS_BLOCK_COMMENT;
      } else if (scanner->expression_can_start_regex) {
        scanner->expression_mode = JS_REGEX;
      } else {
        scanner->expression_can_start_regex = true;
      }
      has_content = true;
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
      scanner->expression_can_start_regex = keyword_allows_regex(word);
      has_content = true;
      continue;
    }

    if (c == '{') {
      scanner->expression_brace_depth++;
      scanner->expression_can_start_regex = true;
      advance(lexer);
      has_content = true;
      continue;
    }

    if (c == '}') {
      advance(lexer);
      scanner->expression_brace_depth--;
      scanner->expression_can_start_regex = false;
      if (scanner->expression_template_depth_count > 0 &&
          scanner->expression_brace_depth ==
              scanner->expression_template_depths
                  [scanner->expression_template_depth_count - 1]) {
        scanner->expression_template_depth_count--;
        scanner->expression_mode = JS_TEMPLATE;
      }
      has_content = true;
      continue;
    }

    if (c == ')' || c == ']' || c == '.') {
      scanner->expression_can_start_regex = false;
    } else if (c == '(' || c == '[' || c == ',' || c == ':' || c == ';' ||
               c == '?' || c == '=' || c == '!' || c == '&' || c == '|' ||
               c == '+' || c == '-' || c == '*' || c == '%' || c == '^' ||
               c == '~' || c == '<' || c == '>') {
      scanner->expression_can_start_regex = true;
    } else if (c >= '0' && c <= '9') {
      scanner->expression_can_start_regex = false;
    }

    advance(lexer);
    has_content = true;
  }

  if (!has_content) {
    reset_expression(scanner);
    return false;
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = EXPRESSION_CONTENT;
  reset_expression(scanner);
  return true;
}

// Scan the iterable portion of `each ... in iterable [key expression]`.
// Beast recognizes ` key ` only at the top level, outside quoted strings and
// balanced delimiters. Stop the token before that separator so `key` remains
// visible to the grammar and can receive its own expression injection.
static bool scan_each_iterable(TSLexer *lexer, bool reject_leading_key,
                               bool *rejected_leading_key) {
  *rejected_leading_key = false;
  if (lexer->eof(lexer) || lexer->lookahead == '\n' ||
      lexer->lookahead == '\r')
    return false;

  uint32_t delimiter_depth = 0;
  int32_t quote = 0;
  bool escaped = false;
  bool has_content = false;

  // After at least one iterable fragment, a continuation payload beginning
  // with `key ` belongs to Beast's key clause, not to the iterable expression.
  if (reject_leading_key) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
      advance(lexer);
  }
  if (reject_leading_key && lexer->lookahead == 'k') {
    advance(lexer);
    if (lexer->lookahead == 'e') {
      advance(lexer);
      if (lexer->lookahead == 'y') {
        advance(lexer);
        if (lexer->lookahead == ' ') {
          *rejected_leading_key = true;
          return false;
        }
      }
    }
    has_content = true;
    lexer->mark_end(lexer);
  }

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

static bool emit_continuation(TSLexer *lexer) {
  advance(lexer);
  // For payload continuations, end the hidden token immediately after `~`.
  // Any authored whitespace remains at the start of the payload fragment and
  // separates disjoint ranges passed to a language injection.
  lexer->mark_end(lexer);

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
    advance(lexer);

  // Empty and comment continuations are complete no-op tokens. Leave their
  // trailing newline for the next continuation or logical line terminator.
  if (lexer->eof(lexer) || lexer->lookahead == '\n' ||
      lexer->lookahead == '\r') {
    lexer->mark_end(lexer);
  } else if (lexer->lookahead == '/') {
    advance(lexer);
    if (lexer->lookahead == '/') {
      advance(lexer);
      while (!lexer->eof(lexer) && lexer->lookahead != '\n' &&
             lexer->lookahead != '\r')
        advance(lexer);
      lexer->mark_end(lexer);
    }
  }

  lexer->result_symbol = CONTINUATION;
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

  if (valid_symbols[CONTINUATION] && lexer->lookahead == '~')
    return emit_continuation(lexer);

  // The compiler removes ordinary blank/comment lines before joining logical
  // lines. Mirror that behavior when such lines sit between a predecessor and
  // a continuation. If no continuation follows, return the same layout token
  // that would have been emitted before this lookahead.
  if (valid_symbols[CONTINUATION] && lexer->lookahead == '/') {
    enum TokenType fallback = ERROR_SENTINEL;
    bool fallback_marks_content = false;
    if (valid_symbols[INDENT] && indent > (uint16_t)current_indent) {
      fallback = INDENT;
      fallback_marks_content = true;
    } else if (valid_symbols[DEDENT] && indent < (uint16_t)current_indent) {
      fallback = DEDENT;
      fallback_marks_content =
          scanner->len > 1 &&
          indent >= (uint16_t)scanner->indents[scanner->len - 2];
    } else if (valid_symbols[NEWLINE]) {
      fallback = NEWLINE;
      fallback_marks_content = indent == (uint16_t)current_indent;
    }
    if (fallback_marks_content)
      lexer->mark_end(lexer);

    for (;;) {
      if (lexer->lookahead != '/')
        break;
      advance(lexer);
      if (lexer->lookahead != '/')
        break;
      advance(lexer);
      while (!lexer->eof(lexer) && lexer->lookahead != '\n' &&
             lexer->lookahead != '\r')
        advance(lexer);
      if (lexer->eof(lexer))
        break;
      if (lexer->lookahead == '\r') {
        advance(lexer);
        if (lexer->lookahead == '\n')
          advance(lexer);
      } else {
        advance(lexer);
      }
      while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
        advance(lexer);
      while (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
        if (lexer->lookahead == '\r') {
          advance(lexer);
          if (lexer->lookahead == '\n')
            advance(lexer);
        } else {
          advance(lexer);
        }
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t')
          advance(lexer);
      }
      if (lexer->lookahead == '~')
        return emit_continuation(lexer);
      if (lexer->lookahead != '/')
        break;
    }

    if (fallback == INDENT) {
      if (scanner->len == MAX_INDENTS)
        return false;
      scanner->indents[scanner->len++] = (int16_t)indent;
      lexer->result_symbol = INDENT;
      return true;
    }
    if (fallback == DEDENT) {
      scanner->len--;
      lexer->result_symbol = DEDENT;
      return true;
    }
    if (fallback == NEWLINE) {
      lexer->result_symbol = NEWLINE;
      return true;
    }
    return false;
  }

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
  bool rejected_leading_key = false;
  if (valid_symbols[EACH_ITERABLE_CONTINUATION] &&
      !valid_symbols[ERROR_SENTINEL]) {
    if (scan_each_iterable(lexer, true, &rejected_leading_key)) {
      lexer->result_symbol = EACH_ITERABLE_CONTINUATION;
      return true;
    }
    if (rejected_leading_key)
      return false;
  }
  if (valid_symbols[EACH_ITERABLE] && !valid_symbols[ERROR_SENTINEL] &&
      scan_each_iterable(lexer, false, &rejected_leading_key)) {
    lexer->result_symbol = EACH_ITERABLE;
    return true;
  }
  if (valid_symbols[EXPRESSION_CONTENT] && !valid_symbols[ERROR_SENTINEL] &&
      scan_expression_content(lexer, scanner)) {
    return true;
  }
  return scan(lexer, valid_symbols, scanner);
}
