# EXP-2026-11-15-753 — right-sizing the context frees VRAM and buys decode at every length

Base: `00cf5e5a`. EXP-752 showed the limit is VRAM and each 256 MiB of expert cache is worth ~1.75 tok/s.
The context was reserving memory for tokens that are never generated.

## 1. The reserve

`-c 262144` reserves KV for 256K tokens. At q4_0 with 10 attention layers that is roughly **2880 MiB**
reserved, while the pinned workload uses 32768 prompt tokens plus 512 generated. Most of that reserve is
never touched, but it is subtracted from the budget the expert cache could use.

## 2. Results, reference argv otherwise, reference hash confirmed in every arm

| ctx | cache | prefill | decode @128 | @256 | @512 |
|---:|---:|---:|---:|---:|---:|
| 262144 | 4608 | 2428 | 38.68 | 40.25 | 41.04 |
| 131072 | 4608 | 2416 | - | - | 41.78 |
| 65536 | 4608 | 2434 | 40.17 | 41.55 | 42.29 |
| **65536** | **4992** | **2440** | **41.09** | **42.77** | **43.45** |

Hashes are unchanged at every length: `c2ff42cc` @512, `9d90a010` @128, `fcbac5fa` @256. So this is the
same computation, faster.

**Against the previous default (262144 / 4608), the final configuration is +6.2% @128, +6.3% @256, +5.9%
@512.** Prefill also improves slightly.

## 3. The frontier at ctx 65536

| cache | result |
|---:|---|
| 4608 | 42.29 @512 |
| 4864 | 43.09 @512 |
| **4992** | **43.45 / 42.92** (two independent runs) @512, 41.09 @128, 42.77 @256 |
| 5056 | **OOM** |
| 5120 | **OOM** (reproduced twice) |
| 5376 | **OOM** |

So 4992 is the real frontier at this context, and it reproduces across two full runs plus both other
lengths. The earlier impression that "5120 fails" was right; 4992 is the last step that fits.

## 4. The trade, stated plainly

| axis | 262144/4608 -> 65536/4992 |
|---|---|
| prefill | 2428 -> 2440 (**+0.5%**) |
| decode @512 | 41.04 -> **43.45** (**+5.9%**) |
| VRAM | 7804 -> 11735 MiB |
| **max context** | **262144 -> 65536 (-75%)** |

**This is not a free win.** It trades three quarters of the supported context for ~6% decode. It is the
right trade for the pinned benchmark workload (32768 in, 512 out), and the wrong trade for long-context
serving.

Both configurations are therefore recorded, and the choice is a single flag:

- `-c 65536 --moe-cache-mib 4992` - benchmark / short-context profile, 43.45 @512;
- `-c 262144 --moe-cache-mib 4608` - long-context profile, 41.04 @512, full 256K context preserved.

Which one becomes the default is a policy decision, not a measurement, so the default is left at the
long-context profile until Igor chooses. `run-apex.sh --ctx 65536 --cache 4992` reproduces the fast arm.

## 5. Why this had been invisible

Every previous sweep varied cache size at a fixed context, so the KV reserve was constant and could not
show up as a lever. And every previous context experiment was about *serving longer prompts*, not about
the cost of reserving for them. The two were measured separately and never traded against each other.

## 6. Cost

Nine configurations, twenty-seven runs, four OOM starts (5056, 5120 twice, 5376).
