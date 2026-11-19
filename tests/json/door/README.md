# door

**Requirement:** P10-07 (replace or justify vendored executable dependencies) - the ported JSON
parser's door.
**Produced by:** `tools/json-door-check.sh`; entered the permanent tree on 2026-11-19.
**Tier:** fast - no model, no GPU. Needs the built library; skips (exit 77) without it.

## What it proves

Equality of the ported parser with nlohmann is already proved elsewhere (78 parse cases over
81 850 values, 188 dump cases). What this pins is that the door is composed and reachable on the
path the adapter will use:

- five valid texts parse with return 0 and each reports its own root kind
  (`7 6 5 2 0` for object, array, string, number, null);
- four invalid texts are refused with a negative return rather than silently accepted.

If the door is not composed, or a unit silently replaces it, this fails before the adapter work
starts rather than after.

## Run

```
bash tests/json/door/run.sh
```

Expected: `json door: valid=5 invalid=4 failures=0`, exit 0. The log records the library path, its
SHA-256 and the source revision.
