import { fork } from "node:child_process";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";

const project = await mkdtemp(join(tmpdir(), "beast-smoke-"));
const utilsPath = join(project, "utils.ts");
const appPath = join(project, "App.btsx");
await writeFile(utilsPath, "export function formatPrice(value: number): string {\n  return String(value);\n}\n");
const appSource = 'import { formatPrice } from "./utils.ts";\nsetup const label = formatPrice(1) + missingName;\np #{label}\n';
await writeFile(appPath, appSource);
const appUri = pathToFileURL(appPath).href;

const serverPath = new URL("../dist/server.cjs", import.meta.url);
const server = fork(serverPath, ["--node-ipc"], {
  stdio: ["ignore", "pipe", "pipe", "ipc"],
});

let stderr = "";
server.stderr?.setEncoding("utf8");
server.stderr?.on("data", (chunk) => {
  stderr += chunk;
});

let failure;
function fail(message) {
  failure ??= new Error(`${message}\n${stderr}`);
  server.kill();
}

const timeout = setTimeout(() => fail("language server smoke test timed out"), 15_000);

const exit = new Promise((resolve, reject) => {
  server.once("error", reject);
  server.once("exit", (code, signal) => {
    if (failure) {
      reject(failure);
    } else if (code === 0) {
      resolve();
    } else {
      reject(new Error(`language server exited with ${signal ?? `code ${code}`}\n${stderr}`));
    }
  });
});

server.on("message", (message) => {
  if (message?.id === 1) {
    if (message.error || !message.result?.capabilities?.completionProvider?.resolveProvider) {
      fail("language server returned an invalid initialize response");
      return;
    }
    server.send({ jsonrpc: "2.0", method: "initialized", params: {} });
    server.send({
      jsonrpc: "2.0",
      method: "textDocument/didOpen",
      params: { textDocument: { uri: appUri, languageId: "beast", version: 1, text: appSource } },
    });
    return;
  }

  // The bundled TypeScript service must resolve `.ts` imports and report
  // unresolved names; this breaks if the bundle's TypeScript or lib files are wrong.
  if (message?.method === "textDocument/publishDiagnostics" && message.params.uri === appUri) {
    const codes = message.params.diagnostics.map((diagnostic) => diagnostic.code);
    if (!codes.includes(2304) || codes.includes(2307)) {
      fail(`unexpected TypeScript diagnostics: ${JSON.stringify(message.params.diagnostics)}`);
      return;
    }
    server.send({
      jsonrpc: "2.0",
      id: 2,
      method: "textDocument/completion",
      params: {
        textDocument: { uri: appUri },
        position: { line: 1, character: "setup const label = formatPr".length },
        context: { triggerKind: 1 },
      },
    });
    return;
  }

  if (message?.id === 2) {
    const items = message.result?.items ?? message.result ?? [];
    if (!items.some((item) => item.label === "formatPrice" && item.data?.kind === "typescript")) {
      fail(`missing TypeScript completions: ${JSON.stringify(items.slice(0, 10))}`);
      return;
    }
    server.send({ jsonrpc: "2.0", id: 3, method: "shutdown", params: null });
    return;
  }

  if (message?.id === 3) {
    server.send({ jsonrpc: "2.0", method: "exit", params: null });
  }
});

server.send({
  jsonrpc: "2.0",
  id: 1,
  method: "initialize",
  params: {
    processId: process.pid,
    rootUri: pathToFileURL(project).href,
    capabilities: {},
    workspaceFolders: [{ uri: pathToFileURL(project).href, name: "smoke" }],
  },
});

try {
  await exit;
} finally {
  clearTimeout(timeout);
  await rm(project, { recursive: true, force: true });
}
