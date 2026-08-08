; Beast indents — Python-style indentation
; Zed uses @indent to decide when Enter should indent, and @end to dedent.

(block) @indent

; Dedent after block's dedent token is implicit — but we still mark
; the dedent region so Zed knows to outdent next line.
(block _dedent @end)

; Attribute parens inside an element line should not affect indent,
; but block after newline controls indent.
(element block: (block) @indent)
(if_clause block: (block) @indent)
(elseif_clause block: (block) @indent)
(else_clause block: (block) @indent)
(each_statement block: (block) @indent)
