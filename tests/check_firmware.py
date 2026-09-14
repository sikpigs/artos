#!/usr/bin/env python3
"""Check linked board memory and vectors, including Thumb aliases to IRQ dispatch."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf', type=Path, required=True)
p.add_argument('--board', choices=['nucleo_f411re', 'nucleo_f439zi'], required=True)
p.add_argument('--objcopy', default='arm-none-eabi-objcopy')
p.add_argument('--nm', default='arm-none-eabi-nm')
a = p.parse_args()
stack, count, console_irq = ((0x20020000, 86, 38) if a.board == 'nucleo_f411re'
                             else (0x20030000, 91, 39))
symbols = {}
for line in subprocess.check_output([a.nm, '-n', str(a.elf)], text=True).splitlines():
    parts = line.split()
    if len(parts) == 3:
        symbols[parts[2]] = int(parts[0], 16)
with tempfile.TemporaryDirectory() as tmp:
    binary = Path(tmp) / 'vectors.bin'
    subprocess.run([a.objcopy, '--dump-section', f'.isr_vector={binary}', str(a.elf)], check=True)
    data = binary.read_bytes()
assert len(data) == (16 + count) * 4, 'incorrect device vector table size'
vectors = struct.unpack('<' + 'I' * (16 + count), data)
assert vectors[0] == stack == symbols['_estack'], 'incorrect main stack boundary'
assert vectors[1] == symbols['Reset_Handler'] | 1, 'reset vector must select Thumb code'
assert vectors[3] == symbols['HardFault_Handler'] | 1, 'fault handler override lost'
assert vectors[16 + console_irq] == symbols['Default_IRQHandler'] | 1, 'console does not reach common IRQ dispatch'
assert symbols['IRQ_Handler'] != symbols['Default_Handler']
assert symbols['__arena_end'] == stack - 4096
assert symbols['_ebss'] <= symbols['__arena_end']
# Check every external vector against the SoC's vector source, including reserved slots.
chip = 'stm32f411' if a.board == 'nucleo_f411re' else 'stm32f439'
source = Path(__file__).resolve().parents[1] / f'bsp/soc/st/stm32f4/{chip}/startup/vectors.s'
import re
names = re.findall(r'^\s*\.word\s+(\w+)', source.read_text(), re.M)
for i, name in enumerate(names[16:], 16):
    assert vectors[i] == (0 if name == '0' else symbols[name] | 1), f'bad vector {i}: {name}'
print(f'{a.board}: linked vectors, IRQ dispatch, stack and arena boundaries verified')
