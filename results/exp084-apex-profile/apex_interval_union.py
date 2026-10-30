"""Pure helpers for Nsight timeline interval analysis.

All durations are integer nanoseconds and intervals are half-open
[start, end). None of the functions mutates their inputs or performs side
effects: no imports, no file or network access.
"""


def _merge(intervals):
    """Sweep-line merge of valid intervals, result sorted by start."""
    merged = []
    for start, end in sorted(intervals):
        if end <= start:
            continue
        if merged and start <= merged[-1][1]:
            if end > merged[-1][1]:
                merged[-1] = (merged[-1][0], end)
        else:
            merged.append((start, end))
    return merged


def _check_intervals(intervals):
    for start, end in intervals:
        if end < start:
            raise ValueError("invalid interval %r: end must not be below start" % ((start, end),))


def merge_intervals(intervals, window):
    """Merge, sort and clip intervals to the window [lo, hi].

    Handles unordered, duplicated, overlapping and out-of-window input.
    Returns a sorted list of non-overlapping pairs covering the same time
    as the input. Raises ValueError for end < start intervals or a window
    with lo > hi.
    """
    lo, hi = window
    if lo > hi:
        raise ValueError("invalid window %r: lo must not exceed hi" % ((lo, hi),))
    _check_intervals(intervals)
    clipped = []
    for start, end in intervals:
        if lo <= end and start <= hi:
            clipped.append((max(start, lo), min(end, hi)))
    return _merge(clipped)


def duration(intervals):
    """Total length of the merged, non-overlapping intervals."""
    _check_intervals(intervals)
    return sum(end - start for start, end in _merge(intervals))


def complement(intervals, window):
    """Sorted half-open gaps of [lo, hi] not covered by the interval union."""
    lo, hi = window
    if lo > hi:
        raise ValueError("invalid window %r: lo must not exceed hi" % ((lo, hi),))
    _check_intervals(intervals)
    covered = merge_intervals(intervals, window)
    gaps = []
    cur = lo
    for start, end in covered:
        if start > cur:
            gaps.append((cur, start))
        cur = max(cur, end)
    if cur < hi:
        gaps.append((cur, hi))
    return gaps


def summarize(window, kernels, copies):
    """Summarize GPU activity from kernel and copy intervals.

    Returns whole-number nanosecond counts: the kernel union, the copy
    union, their combined GPU union, the overlap between the two unions
    (kernel_union + copy_union - gpu_union) and the uncovered remainder of
    the window (idle). This describes GPU activity of the listed streams
    only; idle time is not a claim about CPU bottlenecking.
    """
    lo, hi = window
    if lo > hi:
        raise ValueError("invalid window %r: lo must not exceed hi" % ((lo, hi),))
    _check_intervals(kernels)
    _check_intervals(copies)
    kernels_only = merge_intervals(kernels, window)
    copies_only = merge_intervals(copies, window)
    gpu = merge_intervals(list(kernels) + list(copies), window)
    kernel_union_ns = sum(e - s for s, e in kernels_only)
    copy_union_ns = sum(e - s for s, e in copies_only)
    gpu_union_ns = sum(e - s for s, e in gpu)
    return {
        "window_ns": hi - lo,
        "kernel_union_ns": kernel_union_ns,
        "copy_union_ns": copy_union_ns,
        "gpu_union_ns": gpu_union_ns,
        "overlap_ns": kernel_union_ns + copy_union_ns - gpu_union_ns,
        "idle_ns": hi - lo - gpu_union_ns,
    }
