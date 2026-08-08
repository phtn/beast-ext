/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'beast',

  // Only horizontal whitespace is auto-skipped between tokens. Newlines
  // and indentation are meaningful and handled explicitly by the external
  // scanner (src/scanner.c), which emits _newline / _indent / _dedent based
  // on each line's leading-space column, the same strategy tree-sitter's
  // own Python and YAML grammars use.
  extras: ($) => [/[ \t]/],

  externals: ($) =>
    [$._newline, $._indent, $._dedent, $._error_sentinel, $._expression_content],

  // Lets tree-sitter prefer literal keyword tokens ("if", "each", ...) over
  // the generic `identifier` token whenever both would match the same text.
  word: ($) => $.identifier,

  conflicts: ($) => [],

  rules: {
    source_file: ($) => seq(optional($._newline), repeat($._statement)),

    _statement: ($) =>
      choice($._comment_statement, $.if_statement, $.each_statement, $.text_line, $.element),

    _comment_statement: ($) => seq($.comment, $._newline),
    comment: ($) => token(seq('//', /[^\r\n]*/)),

    // ---- element lines: `.card`, `#main.wrap`, `Button(...) label` ----
    element: ($) =>
      seq(
        field('selector', $.selector),
        optional(field('attributes', $.attributes)),
        optional(field('text', $.text_content)),
        choice($._newline, field('block', $.block))
      ),

    selector: ($) =>
      choice(
        seq(field('tag', $.identifier), repeat(choice($.class_selector, $.id_selector))),
        repeat1(choice($.class_selector, $.id_selector))
      ),

    class_selector: ($) => seq('.', field('name', $.css_name)),
    id_selector: ($) => seq('#', field('name', $.css_name)),

    // ---- attribute list: `(href="/x" onClick={fn} disabled)` ----
    attributes: ($) => seq('(', repeat(seq($.attribute, optional(','))), ')'),

    attribute: ($) =>
      seq(
        field('name', $.attribute_name),
        optional(seq('=', field('value', choice($.string, $.expression))))
      ),

    // ---- if / elseif / else chain ----
    if_statement: ($) => seq($.if_clause, repeat($.elseif_clause), optional($.else_clause)),

    if_clause: ($) =>
      seq('if', field('condition', $.line_expression), choice($._newline, field('block', $.block))),

    elseif_clause: ($) =>
      seq(
        'elseif',
        field('condition', $.line_expression),
        choice($._newline, field('block', $.block))
      ),

    else_clause: ($) => seq('else', choice($._newline, field('block', $.block))),

    // ---- each item[, index] in iterable ----
    each_statement: ($) =>
      seq(
        'each',
        field('item', $.identifier),
        optional(seq(',', field('index', $.identifier))),
        'in',
        field('iterable', $.line_expression),
        choice($._newline, field('block', $.block))
      ),

    // ---- explicit text-only line: `| some text #{expr}` ----
    text_line: ($) => seq('|', optional(field('text', $.text_content)), $._newline),

    block: ($) => seq($._newline, $._indent, repeat1($._statement), $._dedent),

    // ---- text content with #{...} interpolation ----
    text_content: ($) => repeat1(choice($.interpolation, $.text_fragment)),

    text_fragment: ($) => token(prec(-1, /[^\n#]+|#/)),

    interpolation: ($) =>
      seq(
        '#{',
        optional(field('body', alias($._expression_content, $.expression_body))),
        '}'
      ),

    expression: ($) =>
      seq(
        '{',
        optional(field('body', alias($._expression_content, $.expression_body))),
        '}'
      ),

    // Rest-of-line raw expression text, used for if/elseif/each conditions
    // where there's no brace delimiter (e.g. `if user.isAdmin`).
    line_expression: ($) => token(prec(-1, /[^\n]+/)),

    string: ($) =>
      choice(
        seq('"', repeat(choice($.escape_sequence, /[^"\\\r\n]+/)), '"'),
        seq("'", repeat(choice($.escape_sequence, /[^'\\\r\n]+/)), "'")
      ),

    escape_sequence: ($) => token(seq('\\', /[^\r\n]/)),

    identifier: ($) => /[A-Za-z_][A-Za-z0-9_]*/,
    attribute_name: ($) => /[A-Za-z_][A-Za-z0-9_:.-]*/,
    css_name: ($) => /[A-Za-z_][A-Za-z0-9_-]*/
  }
})
