"""
Trace SendFrameUIMessage and GetFrameByLabel initialization.
Find the scan patterns used to locate these game functions.
"""

import pefile
import struct

DLL_PATH = r"c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"

def main():
    pe = pefile.PE(DLL_PATH)
    data = open(DLL_PATH, 'rb').read()
    image_base = pe.OPTIONAL_HEADER.ImageBase

    text_sec = rdata_sec = data_sec = None
    for section in pe.sections:
        name = section.Name.rstrip(b'\x00').decode()
        if name == '.text': text_sec = section
        elif name == '.rdata': rdata_sec = section
        elif name == '.data': data_sec = section

    text_data = data[text_sec.PointerToRawData:text_sec.PointerToRawData + text_sec.SizeOfRawData]
    text_va = image_base + text_sec.VirtualAddress
    rdata_data = data[rdata_sec.PointerToRawData:rdata_sec.PointerToRawData + rdata_sec.SizeOfRawData]
    rdata_va = image_base + rdata_sec.VirtualAddress
    data_va = image_base + data_sec.VirtualAddress
    data_va_end = data_va + data_sec.Misc_VirtualSize

    # Find ALL stores to .data section (A3 <data_addr> pattern)
    # and extract the Scanner::Find patterns before each store
    print("="*70)
    print("ALL FUNCTION POINTER STORES WITH PATTERNS")
    print("="*70)

    results = []
    i = 0
    while i < len(text_data) - 5:
        if text_data[i] == 0xA3:
            store_addr = struct.unpack('<I', text_data[i+1:i+5])[0]
            if data_va <= store_addr < data_va_end:
                code_va = text_va + i

                # Look backwards for push+push+call pattern (Scanner::Find)
                # The call to Scanner::Find is typically 5-15 bytes before A3
                # Pattern: push <pattern_rdata> / push <mask_rdata> / call Scanner / A3 <store>
                # Or with offset: push <offset> / push <mask> / push <pattern> / call Scanner / A3
                # Or with assertion: push <assert_msg> / push <assert_file> / call FindAssertion / A3

                best_info = None

                for call_back in range(5, 40):
                    ci = i - call_back
                    if ci < 0: break
                    if text_data[ci] != 0xE8: continue

                    # Found a CALL before the store
                    rel = struct.unpack('<i', text_data[ci+1:ci+5])[0]
                    call_target = text_va + ci + 5 + rel

                    # Look for pushes before this call
                    pushes = []
                    j = ci - 1
                    while j >= max(0, ci - 30):
                        if text_data[j] == 0x68 and j + 4 < ci:  # push imm32
                            imm = struct.unpack('<I', text_data[j+1:j+5])[0]
                            pushes.insert(0, ('imm32', imm))
                            j -= 5
                        elif text_data[j] == 0x6A:  # push imm8
                            imm = struct.unpack('b', bytes([text_data[j+1]]))[0]
                            pushes.insert(0, ('imm8', imm))
                            j -= 2
                        else:
                            break

                    if len(pushes) >= 2:
                        # Resolve rdata references
                        info = {
                            'store': store_addr,
                            'code': code_va,
                            'call_target': call_target,
                            'pushes': [],
                        }
                        for ptype, pval in pushes:
                            if ptype == 'imm32' and rdata_va <= pval < rdata_va + len(rdata_data):
                                ro = pval - rdata_va
                                raw_b = rdata_data[ro:ro+48]
                                s = raw_b.split(b'\x00')[0]
                                try:
                                    decoded = s.decode('ascii')
                                    if all(c in 'x?' for c in decoded) and len(decoded) > 2:
                                        info['pushes'].append(('mask', decoded, pval))
                                    elif decoded.startswith(('p:\\', 'P:\\', 'C:\\')):
                                        info['pushes'].append(('file', decoded, pval))
                                    elif len(decoded) > 2 and all(32 <= c < 127 for c in s):
                                        info['pushes'].append(('str', decoded, pval))
                                    else:
                                        pat = ' '.join(f'{b:02X}' for b in s)
                                        info['pushes'].append(('pattern', pat, pval))
                                except:
                                    pat = ' '.join(f'{b:02X}' for b in s)
                                    info['pushes'].append(('pattern', pat, pval))
                            elif ptype == 'imm32':
                                info['pushes'].append(('addr', f'0x{pval:08X}', pval))
                            elif ptype == 'imm8':
                                info['pushes'].append(('offset', str(pval), pval))

                        best_info = info
                        break

                if best_info and best_info['pushes']:
                    results.append(best_info)

            i += 5
        else:
            i += 1

    # Print results grouped by interesting ones
    for r in results:
        has_pattern = any(t == 'pattern' or t == 'mask' for t, _, _ in r['pushes'])
        has_file = any(t == 'file' for t, _, _ in r['pushes'])

        if has_pattern or has_file:
            print(f"\n  Store 0x{r['store']:08X} @ code 0x{r['code']:08X} -> call 0x{r['call_target']:08X}")
            for ptype, pval, raw in r['pushes']:
                if ptype == 'mask':
                    print(f"    Mask:    \"{pval}\"")
                elif ptype == 'pattern':
                    print(f"    Pattern: {pval}")
                elif ptype == 'file':
                    print(f"    File:    \"{pval}\"")
                elif ptype == 'str':
                    print(f"    Assert:  \"{pval}\"")
                elif ptype == 'offset':
                    print(f"    Offset:  {pval}")
                elif ptype == 'addr':
                    print(f"    Addr:    {pval}")

    print(f"\n\nTotal: {len(results)} stores found")

if __name__ == '__main__':
    main()
