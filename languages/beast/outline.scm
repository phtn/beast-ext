; Beast outline — local components, elements, and control flow.

(component_declaration
  name: (component_name) @name) @item

(element
  selector: (selector) @name) @item

(each_statement
  item: (identifier) @name) @item

(if_clause
  condition: (continued_line_expression
    . (line_expression) @name)) @item

(elseif_clause
  condition: (continued_line_expression
    . (line_expression) @name)) @item

(else_clause
  "else" @name) @item

(switch_statement
  discriminant: (continued_line_expression
    . (line_expression) @name)) @item

(case_clause
  condition: (continued_line_expression
    . (line_expression) @name)) @item

(default_clause
  "default" @name) @item

(try_clause
  "try" @name) @item

(pending_clause
  "pending" @name) @item

(catch_clause
  "catch" @name) @item
