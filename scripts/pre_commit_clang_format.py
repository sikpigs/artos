#!/usr/bin/env python3
"""Compare staged C/C++ contents with clang-format, leaving all files untouched."""

import difflib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


EXTENSIONS = {'.c', '.cc', '.cpp', '.cxx', '.C', '.h', '.hh', '.hpp', '.hxx'}


def git(*args):
    return subprocess.check_output(['git', *args])


def main():
    root = Path(os.fsdecode(git('rev-parse', '--show-toplevel')).rstrip('\n'))
    os.chdir(root)
    sources = [os.fsdecode(name) for name in git(
        'diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'
    ).split(b'\0') if name and Path(os.fsdecode(name)).suffix in EXTENSIONS]
    if not sources:
        return 0
    formatter = shutil.which(os.environ.get('CLANG_FORMAT', 'clang-format'))
    if not formatter:
        raise RuntimeError('clang-format is missing. Install clang-format or set CLANG_FORMAT.')

    # Materialize only staged style files, so unstaged configuration changes
    # cannot affect the commit. Preserve directory nesting for style lookup.
    with tempfile.TemporaryDirectory(prefix='pre-commit-clang-format-') as temporary:
        snapshot = Path(temporary)
        for entry in git('ls-files', '--stage', '-z').split(b'\0'):
            if not entry:
                continue
            metadata, name_bytes = entry.split(b'\t', 1)
            name = os.fsdecode(name_bytes)
            if Path(name).name not in {'.clang-format', '_clang-format'}:
                continue
            mode, oid, stage = metadata.split()
            if stage != b'0' or mode not in {b'100644', b'100755'}:
                raise RuntimeError(f'Style file must be a regular, resolved file: {name}')
            destination = snapshot / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(git('cat-file', 'blob', oid.decode()))

        failed = False
        for name in sources:
            original = git('show', f':{name}')
            assumed = snapshot / name
            assumed.parent.mkdir(parents=True, exist_ok=True)
            result = subprocess.run([
                formatter, '--style=file', '--fallback-style=none',
                f'--assume-filename={assumed}',
            ], input=original, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode:
                failed = True
                print(f'clang-format failed for {name}:', file=sys.stderr)
                sys.stderr.write(result.stderr.decode('utf-8', errors='replace'))
                continue
            if result.stdout == original:
                continue
            failed = True
            print(f'Formatting required: {name}', file=sys.stderr)
            sys.stderr.writelines(difflib.unified_diff(
                original.decode('utf-8', errors='replace').splitlines(keepends=True),
                result.stdout.decode('utf-8', errors='replace').splitlines(keepends=True),
                fromfile=f'a/{name}', tofile=f'b/{name}',
            ))
        if failed:
            print('\nCommit blocked: fix the formatting/errors above and stage the changes. '
                  'The hook did not modify your files or index.', file=sys.stderr)
        return int(failed)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Commit blocked: {error}', file=sys.stderr)
        sys.exit(1)
