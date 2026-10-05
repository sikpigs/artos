import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock

EXTENSION = Path(__file__).resolve().parents[1]
ROOT = EXTENSION.parent
spec = importlib.util.spec_from_file_location('artos_probe', EXTENSION / 'probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProbeTests(unittest.TestCase):
    def test_detection_reads_full_id_and_matches_flash_size(self):
        run = Mock(return_value=subprocess.CompletedProcess([], 0,
            'ARTOS_DEVICE_ID=0x10006431\nARTOS_FLASH_KIB=512\n', ''))
        target = probe.detect('ABC123', '/tools/openocd', run)
        self.assertEqual(target['board'], 'nucleo_f411re')
        self.assertEqual(target['chip_id'], 0x431)
        command = run.call_args.args[0]
        self.assertIn('adapter serial ABC123', command)
        self.assertIn('reset_config none', command)
        self.assertNotIn('reset', command)
        self.assertEqual(command[0], '/tools/openocd')

    def test_f42_f43_family_is_inferred_and_smaller_flash_is_rejected(self):
        for flash, expected in [(2048, 'nucleo_f439zi'), (1024, None)]:
            run = Mock(return_value=subprocess.CompletedProcess([], 0, '',
                f'ARTOS_DEVICE_ID=1049\nARTOS_FLASH_KIB={flash}\n'))
            target = probe.detect('ABC123', run=run)
            self.assertEqual(target['board'], expected)
            self.assertTrue(target['inferred'])
            if expected:
                self.assertEqual(probe.resolve_board('auto', target), expected)
            else:
                with self.assertRaises(ValueError):
                    probe.resolve_board('nucleo_f439zi', target)

    def test_injection_and_mismatched_targets_fail_before_execution(self):
        run = Mock()
        for serial in ['', 'ABC; shutdown', 'ABC\nreset', '[exec something]']:
            with self.assertRaises(ValueError):
                probe.detect(serial, run=run)
        run.assert_not_called()
        with self.assertRaises(ValueError):
            probe.resolve_board('nucleo_f439zi', dict(board='nucleo_f411re', chip_id=0x431,
                                                    flash_kib=512, family='STM32F411'))

    def test_failed_and_incomplete_detection_does_not_guess(self):
        for result in [subprocess.CompletedProcess([], 1, '', 'busy'),
                       subprocess.CompletedProcess([], 0, 'unrelated idcode: 0x431', ''),
                       subprocess.CompletedProcess([], 0, 'ARTOS_DEVICE_ID=0x431', '')]:
            with self.assertRaises(ValueError):
                probe.detect('ABC123', run=Mock(return_value=result))

    def test_usb_discovery_filters_vendor_and_product_and_preserves_serial(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, vendor, product, serial in [('1-1', '0483', '374b', 'ABC123'),
                                                   ('1-2', '0483', '1234', 'OTHER'),
                                                   ('1-3', '1234', '374b', 'OTHER')]:
                device = root / name
                device.mkdir()
                for key, value in [('idVendor', vendor), ('idProduct', product), ('serial', serial)]:
                    (device / key).write_text(value)
            self.assertEqual(probe.linux_probes(root), [dict(serial='ABC123', product='ST-LINK')])

    def test_prepare_uses_saved_seed_and_pins_serial_without_a_shell(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / 'build/release'
            build.mkdir(parents=True)
            (build / 'CMakeCache.txt').write_text('KCONFIG_DEFCONFIG:FILEPATH=/custom seed\n')
            run = Mock()
            probe.prepare(root, 'release', 'nucleo_f439zi', 'ABC123', '/open ocd', run)
            generation, configure = [call.args[0] for call in run.call_args_list]
            self.assertIn('/custom seed', generation)
            self.assertIn('--board', generation)
            self.assertIn('OPENOCD_SERIAL=ABC123', configure)
            self.assertIn('OPENOCD_EXECUTABLE=/open ocd', configure)
            self.assertIn(f'Python3_EXECUTABLE={sys.executable}', configure)

    def test_monitor_port_is_matched_to_selected_probe_not_first_tty(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tty_root = root / 'tty'
            tty_root.mkdir()
            for index, serial in enumerate(('PROBE111', 'PROBE222')):
                usb = root / serial
                usb.mkdir()
                (usb / 'serial').write_text(serial)
                interface = usb / 'interface'
                interface.mkdir()
                tty = tty_root / f'ttyACM{index}'
                tty.mkdir()
                (tty / 'device').symlink_to(interface)
            self.assertEqual(probe.serial_port('PROBE222', tty_root), '/dev/ttyACM1')
            with self.assertRaises(ValueError):
                probe.serial_port('MISSING', tty_root)

    def test_switching_board_preserves_saved_software_settings(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / '.config').write_text('CONFIG_BOARD_NUCLEO_F411RE=y\nCONFIG_ARTOS=y\nCONFIG_ARTOS_TICK_HZ=250\nCONFIG_MONITOR=y\nCONFIG_CONSOLE_RX_BUFFER_SIZE=512\n')
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/configure.py'),
                '--source', str(ROOT), '--build', str(build), '--defconfig',
                str(ROOT / 'configs/nucleo_f439zi_defconfig'), '--build-type', 'Debug',
                '--board', 'nucleo_f439zi'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            config = (build / '.config').read_text()
            self.assertIn('CONFIG_BOARD_NUCLEO_F439ZI=y', config)
            self.assertIn('CONFIG_ARTOS_TICK_HZ=250', config)
            self.assertIn('CONFIG_CONSOLE_RX_BUFFER_SIZE=512', config)
            self.assertIn('stm32f439zi', (build / 'generated/config.cmake').read_text())


if __name__ == '__main__':
    unittest.main()
