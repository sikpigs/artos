#!/usr/bin/env python3
"""Regression checks for the grouped register generator and its source contract."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import svd_to_regmap as svd


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.root = ET.parse(svd.BSP / 'svd/STM32F411.svd').getroot()
        self.model = svd.normalize(self.root, [p.findtext('name') for p in self.root.findall('peripherals/peripheral') if p.findtext('name') != 'TIM8'])

    def test_inherited_layout_and_instance_reset_values(self):
        svd.validate(self.model)
        self.assertEqual(svd.signature(self.model['GPIOE']), svd.signature(self.model['GPIOA']))
        self.assertEqual(self.model['GPIOE']['address'], 0x40021000)
        self.assertEqual(self.model['GPIOA']['registers'][0]['reset'], 0xA8000000)
        self.assertEqual(self.model['GPIOB']['registers'][0]['reset'], 0x280)
        self.assertEqual(self.model['GPIOE']['registers'][0]['reset'], 0)

    def test_inherited_device_properties_and_field_access(self):
        reg = self.root.find("peripherals/peripheral[name='GPIOA']/registers/register[name='MODER']")
        reg.remove(reg.find('size'))
        reg.remove(reg.find('resetValue'))
        model = svd.normalize(self.root, ['GPIOA'])
        svd.validate(model)
        self.assertEqual(model['GPIOA']['registers'][0]['reset'], 0)
        cr = self.model['RCC']['registers'][0]
        self.assertEqual(cr['access'], 'read-write')
        self.assertEqual(next(f for f in cr['fields'] if f['name'] == 'HSIRDY')['access'], 'read-only')

    def test_inheritance_cycle_rejected(self):
        self.root.find("peripherals/peripheral[name='GPIOH']").set('derivedFrom', 'GPIOE')
        with self.assertRaisesRegex(ValueError, 'cycle'):
            svd.normalize(self.root, ['GPIOE'])

    def test_unsupported_construct_rejected(self):
        reg = self.root.find("peripherals/peripheral[name='RCC']/registers/register")
        ET.SubElement(reg, 'readAction').text = 'clear'
        with self.assertRaisesRegex(ValueError, 'readAction'):
            svd.normalize(self.root, ['RCC'])

    def test_invalid_layouts_rejected(self):
        for mutation, message in [
            (lambda rs: rs[1].update(offset=0), 'overlapping'),
            (lambda rs: rs[0]['fields'][0].update(width=33), 'range'),
            (lambda rs: rs[0]['fields'].append(copy.deepcopy(rs[0]['fields'][0])), 'duplicate'),
        ]:
            model = copy.deepcopy(self.model)
            mutation(model['GPIOA']['registers'])
            with self.assertRaisesRegex(ValueError, message):
                svd.validate(model)

    def test_corrections_and_shared_layout_guard(self):
        import json
        profile = json.loads((svd.BSP / 'svd/regmap.json').read_text())
        svd.corrections(self.model, profile)
        svd.validate(self.model)
        dck = self.model['RCC']['registers'][-1]
        self.assertEqual((dck['name'], dck['offset']), ('DCKCFGR', 0x8C))
        with self.assertRaisesRegex(ValueError, 'already exists'):
            svd.corrections(self.model, profile)
        self.model['GPIOB']['registers'][0]['fields'][0]['access'] = 'read-only'
        with self.assertRaisesRegex(ValueError, 'identical'):
            svd.emit_block(self.model, profile, 'gpio', list(svd.PORTS))

    def test_deterministic_output_and_stale_check(self):
        script = svd.ROOT / 'scripts/svd_to_regmap.py'
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'chip'
            command = [sys.executable, str(script), '--output', str(output)]
            subprocess.run(command, check=True)
            expected = {p.relative_to(output): p.read_bytes() for p in output.rglob('*.h')}
            subprocess.run(command, check=True)
            self.assertEqual({p.relative_to(output): p.read_bytes() for p in output.rglob('*.h')}, expected)
            subprocess.run(command + ['--check'], check=True)
            (output / 'generated/tim.h').write_text('// stale\n')
            result = subprocess.run(command + ['--check'], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('stale or missing', result.stderr)
            self.assertEqual((output / 'generated/tim.h').read_text(), '// stale\n')
        subprocess.run([sys.executable, str(script), '--check'], check=True)

    def test_all_groups_and_instances_are_emitted(self):
        import json
        profile = json.loads((svd.BSP / 'svd/regmap.json').read_text())
        svd.corrections(self.model, profile)
        svd.validate(self.model)
        output = svd.emit(self.model, profile, '', '')
        self.assertEqual(len(self.model), 46)
        self.assertEqual(len(output), 20)  # 19 group headers plus umbrella
        for name, peripheral in self.model.items():
            text = output['generated/' + peripheral['group'].lower() + '.h']
            # Verify declarations independently of formatter alignment/line wraps.
            text = ' '.join(text.split())
            self.assertIn(f'const {name} = reinterpret_cast', text)
            for reg in peripheral['registers']:
                # The layout assertions must account for every source register/view.
                self.assertIn(f"offsetof(registers_t, {reg['name']}) == 0x{reg['offset']:02X}", text)
        self.assertNotIn('TIM8', output['generated/tim.h'])
        self.assertIn('namespace stm32f411::tim::tim10', output['generated/tim.h'])
        self.assertIn('namespace stm32f411::usb_otg_fs::otg_fs_device', output['generated/usb_otg_fs.h'])

    def test_alternate_views_and_invalid_references(self):
        svd.validate(self.model)
        timer = self.model['TIM2']
        views = [r for r in timer['registers'] if r['offset'] == 0x18]
        self.assertEqual({r['name'] for r in views}, {'CCMR1_Input', 'CCMR1_Output'})
        views[0]['alternate'] = 'ABSENT'
        with self.assertRaisesRegex(ValueError, 'alternate'):
            svd.validate(self.model)

    def test_integer_and_cpp_identifier_spelling(self):
        self.assertEqual(svd.number('00000010'), 10)
        self.assertEqual(svd.number('0x10'), 16)
        self.assertEqual(svd.lower_name('OR'), 'or_')
        self.assertEqual(svd.cpp_name('OR'), 'OR')

    def test_stale_group_cleanup_and_unmanaged_file_preservation(self):
        script = svd.ROOT / 'scripts/svd_to_regmap.py'
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'generated').mkdir()
            stale = root / 'generated/obsolete.h'
            stale.write_text('// Generated by scripts/svd_to_regmap.py; do not edit.\n')
            manual = root / 'generated/manual.h'
            manual.write_text('// manual\n')
            command = [sys.executable, str(script), '--output', tmp]
            result = subprocess.run(command + ['--check'], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(stale.exists())
            subprocess.run(command, check=True)
            self.assertFalse(stale.exists())
            self.assertEqual(manual.read_text(), '// manual\n')

    def test_changed_svd_requires_profile_review(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'modified.svd'
            source.write_bytes((svd.BSP / 'svd/STM32F411.svd').read_bytes() + b'\n')
            with self.assertRaisesRegex(ValueError, 'hash changed'):
                svd.generate(source, svd.BSP / 'svd/regmap.json')

    def test_f439_uses_its_own_profile_namespace_and_groups(self):
        soc = svd.SOC_ROOT / 'stm32f439'
        output = svd.generate(soc / 'svd/STM32F439.svd', soc / 'svd/regmap.json')
        self.assertIn('generated/gpio.h', output)
        self.assertIn('namespace stm32f439::gpio', output['generated/gpio.h'])
        self.assertNotIn('stm32f411', ''.join(output.values()))
        self.assertIn('namespace stm32f439::usart::usart3', output['generated/usart.h'])
        self.assertIn('TIM8', output['generated/tim.h'])
        subprocess.run([sys.executable, str(svd.ROOT / 'scripts/svd_to_regmap.py'),
                        '--soc', 'stm32f439', '--check'], check=True)


if __name__ == '__main__':
    unittest.main()
