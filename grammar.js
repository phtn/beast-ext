/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'beast',

  // Horizontal whitespace and `~` continuation prefixes are auto-skipped.
  // Ordinary newlines and indentation remain meaningful and are emitted by
  // the external scanner based on each line's leading-space column.
  extras: ($) => [/[ \t]/, $._continuation],

  externals: ($) =>
    [
      $._newline,
      $._indent,
      $._dedent,
      $._error_sentinel,
      $._expression_content,
      $._each_iterable,
      $._each_iterable_continuation,
      $._continuation
    ],

  // Prefer Beast's literal keywords over JavaScript-compatible identifiers.
  word: ($) => $.identifier,

  conflicts: ($) => [],

  rules: {
    source_file: ($) =>
      seq(
        optional($._newline),
        repeat(choice(prec(1, $._comment_statement), $._declaration)),
        repeat($._statement)
      ),

    _declaration: ($) =>
      choice(
        $.import_declaration,
        $.module_declaration,
        $.component_declaration,
        $.props_declaration,
        $.setup_declaration
      ),

    _statement: ($) =>
      choice(
        $._comment_statement,
        $.if_statement,
        $.each_statement,
        $.switch_statement,
        $.try_statement,
        $.fragment_statement,
        $.style_statement,
        $.text_line,
        $.element
      ),

    _comment_statement: ($) => seq($.comment, $._newline),
    comment: ($) => token(seq('//', /[^\r\n]*/)),

    // ---- source declarations: module/import/component/props/setup ----
    import_declaration: ($) =>
      seq('import', field('source', $.continued_line_expression), $._newline),

    module_declaration: ($) =>
      seq(
        'module',
        choice(
          seq(field('source', $.continued_source_code), $._newline),
          field('source', $.source_block)
        )
      ),

    component_declaration: ($) =>
      seq(
        'component',
        field('name', $.component_name),
        field('body', $.component_block)
      ),

    component_block: ($) =>
      seq(
        $._newline,
        $._indent,
        repeat(
          choice(
            prec(1, $._comment_statement),
            $.props_declaration,
            $.setup_declaration
          )
        ),
        repeat1($._statement),
        $._dedent
      ),

    props_declaration: ($) =>
      seq('props', field('parameter', $.continued_line_expression), $._newline),

    setup_declaration: ($) =>
      seq(
        'setup',
        choice(
          seq(field('source', $.continued_source_code), $._newline),
          field('source', $.source_block)
        )
      ),

    // Indented TypeScript source is structurally opaque to Beast. Model its
    // indentation so the outer Beast tree remains stable, then inject the
    // entire source_block into the TypeScript grammar for highlighting.
    source_block: ($) =>
      seq($._newline, $._indent, repeat1($.source_statement), $._dedent),

    source_statement: ($) =>
      seq(
        field('source', $.continued_source_code),
        choice($._newline, field('continuation', $.source_block))
      ),

    continued_source_code: ($) => repeat1($.source_code),
    source_code: ($) => token(prec(-1, /[^\r\n]+/)),

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
        seq(
          field('tag', $.component_name),
          repeat(field('member', $.component_member)),
          repeat(choice($.class_selector, $.id_selector))
        ),
        seq(
          field('tag', $.tag_name),
          repeat(choice($.class_selector, $.id_selector))
        ),
        repeat1(choice($.class_selector, $.id_selector))
      ),

    component_member: ($) => seq('.', field('name', $.component_member_name)),
    class_selector: ($) => seq('.', field('name', $.css_name)),
    id_selector: ($) => seq('#', field('name', $.css_name)),

    // ---- attribute list: `(href="/x" {...props} disabled)` ----
    attributes: ($) =>
      seq(
        '(',
        repeat(seq(choice($.attribute, $.spread_attribute), optional(','))),
        ')'
      ),

    attribute: ($) =>
      seq(
        field('name', $.attribute_name),
        optional(seq('=', field('value', choice($.string, $.expression))))
      ),

    spread_attribute: ($) =>
      seq(
        '{',
        '...',
        field('argument', $.expression_body),
        '}'
      ),

    // ---- if / elseif / else chain ----
    if_statement: ($) => seq($.if_clause, repeat($.elseif_clause), optional($.else_clause)),

    if_clause: ($) =>
      seq('if', field('condition', $.continued_line_expression), choice($._newline, field('block', $.block))),

    elseif_clause: ($) =>
      seq(
        'elseif',
        field('condition', $.continued_line_expression),
        choice($._newline, field('block', $.block))
      ),

    else_clause: ($) => seq('else', choice($._newline, field('block', $.block))),

    // ---- each item[, index] in iterable [key expression] / empty ----
    each_statement: ($) =>
      seq(
        'each',
        field('item', $.identifier),
        optional(seq(',', field('index', $.identifier))),
        'in',
        field('iterable', $.continued_each_iterable),
        optional(seq('key', field('key', $.continued_line_expression))),
        choice($._newline, field('block', $.block)),
        optional(field('empty', $.empty_clause))
      ),

    empty_clause: ($) => seq('empty', choice($._newline, field('block', $.block))),

    // ---- switch expression / case expression / default ----
    switch_statement: ($) =>
      seq(
        'switch',
        field('discriminant', $.continued_line_expression),
        field('body', $.switch_block)
      ),

    switch_block: ($) =>
      seq(
        $._newline,
        $._indent,
        repeat1(choice($._comment_statement, $.case_clause, $.default_clause)),
        $._dedent
      ),

    case_clause: ($) =>
      seq('case', field('condition', $.continued_line_expression), choice($._newline, field('block', $.block))),

    default_clause: ($) => seq('default', choice($._newline, field('block', $.block))),

    // ---- try / pending / catch [bindings] ----
    try_statement: ($) =>
      seq(
        $.try_clause,
        choice(
          seq($.pending_clause, optional($.catch_clause)),
          $.catch_clause
        )
      ),

    try_clause: ($) => seq('try', choice($._newline, field('block', $.block))),

    pending_clause: ($) => seq('pending', choice($._newline, field('block', $.block))),

    catch_clause: ($) =>
      seq(
        'catch',
        optional(field('bindings', $.continued_line_expression)),
        choice($._newline, field('block', $.block))
      ),

    // ---- explicit fragment / raw scoped CSS ----
    fragment_statement: ($) =>
      seq('fragment', field('block', $.block)),

    style_statement: ($) =>
      seq('style', field('body', $.style_block)),

    // CSS is structurally opaque to Beast. Preserve its indentation tree so
    // the entire block can be injected into the CSS grammar.
    style_block: ($) =>
      seq($._newline, $._indent, repeat1($.style_source_statement), $._dedent),

    style_source_statement: ($) =>
      seq(
        field('source', $.continued_style_source),
        choice($._newline, field('continuation', $.style_block))
      ),

    continued_style_source: ($) => repeat1($.style_source),
    style_source: ($) => token(prec(-1, /[^\r\n]+/)),

    // ---- explicit text-only line: `| some text #{expr}` ----
    text_line: ($) => seq('|', optional(field('text', $.text_content)), $._newline),

    block: ($) => seq($._newline, $._indent, repeat1($._statement), $._dedent),

    // ---- text content with #{...} interpolation ----
    text_content: ($) => repeat1(choice($.interpolation, $.text_fragment)),

    text_fragment: ($) => token(prec(-1, /[^\r\n#]+|#/)),

    interpolation: ($) =>
      seq(
        '#{',
        optional(field('body', $.expression_body)),
        '}'
      ),

    expression: ($) =>
      seq(
        '{',
        optional(field('body', $.expression_body)),
        '}'
      ),

    // Rest-of-line raw expression/source text. JavaScript and TypeScript
    // structure within these slices is delegated to injections.scm.
    continued_each_iterable: ($) =>
      seq(
        alias($._each_iterable, $.line_expression),
        repeat(alias($._each_iterable_continuation, $.line_expression))
      ),
    continued_line_expression: ($) => repeat1($.line_expression),
    expression_body: ($) => repeat1(alias($._expression_content, $.expression_fragment)),
    line_expression: ($) => token(prec(-1, /[^\r\n]+/)),

    string: ($) =>
      choice(
        seq('"', repeat(choice($.escape_sequence, /[^"\\\r\n]+/)), '"'),
        seq("'", repeat(choice($.escape_sequence, /[^'\\\r\n]+/)), "'")
      ),

    escape_sequence: ($) => token(seq('\\', /[^\r\n]/)),

    identifier: ($) => /[A-Za-z_$][A-Za-z0-9_$]*/,
    component_name: ($) => /[A-Z][A-Za-z0-9_$:-]*/,
    component_member_name: ($) => token(prec(1, /[A-Z_$][A-Za-z0-9_$]*/)),
    tag_name: ($) => /[a-z][A-Za-z0-9_$:-]*/,
    attribute_name: ($) => /[A-Za-z_$][A-Za-z0-9_$:-]*/,
    css_name: ($) => /[A-Za-z0-9_-]+/
  }
})
