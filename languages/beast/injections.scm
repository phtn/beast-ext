; Beast injections — embed JavaScript/TypeScript inside Beast expressions
; Beast hands embedded expressions to the real TypeScript parser (exprParser.ts),
; so highlighting them as JS/TS is correct.

; JS inside #{ ... } interpolation
(interpolation
  body: (expression_body) @injection.content
  (#set! injection.language "tsx"))

; JS inside attribute { ... } e.g. key={message.id}, onClick={() => {}}.
; Anchor the expression to an attribute so nested JS braces are not injected
; a second time.
(attribute
  value: (expression
    body: (expression_body) @injection.content)
  (#set! injection.language "tsx"))

; Conditions after if/elseif/each are raw JS expressions on the same line:
; `if user.isAdmin` , `each item, i in messages`
(if_clause
  condition: (line_expression) @injection.content
  (#set! injection.language "tsx"))

(elseif_clause
  condition: (line_expression) @injection.content
  (#set! injection.language "tsx"))

(each_statement
  iterable: (line_expression) @injection.content
  (#set! injection.language "tsx"))

; Strings are plain strings; don't inject
