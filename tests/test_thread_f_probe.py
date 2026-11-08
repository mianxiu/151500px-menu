import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('thread_f_probe', Path(__file__).parents[1] / 'tools/thread-f-probe.py')
probe = importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)


class ThreadFAnalysisTests(unittest.TestCase):
    def analyze(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'events.tsv';path.write_text(text, 'utf-8')
            return probe.analyze(path)

    def test_peeks_are_not_extra_presses_and_repeats_are_not_new_downs(self):
        rows = [(1, 3, 1), (1, 3, 1), (1, 0, 1), (1, 0, 0x40000001), (1, 0, 0xc0000001), (3, 256, 1)]
        text = 'frequency\t10000000\ncount\t6\nlost\t0\n'
        for index, (stage, message, flags) in enumerate(rows):
            text += f'{index}\t{index+1}\t{stage}\t{message}\t{flags}\t7\t0\t12\t134481924\n'
        result = self.analyze(text)
        self.assertEqual((result['keyboard_peeks'], result['down'], result['repeat'], result['up']), (2, 1, 1, 1))
        self.assertEqual(result['migration_gate'], 'not-validated')

    def test_empty_capture_does_not_claim_migration(self):
        result = self.analyze('frequency\t10000000\ncount\t0\nlost\t0\n')
        self.assertEqual(result['keyboard_actions'], 0)
        self.assertEqual(result['migration_gate'], 'not-validated')

    def test_invalid_truncated_or_reordered_export_is_rejected(self):
        for text in ('frequency\t0\ncount\t0\nlost\t0\n',
                     'frequency\t10\ncount\t1\nlost\t0\n',
                     'frequency\t10\ncount\t1\nlost\t1\n',
                     'frequency\t10\ncount\t1\nlost\t0\n0\t1\t5\t0\t1\t0\t0\t0\t0\n',
                     'frequency\t10\ncount\t2\nlost\t0\n0\t2\t1\t0\t1\t0\t0\t0\t0\n1\t1\t1\t0\t1\t0\t0\t0\t0\n',
                     'frequency\t10\ncount\t1\nlost\t0\n1\t1\t1\t0\t1\t0\t0\t0\t0\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):self.analyze(text)
