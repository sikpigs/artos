"""Exercise the terminal against a POSIX pseudo-terminal, without hardware."""

import importlib.util
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import termios
import time
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts' / 'serial_console.py'
spec = importlib.util.spec_from_file_location('serial_console', SCRIPT)
serial_console = importlib.util.module_from_spec(spec)
spec.loader.exec_module(serial_console)


def read_until(fd, expected):
    data = b''
    deadline = time.monotonic() + 5
    while expected not in data:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
            raise AssertionError(f'Timed out waiting for {expected!r}: {data!r}')
        chunk = os.read(fd, 4096)
        if not chunk:
            raise AssertionError(f'Unexpected EOF: {data!r}')
        data += chunk
    return data


class SerialConsoleTests(unittest.TestCase):
    def test_discovery(self):
        with patch.object(serial_console.glob, 'glob', return_value=[]):
            with self.assertRaisesRegex(ValueError, 'No ST-LINK'):
                serial_console.find_port()
        port = '/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_123'
        with patch.object(serial_console.glob, 'glob', return_value=[port]):
            self.assertEqual(serial_console.find_port(), port)
        with patch.object(serial_console.glob, 'glob', return_value=[port, port + '456']):
            with self.assertRaisesRegex(ValueError, 'Multiple'):
                serial_console.find_port()

    def test_bidirectional_terminal_and_restore(self):
        master, slave = pty.openpty()
        settings = termios.tcgetattr(slave)
        process = subprocess.Popen([sys.executable, str(SCRIPT), '--port', os.ttyname(slave)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        try:
            read_until(process.stdout.fileno(), b'Ctrl+] exits.')
            self.assertEqual(termios.tcgetattr(slave)[4], termios.B115200)
            os.write(master, b'monitor ready\r\n')
            read_until(process.stdout.fileno(), b'monitor ready\r\n')
            process.stdin.write(b'20000000\r')
            process.stdin.flush()
            self.assertEqual(read_until(master, b'20000000\r'), b'20000000\r')
            process.stdin.write(b'\x1d')
            process.stdin.flush()
            self.assertEqual(process.wait(timeout=5), 0, process.stderr.read())
            self.assertEqual(termios.tcgetattr(slave), settings)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdin.close()
            process.stdout.close()
            process.stderr.close()
            os.close(slave)
            os.close(master)


if __name__ == '__main__':
    unittest.main()
