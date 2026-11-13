# Goose for VS Code

Syntax highlighting, snippets and language configuration for Goose (`.goose`),
the language the FrankenMoE runtime is written in.

This package is isolated from the runtime: it only adds editor support, it does
not change the compiler, the runtime or any program behaviour.

## Install

From this directory:

```
npx --yes @vscode/vsce package
code --install-extension goose-language-0.1.0.vsix
```

Or copy the directory into `~/.vscode/extensions/goose-language` for local
development and reload the window.

## What it provides

| file | content |
|---|---|
| `syntaxes/goose.tmLanguage.json` | TextMate grammar, scope name `source.goose` |
| `language-configuration.json` | comments, brackets, auto-closing, indentation, word pattern |
| `snippets/goose.json` | `fn`, `export fn`, `extern fn`, `struct`, `enum`, `import`, `namespace`, `if`, `while`, `for`, `match` |

The grammar is written from the language itself, not from Go or Rust:

- comments `//` and `/* ... */`, including nesting;
- strings: `"..."`, `'...'` and raw `"""..."""`, with the escape table;
- numbers: decimal, hexadecimal, decimal and hexadecimal floats with exponents;
- declarations: `fn`/`thread_fn`, `struct`/`enum`/`type`, `namespace`,
  `const`/`let`/`var`/`pad`/`reusable`, `extern "symbol" fn`;
- keywords: `if`, `else`, `while`, `for`, `in`, `loop`, `block`, `guard`,
  `match`, `return`, `break`, `continue`, `import`, `as`;
- modifiers: `export`, `extern`, `simd`, `recursive`;
- primitive types `bool`, `varint`, `i8`-`i64`, `u8`-`u64`, `f32`, `f64`;
- literals `true`, `false`, `null`, `self`;
- namespaces (`name::`), calls, members, `len`/`cap`, constants, operators from
  the compiler lexer and punctuation.

## Test

The test tokenizes a synthetic file and real repository sources with
`vscode-textmate` and asserts the scopes. The two test-only packages are fetched
by `npx`:

```
mkdir -p /tmp/goose-grammar-test
cd /tmp/goose-grammar-test
npx --yes npm@10 install --no-audit --no-fund vscode-textmate@9 vscode-oniguruma@2
cd -
NODE_PATH=/tmp/goose-grammar-test/node_modules node test/grammar.test.js
```

Expected: the synthetic checks pass and the real files tokenize without
`invalid` scopes (`goose/jinja/lexer.goose`, `parser.goose`, `runtime.goose`).

## GitHub highlighting

A grammar in this repository does not make github.com highlight `.goose` files.
That requires an accepted change in `github-linguist/linguist`; see
`linguist/README.md` for the current requirements, the prepared entry and the
samples. The upstream step is not done and cannot be done until Goose meets the
usage threshold.

## License

MIT for this package. The samples under `linguist/samples/` are Apache-2.0,
copied from the FrankenMoE-CUDA repository.
