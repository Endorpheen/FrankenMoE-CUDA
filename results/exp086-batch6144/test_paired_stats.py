"""Unit tests for summarize_pairs."""

import unittest

from statistics import median

from paired_stats import summarize_pairs


class SummarizePairsTests(unittest.TestCase):

    def test_constant_ratio_gives_the_ratio_and_all_wins(self):
        summary = summarize_pairs([10, 20, 40], [9, 18, 36])
        self.assertEqual(summary["n_pairs"], 3)
        self.assertEqual(summary["control_median_s"], 20)
        self.assertEqual(summary["candidate_median_s"], 18)
        self.assertAlmostEqual(summary["median_paired_time_reduction_pct"], 10)
        self.assertEqual(summary["wins"], 3)

    def test_mixed_improvements_and_setbacks_gives_zero_median(self):
        summary = summarize_pairs([10, 20, 40], [11, 18, 40])
        self.assertAlmostEqual(summary["median_paired_time_reduction_pct"], 0)
        self.assertEqual(summary["wins"], 1)
        self.assertAlmostEqual(summary["paired_time_reduction_pct"][0], -10)
        self.assertAlmostEqual(summary["paired_time_reduction_pct"][1], 10)
        self.assertEqual(summary["paired_time_reduction_pct"][2], 0)

    def test_median_is_of_pairwise_pcts_not_of_medians(self):
        control, candidate = [10, 20, 40], [11, 18, 40]
        percent_of_medians = 100 * (1 - median(candidate) / median(control))
        summary = summarize_pairs(control, candidate)
        self.assertNotAlmostEqual(summary["median_paired_time_reduction_pct"],
                                  percent_of_medians)

    def test_single_pair(self):
        summary = summarize_pairs([5.0], [4.0])
        self.assertEqual(summary["n_pairs"], 1)
        self.assertAlmostEqual(summary["median_paired_time_reduction_pct"], 20)
        self.assertEqual(summary["wins"], 1)

    def test_even_pair_count_is_median_of_the_middle_two(self):
        summary = summarize_pairs([10, 20, 40, 80], [9, 18, 40, 75])
        reductions = summary["paired_time_reduction_pct"]
        expected = (sorted(reductions)[1] + sorted(reductions)[2]) / 2
        self.assertAlmostEqual(summary["median_paired_time_reduction_pct"],
                             expected)
        self.assertEqual(summary["wins"], 3)

    def test_bad_inputs_are_rejected(self):
        bad_cases = [
            ([], [10, 20]),
            ([10, 20], []),
            ([10, 20], [10, 20, 30]),
            ([-1, 20], [9, 18]),
            ([10, 0], [9, 18]),
            ([10, 20], [9, float("nan")]),
            ([10, 20], [9, float("inf")]),
            ([10, 20], [9, -float("inf")]),
        ]
        for control, candidate in bad_cases:
            with self.subTest(control=control, candidate=candidate):
                with self.assertRaises(ValueError):
                    summarize_pairs(control, candidate)


if __name__ == "__main__":
    unittest.main()
