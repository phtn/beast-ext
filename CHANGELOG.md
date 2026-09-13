# Changelog

All notable changes to the Beast editor extension are documented here.

## 0.3.1

- Update to `beast-language-server@0.2.1` in VS Code and Zed.
- Report TypeScript reference and type errors in `.btsx` files at their Beast
  location, including props passed to imported `.btsx` components.
- Add TypeScript completions with auto-imports from workspace `.ts` files, plus
  TypeScript hover and go-to-definition.
- JSX type checking requires `octane` to be installed in the project.

## 0.3.0

- Add VS Code syntax highlighting and indentation-aware language configuration.
- Bundle the Beast language server for diagnostics, completion, navigation,
  hover information, document links, and workspace references.
- Retain the existing Zed extension and tree-sitter grammar distribution.
