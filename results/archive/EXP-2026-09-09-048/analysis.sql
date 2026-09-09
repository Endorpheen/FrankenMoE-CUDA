-- EXP-2026-09-09-048 offline analysis: composition of the CPU-expert verify-structure pass.
-- Read-only queries over the EXP-043 Nsight sqlite export (sha256
-- f86cd32ba4c05d611e448dd76d793b517233a5a4af934f8017f149ce48e34a2f, /tmp/exp043-profile/exp043.sqlite,
-- 433053 events). Opened strictly read-only: file:...?immutable=1. No server, model, or build runs.
--
-- Window under test: the 4-token trunk batch (batch 2 of the checkpoint break 391+4) from the
-- EXP-043 measured request. This pass is structurally the decode verify path: batch < 32 keeps
-- MUL_MAT_ID experts on the CPU (GGML_OP_OFFLOAD_MIN_BATCH, ggml-cuda.cu:5641), with per-layer
-- GPU router/dense clusters, per-layer D2H readbacks, and per-node stream syncs.
--
-- Bounds follow the EXP-043 tail attribution: SE = end of last expert_gather = 152762456149 ns;
-- batch 2 = SE+88.1 ms .. SE+217.6 ms  =>  W2 = [152850556149, 152980056149], length 129.5 ms.
-- Boundary uncertainty from the rounded card offsets is ~ +/-0.5 ms (0.4% of the window).

-- 0. Window length
SELECT 'W2_len_ms' AS item, printf('%.3f', (152980056149-152850556149)/1e6) AS v;

-- 1. GPU activity in W2: kernels, memset, memcpy by kind
SELECT 'kernels' AS what, COUNT(*) n, printf('%.3f', SUM(end-start)/1e6) sum_ms, printf('%.2f', AVG(end-start)/1e3) mean_us
FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=152850556149 AND end<=152980056149;
SELECT 'memset', COUNT(*), printf('%.3f', SUM(end-start)/1e6), printf('%.2f', AVG(end-start)/1e3)
FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start>=152850556149 AND end<=152980056149;
SELECT 'memcpy_kind|' || copyKind, COUNT(*), SUM(bytes), printf('%.3f', SUM(end-start)/1e6), printf('%.2f', AVG(end-start)/1e3)
FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=152850556149 AND end<=152980056149 GROUP BY copyKind;

-- 2. GPU busy union (kernel+memcpy+memset) and gaps in W2
WITH act AS (
  SELECT start, end FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=152850556149 AND end<=152980056149
  UNION ALL SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=152850556149 AND end<=152980056149
  UNION ALL SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start>=152850556149 AND end<=152980056149
), g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM act),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e, LAG(MAX(end)) OVER (ORDER BY MIN(start)) prev_e FROM b GROUP BY gid)
SELECT 'gpu_busy_union' AS what, COUNT(*) segs, printf('%.3f', SUM(e-s)/1e6) busy_ms,
       printf('%.3f', 129.5 - SUM(e-s)/1e6) idle_ms,
       SUM(CASE WHEN prev_e IS NOT NULL AND s-prev_e>100000 THEN 1 ELSE 0 END) gaps_gt_100us,
       printf('%.3f', MAX(CASE WHEN prev_e IS NOT NULL THEN s-prev_e ELSE 0 END)/1e6) max_gap_ms
FROM u;

-- 3. Host CUDA API rows in W2 (+0.2 ms tail)
SELECT substr(s.value,1,40) api, COUNT(*) n, printf('%.3f', SUM(r.end-r.start)/1e6) sum_ms, printf('%.2f', AVG(r.end-r.start)/1e3) mean_us
FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId
WHERE r.start>=152850556149 AND r.end<=152980056149+200000
GROUP BY s.value ORDER BY SUM(r.end-r.start) DESC;

-- 4. Synchronization rows in W2
SELECT syncType, COUNT(*), printf('%.3f', SUM(end-start)/1e6), printf('%.1f', AVG(end-start)/1e3)
FROM CUPTI_ACTIVITY_KIND_SYNCHRONIZATION
WHERE start>=152850556149 AND end<=152980056149+200000 GROUP BY syncType;

-- 5. Top GPU kernels in W2 (what the per-layer cluster consists of)
SELECT substr(s.value,1,58) kernel, COUNT(*) n, printf('%.3f', SUM(k.end-k.start)/1e6) sum_ms
FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.demangledName
WHERE k.start>=152850556149 AND k.end<=152980056149
GROUP BY s.value ORDER BY SUM(k.end-k.start) DESC LIMIT 14;
