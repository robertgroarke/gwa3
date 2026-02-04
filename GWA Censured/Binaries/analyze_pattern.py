#!/usr/bin/env python3
"""
Detailed analysis of MapLoading pattern in Guild Wars executables.
"""

import struct
from pathlib import Path

def analyze_pattern_context(file_path, exe_name):
    """Analyze the context around the ScanMapLoading pattern."""
    print(f"\n{'='*70}")
    print(f"DETAILED ANALYSIS: {exe_name}")
    print(f"{'='*70}")
    
    with open(file_path, 'rb') as f:
        data = f.read()
    
    # Search for 6A2C50E8 (PUSH 0x2C; PUSH EAX; CALL)
    pattern = bytes([0x6A, 0x2C, 0x50, 0xE8])
    
    matches = []
    pos = 0
    while True:
        pos = data.find(pattern, pos)
        if pos == -1:
            break
        matches.append(pos)
        pos += 1
    
    print(f"\nFound {len(matches)} matches for pattern 6A2C50E8")
    
    for i, offset in enumerate(matches):
        print(f"\n{'='*50}")
        print(f"Match #{i+1} at offset 0x{offset:08X}")
        print('='*50)
        
        # Get extended context
        start_ctx = max(0, offset - 16)
        end_ctx = min(len(data), offset + 64)
        context = data[start_ctx:end_ctx]
        
        # Get bytes relative to pattern start
        pattern_start = offset - start_ctx
        bytes_from_pattern = data[offset:offset+48]
        
        print(f"\nRAW BYTES from pattern start (48 bytes):")
        for j in range(0, len(bytes_from_pattern), 16):
            chunk = bytes_from_pattern[j:j+16]
            hex_str = ' '.join(f'{b:02X}' for b in chunk)
            print(f"  +{j:02d}: {hex_str}")
        
        print(f"\nDISASSEMBLY:")
        pos = 0
        
        # 6A 2C - PUSH 0x2C
        print(f"  +{pos:02d}: 6A 2C          PUSH 0x2C (44)")
        pos += 2
        
        # 50 - PUSH EAX
        print(f"  +{pos:02d}: 50             PUSH EAX")
        pos += 1
        
        # E8 xx xx xx xx - CALL rel32
        if len(bytes_from_pattern) > pos + 5:
            call_offset = struct.unpack('<i', bytes_from_pattern[pos+1:pos+5])[0]
            call_target = offset + pos + 5 + call_offset
            print(f"  +{pos:02d}: E8 {bytes_from_pattern[pos+1]:02X} {bytes_from_pattern[pos+2]:02X} {bytes_from_pattern[pos+3]:02X} {bytes_from_pattern[pos+4]:02X}   CALL 0x{call_target:08X}")
            pos += 5
        
        # 83 C4 08 - ADD ESP, 8
        if len(bytes_from_pattern) > pos + 3 and bytes_from_pattern[pos] == 0x83:
            print(f"  +{pos:02d}: 83 C4 08       ADD ESP, 8")
            pos += 3
        
        # Next instructions - look for the memory store
        remaining = bytes_from_pattern[pos:]
        print(f"\n  +{pos:02d}: Following bytes: {' '.join(f'{b:02X}' for b in remaining[:16])}")
        
        # Check what instruction follows
        if len(remaining) >= 1:
            opcode = remaining[0]
            
            if opcode == 0xA1:  # MOV EAX, [addr]
                if len(remaining) >= 5:
                    addr = struct.unpack('<I', remaining[1:5])[0]
                    print(f"\n  FOUND: A1 instruction (MOV EAX, [0x{addr:08X}]) at offset +{pos}")
                    print(f"  -> This is the map loading ADDRESS POINTER at offset +{pos + 1}")
                    print(f"  -> The 4-byte address starts at offset +{pos + 1}")
                    print(f"  -> To get the pointer VALUE, use offset: {pos + 1}")
                    
            elif opcode == 0xA3:  # MOV [addr], EAX
                if len(remaining) >= 5:
                    addr = struct.unpack('<I', remaining[1:5])[0]
                    print(f"\n  FOUND: A3 instruction (MOV [0x{addr:08X}], EAX) at offset +{pos}")
                    print(f"  -> Target address: 0x{addr:08X}")
                    
            elif opcode == 0xC7:  # MOV [addr], imm32
                if len(remaining) >= 6:
                    print(f"\n  FOUND: C7 instruction (MOV mem, imm) at offset +{pos}")
                    if remaining[1] == 0x05:  # MOV [addr], imm32
                        addr = struct.unpack('<I', remaining[2:6])[0]
                        imm = struct.unpack('<I', remaining[6:10])[0] if len(remaining) >= 10 else 0
                        print(f"  -> MOV [0x{addr:08X}], 0x{imm:08X}")
                    
            elif opcode == 0x85:  # TEST reg, reg
                print(f"\n  FOUND: 85 instruction (TEST) at offset +{pos}")
                # Look further for A1 or A3
                for j in range(len(remaining) - 1):
                    if remaining[j] == 0xA1 and j + 5 <= len(remaining):
                        addr = struct.unpack('<I', remaining[j+1:j+5])[0]
                        print(f"  -> Later found A1 (MOV EAX, [0x{addr:08X}]) at offset +{pos+j}")
                        break
                    elif remaining[j] == 0xA3 and j + 5 <= len(remaining):
                        addr = struct.unpack('<I', remaining[j+1:j+5])[0]
                        print(f"  -> Later found A3 (MOV [0x{addr:08X}], EAX) at offset +{pos+j}")
                        break
        
        # IMPORTANT: Check for the specific pattern sequence we need
        # Looking for the second occurrence which has C7 05 after ADD ESP,8
        if len(remaining) >= 10 and remaining[0] == 0xC7 and remaining[1] == 0x05:
            addr = struct.unpack('<I', remaining[2:6])[0]
            imm = struct.unpack('<I', remaining[6:10])[0]
            print(f"\n  *** THIS IS THE MAP LOADING RESET CODE ***")
            print(f"  -> Setting [0x{addr:08X}] = 0x{imm:08X}")
            print(f"  -> The address 0x{addr:08X} is likely the map_loading variable!")

def main():
    old_exe = Path(r"c:\Users\Robert\Documents\GWA Censured BotsHub\GWA Censured\Binaries\Gw_old.exe")
    new_exe = Path(r"c:\Users\Robert\Documents\GWA Censured BotsHub\GWA Censured\Binaries\Gw_new.exe")
    
    if old_exe.exists():
        analyze_pattern_context(old_exe, "gw_old.exe")
    
    print("\n" * 2)
    
    if new_exe.exists():
        analyze_pattern_context(new_exe, "gw_new.exe")
    
    # Summary
    print("\n" + "="*70)
    print("SUMMARY & RECOMMENDATIONS")
    print("="*70)
    print("""
Based on the analysis:

The pattern '6A2C50E8' is found in both executables.
After the CALL and ADD ESP,8, we see:
- OLD: A1 DC28EE00 (MOV EAX, [0x00EE28DC]) - reads from address
- NEW: A1 08D70301 (MOV EAX, [0x010308D7]) - reads from address

For the SECOND match, we see:
- OLD: C7 05 DC28EE00 00000000 (MOV [0x00EE28DC], 0)
- NEW: C7 05 08D70301 00000000 (MOV [0x010308D7], 0)

The offset structure:
+0:  6A 2C      - PUSH 0x2C
+2:  50         - PUSH EAX  
+3:  E8 xx xx xx xx - CALL (5 bytes)
+8:  83 C4 08   - ADD ESP, 8 (3 bytes)
+11: A1         - MOV EAX, [addr]
+12: xx xx xx xx - The 4-byte address

So the CORRECT OFFSET is: 12 (to get the 4-byte address pointer)

For GWA2.au3:
  $map_loading = MemoryRead(GetScannedAddress('ScanMapLoading', 12))

The pattern should work as-is with offset 12!
""")

if __name__ == "__main__":
    main()
