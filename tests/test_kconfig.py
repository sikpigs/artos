"""Host-side tests for generation, validation, and saved configuration behavior."""
import json
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

    def test_cmake_presets_only_select_build_modes(self):
        presets = json.loads((SOURCE / "CMakePresets.json").read_text())
        configure = {preset["name"]: preset for preset in presets["configurePresets"]}
        self.assertEqual(set(configure), {"base", "debug", "release", "min-size"})
        self.assertEqual(configure["debug"]["cacheVariables"]["CMAKE_BUILD_TYPE"], "Debug")
        self.assertEqual(configure["release"]["cacheVariables"]["CMAKE_BUILD_TYPE"], "Release")
        self.assertEqual(configure["min-size"]["cacheVariables"]["CMAKE_BUILD_TYPE"], "MinSizeRel")
        self.assertNotIn("KCONFIG_DEFCONFIG", json.dumps(presets))
        self.assertEqual({preset["name"] for preset in presets["buildPresets"]},
                         {"debug", "release", "min-size"})
        self.assertIn('configs/default_defconfig',
                      (SOURCE / "cmake/kconfig.cmake").read_text())

    def test_defaults_and_unchanged_output(self):
        self.generate("")
        header = self.build / "generated/build_config.h"
        self.assertIn("tick_hz = 1000", header.read_text())
        self.assertIn("max_threads = 8", header.read_text())
        self.assertIn("max_deferred_work_items = 8", header.read_text())
        before = header.stat().st_mtime_ns
        self.generate("")
        self.assertEqual(before, header.stat().st_mtime_ns)

    def test_saved_config_overrides_seed(self):
        self.generate("CONFIG_ARTOS_TICK_HZ=2000\nCONFIG_ARTOS_PRIORITY_LEVELS=256\nCONFIG_ARTOS_MAX_THREADS=12\n")
        self.generate("CONFIG_ARTOS_TICK_HZ=1000\n")
        text = (self.build / "generated/build_config.h").read_text()
        self.assertIn("tick_hz = 2000", text)
        self.assertIn("priority_levels = 256", text)
        self.assertIn("max_threads = 12", text)

    def test_kernel_disabled(self):
        self.generate("# CONFIG_ARTOS is not set\nCONFIG_CONSOLE=y\nCONFIG_MONITOR=y\nCONFIG_CONSOLE_RX_BUFFER_SIZE=512\n")
        cmake = (self.build / "generated/config.cmake").read_text()
        self.assertIn("set(CONFIG_ARTOS OFF)", cmake)
        self.assertIn("set(CONFIG_CONSOLE OFF)", cmake)
        self.assertIn("set(CONFIG_MONITOR OFF)", cmake)
        header = (self.build / "generated/build_config.h").read_text()
        self.assertIn("enabled = false", header)
        self.assertIn("max_threads = 8", header)
        self.assertIn("max_deferred_work_items = 8", header)
        self.assertIn("rx_buffer_size = 0", header)

    def test_monitor_disabled_keeps_console_buffer(self):
        self.generate("# CONFIG_MONITOR is not set\n")
        cmake = (self.build / "generated/config.cmake").read_text()
        self.assertIn("set(CONFIG_MONITOR OFF)", cmake)
        header = (self.build / "generated/build_config.h").read_text()
        self.assertIn("rx_buffer_size = 256", header)

    def test_console_disabled_disables_monitor(self):
        self.generate("# CONFIG_CONSOLE is not set\n")
        cmake = (self.build / "generated/config.cmake").read_text()
        self.assertIn("set(CONFIG_CONSOLE OFF)", cmake)
        self.assertIn("set(CONFIG_MONITOR OFF)", cmake)
        self.assertIn("rx_buffer_size = 0",
                      (self.build / "generated/build_config.h").read_text())

    def test_retired_monitor_workspace_option_rejected(self):
        result = self.generate("CONFIG_MONITOR_WORKSPACE_SIZE=160\n", success=False)
        self.assertIn("undefined symbol MONITOR_WORKSPACE_SIZE", result.stderr)

    def test_f439_selects_soc_variant_and_cpu(self):
        self.generate("CONFIG_BOARD_NUCLEO_F439ZI=y\n")
        cmake = (self.build / "generated/config.cmake").read_text()
        self.assertIn('set(CONFIG_BSP "st/nucleo_f439zi")', cmake)
        self.assertIn('set(CONFIG_SOC "stm32f439")', cmake)
        self.assertIn('set(CONFIG_MCU_VARIANT "stm32f439zi")', cmake)
        self.assertIn('set(CONFIG_CPU "cortex_m4")', cmake)
        self.generate("CONFIG_BOARD_NUCLEO_F411RE=y\n")
        self.assertIn('set(CONFIG_BOARD "nucleo_f439zi")',
                      (self.build / "generated/config.cmake").read_text())

    def test_invalid_range_rejected(self):
        result = self.generate("CONFIG_ARTOS_PRIORITY_LEVELS=257\n", success=False)
        self.assertIn("range", result.stderr)
        self.assertFalse((self.build / "generated/build_config.h").exists())

    def test_unknown_symbol_rejected(self):
        result = self.generate("CONFIG_BOARD_MISSPELLED=y\n", success=False)
        self.assertIn("BOARD_MISSPELLED", result.stderr)

    def test_thread_limit_boundaries(self):
        for limit in (2, 64):
            with self.subTest(limit=limit):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate(f"CONFIG_ARTOS_MAX_THREADS={limit}\n")
                self.assertIn(f"max_threads = {limit}",
                              (self.build / "generated/build_config.h").read_text())

    def test_deferred_work_limits_and_saved_value(self):
        for limit in (1, 12, 64):
            with self.subTest(limit=limit):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate(f"CONFIG_ARTOS_MAX_DEFERRED_WORK_ITEMS={limit}\n")
                self.generate("CONFIG_ARTOS_MAX_DEFERRED_WORK_ITEMS=8\n")
                self.assertIn(f"max_deferred_work_items = {limit}",
                              (self.build / "generated/build_config.h").read_text())
                self.assertIn(f"#define CONFIG_ARTOS_MAX_DEFERRED_WORK_ITEMS {limit}",
                              (self.build / "generated/autoconf.h").read_text())

    def test_invalid_deferred_work_limits_rejected(self):
        for limit in (0, 65):
            with self.subTest(limit=limit):
                self.generate(f"CONFIG_ARTOS_MAX_DEFERRED_WORK_ITEMS={limit}\n", success=False)
                self.assertFalse((self.build / "generated/build_config.h").exists())

    def test_invalid_thread_limits_rejected(self):
        for limit in (1, 65):
            with self.subTest(limit=limit):
                self.generate(f"CONFIG_ARTOS_MAX_THREADS={limit}\n", success=False)
                self.assertFalse((self.build / "generated/build_config.h").exists())


if __name__ == "__main__":
    unittest.main()
