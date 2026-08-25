import { readFile } from "node:fs/promises";
import { createRequire } from "node:module";

import oniguruma from "vscode-oniguruma";
import textmate from "vscode-textmate";

const { loadWASM, OnigScanner, OnigString } = oniguruma;
const { INITIAL, Registry } = textmate;

const require = createRequire(import.meta.url);
const wasmPath = require.resolve("vscode-oniguruma/release/onig.wasm");
const grammarPath = new URL("../syntaxes/beast.tmLanguage.json", import.meta.url);
const fixturePath = new URL("../test/fixtures/compiler.btsx", import.meta.url);

await loadWASM(await readFile(wasmPath));

const grammarDefinition = JSON.parse(await readFile(grammarPath, "utf8"));
const registry = new Registry({
  onigLib: Promise.resolve({
    createOnigScanner: (patterns) => new OnigScanner(patterns),
    createOnigString: (value) => new OnigString(value),
  }),
  loadGrammar: async (scopeName) =>
    scopeName === "source.beast" ? grammarDefinition : null,
});
const grammar = await registry.loadGrammar("source.beast");

if (grammar === null) {
  throw new Error("failed to load the Beast TextMate grammar");
}

let ruleStack = INITIAL;
const scopes = [];
const fixture = await readFile(fixturePath, "utf8");

for (const line of fixture.split(/\r?\n/u)) {
  const result = grammar.tokenizeLine(line, ruleStack);
  ruleStack = result.ruleStack;
  scopes.push(...result.tokens.flatMap((token) => token.scopes));
}

for (const expectedScope of [
  "keyword.declaration.component.beast",
  "entity.name.type.component.beast",
  "meta.interpolation.beast",
  "meta.embedded.inline.typescript",
]) {
  if (!scopes.includes(expectedScope)) {
    throw new Error(`fixture did not produce expected scope: ${expectedScope}`);
  }
}
