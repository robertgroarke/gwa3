#!/usr/bin/env python3
"""
Pattern scanner for Guild Wars executables.
Searches for ScanMapLoading pattern and its variants.
"""

import re
import sys
from pathlib import Path

def hex_to_bytes(hex_string):
    """Convert hex string to bytes, supporting ?? wildcards."""
    hex_string = hex_string.replace(' ', '').upper()
    return hex_string

def create_regex_pattern(hex_pattern):
    """Create regex pattern from hex string with ?? wildcards."""
    hex_pattern = hex_pattern.replace(' ', '').upper()
    regex_parts = []
    i = 0
    while i < len(hex_pattern):
        if hex_pattern[i:i+2] == '??':
            regex_parts.append('.')
            i += 2
        else:
            byte_val = int(hex_pattern[i:i+2], 16)
            regex_parts.append(re.escape(bytes([byte_val]).decode('latin-1')))
            i += 2
    return re.compile(''.join(regex_parts).encode('latin-1'), re.DOTALL)

def search_pattern(file_path, hex_pattern, description=""):
    """Search for a hex pattern in a binary file."""
    print(f"\n{'='*60}")
    print(f"Searching for: {description}")
    print(f"Pattern: {hex_pattern}")
    print(f"File: {file_path}")
    print('='*60)
    
    with open(file_path, 'rb') as f:
        data = f.read()
    
    pattern = create_regex_pattern(hex_pattern)
    matches = []
    
    for match in pattern.finditer(data):
        offset = match.start()
        matches.append(offset)
        
        # Show context around match
        start = max(0, offset - 4)
        end = min(len(data), offset + 32)
        context = data[start:end]
        
        print(f"\nMatch at offset: 0x{offset:08X} (decimal: {offset})")
        print(f"Context bytes: {context.hex().upper()}")
        
        # Highlight the matched portion
        match_start_in_context = offset - start
        match_len = match.end() - match.start()
        hex_context = context.hex().upper()
        
        # Show disassembly hints
        matched_bytes = data[offset:offset+32]
        print(f"Following 32 bytes: {matched_bytes.hex().upper()}")
        
        # Look for A3 (MOV [addr], EAX) instruction after the call
        for i, b in enumerate(matched_bytes):
            if b == 0xA3:  # A3 = MOV [addr], EAX
                print(f"  -> Found A3 (MOV [addr], EAX) at offset +{i}")
                addr_bytes = matched_bytes[i+1:i+5]
                if len(addr_bytes) == 4:
                    addr = int.from_bytes(addr_bytes, 'little')
                    print(f"     Target address: 0x{addr:08X}")
            elif b == 0x89 and i+1 < len(matched_bytes) and matched_bytes[i+1] == 0x05:  # 89 05 = MOV [addr], EAX
                print(f"  -> Found 89 05 (MOV [addr], EAX) at offset +{i}")
                addr_bytes = matched_bytes[i+2:i+6]
                if len(addr_bytes) == 4:
                    addr = int.from_bytes(addr_bytes, 'little')
                    print(f"     Target address: 0x{addr:08X}")
    
    if not matches:
        print("No matches found.")
    else:
        print(f"\nTotal matches: {len(matches)}")
    
    return matches

def main():
    # File paths
    old_exe = Path(r"c:\Users\Robert\Documents\GWA Censured BotsHub\GWA Censured\Binaries\Gw_old.exe")
    new_exe = Path(r"c:\Users\Robert\Documents\GWA Censured BotsHub\GWA Censured\Binaries\Gw_new.exe")
    
    # Original patterns
    patterns = [
        # Original short pattern
        ("6A2C50E8", "Original short pattern (PUSH 0x2C; PUSH EAX; CALL)"),
        
        # Extended pattern with wildcards for the CALL offset and expected A3
        ("6A2C50E8????????83C408A3", "Extended pattern with A3"),
        
        # Variant: maybe they changed the order or used different register
        ("6A2C50E8????????83C408", "Pattern without A3 (maybe 89 05 follows)"),
        
        # Try with 89 05 instead of A3
        ("6A2C50E8????????83C4088905", "Pattern with 89 05 (MOV [addr], EAX)"),
        
        # Maybe they changed 0x2C to something else - check for similar patterns
        ("6A??50E8????????83C408A3", "Generic PUSH imm8; PUSH EAX; CALL; ADD ESP,8; MOV"),
    ]
    
    print("\n" + "="*70)
    print("SCANNING OLD EXECUTABLE (gw_old.exe)")
    print("="*70)
    
    if old_exe.exists():
        for pattern, desc in patterns:
            search_pattern(old_exe, pattern, desc)
    else:
        print(f"File not found: {old_exe}")
    
    print("\n" + "="*70)
    print("SCANNING NEW EXECUTABLE (gw_new.exe)")
    print("="*70)
    
    if new_exe.exists():
        for pattern, desc in patterns:
            search_pattern(new_exe, pattern, desc)
    else:
        print(f"File not found: {new_exe}")
    
    # Additional search for any PUSH 0x2C followed by CALL pattern
    print("\n" + "="*70)
    print("ADDITIONAL ANALYSIS: Looking for all PUSH 0x2C; PUSH reg; CALL patterns")
    print("="*70)
    
    broader_patterns = [
        ("6A2C5?E8", "PUSH 0x2C; PUSH <any reg>; CALL"),
    ]
    
    if new_exe.exists():
        for pattern, desc in broader_patterns:
            search_pattern(new_exe, pattern, desc)

if __name__ == "__main__":
    main()
