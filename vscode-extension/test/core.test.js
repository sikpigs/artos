'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const core = require('../core');

test('helper commands preserve paths with spaces and route to the selected serial', () => {
    const args = core.helperArgs('/tools with spaces/probe.py', '/repo with spaces', 'flash',
        { preset: 'release', board: 'auto', serial: '12345678ABCD' }, '/tools/Open OCD');
    assert.equal(args[0], '/tools with spaces/probe.py');
    assert.equal(args[args.indexOf('--root') + 1], '/repo with spaces');
    assert.equal(args[args.indexOf('--serial') + 1], '12345678ABCD');
    assert.equal(args[args.indexOf('--openocd') + 1], '/tools/Open OCD');
});

test('debug uses selected probe, mode, custom ELF and matching board/SVD', () => {
    const root = '/firmware';
    const config = core.debugConfiguration(root,
        { board: 'auto', preset: 'release', serial: 'ABC123' }, { board: 'nucleo_f439zi' },
        '/firmware/build/release/custom.elf', '/opt/openocd', 'gdb-multiarch');
    assert.deepEqual(config.openOCDLaunchCommands, ['adapter serial ABC123']);
    assert.equal(config.executable, '/firmware/build/release/custom.elf');
    assert.ok(config.configFiles[0].endsWith('nucleo_f439zi/openocd.cfg'));
    assert.ok(config.svdFile.endsWith('STM32F439.svd'));
    assert.equal(config.serverpath, '/opt/openocd');
});

test('opening extension directory finds the parent project', t => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'artos-extension-'));
    t.after(() => fs.rmSync(root, { recursive: true, force: true }));
    fs.writeFileSync(path.join(root, 'artos.py'), '');
    const extension = path.join(root, 'vscode-extension');
    fs.mkdirSync(extension);
    assert.equal(core.projectRoot([{ uri: { fsPath: extension } }], () => '', extension), root);
    assert.equal(core.projectRoot([], () => '', extension), root);
});
