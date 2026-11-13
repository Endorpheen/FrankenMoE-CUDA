extern "fg143_job_get" fn job_get(context:i64,field:i64)->i64;
extern "fg143_job_set" fn job_set(context:i64,field:i64,value:i64);
extern "fg143_job_create" fn job_create(context:i64,threads:i64);
extern "fg143_job_free" fn job_free(context:i64);
extern "fg143_job_team" fn job_team(context:i64,threads:i64);
extern "fg143_job_barrier" fn job_barrier();
extern "fg143_job_place" fn job_place(context:i64,thread:i64);
extern "fg143_job_compute" fn job_compute(context:i64,thread:i64);
extern "fg143_job_clear_affinity" fn job_clear_affinity();
fn job_worker(c:i64,single:bool) {
    let thread=if single {0}else {job_get(c,10)};
    let actual=if single {1}else {job_get(c,11)};
    assert(actual>0&&thread>=0&&thread<actual&&actual<=job_get(c,4));
    if thread==0 {job_set(c,5,actual);}
    if !single {job_barrier();}
    if !single {job_place(c,thread);}
    job_compute(c,thread);witness(4);
}
fn graph_job(c:i64,out:i64[:]) {
    assert(out.len>=1&&job_get(c,0)!=0);let requested=job_get(c,1);
    assert(requested>0&&job_get(c,2)>=0&&(job_get(c,2)==0||job_get(c,3)!=0));
    let created=job_get(c,6)==0;
    if created {job_create(c,requested);}else {
        job_set(c,0,0);job_set(c,1,0);job_set(c,2,0);job_set(c,3,-1);job_set(c,4,job_get(c,12));
    }
    assert(job_get(c,6)!=0&&requested<=job_get(c,4));
    if requested>1 {job_team(c,requested);}else {job_worker(c,true);}
    job_clear_affinity();let status=job_get(c,7);
    if created {job_free(c);}out[0]=status;witness(3);
}
