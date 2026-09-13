// The bundled language server embeds the TypeScript compiler, which looks for
// its default `lib.*.d.ts` files next to the executing script (dist/server.cjs).
import { copyFile, mkdir, readdir, readFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const dist = join(root, "dist");
// Resolve TypeScript the way esbuild does from the server entry, not this repo's own.
const require = createRequire(join(root, "node_modules/beast-language-server/dist/server.js"));
const packagePath = require.resolve("typescript/package.json");
const { version } = JSON.parse(await readFile(packagePath, "utf8"));
const major = Number(version.split(".")[0]);
if (major < 5 || major >= 7) {
  throw new Error(
    `beast-language-server resolved typescript@${version}; it needs the TypeScript 5.x/6.x JavaScript API`,
  );
}

const lib = join(dirname(packagePath), "lib");
const files = (await readdir(lib)).filter((name) => /^lib\..*\.d\.ts$/u.test(name));
await mkdir(dist, { recursive: true });
await Promise.all(files.map((name) => copyFile(join(lib, name), join(dist, name))));
console.log(`copied ${files.length} TypeScript ${version} lib files to dist/`);
