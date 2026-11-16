# JSON Schema -> GBNF: native expectation pairs

All expected strings below are copied verbatim from the native test corpus
`upstream/llama.cpp/tests/test-json-schema-to-grammar.cpp` (leading indentation
of the raw string literal trimmed). The native implementation under test is
`upstream/llama.cpp/common/json-schema-to-grammar.cpp`.

The draft covers pairs 1-12. Pairs 1-6 are the primitive/const/enum path, 7-9
the object path (required and optional chains), 10-11 the array repetition
path, and 12 the empty-schema path.

## 1. Empty schema becomes the object primitive

Schema: `{}`

```
array ::= "[" space ( value ("," space value)* )? space "]"
boolean ::= ("true" | "false")
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
null ::= "null"
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
object ::= "{" space ( string ":" space value ("," space string ":" space value)* )? space "}"
root ::= object
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
value ::= object | array | string | number | boolean | null
```

Source: test `empty schema (object)`, lines 331-349.

## 2. Boolean

Schema: `{"type": "boolean"}`

```
root ::= ("true" | "false")
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `boolean`, lines 448-458.

## 3. Integer

Schema: `{"type": "integer"}`

```
integral-part ::= [0] | [1-9] [0-9]{0,15}
root ::= ("-"? integral-part)
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `integer`, lines 460-472.

## 4. Number

Schema: `{"type": "number"}`

```
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
root ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `number`, lines 621-634.

## 5. String const

Schema: `{"const": "foo"}`

```
root ::= "\"foo\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `string const`, lines 473-484.

## 6. Non-string const

Schema: `{"const": 123}`

```
root ::= "123"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `non-string const`, lines 485-496.

## 7. Non-string enum

Schema: `{"enum": ["red", "amber", "green", null, 42, ["foo"]]}`

```
root ::= ("\"red\"" | "\"amber\"" | "\"green\"" | "null" | "42" | "[\"foo\"]")
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `non-string enum`, lines 497-508.

## 8. Required properties in document order

Schema:

```json
{
  "type": "object",
  "properties": {
    "b": {"type": "string"},
    "c": {"type": "string"},
    "a": {"type": "string"}
  },
  "required": ["a", "b", "c"],
  "additionalProperties": false,
  "definitions": {}
}
```

```
a-kv ::= "\"a\"" space ":" space string
b-kv ::= "\"b\"" space ":" space string
c-kv ::= "\"c\"" space ":" space string
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "{" space b-kv "," space c-kv "," space a-kv space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `required props in original order`, lines 825-853. The required
chain follows the `properties` order (b, c, a), not the `required` array order.

## 9. One optional property

Schema:

```json
{
  "properties": {"a": {"type": "string"}},
  "additionalProperties": false
}
```

```
a-kv ::= "\"a\"" space ":" space string
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "{" space  (a-kv )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `1 optional prop`, lines 854-873. Note the two spaces after
`space`: the object prefix `"{" space ` and the optional group ` (`.

## 10. N optional properties and the recursive rest rules

Schema:

```json
{
  "properties": {
    "a": {"type": "string"},
    "b": {"type": "string"},
    "c": {"type": "string"}
  },
  "additionalProperties": false
}
```

```
a-kv ::= "\"a\"" space ":" space string
a-rest ::= ( "," space b-kv )? b-rest
b-kv ::= "\"b\"" space ":" space string
b-rest ::= ( "," space c-kv )?
c-kv ::= "\"c\"" space ":" space string
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "{" space  (a-kv a-rest | b-kv b-rest | c-kv )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `N optional props`, lines 874-897.

## 11. minItems uses the separator repetition

Schema:

```json
{
  "items": {"type": "boolean"},
  "minItems": 2
}
```

```
boolean ::= ("true" | "false")
root ::= "[" space boolean ("," space boolean)+ space "]"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `minItems`, lines 635-650.

## 12. maxItems 2 uses the optional repetition

Schema:

```json
{
  "items": {"type": "boolean"},
  "maxItems": 2
}
```

```
boolean ::= ("true" | "false")
root ::= "[" space (boolean ("," space boolean)?)? space "]"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `maxItems 2`, lines 683-698.

## Out-of-scope pairs (not in the draft)

The same corpus also fixes the expected text for `minimum`/`maximum`
(lines 87-312), `pattern`/`format` (lines 351-446, 758-824), tuples
(`prefixItems` array, lines 541-571), `anyOf`/`oneOf` (lines 1184-1254),
`allOf` (lines 1255-1343) and `$ref` (lines 1344-1411). Those are covered by
`bounds.goose`, `pattern.goose`, `visit.goose`, `allof.goose` and `refs.goose`
respectively; the draft rejects them through `meta[i*11+1]` instead of
approximating them.

## 13. Type array: nullable string array

Schema:

```json
{
  "type": ["array", "null"],
  "prefixItems": {"type": "string"}
}
```

```
alternative-0 ::= "[" space (string ("," space string)*)? space "]"
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
null ::= "null"
root ::= alternative-0 | null
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `nullable string array`, lines 524-540. Native expands the type
array into one schema copy per type and joins the copies as a union (cpp
926-934); the alternative names are `alternative-<i>` for the empty root name.

## 14. anyOf (oneOf uses the same branch)

Schema:

```json
{
  "anyOf": [
    {"$ref": "#/definitions/foo"},
    {"$ref": "#/definitions/bar"}
  ],
  "definitions": {
    "foo": {"properties": {"a": {"type": "number"}}},
    "bar": {"properties": {"b": {"type": "number"}}}
  },
  "type": "object"
}
```

```
alternative-0 ::= ref-definitions-foo
alternative-1 ::= ref-definitions-bar
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
ref-definitions-bar ::= "{" space  (ref-definitions-bar-b-kv )? space "}"
ref-definitions-bar-b-kv ::= "\"b\"" space ":" space number
ref-definitions-foo ::= "{" space  (ref-definitions-foo-a-kv )? space "}"
ref-definitions-foo-a-kv ::= "\"a\"" space ":" space number
root ::= alternative-0 | alternative-1
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `anyOf`, lines 1185-1216. `oneOf` has no grammar case in this
corpus; native selects it with the same code path (`schema.contains("oneOf") ?
schema.at("oneOf") : schema.at("anyOf")`, cpp 918-924), so this text is the
expected output for the same alternatives under `oneOf`.

## 15. Top-level local $ref

Schema:

```json
{
  "$ref": "#/definitions/foo",
  "definitions": {
    "foo": {
      "type": "object",
      "properties": {"a": {"type": "string"}},
      "required": ["a"],
      "additionalProperties": false
    }
  }
}
```

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
ref-definitions-foo ::= "{" space ref-definitions-foo-a-kv space "}"
ref-definitions-foo-a-kv ::= "\"a\"" space ":" space string
root ::= ref-definitions-foo
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `top-level $ref`, lines 1155-1183.

## 16. Pattern: simple anchored regexp

Schema: `{"type": "string", "pattern": "^abc?d*efg+(hij)?kl$"}`

```
root ::= "\"" ("ab" "c"? "d"* "ef" "g"+ ("hij")? "kl") "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `simple regexp`, lines 758-770.

## 17. Pattern: escaped punctuation

Schema: `{"type": "string", "pattern": "^\\[\\]\\{\\}\\(\\)\\|\\+\\*\\?$"}`

```
root ::= "\"" ("[]{}()|+*?") "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `regexp escapes`, lines 771-783.

## 18. Pattern: quote

Schema: `{"type": "string", "pattern": "^\"$"}`

```
root ::= "\"" ("\"") "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `regexp quote`, lines 784-796.

## 19. Pattern: classes, braces, dot and a sub-rule

Schema:

```json
{
  "type": "string",
  "pattern": "^(?:\\([0-9]{1,3}\\))?[0-9]{3}-[0-9]{4} a{3,5}nd...$"
}
```

```
dot ::= [^\x0A\x0D]
root ::= "\"" (("(" root-1{1,3} ")")? root-1{3,3} "-" root-1{4,4} " " "a"{3,5} "nd" dot dot dot) "\""
root-1 ::= [0-9]
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `regexp`, lines 810-823. The first sub-rule gets suffix 1 because
native's `sub_rule_ids[sub]` inserts the map key before it reads `size()`
(cpp 564-568); the Python reference uses `len(...) + 1`.

## 20. Pattern: non-capturing group

Schema: `{"type": "string", "pattern": "^(?:foo|bar)baz$"}`

```
root ::= "\"" (("foo" | "bar") "baz") "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: C++-only test `regexp with non-capturing group`, lines 1543-1554.

## 21. Pattern: unanchored falls back to string

Schema: `{"type": "string", "pattern": "[0-9]+"}`

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= string
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: C++-only test `unanchored regexp`, lines 1569-1581. An unsupported
pattern (`\w`, escaped `-` in a class, lookaround) uses the same fallback and
must not leak its partial `root-<n>` rules; see lines 1585-1600.

## 22. minLength 1

Schema: `{"type": "string", "minLength": 1}`

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "\"" char+ "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `string w/ min length 1`, lines 391-403.

## 23. maxLength 3

Schema: `{"type": "string", "maxLength": 3}`

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "\"" char{0,3} "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `string w/ max length`, lines 419-431.

## 24. minLength 1 and maxLength 4

Schema: `{"type": "string", "minLength": 1, "maxLength": 4}`

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "\"" char{1,4} "\""
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `string w/ min & max length`, lines 432-447.

## 25. Integer minimum 3

Schema: `{"type": "integer", "minimum": 3}`

```
root ::= ([1-2] [0-9]{1,15} | [3-9] [0-9]{0,15})
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `min 3`, lines 113-125.

## 26. Integer maximum 30

Schema: `{"type": "integer", "maximum": 30}`

```
root ::= ("-" [1-9] [0-9]{0,15} | [0-9] | ([1-2] [0-9] | [3] "0"))
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `max 30`, lines 165-177.

## 27. Integer minimum 0 and maximum 23

Schema: `{"type": "integer", "minimum": 0, "maximum": 23}`

```
root ::= ([0-9] | ([1] [0-9] | [2] [0-3]))
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `min 0 max 23`, lines 243-256.

## 28. Integer minimum -5

Schema: `{"type": "integer", "minimum": -5}`

```
root ::= ("-" ([0-5]) | [0] | [1-9] [0-9]{0,15})
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `min -5`, lines 178-190. The exclusive variants shift the bound by
one in cpp 1057-1064 and are covered by the same rule builder.

## 29. allOf: required object merge plus optional anyOf alternatives

Schema:

```json
{
  "allOf": [
    {"$ref": "#/definitions/foo"},
    {"$ref": "#/definitions/bar"},
    {"anyOf": [
      {"$ref": "#/definitions/baz"},
      {"$ref": "#/definitions/bam"}
    ]}
  ],
  "definitions": {
    "foo": {"properties": {"a": {"type": "number"}}},
    "bar": {"properties": {"b": {"type": "number"}}},
    "bam": {"properties": {"c": {"type": "number"}}},
    "baz": {"properties": {"d": {"type": "number"}}}
  },
  "type": "object"
}
```

```
a-kv ::= "\"a\"" space ":" space number
b-kv ::= "\"b\"" space ":" space number
c-kv ::= "\"c\"" space ":" space number
d-kv ::= "\"d\"" space ":" space number
d-rest ::= ( "," space c-kv )?
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "{" space a-kv "," space b-kv ( "," space ( d-kv d-rest | c-kv ) )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `mix of allOf, anyOf and $ref`, lines 1256-1299.

## 30. allOf with one enum schema

Schema:

```json
{
  "allOf": [{"$ref": "#/definitions/foo"}],
  "definitions": {"foo": {"type": "string", "enum": ["a", "b"]}}
}
```

```
root ::= ("\"a\"" | "\"b\"")
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `allOf with enum schema`, lines 1300-1319.

## 31. allOf with two enum schemas

Schema:

```json
{
  "allOf": [
    {"$ref": "#/definitions/foo"},
    {"$ref": "#/definitions/bar"}
  ],
  "definitions": {
    "foo": {"type": "string", "enum": ["a", "b", "c"]},
    "bar": {"type": "string", "enum": ["b", "c", "d"]}
  }
}
```

```
root ::= ("\"b\"" | "\"c\"")
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `allOf with multiple enum schemas`, lines 1320-1343. The
intersection is the constants whose occurrence count equals the number of
top-level `allOf` components (cpp 1003-1012).

## 32. anyOf with a $ref to an alternative inside the same document

Schema:

```json
{
  "properties": {
    "a": {
      "anyOf": [
        {"type": "string"},
        {"type": "number"}
      ]
    },
    "b": {
      "anyOf": [
        {"$ref": "#/properties/a/anyOf/0"},
        {"type": "boolean"}
      ]
    }
  },
  "type": "object"
}
```

```
a ::= string | number
a-kv ::= "\"a\"" space ":" space a
a-rest ::= ( "," space b-kv )?
b ::= b-0 | boolean
b-0 ::= string
b-kv ::= "\"b\"" space ":" space b
boolean ::= ("true" | "false")
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "{" space  (a-kv a-rest | b-kv )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `anyOf $ref`, lines 1219-1253. This pair resolves CASE-NOTE-1:
`_resolve_ref` returns the name that `visit` returned for the target, and a
non-root string target returns the shared primitive name `string`, so the
alias is `b-0 ::= string` and no `ref-properties-...` rule appears.

## 33. prefixItems tuple with one item

Schema: `{"prefixItems": [{"type": "string"}]}`

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
root ::= "[" space string space "]"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `tuple1`, lines 541-554.

## 34. prefixItems tuple with two items

Schema: `{"prefixItems": [{"type": "string"}, {"type": "number"}]}`

```
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "[" space string "," space number space "]"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `tuple2`, lines 555-571.

## 35. items array tuple with date, uuid, time and date-time

Schema:

```json
{
  "items": [
    {"format": "date"},
    {"format": "uuid"},
    {"format": "time"},
    {"format": "date-time"}
  ]
}
```

```
date ::= [0-9]{4} "-" ( "0" [1-9] | "1" [0-2] ) "-" ( "0" [1-9] | [1-2] [0-9] | "3" [0-1] )
date-string ::= "\"" date "\""
date-time ::= date "T" time
date-time-string ::= "\"" date-time "\""
root ::= "[" space tuple-0 "," space uuid "," space tuple-2 "," space tuple-3 space "]"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
time ::= ([01] [0-9] | "2" [0-3]) ":" [0-5] [0-9] ":" [0-5] [0-9] ( "." [0-9]{3} )? ( "Z" | ( "+" | "-" ) ( [01] [0-9] | "2" [0-3] ) ":" [0-5] [0-9] )
time-string ::= "\"" time "\""
tuple-0 ::= date-string
tuple-2 ::= time-string
tuple-3 ::= date-time-string
uuid ::= "\"" [0-9a-fA-F]{8} "-" [0-9a-fA-F]{4} "-" [0-9a-fA-F]{4} "-" [0-9a-fA-F]{4} "-" [0-9a-fA-F]{12} "\""
```

Source: test `exotic formats`, lines 351-377. This is also the native
`format` coverage: `date`, `uuid`, `time`, `date-time` are the recognized
formats (cpp 249-256 and 1039-1045). `uuid1`..`uuid5` use the same uuid rule
under their own name (cpp 1039-1041). Unknown formats (`email`, `uri`,
`hostname`, ...) are ignored and fall through to the type handling; no error and
no rule is emitted for them.

## 36. items wins over prefixItems

Schema:

```json
{
  "type": "array",
  "items": {},
  "prefixItems": {"type": "string"}
}
```

```
array ::= "[" space ( value ("," space value)* )? space "]"
boolean ::= ("true" | "false")
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
item ::= object
null ::= "null"
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
object ::= "{" space ( string ":" space value ("," space string ":" space value)* )? space "}"
root ::= "[" space (item ("," space item)*)? space "]"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
value ::= object | array | string | number | boolean | null
```

Source: test `array with empty items and prefixItems`, lines 596-620. Native
selects `items` first (`schema.contains("items") ? ... : prefixItems`, cpp
1017), so `prefixItems` is ignored here.

Tuple has no `minItems`/`maxItems` interaction: the tuple branch returns before
the repetition path (cpp 1016-1028), so both bounds are ignored for a tuple.
There is no native test for that combination; this is code-derived.

## 37. additionalProperties schema

Schema:

```json
{
  "type": "object",
  "additionalProperties": {"type": "array", "items": {"type": "number"}}
}
```

```
additional-kv ::= string ":" space additional-value
additional-value ::= "[" space (number ("," space number)*)? space "]"
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "{" space  (additional-kv ( "," space additional-kv )* )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `additional props`, lines 924-943. With no named properties the
key rule is the `string` primitive, the value is the visited additional schema,
and the `*` branch is the only optional branch.

## 38. additionalProperties true

Schema: `{"type": "object", "additionalProperties": true}`

```
array ::= "[" space ( value ("," space value)* )? space "]"
boolean ::= ("true" | "false")
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
null ::= "null"
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
object ::= "{" space ( string ":" space value ("," space string ":" space value)* )? space "}"
root ::= object
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
value ::= object | array | string | number | boolean | null
```

Source: test `additional props (true)`, lines 944-967. Native does not build
the additional rule when there are no `properties`, because the object branch
condition requires `additionalProperties != true` (cpp 945-947); it falls to
the object primitive. With properties present, `true` builds
`additional-kv` with value rule `value`.

## 39. required + additionalProperties schema

Schema:

```json
{
  "type": "object",
  "properties": {"a": {"type": "number"}},
  "required": ["a"],
  "additionalProperties": {"type": "string"}
}
```

```
a-kv ::= "\"a\"" space ":" space number
additional-k ::= ["] ( [a] char+ | [^"a] char* )? ["]
additional-kv ::= additional-k ":" space string
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "{" space a-kv ( "," space ( additional-kv ( "," space additional-kv )* ) )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
string ::= "\"" char* "\""
```

Source: test `required + additional props`, lines 1002-1026. `additional-k` is
the native `_not_strings` trie over the property names.

## 40. optional + additionalProperties schema

Schema:

```json
{
  "type": "object",
  "properties": {"a": {"type": "number"}},
  "additionalProperties": {"type": "number"}
}
```

```
a-kv ::= "\"a\"" space ":" space number
a-rest ::= ( "," space additional-kv )*
additional-k ::= ["] ( [a] char+ | [^"a] char* )? ["]
additional-kv ::= additional-k ":" space number
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "{" space  (a-kv a-rest | additional-kv ( "," space additional-kv )* )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `optional + additional props`, lines 1028-1052. The `*` branch is
appended after the named optional, and its rest rule is
`( "," space additional-kv )*`.

## 41. required + optional + additionalProperties schema

Schema:

```json
{
  "type": "object",
  "properties": {
    "and": {"type": "number"},
    "also": {"type": "number"}
  },
  "required": ["and"],
  "additionalProperties": {"type": "number"}
}
```

```
additional-k ::= ["] ( [a] ([l] ([s] ([o] char+ | [^"o] char*) | [^"s] char*) | [n] ([d] char+ | [^"d] char*) | [^"ln] char*) | [^"a] char* )? ["]
additional-kv ::= additional-k ":" space number
also-kv ::= "\"also\"" space ":" space number
also-rest ::= ( "," space additional-kv )*
and-kv ::= "\"and\"" space ":" space number
char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
decimal-part ::= [0-9]{1,16}
integral-part ::= [0] | [1-9] [0-9]{0,15}
number ::= ("-"? integral-part) ("." decimal-part)? ([eE] [-+]? integral-part)?
root ::= "{" space and-kv ( "," space ( also-kv also-rest | additional-kv ( "," space additional-kv )* ) )? space "}"
space ::= | " " | "\n"{1,2} [ \t]{0,20}
```

Source: test `required + optional + additional props`, lines 1053-1078. This
is the nested-trie case for `_not_strings`; the empty-name case (lines
1079-1103) and common-prefix case (lines 1129-1153) use the same trie.
