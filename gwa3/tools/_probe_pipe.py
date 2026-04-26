import sys
import win32file, pywintypes

# Try several variants
names = [
    r"\\.\pipe\gwa3_llm_biscuit",          # standard
    "\\\\.\\pipe\\\\gwa3_llm_biscuit",     # literal extra backslash via escape
    r"\\.\pipe" + "\\\\gwa3_llm_biscuit",  # same via concat
    r"\\.\pipe\\gwa3_llm_biscuit",          # raw, double-slash (interpreted)
]
for name in names:
    b = name.encode("ascii")
    print("tryB:", b, "len=", len(b))
    try:
        h = win32file.CreateFile(name,
            win32file.GENERIC_READ | win32file.GENERIC_WRITE,
            0, None, win32file.OPEN_EXISTING, 0, None)
        print("  OK handle=", h)
        win32file.CloseHandle(h)
    except pywintypes.error as e:
        print("  ERR:", e)
