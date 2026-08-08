#include "tree_sitter/parser.h"
#include <assert.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

// Beast external scanner: indentation-sensitive, Python/YAML-style.
// Manages _newline / _indent / _dedent tokens declared in grammar.js.
// Extras in grammar = /[ \t]/ (spaces/tabs are skipped). Newlines and
// indentation are *not* extras — we emit them here.
//
// Token types must match order in `externals` in grammar.js:
enum TokenType {
  NEWLINE,
  INDENT,
  DEDENT,
  ERROR_SENTINEL,
};

#define MAX_INDENTS 64

typedef struct {
  int16_t indents[MAX_INDENTS];
  uint16_t len;
} Scanner;

void *tree_sitter_beast_external_scanner_create() {
  Scanner *scanner = (Scanner *)calloc(1, sizeof(Scanner));
  return scanner;
}

void tree_sitter_beast_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_beast_external_scanner_serialize(void *payload, char *buffer) {
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

void tree_sitter_beast_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  scanner->len = 0;
  if (length == 0) return;
  size_t size = 0;
  scanner->len = (uint8_t)buffer[size++];
  if (scanner->len > MAX_INDENTS) scanner->len = MAX_INDENTS;
  for (uint16_t i = 0; i < scanner->len && size + 1 < length; i++) {
    int16_t hi = (int16_t)((uint8_t)buffer[size++]);
    int16_t lo = (int16_t)((uint8_t)buffer[size++]);
    scanner->indents[i] = (int16_t)((hi << 8) | lo);
  }
  if (scanner->len > 0 && scanner->indents[0] != 0) {
    // ensure base 0 stays at bottom if deserialized incorrectly
  }
}

static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }
static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

// Helper: column uses spaces only width currently (tabs count as 8 to be safe)
// Grammar extras skip [ \t], so this matches parser's view.
// We count indent level as raw column (number of leading spaces/tabs after newline).
static bool scan(TSLexer *lexer, const bool *valid_symbols, Scanner *scanner) {
  bool has_newline = false;
  // Bookkeeping: indent after newline handling
  // 1) consume any newlines + leading whitespace, tracking last newline's indent column
  // 2) decide what token to emit based on indent change

  // If we're at EOF, emit pending dedents
  if (lexer->eof(lexer)) {
    if (valid_symbols[DEDENT] && scanner->len > 0) {
      // Pop one dedent per scan call (tree-sitter calls repeatedly)
      // Leave one level (0) on stack.
      if (scanner->len > 1) {
        scanner->len--;
        lexer->result_symbol = DEDENT;
        return true;
      }
    }
    return false;
  }

  // Skip nothing except we need to see newlines. The extras handling in grammar
  // already skips spaces/tabs *within* a line, but newlines are NOT extras.
  // However lexer may be in middle of line — we only act when we see newline.

  // Consume whitespace that precedes a newline? Actually tree-sitter calls scanner
  // only when one of our external tokens is valid. We should check for newline.

  // First, look ahead: if next char is '\n' or '\r', we have a newline boundary.
  // We also handle \r\n.

  // Peek if we're at a newline
  bool at_newline = false;
  if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
    at_newline = true;
  }

  if (at_newline) {
    // Consume one run of newlines (handles blank lines)
    // For blank lines we do NOT emit indent/dedent — just consume them.
    int32_t indent = -1;
    bool found_content = false;

    while (true) {
      has_newline = true;
      // consume \r and \n
      if (lexer->lookahead == '\r') {
        advance(lexer);
        if (lexer->lookahead == '\n') advance(lexer);
      } else if (lexer->lookahead == '\n') {
        advance(lexer);
      } else {
        break;
      }

      // After newline, count indentation (spaces/tabs)
      int32_t column = 0;
      while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        if (lexer->lookahead == '\t') column += 8 - (column % 8);
        else column++;
        advance(lexer);
      }

      // Handle comments-style blank? Beast has `// comment` — but that is a
      // grammar token, not whitespace. So blank line detection:
      // if next char is newline again -> blank line, continue loop
      if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
        // blank line — no indent change
        continue;
      }
      if (lexer->eof(lexer)) {
        // EOF after newline + indent — emit dedents first, then EOF
        indent = column;
        found_content = false;
        break;
      }
      // If line is `//` comment? Actually comment is a statement starting with //
      // We should NOT skip it — the comment token will consume `//...` itself.
      // But blank-line logic already handled pure whitespace lines. For comment
      // lines, we still need to emit indent logic based on their column.
      // So treat comment line as real content.
      indent = column;
      found_content = true;
      break;
    }

    // At this point we've consumed newlines + indent spaces (as lexer progress),
    // but we must not over-consume if we looked past EOF.
    // Now decide token based on indent vs stack.

    if (!found_content) {
      // EOF case after final newline
      if (valid_symbols[DEDENT] && scanner->len > 1) {
        // Don't emit newline at EOF if we need a dedent — dedent has priority
        // But we already consumed the newline chars; mark newline as handled
        // Re-check: tree-sitter expects dedents before EOF.
        // We consumed the newline bytes via advance(), but we need to return DEDENT.
        // The newline will be synthesized as dedent boundary, not a separate token.
        // Trick: if indent < current top, we should emit DEDENT, not NEWLINE.
        // So check indent stack.
        int16_t top = scanner->indents[scanner->len - 1];
        if ((int16_t)indent < top) {
          scanner->len--;
          lexer->result_symbol = DEDENT;
          return true;
        }
      }
      if (valid_symbols[NEWLINE] && has_newline) {
        lexer->result_symbol = NEWLINE;
        return true;
      }
      return false;
    }

    // found_content == true, indent is column of next real line
    int16_t top = scanner->len > 0 ? scanner->indents[scanner->len - 1] : 0;

    if ((int16_t)indent > top) {
      // Indent increase
      if (valid_symbols[INDENT]) {
        if (scanner->len < MAX_INDENTS) {
          scanner->indents[scanner->len++] = (int16_t)indent;
        }
        lexer->result_symbol = INDENT;
        return true;
      }
      // If INDENT not valid here (error recovery), just emit newline and keep stack
      // but parser will error. We still need to not lose indent.
      // Push anyway to keep sync, then fall through to newline?
      if (scanner->len < MAX_INDENTS) {
        scanner->indents[scanner->len++] = (int16_t)indent;
      }
    } else if ((int16_t)indent < top) {
      // Dedent — may be multiple levels, but we emit one per scan call
      if (valid_symbols[DEDENT]) {
        scanner->len--;
        lexer->result_symbol = DEDENT;
        return true;
      }
      // If DEDENT not expected, still pop to resync? Pop until <= indent
      // so future calls emit correct sequence.
      while (scanner->len > 1 && scanner->indents[scanner->len - 1] > (int16_t)indent) {
        scanner->len--;
      }
    }

    // Same indent level — emit newline
    if (valid_symbols[NEWLINE] && has_newline) {
      lexer->result_symbol = NEWLINE;
      // Mark end so lexer doesn't re-consume indent spaces as content
      lexer->mark_end(lexer);
      return true;
    }
    return false;
  }

  // Not at newline — handle dedents at EOF or when parser expects them mid-stream?
  // Python scanner also handles case where indent is expected but we're not at newline:
  // It will skip spaces to EOL to check. We already handled that above.
  // If we're not at newline and DEDENT is valid and stack > 0, check if next is EOF?

  if (lexer->eof(lexer) && valid_symbols[DEDENT] && scanner->len > 1) {
    scanner->len--;
    lexer->result_symbol = DEDENT;
    return true;
  }

  // If parser is in error recovery and expects ERROR_SENTINEL, emit it
  if (valid_symbols[ERROR_SENTINEL]) {
    // Python grammar uses this to recover from invalid dedents
    // We emit it when indent is inconsistent
    // For now, just allow recovery by consuming one char
    return false;
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
  return scan(lexer, valid_symbols, scanner);
}
