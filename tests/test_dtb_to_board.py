#!/usr/bin/env python3
"""Integration tests using real dtc/libfdt, including deliberately invalid bindings."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dtb_to_board', ROOT / 'scripts/dtb_to_board.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)


class BoardGeneration(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def generate(self, chip='stm32f411', board='nucleo_f411re', overlay='', soc_replace=None, expected_board=None):
        soc = ROOT / 'bsp/soc/st/stm32f4' / chip / 'dts'
        source = (soc / (chip + '.dtsi')).read_text()
        if soc_replace:
            self.assertIn(soc_replace[0], source)
            source = source.replace(*soc_replace)
        (self.work / (chip + '.dtsi')).write_text(source)
        dts = (ROOT / 'bsp/boards/st' / board / 'board.dts').read_text() + '\n' + overlay
        (self.work / 'board.dts').write_text(dts)
        subprocess.run(['dtc', '-I', 'dts', '-O', 'dtb', '-i', str(self.work),
                        '-o', str(self.work / 'board.dtb'), str(self.work / 'board.dts')],
                       check=True, capture_output=True)
        gen.generate(self.work / 'board.dtb', soc / 'bindings.json', expected_board or board, self.work / 'out')
        return ((self.work / 'out/board.h').read_text(), (self.work / 'out/board_devices.h').read_text())

    def test_both_boards_deterministic(self):
        for chip, board, pin, uart in [('stm32f411', 'nucleo_f411re', 'PA<2>', 'usart2'),
                                       ('stm32f439', 'nucleo_f439zi', 'PD<8>', 'usart3')]:
            with self.subTest(board=board):
                first = self.generate(chip, board)
                self.assertEqual(first, self.generate(chip, board))
                self.assertIn(pin, first[0])
                self.assertIn(f'{chip}::usart::{uart}_device', first[1])
                self.assertNotIn('enable_', first[1])

    def test_description_changes_headers_and_devices(self):
        header, devices = self.generate('stm32f439', 'nucleo_f439zi',
            '&uart3 { current-speed = <57600>; };\n&heartbeat { gpios = <&gpiod 12 0>; };')
        self.assertIn('baud_rate = 57600', header)
        self.assertIn('PD<12>', header)
        self.assertIn('heartbeat = chosen::heartbeat::pin', devices)
        self.assertNotIn('enable_', devices)

    def test_chosen_selects_another_stopwatch_timer(self):
        for chip, board in [('stm32f411', 'nucleo_f411re'), ('stm32f439', 'nucleo_f439zi')]:
            with self.subTest(board=board):
                header, devices = self.generate(chip, board, overlay='''/ {
                    chosen { artos,stopwatch = <&tim5>; };
                };
                &tim5 { status = "okay"; };
                &tim2 { status = "disabled"; };
                ''')
                self.assertIn('address = 0x40000c00UL', header)
                self.assertIn('irq_t::tim5', header)
                self.assertIn(f'{chip}::timer::timer5', devices)

    def test_chosen_selects_another_led(self):
        header, _ = self.generate(overlay='''/ {
            leds { other: other { gpios = <&gpioa 6 0>; }; };
            chosen { artos,heartbeat = <&other>; };
        };
        &heartbeat { status = "disabled"; };
        ''')
        self.assertIn('PA<6>', header)
        self.assertNotIn('PA<5>', header)

    def test_reject_invalid_selected_devices(self):
        cases = [
            ('&uart2 { status = "disabled"; };', 'disabled device'),
            ('&uart2 { compatible = "unsupported,uart"; };', 'expected compatible'),
            ('&uart2 { interrupts = <39>; };', 'IRQ'),
            ('&uart2 { reg = <0x40004800 0x400>; };', 'register range'),
            ('&uart2 { current-speed = <0>; };', 'Baud rate'),
            ('&uart2 { current-speed = <2000000>; };', 'Baud rate'),
            ('&uart2 { current-speed = <1>; };', 'Baud rate'),
            ('&uart2 { current-speed = <115200 1>; };', 'requires 1 cells'),
            ('&uart2 { artos,alternate-function = <8>; };', 'mapping'),
            ('&uart2 { artos,tx-pin = <&gpioa 4 0>; };', 'mapping'),
            ('&uart2 { artos,tx-pin = <0xdead 2 0>; };', 'Unknown phandle'),
            ('&uart2 { hardware-flow-control; };', 'unsupported properties'),
            ('&heartbeat { gpios = <&gpioa 16 0>; };', 'pins 0..15'),
            ('&heartbeat { gpios = <&gpioa 5 1>; };', 'active-high'),
            ('&heartbeat { gpios = <&gpioa 2 0>; };', 'overlap'),
            ('&gpioa { status = "disabled"; };', 'disabled device'),
            ('&gpioa { #gpio-cells = <1>; };', 'two specifier cells'),
            ('&hsi { clock-frequency = <8000000>; };', '16 MHz'),
            ('&systick { reg = <0xe000e014 0x10>; };', 'SysTick range'),
            ('&tim2 { status = "disabled"; };', 'disabled device'),
            ('&tim2 { compatible = "unsupported,timer"; };', 'expected compatible'),
            ('&tim2 { reg = <0x40000400 0x400>; };', 'stopwatch timer register range'),
            ('&tim2 { counter-width = <16>; };', '32-bit timer'),
            ('&tim2 { interrupts = <29>; };', 'Stopwatch IRQ'),
            ('&tim2 { interrupt-parent = <&hsi>; };', 'Stopwatch interrupt controller'),
            ('&tim2 { clocks = <0xdead>; };', 'Unknown phandle'),
            ('/ { chosen { /delete-property/ artos,stopwatch; }; };', 'missing artos,stopwatch'),
            ('&nvic { #interrupt-cells = <2>; };', 'interrupt controller'),
            ('/ { chosen { artos,console = <0xdead>; }; };', 'Unknown phandle'),
            ('/ { chosen { /delete-property/ artos,heartbeat; }; };', 'missing artos,heartbeat'),
            ('/ { /delete-node/ chosen; };', 'missing node'),
            ('/ { soc { status = "disabled"; }; };', 'disabled device'),
        ]
        for overlay, message in cases:
            with self.subTest(overlay=overlay):
                with self.assertRaisesRegex(gen.BindingError, message):
                    self.generate(overlay=overlay)

    def test_reject_wrong_board(self):
        with self.assertRaisesRegex(gen.BindingError, 'does not match'):
            self.generate(expected_board='nucleo_f439zi')

    def test_reject_address_translation(self):
        with self.assertRaisesRegex(gen.BindingError, 'translation'):
            self.generate(soc_replace=('ranges;', 'ranges = <0 0x1000 0x100000>;'))

    def test_invalid_dtb(self):
        invalid = self.work / 'bad.dtb'
        invalid.write_bytes(b'not a flattened tree')
        with self.assertRaises(gen.BindingError):
            gen.Tree(invalid)


if __name__ == '__main__':
    unittest.main()
