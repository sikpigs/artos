#!/usr/bin/env python3
"""Flash and validate queue shutdown with real blocked threads on Nucleo hardware."""
import argparse
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
GDB_SCRIPT = Path(__file__).with_name("queue_gdb.py")
BOARDS = {
    "nucleo_f411re": (ROOT / "bsp/boards/st/nucleo_f411re/openocd.cfg", 0x20020000),
    "nucleo_f439zi": (ROOT / "bsp/boards/st/nucleo_f439zi/openocd.cfg", 0x20030000),
}


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def require_elf(elf, board, nm):
    symbols = {}
    for line in subprocess.check_output([nm, str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    if "queue_hw_done" not in symbols or "queue_hw_result" not in symbols:
        raise RuntimeError("ELF is not the queue hardware test image")
    if symbols.get("_estack") != BOARDS[board][1]:
        raise RuntimeError(f"ELF stack top does not match {board}")


def wait_for_openocd(process, port):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("OpenOCD exited before opening its GDB port")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("OpenOCD did not open its GDB port")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", required=True, choices=BOARDS)
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--openocd", default="openocd")
    parser.add_argument("--gdb", default="gdb-multiarch")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args()
    elf = args.elf.resolve()
    if not elf.is_file():
        parser.error(f"ELF not found: {elf}")
    require_elf(elf, args.board, args.nm)

    port = free_port()
    command = [args.openocd, "-f", str(BOARDS[args.board][0]),
               "-c", f"gdb_port {port}", "-c", "telnet_port disabled",
               "-c", "tcl_port disabled"]
    process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True)
    try:
        wait_for_openocd(process, port)
        environment = os.environ.copy()
        environment["ARTOS_GDB_TARGET"] = f"127.0.0.1:{port}"
        result = subprocess.run(
            [args.gdb, "-nx", "--batch", "-q", str(elf),
             "-ex", f"python exec(compile(open({str(GDB_SCRIPT)!r}).read(), "
                    f"{str(GDB_SCRIPT)!r}, 'exec'))"],
            cwd=ROOT, env=environment, capture_output=True, text=True, timeout=45)
        output = result.stdout + result.stderr
        if result.returncode != 0 or "PASS queue:" not in output:
            raise RuntimeError(output)
        print(next(line for line in output.splitlines() if line.startswith("PASS queue:")))
    finally:
        process.terminate()
        try:
            output, _ = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()
        if process.returncode not in (0, -15) and output:
            print(output, file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"hardware test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
