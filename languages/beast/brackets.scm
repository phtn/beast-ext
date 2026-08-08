; Beast brackets — Zed highlights matching pairs and rainbow colors

("(" @open ")" @close)
("{" @open "}" @close)
("#{" @open "}" @close)
("[" @open "]" @close)
("\"" @open "\"" @close)
("'" @open "'" @close)

; Attributes parens are also brackets — already covered by "(" handling
; but keep explicit for completeness
(attributes "(" @open ")" @close)
