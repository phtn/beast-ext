; Beast embeds TypeScript source and expressions and hands final validation to
; Octane. Use the TypeScript grammar because Beast source slices are native
; TypeScript rather than TSX-shaped intermediate code.

; Complete import declarations are already valid TypeScript statements.
((import_declaration) @injection.content
  (#set! injection.language "typescript")
  (#set! injection.include-children))

; Inline and block module/setup source.
(module_declaration
  source: (source_code) @injection.content
  (#set! injection.language "typescript"))

(module_declaration
  source: (source_block) @injection.content
  (#set! injection.language "typescript")
  (#set! injection.include-children))

(setup_declaration
  source: (source_code) @injection.content
  (#set! injection.language "typescript"))

(setup_declaration
  source: (source_block) @injection.content
  (#set! injection.language "typescript")
  (#set! injection.include-children))

; A props parameter is preserved as TypeScript source.
(props_declaration
  parameter: (line_expression) @injection.content
  (#set! injection.language "typescript"))

; Expressions inside text and attributes.
(interpolation
  body: (expression_body) @injection.content
  (#set! injection.language "typescript"))

(attribute
  value: (expression
    body: (expression_body) @injection.content)
  (#set! injection.language "typescript"))

; Conditions and loop expressions.
(if_clause
  condition: (line_expression) @injection.content
  (#set! injection.language "typescript"))

(elseif_clause
  condition: (line_expression) @injection.content
  (#set! injection.language "typescript"))

(each_statement
  iterable: (line_expression) @injection.content
  (#set! injection.language "typescript"))

(each_statement
  key: (line_expression) @injection.content
  (#set! injection.language "typescript"))

; Switch discriminants and case expressions.
(switch_statement
  discriminant: (line_expression) @injection.content
  (#set! injection.language "typescript"))

(case_clause
  condition: (line_expression) @injection.content
  (#set! injection.language "typescript"))

; Catch bindings can be written directly or inside parentheses.
(catch_clause
  bindings: (line_expression) @injection.content
  (#set! injection.language "typescript"))
