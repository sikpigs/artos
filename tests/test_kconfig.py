"""Host-side tests for generation, validation, and saved configuration behavior."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import kconfiglib

SOURCE = Path(__file__).resolve().parents[1]


class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.seed = self.root / "seed"
        self.build = self.root / "build"

    def generate(self, seed, success=True, build_type="Release"):
        self.seed.write_text(seed)
        result = subprocess.run(
            [sys.executable, str(SOURCE / "scripts/configure.py"),
             "--source", str(SOURCE), "--build", str(self.build),
             "--defconfig", str(self.seed), "--build-type", build_type],
            capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_cmake_presets_separate_firmware_modes_from_host_tests(self):
        presets = json.loads((SOURCE / "CMakePresets.json").read_text())
        configure = {preset["name"]: preset for preset in presets["configurePresets"]}
        self.assertEqual(set(configure),
                         {"base", "debug", "release", "rel-with-deb-info", "min-size", "host-tests"})
        self.assertEqual(configure["debug"]["cacheVariables"]["CMAKE_BUILD_TYPE"], "Debug")
        self.assertEqual(configure["release"]["cacheVariables"]["CMAKE_BUILD_TYPE"], "Release")
        self.assertEqual(configure["rel-with-deb-info"]["cacheVariables"]["CMAKE_BUILD_TYPE"],
                         "RelWithDebInfo")
        self.assertEqual(configure["min-size"]["cacheVariables"]["CMAKE_BUILD_TYPE"], "MinSizeRel")
        self.assertEqual(configure["host-tests"]["cacheVariables"]["ARTOS_HOST_TESTS"], "ON")
        self.assertNotIn("KCONFIG_DEFCONFIG", json.dumps(presets))
        self.assertEqual({preset["name"] for preset in presets["buildPresets"]},
                         {"debug", "release", "rel-with-deb-info", "min-size", "host-tests"})
        self.assertEqual({preset["name"] for preset in presets["testPresets"]},
                         {"all", "native", "python", "hooks"})
        self.assertIn('configs/default_defconfig',
                      (SOURCE / "cmake/kconfig.cmake").read_text())

    def test_defaults_and_unchanged_output(self):
        self.generate("")
        header = self.build / "generated/build_config.h"
        self.assertIn("tick_hz = 1000", header.read_text())
        self.assertIn("max_threads = 8", header.read_text())
        self.assertIn("runtime_history_window_ms = 1000", header.read_text())
        self.assertIn("runtime_history_bucket_count = 10", header.read_text())
        self.assertIn("max_deferred_work_items = 8", header.read_text())
        self.assertIn("max_active_guards = 8", header.read_text())
        self.assertIn("scheduler = fixed_priority_round_robin_config { "
                      ".priority_aging_ticks = std::nullopt }", header.read_text())
        self.assertNotIn("strict_fixed_priority_config", header.read_text())
        before = header.stat().st_mtime_ns
        self.generate("")
        self.assertEqual(before, header.stat().st_mtime_ns)

    def test_scheduler_cutoff_defaults_and_override(self):
        for build_type, expected in (("Debug", 2500), ("Release", 500), ("MinSizeRel", 800)):
            with self.subTest(build_type=build_type):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate("", build_type=build_type)
                self.assertIn(f"scheduler_min_cycles_remaining = {expected}",
                              (self.build / "generated/build_config.h").read_text())

        (self.build / ".config").unlink()
        self.generate("CONFIG_ARTOS_SCHEDULER_MIN_CYCLES_REMAINING=0\n", build_type="Debug")
        self.assertIn("scheduler_min_cycles_remaining = 0",
                      (self.build / "generated/build_config.h").read_text())

    def test_max_thread_name_length_config(self):
        self.generate("")
        self.assertIn("max_thread_name_length = 15",
                      (self.build / "generated/build_config.h").read_text())

        (self.build / ".config").unlink()
        self.generate("CONFIG_ARTOS_MAX_THREAD_NAME_LENGTH=31\n")
        self.assertIn("max_thread_name_length = 31",
                      (self.build / "generated/build_config.h").read_text())
        self.assertIn("#define CONFIG_ARTOS_MAX_THREAD_NAME_LENGTH 31",
                      (self.build / "generated/autoconf.h").read_text())
        probe = "#include <artos.h>\nstatic_assert(sizeof(artos::thread::name_t) == 32);\n"
        result = subprocess.run(
            ["c++", "-std=c++23", "-fsyntax-only", "-x", "c++",
             "-I", str(self.build / "generated"), "-I", str(SOURCE / "include"),
             "-I", str(SOURCE / "port/include"),
             "-I", str(SOURCE / "arch/arm/cortex_m4/include"), "-"],
            input=probe, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def check_arm_tcb_layout(self, aging):
        compiler = shutil.which("arm-none-eabi-g++")
        if compiler is None:
            self.skipTest("arm-none-eabi-g++ is required for ARM TCB layout checks")
        # Cover each name-padding residue and optional TCB extension layout.
        for name_length, extensions in ((15, False), (1, False), (2, False), (3, False),
                                        (15, True), (1, True), (2, True), (3, True)):
            with self.subTest(aging=aging, name_length=name_length, extensions=extensions):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate(
                    f"CONFIG_ARTOS_SCHEDULER_PRIORITY_AGING={'y' if aging else 'n'}\n"
                    f"CONFIG_ARTOS_MAX_THREAD_NAME_LENGTH={name_length}\n"
                    f"CONFIG_ARTOS_THREAD_STACK_HWM={'y' if extensions else 'n'}\n"
                    f"CONFIG_ARTOS_THREAD_STACK_CANARY={'y' if extensions else 'n'}\n"
                    f"CONFIG_ARTOS_THREAD_STATS={'y' if extensions else 'n'}\n")
                # Compile the real ARM layout, including storage size assertions.
                probe = "#include <internal/thread.h>\n"
                result = subprocess.run(
                    [compiler, "-std=c++23", "-mcpu=cortex-m4", "-mthumb",
                     "-fsyntax-only", "-x", "c++",
                     "-I", str(self.build / "generated"),
                     "-I", str(SOURCE / "include"),
                     "-I", str(SOURCE / "port/include"),
                     "-I", str(SOURCE / "arch/arm/cortex_m4/include"),
                     "-I", str(SOURCE / "kernel"), "-"],
                    input=probe, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_arm_tcb_layout_without_aging(self):
        self.check_arm_tcb_layout(False)

    def test_arm_tcb_layout_with_aging(self):
        self.check_arm_tcb_layout(True)

    def test_thread_stack_hwm_config(self):
        self.generate("")
        self.assertIn("thread_stack_hwm = false",
                      (self.build / "generated/build_config.h").read_text())
        self.assertNotIn("CONFIG_ARTOS_THREAD_STACK_HWM",
                         (self.build / "generated/autoconf.h").read_text())

        (self.build / ".config").unlink()
        self.generate("CONFIG_ARTOS_THREAD_STACK_HWM=y\n")
        self.assertIn("thread_stack_hwm = true",
                      (self.build / "generated/build_config.h").read_text())
        self.assertIn("#define CONFIG_ARTOS_THREAD_STACK_HWM 1",
                      (self.build / "generated/autoconf.h").read_text())

    def test_thread_stack_protection_layout(self):
        cases = (
            ("", False),
            ("CONFIG_ARTOS_THREAD_STACK_PROTECTION=y\n", True),
            ("CONFIG_BOARD_NUCLEO_F439ZI=y\n", False),
            ("CONFIG_BOARD_NUCLEO_F439ZI=y\nCONFIG_ARTOS_THREAD_STACK_PROTECTION=y\n", True),
            ("CONFIG_ARTOS=n\nCONFIG_ARTOS_THREAD_STACK_PROTECTION=y\n", False),
        )
        for seed, enabled in cases:
            with self.subTest(seed=seed):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate(seed)
                self.assertIn(f"thread_stack_protection = {str(enabled).lower()}",
                              (self.build / "generated/build_config.h").read_text())
                autoconf = (self.build / "generated/autoconf.h").read_text()
                self.assertEqual("#define CONFIG_ARTOS_THREAD_STACK_PROTECTION 1" in autoconf,
                                 enabled)
                alignment = 32 if enabled else 8
                prefix = 32 if enabled else 0
                probe = (
                    "#include <artos.h>\n"
                    "using storage = artos::thread::storage_t<1024>;\n"
                    f"static_assert(storage::guard_words * sizeof(artos::thread::stack_word_t) == {prefix});\n"
                    "static_assert(storage::usable_words * sizeof(artos::thread::stack_word_t) == 1024);\n"
                    f"static_assert(alignof(storage) >= {alignment});\n"
                    f"static_assert((offsetof(storage, stack_allocation) + {prefix}) % {alignment} == 0);\n"
                )
                result = subprocess.run(
                    ["c++", "-std=c++23", "-fsyntax-only", "-x", "c++",
                     "-I", str(self.build / "generated"), "-I", str(SOURCE / "include"),
                     "-I", str(SOURCE / "port/include"),
                     "-I", str(SOURCE / "arch/arm/cortex_m4/include"), "-"],
                    input=probe, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_thread_stats_config(self):
        self.generate("")
        self.assertIn("thread_stats = false",
                      (self.build / "generated/build_config.h").read_text())
        self.assertNotIn("CONFIG_ARTOS_THREAD_STATS",
                         (self.build / "generated/autoconf.h").read_text())

        (self.build / ".config").unlink()
        self.generate("CONFIG_ARTOS_THREAD_STATS=y\n")
        self.assertIn("thread_stats = true",
                      (self.build / "generated/build_config.h").read_text())
        self.assertIn("#define CONFIG_ARTOS_THREAD_STATS 1",
                      (self.build / "generated/autoconf.h").read_text())

    def test_runtime_history_duration_and_storage(self):
        for window_ms, bucket_count, bucket_ms in ((1000, 10, 100), (1000, 100, 10), (1200, 12, 100)):
            with self.subTest(window_ms=window_ms, bucket_count=bucket_count):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate("CONFIG_ARTOS_THREAD_STATS=y\n"
                              f"CONFIG_ARTOS_RUNTIME_HISTORY_WINDOW_MS={window_ms}\n"
                              f"CONFIG_ARTOS_RUNTIME_HISTORY_BUCKET_COUNT={bucket_count}\n")
                header = (self.build / "generated/build_config.h").read_text()
                self.assertIn(f"runtime_history_window_ms = {window_ms}", header)
                self.assertIn(f"runtime_history_bucket_count = {bucket_count}", header)
                self.assertIn("runtime_history_window_ms / runtime_history_bucket_count", header)
                probe = f"""#include <artos.h>
#include <array>
#include <type_traits>
static_assert(build_config::artos::runtime_history_bucket_ms == {bucket_ms});
static_assert(std::is_same_v<
    decltype(artos::thread::statistics_t{{}}.runtime_history_cycles),
    std::array<std::uint32_t, {bucket_count}>>);
"""
                result = subprocess.run(
                    ["c++", "-std=c++23", "-fsyntax-only", "-x", "c++",
                     "-I", str(self.build / "generated"), "-I", str(SOURCE / "include"),
                     "-I", str(SOURCE / "port/include"),
                     "-I", str(SOURCE / "arch/arm/cortex_m4/include"), "-"],
                    input=probe, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_runtime_history_rejects_uneven_buckets_at_compile_time(self):
        self.generate("CONFIG_ARTOS_THREAD_STATS=y\n"
                      "CONFIG_ARTOS_RUNTIME_HISTORY_WINDOW_MS=1000\n"
                      "CONFIG_ARTOS_RUNTIME_HISTORY_BUCKET_COUNT=30\n")
        result = subprocess.run(
            ["c++", "-std=c++23", "-fsyntax-only", "-x", "c++",
             "-I", str(self.build / "generated"), "-"],
            input="#include <build_config.h>\n", capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("runtime history window must divide evenly", result.stderr)

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
        self.assertIn("thread_stack_hwm = false", header)
        self.assertIn("thread_stats = false", header)
        self.assertIn("max_deferred_work_items = 8", header)
        self.assertIn("max_active_guards = 8", header)
        self.assertIn("scheduler = fixed_priority_round_robin_config { "
                      ".priority_aging_ticks = std::nullopt }", header)
        self.assertNotIn("strict_fixed_priority_config", header)
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

    def test_active_guard_limits_and_saved_value(self):
        for limit in (1, 12, 64):
            with self.subTest(limit=limit):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate(f"CONFIG_ARTOS_MAX_ACTIVE_GUARDS={limit}\n")
                self.generate("CONFIG_ARTOS_MAX_ACTIVE_GUARDS=8\n")
                self.assertIn(f"max_active_guards = {limit}",
                              (self.build / "generated/build_config.h").read_text())
                self.assertIn(f"#define CONFIG_ARTOS_MAX_ACTIVE_GUARDS {limit}",
                              (self.build / "generated/autoconf.h").read_text())

    def test_invalid_active_guard_limits_rejected(self):
        for limit in (0, 65):
            with self.subTest(limit=limit):
                self.generate(f"CONFIG_ARTOS_MAX_ACTIVE_GUARDS={limit}\n", success=False)
                self.assertFalse((self.build / "generated/build_config.h").exists())

    def test_fixed_priority_round_robin_is_not_selectable(self):
        with mock.patch.dict(os.environ, {"srctree": str(SOURCE)}):
            kconf = kconfiglib.Kconfig(str(SOURCE / "Kconfig"), warn_to_stderr=False)
        symbol = kconf.syms["ARTOS_SCHEDULER_FIXED_PRIORITY_ROUND_ROBIN"]
        self.assertTrue(symbol.nodes)
        self.assertTrue(all(node.prompt is None for node in symbol.nodes))
        self.assertNotIn("ARTOS_SCHEDULER_STRICT_FIXED_PRIORITY", kconf.syms)

    def test_priority_aging_interval_and_saved_value(self):
        self.generate("CONFIG_ARTOS_TICK_HZ=2000\nCONFIG_ARTOS_SCHEDULER_PRIORITY_AGING=y\n")
        header = (self.build / "generated/build_config.h").read_text()
        self.assertIn("scheduler = fixed_priority_round_robin_config { "
                      ".priority_aging_ticks = 2000 }", header)

        (self.build / ".config").unlink()
        self.generate("CONFIG_ARTOS_SCHEDULER_PRIORITY_AGING=y\nCONFIG_ARTOS_PRIORITY_AGING_TICKS=250\n")
        self.generate("")
        header = (self.build / "generated/build_config.h").read_text()
        self.assertIn("scheduler = fixed_priority_round_robin_config { "
                      ".priority_aging_ticks = 250 }", header)
        self.assertIn("#define CONFIG_ARTOS_PRIORITY_AGING_TICKS 250",
                      (self.build / "generated/autoconf.h").read_text())

    def test_invalid_priority_aging_intervals_rejected(self):
        for ticks in (0, 2147483648):
            with self.subTest(ticks=ticks):
                (self.build / ".config").unlink(missing_ok=True)
                self.generate("CONFIG_ARTOS_SCHEDULER_PRIORITY_AGING=y\n"
                              f"CONFIG_ARTOS_PRIORITY_AGING_TICKS={ticks}\n", success=False)
                self.assertFalse((self.build / "generated/build_config.h").exists())

    def test_invalid_thread_limits_rejected(self):
        for limit in (1, 65):
            with self.subTest(limit=limit):
                self.generate(f"CONFIG_ARTOS_MAX_THREADS={limit}\n", success=False)
                self.assertFalse((self.build / "generated/build_config.h").exists())


if __name__ == "__main__":
    unittest.main()
