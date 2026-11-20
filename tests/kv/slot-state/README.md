# Apex slot state compatibility

This model fixture exercises the production `/slots/0` save, erase and restore operations under the Apex server lock. It uses the model profile, request and process lifecycle provided by `tools/apex-test.sh`.

Use one temporary directory as `APEX_SLOT_STATE_DIR` for three server starts and a single text request as `APEX_REQUEST`. Run `bash tests/kv/slot-state/run.sh` on the control with `APEX_SLOT_SAVE=control.bin`. On the candidate set `APEX_SLOT_RESTORE=control.bin` and `APEX_SLOT_SAVE=candidate.bin`. On the control set `APEX_SLOT_RESTORE=candidate.bin` and clear `APEX_SLOT_SAVE`. Use a distinct `APEX_SLOT_TAG` for every start.

Run `python3 tests/kv/slot-state/probe.py --compare <control-warm.json> <other-replay.json> ...` on the saved warm and replay responses from all three runs. The signature covers content, token IDs, saved probabilities, reused prefix and evaluated suffix. Save and restore must also report the full state size.

The fixture checks the slot API and serialized-state compatibility for P5-01 and P5-08. Its workload is for correctness qualification.
