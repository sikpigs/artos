'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const core = require('../core');
const extensionPath = path.resolve(__dirname, '..');
const root = path.dirname(extensionPath);

function harness(options = {}) {
    const commands = new Map();
    const state = new Map();
    const errors = [];
    const tasks = [];
    const processListeners = new Set();
    const endListeners = new Set();
    const queries = [];
    const items = [];
    let debugConfig;
    const disposable = () => ({ dispose() {} });
    const listen = collection => callback => {
        collection.add(callback);
        return { dispose() { collection.delete(callback); } };
    };
    const folder = { uri: { fsPath: options.folder || root } };
    const vscode = {
        workspace: {
            workspaceFolders: [folder], isTrusted: true,
            getConfiguration: () => ({ get: () => '' }),
            onDidChangeWorkspaceFolders: disposable, onDidChangeConfiguration: disposable
        },
        window: {
            createOutputChannel: () => ({ append() {}, appendLine() {}, dispose() {} }),
            createStatusBarItem: (id, alignment, priority) => {
                const item = { id, alignment, priority, visible: false,
                    show() { this.visible = true; }, hide() { this.visible = false; }, dispose() {} };
                items.push(item);
                return item;
            },
            onDidChangeActiveTextEditor: disposable,
            showErrorMessage: message => errors.push(message),
            showInformationMessage: () => options.notification || Promise.resolve(),
            showQuickPick: async items => items[items.length - 1]
        },
        commands: { registerCommand(name, handler) { commands.set(name, handler); return disposable(); } },
        extensions: { getExtension: () => ({}) },
        debug: { activeDebugSession: undefined, startDebugging: async (unused, config) => { debugConfig = config; return true; } },
        tasks: {
            onDidEndTaskProcess: listen(processListeners), onDidEndTask: listen(endListeners),
            executeTask: async task => { tasks.push(task); return { task }; }
        },
        Task: class { constructor(definition, scope, name, source, execution) { Object.assign(this, { definition, scope, name, source, execution }); } },
        ProcessExecution: class { constructor(command, args, options) { Object.assign(this, { command, args, options }); } },
        TaskScope: { Workspace: 1 }, TaskRevealKind: { Always: 1 }, TaskPanelKind: { Dedicated: 1 }, StatusBarAlignment: { Left: 1 }
    };
    const fakeExec = (python, args, options, callback) => {
        const action = args[1]; queries.push({ action, args, python, options });
        let result;
        if (action === 'list') result = [{ serial: 'PROBE111', product: 'ST-LINK' }, { serial: 'PROBE222', product: 'ST-LINK' }];
        if (action === 'detect') result = { family: 'STM32F42x/F43x', chip_id: 0x419, flash_kib: 2048, board: 'nucleo_f439zi' };
        if (action === 'debug-info') result = { executable: root + '/build/debug/custom.elf' };
        queueMicrotask(() => callback(null, JSON.stringify(result), ''));
    };
    const sandbox = {
        require: name => name === 'vscode' ? vscode : name === 'node:child_process' ? { execFile: fakeExec } : name === './core' ? core : require(name),
        module: { exports: {} }, process
    };
    vm.runInNewContext(fs.readFileSync(path.join(extensionPath, 'extension.js'), 'utf8'), sandbox);
    sandbox.module.exports.activate({ extensionPath, subscriptions: [], workspaceState: {
        get: (key, fallback) => state.get(key) || fallback, update: async (key, value) => state.set(key, { ...value })
    } });
    function finish(code) {
        const execution = { task: tasks[tasks.length - 1] };
        [...processListeners].forEach(listener => listener({ execution, exitCode: code }));
        [...endListeners].forEach(listener => listener({ execution }));
    }
    return { commands, state, errors, tasks, queries, items, vscode, finish, get debugConfig() { return debugConfig; } };
}

const tick = () => new Promise(resolve => setImmediate(resolve));

test('activation displays all controls without a connected probe or helper invocation', async () => {
    const h = harness();
    assert.equal(h.items.length, 7);
    assert.ok(h.items.every(item => item.visible));
    assert.equal(h.items[4].command, 'artos.flash');
    assert.ok(h.items[1].text.includes('Select ST-LINK'));
    assert.equal(h.queries.length, 0);
    await h.commands.get('artos.showControls')();
    assert.ok(h.items.every(item => item.visible));
});

test('opening the extension source folder displays the parent project controls', () => {
    const h = harness({ folder: extensionPath });
    assert.ok(h.items.every(item => item.visible));
    assert.equal(h.items[4].command, 'artos.flash');
});

test('missing project displays a setup control instead of silently hiding everything', async () => {
    const h = harness({ folder: '/tmp/not-an-artos-project' });
    assert.equal(h.items.filter(item => item.visible).length, 1);
    assert.ok(h.items[0].text.includes('ARTOS: Open Project'));
    assert.equal(h.items[0].command, 'artos.showControls');
    await h.commands.get('artos.showControls')();
    assert.equal(h.queries.length, 0);
});

test('manifest activates after startup even when workspace file detection misses', () => {
    const manifest = JSON.parse(fs.readFileSync(path.join(extensionPath, 'package.json'), 'utf8'));
    assert.ok(manifest.activationEvents.includes('onStartupFinished'));
    assert.ok(manifest.contributes.commands.some(command => command.command === 'artos.showControls'));
});

test('development launch uses a separate profile and forces a new window', () => {
    const launch = JSON.parse(fs.readFileSync(path.join(extensionPath, '.vscode/launch.json'), 'utf8'));
    const args = launch.configurations[0].args;
    assert.ok(args.includes('--new-window'));
    assert.ok(args.includes('--user-data-dir=${workspaceFolder}/.dev-host'));
    assert.ok(args.includes('--extensionDevelopmentPath=${workspaceFolder}'));
    assert.ok(fs.readFileSync(path.join(extensionPath, '.vscodeignore'), 'utf8').includes('.dev-host/**'));
});

test('selected probe is remembered and concurrent flash commands are serialized', async () => {
    const h = harness();
    await h.commands.get('artos.selectProbe')();
    assert.equal(h.state.get(`artos:${root}`).serial, 'PROBE222');
    assert.equal(h.state.get(`artos:${root}`).inferredBoard, 'nucleo_f439zi');
    const flash = h.commands.get('artos.flash')();
    await tick();
    assert.equal(h.tasks.length, 1);
    const args = h.tasks[0].execution.args;
    assert.equal(args[1], 'flash');
    assert.equal(args[args.indexOf('--serial') + 1], 'PROBE222');
    await h.commands.get('artos.flash')();
    assert.equal(h.tasks.length, 1);
    h.finish(0);
    await flash;
    assert.deepEqual(h.errors, []);
});

test('completed probe detection unlocks controls while its notification remains open', async () => {
    let dismiss;
    const notification = new Promise(resolve => { dismiss = resolve; });
    const h = harness({ notification });
    let completed = false;
    const selection = h.commands.get('artos.selectProbe')().then(() => { completed = true; });
    await tick();
    const completedBeforeDismissal = completed;
    const flashAvailable = h.items[4].command === 'artos.flash';
    // Always release the notification so a failing regression test leaves no pending operation.
    dismiss();
    await selection;
    assert.equal(completedBeforeDismissal, true);
    assert.equal(flashAvailable, true);
});

test('detect-target notification does not block another toolbar action', async () => {
    let dismiss;
    const notification = new Promise(resolve => { dismiss = resolve; });
    const h = harness({ notification });
    const detection = h.commands.get('artos.detectTarget')();
    await tick();
    const presetAvailable = h.items[2].command === 'artos.selectPreset';
    await h.commands.get('artos.selectPreset')();
    const changedBeforeDismissal = h.state.get(`artos:${root}`).preset === 'min-size';
    dismiss();
    await detection;
    assert.equal(presetAvailable, true);
    assert.equal(changedBeforeDismissal, true);
});

test('debug waits for the build and selects the same probe and inferred board', async () => {
    const h = harness();
    await h.commands.get('artos.selectProbe')();
    const debug = h.commands.get('artos.debug')();
    await tick();
    assert.equal(h.tasks[0].execution.args[1], 'build');
    assert.equal(h.debugConfig, undefined);
    h.finish(0);
    await debug;
    assert.deepEqual(h.debugConfig.openOCDLaunchCommands, ['adapter serial PROBE222']);
    assert.equal(h.debugConfig.device, 'STM32F439ZI');
    assert.ok(h.debugConfig.executable.endsWith('/custom.elf'));
});

test('cancelled builds do not start debug and active debug blocks flashing', async () => {
    const h = harness();
    await h.commands.get('artos.selectProbe')();
    const debug = h.commands.get('artos.debug')();
    await tick();
    h.finish(undefined);
    await debug;
    assert.equal(h.debugConfig, undefined);
    assert.ok(h.errors[0].includes('cancelled'));
    h.vscode.debug.activeDebugSession = {};
    await h.commands.get('artos.flash')();
    assert.equal(h.tasks.length, 1);
    assert.ok(h.errors[1].includes('Stop the active debug session'));
});

test('untrusted workspace does not execute helper commands or tasks', async () => {
    const h = harness();
    h.vscode.workspace.isTrusted = false;
    await h.commands.get('artos.flash')();
    assert.equal(h.tasks.length, 0);
    assert.equal(h.queries.length, 0);
});
