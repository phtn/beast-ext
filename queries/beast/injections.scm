; Beast embeds TypeScript source and expressions and hands final validation to
; Octane. Use the TypeScript grammar because Beast source slices are native
; TypeScript rather than TSX-shaped intermediate code.

; Multiple @injection.content captures in one match become disjoint included
; ranges in Zed. Continuation prefixes are therefore omitted while all payload
; fragments are parsed as one logical TypeScript/CSS document.
(import_declaration
  "import" @injection.content
  source: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

; Inline and block module/setup source.
((continued_source_code
  (source_code)+ @injection.content)
  (#set! injection.language "typescript"))

; A props parameter is preserved as TypeScript source.
(props_declaration
  parameter: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

; Expressions inside text and attributes.
(interpolation
  body: (expression_body
    (expression_fragment)+ @injection.content)
  (#set! injection.language "typescript"))

(attribute
  value: (expression
    body: (expression_body
      (expression_fragment)+ @injection.content))
  (#set! injection.language "typescript"))

(spread_attribute
  argument: (expression_body
    (expression_fragment)+ @injection.content)
  (#set! injection.language "typescript"))

; Raw style blocks are native CSS with their source indentation preserved.
((continued_style_source
  (style_source)+ @injection.content)
  (#set! injection.language "css")
  (#set! injection.combined))

; Conditions and loop expressions.
(if_clause
  condition: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

(elseif_clause
  condition: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

(each_statement
  iterable: (continued_each_iterable
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

(each_statement
  key: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

; Switch discriminants and case expressions.
(switch_statement
  discriminant: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

(case_clause
  condition: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))

; Catch bindings can be written directly or inside parentheses.
(catch_clause
  bindings: (continued_line_expression
    (line_expression)+ @injection.content)
  (#set! injection.language "typescript"))
