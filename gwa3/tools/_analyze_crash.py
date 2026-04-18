"""Analyze a GW minidump: show exception info, crashing module, and
attempt to resolve the crash address against loaded modules."""

import sys
from pathlib import Path

from minidump.minidumpfile import MinidumpFile


def main(dmp_path: str):
    mf = MinidumpFile.parse(dmp_path)

    # Exception info
    exc = mf.exception
    if exc and exc.exception_records:
        rec = exc.exception_records[0]
        print(f"=== Exception ===")
        print(f"thread_id        = {exc.thread_id}")
        print(f"exception_code   = 0x{rec.ExceptionCode:08x}"
              f" ({rec.ExceptionCode_NAMED.name if rec.ExceptionCode_NAMED else '?'})")
        print(f"exception_flags  = 0x{rec.ExceptionFlags:08x}")
        print(f"exception_addr   = 0x{rec.ExceptionAddress:08x}")
        if rec.NumberParameters > 0:
            ps = list(rec.ExceptionInformation)[:rec.NumberParameters]
            print(f"params           = {[hex(p) for p in ps]}")
    else:
        print("No exception record in dump")
        return 1

    # Find which module the exception address falls in
    addr = exc.exception_records[0].ExceptionAddress
    crash_mod = None
    for m in mf.modules.modules:
        if m.baseaddress <= addr < m.baseaddress + m.size:
            crash_mod = m
            break
    if crash_mod:
        offset = addr - crash_mod.baseaddress
        print(f"\n=== Crashing module ===")
        print(f"name   = {crash_mod.name}")
        print(f"base   = 0x{crash_mod.baseaddress:08x}")
        print(f"size   = 0x{crash_mod.size:x}")
        print(f"offset = 0x{offset:x}")
    else:
        print(f"\nAddress 0x{addr:08x} not in any loaded module")

    # Loaded modules (just gwa3 + Gw for context)
    print(f"\n=== Relevant modules ===")
    for m in mf.modules.modules:
        nm = Path(m.name).name.lower()
        if "gwa3" in nm or nm == "gw.exe":
            print(f"  {Path(m.name).name:30s} "
                  f"base=0x{m.baseaddress:08x} size=0x{m.size:x}")

    # Thread info
    print(f"\n=== Threads (crash tid={exc.thread_id}) ===")
    for t in mf.threads.threads:
        marker = " <-- CRASH" if t.ThreadId == exc.thread_id else ""
        print(f"  tid={t.ThreadId} teb=0x{t.Teb:08x}{marker}")

    # Crash thread context — get EIP/ESP/registers
    crash_ctx = None
    for t in mf.threads.threads:
        if t.ThreadId == exc.thread_id and t.ThreadContext is not None:
            crash_ctx = t.ThreadContext
            break
    if crash_ctx:
        print(f"\n=== Crash thread CPU context ===")
        for attr in ("Eip", "Esp", "Ebp", "Eax", "Ebx", "Ecx", "Edx",
                     "Esi", "Edi"):
            v = getattr(crash_ctx, attr, None)
            if v is not None:
                print(f"  {attr.lower()} = 0x{v:08x}")

    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: analyze_crash.py <minidump.dmp>")
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
