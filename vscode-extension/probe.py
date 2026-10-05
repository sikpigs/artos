#!/usr/bin/env python3
"""USB discovery and firmware operations for the ARTOS VS Code extension."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

BOARDS = {
    "nucleo_f411re": (0x431, "STM32F411", 512),
    "nucleo_f439zi": (0x419, "STM32F42x/F43x", 2048),
}
PRESETS = {"debug": "Debug", "release": "Release",
           "rel-with-deb-info": "RelWithDebInfo", "min-size": "MinSizeRel"}
USB_IDS = {0x3748, 0x374B, 0x374D, 0x374E, 0x374F, 0x3752, 0x3753, 0x3754}


def validate_serial(serial):
    if not serial or not re.fullmatch(r"[A-Za-z0-9]{1,64}", serial):
        raise ValueError("Select a probe with an alphanumeric USB serial number. Older ST-LINK firmware may need updating.")
    return serial


def linux_probes(usb_root=Path('/sys/bus/usb/devices')):
    probes = []
    for device in sorted(usb_root.iterdir()):
        try:
            if int((device / 'idVendor').read_text().strip(), 16) != 0x0483:
                continue
            if int((device / 'idProduct').read_text().strip(), 16) not in USB_IDS:
                continue
            serial = (device / 'serial').read_text().strip()
            product = (device / 'product').read_text().strip() if (device / 'product').exists() else 'ST-LINK'
            probes.append(dict(serial=validate_serial(serial), product=product))
        except (OSError, ValueError):
            continue
    return probes


def list_probes():
    # Linux sysfs needs no USB library and does not connect to or halt the target.
    if sys.platform.startswith('linux'):
        probes = linux_probes()
    else:
        try:
            import usb.core
            import usb.util
        except ImportError as error:
            raise ValueError('Install vscode-extension/requirements.txt into the configured Python environment; PyUSB also needs libusb.') from error
        probes = []
        for device in usb.core.find(find_all=True, idVendor=0x0483):
            if device.idProduct not in USB_IDS:
                continue
            try:
                probes.append(dict(serial=validate_serial(device.serial_number),
                                   product=device.product or 'ST-LINK'))
            finally:
                usb.util.dispose_resources(device)
    serials = [probe['serial'] for probe in probes]
    if len(set(serials)) != len(serials):
        raise ValueError('Two ST-LINK devices report the same serial number; they cannot be selected reliably.')
    return probes


def detect(serial, openocd='openocd', run=subprocess.run):
    validate_serial(serial)
    command = [openocd, '-f', 'board/st_nucleo_f4.cfg',
               '-c', f'adapter serial {serial}', '-c', 'adapter speed 950',
               '-c', 'reset_config none', '-c', 'gdb_port disabled',
               '-c', 'tcl_port disabled', '-c', 'telnet_port disabled',
               '-c', 'init',
               '-c', 'echo ARTOS_DEVICE_ID=[$_TARGETNAME read_memory 0xE0042000 32 1]',
               '-c', 'echo ARTOS_FLASH_KIB=[$_TARGETNAME read_memory 0x1FFF7A22 16 1]',
               '-c', 'shutdown']
    result = run(command, capture_output=True, text=True, timeout=20)
    output = result.stdout + result.stderr
    if result.returncode:
        raise ValueError('Cannot identify the selected target. Check USB permissions, target power, and close other debug sessions.\n' + output[-3000:])
    match = re.search(r'ARTOS_DEVICE_ID=(0x[0-9a-fA-F]+|[0-9]+)', output)
    if not match:
        raise ValueError('OpenOCD did not return a target device ID.\n' + output[-3000:])
    chip_id = int(match[1], 16 if match[1].startswith('0x') else 10) & 0xFFF
    size = re.search(r'ARTOS_FLASH_KIB=(0x[0-9a-fA-F]+|[0-9]+)', output)
    if not size:
        raise ValueError('OpenOCD did not return the target flash size.')
    flash_kib = int(size[1], 16 if size[1].startswith('0x') else 10)
    compatible = next((name for name, data in BOARDS.items() if data[0] == chip_id), None)
    board = compatible if compatible and BOARDS[compatible][2] == flash_kib else None
    family = BOARDS[compatible][1] if compatible else 'Unknown/unsupported STM32'
    return dict(serial=serial, chip_id=chip_id, family=family, board=board,
                flash_kib=flash_kib, inferred=True)


def resolve_board(requested, target):
    board = target['board'] if requested == 'auto' else requested
    if board not in BOARDS:
        raise ValueError('Target is unsupported or could not be inferred. Supported targets are STM32F411 and STM32F42x/F43x.')
    if BOARDS[board][0] != target['chip_id'] or BOARDS[board][2] != target['flash_kib']:
        raise ValueError(f"Selected board {board} does not match detected {target['family']} (0x{target['chip_id']:03x}).")
    return board


def cache_value(root, preset, key, default=None):
    cache = root / 'build' / preset / 'CMakeCache.txt'
    if cache.exists():
        match = re.search(r'^' + re.escape(key) + r':[^=]+=(.*)$', cache.read_text(), re.MULTILINE)
        if match:
            return match[1]
    return default


def prepare(root, preset, board, serial, openocd, run=subprocess.run):
    seed = cache_value(root, preset, 'KCONFIG_DEFCONFIG', str(root / 'configs' / f'{board}_defconfig'))
    run([sys.executable, str(root / 'scripts/configure.py'), '--source', str(root),
         '--build', str(root / 'build' / preset), '--defconfig', seed,
         '--build-type', PRESETS[preset], '--board', board], cwd=root, check=True)
    command = [sys.executable, str(root / 'artos.py'), '--preset', preset, 'configure',
               '-D', f'OPENOCD_EXECUTABLE={openocd}',
               '-D', f'Python3_EXECUTABLE={sys.executable}']
    if serial:
        command.extend(['-D', f'OPENOCD_SERIAL={validate_serial(serial)}'])
    run(command, cwd=root, check=True)


def serial_port(serial, tty_root=Path('/sys/class/tty')):
    ports = []
    for tty in tty_root.iterdir():
        device = (tty / 'device').resolve()
        for parent in (device, *device.parents):
            try:
                if (parent / 'serial').read_text().strip() == serial:
                    ports.append('/dev/' + tty.name)
                    break
            except OSError:
                pass
    if len(ports) != 1:
        raise ValueError('Cannot find a unique serial console for the selected ST-LINK.')
    return ports[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('list', 'detect', 'build', 'flash', 'flash-monitor', 'debug-info'))
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--preset', choices=PRESETS, default='debug')
    parser.add_argument('--board', choices=('auto', *BOARDS), default='auto')
    parser.add_argument('--serial')
    parser.add_argument('--openocd', default='openocd')
    args = parser.parse_args()
    try:
        root = args.root.resolve()
        if not (root / 'artos.py').is_file():
            raise ValueError('Project root must contain artos.py.')
        if args.action == 'list':
            print(json.dumps(list_probes()))
            return 0
        if args.action == 'debug-info':
            output = cache_value(root, args.preset, 'ARTOS_OUTPUT_NAME', 'artos')
            print(json.dumps(dict(executable=str(root / 'build' / args.preset / f'{output}.elf'))))
            return 0
        if args.action == 'detect':
            print(json.dumps(detect(args.serial, args.openocd)))
            return 0
        target = detect(args.serial, args.openocd) if args.serial else None
        if target:
            board = resolve_board(args.board, target)
            print(f"Detected {target['family']}; using {board} (board inferred from MCU).", flush=True)
        elif args.action == 'build' and args.board != 'auto':
            board = args.board
        else:
            raise ValueError('Select an ST-LINK probe, or choose a board explicitly for an offline build.')
        # Resolve the console before flashing; never open another probe's UART.
        port = None
        if args.action == 'flash-monitor':
            if not sys.platform.startswith('linux'):
                raise ValueError('The project serial monitor currently supports Linux; use a separate serial terminal on this platform.')
            port = serial_port(args.serial)
        prepare(root, args.preset, board, args.serial, args.openocd)
        command = [sys.executable, str(root / 'artos.py'), '--preset', args.preset,
                   args.action]
        if port:
            command.extend(['--port', port])
        subprocess.run(command, cwd=root, check=True)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'ARTOS: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
