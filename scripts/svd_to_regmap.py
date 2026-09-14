#!/usr/bin/env python3
"""Generate SoC register headers, one file per SVD groupName. No third-party Python dependencies.

Deliberately limited to flat, aligned 32-bit registers and peripheral inheritance.
Unsupported semantics in selected peripherals are errors, not silently dropped.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOC_ROOT = ROOT / 'bsp/soc/st/stm32f4'
BSP = SOC_ROOT / 'stm32f411'  # Default device; callers may select another SoC.
PORTS = ('GPIOA', 'GPIOB', 'GPIOC', 'GPIOD', 'GPIOE', 'GPIOH')
ACCESS = {'read-only': 'read_only_register_t', 'write-only': 'write_only_register_t',
          'read-write': 'register_t'}
PROPERTIES = ('size', 'access', 'resetValue', 'resetMask')
SUPPORTED_TAGS = set(PROPERTIES) | {
    'peripheral', 'name', 'description', 'groupName', 'baseAddress', 'addressBlock',
    'offset', 'usage', 'interrupt', 'value', 'registers', 'register', 'displayName',
    'addressOffset', 'fields', 'field', 'bitOffset', 'bitWidth', 'alternateRegister',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    value = value.strip()
    return int(value, 0) if value.lower().startswith(('0x', '0b')) else int(value, 10)


def identifier(name):
    require(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', name) is not None,
            f'unsupported identifier: {name!r}')
    return name


CPP_KEYWORDS = set("alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq".split())


def cpp_name(name):
    identifier(name)
    return name + '_' if name in CPP_KEYWORDS else name


def lower_name(name):
    return cpp_name(name.lower())



def properties(node, inherited):
    result = dict(inherited)
    result.update({key: node.findtext(key) for key in PROPERTIES if node.find(key) is not None})
    return result


def normalize(root, selected):
    require(root.findtext('addressUnitBits') == '8', 'only byte-addressed devices supported')
    peripherals = {}
    for p in root.findall('peripherals/peripheral'):
        name = p.findtext('name')
        require(name not in peripherals, f'duplicate peripheral: {name}')
        peripherals[name] = p
    resolved = {}

    def resolve(name, stack=()):
        require(name not in stack, f'peripheral inheritance cycle: {stack + (name,)}')
        require(name in peripherals, f'unknown peripheral: {name}')
        if name in resolved:
            return resolved[name]
        node = peripherals[name]
        require(set(node.attrib) <= {'derivedFrom'}, f'{name}: unsupported peripheral attributes')
        for e in node.iter():
            require(e is node or not e.attrib, f'{name}: unsupported attributes on {e.tag}')
            require(e.tag in SUPPORTED_TAGS, f'{name}: unsupported SVD construct {e.tag}')
        base = node.get('derivedFrom')
        if base:
            parent = resolve(base, stack + (name,))
            require(node.find('registers') is None, f'{name}: derived register overrides unsupported')
            require(not any(node.find(k) is not None for k in PROPERTIES),
                    f'{name}: derived property overrides unsupported')
            result = copy.deepcopy(parent)
            result.update(name=name, address=number(node.findtext('baseAddress')))
            result['group'] = node.findtext('groupName', parent['group'])
        else:
            inherited = properties(node, properties(root, {}))
            regs = []
            for reg in node.findall('registers/register'):
                rn = identifier(reg.findtext('name'))
                props = properties(reg, inherited)
                require(number(props.get('size', '0')) == 32, f'{name}.{rn}: only 32-bit registers supported')
                fields = []
                for field in reg.findall('fields/field'):
                    fn = identifier(field.findtext('name'))
                    fields.append(dict(name=fn, position=number(field.findtext('bitOffset')),
                                       width=number(field.findtext('bitWidth')),
                                       access=field.findtext('access', props.get('access'))))
                access = props.get('access')
                if access is None:
                    kinds = {f['access'] for f in fields}
                    require(bool(kinds) and kinds <= ACCESS.keys(), f'{name}.{rn}: unresolved access')
                    access = next(iter(kinds)) if len(kinds) == 1 else 'read-write'
                regs.append(dict(name=rn, offset=number(reg.findtext('addressOffset')), access=access,
                                 reset=number(props['resetValue']), reset_mask=number(props['resetMask']),
                                 alternate=reg.findtext('alternateRegister'),
                                 fields=fields))
            result = dict(name=name, group=node.findtext('groupName', name),
                          address=number(node.findtext('baseAddress')), registers=regs)
        resolved[name] = result
        return result

    return {name: copy.deepcopy(resolve(name)) for name in selected}


def corrections(model, profile):
    for patch in profile['corrections']:
        require(bool(patch['reason']) and bool(patch['source']), 'correction needs provenance')
        regs = model[patch['peripheral']]['registers']
        if patch['kind'] == 'register':
            value = copy.deepcopy(patch['value'])
            require(not any(r['name'] == value['name'] for r in regs), 'correction register already exists')
            value.setdefault('alternate', None)
            regs.append(value)
        elif patch['kind'] == 'field':
            reg = next(r for r in regs if r['name'] == patch['register'])
            value = copy.deepcopy(patch['value'])
            require(not any(f['name'] == value['name'] for f in reg['fields']), 'correction field already exists')
            reg['fields'].append(value)
        else:
            raise ValueError(f"unknown correction kind: {patch['kind']}")


def validate(model):
    for p in model.values():
        identifier(p['name'])
        require(0 <= p['address'] <= 0xFFFFFFFF and p['address'] % 4 == 0, 'invalid base address')
        end = 0
        names = set()
        by_name = {r['name']: r for r in p['registers']}
        offsets = {}
        require(bool(p['registers']), 'empty register block')
        for r in sorted(p['registers'], key=lambda r: r['offset']):
            identifier(r['name'])
            require(r['name'].lower() not in names, 'duplicate register name')
            names.add(r['name'].lower())
            require(r['offset'] >= 0 and r['offset'] % 4 == 0, 'unaligned register')
            if r.get('alternate'):
                target = by_name.get(r['alternate'])
                require(target is not None and target['offset'] == r['offset'] and not target.get('alternate'),
                        f"invalid alternate register: {p['name']}.{r['name']}")
            offsets.setdefault(r['offset'], []).append(r)
            end = max(end, r['offset'] + 4)
            require(p['address'] + end <= 0x100000000, 'register address overflow')
            require(r['access'] in ACCESS, 'unsupported register access')
            require(0 <= r['reset'] <= 0xFFFFFFFF and 0 <= r['reset_mask'] <= 0xFFFFFFFF,
                    'invalid reset metadata')
            used = 0
            field_names = set()
            for f in r['fields']:
                identifier(f['name'])
                require(f['name'].lower() not in field_names, 'duplicate field name')
                field_names.add(f['name'].lower())
                require(f['access'] in ACCESS, 'unsupported field access')
                require(0 <= f['position'] < 32 and 1 <= f['width'] <= 32 - f['position'], 'invalid field range')
                mask = ((1 << f['width']) - 1) << f['position']
                require(not used & mask, 'overlapping fields')
                used |= mask
            r['fields'].sort(key=lambda f: (f['position'], f['name']))
        for views in offsets.values():
            require(sum(not r.get('alternate') for r in views) == 1, 'overlapping registers without alternateRegister')
        p['registers'].sort(key=lambda r: (r['offset'], r['name']))


def signature(p):
    return [{k: v for k, v in r.items() if k not in {'reset', 'reset_mask'}} for r in p['registers']]


def banner(svd_hash, profile_hash, profile):
    return ['#pragma once', '', '// Generated by scripts/svd_to_regmap.py; do not edit.',
            f"// Source: {profile['device']}.svd, Copyright (c) 2024 STMicroelectronics, Apache-2.0.",
            '// See the selected SoC svd/LICENSE-Apache-2.0 and regmap.json for corrections.',
            f'// SVD SHA-256: {svd_hash}', f'// Profile SHA-256: {profile_hash}',
            '// Field access is metadata; mmio field operations do not enforce it.']


def emit_block(model, profile, ns, instances):
    namespace = profile['namespace']
    p = model[instances[0]]
    require(all(signature(model[n]) == signature(p) for n in instances),
            f'{ns}: instances do not have identical register/field layouts')
    lines = [f'namespace {namespace}::{ns} {{', '    namespace tags {']
    for r in p['registers']:
        lines.append(f"        struct {lower_name(r['name'])};")
    lines.append('    }')
    for r in p['registers']:
        rn = lower_name(r['name'])
        lines.append(f"    using {cpp_name(r['name'])} = artos::mmio::{ACCESS[r['access']]}<tags::{rn}>;")
    for r in p['registers']:
        rn = lower_name(r['name'])
        lines += ['', f'    namespace {rn} {{']
        for f in r['fields']:
            lines.append(f"        inline constexpr artos::mmio::field_t<tags::{rn}, {f['position']}, {f['width']}> {lower_name(f['name'])} {{}}; // {f['access']}")
        for alias, sources in profile.get('aliases', {}).get(f"{p['name']}.{r['name']}", {}).items():
            identifier(alias)
            fs = [next(f for f in r['fields'] if f['name'] == name) for name in sources]
            fs.sort(key=lambda f: f['position'])
            require(bool(fs) and len({f['access'] for f in fs}) == 1, 'incompatible alias fields')
            require(all(a['position'] + a['width'] == b['position'] for a, b in zip(fs, fs[1:])), 'noncontiguous alias')
            require(alias not in {lower_name(f['name']) for f in r['fields']}, 'alias collision')
            lines.append(f"        inline constexpr artos::mmio::field_t<tags::{rn}, {fs[0]['position']}, {sum(f['width'] for f in fs)}> {cpp_name(alias)} {{}};")
        lines.append('    }')
    lines += ['', '    struct registers_t', '    {']
    by_offset = {}
    for r in p['registers']:
        by_offset.setdefault(r['offset'], []).append(r)
    end = 0
    for offset, views in by_offset.items():
        if offset > end:
            lines.append(f"        std::uint32_t reserved_{end:02x}[{(offset - end) // 4}];")
        if len(views) > 1:
            lines += ['        union', '        {']
        for r in views:
            indent = '            ' if len(views) > 1 else '        '
            lines.append(f"{indent}{namespace}::{ns}::{cpp_name(r['name'])} {cpp_name(r['name'])};")
        if len(views) > 1:
            lines.append('        };')
        end = offset + 4
    lines += ['    };', '    static_assert(alignof(registers_t) == alignof(std::uint32_t));']
    for r in p['registers']:
        lines.append(f"    static_assert(offsetof(registers_t, {cpp_name(r['name'])}) == 0x{r['offset']:02X});")
    lines.append(f'    static_assert(sizeof(registers_t) == 0x{end:X});')
    for name in instances:
        address_name = 'address' if ns == 'rcc' else lower_name(name) + '_address'
        lines += ['', f"    inline constexpr std::uintptr_t {address_name} = 0x{model[name]['address']:08X}UL;",
                  f'    inline registers_t* const {cpp_name(name)} = reinterpret_cast<registers_t*>({address_name});',
                  f'    namespace metadata::{lower_name(name)} {{']
        for r in model[name]['registers']:
            lines += [f"        namespace {lower_name(r['name'])} {{",
                      f"            inline constexpr std::uint32_t reset_value = 0x{r['reset']:08X}UL;",
                      f"            inline constexpr std::uint32_t reset_mask = 0x{r['reset_mask']:08X}UL;",
                      '        }']
        lines.append('    }')
    return lines + ['}', '']


def emit(model, profile, svd_hash, profile_hash):
    namespace = profile['namespace']
    groups = {}
    for name, peripheral in sorted(model.items()):
        group = lower_name(peripheral['group'])
        groups.setdefault(group, []).append(name)
    output = {}
    for group, instances in sorted(groups.items()):
        lines = banner(svd_hash, profile_hash, profile) + [
            '#include <cstddef>', '#include <cstdint>', '#include <mmio.h>', '']
        if all(signature(model[n]) == signature(model[instances[0]]) for n in instances):
            lines += emit_block(model, profile, group, instances)
        else:
            # A group name describes a family, not necessarily an identical layout.
            # Each differing instance gets its own tags, fields and register type.
            for name in instances:
                ns = group + '::' + lower_name(name)
                lines += emit_block(model, profile, ns, [name])
            lines.append(f'namespace {namespace}::{group} {{')
            for name in instances:
                lines.append(f'    using {lower_name(name)}::{cpp_name(name)};')
            lines += ['}', '']
        output[f'generated/{group}.h'] = '\n'.join(lines)
    umbrella = banner(svd_hash, profile_hash, profile) + ['']
    umbrella += [f'#include <{namespace}/generated/{group}.h>' for group in sorted(groups)]
    output['regmap.h'] = '\n'.join(umbrella) + '\n'
    # Use the same formatting contract as editor saves and the pre-commit hook.
    # Format before both writing and freshness checks, so they compare identical text.
    formatter = os.environ.get('CLANG_FORMAT', 'clang-format')
    for name, text in output.items():
        result = subprocess.run(
            [formatter, f'--style=file:{ROOT / ".clang-format"}',
             f'--assume-filename={name}'],
            input=text, capture_output=True, text=True)
        require(result.returncode == 0, f'clang-format failed for {name}: {result.stderr}')
        output[name] = result.stdout
    return output


def generate(svd, profile_path):
    source = svd.read_bytes()
    profile_bytes = profile_path.read_bytes()
    profile = json.loads(profile_bytes)
    digest = hashlib.sha256(source).hexdigest()
    require(digest == profile['svd_sha256'], 'SVD hash changed; review corrections before updating the profile')
    root = ET.fromstring(source)
    require(root.findtext('name') == profile['device'], 'SVD device does not match profile')
    identifier(profile['namespace'])
    selected = {p.findtext('name') for p in root.findall('peripherals/peripheral')}
    for exclusion in profile.get('exclude_peripherals', []):
        require(bool(exclusion['reason']) and bool(exclusion['source']), 'exclusion needs provenance')
        require(exclusion['name'] in selected, 'excluded peripheral absent; review profile')
        selected.remove(exclusion['name'])
    model = normalize(root, sorted(selected))
    corrections(model, profile)
    validate(model)
    return emit(model, profile, digest, hashlib.sha256(profile_bytes).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--soc', default='stm32f411', help='SoC directory under bsp/soc/st/stm32f4')
    parser.add_argument('--soc-dir', type=Path, help='explicit SoC directory, including its svd/regmap.json')
    parser.add_argument('--svd', type=Path)
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--output', type=Path, help='output directory for regmap.h and generated/<group>.h')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    identifier(args.soc)
    soc = args.soc_dir or SOC_ROOT / args.soc
    args.profile = args.profile or soc / 'svd/regmap.json'
    try:
        profile = json.loads(args.profile.read_text())
        args.svd = args.svd or soc / 'svd' / (profile['device'] + '.svd')
        args.output = args.output or soc / 'include' / profile['namespace']
        output = generate(args.svd, args.profile)
        # Only remove stale files bearing our generator marker, never user headers.
        marker = '// Generated by scripts/svd_to_regmap.py; do not edit.'
        stale = [p for p in (args.output / 'generated').glob('*.h')
                 if p.relative_to(args.output).as_posix() not in output and marker in p.read_text()]
        if args.check:
            require(not stale, f'stale generated group files: {stale}; regenerate headers')
            for name, content in output.items():
                path = args.output / name
                require(path.is_file() and path.read_bytes() == content.encode(),
                        f'stale or missing {path}; run scripts/svd_to_regmap.py')
        else:
            for name, content in output.items():
                path = args.output / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            for path in stale:
                path.unlink()
    except (ValueError, KeyError, StopIteration, TypeError, OSError, ET.ParseError) as error:
        parser.exit(1, f'SVD generation failed: {error}\n')


if __name__ == '__main__':
    main()
