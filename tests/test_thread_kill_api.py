"""Compile task termination APIs and feature-dependent native/ARM TCB layouts."""

import itertools
from pathlib import Path
import shutil
import subprocess
import unittest

SOURCE = Path(__file__).resolve().parents[1]
PROBE = r"""#include <artos.h>
#include <internal/thread.h>
#include <type_traits>
using artos::thread::try_kill;
using artos::thread::kill;
using artos::thread::kill_result_t;
using task_id = artos::thread::id_t;
static_assert(std::is_same_v<decltype(try_kill(task_id{})), kill_result_t>);
static_assert(std::is_same_v<decltype(kill(task_id{})), kill_result_t>);
static_assert(noexcept(try_kill(task_id{})) && noexcept(kill(task_id{})));
static_assert(kill_result_t::busy != kill_result_t::success);
static_assert(kill_result_t::unsupported != kill_result_t::invalid_thread);
void exercise(task_id id) {
    (void)try_kill(id);
    (void)kill(id);
}
"""


class ThreadKillApiTests(unittest.TestCase):
    def check_layouts(self, compiler_name, target_flags=()):
        compiler = shutil.which(compiler_name)
        if compiler is None:
            self.skipTest(f"{compiler_name} is unavailable")
        features = ("ARTOS_TEST_PRIORITY_AGING", "ARTOS_TEST_THREAD_STATS",
                    "ARTOS_TEST_THREAD_CANARY")
        for enabled in itertools.product((False, True), repeat=len(features)):
            flags = [f"-D{name}=1" for name, use in zip(features, enabled) if use]
            with self.subTest(features=flags):
                command = [compiler, "-std=c++23", "-Wall", "-Wextra", "-Werror",
                           "-fsyntax-only", "-x", "c++", *target_flags, *flags]
                for directory in ("tests/host/support", "include", "port/include",
                                  "arch/arm/cortex_m4/include", "kernel"):
                    command.extend(("-I", str(SOURCE / directory)))
                command.append("-")
                result = subprocess.run(command, input=PROBE, capture_output=True,
                                        text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_native_api_and_layouts(self):
        self.check_layouts("c++")

    def test_arm_api_and_layouts(self):
        self.check_layouts("arm-none-eabi-g++", ("-mcpu=cortex-m4", "-mthumb"))


if __name__ == "__main__":
    unittest.main()
