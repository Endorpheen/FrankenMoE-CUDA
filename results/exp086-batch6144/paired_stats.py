"""Pair-wise timing statistics for comparing two sets of matching runs.

Given a control and a candidate timing list of equal length, the module
computes, for each corresponding pair, the relative reduction
100 * (1 - candidate / control), and summarizes the pairs with their
medians together with the share of pairs where the candidate was
strictly faster.
"""

from statistics import median
from typing import Sequence


def _validate_times(times, label):
    """Raise ValueError unless every item is a positive, finite number.

    Empty lists, non-positive values, NaN and infinities all fail; the
    index in the message identifies the offending entry.
    """
    for index, value in enumerate(times):
        if not isinstance(value, (int, float)):
            raise ValueError(f"{label}[{index}] is not a number: {value!r}")
        if not (value > 0) or value != value or abs(value) == float("inf"):
            raise ValueError(f"{label}[{index}] is not a positive, finite "
                             f"value: {value!r}")


def summarize_pairs(control_s, candidate_s):
    """Return pair-wise reduction statistics for two equal-length lists.

    Args:
        control_s: non-empty sequence of positive, finite control times (s).
        candidate_s: non-empty sequence of positive, finite candidate times
            (s), the same length as ``control_s``.

    Returns:
        dict holding the pair count, both medians, the per-pair reductions,
        the median of the per-pair reductions and the win count:

        * ``n_pairs``: number of paired runs
        * ``control_median_s``: median of the control times
        * ``candidate_median_s``: median of the candidate times
        * ``paired_time_reduction_pct``: 100 * (1 - candidate / control)
          for each corresponding pair
        * ``median_paired_time_reduction_pct``: median of the per-pair
          reductions (not 100 * (1 - candidate median / control median))
        * ``wins``: number of pairs where the candidate is strictly faster

    Raises:
        ValueError: if either list is empty, the lengths differ, or any
            value is non-positive, NaN or infinite.
    """
    if len(control_s) != len(candidate_s):
        raise ValueError(f"control has {len(control_s)} values but candidate "
                         f"has {len(candidate_s)}")
    _validate_times(control_s, "control")
    _validate_times(candidate_s, "candidate")

    reductions = [
        100.0 * (1.0 - candidate / control)
        for control, candidate in zip(control_s, candidate_s)
    ]
    wins = sum(1 for control, candidate in zip(control_s, candidate_s)
               if candidate < control)
    return {
        "n_pairs": len(control_s),
        "control_median_s": median(control_s),
        "candidate_median_s": median(candidate_s),
        "paired_time_reduction_pct": reductions,
        "median_paired_time_reduction_pct": median(reductions),
        "wins": wins,
    }
