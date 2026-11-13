# Isolated DeepSeek safety bundle

This is an additional safety mechanism,not a new migration goal. The existing Command Code skill/factory is unchanged. It reuses its installed CLI discovery and transport authorization only. No worker or autonomous migration is started by prepare or selftest.

## Exact host entry points

Bundle:`/home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108`

Controller:`.../trusted/guard.py`;system instructions:`.../trusted/SYSTEM_INSTRUCTIONS.md`.

1. Verify from a HOST terminal:

```
python3 /home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108/trusted/guard.py verify
python3 /home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108/trusted/guard.py selftest
```

2. Start the independent controller in one HOST terminal. This command is intentionally not accessible to the sandbox worker:

```
python3 /home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108/trusted/guard.py supervise
```

3. In another HOST terminal,manually launch Command Code inside the namespace,then paste Igor's startup prompt. No host skills or hidden instructions are imported:

```
python3 /home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108/trusted/guard.py agent -- --model deepseek/deepseek-v4.1-flash --permission-mode yolo --safe-mode --no-skills --no-auto-update --skip-onboarding --trust
```

The explicit CLI auto-permission mode applies only inside the tested OS namespace;it does not grant host writes or checker authority. Never use that mode in a normal host session.

Do not start a normal unsandboxed Command Code and assume these protections apply. CLI authentication lives only in the namespace's ephemeral home. It is not copied into this bundle or logs. It remains readable to the authorized CLI's own process/tools;this implementation is not a credential-hiding API proxy. Network is shared for the provider;network-service attacks are not prevented by filesystem namespaces. Do not grant unrelated credentials or expose privileged host network services.

4. Candidate requests use `/guard/request.py` INSIDE the sandbox;inspect corresponding HOST `results/ID.json`,per-stage logs and the append-only/hash-chained `audit.jsonl`. `accepted` is always false. Local and integration candidates remain review-only. No main/prod remote or automatic binary installation exists.

5. Check status or recover in a HOST terminal:

```
python3 /home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108/trusted/guard.py status
python3 /home/end0/AI/LlamaServer/franken-goose-deepseek-autonomy-20261108/trusted/guard.py restore FULL_40_CHARACTER_CHECKPOINT_HASH
```

Restoration creates a new directory under `restorations/`;it does not reset/delete/overwrite a prior result. Full source consists of the sealed reference plus checkpoint Goose modules. Preserve the whole bundle,including `checkpoints/history.git`,`state.json`,`sealed.json` and audit/results. A failure never updates last-success pointers. Restart only the host controller after inspecting state and confirming no live check;three consecutive failures halt admission. No agent-accessible resume/unhalt/promotion command exists.

## Scope and honest limits

The ready independent recipe is the exact EXP174 SET_ROWS ABI/native oracle. It cannot establish correctness of new GGUF/server/MoE/MMQ subsystems merely because they compile. New source can be typechecked and checkpointed as drafts,but integration progression is blocked until Codex adds a matching independent frozen recipe. This is deliberate fail-closed behavior,not full autonomous final acceptance. The remaining Apex requirements are unchanged in the existing plan.

Existing scoped tests are reused. Additional tests cover the actual isolation/controller gap. CPU uninitialized-memory detection for arbitrary new C++/Goose,all possible CUDA races/failures,full GPU-driver robustness and general allocation-capacity ownership are not universally certified. Failed/inconclusive/missing coverage never becomes TRUSTED. The complete guard is outside the worker's writable mounts;another ordinary host process with the same Unix account can still change it. This is process isolation,not protection against a hostile host owner or kernel escape.

There is no delegated cgroup/filesystem quota in this setup. Core dumps are disabled,individual files/descriptors and command durations are bounded;RAM/VRAM/swap are recorded at integration. Aggregate agent/fork/disk/VRAM resource exhaustion is not a hard guarantee. Use only one agent,keep free disk/RAM and stop on pressure. No RAM/VRAM quota or GPU virtualization is claimed. ASan needs a huge virtual address range,so an RLIMIT_AS pretending to cap resident memory would break the checker and is intentionally not used.

No hardware isolation protects other desktop GPU users from a driver/kernel hang. Build/test processes are network-isolated,host readonly,private-PID,with a fresh writable candidate copy. The agent itself has no GPU device. Final acceptance remains with Codex/Igor even after automated success. The absence of a promotion interface is the strongest protection against unnoticed working-version regressions.

Do not edit sealed inputs to make failures pass. A human/Codex extension needs a new bundle/version and new fault-injection report. There is no automatic root-level reset or cleanup. Retain failed stages. Do not commit private bundle credentials,model weights,requests,responses or local COMPILER_FEEDBACK.md.

The final preparation did not run a second full Apex inside this new controller sandbox. Raw-token/probability verification was tested against the actual completed EXP174 pair,including deliberate sixth-token and probability corruption. The first significant integration must still pass its fresh controlled Apex gate;local qualification never substitutes for it.
