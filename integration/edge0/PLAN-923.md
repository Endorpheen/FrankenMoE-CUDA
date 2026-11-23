# EXP923: reuse identical within-token group projections

**Historical reference.** This note records an Edge0 experiment from the Flint lineage. See [the model registry](../../docs/goose-migration/MODEL-ARCHITECTURES.md) for current qualification.

Reference EXP921, exact six-ELF checkpoint retained; EXP922 is rejected.
Scope: pinned Q4 DU pack,75/256,ctx512,t8,ngl48,MTPoff,normalEOS,all accepted
host caches unchanged. No Apex and no foreign process changes.

Ten selected experts use only eight group projections. For repeated group IDs
within one layer/token, D(x) and its inverse-scaled result are mathematically
and operationally identical. Build those CPU nodes once in Goose and reuse
handles, keeping all expert gate/up/nonlinearity/down/U and rank sum operations
unchanged. Never reuse between layers, tokens, bindings or requests.
With independent uniform groups, expected unique groups~5.90/10; actual routing
is UNKNOWN until measured. D is1.8432MB per expert out of~4.8512MB for the five
latent matrices. An indicative bandwidth estimate saves~15.6% latent traffic;
with measured CPU54.8% of decode an optimistic overall bound is~1.09x, not a
promise. Cache RAM/VRAM quotas unchanged; graph arena allocations shrink.

Goose retains all shape checks on reused branches. A disable-only control flag
FRANKEN_EDGE0_D_REUSE=0 assigns distinct projection identities. Correctness:
real top10/shared callback under both modes/rebind, fixed existing F32 tolerance,
repeat bits; all256 model token IDs and answer bytes equal frozen control.
Inspect actual CPU graph matvec node counts to establish the mechanism. One short
Russian EOS request. Serial performance A-B-B-A, four75/256 requests, up to8min.
Process swap0 and global swap-out delta0 are mandatory. Primary single-response
decode gain>=5%, no prefill/RAM/VRAM regression. Stop on failed parity/resource
or insufficient paired gain; do not select a fastest sample. No full rebuild.
