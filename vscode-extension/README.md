# ARTOS Board Tools

A local VS Code extension for this firmware project. The status bar provides a
board configuration selector, ST-LINK selector, build mode, Build, Flash,
Flash + Monitor, and Debug. Probe and mode selections are remembered per project.

## Run from source

1. Open **this `vscode-extension` directory** as a folder in VS Code.
2. Press **F5** and choose **Run ARTOS extension**. No npm install or compilation
   is needed: the extension uses JavaScript and Node's built-in libraries.
3. The Extension Development Host opens the parent firmware project. Click
   **Select ST-LINK**, then use **Flash**. One connected probe is selected
   automatically; with several probes, the menu shows their full USB serials.

The launcher uses a separate local `.dev-host/` profile and forces a new window
so an existing project window cannot capture the development launch. The new
window's title should include **Extension Development Host**. Trust this local
project if prompted. The development profile is excluded from Git and VSIX
packages; your regular VS Code profile is preserved.

Commands are also available in the Command Palette under **ARTOS**.
The extension runs only in trusted local workspaces.

## Install for regular use

With the VS Code extension packager (`vsce`) installed:

```sh
cd vscode-extension
npm run package
```

In VS Code, run **Extensions: Install from VSIX...** and choose
`artos-board-tools-0.1.2.vsix`. Reload and open the firmware project.
The included VSIX, when built, is ignored by Git; the source remains in this folder.
This does not publish anything to the Marketplace.

## If the controls are missing

During development the buttons appear in the **new Extension Development Host
window**, not the original window where you pressed F5. Check the bottom status
bar; enable **View > Appearance > Status Bar** if it is hidden. Then run
**ARTOS: Show Controls** from that window's Command Palette. If project detection
fails, the status bar shows **ARTOS: Open Project** and the command explains how
to select the folder containing `artos.py`. The **ARTOS Board Tools** output
channel records the detected project or the reason detection failed.

If the ARTOS commands do not appear in that window's Command Palette, the
extension has not loaded. Restart **Run ARTOS extension** from this folder,
or install the current VSIX for use in your regular window. Trust the project
when prompted; the extension is disabled in Restricted Mode.

## Tools and configuration

Install the firmware prerequisites described in the project's root `README.md`,
including the project `.venv`, Kconfiglib, CMake, Ninja, the ARM toolchain and
OpenOCD. Linux discovery uses USB sysfs and needs no additional Python packages.
ST-LINK access still requires the usual USB/udev permissions.

On macOS or Windows, USB discovery uses PyUSB. Install `requirements.txt` from
this directory into the Python environment used by the extension, and install
libusb with a backend/USB driver compatible with OpenOCD. Flash + Monitor uses
this project's existing Linux serial terminal; on other platforms use an
external serial terminal. These platforms have not been tested with hardware.

Optional VS Code settings:

- `artos.projectRoot`: path to the directory containing `artos.py`. Relative
  paths resolve against the workspace folder. Normally the extension finds the
  project automatically, including when this extension folder is opened.
- `artos.pythonPath`: Python executable. By default it uses the project's
  `.venv`, then `python3` (`python` on Windows).
- `artos.openocdPath`: OpenOCD executable, default `openocd`.

For Debug, install **Cortex-Debug** (`marus25.cortex-debug`). It builds the selected
mode and uses that board's OpenOCD configuration, SVD, and the selected ST-LINK
serial. Linux defaults to `gdb-multiarch`; set `cortex-debug.gdbPath` to override.
The project's custom `ARTOS_OUTPUT_NAME` is respected.
Stop a debug session before refreshing/detecting a probe or starting another
operation. Detection opens a debug connection, but does not request a reset or
flash. Other applications using the probe must also release it.

## Detection and board selection

USB discovery lists probe identities without connecting to the target. Selecting
a probe reads the attached STM32's DBGMCU device ID and flash-size register via
OpenOCD. The supported mappings are:

| Target information | Inferred configuration |
| --- | --- |
| STM32F411, device ID `0x431`, 512 KiB flash | NUCLEO-F411RE |
| STM32F42x/F43x, device ID `0x419`, 2048 KiB flash | NUCLEO-F439ZI |

These are **inferences among the project's supported boards**. MCU information
cannot establish the PCB model, package, pin wiring, or the exact F42x/F43x variant.
An F429 board, for example, can have the same ID and flash size as an F439 board.
Only use the inferred F439ZI configuration when the attached hardware is actually
compatible. The Board selector provides Auto and explicit configurations. Unknown
targets, insufficient flash sizes and mismatched board families are rejected
before configuration/build/flash. A manual selection does not bypass that check.

Before every operation involving a selected probe, the helper rechecks its MCU
and flash size. A disconnected saved probe causes an error; the extension never
silently switches to another connected probe. For an offline build, choose a
board explicitly before selecting a probe.

Changing boards updates the build mode's Kconfig choice through Kconfiglib and
retains saved software settings. A new configuration uses the matching board
seed (or the previously cached `KCONFIG_DEFCONFIG`). Each build mode keeps its
own settings. CMake then builds/programs through `artos.py`; build output appears
in a cancellable VS Code task terminal. Operations are serialized.

The selected serial is cached as `OPENOCD_SERIAL` in that build directory, so
ordinary `artos.py flash` and `debug-server` calls also use it afterward. To return
the CLI to its original unrestricted probe selection:

```sh
./artos.py --preset debug configure -D OPENOCD_SERIAL=
```

Flash + Monitor resolves the UART belonging to the selected USB serial before
flashing. It never falls back to another probe's console. **Ctrl+]** exits the
monitor. Use the task terminal's stop control to cancel a build/flash operation.

## Verify

From the project root:

```sh
node --test vscode-extension/test/*.test.js
.venv/bin/python -m unittest discover -s vscode-extension/test -p 'test_*.py'
```

Tests cover target identification, unsupported/mismatched devices, serial
validation and routing, configuration preservation, USB filtering, and the
extension's command/task wiring. Physical flashing needs a connected board.
