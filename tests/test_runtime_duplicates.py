"""
Runtime duplicate function detector for AutoIt.

Au3Check is case-SENSITIVE but AutoIt runtime is case-INSENSITIVE.
This script catches the exact class of bugs that Au3Check misses:
functions with different casing that collide at runtime.

Usage: python tests/test_runtime_duplicates.py
Exit code: number of duplicates found (0 = clean)
"""
import re
import os
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FROGGY = os.path.join(PROJECT_ROOT, "GWA Censured", "Froggy_HM_v1.6.au3")
LIB_CUSTOM = os.path.join(PROJECT_ROOT, "GWA Censured", "lib", "custom")
LIB_BOTSHUB = os.path.join(PROJECT_ROOT, "GWA Censured", "lib", "botshub")

def find_au3_files(*dirs):
    """Find all .au3 files in given directories."""
    files = []
    for d in dirs:
        if os.path.isfile(d):
            files.append(d)
        elif os.path.isdir(d):
            for f in os.listdir(d):
                if f.endswith('.au3'):
                    files.append(os.path.join(d, f))
    return files

def extract_functions(filepath):
    """Extract all function names from an .au3 file."""
    funcs = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            for lineno, line in enumerate(f, 1):
                match = re.match(r'^Func\s+(\w+)\s*\(', line)
                if match:
                    funcs.append((match.group(1), filepath, lineno))
    except FileNotFoundError:
        pass
    return funcs

def main():
    # Collect all function definitions
    all_files = find_au3_files(FROGGY, LIB_CUSTOM, LIB_BOTSHUB)
    all_funcs = []
    for f in all_files:
        all_funcs.extend(extract_functions(f))

    print(f"Scanned {len(all_files)} files, found {len(all_funcs)} function definitions")

    # Group by case-insensitive name
    by_lower = {}
    for name, filepath, lineno in all_funcs:
        key = name.lower()
        if key not in by_lower:
            by_lower[key] = []
        by_lower[key].append((name, os.path.relpath(filepath, PROJECT_ROOT), lineno))

    # Find duplicates
    duplicates = {k: v for k, v in by_lower.items() if len(v) > 1}

    if not duplicates:
        print("\nPASS: Zero case-insensitive duplicate functions")
        return 0

    print(f"\nFAIL: {len(duplicates)} duplicate function name(s) found:\n")
    for key, locations in sorted(duplicates.items()):
        print(f"  {key}():")
        for name, path, lineno in locations:
            print(f"    {path}:{lineno}  Func {name}()")
        print()

    return len(duplicates)

if __name__ == '__main__':
    sys.exit(main())
