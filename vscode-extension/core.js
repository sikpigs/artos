'use strict';
const fs = require('node:fs');
const path = require('node:path');

const boards = {
    nucleo_f411re: { label: 'NUCLEO-F411RE', soc: 'stm32f411', device: 'STM32F411RE' },
    nucleo_f439zi: { label: 'NUCLEO-F439ZI', soc: 'stm32f439', device: 'STM32F439ZI' }
};
const presets = {
    debug: 'Debug', release: 'Release',
    'rel-with-deb-info': 'Release with Debug Info', 'min-size': 'Minimum Size'
};

function projectRoot(folders, configured, extensionPath) {
    const candidates = [];
    for (const folder of folders) {
        const setting = configured(folder);
        if (setting) candidates.push(path.resolve(folder.uri.fsPath, setting));
        else candidates.push(folder.uri.fsPath, path.dirname(folder.uri.fsPath));
    }
    if (!folders.length) candidates.push(path.dirname(extensionPath));
    return candidates.find(root => fs.existsSync(path.join(root, 'artos.py')));
}

function pythonPath(root, configured, platform = process.platform) {
    if (configured) return configured;
    const local = path.join(root, '.venv', platform === 'win32' ? 'Scripts/python.exe' : 'bin/python');
    return fs.existsSync(local) ? local : platform === 'win32' ? 'python' : 'python3';
}

function helperArgs(helper, root, action, state, openocd) {
    const args = [helper, action, '--root', root, '--preset', state.preset,
        '--board', state.board, '--openocd', openocd];
    if (state.serial) args.push('--serial', state.serial);
    return args;
}

function debugConfiguration(root, state, target, executable, openocd, gdbPath) {
    const board = boards[state.board === 'auto' ? target.board : state.board];
    if (!board) throw new Error('Choose a supported board before debugging.');
    const config = {
        name: `ARTOS: ${board.label} (${state.serial})`, type: 'cortex-debug', request: 'launch',
        servertype: 'openocd', cwd: root, executable, serverpath: openocd,
        configFiles: [path.join(root, 'bsp/boards/st', state.board === 'auto' ? target.board : state.board, 'openocd.cfg')],
        openOCDLaunchCommands: [`adapter serial ${state.serial}`],
        device: board.device, runToEntryPoint: 'main',
        svdFile: path.join(root, 'bsp/soc/st/stm32f4', board.soc, 'svd', `${board.soc.toUpperCase()}.svd`)
    };
    if (gdbPath) config.gdbPath = gdbPath;
    return config;
}

module.exports = { boards, presets, projectRoot, pythonPath, helperArgs, debugConfiguration };
