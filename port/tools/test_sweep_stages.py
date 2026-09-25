#!/usr/bin/env python3

import unittest

import sweep_stages


class SweepStagesTest(unittest.TestCase):
    def test_repeat_runs_every_requested_attempt_after_a_failure(self):
        statuses = iter(("ok", "signal", "ok"))
        attempts = sweep_stages.repeated_attempts(
            lambda: {"status": next(statuses)}, 3, False)
        self.assertEqual([attempt["status"] for attempt in attempts],
                         ["ok", "signal", "ok"])
        report = sweep_stages.summarize_attempts(attempts, 3)
        self.assertEqual(report["status"], "signal")
        self.assertEqual(report["requested_attempts"], 3)
        self.assertEqual(report["completed_attempts"], 3)
        self.assertEqual(report["passed"], 2)
        self.assertEqual(len(report["attempts"]), 3)

    def test_fail_fast_is_explicit(self):
        calls = 0

        def fail():
            nonlocal calls
            calls += 1
            return {"status": "signal"}

        attempts = sweep_stages.repeated_attempts(fail, 4, True)
        self.assertEqual(calls, 1)
        self.assertEqual(len(attempts), 1)


if __name__ == "__main__":
    unittest.main()
