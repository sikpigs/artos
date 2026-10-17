"""Compile the event interface and TCB layout with native and ARM compilers."""

from pathlib import Path
import itertools
import shutil
import subprocess
import unittest


SOURCE = Path(__file__).resolve().parents[1]
PROBE = r"""#include <event.h>
#include <internal/thread.h>
#include <type_traits>
#include <utility>

using event = artos::event_t;
using result = artos::event_result_t;
static_assert(std::is_same_v<decltype(std::declval<event&>().post(1)), result>);
static_assert(std::is_same_v<decltype(std::declval<event&>().set(1)), result>);
static_assert(std::is_same_v<decltype(std::declval<event&>().post_from_isr(1)), result>);
static_assert(std::is_same_v<decltype(std::declval<event&>().set_from_isr(1)), result>);

void exercise(event& bits) {
    bool woken = false;
    (void)bits.post(1);
    (void)bits.set(1);
    (void)bits.post_from_isr(1);
    (void)bits.post_from_isr(1, nullptr);
    (void)bits.post_from_isr(1, &woken);
    (void)bits.set_from_isr(1);
    (void)bits.set_from_isr(1, nullptr);
    (void)bits.set_from_isr(1, &woken);
    (void)bits.wait_for(artos::any_t{1}, false, artos::tick_t{0});
    (void)bits.wait_for(artos::all_t{1}, true, artos::tick_t{1});
    (void)bits.wait_for(artos::any_t{1}, true, artos::wait_forever);
    (void)bits.wait_for(artos::all_t{1}, false, artos::wait_forever);
    bits.close();
    bits.close_and_wait();
}

void destroy(event* bits) { bits->close_and_delete(); }
"""


class EventApiTests(unittest.TestCase):
    def compile_api(self, compiler_name, extra_flags=(), probe=PROBE):
        compiler = shutil.which(compiler_name)
        if compiler is None:
            self.skipTest(f"{compiler_name} is unavailable")
        command = [compiler, "-std=c++23", "-Wall", "-Wextra", "-Werror",
                   "-fsyntax-only", "-x", "c++", *extra_flags]
        for directory in ("tests/host/support", "include", "port/include",
                          "arch/arm/cortex_m4/include", "kernel"):
            command.extend(("-I", str(SOURCE / directory)))
        command.append("-")
        process = subprocess.run(command, input=probe, capture_output=True,
                                 text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)

    def test_native_api_and_tcb_layout(self):
        self.compile_api("c++")

    def test_native_feature_layouts(self):
        features = ("ARTOS_TEST_PRIORITY_AGING", "ARTOS_TEST_THREAD_STATS",
                    "ARTOS_TEST_THREAD_CANARY")
        for enabled in itertools.product((False, True), repeat=len(features)):
            flags = tuple(f"-D{name}" for name, use in zip(features, enabled) if use)
            with self.subTest(flags=flags):
                self.compile_api("c++", flags)

    def test_clang18_api_uses_expected_compatibility(self):
        self.compile_api("clang++-18")

    def test_clang18_implementation_uses_expected_compatibility(self):
        self.compile_api("clang++-18", probe="#include <event.cpp>\n",
                         extra_flags=("-I", str(SOURCE / "kernel")))

    def test_clang21_api(self):
        self.compile_api("clang++-21")

    def test_arm_api_and_tcb_layout(self):
        self.compile_api("arm-none-eabi-g++", ("-mcpu=cortex-m4", "-mthumb"))


if __name__ == "__main__":
    unittest.main()
