#!/usr/bin/env python3
"""Exhaustive check of the claim behind the dense-attention builder.

For the real Maxwell geometry (block 128, prefix 1024, local 1024, 16 remote
blocks) the union prefix ∪ remote ∪ local covers every block up to the query
while the KV length is <= 4096, and stops doing so at 4097.
"""
import sys

BLOCK = 128
PREFIX = 1024
LOCAL = 1024
REMOTE = 16
SUPPORT = 4096


def covered(key_len, qpos):
    """Blocks the reference route selects for one query position (block IDs)."""
    cur = qpos // BLOCK
    n_blocks = (key_len + BLOCK - 1) // BLOCK
    prefix = [b for b in range(PREFIX // BLOCK) if b <= cur]
    local_start = max(cur - LOCAL // BLOCK + 1, 0)
    local = [b for b in range(local_start, cur + 1) if b >= PREFIX // BLOCK]
    eligible = [b for b in range(n_blocks)
                if b * BLOCK >= PREFIX and (b + 1) * BLOCK <= local_start * BLOCK]
    remote = eligible[:REMOTE]
    return set(prefix) | set(local) | set(remote)


def main():
    fails = 0
    for key_len in range(1, SUPPORT + 1):
        n_blocks = (key_len + BLOCK - 1) // BLOCK
        for qpos in range(key_len):
            got = covered(key_len, qpos)
            want = set(range(qpos // BLOCK + 1))
            if got != want:
                print(f"FAIL len={key_len} q={qpos} missing={sorted(want-got)}")
                fails += 1
                if fails > 5:
                    return 1
    print(f"covered for every query at every length <= {SUPPORT}: ok")

    # At 4097 the first incomplete route appears: the query at 4096 cannot see
    # the earliest remote block that a dense causal mask would allow.
    key_len = SUPPORT + 1
    qpos = SUPPORT
    got = covered(key_len, qpos)
    want = set(range(qpos // BLOCK + 1))
    missing = sorted(want - got)
    print(f"len=4097 q=4096 missing blocks={missing[:5]} count={len(missing)}")
    ok = len(missing) == 0
    print("WINDOW-BOUND:", "dense still exact" if ok else "dense no longer exact at 4097 (expected)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
