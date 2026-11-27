static int launch_expert_overlap(DevicePtr bank, int64_t weight_bytes, int64_t row_stride,
                                 int64_t extent, int64_t kind, int64_t rows,
                                 sl_i64 jobs, int64_t shared_rows, int64_t input_stride) {
    const size_t pending = inkling_overlap_current_uploads;
    if (!pending) {
        return launch_matrix(bank, row_stride, extent, kind, rows, jobs, 3, shared_rows, input_stride);
    }

    enum { max_routes = 8, words = 3 };
    int64_t resident[max_routes * words], awaiting[max_routes * words];
    sl_i64 hit = {resident, 0}, miss = {awaiting, 0};
    if (jobs.len <= max_routes * words) {
        for (int64_t at = 0; at < jobs.len; at += words) {
            const DevicePtr begin = bank + (uint64_t)jobs.data[at + 2];
            const DevicePtr end = begin + (uint64_t)weight_bytes;
            bool pending_source = false;
            for (size_t n = 0; n < pending; ++n) {
                const struct expert_upload_job *upload = &cuda.expert_upload_jobs[n];
                if (begin < upload->destination + upload->bytes && upload->destination < end) {
                    pending_source = true;
                    break;
                }
            }
            sl_i64 *part = pending_source ? &miss : &hit;
            memcpy(part->data + part->len, jobs.data + at, words * sizeof(*jobs.data));
            part->len += words;
        }
    } else {
        miss = jobs;
    }

    int status = 0;
    if (hit.len) status = launch_matrix(bank, row_stride, extent, kind, rows, hit, words,
                                        shared_rows, input_stride);
    if (status == 0) status = submit_expert_uploads(pending);
    if (status == 0 && miss.len) {
        status = cuda.stream_wait_event(NULL, cuda.expert_uploads_ready, 0);
        if (status == 0) cuda.expert_upload_queued = false;
        if (status == 0) status = launch_matrix(bank, row_stride, extent, kind, rows, miss, words,
                                                shared_rows, input_stride);
    }
    if (status == 0 && hit.len && miss.len) {
        ++inkling_overlap_groups;
        inkling_overlap_resident += (uint64_t)hit.len / words;
        inkling_overlap_missing += (uint64_t)miss.len / words;
    }
    return status;
}
