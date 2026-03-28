"""
Find the GWCA Init function that sets up Scanner::Find patterns.
Search for code that writes to known .data addresses (function pointers).
"""

import pefile
import struct

DLL_PATH = r"c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"

# Known .data addresses used by key functions
DATA_ADDRS = {
    0x1008A37C: "GetChildFrame func ptr",
    0x1008A3A0: "SendFrameUIMessage func ptr",
    0x1008A410: "RootFrame ptr",
}

def main():
    pe = pefile.PE(DLL_PATH)
    data = open(DLL_PATH, 'rb').read()
    image_base = pe.OPTIONAL_HEADER.ImageBase

    text_sec = rdata_sec = None
    for section in pe.sections:
        name = section.Name.rstrip(b'\x00').decode()
        if name == '.text':
            text_sec = section
        elif name == '.rdata':
            rdata_sec = section

    text_data = data[text_sec.PointerToRawData:text_sec.PointerToRawData + text_sec.SizeOfRawData]
    text_va = image_base + text_sec.VirtualAddress

    rdata_va_start = image_base + rdata_sec.VirtualAddress if rdata_sec else 0
    rdata_va_end = rdata_va_start + rdata_sec.Misc_VirtualSize if rdata_sec else 0
    rdata_raw_start = rdata_sec.PointerToRawData if rdata_sec else 0
    rdata_data = data[rdata_raw_start:rdata_raw_start + rdata_sec.SizeOfRawData] if rdata_sec else b''

    # Search .text for references to each .data address
    for target_addr, desc in sorted(DATA_ADDRS.items()):
        target_bytes = struct.pack('<I', target_addr)
        print(f"\n=== References to 0x{target_addr:08X} ({desc}) ===")

        pos = 0
        while True:
            idx = text_data.find(target_bytes, pos)
            if idx == -1:
                break
            pos = idx + 1
            code_va = text_va + idx

            # Read context: 8 bytes before and 16 after
            ctx_start = max(0, idx - 8)
            ctx_end = min(len(text_data), idx + 20)
            ctx = text_data[ctx_start:ctx_end]

            # Check what instruction this is part of
            # Common patterns:
            # A3 <addr> = MOV [addr], EAX (store scan result)
            # A1 <addr> = MOV EAX, [addr] (load func ptr)
            # 89 xx <addr> = MOV [addr], reg
            # C7 05 <addr> <imm> = MOV [addr], imm32

            prefix_byte = text_data[idx - 1] if idx > 0 else 0
            is_store = prefix_byte == 0xA3  # MOV [imm32], EAX
            is_load = prefix_byte == 0xA1  # MOV EAX, [imm32]

            ctx_hex = ' '.join(f'{b:02X}' for b in ctx)
            store_mark = " <-- STORE (Scanner result)" if is_store else (" <-- LOAD" if is_load else "")

            print(f"  0x{code_va:08X}: {ctx_hex}{store_mark}")

            # If it's a store, look backwards for the Scanner::Find call pattern
            if is_store:
                # The typical pattern before A3:
                # push <offset>    ; result offset
                # push <mask_addr> ; mask string
                # push <pat_addr>  ; pattern bytes
                # call Scanner::Find
                # A3 <data_addr>   ; store result

                # Look at up to 40 bytes before for PUSH instructions
                scan_start = max(0, idx - 40)
                scan_ctx = text_data[scan_start:idx]

                pushes = []
                i = len(scan_ctx) - 2  # Start just before the A3
                while i >= 0:
                    b = scan_ctx[i]
                    if b == 0x68 and i + 4 < len(scan_ctx):  # PUSH imm32
                        imm = struct.unpack('<I', scan_ctx[i+1:i+5])[0]
                        pushes.insert(0, ('push32', imm, scan_start + i))
                    elif b == 0x6A and i + 1 < len(scan_ctx):  # PUSH imm8
                        imm = struct.unpack('b', bytes([scan_ctx[i+1]]))[0]
                        pushes.insert(0, ('push8', imm, scan_start + i))
                    elif b == 0xE8:  # CALL rel32
                        if i + 4 < len(scan_ctx):
                            rel = struct.unpack('<i', scan_ctx[i+1:i+5])[0]
                            call_target = text_va + scan_start + i + 5 + rel
                            pushes.insert(0, ('call', call_target, scan_start + i))
                    i -= 1

                # Print the push/call sequence
                for ptype, pval, poff in pushes:
                    pva = text_va + poff
                    if ptype == 'push32':
                        note = ""
                        if rdata_va_start <= pval < rdata_va_end:
                            ro = pval - rdata_va_start
                            raw_b = rdata_data[ro:ro+32]
                            try:
                                s = raw_b.split(b'\x00')[0]
                                if all(c in b'x?' for c in s) and len(s) > 3:
                                    note = f'  MASK: "{s.decode()}"'
                                elif all(32 <= c < 127 for c in s[:8]) and len(s) > 2:
                                    note = f'  STR: "{s.decode()}"'
                                else:
                                    pat = ' '.join(f'{b:02X}' for b in raw_b[:len(s)])
                                    note = f'  PATTERN: {pat}'
                            except:
                                pat = ' '.join(f'{b:02X}' for b in raw_b[:16])
                                note = f'  BYTES: {pat}'
                        print(f"    {pva:08X}: push 0x{pval:08X}{note}")
                    elif ptype == 'push8':
                        print(f"    {pva:08X}: push {pval}  (0x{pval & 0xFF:02X}) ; offset")
                    elif ptype == 'call':
                        print(f"    {pva:08X}: call 0x{pval:08X}")

    # Also find ALL Scanner::Find-like patterns by looking for
    # sequences of: push rdata / push rdata / call / A3 data
    print("\n" + "="*70)
    print("ALL SCANNER::FIND + STORE PATTERNS")
    print("="*70)

    data_va_start = image_base + pe.sections[2].VirtualAddress  # .data
    data_va_end = data_va_start + pe.sections[2].Misc_VirtualSize

    # Find all A3 (MOV [imm32], EAX) instructions where target is in .data
    i = 0
    found_patterns = []
    while i < len(text_data) - 5:
        if text_data[i] == 0xA3:
            store_addr = struct.unpack('<I', text_data[i+1:i+5])[0]
            if data_va_start <= store_addr < data_va_end:
                # Found a store to .data. Look backwards for Scanner::Find pattern
                # Check if there's a CALL just before (E8 within 5-10 bytes back)
                for back in range(5, 15):
                    check = i - back
                    if check >= 0 and text_data[check] == 0xE8:
                        # Found call before store. Now look for push rdata before call
                        has_mask = False
                        has_pattern = False
                        mask_str = ""
                        pattern_hex = ""
                        pattern_len = 0

                        # Scan backwards from call for pushes
                        for pb in range(1, 30):
                            pc = check - pb
                            if pc >= 0 and text_data[pc] == 0x68:
                                imm = struct.unpack('<I', text_data[pc+1:pc+5])[0]
                                if rdata_va_start <= imm < rdata_va_end:
                                    ro = imm - rdata_va_start
                                    raw_b = rdata_data[ro:ro+40]
                                    s = raw_b.split(b'\x00')[0]
                                    if all(c in b'x?' for c in s) and len(s) > 3:
                                        has_mask = True
                                        mask_str = s.decode()
                                    elif not has_pattern and len(s) > 3:
                                        has_pattern = True
                                        pattern_hex = ' '.join(f'{b:02X}' for b in s)
                                        pattern_len = len(s)

                        if has_mask and has_pattern:
                            code_va = text_va + i
                            # Check for push imm8 (offset) before the pattern push
                            offset = 0
                            for pb in range(1, 35):
                                pc = check - pb
                                if pc >= 0 and text_data[pc] == 0x6A:
                                    offset = struct.unpack('b', bytes([text_data[pc+1]]))[0]
                                    break

                            found_patterns.append({
                                'store_addr': store_addr,
                                'code_va': code_va,
                                'mask': mask_str,
                                'pattern': pattern_hex,
                                'pattern_len': pattern_len,
                                'offset': offset,
                            })
                        break
            i += 5
        else:
            i += 1

    for p in found_patterns:
        print(f"\n  Store: 0x{p['store_addr']:08X}  Code: 0x{p['code_va']:08X}")
        print(f"  Mask:    \"{p['mask']}\"")
        print(f"  Pattern: {p['pattern']}")
        print(f"  Offset:  {p['offset']}")

if __name__ == '__main__':
    main()
