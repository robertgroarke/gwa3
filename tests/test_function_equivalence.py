#!/usr/bin/env python3
r"""
COA3 Phase 1 - Function Extraction Equivalence Validator

Validates that functions extracted from GWA2.au3 into custom/GWA2_Extensions.au3
are character-for-character identical to their originals.

Usage:
    python test_function_equivalence.py <original_file> <new_file>

Example (self-test):
    python test_function_equivalence.py "GWA Censured/lib/GWA2.au3" "GWA Censured/lib/GWA2.au3"
"""

import re
import sys

# The 20 functions targeted for Phase 1 extraction.
# NOTE: RegisterNameTo32Code, RegisterNameTo16Code, RegisterNameTo8Code are
# listed per the spec but do not yet exist in GWA2.au3.  The script handles
# missing functions gracefully (reports them as NOT FOUND rather than FAIL).
TARGET_FUNCTIONS = [
    "IsPlayerDead",
    "IsHeroDead",
    "GetCastTimeModifier",
    "UseSkillEx",
    "UseSkillTimed",
    "UseHeroSkillEx",
    "UseHeroSkillTimed",
    "GetIsMerchantOpen",
    "GetItemIDFromModelID",
    "GetMerchantItemPtrByModelId",
    "ClearAttributes",
    "Disconnected",
    "_GWA2_GetAlmostInRangeOfAgent",
    "_GWA2_GetInventoryItemPtrByModelId",
    "_GWA2_CountItemInBagsByModelID",
    "Extend_Write",
    "Extend_AssemblerWriteDetour",
    "RegisterNameTo32Code",
    "RegisterNameTo16Code",
    "RegisterNameTo8Code",
]


def read_file_normalized(path: str) -> str:
    """Read a file and normalize line endings to LF only."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()
    return content.replace("\r\n", "\n").replace("\r", "\n")


def extract_function(source: str, func_name: str) -> str | None:
    """
    Extract a single AutoIt function body from source text.

    Matches from  Func <name>(  through the next  EndFunc  (on its own line).
    Returns the full matched text including Func/EndFunc lines, or None if
    the function is not found.
    """
    # Escape the function name for regex safety (handles underscores, etc.)
    escaped = re.escape(func_name)
    # Pattern: start-of-line (optional whitespace) Func <name>( ... through EndFunc
    # Using DOTALL so . matches newlines within the body.
    pattern = rf"^[ \t]*Func\s+{escaped}\s*\(.*?^[ \t]*EndFunc"
    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if match:
        return match.group(0)
    return None


def extract_all(source: str, func_names: list[str]) -> dict[str, str | None]:
    """Extract all target functions from a source string."""
    results = {}
    for name in func_names:
        results[name] = extract_function(source, name)
    return results


def compare_functions(
    original_bodies: dict[str, str | None],
    new_bodies: dict[str, str | None],
) -> int:
    """
    Compare extracted function bodies between original and new files.
    Returns the number of failures.
    """
    failures = 0
    not_found_original = 0
    not_found_new = 0

    print("=" * 72)
    print("COA3 Phase 1 - Function Extraction Equivalence Report")
    print("=" * 72)
    print()

    for name in TARGET_FUNCTIONS:
        orig = original_bodies.get(name)
        new = new_bodies.get(name)

        if orig is None and new is None:
            print(f"  [ NOT FOUND ] {name} -- absent from both files (expected for stubs)")
            not_found_original += 1
            continue

        if orig is None:
            print(f"  [   FAIL    ] {name} -- missing from ORIGINAL file")
            failures += 1
            continue

        if new is None:
            print(f"  [   FAIL    ] {name} -- missing from NEW file")
            failures += 1
            not_found_new += 1
            continue

        orig_lines = orig.split("\n")
        new_lines = new.split("\n")

        if orig == new:
            print(f"  [   PASS    ] {name} ({len(orig_lines)} lines)")
        else:
            failures += 1
            # Find first differing line for diagnostics
            diff_line = None
            for i, (ol, nl) in enumerate(zip(orig_lines, new_lines), start=1):
                if ol != nl:
                    diff_line = i
                    break
            if diff_line is None:
                diff_line = min(len(orig_lines), len(new_lines)) + 1

            print(f"  [   FAIL    ] {name} (orig={len(orig_lines)} lines, new={len(new_lines)} lines, first diff at line {diff_line})")

    print()
    print("-" * 72)

    found_count = sum(
        1 for name in TARGET_FUNCTIONS if original_bodies.get(name) is not None
    )
    print(f"Functions found in original: {found_count}/{len(TARGET_FUNCTIONS)}")
    print(f"Functions found in new file: {found_count - not_found_new}/{found_count}")
    print(f"Passed:  {found_count - failures - not_found_original}")
    print(f"Failed:  {failures}")

    if not_found_original > 0:
        missing = [n for n in TARGET_FUNCTIONS if original_bodies.get(n) is None]
        print(f"Not found (OK - stubs): {', '.join(missing)}")

    print("-" * 72)
    return failures


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <original_file> <new_file>")
        sys.exit(1)

    original_path = sys.argv[1]
    new_path = sys.argv[2]

    print(f"Original: {original_path}")
    print(f"New file: {new_path}")
    print()

    original_source = read_file_normalized(original_path)
    new_source = read_file_normalized(new_path)

    original_bodies = extract_all(original_source, TARGET_FUNCTIONS)
    new_bodies = extract_all(new_source, TARGET_FUNCTIONS)

    failures = compare_functions(original_bodies, new_bodies)
    sys.exit(failures)


if __name__ == "__main__":
    main()
