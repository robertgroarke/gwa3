# GWCA & UIMessage Research — Character Select Programmatic Control

## Goal
Press the Play button and dismiss the reconnect dialog at the character select screen **programmatically** — no keyboard, no mouse, no window focus dependency.

---

## Architecture: GW's UI Systems

### Global UIMessage (SendUIMessage)
- **Function**: `SendUIMessage(UIMessage msgid, void* wParam, void* lParam)`
- **Scanned via**: Pattern `B900000000E8000000005DC3894508` offset -0x14
- **Label**: `UIMessage` in our framework
- **Used for**: In-game commands (map travel, item use, dialog, skill load, etc.)
- **Dispatches to**: All registered handlers
- **Does NOT route**: Frame-level messages (0x22 kMouseClick, 0x31 kMouseClick2, 0x2F kMouseAction) to specific frames

### Frame UIMessage (SendFrameUIMessage)
- **Function**: `SendFrameUIMessage(Frame* frame_callbacks, UIMessage msgid, void* wParam, void* lParam)`
- **Calling convention**: `__thiscall` — ECX = `frame_ptr + 0xA8` (callbacks array)
- **Stack args**: msgid, wParam, lParam (callee cleans — stdcall-like)
- **Game address**: `0x007986D0` (confirmed by BOTH our scan AND gwca.dll)
- **Scan pattern**: `83 C1 DC E8` (4 bytes, unique — only 1 match in .text)
  - At offset +3, there's an E8 (CALL) instruction
  - Resolve via `FunctionFromNearCall(match_addr + 3)` to get target
- **Used for**: Clicking buttons, frame-specific UI events

### Frame System
- **Frame struct**: 0x1C8 bytes (456 bytes), defined in GWCA UIMgr.h
- **Frame array**: Scanned via assertion `P:\Code\Engine\Frame\FrMsg.cpp` + `frame`
- **Frame lookup**: By `frame_hash_id` at offset `0x134` (in FrameRelation at Frame+0x128)
- **Key offsets in Frame**:
  - `0xA8`: `frame_callbacks` (GW::Array, buffer/size/capacity)
  - `0xB8`: `child_offset_id`
  - `0xBC`: `frame_id` (index in global frame array)
  - `0x128`: `FrameRelation` (parent, siblings, hash)
  - `0x134`: `frame_hash_id` (used by GetFrameByHash)
  - `0x18C`: `frame_state` (bit 0x4=created, 0x200=hidden, 0x10=disabled)

---

## Known Frame Hashes (from py4gw frame_aliases.json)

### Character Select Screen
| Hash | Frame |
|---|---|
| **184818986** | `Character_Select_Frame.Play_Button` |
| 41327607 | `Character_Select_Frame.Play_Button_Greyed_Out` |
| **1398610279** | `did_not_cleaning_disconnect_popup_charselect_YES_BUTTON` |
| **3600335809** | `did_not_cleaning_disconnect_popup_charselect_NO_BUTTON` |
| 3372446797 | `Character_Select_Frame.Create_Button` |
| 3379687503 | `Character_Select_Frame.Delete_Button` |
| 1117342925 | `Character_Select_Frame.Log_Out_Button` |
| 1601494406 | `Character_Select_Frame.Edit_Account_Button` |
| 828467986 | `Character_Select_Frame.Character_Frame` |
| 941138463 | `Character_Select_Frame.Status_dropdown` |

### In-Game
| Hash | Frame |
|---|---|
| 3332025202 | Party Formation |
| 2874675009 | Inventory Window |
| 2315448754 | Xunlai Window |
| 1532320307 | Merchant Buy Button |
| 684387150 | Salvage Window |

Full list: `tests/frame_aliases.json` (1164 entries from py4gw)

---

## GWCA DLL Analysis

### Key Exports (from gwca.dll disassembly)
| RVA Offset | Export | Notes |
|---|---|---|
| +0x16660 | `ButtonFrame::Click()` | Calls MouseAction(0x6) then MouseAction(0x7) |
| +0x173D0 | `ButtonFrame::MouseAction(ActionState)` | Builds kMouseAction struct, calls SendFrameUIMessage |
| +0x255E0 | `ButtonClick(Frame*)` | Wrapper: GetFrameById + Click |
| +0x259A0 | `GetChildFrame(Frame*, uint32)` | Navigate frame hierarchy |
| +0x25CC0 | `GetFrameById(uint32)` | Lookup in global frame array |
| +0x25D30 | `GetFrameByLabel(wchar_t*)` | Hash label, search frame array |
| +0x25D90 | `GetFrameContext(Frame*)` | Returns frame context pointer |
| +0x25FE0 | `GetRootFrame()` | Returns root frame pointer |
| +0x274D0 | `SendFrameUIMessage(Frame*, msg, wp, lp)` | GWCA wrapper (hooks, then calls game func) |
| +0x27680 | `SendUIMessage(msg, wp, lp)` | GWCA wrapper for global dispatch |

### Data Section Addresses (relative to gwca.dll base)
| Offset | Content | Value (example) |
|---|---|---|
| +0x8A39C | Original SendFrameUIMsg game func ptr | 0x007986D0 |
| +0x8A3A0 | Hooked SendFrameUIMsg (GWCA's wrapper) | (injection region) |
| +0x8A37C | GetChildFrame game func ptr | 0x0077E2B0 |
| +0x8A410 | RootFrame game func ptr | 0x0079DC20 |
| +0x880F0 | Hash seed (ESI initial) | 0x325D1EAE |
| +0x880F4 | Hash seed (EAX initial) | 0xE2C15C9D |
| +0x880F8 | Hash seed 2 | 0x2170A28A |
| +0x8A3B0 | Frame hash table address | 0x00D25624 |

### GWCA DLL Injection
- gwca.dll loads successfully via `CreateRemoteThread(LoadLibraryW)`
- `Scanner::Initialize(gw_module_handle)` works on non-game thread
- `GW::Initialize()` works but sets up hooks (may conflict with BotsHub hooks)
- Calling `ButtonFrame::Click()` via `CreateRemoteThread` crashes (must run on game thread)

---

## Hash Function (GetFrameByLabel internals)

Located at gwca.dll +0x1A450. Algorithm:
```
hash0 = seed_eax  (from +0x880F4)
hash1 = seed_esi  (from +0x880F0)
result = 0

for each wchar ch in label:
    ch = toupper(ch)    // case-insensitive
    hash0 = (hash0 << 3) ^ ch
    hash1 += table[hash0 & 0xF]   // 4-byte entries at +0x52938
    result ^= (hash0 + hash1)

return result
```

---

## Root Cause: Command Queue Doesn't Execute at Char Select

### The Problem
The BotsHub framework's `MainProc` hook has TWO paths:
- **HandleCase** (line 1535): Reads command, zeros it, increments counter, but **DOES NOT EXECUTE** (`jmp MainExit`)
- **RegularFlow** (line 1553): Reads command, zeros it, **and EXECUTES** (`jmp ebx`)

`HandleCase` is taken when `[BasePointer]->...->field_198 == 0`, which happens at the character select screen (no active map loaded). This means **ALL command queue operations are silently discarded at char select**.

### The Fix
Patch `HandleCase`'s `jmp MainExit` to `jmp RegularFlow` — making it execute commands regardless of game state. This is a 1-byte change in the injected code.

Or: patch the `je HandleCase` conditional jump to `je RegularFlow`.

### Assembler .5 Offset Issue
The BotsHub assembler accumulates a fractional `.5` byte in `$asm_injection_size` from upstream code. This causes ALL extension code labels to be ~4 bytes off from their actual memory position. Workaround: scan memory for instruction signatures to find the real address (calibration).

---

## Current Implementation Status

### Working
- `GetFrameByHash($hash)` — walks frame array, finds frames by hash at offset 0x134
- `IsFrameVisible($hash)` / `IsAtCharSelect()` / `IsReconnectDialogShowing()` — state detection
- `ExtendScanner_FrameUI()` — scans game binary for SendFrameUIMsg function
- Shellcode execution via VirtualAllocEx + command queue (when HandleCase is patched)
- GWCA DLL injection + initialization

### Not Working Yet
- `ClickFrameButton($hash)` — shellcode never executes because HandleCase discards it
- Need to patch HandleCase before any char select commands work

### Execution Pipeline — SOLVED (2026-03-28)
Commands now execute at char select via the rendering hook:
- **MainProc hook**: NOT called at char select (game state checks fail)
- **RenderingModProc hook**: IS called at char select (rendering active)
- Added queue processing to RenderingModProc: `call ebx` with shellcode ending in RET
- HandleCase in MainProc also patched to execute commands (was discarding them)

### Current Blocker
`SendFrameUIMsg(0x007986D0)` is called with:
- ECX = `frame_ptr + 0xA8` (callbacks array as __thiscall this pointer)
- Stack: `msgid (0x31 kMouseClick2), wParam (&kMouseAction), lParam (0)`
- Function returns without error but button doesn't respond
- The wParam struct format may be wrong — GWCA's MouseAction builds a complex struct
- Or the function needs specific frame context that we're not providing

### Additional Findings (2026-03-28 continued)

**RenderingMod hook NOT called at char select** — despite JMP being installed.
Queue commands written to the rendering hook are never consumed. Neither MainProc
nor RenderingMod hooks fire at char select.

**CreateRemoteThread calls work but function does nothing** — tested 5 different
ECX/msgid combinations via CreateRemoteThread. All return silently, no crash,
no click. The function likely requires game-thread-specific context (TLS, render
state, event loop context) that CreateRemoteThread cannot provide.

**Callback entries are valid** — Play button has 4 callback entries, all with
flag = -2147483648 (0x80000000, negative = active). The dispatch code should
call them. But even calling the callback directly via CreateRemoteThread has no effect.

### Current Approach: Need Game Thread Execution
The fundamental blocker is executing code on the GAME THREAD at char select:
- MainProc hook: NOT called at char select
- RenderingMod hook: NOT called at char select
- CreateRemoteThread: runs but game functions ignore non-game-thread calls
- Engine hook: exists but not hooked by default, might fire at char select

### Next Steps
1. **Hook the Engine function** (scan pattern `568B3085F67478...`) — may fire at char select
2. **Use GWCA's GameThread::Enqueue** after injecting gwca.dll + GW::Initialize
3. **Find PreGame-specific hook points** — the char select has its own event loop
4. **Patch the game's own rendering callback** to check our queue
5. If any execution path works: use SendFrameUIMsg(frame+0xA8, 0x31, &action, 0)

---

## Key Files
- `lib/custom/GWA2_FrameUI.au3` — Frame UI system implementation
- `lib/custom/GWA2_Assembly_UISniffer.au3` — UIMessage sniffer (for discovery)
- `tests/frame_aliases.json` — 1164 known frame hashes from py4gw
- `tests/disasm_gwca.py` — gwca.dll disassembly scripts
- `tests/extract_scan_patterns.py` — scan pattern extractor
- `toolbox/GWToolboxpp-master/Dependencies/GWCA/` — GWCA headers + compiled DLL

## External Resources
- py4gw: `github.com/apoguita/Py4GW` — Python GW bot using GWCA (reference for frame hashes)
- py4gw C++: `github.com/apoguita/Py4GW_cpp_files` — C++ bridge showing GWCA API usage
- GWCA (old): `github.com/GregLando113/GWCA` — older version with UIMgr.cpp source
- GWToolboxpp: `github.com/gwdevhub/GWToolboxpp` — uses latest GWCA (binary-only)
