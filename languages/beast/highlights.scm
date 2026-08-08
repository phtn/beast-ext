; Beast (.btsx) highlights — tree-sitter query for Zed
; Node types come from grammar.js (selector, class_selector, id_selector,
; attributes, attribute, string, interpolation, expression, text_fragment,
; if_clause/elseif_clause/else_clause, each_statement, comment, etc.)

; Comments
(comment) @comment

; Keywords
[
  "if"
  "elseif"
  "else"
  "each"
  "in"
] @keyword

; Pipes for explicit text lines
(text_line "|" @punctuation.special)

; Selectors — tags vs components (capitalized = component)
(selector
  tag: (identifier) @tag
  (#match? @tag "^[a-z]"))

(selector
  tag: (identifier) @constructor
  (#match? @constructor "^[A-Z]"))

; Classes and ids
(class_selector "." @punctuation.special)
(class_selector name: (css_name) @attribute)

(id_selector "#" @punctuation.special)
(id_selector name: (css_name) @property)

; Attributes: name and values
(attribute name: (identifier) @property)

(string) @string
; attribute values that are expressions: the braces themselves
(expression "{" @punctuation.bracket)
(expression "}" @punctuation.bracket)

; Interpolation #{...}
(interpolation "#{" @punctuation.special)
(interpolation "}" @punctuation.special)

; The JS/TS inside #{...} and {...} is highlighted via injections.scm,
; but the braces still get punctuation. The inner identifiers get
; @variable/@property via the injection.

; Text fragments are plain text
(text_fragment) @text.literal

; Punctuation for attribute parens and commas
(attributes "(" @punctuation.bracket)
(attributes ")" @punctuation.bracket)
(attributes "," @punctuation.delimiter)

; Operators inside attribute `=` are covered by the injected JS,
; but the `=` itself:
(attribute "=" @operator)

; line_expression (if/each conditions) is injected as JS — no direct capture
; but fallback to variable if injection missing:
(line_expression) @variable

; Strings already captured above
