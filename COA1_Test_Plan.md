# COA 1 Incremental Test Plan

**Objective:** Validate each cherry-picked change from latest BotsHub before moving to the next, ensuring the Froggy script remains functional throughout.

---

## Testing Toolchain

### Available Now

| Tool | Location | Purpose |
|------|----------|---------|
| **Au3Check.exe** | `C:\Program Files (x86)\AutoIt3\Au3Check.exe` | AutoIt syntax checker — catches undefined functions, wrong arg counts, undeclared variables, include chain errors |
| **AutoIt3.exe** | `C:\Program Files (x86)\AutoIt3\AutoIt3.exe` | AutoIt runtime — can execute test harness scripts headlessly |
| **Python 3.13** | `C:\Users\Robert\...\Python313-32\python.exe` | General-purpose test scripting |
| **Keystone Engine** | Python package (installed) | x86 assembler — produces ground-truth opcodes for cross-validation. `from keystone import Ks, KS_ARCH_X86, KS_MODE_32` |

### Baseline Observations

Au3Check currently reports **3 errors, 2 warnings** on the existing GWA2.au3 include chain. These are pre-existing issues (not caused by our changes):
- `DllStructSetData()` wrong arg count in Utils-Debugger.au3 (2 occurrences)
- `Out()` undefined in Utils-Debugger.au3 (defined at runtime via GUI_Functions.au3)
- `$kernel_handle` possibly used before declaration warning
- `$MEMORY_INFO_STRUCT_TEMPLATE` possibly used before declaration warning

**Important:** Our changes must not INCREASE the error/warning count. We record the baseline before each step and verify it doesn't grow.

---

## Test Levels

We use four levels of validation, applied as appropriate per change:

### Level 1: Syntax Validation (Au3Check)
```bash
"/c/Program Files (x86)/AutoIt3/Au3Check.exe" -q \
  -I "c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib" \
  -I "c:/Program Files (x86)/AutoIt3/Include" \
  "c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib/GWA2.au3"
```
**Pass criteria:** Error count does not increase from baseline. No new errors in files we modified.

### Level 2: Include Chain Validation
Run Au3Check on the top-level Froggy script to validate the full include tree:
```bash
"/c/Program Files (x86)/AutoIt3/Au3Check.exe" -q \
  -I "c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib" \
  -I "c:/Program Files (x86)/AutoIt3/Include" \
  "c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/Froggy_HM_v1.6.au3"
```
**Pass criteria:** No new errors introduced. All includes resolve.

### Level 3: ASM Opcode Cross-Validation (Python + Keystone)
For any change that touches the x86 assembler or ASM procedures, we cross-validate opcodes against Keystone (an independent, battle-tested x86 assembler):
```python
from keystone import Ks, KS_ARCH_X86, KS_MODE_32
ks = Ks(KS_ARCH_X86, KS_MODE_32)

def asm_to_hex(instruction):
    encoding, _ = ks.asm(instruction)
    return ''.join(format(b, '02X') for b in encoding)

# Example: validate that 'add esp, 0x24' encodes to '83C424'
assert asm_to_hex('add esp, 0x24') == '83C424'
```
**Pass criteria:** Every new/modified ASM instruction handler in GWA2.au3 produces bytes that match Keystone's output for the same instruction.

### Level 4: Standalone AutoIt Test Harness
For logic changes (new utility functions, pattern format validation), we write small `.au3` test scripts that:
1. Include only the file under test (plus minimal dependencies)
2. Call the function with known inputs
3. Verify outputs via `If ... Then` assertions
4. Exit with code 0 on success, non-zero on failure

```autoit
; Example test harness pattern
#include 'FileUnderTest.au3'
Local $result = FunctionUnderTest($knownInput)
If $result <> $expectedOutput Then
    ConsoleWrite("FAIL: Expected " & $expectedOutput & " got " & $result & @CRLF)
    Exit 1
EndIf
ConsoleWrite("PASS" & @CRLF)
Exit 0
```
Run via: `AutoIt3.exe /ErrorStdOut test_script.au3`
**Pass criteria:** Exit code 0, "PASS" in stdout.

---

## Step-by-Step Test Procedures

### Step 0: Record Baseline (Before Any Changes)

**Actions:**
1. Run Au3Check on GWA2.au3 — record exact error count and error messages
2. Run Au3Check on Froggy_HM_v1.6.au3 — record exact error count
3. Git commit current state so we can revert any step

**Artifacts:**
- `tests/baseline_gwa2_au3check.txt`
- `tests/baseline_froggy_au3check.txt`

---

### Step 1: Update Scan Patterns (3 byte-string changes)

**Changes:**
- TraderHook pattern: `6A5550` → `6A5650`
- CompassFlag pattern: `566A5C57` → `566A5D57`
- EnterMission pattern: `A900001000743A` → `83C902890A5D` (+ offset change)

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 1.1 | L1 | Au3Check on GWA2.au3 | Error count unchanged from baseline |
| 1.2 | L2 | Au3Check on Froggy_HM_v1.6.au3 | Error count unchanged from baseline |
| 1.3 | L4 | Pattern format validation script | All patterns are valid hex strings, even length, expected byte count |

**Test 1.3 — Pattern Validation Script (Python):**
```python
# Verify scan patterns are well-formed hex strings
patterns = {
    'TraderHook': '8D4DFC51576A5650',       # Updated
    'CompassFlag': '8D451050566A5D57',       # Updated
    # EnterMission: verify new pattern + offset
}
for name, pat in patterns.items():
    assert len(pat) % 2 == 0, f"{name}: odd-length hex string"
    bytes.fromhex(pat)  # Raises ValueError if invalid hex
    print(f"PASS: {name} = {pat} ({len(pat)//2} bytes)")
```

**Rollback trigger:** Any new Au3Check error. Pattern format validation failure.

---

### Step 2: Port New ASM Instruction Handlers

**Changes:** Add generic `add reg,imm` / `sub reg,imm` / `mov` handlers from commit ae94a11 into the `_()` function's `Select/Case` block. These replace hardcoded per-register handlers with flexible regex-based handlers that support any register.

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 2.1 | L1 | Au3Check on GWA2.au3 | Error count unchanged |
| 2.2 | L3 | Cross-validate ALL new instruction patterns against Keystone | Every instruction match |
| 2.3 | L3 | Cross-validate EXISTING instructions still work (regression) | No regressions |
| 2.4 | L4 | AutoIt ASM test harness | Assembled bytes match expected |

**Test 2.2/2.3 — Keystone Cross-Validation Script (Python):**
```python
from keystone import Ks, KS_ARCH_X86, KS_MODE_32
ks = Ks(KS_ARCH_X86, KS_MODE_32)

def asm_hex(inst):
    enc, _ = ks.asm(inst)
    return ''.join(f'{b:02X}' for b in enc)

# NEW handlers we're adding - verify expected opcodes
new_instructions = {
    # Generic add reg,imm (short form, fits in signed byte)
    'add esp, 0x24':    '83C424',
    'add eax, 0x0C':    '83C00C',
    'add ebx, 0x10':    '83C310',
    'add edx, 0x04':    '83C204',
    # Generic add reg,imm (long form)
    'add esp, 0x200':   '81C400020000',
    # Generic sub reg,imm
    'sub esp, 0x24':    '83EC24',
    'sub eax, 0x10':    '83E810',
    # mov variants
    'mov ecx, dword ptr [eax+8]':   '8B4808',
    'mov ecx, dword ptr [esi+8]':   '8B4E08',
    'mov edi, dword ptr [esi+8]':   '8B7E08',
    'mov ecx, dword ptr [ecx+8]':   '8B4908',
    # lea variants
    'lea edx, dword ptr [eax+8]':   '8D5008',
}

# EXISTING instructions (regression test)
existing_instructions = {
    'push eax':         '50',
    'push ecx':         '51',
    'push edx':         '52',
    'push ebx':         '53',
    'push 0':           '6A00',
    'push 1':           '6A01',
    'push 3':           '6A03',
    'pop eax':          '58',
    'pop ecx':          '59',
    'ret':              'C3',
    'nop':              '90',
    'call ecx':         'FFD1',
    'mov eax, dword ptr [eax]': '8B00',
    'mov ecx, dword ptr [ecx]': '8B09',
}

fail_count = 0
for inst, expected in {**new_instructions, **existing_instructions}.items():
    actual = asm_hex(inst)
    status = "PASS" if actual == expected else "FAIL"
    if status == "FAIL":
        fail_count += 1
    print(f"{status}: '{inst}' => expected {expected}, got {actual}")

exit(fail_count)
```

**Test 2.4 — AutoIt ASM Test Harness:**

We create a minimal test script that extracts the assembler from GWA2.au3 and tests it directly. This script will:
1. Initialize the assembler globals
2. Call `_()` with test instructions
3. Read `$asm_injection_string` after each call
4. Compare against expected opcodes

```autoit
; tests/test_asm_handlers.au3
#RequireAdmin
#include '../GWA Censured/lib/GWA2.au3'

Local $tests[0][3]  ; [instruction, expected_hex, test_name]
; ... populate with test cases ...

Local $failures = 0
For $i = 0 To UBound($tests) - 1
    $asm_injection_string = ''
    $asm_injection_size = 0
    $asm_code_offset = 0
    _($tests[$i][0])
    If $asm_injection_string <> $tests[$i][1] Then
        ConsoleWrite("FAIL: " & $tests[$i][2] & " - expected " & $tests[$i][1] & " got " & $asm_injection_string & @CRLF)
        $failures += 1
    Else
        ConsoleWrite("PASS: " & $tests[$i][2] & @CRLF)
    EndIf
Next

If $failures > 0 Then
    ConsoleWrite($failures & " test(s) FAILED" & @CRLF)
    Exit 1
EndIf
ConsoleWrite("All tests PASSED" & @CRLF)
Exit 0
```

Run via: `AutoIt3.exe /ErrorStdOut tests/test_asm_handlers.au3`

**NOTE:** This requires admin elevation due to `#RequireAdmin` in GWA2.au3. We can either:
- Accept the UAC prompt (will auto-succeed in an admin terminal)
- Create a stripped-down harness that copies just the `_()` function without the `#RequireAdmin`

**Recommended approach:** Create `tests/test_asm_standalone.au3` that copies ONLY the `_()` function, `ASMNumber()`, `SwapEndian()`, and the three globals — no `#RequireAdmin`, no includes, no DLL opens. This gives us a fast, clean, no-elevation-required test.

**Rollback trigger:** Any Keystone mismatch. Any Au3Check regression. Any AutoIt test harness failure.

---

### Step 3: Cherry-Pick Quest Helper Functions

**Changes:** Copy `TakeQuest()`, `TakeQuestReward()`, `TakeQuestOrReward()`, `IsQuestActive()`, `IsQuestReward()`, `IsQuestCompleted()`, `IsQuestNotFound()`, `QuestStateMatches()` from latest Utils.au3 into our Utils.au3.

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 3.1 | L1 | Au3Check on Utils.au3 | Error count unchanged |
| 3.2 | L2 | Au3Check on Froggy_HM_v1.6.au3 | No new errors |
| 3.3 | L4 | Function signature verification | All functions exist and accept expected param counts |
| 3.4 | L4 | Quest state bitmask unit tests | Pure logic validation of QuestStateMatches |

**Test 3.3 — Signature Verification (AutoIt):**

We can't call quest functions without a game, but we CAN verify they exist and are callable:
```autoit
; tests/test_quest_helpers_exist.au3
; Verify functions are declared and callable (will fail at runtime
; due to no game, but Au3Check validates signatures at parse time)
#include '../GWA Censured/lib/Utils.au3'

; Just verify the functions parse correctly - don't actually call them
; Au3Check validates arg counts and syntax
ConsoleWrite("Quest helpers included successfully" & @CRLF)
Exit 0
```

**Test 3.4 — QuestStateMatches Logic Test:**

`QuestStateMatches($questID, $expectedMask)` uses `BitAND()` on a quest state value. We can test the bitmask logic in isolation IF we can extract or mock `GetQuestByID()`. If not, we verify at the Au3Check level only and rely on the fact that this is proven upstream code.

**Rollback trigger:** Any new Au3Check error. Function not found errors.

---

### Step 4: Cherry-Pick Stuck Detection Functions

**Changes:** Copy `IsPlayerStuck()`, `TryToGetUnstuck()`, `CheckStuck()`, `CheckAndSendStuckCommand()` from latest Utils.au3.

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 4.1 | L1 | Au3Check on Utils.au3 | Error count unchanged |
| 4.2 | L2 | Au3Check on Froggy_HM_v1.6.au3 | No new errors |
| 4.3 | L4 | Dependency check: verify all functions called by stuck detection exist in our codebase | All resolved |

**Test 4.3 — Dependency Audit (automated grep):**

Before copying, we extract every function call made by the stuck detection code and verify each one exists in our codebase:
```bash
# Extract function calls from the stuck detection functions
grep -oP '\b[A-Z]\w+\(' stuck_detection_snippet.au3 | sort -u

# For each, verify it exists in our lib/
for func in GetAgentByID Move GetMyAgent DllStructGetData ...; do
    grep -r "Func $func(" "GWA Censured/lib/" || echo "MISSING: $func"
done
```

**Rollback trigger:** Missing dependency function. New Au3Check error.

---

### Step 5: Integration Smoke Test

After all four steps are complete, run a full integration check.

**Tests:**

| # | Level | Test | Pass Criteria |
|---|-------|------|---------------|
| 5.1 | L1 | Au3Check on every modified file | No new errors vs baseline |
| 5.2 | L2 | Au3Check on Froggy_HM_v1.6.au3 | No new errors vs baseline |
| 5.3 | L2 | Au3Check on LongRunMaintenance_Test.au3 | No new errors |
| 5.4 | L3 | Full Keystone regression suite | All opcodes match |
| 5.5 | L4 | AutoIt "dry load" test | Script includes all libs and exits cleanly (requires admin) |

**Test 5.5 — Dry Load Test (AutoIt):**
```autoit
; tests/test_dry_load.au3
#RequireAdmin
#include '../GWA Censured/lib/GWA2.au3'
#include '../GWA Censured/lib/Utils.au3'
#include '../GWA Censured/lib/Utils-Maintenance.au3'
#include '../GWA Censured/lib/Utils-Salvage.au3'
#include '../GWA Censured/lib/GUI_Functions.au3'

; If we get here, all includes resolved and top-level code ran
ConsoleWrite("DRY LOAD: All includes resolved successfully" & @CRLF)
ConsoleWrite("GWA2 globals initialized: $kernel_handle = " & $kernel_handle & @CRLF)
ConsoleWrite("Labels map created: " & IsMap($labels_map) & @CRLF)
Exit 0
```

This verifies that the entire include chain loads without crashing — catching missing functions, circular dependencies, or initialization failures.

---

## Test Infrastructure Setup

Before starting Step 1, we create:

```
GWA Censured X BotsHub/
  tests/
    run_all_tests.py          # Master test runner (Python)
    test_asm_standalone.au3    # Extracted assembler + opcode tests (no admin)
    test_asm_keystone.py       # Keystone cross-validation suite
    test_pattern_format.py     # Hex pattern format validator
    test_au3check.sh           # Au3Check runner for all files
    test_dry_load.au3          # Full include chain smoke test (admin)
    baseline_errors.txt        # Recorded baseline Au3Check output
```

### Master Test Runner (`run_all_tests.py`)
```python
"""
Run all COA1 tests in sequence.
Exit code = number of failures (0 = all pass).
"""
import subprocess, sys

tests = [
    ("Au3Check: GWA2.au3", [...]),
    ("Au3Check: Froggy", [...]),
    ("Keystone: ASM opcodes", ["python", "test_asm_keystone.py"]),
    ("Pattern format", ["python", "test_pattern_format.py"]),
]
# ... run each, collect results, report summary
```

---

## Decision Gates

After each step, we evaluate a GO/NO-GO:

| Gate | GO Criteria | NO-GO Action |
|------|-------------|--------------|
| After Step 1 | Au3Check clean, patterns valid | Revert scan pattern changes, investigate |
| After Step 2 | Au3Check clean, ALL opcodes match Keystone, AutoIt harness passes | Revert ASM handler changes, investigate which instruction mismatches |
| After Step 3 | Au3Check clean, no missing dependencies | Revert quest helpers, check which functions are missing from our codebase |
| After Step 4 | Au3Check clean, dependency audit clean | Revert stuck detection, add missing deps first |
| After Step 5 | All integration tests pass | Identify which step introduced the failure, revert to last good step |

---

## What We CANNOT Test (Acknowledged Gaps)

These aspects require a running Guild Wars client and cannot be validated by our toolchain:

| Gap | Risk | Mitigation |
|-----|------|------------|
| Scan patterns actually find correct addresses in current game client | High | Patterns are taken directly from latest BotsHub which is verified working by its community |
| Crafting system still works after ASM handler changes | Medium | We don't modify crafting ASM — only ADD new handlers. Existing opcodes tested for regression. |
| Game client injection succeeds | Medium | We only change pattern bytes, not the injection mechanism itself |
| Movement/combat functions behave correctly | Low | We don't modify these functions in COA 1 |
| Maintenance town navigation works | Low | We don't modify NPC coordinates or navigation in COA 1 |

---

## Git Strategy

Each step gets its own commit. If a step fails testing, we revert that single commit:

```
main ← current
  ├── commit: "COA1 Step 0: Record test baselines, add test infrastructure"
  ├── commit: "COA1 Step 1: Update scan patterns for current game client"
  ├── commit: "COA1 Step 2: Port generic ASM instruction handlers from ae94a11"
  ├── commit: "COA1 Step 3: Cherry-pick quest helper functions from latest BotsHub"
  ├── commit: "COA1 Step 4: Cherry-pick stuck detection from latest BotsHub"
  └── commit: "COA1 Step 5: Integration test verification"
```

Each commit is atomic and independently revertible without affecting the others (since the changes target different parts of the code).
