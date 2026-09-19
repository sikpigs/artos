"""Integration tests for staged clang-tidy analysis and failure handling."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/pre_commit_clang_tidy.py'


class TidyHookTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.git('init', '-q')
        self.stage('.clang-tidy', "Checks: '-*,bugprone-unused-raii'\n")
        self.stage('file with spaces.cpp', 'int value;\n')
        build = self.root / 'build/debug'
        build.mkdir(parents=True)
        (build / 'compile_commands.json').write_text(json.dumps([{
            'directory': str(build), 'file': str(self.root / 'file with spaces.cpp'),
            'arguments': ['c++', '-c', str(self.root / 'file with spaces.cpp')],
        }]))
        self.tidy = self.root / 'fake-tidy'
        self.tidy.write_text('#!' + sys.executable + '\n'
                             'import pathlib, sys\n'
                             'assert "--warnings-as-errors=*" in sys.argv\n'
                             'source = pathlib.Path(sys.argv[3])\n'
                             'assert source.read_text() == "int value;\\n"\n'
                             'assert "bad" not in (source.parent / ".clang-tidy").read_text()\n')
        self.tidy.chmod(0o755)

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root)

    def stage(self, name, content):
        (self.root / name).write_text(content)
        self.git('add', '--', name)

    def run_hook(self, **environment):
        return subprocess.run([sys.executable, str(SCRIPT)], cwd=self.root,
                              env={**os.environ, 'CLANG_TIDY': str(self.tidy),
                                   'CLANG_TIDY_BUILD_DIR': 'build/debug', **environment},
                              capture_output=True)

    def commit(self):
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.com',
                 'commit', '-qm', 'initial')

    def test_partial_staging_and_staged_configuration(self):
        (self.root / 'file with spaces.cpp').write_text('unstaged\n')
        (self.root / '.clang-tidy').write_text('bad configuration\n')
        before = self.git('write-tree')
        result = self.run_hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(before, self.git('write-tree'))
        self.assertEqual((self.root / 'file with spaces.cpp').read_text(), 'unstaged\n')

    def test_diagnostic_failure_blocks(self):
        with self.tidy.open('a') as stream:
            stream.write('print("test diagnostic", file=sys.stderr)\nsys.exit(1)\n')
        result = self.run_hook()
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'test diagnostic', result.stderr)
        self.assertIn(b'Commit blocked', result.stderr)

    def test_header_change_checks_existing_translation_units(self):
        self.commit()
        self.stage('header.h', '#pragma once\n')
        result = self.run_hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'file with spaces.cpp', result.stdout)

    def test_missing_source_command_blocks(self):
        self.stage('new.cpp', 'int other;\n')
        result = self.run_hook()
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'missing from compilation database: new.cpp', result.stderr)

    def test_multiple_builds_cover_board_and_host_sources(self):
        for directory, name in [('build/secondary', 'board.cpp'),
                                ('build/host-tests', 'native.cpp')]:
            self.stage(name, 'int value;\n')
            build = self.root / directory
            build.mkdir(parents=True)
            (build / 'generated.h').write_text(name)
            (build / 'compile_commands.json').write_text(json.dumps([{
                'directory': str(build), 'file': str(self.root / name),
                'arguments': ['c++', '-I' + str(build), '-c', str(self.root / name)],
            }]))
        with self.tidy.open('a') as stream:
            stream.write('import json\n'
                         'build = pathlib.Path(sys.argv[2])\n'
                         'entries = json.loads((build / "compile_commands.json").read_text())\n'
                         'entry = next(e for e in entries if e["file"] == str(source))\n'
                         'assert entry["directory"] == str(build)\n'
                         'if source.name != "file with spaces.cpp":\n'
                         '    assert (build / "generated.h").read_text() == source.name\n'
                         '    assert "-I" + str(build) in entry["arguments"]\n')
        before = self.git('write-tree')
        result = self.run_hook(CLANG_TIDY_BUILD_DIR='', CLANG_TIDY_BUILD_DIRS='')
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in ['board.cpp', 'native.cpp', 'file with spaces.cpp']:
            self.assertIn(name.encode(), result.stdout)
        self.assertEqual(before, self.git('write-tree'))
        result = self.run_hook(CLANG_TIDY_BUILD_DIRS=os.pathsep.join(
            ['build/debug', 'build/secondary', 'build/host-tests']))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_active_database_wins_for_shared_source(self):
        other = self.root / 'build/other'
        other.mkdir(parents=True)
        entry = {'directory': str(other), 'file': str(self.root / 'file with spaces.cpp'),
                 'arguments': ['/missing/arm-none-eabi-g++', '-c',
                               str(self.root / 'file with spaces.cpp')]}
        (other / 'compile_commands.json').write_text(json.dumps([entry]))
        (self.root / 'compile_commands.json').symlink_to('build/debug/compile_commands.json')
        result = self.run_hook(CLANG_TIDY_BUILD_DIR='', CLANG_TIDY_BUILD_DIRS='')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count(b'clang-tidy (staged)'), 1)

    def test_missing_tool_and_database_block(self):
        result = self.run_hook(CLANG_TIDY='/missing/tidy')
        self.assertIn(b'clang-tidy is missing', result.stderr)
        self.assertEqual(result.returncode, 1)
        result = self.run_hook(CLANG_TIDY_BUILD_DIR='missing')
        self.assertIn(b'compile_commands.json is missing', result.stderr)
        self.assertEqual(result.returncode, 1)

    def test_non_cpp_commit_skips_tools(self):
        self.commit()
        self.stage('notes.txt', 'notes\n')
        self.assertEqual(self.run_hook(CLANG_TIDY='/missing/tidy').returncode, 0)

    def test_deleted_source_skips_analysis(self):
        self.commit()
        self.git('rm', '--', 'file with spaces.cpp')
        result = self.run_hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, b'')

    @unittest.skipUnless(shutil.which('clang-tidy') and shutil.which('arm-none-eabi-g++'),
                         'clang-tidy and GNU Arm are required')
    def test_real_arm_analysis(self):
        self.stage('.clang-tidy', (SCRIPT.parents[1] / '.clang-tidy').read_text())
        self.stage('file with spaces.cpp', '#include <cstdint>\nstd::uint32_t value;\n')
        database = self.root / 'build/debug/compile_commands.json'
        commands = json.loads(database.read_text())
        commands[0]['arguments'] = [shutil.which('arm-none-eabi-g++'), '-std=c++23',
                                    '-mcpu=cortex-m4', '-mthumb', '-mgeneral-regs-only', '-c',
                                    str(self.root / 'file with spaces.cpp')]
        database.write_text(json.dumps(commands))
        result = self.run_hook(CLANG_TIDY=shutil.which('clang-tidy'))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('-mgeneral-regs-only', json.loads(database.read_text())[0]['arguments'])
        self.stage('file with spaces.cpp', 'int* value = 0;\n')
        result = self.run_hook(CLANG_TIDY=shutil.which('clang-tidy'))
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'modernize-use-nullptr', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
