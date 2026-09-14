#!/usr/bin/env python3
"""Linux/POSIX serial terminal for the Nucleo ST-LINK VCP; no dependencies."""

import argparse
import glob
import os
import select
import sys
import termios
import tty


def find_port():
    ports = sorted(p for p in glob.glob('/dev/serial/by-id/*')
                   if 'stlink' in p.lower() or 'st-link' in p.lower())
    if len(ports) == 1:
        return ports[0]
    if not ports:
        raise ValueError('No ST-LINK serial port found. Connect the board, or pass --port /dev/ttyACM0.')
    raise ValueError('Multiple ST-LINK ports found; select one with --port:\n' + '\n'.join(ports))


def console(port):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    serial_settings = termios.tcgetattr(fd)
    stdin_settings = None
    try:
        # Exclusive access prevents another terminal from stealing monitor input.
        import fcntl
        fcntl.ioctl(fd, termios.TIOCEXCL)
        tty.setraw(fd)
        settings = termios.tcgetattr(fd)
        settings[2] &= ~(termios.PARENB | termios.CSTOPB | termios.CSIZE | termios.CRTSCTS)
        settings[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD
        settings[4] = settings[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, settings)
        os.set_blocking(fd, True)
        print(f'{port} — 115200 8N1, no flow control. Ctrl+] exits.', flush=True)
        if sys.stdin.isatty():
            stdin_settings = termios.tcgetattr(sys.stdin.fileno())
            tty.setraw(sys.stdin.fileno())
        while True:
            ready, _, _ = select.select([fd, sys.stdin.fileno()], [], [])
            if fd in ready:
                data = os.read(fd, 4096)
                if not data:
                    raise OSError('Board disconnected')
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            if sys.stdin.fileno() in ready:
                data = os.read(sys.stdin.fileno(), 4096)
                if not data or b'\x1d' in data:
                    return
                while data:
                    data = data[os.write(fd, data):]
    finally:
        if stdin_settings is not None:
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSANOW, stdin_settings)
        try:
            termios.tcsetattr(fd, termios.TCSANOW, serial_settings)
        except (OSError, termios.error):
            pass  # USB may already have disconnected.
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default=os.environ.get('NUCLEO_SERIAL_PORT'),
                        help='Serial device path (default: NUCLEO_SERIAL_PORT or ST-LINK discovery)')
    args = parser.parse_args()
    try:
        console(args.port or find_port())
    except (OSError, ValueError, termios.error) as error:
        print(f'\nSerial console: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
