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


class SweepCharactersTest(unittest.TestCase):
    def test_route_sets_both_ports_before_the_stage(self):
        import sweep_characters
        route = sweep_characters.character_route(4, 1, 60)
        char_entries = [e for e in route if ":CHAR=" in e]
        self.assertEqual(char_entries, [
            f"{sweep_stages.FORCE_FRAME - 2}:CHAR=4,1,9",
            f"{sweep_stages.FORCE_FRAME - 2}:CHAR=1,2,9",
        ])
        self.assertIn(f"{sweep_stages.FORCE_FRAME}:STAGE=31", route)
        self.assertEqual(route[-1], f"{sweep_stages.FORCE_FRAME + 60}:STOP")

    def test_status_lists_the_26_characters_in_kind_order(self):
        import sweep_characters
        document = sweep_characters.load_status()
        kinds = [c["kind"] for c in document["characters"]]
        self.assertEqual(kinds, list(range(26)))
        for character in document["characters"]:
            for column in sweep_characters.MODES.values():
                self.assertIn(character[column], (None, True, False))

    def test_the_random_square_is_not_swept_by_default(self):
        self.assertNotIn(0, sweep_stages.STAGE_KINDS)
        self.assertEqual(len(sweep_stages.STAGE_KINDS), 29)


if __name__ == "__main__":
    unittest.main()
