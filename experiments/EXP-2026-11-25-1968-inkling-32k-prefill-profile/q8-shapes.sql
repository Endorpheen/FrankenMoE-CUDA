select n.text,k.gridX,k.gridY,k.blockX,count(*),sum(k.end-k.start)*1e-6
from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.shortName join NVTX_EVENTS n
on k.start>=n.start and k.end<=n.end
where s.value='goose_i2_matrix_q8_0' and n.text in ('Inkling attention','Inkling MoE')
group by 1,2,3,4 order by 6 desc;
