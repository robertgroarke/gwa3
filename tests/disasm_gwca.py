"""
Analyze gwca.dll to extract Scanner::Find patterns and game function addresses.
Uses pefile + raw byte scanning (no disassembler needed).
"""

import pefile
import struct
import re

DLL_PATH = r"c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"

def main():
    pe = pefile.PE(DLL_PATH)
    data = open(DLL_PATH, 'rb').read()
    image_base = pe.OPTIONAL_HEADER.ImageBase

    # Get sections
    text_sec = rdata_sec = data_sec = None
    for section in pe.sections:
        name = section.Name.rstrip(b'\x00').decode()
        va_start = image_base + section.VirtualAddress
        va_end = va_start + section.Misc_VirtualSize
        raw_start = section.PointerToRawData
        raw_end = raw_start + section.SizeOfRawData
        print(f"Section {name:8s}: VA 0x{va_start:08X}-0x{va_end:08X}  Raw 0x{raw_start:08X}-0x{raw_end:08X}")
        if name == '.text':
            text_sec = (va_start, va_end, raw_start, raw_end)
        elif name == '.rdata':
            rdata_sec = (va_start, va_end, raw_start, raw_end)
        elif name == '.data':
            data_sec = (va_start, va_end, raw_start, raw_end)

    # ========= EXPORTS =========
    print(f"\n{'='*60}")
    print("EXPORTS")
    print(f"{'='*60}")
    exports = {}
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if exp.name:
                name = exp.name.decode()
                addr = image_base + exp.address
                exports[addr] = name

    # Categorize exports
    categories = {
        'Frame/UI': [],
        'Scanner': [],
        'Other': []
    }
    for addr, name in sorted(exports.items()):
        if any(k in name for k in ['Frame', 'Button', 'Click', 'Label', 'UI', 'Root', 'Child', 'Tooltip']):
            categories['Frame/UI'].append((addr, name))
        elif any(k in name for k in ['Scan', 'Find', 'Pattern']):
            categories['Scanner'].append((addr, name))
        else:
            categories['Other'].append((addr, name))

    for cat, items in categories.items():
        if items:
            print(f"\n--- {cat} ({len(items)}) ---")
            for addr, name in items:
                print(f"  0x{addr:08X}: {name}")

    # ========= ASSERTION STRINGS =========
    print(f"\n{'='*60}")
    print("ASSERTION STRINGS (in .rdata)")
    print(f"{'='*60}")
    if rdata_sec:
        rdata_data = data[rdata_sec[2]:rdata_sec[3]]
        # Search for P:\Code\ or p:\code\
        for match in re.finditer(rb'[Pp]:\\[Cc]ode\\[^\x00]+', rdata_data):
            s = match.group().decode('ascii', errors='replace')
            va = rdata_sec[0] + match.start()
            # Highlight frame/UI related
            marker = " ***" if any(k in s.lower() for k in ['frame', 'frmsg', 'pregame', 'uiroot', 'uimsg', 'ui\\']) else ""
            print(f"  0x{va:08X}: {s}{marker}")

    # ========= SCAN PATTERNS =========
    # GWCA Scanner::Find stores byte patterns as raw byte arrays in .rdata
    # The patterns are referenced by PUSH instructions in .text
    # Look for characteristic Scanner::Find call patterns:
    #   push offset <mask_string>    ; "xxxxxxxxxx"
    #   push offset <pattern_bytes>  ; "\x55\x8B\xEC..."
    #   call Scanner::Find

    # First find "x" mask strings in .rdata (strings like "xxxxxxxxxx")
    print(f"\n{'='*60}")
    print("SCAN MASK STRINGS (xxxxx patterns in .rdata)")
    print(f"{'='*60}")
    if rdata_sec:
        mask_addresses = {}
        for match in re.finditer(rb'x{4,}[?x]*\x00', rdata_data):
            mask = match.group()[:-1].decode('ascii')  # strip null
            va = rdata_sec[0] + match.start()
            mask_addresses[va] = mask
            if len(mask) < 30:
                print(f"  0x{va:08X}: \"{mask}\" ({len(mask)} bytes)")

    # ========= FIND SCANNER::FIND CALLS =========
    # In the .text section, find sequences where mask string addresses are pushed
    # The instruction before usually pushes the pattern address
    print(f"\n{'='*60}")
    print("SCANNER::FIND CALL SITES")
    print(f"{'='*60}")
    if text_sec and rdata_sec:
        text_data = data[text_sec[2]:text_sec[3]]

        for mask_va, mask_str in sorted(mask_addresses.items()):
            # Search for PUSH <mask_va> in .text (68 <le32>)
            mask_bytes = struct.pack('<I', mask_va)
            push_mask = b'\x68' + mask_bytes

            pos = 0
            while True:
                idx = text_data.find(push_mask, pos)
                if idx == -1:
                    break
                pos = idx + 1
                code_va = text_sec[0] + idx

                # Look backwards for the pattern PUSH (should be 5 or 10 bytes before)
                # Pattern: 68 <pattern_addr> ... 68 <mask_addr>
                # Or: push <pattern_addr> followed by other pushes then push <mask_addr>
                context_start = max(0, idx - 20)
                context = text_data[context_start:idx + 20]

                # Find PUSH instructions (0x68) in the context before the mask push
                pattern_addr = None
                for back in range(5, 20, 5):  # check 5, 10, 15 bytes back
                    check_pos = idx - back
                    if check_pos >= 0 and text_data[check_pos] == 0x68:
                        candidate = struct.unpack('<I', text_data[check_pos+1:check_pos+5])[0]
                        # Check if candidate points to .rdata
                        if rdata_sec[0] <= candidate < rdata_sec[1]:
                            pattern_addr = candidate
                            break

                if pattern_addr:
                    # Read the pattern bytes from .rdata
                    pat_offset = pattern_addr - rdata_sec[0]
                    if 0 <= pat_offset < len(rdata_data):
                        # Read up to len(mask) bytes
                        pat_bytes = rdata_data[pat_offset:pat_offset + len(mask_str)]
                        pat_hex = ' '.join(f'{b:02X}' for b in pat_bytes)

                        # Look for the call instruction after the mask push (E8 within next 10 bytes)
                        call_target = None
                        for ahead in range(5, 20):
                            if idx + ahead < len(text_data) and text_data[idx + ahead] == 0xE8:
                                rel = struct.unpack('<i', text_data[idx+ahead+1:idx+ahead+5])[0]
                                call_target = text_sec[0] + idx + ahead + 5 + rel
                                break

                        # Check if there's a result offset push (push <small_number>) before the pattern push
                        offset_val = None
                        for back2 in range(1, 10):
                            bp = idx - (5 if pattern_addr else 0) - back2 * 5
                            if bp >= 0 and text_data[bp] in [0x6A]:  # push imm8
                                offset_val = struct.unpack('b', bytes([text_data[bp+1]]))[0]
                                break

                        print(f"\n  Code at 0x{code_va:08X}:")
                        print(f"    Mask:    \"{mask_str}\"")
                        print(f"    Pattern: {pat_hex}")
                        if offset_val is not None:
                            print(f"    Offset:  {offset_val} (0x{offset_val & 0xFF:02X})")
                        if call_target:
                            # Check if call_target is a known export
                            target_name = exports.get(call_target, f"0x{call_target:08X}")
                            print(f"    Call:    {target_name}")

if __name__ == '__main__':
    main()
