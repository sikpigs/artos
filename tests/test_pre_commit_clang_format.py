"""Integration checks using a temporary Git index and the real clang-format."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/pre_commit_clang_format.py'


@unittest.skipUnless(shutil.which('clang-format'), 'clang-format is required')
class PreCommitTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git('init', '-q')
        shutil.copyfile(ROOT / '.clang-format', self.root / '.clang-format')
        self.git('add', '.clang-format')

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root)

    def check(self, **environment):
        return subprocess.run([sys.executable, str(SCRIPT)], cwd=self.root,
                              env={**os.environ, **environment}, capture_output=True)

    def stage(self, content, name='with spaces.cpp'):
        (self.root / name).write_bytes(content)
        self.git('add', '--', name)

    def test_clean_staged_content_ignores_unstaged_source_and_style(self):
        self.stage(b'int value = 1;\n')
        (self.root / 'with spaces.cpp').write_text('int   value=2;\n')
        (self.root / '.clang-format').write_text('InvalidStyle: true\n')
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_diff_blocks_without_modifying_worktree_or_index(self):
        self.stage(b'int   value=1;\n')
        (self.root / 'with spaces.cpp').write_bytes(b'int value = 2;\n')
        index = self.git('write-tree')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'-int   value=1;', result.stderr)
        self.assertIn(b'+int value = 1;', result.stderr)
        self.assertEqual(self.git('write-tree'), index)
        self.assertEqual((self.root / 'with spaces.cpp').read_bytes(), b'int value = 2;\n')

    def test_no_sources_does_not_require_formatter(self):
        self.stage(b'hello\n', 'notes.txt')
        self.assertEqual(self.check(CLANG_FORMAT='/missing/clang-format').returncode, 0)

    def test_missing_formatter_blocks(self):
        self.stage(b'int value;\n')
        result = self.check(CLANG_FORMAT='/missing/clang-format')
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'clang-format is missing', result.stderr)

    def test_formatter_failure_blocks(self):
        self.stage(b'int value;\n')
        self.stage(b'InvalidStyle: true\n', '.clang-format')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'clang-format failed', result.stderr)

    def test_deleted_source_is_skipped(self):
        self.stage(b'int value;\n')
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.com',
                 'commit', '-qm', 'initial')
        self.git('rm', '--', 'with spaces.cpp')
        self.assertEqual(self.check(CLANG_FORMAT='/missing/clang-format').returncode, 0)


if __name__ == '__main__':
    unittest.main()
