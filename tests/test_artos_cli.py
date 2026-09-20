"""Unit tests for the ARTOS project command driver."""

from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts import artos_cli  # noqa: E402


class Recorder:
    def __init__(self):
        self.commands = []

    def __call__(self, command):
        self.commands.append(tuple(command))


class ArtosCliTests(unittest.TestCase):
    def run_cli(self, *arguments):
        recorder = Recorder()
        with patch.object(artos_cli, "DEFAULT_PRESET", "debug"):
            self.assertEqual(artos_cli.main(arguments, run=recorder), 0)
        return recorder.commands

    def test_build_configures_selected_preset_first(self):
        self.assertEqual(
            self.run_cli("--preset", "release", "build", "-j", "3", "app-target"),
            [
                ("cmake", "--preset", "release"),
                ("cmake", "--build", "--preset", "release", "--target",
                 "app-target", "--parallel", "3"),
            ],
        )

    def test_configure_passes_cache_definitions_without_a_shell(self):
        self.assertEqual(
            self.run_cli("configure", "--fresh", "-D", "OPENOCD_EXECUTABLE=/opt/open ocd"),
            [("cmake", "--preset", "debug", "--fresh",
              "-DOPENOCD_EXECUTABLE=/opt/open ocd")],
        )

    def test_check_uses_all_firmware_check_targets(self):
        commands = self.run_cli("--preset", "min-size", "check")
        self.assertEqual(commands[0], ("cmake", "--preset", "min-size"))
        self.assertEqual(
            commands[1],
            ("cmake", "--build", "--preset", "min-size", "--target",
             *artos_cli.CHECK_TARGETS),
        )

    def test_monitor_forwards_explicit_port(self):
        with patch.object(artos_cli, "project_python", return_value="python-for-project"):
            self.assertEqual(
                self.run_cli("monitor", "--port", "/dev/ttyACM7"),
                [("python-for-project", str(artos_cli.ROOT / "scripts" / "serial_console.py"),
                  "--port", "/dev/ttyACM7")],
            )

    def test_flash_monitor_stops_at_flash_before_monitor(self):
        with patch.object(artos_cli, "project_python", return_value="python-for-project"):
            commands = self.run_cli("flash-monitor")
        self.assertEqual(commands[0], ("cmake", "--preset", "debug"))
        self.assertEqual(
            commands[1],
            ("cmake", "--build", "--preset", "debug", "--target", "flash"),
        )
        self.assertEqual(commands[2][0], "python-for-project")

    def test_test_command_delegates_everything_to_ctest(self):
        self.assertEqual(self.run_cli("test"), [
            ("cmake", "--preset", "host-tests"),
            ("cmake", "--build", "--preset", "host-tests"),
            ("ctest", "--preset", "all"),
        ])


if __name__ == "__main__":
    unittest.main()
