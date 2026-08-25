# Beast

Language support for Beast (`.btsx`), the indentation-first component language
that compiles to Octane-native TSRX.

This repository ships extensions for both Visual Studio Code and Zed.

## Features

- `.btsx` file detection
- Syntax highlighting for source declarations, selectors, attributes, text,
  Octane control flow, comments, embedded TypeScript, and scoped CSS
- Indentation-aware folding, editor indentation, comments, and bracket pairs
- Compiler diagnostics, completion, navigation, hover details, document links,
  symbols, and workspace component references through
  `beast-language-server`
- A **Beast: Restart Language Server** command for troubleshooting

The VS Code package bundles the language server, so users do not need to
install Node.js or a global npm package.

## VS Code development

Install dependencies and create a local VSIX:

```sh
npm ci
npm test
npm run package:vsix
```

Install the generated `beast-<version>.vsix` with **Extensions: Install from
VSIX...**, or open this repository in VS Code and press `F5` to launch an
Extension Development Host.

The **Beast Language Server** output channel contains server logs. Set
`beast.trace.server` to `messages` or `verbose` when protocol tracing is
needed.

## Zed development

1. Open Zed's Extensions view.
2. Run **Install Dev Extension**.
3. Select this repository.
4. Open a `.btsx` file and confirm the language selector shows **Beast**.

After changing `grammar.js` or `src/scanner.c`, regenerate and verify the
parser before reinstalling the development extension:

```sh
npm ci
npm test
npm run build:wasm
cargo check
```

The Zed extension prefers `beast-language-server` from the worktree
environment. Otherwise, it installs the version pinned in `src/lib.rs`
through Zed's npm runtime.

## Grammar development

`npm test` validates both editor distributions. For Zed it validates TOML,
regenerates the parser, runs the corpus, parses feature-complete fixtures,
compiles every query, and compiles the C parser and scanner with warnings
treated as errors. For VS Code it type-checks the client, bundles the client
and language server, and inspects the final package contents.

Additional checks:

```sh
npm run test:fuzz
npm run build:wasm
```

The tree-sitter grammar is in `grammar.js`; indentation and
JavaScript/TypeScript expression boundaries are handled by `src/scanner.c`.
Zed queries and language configuration are under `languages/beast`. VS Code's
TextMate grammar and language configuration are in `syntaxes` and
`language-configuration.json`.

## Publishing to VS Code Marketplace

The Marketplace extension identifier is `phtn.beastjs`. Before the first
release, create or gain access to the `phtn` publisher in the Visual Studio
Marketplace. Then:

1. Update the version in `package.json` and `CHANGELOG.md`.
2. Run `npm ci`, `npm test`, and `npm run package:vsix`.
3. Install and smoke-test the generated VSIX in VS Code.
4. Authenticate `vsce` for the `phtn` publisher and run `npx vsce publish`.

The package task runs `vscode:prepublish`, so the checked VSIX always contains
fresh client and server bundles.

## Publishing to Zed

The Zed extension manifest pins the grammar to an immutable commit. For every
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

## Support

See [SUPPORT.md](SUPPORT.md) for issue-reporting guidance.

## License

MIT
