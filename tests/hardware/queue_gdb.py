"""GDB-side assertions for the queue shutdown hardware test."""
import os
import sys

import gdb


class WaitBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super().__init__("artos::kernel::locked::block_current(artos::wait_queue_t&, artos::wait_forever_t, artos::critical_section_t)")
        self.hits = 0

    def stop(self):
        self.hits += 1
        return False


def run():
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute(f"target extended-remote {os.environ['ARTOS_GDB_TARGET']}")
    gdb.execute("monitor reset halt")
    gdb.execute("load")
    gdb.execute("monitor reset halt")

    wait = WaitBreakpoint()
    done = gdb.Breakpoint("queue_hw_done")
    gdb.execute("continue")
    if gdb.selected_frame().name() != "queue_hw_done":
        raise AssertionError(f"unexpected stop: {gdb.selected_frame().name()}")
    result = int(gdb.parse_and_eval("queue_hw_result"))
    stage = int(gdb.parse_and_eval("queue_hw_stage"))
    if result != 0:
        raise AssertionError(f"firmware check {result} failed at stage {stage}")
    if stage != 6:
        raise AssertionError(f"firmware stopped early at stage {stage}")
    if wait.hits < 2:
        raise AssertionError(f"close_and_wait blocked only {wait.hits} times; expected two")
    print(f"PASS queue: reader/writer cancellation, two blocking joins, timeouts; join waits={wait.hits}")
    wait.delete()
    done.delete()
    gdb.execute("monitor reset halt")
    gdb.execute("detach")


try:
    run()
except Exception as error:
    print(f"FAIL queue: {error}", file=sys.stderr)
    gdb.execute("quit 1")
