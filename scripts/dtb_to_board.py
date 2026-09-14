#!/usr/bin/env python3
"""Compile the supported Artos board bindings from DTB to constexpr C++.

libfdt's fdtget reads the flattened tree; dtc owns DTS syntax/includes/phandles.
This is a deliberately bounded binding compiler, not a general DT implementation.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
from string import Template


class BindingError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise BindingError(message)


class Tree:
    def __init__(self, path, fdtget='fdtget'):
        self.nodes = {}
        self.handles = {}

        def visit(path):
            props = {}
            for name in get('-p', path).splitlines():
                # Property data is read as bytes, preserving string/cell boundaries.
                result = subprocess.run([fdtget, '-t', 'bx', str(self.path), path, name],
                                        capture_output=True, text=True)
                require(result.returncode == 0, result.stderr.strip())
                props[name] = bytes(int(v, 16) for v in result.stdout.split())
            self.nodes[path] = props
            for child in get('-l', path).splitlines():
                visit(path.rstrip('/') + '/' + child)

        # -p/-l take one option, followed by the DTB and node path.
        def get(option, node):
            result = subprocess.run([fdtget, option, str(self.path), node],
                                    capture_output=True, text=True)
            require(result.returncode == 0, result.stderr.strip())
            return result.stdout.strip()

        self.path = path
        visit('/')
        for node, props in self.nodes.items():
            if 'phandle' in props:
                handle = self.cells(node, 'phandle', 1)[0]
                require(handle not in (0, 0xffffffff) and handle not in self.handles,
                        f'{node}: invalid or duplicate phandle')
                self.handles[handle] = node

    def data(self, node, prop):
        require(node in self.nodes and prop in self.nodes[node], f'{node}: missing {prop}')
        return self.nodes[node][prop]

    def cells(self, node, prop, count):
        raw = self.data(node, prop)
        require(len(raw) == count * 4, f'{node}: {prop} requires {count} cells')
        return list(struct.unpack('>' + 'I' * count, raw))

    def strings(self, node, prop):
        raw = self.data(node, prop)
        require(raw.endswith(b'\0'), f'{node}: {prop} requires terminated strings')
        try:
            return raw[:-1].decode('ascii').split('\0')
        except UnicodeDecodeError as error:
            raise BindingError(f'{node}: invalid string {prop}') from error

    def ref(self, handle):
        require(handle in self.handles, f'Unknown phandle {handle}')
        node = self.handles[handle]
        self.enabled(node)
        return node

    def enabled(self, node):
        while node:
            if 'status' in self.nodes[node]:
                require(self.strings(node, 'status') in (['okay'], ['ok']), f'{node}: disabled device')
            node = (node.rpartition('/')[0] or '/') if node != '/' else ''

    def compatible(self, node, value):
        require(self.strings(node, 'compatible') == [value], f'{node}: expected compatible {value}')

    def properties(self, node, allowed):
        require(node in self.nodes, f'{node}: missing node')
        extra = set(self.nodes[node]) - set(allowed.split()) - {'phandle', 'status'}
        require(not extra, f'{node}: unsupported properties {sorted(extra)}')


def generate(dtb, profile_path, board, output, fdtget='fdtget'):
    profile = json.loads(Path(profile_path).read_text())
    soc = profile['soc']
    uart = profile['uart']
    # Binding profiles are code-owned; DTS never supplies C++ fragments.
    require(re.fullmatch(r'stm32f\d+', soc), 'Invalid SoC binding')
    tree = Tree(dtb, fdtget)
    require(tree.strings('/', 'compatible') == ['st,' + board.replace('_', '-'), 'st,' + soc],
            'DTS board/SoC does not match selected build')
    for node in ('/', '/soc'):
        require(tree.cells(node, '#address-cells', 1) == [1] and
                tree.cells(node, '#size-cells', 1) == [1], f'{node}: only 32-bit address/size cells supported')
    tree.compatible('/soc', 'simple-bus')
    tree.properties('/soc', 'compatible #address-cells #size-cells ranges')
    require(tree.data('/soc', 'ranges') == b'', '/soc: address translation is unsupported')
    tree.properties('/chosen', 'artos,console artos,kernel-tick artos,heartbeat')
    selected = {name: tree.ref(tree.cells('/chosen', 'artos,' + name, 1)[0])
                for name in ('console', 'kernel-tick', 'heartbeat')}
    console, tick, led = (selected[key] for key in ('console', 'kernel-tick', 'heartbeat'))
    require(console.rpartition('/')[0] == '/soc' and tick.rpartition('/')[0] == '/soc',
            'Console and SysTick must use the identity-mapped /soc bus')
    tree.compatible(console, 'artos,stm32f4-usart')
    tree.properties(console, 'compatible reg interrupt-parent interrupts clocks current-speed artos,tx-pin artos,rx-pin artos,alternate-function')
    require(tree.cells(console, 'reg', 2) == [uart['address'], 0x400], 'Unsupported console register range')
    require(tree.cells(console, 'interrupts', 1) == [uart['irq']], 'Console IRQ does not match SoC')
    nvic = tree.ref(tree.cells(console, 'interrupt-parent', 1)[0])
    tree.compatible(nvic, 'arm,v7m-nvic')
    tree.properties(nvic, 'compatible reg interrupt-controller #address-cells #interrupt-cells')
    require(nvic.rpartition('/')[0] == '/soc' and tree.cells(nvic, 'reg', 2) == [0xe000e100, 0xc00]
            and tree.cells(nvic, '#interrupt-cells', 1) == [1]
            and tree.data(nvic, 'interrupt-controller') == b'', 'Unsupported interrupt controller')
    tree.compatible(tick, 'artos,cortex-m-systick')
    tree.properties(tick, 'compatible reg clocks')
    require(tree.cells(tick, 'reg', 2) == [0xe000e010, 0x10], 'Unsupported SysTick range')
    for device in (console, tick):
        clock = tree.ref(tree.cells(device, 'clocks', 1)[0])
        tree.compatible(clock, 'fixed-clock')
        tree.properties(clock, 'compatible #clock-cells clock-frequency')
        require(tree.cells(clock, '#clock-cells', 1) == [0] and
                tree.cells(clock, 'clock-frequency', 1) == [16000000],
                'Startup currently supports only the 16 MHz reset HSI clock')
    tree.compatible(led.rpartition('/')[0], 'gpio-leds')
    tree.enabled(led.rpartition('/')[0])
    tree.properties(led, 'gpios')

    def pin(node, prop):
        handle, number, flags = tree.cells(node, prop, 3)
        gpio = tree.ref(handle)
        require(gpio.rpartition('/')[0] == '/soc', 'GPIO must use the identity-mapped /soc bus')
        tree.compatible(gpio, 'artos,stm32f4-gpio')
        tree.properties(gpio, 'compatible reg gpio-controller #gpio-cells')
        require(tree.cells(gpio, '#gpio-cells', 1) == [2] and tree.data(gpio, 'gpio-controller') == b'',
                'Expected GPIO controller with two specifier cells')
        address, size = tree.cells(gpio, 'reg', 2)
        ports = {0x40020000 + (ord(p) - ord('a')) * 0x400: p for p in profile['gpio_ports']}
        require(address in ports and size == 0x400, 'Unsupported GPIO register range')
        require(number < 16 and flags == 0, 'Only pins 0..15 and active-high flags (0) are supported')
        return ports[address], number

    tx, rx, heartbeat = pin(console, 'artos,tx-pin'), pin(console, 'artos,rx-pin'), pin(led, 'gpios')
    af = tree.cells(console, 'artos,alternate-function', 1)[0]
    require(list(tx) == uart['tx'] and list(rx) == uart['rx'] and af == uart['af'],
            'Console pin/alternate-function mapping is not supported by the SoC binding')
    require(heartbeat not in (tx, rx), 'Heartbeat and console pins overlap')
    baud = tree.cells(console, 'current-speed', 1)[0]
    require(baud > 0 and 16 <= (16000000 + baud // 2) // baud <= 65535,
            'Baud rate cannot be represented with oversampling by 16')

    def cpp_pin(value):
        return f'{soc}::gpio::P{value[0].upper()}<{value[1]}>'

    header = f'''// Generated from the selected DTB; do not edit.
#pragma once
#include <cstdint>
#include <irq.h>
#include <{soc}/gpio.h>
#include <{soc}/generated/usart.h>
namespace board {{
    struct root {{
        struct clocks {{
            struct hsi {{ static constexpr std::uint32_t frequency_hz = 16000000; }};
            using core = hsi;
        }};
        struct soc {{
            struct systick {{
                using clock = clocks::core;
                static constexpr std::uintptr_t address = 0xe000e010UL;
            }};
            struct apb1 {{
                using clock = clocks::core;
                struct serial {{
                    using registers = {uart['registers']};
                    static constexpr bool enabled = true;
                    static constexpr std::uintptr_t address = 0x{uart['address']:x}UL;
                    static_assert(address == {uart['address_symbol']});
                    static constexpr auto interrupt = artos::irq::irq_t::{uart['name']};
                    static_assert(static_cast<unsigned>(interrupt) == {uart['irq']});
                    using clock = apb1::clock;
                    static constexpr auto tx = {cpp_pin(tx)};
                    static constexpr auto rx = {cpp_pin(rx)};
                    static constexpr auto alternate_function = stm32::gpio::alternate_function_t::AF{af};
                    static constexpr std::uint32_t baud_rate = {baud};
                }};
            }};
        }};
        struct leds {{
            struct heartbeat {{ static constexpr auto pin = {cpp_pin(heartbeat)}; }};
        }};
    }};
    namespace labels {{
        using {uart['label']} = root::soc::apb1::serial;
        using console_uart = {uart['label']};
        using systick = root::soc::systick;
        using heartbeat = root::leds::heartbeat;
    }}
    namespace chosen {{
        using console = labels::console_uart;
        using kernel_tick = labels::systick;
        using heartbeat = labels::heartbeat;
    }}
    void init() noexcept;
    void init_console() noexcept;
}}
'''
    def gate(port):
        return f'{soc}::rcc::ahb1enr::gpio{port}en'
    devices = Template((Path(__file__).parent / 'templates/board_devices.h.in').read_text()).substitute(
        soc=soc, uart=uart['name'], heartbeat_gate=gate(heartbeat[0]),
        console_gpio_gates=' | '.join(gate(port) for port in sorted({tx[0], rx[0]})))
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    for name, contents in [('board.h', header), ('board_devices.h', devices)]:
        (output / name).write_text(contents)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dtb', type=Path, required=True)
    parser.add_argument('--bindings', type=Path, required=True)
    parser.add_argument('--board', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fdtget', default='fdtget')
    args = parser.parse_args()
    try:
        generate(args.dtb, args.bindings, args.board, args.output, args.fdtget)
    except (BindingError, OSError, ValueError) as error:
        parser.exit(1, f'dtb_to_board: {error}\n')


if __name__ == '__main__':
    main()
