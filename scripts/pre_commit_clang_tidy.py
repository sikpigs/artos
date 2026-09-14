#!/usr/bin/env python3
"""Analyze an isolated copy of the index; never apply fixes or change staging."""

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile


SOURCES = {'.c', '.cc', '.cpp', '.cxx', '.C'}
HEADERS = {'.h', '.hh', '.hpp', '.hxx'}


def git(*args):
    return subprocess.check_output(['git', *args])


def arm_arguments(command):
    """Discover the installed GNU Arm library headers, including multilib paths."""
    compiler = command[0]
    if not Path(compiler).name.startswith('arm-none-eabi-'):
        return []
    flags = [arg for arg in command if arg.startswith(
        ('-mcpu=', '-march=', '-mfpu=', '-mfloat-abi=')) or arg in {'-mthumb', '-marm'}]
    result = subprocess.run([compiler, *flags, '-E', '-x', 'c++', '-v', os.devnull],
                            capture_output=True, text=True, check=True,
                            env={**os.environ, 'LC_ALL': 'C'})
    paths = result.stderr.split('#include <...> search starts here:', 1)
    if len(paths) != 2 or 'End of search list.' not in paths[1]:
        raise RuntimeError(f'Cannot discover system headers from {compiler}')
    includes = paths[1].split('End of search list.', 1)[0].splitlines()
    # Keep Clang's intrinsic headers; use GCC's C++ library and newlib headers.
    return ['--extra-arg=--target=arm-none-eabi', *[
        f'--extra-arg=-isystem{Path(path.strip()).resolve()}' for path in includes
        if path.strip() and '/lib/gcc/' not in str(Path(path.strip()).resolve())
    ]]


def main():
    root = Path(os.fsdecode(git('rev-parse', '--show-toplevel')).rstrip('\n'))
    os.chdir(root)
    changed = [os.fsdecode(name) for name in git(
        'diff', '--cached', '--name-only', '-z').split(b'\0') if name]
    if not any(Path(name).suffix in SOURCES | HEADERS or
               Path(name).name == '.clang-tidy' for name in changed):
        return 0
    tidy = shutil.which(os.environ.get('CLANG_TIDY', 'clang-tidy'))
    if not tidy:
        raise RuntimeError('clang-tidy is missing. Install it or set CLANG_TIDY.')
    explicit_dirs = os.environ.get('CLANG_TIDY_BUILD_DIRS')
    explicit_dir = os.environ.get('CLANG_TIDY_BUILD_DIR')
    if explicit_dirs:
        candidates = [Path(path) / 'compile_commands.json'
                      for path in explicit_dirs.split(os.pathsep) if path]
    elif explicit_dir:
        candidates = [Path(explicit_dir) / 'compile_commands.json']
    else:
        # Prefer the active preset for shared sources, then fill coverage gaps.
        candidates = ([root / 'compile_commands.json']
                      if (root / 'compile_commands.json').is_file() else [])
        candidates += sorted((root / 'build').glob('*/compile_commands.json'))
    databases = []
    for candidate in candidates:
        if not candidate.is_file():
            raise RuntimeError(f'compile_commands.json is missing: {candidate}. Configure that build.')
        database = candidate.resolve()
        if database.parent == root:
            raise RuntimeError('Use an out-of-source compilation database via CLANG_TIDY_BUILD_DIR.')
        if database not in databases:
            databases.append(database)
    if not databases:
        raise RuntimeError('compile_commands.json is missing. Configure board and host builds, '
                           'or set CLANG_TIDY_BUILD_DIRS.')

    with tempfile.TemporaryDirectory(prefix='pre-commit-clang-tidy-') as temporary:
        snapshot = Path(temporary) / 'source'
        snapshot.mkdir()
        git('checkout-index', '--all', f'--prefix={snapshot}/')
        if not (snapshot / '.clang-tidy').is_file():
            raise RuntimeError('Stage the root .clang-tidy configuration before committing.')
        # Reject symlinks rather than reading an unstaged/external source target.
        for path in snapshot.rglob('*'):
            if path.is_symlink():
                raise RuntimeError(f'Staged symlink is unsupported: {path.relative_to(snapshot)}')
        available = {}
        for number, database in enumerate(databases):
            # Preserve build-relative paths and generated headers for each command.
            build = (snapshot / database.parent.relative_to(root)
                     if database.parent.is_relative_to(root)
                     else Path(temporary) / f'build-{number}')
            shutil.copytree(database.parent, build, dirs_exist_ok=True)

            def relocate(value):
                if database.parent.is_relative_to(root):
                    return value.replace(str(root), str(snapshot))
                return value.replace(str(database.parent), str(build)).replace(str(root), str(snapshot))

            rewritten = []
            for entry in json.loads(database.read_text()):
                directory = Path(entry['directory'])
                source = (directory / entry['file']).resolve()
                if not source.is_relative_to(root) or source.suffix not in SOURCES:
                    continue
                name = str(source.relative_to(root))
                if not (snapshot / name).is_file() or name in available:
                    continue
                arguments = entry.get('arguments') or shlex.split(entry['command'])
                # GCC's fault-handler code-generation restriction is unsupported
                # by Clang's ARM frontend. Drop it only in the analysis snapshot;
                # the real firmware database and build retain the restriction.
                if Path(arguments[0]).name.startswith('arm-none-eabi-'):
                    arguments = [arg for arg in arguments if arg != '-mgeneral-regs-only']
                rewritten.append({'directory': relocate(str(directory)),
                                  'file': str(snapshot / name),
                                  'arguments': [relocate(arg) for arg in arguments]})
                # Select one consistent command per source, including its target.
                available[name] = (arguments, build)
            (build / 'compile_commands.json').write_text(json.dumps(rewritten))
        all_units = any(Path(name).suffix in HEADERS or Path(name).name == '.clang-tidy'
                        for name in changed)
        selected = sorted(available) if all_units else [
            name for name in changed if Path(name).suffix in SOURCES and (snapshot / name).is_file()]
        missing = [name for name in changed if Path(name).suffix in SOURCES
                   and (snapshot / name).is_file() and name not in available]
        if missing:
            raise RuntimeError('Staged sources missing from compilation database: '
                               + ', '.join(missing) + '. Configure the matching board/host builds or set '
                               'CLANG_TIDY_BUILD_DIRS (path-separated build directories).')
        if all_units and not selected:
            raise RuntimeError('No staged translation units match the compilation database. Reconfigure it.')
        failed = False
        extras_cache = {}
        for name in selected:
            arguments, build = available[name]
            command = tuple(arguments)
            key = tuple(arg for arg in command if arg == command[0] or arg.startswith('-m'))
            if key not in extras_cache:
                extras_cache[key] = arm_arguments(command)
            print(f'clang-tidy (staged): {name}', flush=True)
            result = subprocess.run([tidy, '-p', str(build), str(snapshot / name),
                                     '--warnings-as-errors=*', *extras_cache[key]], cwd=snapshot)
            failed |= result.returncode != 0
        if failed:
            print('Commit blocked: clang-tidy reported diagnostics or analysis errors above. '
                  'Fix and stage the affected code, or correct the toolchain/build configuration. '
                  'Working files and the index were not modified.', file=sys.stderr)
        return int(failed)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Commit blocked: {error}', file=sys.stderr)
        sys.exit(1)
