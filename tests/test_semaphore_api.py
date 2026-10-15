"""Compile the public semaphore interface without linking kernel code."""

from pathlib import Path
import shutil
import subprocess
import unittest

SOURCE = Path(__file__).resolve().parents[1]


class SemaphoreApiTests(unittest.TestCase):
    def test_release_and_lifecycle_apis_compile(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("a C++ compiler is required")
        probe = """#include <semaphore.h>
void exercise(artos::semaphore_t& semaphore) {
    bool woken = false;
    (void)semaphore.release();
    (void)semaphore.release_from_isr(&woken);
    (void)semaphore.release_from_isr(nullptr);
    (void)semaphore.release_from_isr();
    semaphore.close();
    semaphore.close_and_wait();
}
void destroy(artos::semaphore_t* semaphore) {
    semaphore->close_and_delete();
}
"""
        result = subprocess.run(
            [compiler, "-std=c++23", "-fsyntax-only", "-x", "c++",
             "-I", str(SOURCE / "tests/host/support"),
             "-I", str(SOURCE / "include"),
             "-I", str(SOURCE / "port/include"),
             "-I", str(SOURCE / "arch/arm/cortex_m4/include"), "-"],
            input=probe, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
