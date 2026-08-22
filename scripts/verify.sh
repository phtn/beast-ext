#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

tree_sitter="./node_modules/.bin/tree-sitter"
taplo="./node_modules/.bin/taplo"

"$taplo" lint extension.toml languages/beast/config.toml
"$tree_sitter" generate
"$tree_sitter" test
"$tree_sitter" parse -q -p . \
  test/fixtures/full.btsx test/fixtures/expressions.btsx \
  test/fixtures/compiler.btsx test/fixtures/styling.btsx \
  test/fixtures/continuation.btsx

if "$tree_sitter" parse -q -p . \
  test/fixtures/orphan-continuation.btsx >/dev/null 2>&1; then
  echo "expected orphan continuation fixture to fail parsing" >&2
  exit 1
fi

for query_file in languages/beast/*.scm; do
  "$tree_sitter" query -q -p . "$query_file" \
    test/fixtures/full.btsx test/fixtures/expressions.btsx \
    test/fixtures/compiler.btsx test/fixtures/styling.btsx \
    test/fixtures/continuation.btsx
done

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc \
  -fsyntax-only src/parser.c src/scanner.c

git diff --check
