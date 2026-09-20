"""Command-line workflows for configuring, building, and using ARTOS firmware."""

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Callable, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PRESET = os.environ.get("ARTOS_PRESET", "debug")
CHECK_TARGETS = (
    "bsp-checks",
    "configuration-checks",
    "kconfig-checks",
    "devicetree-checks",
    "regmap-checks",
)

RunCommand = Callable[[Sequence[str]], None]


class CommandRunner:
    """Print and execute commands without involving a shell."""

    def __init__(self, root: Path, dry_run: bool = False):
        self.root = root
        self.dry_run = dry_run

    def __call__(self, command: Sequence[str]) -> None:
        print(f"+ {shlex.join(command)}", flush=True)
        if not self.dry_run:
            subprocess.run(command, cwd=self.root, check=True)


@dataclass(frozen=True)
class Context:
    preset: str
    run: RunCommand

    def configure(self, extra_args: Sequence[str] = ()) -> None:
        self.run(("cmake", "--preset", self.preset, *extra_args))

    def build(self, targets: Sequence[str] = (), extra_args: Sequence[str] = ()) -> None:
        command = ["cmake", "--build", "--preset", self.preset]
        if targets:
            command.extend(("--target", *targets))
        command.extend(extra_args)
        self.run(command)


def add_build_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("-j", "--jobs", type=int, help="number of parallel build jobs")
    parser.add_argument("-v", "--verbose", action="store_true", help="show build commands")


def build_options(args: argparse.Namespace) -> list[str]:
    options = []
    if getattr(args, "jobs", None) is not None:
        options.extend(("--parallel", str(args.jobs)))
    if getattr(args, "verbose", False):
        options.append("--verbose")
    return options


def configured_build(context: Context, args: argparse.Namespace,
                     targets: Sequence[str]) -> None:
    context.configure()
    context.build(targets, build_options(args))


def run_configure(context: Context, args: argparse.Namespace) -> None:
    options = []
    if args.fresh:
        options.append("--fresh")
    options.extend(f"-D{definition}" for definition in args.define)
    context.configure(options)


def run_build(context: Context, args: argparse.Namespace) -> None:
    configured_build(context, args, args.targets)


def run_target(context: Context, args: argparse.Namespace) -> None:
    configured_build(context, args, (args.target,))


def run_check(context: Context, args: argparse.Namespace) -> None:
    configured_build(context, args, CHECK_TARGETS)


def project_python() -> str:
    virtualenv_python = ROOT / ".venv" / "bin" / "python"
    return str(virtualenv_python) if virtualenv_python.is_file() else sys.executable


def run_monitor(context: Context, args: argparse.Namespace) -> None:
    command = [project_python(), str(ROOT / "scripts" / "serial_console.py")]
    if args.port:
        command.extend(("--port", args.port))
    context.run(command)


def run_flash_monitor(context: Context, args: argparse.Namespace) -> None:
    configured_build(context, args, ("flash",))
    run_monitor(context, args)


def run_tests(context: Context, args: argparse.Namespace) -> None:
    del args
    context.run(("cmake", "--preset", "host-tests"))
    context.run(("cmake", "--build", "--preset", "host-tests"))
    context.run(("ctest", "--preset", "all"))


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="artos.py",
        description="Configure, build, verify, and run ARTOS firmware workflows.",
    )
    parser.add_argument(
        "-p", "--preset", default=DEFAULT_PRESET, metavar="NAME",
        help=f"CMake configure/build preset (default: {DEFAULT_PRESET})",
    )
    parser.add_argument(
        "-n", "--dry-run", action="store_true",
        help="print commands without running them",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    configure = subparsers.add_parser("configure", help="configure the selected preset")
    configure.add_argument("--fresh", action="store_true", help="discard the CMake cache first")
    configure.add_argument(
        "-D", "--define", action="append", default=[], metavar="NAME=VALUE",
        help="set a CMake cache value (repeatable)",
    )
    configure.set_defaults(handler=run_configure)

    build = subparsers.add_parser("build", help="configure and build firmware")
    build.add_argument("targets", nargs="*", metavar="TARGET", help="optional CMake targets")
    add_build_options(build)
    build.set_defaults(handler=run_build)

    target_commands = {
        "menuconfig": ("menuconfig", "edit board and feature configuration"),
        "save-defconfig": ("savedefconfig", "write a minimal config to the build directory"),
        "clean": ("clean", "remove products for the selected preset"),
        "flash": ("flash", "build, program, verify, and reset the board"),
        "debug-server": ("debug-server", "start OpenOCD for a debugger"),
    }
    for name, (target, help_text) in target_commands.items():
        command = subparsers.add_parser(name, help=help_text)
        command.set_defaults(handler=run_target, target=target)
        if name in ("clean", "flash"):
            add_build_options(command)

    monitor = subparsers.add_parser("monitor", help="open the ST-LINK serial console")
    monitor.add_argument("--port", help="serial device (otherwise auto-detected)")
    monitor.set_defaults(handler=run_monitor)

    flash_monitor = subparsers.add_parser(
        "flash-monitor", help="flash, then open the serial console")
    flash_monitor.add_argument("--port", help="serial device (otherwise auto-detected)")
    add_build_options(flash_monitor)
    flash_monitor.set_defaults(handler=run_flash_monitor)

    check = subparsers.add_parser("check", help="run firmware compile and generation checks")
    add_build_options(check)
    check.set_defaults(handler=run_check)

    tests = subparsers.add_parser("test", help="run native and Python tests")
    tests.set_defaults(handler=run_tests)
    return parser


def main(argv: Sequence[str] | None = None, run: RunCommand | None = None) -> int:
    parser = create_parser()
    args = parser.parse_args(argv)
    runner = run or CommandRunner(ROOT, args.dry_run)
    context = Context(args.preset, runner)
    try:
        args.handler(context, args)
    except subprocess.CalledProcessError as error:
        return error.returncode
    except FileNotFoundError as error:
        parser.error(f"required program not found: {error.filename}")
    except KeyboardInterrupt:
        return 130
    return 0
