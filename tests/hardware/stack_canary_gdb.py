"""GDB-side canary injection and record assertions for test_stack_canary.py."""
import os
import struct
import sys

import gdb


PHASE = os.environ["ARTOS_CANARY_PHASE"]
TARGET = os.environ["ARTOS_GDB_TARGET"]
PHASE_VALUE = {"incoming": 2, "outgoing": 1}[PHASE]
BREAKPOINT = {"incoming": "artos_context_start", "outgoing": "artos_context_switch"}[PHASE]
THREAD_SYMBOL = {"incoming": "next_thread", "outgoing": "current_thread"}[PHASE]


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run():
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute(f"target extended-remote {TARGET}")
    gdb.execute("monitor reset halt")
    gdb.execute("load")
    gdb.execute("monitor reset halt")

    entry = gdb.Breakpoint(BREAKPOINT)
    gdb.execute("continue")
    check(gdb.selected_frame().name() == BREAKPOINT,
          f"expected to stop in {BREAKPOINT}, got {gdb.selected_frame().name()}")
    entry.delete()

    base = gdb.parse_and_eval(f"'artos::kernel::internal::{THREAD_SYMBOL}'")
    address = int(base)
    check(address != 0, f"{THREAD_SYMBOL} is null")
    tcb_type = gdb.lookup_type("artos::thread::internal::tcb_t")
    tcb = base.cast(tcb_type.pointer()).dereference()
    guard_address = int(tcb["canary_begin"])
    thread_id = int(tcb["id"])
    registry = gdb.parse_and_eval("'artos::kernel::internal::threads'")["_M_elems"]
    registry_size = registry.type.strip_typedefs().range()[1] + 1
    matching_slots = [index for index in range(registry_size) if int(registry[index]) == address]
    check(len(matching_slots) == 1, "thread is not in exactly one registry slot")
    expected_slot = matching_slots[0]
    check(guard_address != 0, "canary pointer is null")
    check(int(gdb.parse_and_eval("stack_overflow_record")["complete"]) == 0,
          "overflow record was not cleared on reset")
    inferior = gdb.selected_inferior()
    original = bytes(inferior.read_memory(guard_address, 8))
    check(original == struct.pack("<II", 0xA5A5A5A5, 0xA5A5A5A5),
          f"canary was already damaged: {original.hex()}")
    inferior.write_memory(guard_address, struct.pack("<I", 0xA5A5A5A4))

    trap = gdb.Breakpoint("cpu::port::fatal_trap")
    gdb.execute("continue")
    check(gdb.selected_frame().name() == "cpu::port::fatal_trap",
          f"expected fatal_trap, got {gdb.selected_frame().name()}")
    trap.delete()

    record = gdb.parse_and_eval("stack_overflow_record")
    check(int(record["complete"]) == 1, "overflow record is incomplete")
    check(int(record["reason"]) == 1, "wrong overflow reason")
    check(int(record["phase"]) == PHASE_VALUE, "wrong switch phase")
    check(int(record["tcb_address"]) == address, "wrong TCB address")
    check(int(record["thread_id"]) == thread_id, "wrong thread ID")
    check(int(record["slot"]) == expected_slot, "wrong thread slot")
    print(f"PASS {PHASE}: thread_id={thread_id:#x}, slot={int(record['slot'])}, "
          f"tcb={address:#x}, guard={guard_address:#x}")
    gdb.execute("monitor reset run")
    gdb.execute("detach")


try:
    run()
except Exception as error:
    print(f"FAIL {PHASE}: {error}", file=sys.stderr)
    gdb.execute("quit 1")
