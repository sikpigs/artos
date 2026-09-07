"""Host-side tests for generation, validation, and saved configuration behavior."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]


class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.seed = self.root / "seed"
        self.build = self.root / "build"

    def generate(self, seed, success=True):
        self.seed.write_text(seed)
        result = subprocess.run(
            [sys.executable, str(SOURCE / "scripts/configure.py"),
             "--source", str(SOURCE), "--build", str(self.build),
             "--defconfig", str(self.seed)], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_defaults_and_unchanged_output(self):
        self.generate("")
        header = self.build / "generated/build_config.h"
        self.assertIn("tick_hz = 1000", header.read_text())
        before = header.stat().st_mtime_ns
        self.generate("")
        self.assertEqual(before, header.stat().st_mtime_ns)

    def test_saved_config_overrides_seed(self):
        self.generate("CONFIG_ARTOS_TICK_HZ=2000\nCONFIG_ARTOS_PRIORITY_LEVELS=256\n")
        self.generate("CONFIG_ARTOS_TICK_HZ=1000\n")
        text = (self.build / "generated/build_config.h").read_text()
        self.assertIn("tick_hz = 2000", text)
        self.assertIn("priority_levels = 256", text)

    def test_kernel_disabled(self):
        self.generate("# CONFIG_ARTOS is not set\nCONFIG_MONITOR_RX_BUFFER_SIZE=512\n")
        self.assertIn("set(CONFIG_ARTOS OFF)", (self.build / "generated/config.cmake").read_text())
        header = (self.build / "generated/build_config.h").read_text()
        self.assertIn("enabled = false", header)
        self.assertIn("rx_buffer_size = 512", header)

    def test_monitor_disabled_releases_memory(self):
        self.generate("# CONFIG_MONITOR is not set\n")
        cmake = (self.build / "generated/config.cmake").read_text()
        self.assertIn("set(CONFIG_MONITOR OFF)", cmake)
        self.assertIn("set(CONFIG_MONITOR_WORKSPACE_SIZE 0)", cmake)
        header = (self.build / "generated/build_config.h").read_text()
        self.assertIn("workspace_size = 0", header)
        self.assertIn("rx_buffer_size = 0", header)

    def test_monitor_workspace_size(self):
        self.generate("CONFIG_MONITOR_WORKSPACE_SIZE=160\n")
        self.assertIn("set(CONFIG_MONITOR_WORKSPACE_SIZE 160)",
                      (self.build / "generated/config.cmake").read_text())

    def test_monitor_workspace_too_small(self):
        self.generate("CONFIG_MONITOR_WORKSPACE_SIZE=159\n", success=False)

    def test_invalid_range_rejected(self):
        result = self.generate("CONFIG_ARTOS_PRIORITY_LEVELS=257\n", success=False)
        self.assertIn("range", result.stderr)
        self.assertFalse((self.build / "generated/build_config.h").exists())

    def test_unknown_symbol_rejected(self):
        result = self.generate("CONFIG_BOARD_MISSPELLED=y\n", success=False)
        self.assertIn("BOARD_MISSPELLED", result.stderr)


if __name__ == "__main__":
    unittest.main()
