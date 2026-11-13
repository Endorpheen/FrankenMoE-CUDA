-- EXP-2026-09-09-049 offline analysis: where CUDA Graph capture engages/never engages,
-- over the saved EXP-043 Nsight sqlite export (sha256
-- f86cd32ba4c05d611e448dd76d793b517233a5a4af934f8017f149ce48e34a2f, /tmp/exp043-profile/exp043.sqlite).
-- Read-only (file:...?immutable=1). No server, model, or build runs. Code citations refer to
-- work/llama.cpp-exp046-iq2s-verify (the tree of build/exp046-default-runtime).

-- 1. All CUDA-graph API activity over the whole profile, with time span.
-- Expected vs observed: the process ran model load + warmup request (with ~104 decode tokens)
-- + measured 395-token prefill request (n_predict=0, NO target decode) + teardown.
SELECT s.value, COUNT(*) n,
       printf('%.3f', MIN(r.start)/1e9) first_s, printf('%.3f', MAX(r.end)/1e9) last_s
FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId
WHERE s.value LIKE '%Graph%' OR s.value LIKE '%Capture%'
GROUP BY s.value ORDER BY n DESC;

-- 2. Timestamps of every capture/instantiate/launch: attribution to phases.
-- Startup model-load warmup runs 7.6-8.4 s; warmup request ~125 s; measured request
-- window [149.3, 152.98] s (W1 bulk prefill + checkpoint + W2 4-token batch, window end 152.980056).
SELECT printf('%.4f', r.start/1e9) t_s, s.value
FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId
WHERE s.value IN ('cudaStreamBeginCapture_v10000','cudaGraphInstantiate_v12000','cudaGraphLaunch_v10000')
ORDER BY r.start;
