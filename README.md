# Beast for Zed

Syntax highlighting and structural editing for Beast (`.btsx`), an
indentation-first component language that compiles to Octane-native TSRX.

## Features

- `.btsx` file detection
- Nested indentation-aware parsing
- Highlighting for source declarations, selectors, attributes, text, Octane
  control flow, and comments
- TypeScript injection for module and setup source, attributes,
  interpolations, and control-flow expressions
- Bracket matching, folding, outlines, and editor indentation
- Compiler diagnostics, completion, navigation, hover details, document links,
  and workspace component references through `beast-language-server`

## Install for development

1. Open Zed's Extensions view.
2. Run **Install Dev Extension**.
3. Select this repository.
4. Open a `.btsx` file and confirm the language selector shows **Beast**.

After changing `grammar.js` or `src/scanner.c`, regenerate and verify the
parser before reinstalling the dev extension:

```sh
npm ci
npm test
npm run build:wasm
cargo check
```

The extension prefers `beast-language-server` from the worktree environment.
Otherwise, it installs the version pinned in `src/lib.rs` through Zed's npm
runtime.

## Grammar development

`npm test` validates both TOML files, regenerates the parser, runs the corpus,
parses feature-complete fixtures, compiles every Zed query, compiles the C
parser and external scanner with warnings treated as errors, and checks the
diff for whitespace errors.

Additional checks:

```sh
npm run test:fuzz
npm run build:wasm
```

The grammar source is in `grammar.js`; indentation and JavaScript/TypeScript
expression boundaries are handled by `src/scanner.c`. Zed-specific language
configuration and queries are under `languages/beast`.

## Publishing to Zed

The extension manifest pins the grammar to an immutable commit. For every
release:

1. Run `npm ci`, `npm test`, `npm run test:fuzz`, and `npm run build:wasm`.
2. Commit the regenerated parser, tests, queries, and Wasm grammar.
3. Update `extension.toml`'s grammar `rev` to that commit and make a second
   commit for the pin.
4. Publish the tested `beast-language-server` version and update the matching
   `SERVER_VERSION` in `src/lib.rs`.
5. Bump the version in `extension.toml`, `Cargo.toml`, `package.json`, and
   `tree-sitter.json`.
6. Update the extension's submodule and matching version in
   `zed-industries/extensions`.

The extension repository and grammar URL must remain publicly accessible over
HTTPS, and the pinned grammar revision must remain reachable.

## License

MIT
