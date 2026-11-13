'use strict';

// Grammar test: tokenizes synthetic Goosed code and real repository files with
// vscode-textmate and checks the scopes the grammar is expected to produce.
//
// Run with the two test-only packages supplied by npx:
//   npx --yes -p vscode-textmate@9 -p vscode-oniguruma@2 node test/grammar.test.js

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const textmate = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

const here = __dirname;
const grammarPath = path.resolve(here, '../syntaxes/goose.tmLanguage.json');
const repo = path.resolve(here, '../../..');

async function loadGrammar() {
    const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
    const lib = {
        createOnigScanner: patterns => new oniguruma.OnigScanner(patterns),
        createOnigString: value => new oniguruma.OnigString(value),
    };
    await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
    const registry = new textmate.Registry({
        onigLib: Promise.resolve(lib),
        loadGrammar: async () => textmate.parseRawGrammar(fs.readFileSync(grammarPath, 'utf8'), grammarPath),
    });
    return registry.loadGrammar('source.goose');
}

function tokenize(grammar, source) {
    let state = textmate.INITIAL;
    const tokens = [];
    for (const line of source.split('\n')) {
        const result = grammar.tokenizeLine(line, state);
        state = result.ruleStack;
        for (const token of result.tokens) {
            tokens.push({ text: line.slice(token.startIndex, token.endIndex), scopes: token.scopes });
        }
    }
    return tokens;
}

const has = (tokens, text, scope) =>
    tokens.some(t => t.text === text && t.scopes.some(s => s.startsWith(scope)));

const checks = [];

function check(name, fn) {
    checks.push([name, fn]);
}

check('declarations, keywords, types, literals', tokens => {
    assert.ok(has(tokens, 'fn', 'storage.type.function'), 'fn keyword');
    assert.ok(has(tokens, 'add', 'entity.name.function'), 'function name');
    assert.ok(has(tokens, 'Number', 'entity.name.type'), 'struct name');
    assert.ok(has(tokens, 'i64', 'storage.type.primitive'), 'primitive type');
    assert.ok(has(tokens, 'if', 'keyword.control'), 'if keyword');
    assert.ok(has(tokens, 'while', 'keyword.control'), 'while keyword');
    assert.ok(has(tokens, 'for', 'keyword.control'), 'for keyword');
    assert.ok(has(tokens, 'import', 'keyword.other.import'), 'import keyword');
    assert.ok(has(tokens, 'true', 'constant.language.boolean'), 'boolean literal');
    assert.ok(has(tokens, 'self', 'variable.language'), 'self');
});

check('comments, strings, numbers, operators', tokens => {
    assert.ok(has(tokens, '// line comment', 'comment.line.double-slash'), 'line comment');
    assert.ok(tokens.some(t => t.text.includes('block') && t.scopes.includes('comment.block.goose')), 'block comment');
    assert.ok(tokens.some(t => t.scopes.includes('string.quoted.double.goose')), 'double string');
    assert.ok(tokens.some(t => t.scopes.includes('string.quoted.triple.goose')), 'raw string');
    assert.ok(has(tokens, '123', 'constant.numeric.integer'), 'integer');
    assert.ok(has(tokens, '0x2a', 'constant.numeric.integer.hex'), 'hex integer');
    assert.ok(has(tokens, '1.5', 'constant.numeric.float'), 'float');
    assert.ok(has(tokens, '->', 'keyword.operator'), 'arrow operator');
    assert.ok(has(tokens, '!=', 'keyword.operator'), 'not-equal operator');
});

check('members, namespaces, constants', tokens => {
    assert.ok(has(tokens, 'field', 'variable.other.property'), 'member access');
    assert.ok(has(tokens, 'len', 'support.variable.property'), 'len property');
    assert.ok(has(tokens, 'MAX_SIZE', 'constant.other'), 'constant');
    assert.ok(has(tokens, 'json', 'entity.name.namespace'), 'namespace qualifier');
});

const sample = `// line comment
/* block */
import std;
namespace json;
struct Number { value: f64, int: i64 }
enum Value { Null, Bool }
const MAX_SIZE: i64 = 123;
export fn add(a: i64, b: i64) -> i64 {
    var total: i64 = 0x2a;
    let ratio = 1.5;
    if a != b { return self.value.field + self.len } else { return total + 123 }
    while total < 10 { total += 1 }
    for i in 0..10 { total = total + i }
    let flag = true;
    let text = "hi\\n";
    let raw = """raw""";
    return json::parse(text) + MAX_SIZE;
}
`;

const realFiles = [
    'goose/jinja/lexer.goose',
    'goose/jinja/parser.goose',
    'goose/jinja/runtime.goose',
];

(async () => {
    const grammar = await loadGrammar();

    const tokens = tokenize(grammar, sample);
    for (const [name, fn] of checks) {
        fn(tokens);
        console.log(`ok - ${name}`);
    }

    for (const relative of realFiles) {
        const file = path.join(repo, relative);
        const source = fs.readFileSync(file, 'utf8');
        const real = tokenize(grammar, source);
        const counts = new Map();
        for (const t of real) {
            const scope = t.scopes[1] || t.scopes[0];
            counts.set(scope, (counts.get(scope) || 0) + 1);
        }
        const fnCount = [...counts].filter(([s]) => s.startsWith('storage.type.function')).reduce((a, [, n]) => a + n, 0);
        const commentCount = [...counts].filter(([s]) => s.startsWith('comment.')).reduce((a, [, n]) => a + n, 0);
        assert.ok(fnCount > 0, `${relative}: no fn tokens`);
        assert.ok(commentCount > 0, `${relative}: no comment tokens`);
        assert.ok(real.every(t => !t.scopes.includes('invalid.illegal')), `${relative}: invalid scope emitted`);
        console.log(`ok - ${relative}: ${real.length} tokens, ${counts.size} scopes`);
    }

    console.log('grammar tests passed');
})().catch(error => {
    console.error(error);
    process.exit(1);
});
