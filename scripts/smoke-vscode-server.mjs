import { fork } from "node:child_process";

const serverPath = new URL("../dist/server.cjs", import.meta.url);
const server = fork(serverPath, ["--node-ipc"], {
  stdio: ["ignore", "pipe", "pipe", "ipc"],
});

let stderr = "";
server.stderr?.setEncoding("utf8");
server.stderr?.on("data", (chunk) => {
  stderr += chunk;
});

const timeout = setTimeout(() => {
  server.kill();
  throw new Error(`language server smoke test timed out\n${stderr}`);
}, 5_000);

const exit = new Promise((resolve, reject) => {
  server.once("error", reject);
  server.once("exit", (code, signal) => {
    if (code === 0) {
      resolve();
      return;
    }
    reject(
      new Error(
        `language server exited with ${signal ?? `code ${code}`}\n${stderr}`,
      ),
    );
  });
});

server.on("message", (message) => {
  if (message?.id === 1) {
    if (message.error || !message.result?.capabilities?.completionProvider) {
      server.kill();
      throw new Error("language server returned an invalid initialize response");
    }
    server.send({ jsonrpc: "2.0", method: "initialized", params: {} });
    server.send({ jsonrpc: "2.0", id: 2, method: "shutdown", params: null });
    return;
  }

  if (message?.id === 2) {
    server.send({ jsonrpc: "2.0", method: "exit", params: null });
  }
});

server.send({
  jsonrpc: "2.0",
  id: 1,
  method: "initialize",
  params: {
    processId: process.pid,
    rootUri: null,
    capabilities: {},
    workspaceFolders: null,
  },
});

await exit;
clearTimeout(timeout);
