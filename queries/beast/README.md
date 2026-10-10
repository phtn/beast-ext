# Beast queries for Neovim

Reference queries for highlighting, folding, indentation, and TypeScript/CSS
injections. These use Neovim capture names and combined injection ranges;
Zed's queries live separately in `languages/beast`.

After changing the grammar or these queries, build the parser and validate
with `ts_query_ls check -f queries/beast`. Keep the corresponding
`nvim-treesitter` queries in sync when submitting an update.
