; Beast brackets — Zed highlights matching pairs and rainbow colors

("(" @open ")" @close)
("{" @open "}" @close)
("#{" @open "}" @close)

(("\"" @open "\"" @close)
  (#set! rainbow.exclude))

(("'" @open "'" @close)
  (#set! rainbow.exclude))
