; Beast (.btsx) highlights — tree-sitter query for Zed

; Comments
(comment) @comment

; Beast declarations and control flow
[
  "import"
  "module"
  "component"
  "props"
  "setup"
  "if"
  "elseif"
  "else"
  "each"
  "in"
  "key"
  "empty"
  "switch"
  "case"
  "default"
  "try"
  "pending"
  "catch"
] @keyword

(component_declaration
  name: (component_name) @function)

(each_statement
  item: (identifier) @variable.parameter)

(each_statement
  index: (identifier) @variable.parameter)

; Pipes for explicit text lines
(text_line "|" @punctuation.special)

; Selectors — lowercase tags and component references are distinct nodes.
(selector
  tag: (tag_name) @tag)

(selector
  tag: (component_name) @constructor)

(component_member "." @punctuation.delimiter)
(component_member name: (component_member_name) @constructor)

; Classes and ids
(class_selector "." @punctuation.special)
(class_selector name: (css_name) @attribute)

(id_selector "#" @punctuation.special)
(id_selector name: (css_name) @property)

; Attributes: names and values
(attribute name: (attribute_name) @property)

(string) @string
(escape_sequence) @string.escape

; Braces around attribute expressions and interpolations remain Beast
; punctuation; their contents are highlighted by TypeScript injections.
(expression "{" @punctuation.bracket)
(expression "}" @punctuation.bracket)

(interpolation "#{" @punctuation.special)
(interpolation "}" @punctuation.special)

; Text fragments are literal output.
(text_fragment) @text.literal

; Attribute punctuation.
(attributes "(" @punctuation.bracket)
(attributes ")" @punctuation.bracket)
(attributes "," @punctuation.delimiter)
(attribute "=" @operator)

; Embedded source and expressions receive their detailed highlighting from
; injections.scm. These captures are readable fallbacks when TSX is absent.
[
  (source_code)
  (line_expression)
] @embedded
