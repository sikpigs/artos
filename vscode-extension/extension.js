'use strict';
const vscode = require('vscode');
const path = require('node:path');
const { execFile } = require('node:child_process');
const { randomUUID } = require('node:crypto');
const core = require('./core');

function activate(context) {
    const output = vscode.window.createOutputChannel('ARTOS Board Tools');
    context.subscriptions.push(output);
    const items = [];
    let busy = false;
    let active;
    let locateError = '';
    const helper = path.join(context.extensionPath, 'probe.py');

    function locate() {
        const folders = vscode.workspace.workspaceFolders || [];
        const editorFolder = vscode.window.activeTextEditor && vscode.workspace.getWorkspaceFolder(vscode.window.activeTextEditor.document.uri);
        const ordered = editorFolder ? [editorFolder, ...folders.filter(f => f !== editorFolder)] : folders;
        const root = core.projectRoot(ordered, folder => vscode.workspace.getConfiguration('artos', folder.uri).get('projectRoot'), context.extensionPath);
        if (!root) throw new Error('Open the ARTOS project, or set artos.projectRoot to the directory containing artos.py.');
        const folder = ordered.find(f => root === f.uri.fsPath || root === path.dirname(f.uri.fsPath));
        const config = vscode.workspace.getConfiguration('artos', folder?.uri);
        const stored = context.workspaceState.get(`artos:${root}`, {});
        const state = { serial: stored.serial || '', board: core.boards[stored.board] ? stored.board : 'auto',
            preset: core.presets[stored.preset] ? stored.preset : 'debug',
            inferredBoard: core.boards[stored.inferredBoard] ? stored.inferredBoard : undefined };
        return { root, folder, config, state, python: core.pythonPath(root, config.get('pythonPath')),
            openocd: config.get('openocdPath') || 'openocd' };
    }

    async function save(session) {
        await context.workspaceState.update(`artos:${session.root}`, session.state);
        active = session;
        render();
    }

    function render() {
        if (!active) {
            items.forEach(item => item.hide());
            if (items.length) {
                items[0].text = '$(circuit-board) ARTOS: Open Project';
                items[0].command = 'artos.showControls';
                items[0].tooltip = locateError || 'Open the folder containing artos.py.';
                items[0].show();
            }
            return;
        }
        const state = active.state;
        const labels = [
            `$(circuit-board) ${core.boards[state.board]?.label || (core.boards[state.inferredBoard] ? core.boards[state.inferredBoard].label + ' (Auto)' : 'Board: Auto')}`,
            `$(plug) ${state.serial ? 'ST-LINK …' + state.serial.slice(-8) : 'Select ST-LINK'}`,
            `$(gear) ${core.presets[state.preset]}`,
            '$(tools) Build', '$(arrow-down) Flash', '$(terminal) Flash + Monitor', '$(debug-alt) Debug'
        ];
        items.forEach((item, index) => {
            item.text = busy && index === 4 ? '$(sync~spin) ARTOS' : labels[index];
            item.command = busy ? undefined : item.normalCommand;
            item.tooltip = busy ? 'An ARTOS operation is running. Cancel it in the task terminal.' : item.normalTooltip;
            if (index === 1 && !busy && state.serial) item.tooltip = `Selected ST-LINK: ${state.serial}\nClick to refresh the connected probe list.`;
            item.show();
        });
    }

    function query(session, action) {
        const args = core.helperArgs(helper, session.root, action, session.state, session.openocd);
        return new Promise((resolve, reject) => {
            execFile(session.python, args, { cwd: session.root, timeout: 25000, maxBuffer: 1024 * 1024 }, (error, stdout, stderr) => {
                if (stderr) output.append(stderr);
                if (error) { reject(new Error(stderr.trim() || error.message)); return; }
                try { resolve(JSON.parse(stdout)); } catch { reject(new Error(`Invalid response from ARTOS helper: ${stdout}`)); }
            });
        });
    }

    async function detect(session) {
        const target = await query(session, 'detect');
        session.state.inferredBoard = target.board || undefined;
        await save(session);
        output.appendLine(`ST-LINK ${session.state.serial}: ${target.family}, ID 0x${target.chip_id.toString(16)}; inferred board: ${target.board || 'unknown'}`);
        return target;
    }

    async function chooseProbe(session) {
        const probes = await query(session, 'list');
        if (!probes.length) throw new Error('No supported ST-LINK probes found. Check the USB cable and permissions.');
        let picked;
        if (probes.length === 1) picked = probes[0];
        else picked = await vscode.window.showQuickPick(probes.map(probe => ({
            label: probe.product, description: probe.serial,
            detail: probe.serial === session.state.serial ? 'Currently selected' : undefined, ...probe
        })), { title: 'Select ST-LINK probe', matchOnDescription: true });
        if (!picked) return false;
        // A board override applies to the old target; a newly chosen probe starts in Auto mode.
        if (session.state.serial !== picked.serial) {
            session.state.board = 'auto';
            session.state.inferredBoard = undefined;
        }
        session.state.serial = picked.serial;
        await save(session);
        return true;
    }

    function runTask(session, action) {
        const id = randomUUID();
        const task = new vscode.Task({ type: 'artos', id }, session.folder || vscode.TaskScope.Workspace,
            `ARTOS ${action} (${session.state.preset})`, 'ARTOS',
            new vscode.ProcessExecution(session.python,
                core.helperArgs(helper, session.root, action, session.state, session.openocd), { cwd: session.root }), ['$gcc']);
        task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, focus: action === 'flash-monitor',
            panel: vscode.TaskPanelKind.Dedicated, clear: true };
        return new Promise((resolve, reject) => {
            let exitCode;
            const processListener = vscode.tasks.onDidEndTaskProcess(event => {
                if (event.execution.task.definition.id === id) exitCode = event.exitCode;
            });
            const taskListener = vscode.tasks.onDidEndTask(event => {
                if (event.execution.task.definition.id !== id) return;
                processListener.dispose(); taskListener.dispose();
                if (exitCode === 0) resolve();
                else reject(new Error(`ARTOS ${action} ${exitCode === undefined ? 'was cancelled' : 'failed'}. See the task terminal.`));
            });
            vscode.tasks.executeTask(task).catch(error => {
                processListener.dispose(); taskListener.dispose(); reject(error);
            });
        });
    }

    async function operate(session, action) {
        if (vscode.debug.activeDebugSession) throw new Error('Stop the active debug session before accessing ST-LINK.');
        if (action !== 'build' || session.state.board === 'auto') {
            if (!session.state.serial && !(await chooseProbe(session))) return;
        }
        await runTask(session, action);
    }

    const handlers = {
        selectProbe: async session => {
            if (vscode.debug.activeDebugSession) throw new Error('Stop debugging before detecting a target.');
            if (await chooseProbe(session)) {
                const target = await detect(session);
                void vscode.window.showInformationMessage(`ST-LINK ${session.state.serial}: ${target.family}. ${target.board ? `${core.boards[target.board].label} inferred; use Board to override.` : 'Unsupported target; flashing is disabled.'}`);
            }
        },
        detectTarget: async session => {
            if (vscode.debug.activeDebugSession) throw new Error('Stop debugging before detecting a target.');
            if (!session.state.serial && !(await chooseProbe(session))) return;
            const target = await detect(session);
            void vscode.window.showInformationMessage(`${target.family}; ${target.board ? core.boards[target.board].label + ' inferred' : 'unsupported target'}.`);
        },
        selectBoard: async session => {
            const picked = await vscode.window.showQuickPick([
                { label: 'Auto — infer from selected target MCU', value: 'auto', detail: 'F42x/F43x targets suggest F439ZI; the exact board cannot be proven from the MCU ID.' },
                ...Object.entries(core.boards).map(([value, board]) => ({ label: board.label, value }))
            ], { title: 'Board configuration' });
            if (picked) { session.state.board = picked.value; await save(session); }
        },
        selectPreset: async session => {
            const picked = await vscode.window.showQuickPick(Object.entries(core.presets).map(([value, label]) => ({ label, value })), { title: 'Build mode' });
            if (picked) { session.state.preset = picked.value; await save(session); }
        },
        build: session => operate(session, 'build'),
        flash: session => operate(session, 'flash'),
        flashMonitor: session => operate(session, 'flash-monitor'),
        debug: async session => {
            if (!vscode.extensions.getExtension('marus25.cortex-debug')) throw new Error('Install the recommended Cortex-Debug extension to debug ARTOS.');
            if (vscode.debug.activeDebugSession) throw new Error('Stop the active debug session first.');
            if (!session.state.serial && !(await chooseProbe(session))) return;
            const target = await detect(session);
            await runTask(session, 'build');
            const info = await query(session, 'debug-info');
            const gdbPath = vscode.workspace.getConfiguration('cortex-debug', session.folder?.uri).get('gdbPath') || (process.platform === 'linux' ? 'gdb-multiarch' : undefined);
            const config = core.debugConfiguration(session.root, session.state, target, info.executable, session.openocd, gdbPath);
            if (!(await vscode.debug.startDebugging(session.folder, config))) throw new Error('The ARTOS debug session could not start.');
        }
    };

    for (const [name, handler] of Object.entries(handlers)) {
        context.subscriptions.push(vscode.commands.registerCommand(`artos.${name}`, async () => {
            if (busy) { vscode.window.showInformationMessage('An ARTOS operation is already running.'); return; }
            if (!vscode.workspace.isTrusted) return;
            busy = true;
            try {
                active = locate(); render();
                await handler(active);
            } catch (error) {
                output.appendLine(error.stack || error.message);
                vscode.window.showErrorMessage(error.message);
            } finally { busy = false; render(); }
        }));
    }
    const controls = [
        ['selectBoard', 'Select board configuration; Auto infers it from the target MCU.'],
        ['selectProbe', 'Refresh and select an ST-LINK, then detect its target.'],
        ['selectPreset', 'Select build mode.'], ['build', 'Build ARTOS.'],
        ['flash', 'Build, flash, verify and reset the selected ST-LINK target.'],
        ['flashMonitor', 'Flash and open the selected probe’s serial console (Linux).'],
        ['debug', 'Build and launch Cortex-Debug using the selected ST-LINK.']
    ];
    controls.forEach(([command, tooltip], index) => {
        const item = vscode.window.createStatusBarItem(`artos.${command}`, vscode.StatusBarAlignment.Left, 1000 - index);
        item.name = `ARTOS ${command}`; item.normalCommand = `artos.${command}`; item.normalTooltip = tooltip;
        items.push(item); context.subscriptions.push(item);
    });
    function refresh() {
        if (busy) return;
        try {
            const previousRoot = active?.root;
            active = locate();
            locateError = '';
            if (previousRoot !== active.root) output.appendLine(`ARTOS controls active for ${active.root}`);
        } catch (error) {
            active = undefined;
            if (locateError !== error.message) output.appendLine(error.message);
            locateError = error.message;
        }
        render();
    }
    context.subscriptions.push(vscode.commands.registerCommand('artos.showControls', async () => {
        refresh();
        if (active) {
            void vscode.window.showInformationMessage('ARTOS controls are in this window’s bottom status bar. If it is hidden, enable View > Appearance > Status Bar.');
        } else {
            const choice = await vscode.window.showInformationMessage(locateError, 'Open Project');
            if (choice === 'Open Project') await vscode.commands.executeCommand('vscode.openFolder');
        }
    }));
    context.subscriptions.push(vscode.workspace.onDidChangeWorkspaceFolders(refresh),
        vscode.window.onDidChangeActiveTextEditor(refresh), vscode.workspace.onDidChangeConfiguration(refresh));
    refresh();
}

module.exports = { activate };
