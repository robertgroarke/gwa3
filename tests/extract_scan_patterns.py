"""
Extract Scanner::Find patterns from gwca.dll by disassembling key functions.

Each GWCA function internally calls a game function found via Scanner::Find.
The scan patterns are stored in .rdata and referenced during GWCA initialization.
We disassemble the Init function to find all Scanner::Find calls and their patterns.
"""

import pefile
import struct

DLL_PATH = r"c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"

# Key exports we want to understand
KEY_EXPORTS = {
    0x100255E0: "ButtonClick",
    0x100259A0: "GetChildFrame",
    0x10025CC0: "GetFrameById",
    0x10025D30: "GetFrameByLabel",
    0x10025D90: "GetFrameContext",
    0x10025FE0: "GetRootFrame",
    0x100274D0: "SendFrameUIMessage",
    0x10027680: "SendUIMessage",
    0x10016660: "ButtonFrame::Click",
}

def rva_to_raw(pe, rva):
    """Convert RVA to raw file offset."""
    for section in pe.sections:
        if section.VirtualAddress <= rva < section.VirtualAddress + section.SizeOfRawData:
            return section.PointerToRawData + (rva - section.VirtualAddress)
    return None

def main():
    pe = pefile.PE(DLL_PATH)
    data = open(DLL_PATH, 'rb').read()
    image_base = pe.OPTIONAL_HEADER.ImageBase

    # Get section info
    rdata_va_start = rdata_va_end = 0
    rdata_raw_start = 0
    for section in pe.sections:
        name = section.Name.rstrip(b'\x00').decode()
        if name == '.rdata':
            rdata_va_start = image_base + section.VirtualAddress
            rdata_va_end = rdata_va_start + section.Misc_VirtualSize
            rdata_raw_start = section.PointerToRawData

    print("="*70)
    print("DISASSEMBLY OF KEY GWCA FUNCTIONS")
    print("="*70)

    for va, name in sorted(KEY_EXPORTS.items()):
        rva = va - image_base
        raw = rva_to_raw(pe, rva)
        if raw is None:
            print(f"\n--- {name} @ 0x{va:08X}: COULD NOT MAP ---")
            continue

        print(f"\n--- {name} @ 0x{va:08X} ---")

        # Read 256 bytes of the function
        func_bytes = data[raw:raw+256]

        # Simple x86 disassembly - find CALL and PUSH instructions
        i = 0
        while i < min(200, len(func_bytes)):
            addr = va + i
            b = func_bytes[i]

            if b == 0xC3:  # RET
                print(f"  {addr:08X}: C3  ret")
                break
            elif b == 0xCC:  # INT3
                print(f"  {addr:08X}: CC  int3")
                break
            elif b == 0x68:  # PUSH imm32
                if i + 4 < len(func_bytes):
                    imm = struct.unpack('<I', func_bytes[i+1:i+5])[0]
                    note = ""
                    if rdata_va_start <= imm < rdata_va_end:
                        # Read the data at this rdata address
                        rdata_offset = imm - rdata_va_start + rdata_raw_start
                        if rdata_offset < len(data):
                            # Try reading as string
                            raw_bytes = data[rdata_offset:rdata_offset+32]
                            # Check if it's a mask string (all 'x' and '?')
                            try:
                                s = raw_bytes.split(b'\x00')[0].decode('ascii')
                                if all(c in 'x?' for c in s) and len(s) > 3:
                                    note = f'  ; MASK: "{s}"'
                                elif all(32 <= c < 127 for c in raw_bytes[:min(8, len(raw_bytes.split(b'\x00')[0]))]):
                                    s = raw_bytes.split(b'\x00')[0].decode('ascii', errors='replace')
                                    if len(s) > 2:
                                        note = f'  ; STR: "{s}"'
                            except:
                                pass
                            if not note:
                                # Show as hex pattern bytes
                                pat = ' '.join(f'{b:02X}' for b in raw_bytes[:16])
                                note = f'  ; DATA: {pat}'
                    print(f"  {addr:08X}: 68 {imm:08X}  push 0x{imm:08X}{note}")
                    i += 5
                    continue
            elif b == 0xE8:  # CALL rel32
                if i + 4 < len(func_bytes):
                    rel = struct.unpack('<i', func_bytes[i+1:i+5])[0]
                    target = addr + 5 + rel
                    # Check if target is a known export
                    target_name = ""
                    for exp_va, exp_name in KEY_EXPORTS.items():
                        if target == exp_va:
                            target_name = f"  ; {exp_name}"
                            break
                    print(f"  {addr:08X}: E8 {rel:08X}  call 0x{target:08X}{target_name}")
                    i += 5
                    continue
            elif b == 0xFF:  # Various FF opcodes
                if i + 1 < len(func_bytes):
                    modrm = func_bytes[i+1]
                    if modrm == 0x15:  # CALL [imm32]
                        if i + 5 < len(func_bytes):
                            imm = struct.unpack('<I', func_bytes[i+2:i+6])[0]
                            print(f"  {addr:08X}: FF 15 {imm:08X}  call dword ptr [0x{imm:08X}]")
                            i += 6
                            continue
                    elif modrm == 0x25:  # JMP [imm32]
                        if i + 5 < len(func_bytes):
                            imm = struct.unpack('<I', func_bytes[i+2:i+6])[0]
                            print(f"  {addr:08X}: FF 25 {imm:08X}  jmp dword ptr [0x{imm:08X}]")
                            i += 6
                            continue
            elif b == 0xA1:  # MOV EAX, [imm32]
                if i + 4 < len(func_bytes):
                    imm = struct.unpack('<I', func_bytes[i+1:i+5])[0]
                    print(f"  {addr:08X}: A1 {imm:08X}  mov eax, [0x{imm:08X}]")
                    i += 5
                    continue

            # Generic: show raw bytes
            # print(f"  {addr:08X}: {b:02X}")
            i += 1

    # Now find the GWCA Init function that sets up all the Scanner::Find calls
    # It typically stores function pointers in .data section static variables
    # Look for the pattern of multiple Scanner::Find calls
    print("\n" + "="*70)
    print("SEARCHING FOR SCANNER INITIALIZATION")
    print("="*70)

    # The Scanner::Find calls would be in an Init() function
    # Look for functions that reference many .rdata addresses (scan patterns)
    # and store results in .data addresses (function pointers)

    # Find all exports that might be Init functions
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if exp.name:
                name = exp.name.decode()
                if 'Init' in name and 'UI' in name:
                    addr = image_base + exp.address
                    print(f"\nUI Init export: 0x{addr:08X}: {name}")

if __name__ == '__main__':
    main()
