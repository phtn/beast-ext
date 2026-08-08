; Beast outline — shows components/elements + control flow in Zed's outline panel

(element
  selector: (selector) @name) @item

(each_statement
  item: (identifier) @name) @item

(if_clause
  condition: (line_expression) @name) @item

(elseif_clause
  condition: (line_expression) @name) @item

(else_clause) @item
