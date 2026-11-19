# The cheap verification set

Four checks that can be run after any change to the shipped library, in about a minute, with no model and no GPU. They exist
because the expensive question - does the runtime still answer correctly at speed - is only worth asking after these pass.

```bash
python3 tools/check-accepted-components.py builds/flint-acc/bin   # every accepted component still in the library
bash tools/json-door-check.sh                                    # the composed JSON parser answers
bash tools/json-roundtrip-check.sh                               # parse and dump round-trip the canonical text
bash tools/model-manager-doors-check.sh                          # status, source, stop, remove, unload
```

Measured on 2026-11-19 after composing the model manager, the JSON parser and the JSON dumper into the unit:

```
check-accepted-components: ok, every accepted component is in the shipped library
PASS: the ported parser is composed and answers on the shipped library
PASS: both units round-trip the canonical text, key order included
PASS: status, source, stop, remove and unload all behave as the module states
```

What each one would have caught, from this project's own history: the component check caught the host-registration loss and the
unloadable library; the door checks catch a door that is compiled but not exporting, which is the failure mode of a stale
generated unit; the round trip catches a parse or dump that drifts from the canonical text.

Not covered here, and deliberately: anything that needs a model. Those questions are answered by `tools/apex-test.sh` on a quiet
card and by the AWT, not by this set.
