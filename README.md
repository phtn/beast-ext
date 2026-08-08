# Beast for Zed

Syntax highlighting and structural editing for Beast (`.btsx`), a Pug-style,
indentation-based language that compiles to TSX.

## Features

- `.btsx` file detection
- Nested indentation-aware parsing
- Highlighting for selectors, attributes, text, control flow, and comments
- TSX injection for attributes, interpolations, and control-flow expressions
- Bracket matching, folding, outlines, and editor indentation

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
```

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
4. Bump the version in `extension.toml`, `package.json`, and `tree-sitter.json`.
5. Update the extension's submodule and matching version in
   `zed-industries/extensions`.

The extension repository and grammar URL must remain publicly accessible over
HTTPS, and the pinned grammar revision must remain reachable.

## License

MIT
