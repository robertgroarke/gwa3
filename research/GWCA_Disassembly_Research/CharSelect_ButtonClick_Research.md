# CharSelect ButtonClick Research

## Overview

**Goal:** Programmatically press the Play button and handle the reconnect dialog at the Guild Wars character select screen.

**Constraint:** No mouse clicks, no keyboard input — fully autonomous bot operation. The bot must work headless (minimized, no focus) so multiple GW instances can run simultaneously without interfering with each other or the user's desktop.

This research covers the internal UI architecture of Guild Wars at the character select screen, the multiple approaches attempted to send synthetic button clicks, and the final working solution.

---

## Architecture: How GW's UI Works at Char Select

Guild Wars uses a **frame-based UI system**. Every UI element (buttons, dialogs, labels, panels) is represented by a **frame struct** of size `0x1C8` bytes.

### Frame Structure (key offsets)

| Offset | Size | Description |
|--------|------|-------------|
| `0xC0` | DWORD | Frame state flags (visibility, enabled, etc.) |
| `0x128` | PTR | Pointer used by `GetFrameContext` |
| `0x134` | DWORD | Frame hash — unique identifier for the UI element |

### FrameArray

All frames are stored in a **FrameArray** — an array of frame pointers accessed via a scanned label in the game's memory. The scan finds this array at initialization, and individual frames are located by iterating the array and matching the hash at offset `0x134`.

### Key Frame Hashes

| UI Element | Hash Value |
|------------|------------|
| **Play button** | `184818986` |
| **Reconnect YES** | `1398610279` |
| **Reconnect NO** | `3600335809` |

### Frame Visibility

A frame is **visible** when its state field at offset `0xC0` has the flag `0x20000` set:

```autoit
$state = DllStructGetData($frameStruct, 1)  ; read DWORD at +0xC0
$visible = BitAND($state, 0x20000) <> 0
```

### GetFrameContext

The frame's context pointer is derived from the frame struct:

```
context = [frame + 0x128] - 0x128
```

This gives the **parent frame pointer**, which is used to reach the callbacks table needed for sending UI messages.

---

## SendFrameUIMsg Function

This is the game's internal function for dispatching UI messages to frames. It is the mechanism behind all button clicks, checkbox toggles, and dialog interactions.

### Signature

```
__thiscall SendFrameUIMsg(ECX=frame_callbacks_ptr, DWORD msgid, DWORD wParam, DWORD lParam)
```

- **ECX** = `context + 0xA8` (the callbacks pointer)
- **msgid** = message type (e.g., `0x31` for mouse click)
- **wParam** = action data (e.g., `0x7` for MouseUp)
- **lParam** = 0 (unused for clicks)

### Finding the Function

Scan the `.text` section for the byte pattern:

```
83 C1 DC E8
```

This matches `add ecx, -0x24; call <target>`. The **call target** is `SendFrameUIMsg`.

**Critical distinction:** The `add ecx, -0x24` instruction exists at a DIFFERENT call site within the game code. It is a pre-adjustment that specific caller makes before invoking `SendFrameUIMsg`. When calling `SendFrameUIMsg` directly (from injected shellcode), you must NOT apply the `-0x24` adjustment. Pass `context + 0xA8` directly as ECX.

### Message Parameters

| Parameter | Value | Meaning |
|-----------|-------|---------|
| `msgid` | `0x31` | `kMouseClick2` — synthetic mouse click |
| `wParam` (action_state) | `0x7` | MouseUp event |
| `lParam` | `0` | Unused |

**Important:** A single MouseUp (`0x7`) is sufficient. Sending a MouseDown+MouseUp pair is NOT required and can cause issues.

---

## Approaches Tried and Why They Failed

### 1. CreateRemoteThread (FAILED)

**Approach:** Allocate executable memory in the GW process via `VirtualAllocEx`, write shellcode that calls `SendFrameUIMsg`, and launch it via `CreateRemoteThread`.

**Result:** The thread **hangs**. `WaitForSingleObject` returns `WAIT_TIMEOUT`.

**Root cause:** `SendFrameUIMsg` requires **game-thread context**. Guild Wars' UI system uses internal mutexes/locks that are held by the game's main thread. Calling `SendFrameUIMsg` from a foreign thread deadlocks waiting to acquire those locks. The game thread never releases them because it's waiting for its next tick, creating a classic deadlock.

### 2. GameTick Hook (FAILED)

**Approach:** Find the game's tick function by scanning for the assertion string `"renderElapsed >= 0"` in `FrApi.cpp`. Install a detour (5-byte JMP) at the assertion location to redirect execution to our shellcode.

**Problems:**

1. **The assertion path doesn't execute during normal gameplay.** It's a debug/error path. The detour never fires under normal conditions, so button clicks never happen.

2. **When the assertion IS reached** (e.g., during trader interactions or certain frame-rate edge cases), **our detour corrupts those 5 bytes of game code**. The original instruction bytes at the assertion site are part of a live code path that other game systems use. Overwriting them causes GW to crash — particularly visible as crashes at the material trader.

### 3. Rendering Hook Queue Processing (WORKS)

**Approach:** Use the existing `RenderingModProc` hook, which fires every frame including at char select. Add queue processing code that checks for pending button-click commands.

**Placement:** The queue processing code is inserted BEFORE the `cmp dword[DisableRendering], 1` instruction.

**Why before:** The assembler auto-generates an **unconditional JMP** (`E9` opcode) after the `cmp` instruction — NOT a conditional jump. This means everything placed after that `cmp` would be **dead code** that never executes. The queue check must come before.

**Queue processing logic:**

```asm
; Check if we're at char select (not in-game)
cmp dword [MapIsLoaded], 1
jnz .process_queue        ; MapIsLoaded=0 means char select

; In-game path: skip queue, proceed to normal rendering
jmp .normal_rendering

.process_queue:
; Read command from QueueBase
mov eax, dword [QueueBase]
test eax, eax
jz .normal_rendering      ; No pending command

; Execute the queued shellcode
call dword [SavedIndex]   ; SavedIndex points to the shellcode

.normal_rendering:
; ... original RenderingModProc continues ...
```

**In-game behavior (MapIsLoaded=1):** The `jnz` skips queue processing entirely, falling through to `add esp, 4` — identical behavior to the original unmodified hook. Zero performance impact during gameplay.

### 4. HandleCase in MainProc (DOESN'T FIRE at char select)

**Approach:** Use `MainProc`, which hooks the game's main processing function. `HandleCase` triggers when `[field_198] == 0`, which corresponds to the char select state.

**Problem 1:** The hooked function is **not called at char select** — it only fires during active gameplay. The game uses a different execution path at the character select screen.

**Problem 2:** Even if it fired, `HandleCase` uses `jmp ebx` (an indirect jump with no return address pushed). Shellcode ending with `ret` would pop garbage off the stack and crash.

### 5. PostMessage / ControlClick (FAILED)

**Approach:** Use Windows API to send synthetic mouse messages to the GW window.

**Results:**

| Method | Result |
|--------|--------|
| `PostMessage WM_LBUTTONDOWN/UP` | Silently ignored |
| `ControlClick` | Silently ignored |
| Physical `MouseClick` | Works (but requires cursor movement) |

**Root cause:** Guild Wars uses a DirectX renderer that processes input through DirectInput, not the Windows message queue. Synthetic Windows messages (`WM_LBUTTONDOWN`, `WM_LBUTTONUP`) are never seen by the game's input handler. Only real mouse events (physical cursor movement + click) register.

This rules out `PostMessage`/`ControlClick` for headless operation.

---

## The Double-Init Bug (ROOT CAUSE of trader crashes)

### The Problem

When using `-autolaunch` mode, `InitializeGameClientForGWA2` is called **twice**:

1. **First call** at char select — in `GWLauncher_AutoLaunchAndConnect` Phase 2
2. **Second call** in-game — in `_ConnectToSelectedClient` via `InitializeGameClientData`

### How It Manifests

The second init re-scans for `$memory_interface_header`. The game's memory layout **changes between char select and in-game** (modules load/unload, heap allocations shift). The scan finds the header at a **different address**.

The stored injection base at that wrong header is garbage. `CompleteASMCode` then resolves **all labels** to wrong addresses. Command struct pointers become invalid. Any game interaction that uses those pointers — particularly the material trader — crashes GW.

### Evidence

```
First init:  SceneContext: 0x08AD0320, Trader: 0x08AD1180
Second init: SceneContext: 0x08830320, Trader: 0x08831180
Delta: 0x2A0000 bytes — labels shifted by ~2.7MB!
```

The `0x2A0000` (2.7MB) delta means every single function pointer, data address, and jump target resolved by the assembler is off by millions of bytes. The game crashes as soon as any of these stale pointers are dereferenced.

### The Fix

Cache `$memory_interface_header` from the first init. On subsequent calls, reuse the cached value:

```autoit
If $memory_interface_header <> 0 Then
    Debug('Reusing cached memory_interface_header')
ElseIf ...
    ; Normal scan logic for first init
EndIf
```

This ensures the injection base remains stable across the char-select-to-gameplay transition. All label addresses stay correct.

---

## Assembly Layout Sensitivity

The injected assembly code block is extremely sensitive to layout changes. The assembler resolves labels to absolute addresses at init time, and any shift in code or data size cascades to all subsequent addresses.

### Rules

1. **NEVER add or remove data fields in `AssemblerCreateData()`** — this shifts `QueueBase`, `AgentCopyBase`, and every other data label.

2. **NEVER add new assembler functions or change existing function code sizes** — this shifts all subsequent code labels. A single extra byte in one function moves every function after it.

3. **The assembler has a ".5 offset" bug** where `$asm_injection_size` accumulates fractional bytes across operations. Over many instructions, this makes label-based jumps off by approximately 4 bytes.

   **Workaround:** Use hardcoded jump offsets instead of symbolic labels for critical jumps:
   ```asm
   ; Instead of: jnz .some_label
   ; Use hardcoded offset:
   db 0x75, 0x3D   ; jnz +0x3D (61 bytes forward)
   ```

4. **The `cmp dword[DisableRendering], 1` instruction auto-generates an unconditional JMP (`E9`)** — NOT a conditional jump. This is an assembler quirk. Everything placed after this instruction is dead code. All new logic must be inserted BEFORE this `cmp`.

---

## Working Solution (Final)

The complete working solution combines direct memory reads for detection with the rendering hook queue for button clicks.

### 1. Frame Detection (No injection needed)

`IsAtCharSelect` and `IsReconnectDialogShowing` work via **direct memory reads** from the AutoIt process. They iterate the FrameArray, match frame hashes, and check visibility flags. This requires no code injection and works reliably at all times.

```autoit
; Check if Play button frame is visible
Func IsAtCharSelect()
    Local $frame = FindFrameByHash(184818986)  ; Play button hash
    If $frame = 0 Then Return False
    Local $state = ReadFrameState($frame)       ; Read DWORD at +0xC0
    Return BitAND($state, 0x20000) <> 0         ; Visibility flag
EndFunc

; Check if reconnect dialog is showing
Func IsReconnectDialogShowing()
    Local $frame = FindFrameByHash(1398610279)  ; YES button hash
    If $frame = 0 Then Return False
    Local $state = ReadFrameState($frame)
    Return BitAND($state, 0x20000) <> 0
EndFunc
```

### 2. Button Clicks via Rendering Hook Queue

Shellcode is written to allocated memory and queued via `Enqueue()`. The `RenderingModProc` hook processes the queue on the **game thread** (before the `DisableRendering` check), ensuring proper thread context.

### 3. Shellcode Structure

```asm
; Button click shellcode
mov ecx, <context + 0xA8>    ; Callbacks pointer (ECX for __thiscall)
push 0                        ; lParam = 0
push 7                        ; wParam = 0x7 (MouseUp action_state)
push 0x31                     ; msgid = kMouseClick2
call <SendFrameUIMsg>         ; Call the game's UI message dispatcher
ret                           ; Return to queue processor
```

**Key details:**
- **ECX** = `context + 0xA8` — the callbacks pointer, passed directly. No `-0x24` adjustment.
- **Single MouseUp** (`action_state = 0x7`) — not a MouseDown+MouseUp pair.
- **`ret`** at the end returns control to the `RenderingModProc` queue processor, which then continues with normal rendering.

### 4. Double-Init Fix

Cache `$memory_interface_header` on first init. All subsequent calls to `InitializeGameClientForGWA2` reuse the cached value, preventing address drift between char select and in-game states.

---

## Files Modified

| File | Changes |
|------|---------|
| **GWA2_Assembly.au3** | RenderingModProc queue processing (before DisableRendering check), `$memory_interface_header` caching, `scan_patterns` array size update |
| **GWA2_FrameUI.au3** | Frame detection (`IsAtCharSelect`, `IsReconnectDialogShowing`), `ClickFrameButton`, `PressPlayButton`, `DismissReconnectDialog` |
| **GWLauncher.au3** | `AutoLaunchAndConnect` flow, hero dropdown config, Phase 4 wait-for-map logic |
| **Froggy_HM_v1.6.au3** | Headless autolaunch support, hero config loading after `GUI_Create` |
| **BotsHub_Stubs.au3** | Extension flags, `Extend_*` function stubs for BotsHub compatibility |
