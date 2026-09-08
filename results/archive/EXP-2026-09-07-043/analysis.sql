-- EXP-2026-09-07-043 window analysis of the EXP-043 diag profile (STAGE2).
-- Read-only queries over the Nsight sqlite export (exp043.sqlite, 433053 events).
-- Timestamps are ns since profiler session start; single clock domain.
-- Window bounds come from the expert_prefill NVTX ranges placed in server-context.cpp:
--   warmup  window: 106820483305 .. 125442245632 (18621.762 ms, not analyzed in depth)
--   measured window (WS..WE): 150407570245 .. 152980230160 (2572.660 ms, 395 tokens,
--   cache_prompt=false, n_predict=0; server timings: prompt eval 2573.15 ms)

-- 0. Window sanity: NVTX interval counts inside the measured window
SELECT 'gather_n' AS item, COUNT(*) AS v FROM NVTX_EVENTS WHERE text='expert_gather' AND start>=150407570245 AND end<=152980230160
UNION ALL SELECT 'submit_n', COUNT(*) FROM NVTX_EVENTS WHERE text='expert_submit' AND start>=150407570245 AND end<=152980230160
UNION ALL SELECT 'slotwait_n', COUNT(*) FROM NVTX_EVENTS WHERE text='expert_slotwait' AND start>=150407570245 AND end<=152980230160;

-- 1. CPU-side interval stats inside the measured window: gather (with percentiles)
WITH g AS (SELECT end-start d FROM NVTX_EVENTS WHERE text='expert_gather' AND start>=150407570245 AND end<=152980230160),
r AS (SELECT d, ROW_NUMBER() OVER (ORDER BY d) rn, COUNT(*) OVER () n FROM g)
SELECT 'expert_gather' AS what, n AS cnt, printf('%.3f', SUM(d)/1e6) AS sum_ms, printf('%.6f', AVG(d)/1e3) AS mean_us,
       printf('%.3f', MAX(d)/1e6) AS max_ms, printf('%.6f', MAX(CASE WHEN rn=(n+1)/2 THEN d END)/1e3) AS p50_us,
       printf('%.6f', MAX(CASE WHEN rn=CAST(0.99*n AS INTEGER)+1 THEN d END)/1e3) AS p99_us
FROM r GROUP BY n;

-- 1b. submit / slotwait sums
SELECT 'expert_submit' AS what, COUNT(*) cnt, printf('%.3f', SUM(end-start)/1e6) sum_ms, printf('%.6f', AVG(end-start)/1e3) mean_us, printf('%.3f', MAX(end-start)/1e6) max_ms
FROM NVTX_EVENTS WHERE text='expert_submit' AND start>=150407570245 AND end<=152980230160;
SELECT 'expert_slotwait' AS what, COUNT(*) cnt, printf('%.3f', SUM(end-start)/1e6) sum_ms, printf('%.6f', AVG(end-start)/1e3) mean_us, printf('%.3f', MAX(end-start)/1e6) max_ms
FROM NVTX_EVENTS WHERE text='expert_slotwait' AND start>=150407570245 AND end<=152980230160;

-- 2. Copies by kind inside the measured window
SELECT copyKind, COUNT(*) n, SUM(bytes) bytes, printf('%.2f', SUM(bytes)/1048576.0) mib
FROM CUPTI_ACTIVITY_KIND_MEMCPY
WHERE start >= 150407570245 AND end <= 152980230160
GROUP BY copyKind ORDER BY copyKind;

-- 3. GPU activity sums inside the measured window
SELECT 'gpu_h2d_sum_ms' item, printf('%.3f', SUM(end-start)/1e6) v FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=150407570245 AND end<=152980230160
UNION ALL SELECT 'gpu_dtoh_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=2 AND start>=150407570245 AND end<=152980230160
UNION ALL SELECT 'gpu_dtod_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=8 AND start>=150407570245 AND end<=152980230160
UNION ALL SELECT 'kernel_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=150407570245 AND end<=152980230160
UNION ALL SELECT 'memset_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start>=150407570245 AND end<=152980230160;

-- 4. GPU busy unions and idle gaps inside the measured window (memcpy+kernel+memset)
WITH act AS (
  SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=150407570245 AND end<=152980230160
  UNION ALL SELECT start, end FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=150407570245 AND end<=152980230160
  UNION ALL SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start>=150407570245 AND end<=152980230160
), g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM act),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e, LAG(MAX(end)) OVER (ORDER BY MIN(start)) prev_e FROM b GROUP BY gid)
SELECT COUNT(*) busy_segments, printf('%.3f', SUM(e-s)/1e6) gpu_busy_union_ms,
       printf('%.3f', 2572.660 - SUM(e-s)/1e6) window_minus_busy_ms,
       SUM(CASE WHEN prev_e IS NOT NULL AND s-prev_e > 1000000 THEN 1 ELSE 0 END) gaps_gt_1ms,
       SUM(CASE WHEN prev_e IS NOT NULL AND s-prev_e > 100000 THEN 1 ELSE 0 END) gaps_gt_100us,
       printf('%.3f', MAX(CASE WHEN prev_e IS NOT NULL THEN s-prev_e ELSE 0 END)/1e6) max_gap_ms
FROM u;

-- 5. H2D-only union and kernel-only union inside the measured window
WITH c AS (SELECT start,end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=150407570245 AND end<=152980230160),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM c),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'gpu_h2d_union_ms' item, printf('%.3f', SUM(e-s)/1e6) v,
       printf('%.2f', (SELECT SUM(bytes) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=150407570245 AND end<=152980230160) / 1048576.0 / (SUM(e-s)/1e9)) AS achieved_MiBps
FROM u;
WITH k AS (SELECT start,end FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=150407570245 AND end<=152980230160),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM k),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'kernel_union_ms' item, printf('%.3f', SUM(e-s)/1e6) v FROM u;

-- 6. Real H2D/kernel intersection (sweep with running counts)
WITH ev AS (
  SELECT start t, 1 dh, 0 dk FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=150407570245 AND end<=152980230160
  UNION ALL SELECT end, -1, 0 FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=150407570245 AND end<=152980230160
  UNION ALL SELECT start, 0, 1 FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=150407570245 AND end<=152980230160
  UNION ALL SELECT end, 0, -1 FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=150407570245 AND end<=152980230160
), cum AS (
  SELECT t, SUM(dh) OVER (ORDER BY t ROWS UNBOUNDED PRECEDING) ch,
           SUM(dk) OVER (ORDER BY t ROWS UNBOUNDED PRECEDING) ck,
           LEAD(t) OVER (ORDER BY t) tn FROM ev
)
SELECT 'h2d_kernel_intersect_ms' item, printf('%.3f', SUM(tn-t)/1e6) v FROM cum WHERE tn IS NOT NULL AND ch>0 AND ck>0;

-- 7. Host CUDA API inside the window: top names by summed duration
WITH a AS (
  SELECT s.value AS name, r.start, r.end
  FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id = r.nameId
  WHERE r.start >= 150407570245 AND r.end <= 152980230160 + 200000000
)
SELECT name, COUNT(*) n, printf('%.3f', SUM(end-start)/1e6) sum_ms, printf('%.6f', AVG(end-start)/1e3) mean_us
FROM a GROUP BY name ORDER BY sum_ms DESC LIMIT 12;

-- 8. Synchronization rows inside the window
SELECT syncType, COUNT(*) n, printf('%.3f', SUM(end-start)/1e6) sum_ms, printf('%.3f', MAX(end-start)/1e6) max_ms
FROM CUPTI_ACTIVITY_KIND_SYNCHRONIZATION
WHERE start >= 150407570245 AND end <= 152980230160 + 200000000
GROUP BY syncType;

-- 9. CPU gather union inside the measured window (are gathers contiguous on the CPU?)
WITH a AS (SELECT start,end FROM NVTX_EVENTS WHERE text='expert_gather' AND start>=150407570245 AND end<=152980230160),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM a),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'gather_union_ms' item, printf('%.3f', SUM(e-s)/1e6) v, printf('%.3f', 2572.660 - SUM(e-s)/1e6) window_minus_gather_ms FROM u;

-- 10. Top kernels by summed duration inside the window
SELECT substr(s.value,1,56) kernel, COUNT(*) n, printf('%.3f', SUM(k.end-k.start)/1e6) sum_ms
FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id = k.demangledName
WHERE k.start>=150407570245 AND k.end<=152980230160
GROUP BY s.value ORDER BY SUM(k.end-k.start) DESC LIMIT 8;

-- 11. Warmup window compact summary (sanity: same request, cold first pass)
SELECT 'warm_gather_sum_ms' item, printf('%.3f', SUM(end-start)/1e6) v FROM NVTX_EVENTS WHERE text='expert_gather' AND start>=106820483305 AND end<=125442245632
UNION ALL SELECT 'warm_submit_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM NVTX_EVENTS WHERE text='expert_submit' AND start>=106820483305 AND end<=125442245632
UNION ALL SELECT 'warm_slotwait_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM NVTX_EVENTS WHERE text='expert_slotwait' AND start>=106820483305 AND end<=125442245632
UNION ALL SELECT 'warm_h2d_n', COUNT(*) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=106820483305 AND end<=125442245632
UNION ALL SELECT 'warm_h2d_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=106820483305 AND end<=125442245632
UNION ALL SELECT 'warm_kernel_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=106820483305 AND end<=125442245632;
