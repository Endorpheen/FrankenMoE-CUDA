-- EXP-2026-09-06-037 offline analysis of EXP-031b Nsight export.
-- Read-only queries over CUPTI tables. Timestamps are ns since profiler
-- session start; nothing here joins different clock domains.
-- Request window bounds (established from the saved trace itself):
--   WS = 10417843203 (first H2D memcpy of the in-request burst, after a
--        7023 ms idle gap; startup activity ends at 3394783 us-scale)
--   WE = 14363385682 (last kernel end of the burst)

-- 0. Exact burst bounds and window membership sanity
SELECT 'burst_h2d_start' AS item, printf('%.9f', MIN(start)/1e9) AS value FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start >= 10417843000
UNION ALL
SELECT 'burst_end', printf('%.9f', MAX(end)/1e9) FROM (SELECT end FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start >= 10417843000 UNION ALL SELECT end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start >= 10417843000 UNION ALL SELECT end FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start >= 10417843000);

-- 1. Copies by kind inside the request window
SELECT copyKind, COUNT(*) AS n, SUM(bytes) AS bytes, printf('%.2f', SUM(bytes)/1048576.0) AS mib
FROM CUPTI_ACTIVITY_KIND_MEMCPY
WHERE start >= 10417843203 AND end <= 14363385682
GROUP BY copyKind ORDER BY copyKind;

-- 2. H2D size percentiles (nearest-rank) inside the window
WITH h AS (SELECT bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start >= 10417843203 AND end <= 14363385682),
r AS (SELECT bytes, ROW_NUMBER() OVER (ORDER BY bytes) rn, COUNT(*) OVER () n FROM h)
SELECT 'h2d_bytes_median' AS item, MAX(bytes) AS v FROM r WHERE rn = (n+1)/2
UNION ALL SELECT 'h2d_bytes_p90', MAX(bytes) FROM r WHERE rn = CAST(0.90*n AS INTEGER)+1
UNION ALL SELECT 'h2d_bytes_p99', MAX(bytes) FROM r WHERE rn = CAST(0.99*n AS INTEGER)+1
UNION ALL SELECT 'h2d_bytes_max', MAX(bytes) FROM r
UNION ALL SELECT 'h2d_bytes_min', MIN(bytes) FROM r
UNION ALL SELECT 'h2d_bytes_mean', CAST(SUM(bytes)/n AS INTEGER) FROM r WHERE rn=1;

-- 3. Host CUDA API durations by call name inside the window (sum; union per name)
WITH a AS (
  SELECT s.value AS name, r.start, r.end
  FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id = r.nameId
  WHERE r.start >= 10417843203 AND r.end <= 14363385682 + 200000000
), g AS (
  SELECT name, start, end,
         MAX(end) OVER (PARTITION BY name ORDER BY start, end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax
  FROM a
), b AS (
  SELECT name, start, end, SUM(CASE WHEN pmax IS NULL OR start > pmax THEN 1 ELSE 0 END)
         OVER (PARTITION BY name ORDER BY start, end ROWS UNBOUNDED PRECEDING) gid FROM g
), u AS (SELECT name, gid, MIN(start) s, MAX(end) e FROM b GROUP BY name, gid)
SELECT a.name, COUNT(*) n, printf('%.3f', SUM(a.end-a.start)/1e6) sum_ms FROM a GROUP BY a.name ORDER BY sum_ms DESC;
WITH base AS (SELECT s.value name, r.start, r.end FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId WHERE r.start>=10417843203 AND r.end<=14563385682),
g AS (SELECT name, start, end, MAX(end) OVER (PARTITION BY name ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM base),
b AS (SELECT name, start, end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (PARTITION BY name ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT name, gid, MIN(start) s, MAX(end) e FROM b GROUP BY name, gid)
SELECT name, printf('%.3f', SUM(e-s)/1e6) union_ms FROM u GROUP BY name ORDER BY union_ms DESC LIMIT 6;

-- 3b. Host memcpyAsync union, and memcpyAsync+streamSynchronize union
WITH base AS (SELECT start,end FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId WHERE s.value LIKE 'cudaMemcpy%Async%' AND r.start>=10417843203 AND r.end<=14563385682),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM base),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'memcpyAsync_host_union_ms', printf('%.3f', SUM(e-s)/1e6) FROM u;
WITH base AS (SELECT start,end FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId WHERE (s.value LIKE 'cudaMemcpy%Async%' OR s.value LIKE 'cudaStreamSynchronize%') AND r.start>=10417843203 AND r.end<=14563385682),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM base),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'memcpy+sync_host_union_ms', printf('%.3f', SUM(e-s)/1e6) FROM u;

-- 4. GPU H2D sum and union; kernel sum and union; memset inside window
SELECT 'gpu_h2d_sum_ms' item, printf('%.3f', SUM(end-start)/1e6) v FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=10417843203 AND end<=14363385682
UNION ALL SELECT 'gpu_dtoh_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=2 AND start>=10417843203 AND end<=14363385682
UNION ALL SELECT 'gpu_dtod_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=8 AND start>=10417843203 AND end<=14363385682
UNION ALL SELECT 'kernel_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=10417843203 AND end<=14363385682
UNION ALL SELECT 'memset_sum_ms', printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start>=10417843203 AND end<=14363385682;

WITH k AS (SELECT start, end FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=10417843203 AND end<=14363385682),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM k),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'kernel_union_ms' item, printf('%.3f', SUM(e-s)/1e6) v FROM u;

WITH c AS (SELECT start,end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=10417843203 AND end<=14363385682),
g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM c),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e FROM b GROUP BY gid)
SELECT 'gpu_h2d_union_ms' item, printf('%.3f', SUM(e-s)/1e6) v FROM u;

-- 5. Real H2D/kernel intersection (sweep with running counts)
WITH ev AS (
  SELECT start t,  1 dh, 0 dk FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=10417843203 AND end<=14363385682
  UNION ALL SELECT end, -1, 0 FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=10417843203 AND end<=14363385682
  UNION ALL SELECT start, 0,  1 FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=10417843203 AND end<=14363385682
  UNION ALL SELECT end, 0, -1 FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=10417843203 AND end<=14363385682
), cum AS (
  SELECT t, SUM(dh) OVER (ORDER BY t ROWS UNBOUNDED PRECEDING) ch,
           SUM(dk) OVER (ORDER BY t ROWS UNBOUNDED PRECEDING) ck,
           LEAD(t) OVER (ORDER BY t) tn FROM ev
)
SELECT 'h2d_kernel_intersect_ms' item, printf('%.3f', SUM(tn-t)/1e6) v FROM cum WHERE tn IS NOT NULL AND ch>0 AND ck>0;

-- 6. Synchronization activity inside the window
SELECT syncType, COUNT(*) n, printf('%.3f', SUM(end-start)/1e6) sum_ms,
       printf('%.3f', MAX(end-start)/1e6) max_ms
FROM CUPTI_ACTIVITY_KIND_SYNCHRONIZATION
WHERE start >= 10417843203 AND end <= 14363385682 + 200000000
GROUP BY syncType;

-- 7. Gaps without CUDA activity inside the window (union of memcpy/kernel/memset)
WITH act AS (
  SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=10417843203 AND end<=14363385682
  UNION ALL SELECT start, end FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=10417843203 AND end<=14363385682
  UNION ALL SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMSET WHERE start>=10417843203 AND end<=14363385682
), g AS (SELECT start,end, MAX(end) OVER (ORDER BY start,end ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) pmax FROM act),
b AS (SELECT start,end, SUM(CASE WHEN pmax IS NULL OR start>pmax THEN 1 ELSE 0 END) OVER (ORDER BY start,end ROWS UNBOUNDED PRECEDING) gid FROM g),
u AS (SELECT gid, MIN(start) s, MAX(end) e,
      LAG(MAX(end)) OVER (ORDER BY MIN(start)) prev_e FROM b GROUP BY gid)
SELECT COUNT(*) AS busy_segments,
       printf('%.3f', SUM(e-s)/1e6) AS gpu_busy_union_ms,
       printf('%.3f', (MAX(e)-MIN(s))/1e6) AS span_ms,
       printf('%.3f', (MAX(e)-MIN(s) - SUM(e-s))/1e6) AS total_gap_ms,
       SUM(CASE WHEN prev_e IS NOT NULL AND s-prev_e > 1000000 THEN 1 ELSE 0 END) AS gaps_gt_1ms,
       printf('%.3f', MAX(CASE WHEN prev_e IS NOT NULL THEN s-prev_e ELSE 0 END)/1e6) AS max_gap_ms
FROM u;

-- 8. Per-stream distribution inside the window (copies, kernels, sync)
SELECT 'memcpy' src, streamId, COUNT(*) n, printf('%.2f', SUM(bytes)/1048576.0) mib, printf('%.3f', SUM(end-start)/1e6) dur_ms
FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE start>=10417843203 AND end<=14363385682 GROUP BY streamId
UNION ALL
SELECT 'kernel', streamId, COUNT(*), NULL, printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_KERNEL WHERE start>=10417843203 AND end<=14363385682 GROUP BY streamId
UNION ALL
SELECT 'sync', streamId, COUNT(*), NULL, printf('%.3f', SUM(end-start)/1e6) FROM CUPTI_ACTIVITY_KIND_SYNCHRONIZATION WHERE start>=10417843203 AND end<=14363385682 GROUP BY streamId;

-- 9. Host memcpyAsync submission durations (RUNTIME joined to activity by correlationId)
WITH h AS (
  SELECT r.start, r.end, printf('%s', s.value) name
  FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId
  WHERE r.start>=10417843203 AND r.end<=14363385682 + 200000000 AND s.value LIKE 'cudaMemcpy%Async%'
), st AS (SELECT (end-start) d FROM h)
SELECT COUNT(*) n, printf('%.3f', SUM(d)/1e6) sum_ms, printf('%.3f', AVG(d)/1e6) mean_ms,
       printf('%.3f', MAX(d)/1e6) max_ms FROM st;

-- 10. Destination-address adjacency in the trace itself (if virtualAddress populated)
SELECT 'h2d_with_virtualAddress' item, COUNT(*) v FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND start>=10417843203 AND end<=14363385682 AND virtualAddress IS NOT NULL AND virtualAddress != 0;

-- 11. Expert-only H2D on the main stream (matches EXP-032 count exactly)
SELECT 'expert_h2d_stream16' item, COUNT(*) n, printf('%.2f',SUM(bytes)/1048576.0) mib,
       printf('%.3f',MIN(start)/1e9) first_s, printf('%.3f',MAX(end)/1e9) last_s,
       printf('%.3f',(MAX(end)-MIN(start))/1e6) span_ms
FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 AND streamId=16 AND start>=10417843203 AND end<=14363385682;

-- 12. Non-expert copies inside the window, attributed by correlationId to API name
SELECT m.copyKind, m.streamId, s.value api, COUNT(*) n, printf('%.2f',SUM(m.bytes)/1048576.0) mib
FROM CUPTI_ACTIVITY_KIND_MEMCPY m
LEFT JOIN CUPTI_ACTIVITY_KIND_RUNTIME r ON r.correlationId = m.correlationId
LEFT JOIN StringIds s ON s.id = r.nameId
WHERE m.start>=10417843203 AND m.end<=14363385682 AND (m.copyKind IN (2,8) OR m.streamId!=16)
GROUP BY m.copyKind, m.streamId, s.value ORDER BY m.copyKind, m.streamId;

-- 13. Top kernels by summed duration inside the window
SELECT substr(s.value,1,64) kernel, COUNT(*) n, printf('%.3f', SUM(k.end-k.start)/1e6) sum_ms
FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id = k.demangledName
WHERE k.start>=10417843203 AND k.end<=14363385682
GROUP BY s.value ORDER BY SUM(k.end-k.start) DESC LIMIT 6;
