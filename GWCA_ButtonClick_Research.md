# GWCA ButtonClick Research — Detailed Findings

## Session: 2026-03-28

### Critical Bug Found: `_WriteLE32` Was Broken

**Root cause of ALL prior click failures.** The function used `DllStructGetData($tmp, 1, $b)` element indexing on a dword field — only element 1 returns the value, elements 2-4 return 0. This meant every shellcode address and call offset was corrupted (only low byte written, rest zeroed).

**Fixed in** `GWA2_FrameUI.au3:174` — replaced with `BitShift`/`BitAND` byte extraction.

**Verified by**: reading shellcode bytes back from game memory and confirming marker writes execute correctly.

---

### Rendering Hook Findings

| Condition | Hook Active? |
|---|---|
| Fresh client, first InitializeGameClientForGWA2 | YES |
| After GW::Initialize (GWCA) | NO — hooks conflict |
| After gwca.dll load + Scanner::Initialize only | YES |
| Re-running InitializeGameClientForGWA2 on same PID | Intermittent |

The BotsHub rendering hook (`RenderingModProc`) processes the command queue via `call dword[SavedIndex]` — shellcode must end with `RET`.

GWCA's `GW::Initialize()` hooks the same game functions (rendering, etc.) and overwrites BotsHub's hooks, breaking command queue processing.

---

### SendFrameUIMsg Analysis

**Address**: game+0x2286D0 (consistent across all clients tested)

**NOT hooked** by BotsHub — starts with original prologue `55 8B EC` (push ebp; mov ebp, esp).

**Same address as BotsHub's "Action" label** — both scans resolve to game+0x2286D0. This is a general frame message dispatcher.

**Function disassembly** (first 32 bytes):
```x86
55           push ebp
8B EC        mov ebp, esp
56           push esi
8B 75 08     mov esi, [ebp+8]    ; esi = first stack arg (msgid)
57           push edi
8B F9        mov edi, ecx        ; edi = this (ECX = frame_callbacks)
83 FE 09     cmp esi, 9          ; check msgid
75 0C        jnz +12
68 77010000  push 0x177          ; if msg=9: push 0x177
B9 30246700  mov ecx, 0x672430   ; static data ptr
EB 0F        jmp +15
83 FE 0B     cmp esi, 0xB        ; check msg=11
75 14        jnz +20
```

**Calling convention**: `__thiscall(ECX=callbacks, msgid, wParam, lParam)`

**Game call site context** (at scan pattern `83 C1 DC E8`):
```x86
8D 45 EC    lea eax, [ebp-0x14]  ; wParam = local struct on stack
6A 00       push 0               ; lParam = 0
50          push eax             ; wParam
6A 2B       push 0x2B            ; msgid = 43 (NOT 0x2F or 0x31!)
83 C1 DC    add ecx, -0x24       ; ECX adjustment before call
E8 ...      call SendFrameUIMsg
```

**ECX at call site**: The game does `add ecx, -0x24` before calling. If ECX was frame+0xA8 (callbacks array), the function receives ECX = frame+0x84.

---

### All Tested Approaches (with correct _WriteLE32)

#### SendFrameUIMsg Variations (7 tests, all failed, no crash)
| ECX Value | MsgID | wParam | Result |
|---|---|---|---|
| frame+0x84 | 0x2F (kMouseAction) | action struct | No click |
| frame+0xA8 | 0x2F | action struct | No click |
| cbBuf ptr | 0x2F | action struct | No click |
| frame+0x84 | 0x31 (kMouseClick2) | action struct | No click |
| frame_ptr | 0x2F | action struct | No click |
| frame+0x84 | 0x22 (kMouseClick) | action struct | No click |
| frame+0x84 | 0x2F | NULL | No click |

#### GWCA Function Calls (via rendering hook)
| Function | Args | Result |
|---|---|---|
| ButtonClick(frame_ptr) | cdecl, push Frame* | No click |
| ButtonClick(frame_id) | cdecl, push uint32 | No click |
| ButtonFrame::Click() | thiscall ECX=Frame* | No click |
| MouseAction(6) + MouseAction(7) | thiscall ECX=Frame* | No click |
| GetFrameById(40) + Click | combined | No click |

#### Other Approaches
| Approach | Result |
|---|---|
| Direct callback[0] call (cdecl args) | No click |
| Direct callback[0] call (thiscall) | No click |
| ClickFrameButton (existing code, fixed) | No click |
| GWCA with manual data section population | No click |

**All approaches**: game stays alive, function returns normally, no crash, no visible effect.

---

### GWCA Data Section Offsets

| Offset | Content | Set By |
|---|---|---|
| +0x8A39C | Original SendFrameUIMsg game func ptr | GW::Initialize |
| +0x8A3A0 | Hooked SendFrameUIMsg (GWCA wrapper) | GW::Initialize |
| +0x8A37C | GetChildFrame game func ptr | GW::Initialize |
| +0x8A410 | RootFrame game func ptr | GW::Initialize |
| +0x8A3D0 | SetWindowVisible | GW::Initialize |
| +0x8A3B0 | Frame hash table address | GW::Initialize |
| +0x880F0 | Hash seed (ESI initial) | GW::Initialize |
| +0x880F4 | Hash seed (EAX initial) | GW::Initialize |
| +0x880F8 | Hash seed 2 | GW::Initialize |

**Scanner::Initialize** only sets up the pattern scanner engine. **GW::Initialize** calls each module's Initialize() which uses Scanner::Find to populate function pointers.

**Manual population tested**: wrote SendFrameUIMsg, GetChildFrame, RootFrame, FrameHashTable from BotsHub scan values. GWCA ButtonClick still didn't work — likely needs additional pointers or internal state.

---

### GWCA Source Analysis (from headers)

**Frame struct** (0x1C8 bytes):
- `+0xA8`: `GW::Array<FrameInteractionCallback> frame_callbacks`
- `+0xB4`: `child_offset_id` (GWCA source) — BUT our code uses 0xB8
- `+0xB8`: `frame_id` (GWCA source) — BUT our code uses 0xBC
- `+0x128`: `FrameRelation` (parent, siblings, hash)
- `+0x134`: `frame_hash_id`
- `+0x18C`: `frame_state`

**NOTE**: There's a 4-byte offset discrepancy between GWCA source offsets and what works in practice. Our code reads frame_id=40 from offset 0xBC (correct value), but GWCA source says frame_id is at 0xB8. This suggests either struct padding differences or a version mismatch.

**FrameInteractionCallback struct**:
```cpp
struct FrameInteractionCallback {
    UIInteractionCallback callback;  // void(__cdecl*)(InteractionMessage*, void*, void*)
    void* uictl_context;
    uint32_t h0008;
};
```
Each entry is 12 bytes. The callback takes an `InteractionMessage*` as first arg (NOT individual frame/msg/wp/lp args).

**kMouseAction struct** (UIPacket):
```cpp
struct kMouseAction {
    uint32_t frame_id;
    uint32_t child_offset_id;
    ActionState current_state;  // 6=MouseDown, 7=MouseUp, 8=MouseClick
    void* wparam = 0;
    void* lparam = 0;
};
```

**ButtonClick declaration**: `bool ButtonClick(Frame* btn_frame);`
**SendFrameUIMessage declaration**: `bool SendFrameUIMessage(Frame* frame, UIMessage message_id, void* wParam, void* lParam = nullptr);`

---

### Play Button Frame State (from inject_research dump)

| Field | Value |
|---|---|
| Frame ptr | 0x08229640 (varies per session) |
| Frame ID | 40 |
| Child offset ID | 7 |
| State | 0x4904 (created, not hidden, not disabled) |
| Callbacks | 4 entries, 12 bytes each |
| callback[0] | game+0x20D6B0 (consistent offset) |
| field_0x1C4 | 0 |

---

---

## GWCA Function Disassembly (from gwca.dll binary)

### ButtonClick @ +0x255E0 (14 bytes)
```x86
push ebp
mov ebp, esp
mov ecx, [ebp+8]    ; ecx = Frame* btn_frame
test ecx, ecx       ; null check
jz return_false
pop ebp
jmp 0x16660          ; TAIL CALL → ButtonFrame::Click (ecx=Frame*)
return_false:
xor al, al           ; return false
pop ebp
ret
```
Simply null-checks, then tail-calls `Click()` with ECX=Frame*.

### ButtonFrame::Click @ +0x16660 (35 bytes)
```x86
push esi
push 6               ; ActionState::MouseDown
mov esi, ecx         ; save Frame*
call MouseAction     ; → +0x173D0
test al, al
jz fail              ; if MouseDown failed, return false
push 7               ; ActionState::MouseUp
mov ecx, esi         ; restore Frame*
call MouseAction     ; → +0x173D0
test al, al
jz fail
mov al, 1            ; return true
pop esi
ret
fail:
xor al, al           ; return false
pop esi
ret
```
Calling convention: `__thiscall`. ECX=Frame*. Calls MouseAction(6) then MouseAction(7).

### ButtonFrame::MouseAction @ +0x173D0 (CRITICAL)
```x86
push ebp
mov ebp, esp
sub esp, 0x24                    ; 36 bytes locals
mov eax, [0x100884C0]            ; stack cookie
xor eax, ebp
mov [ebp-4], eax
push esi
mov esi, ecx                     ; esi = this (Frame*)

; *** CHECK FRAME STATE ***
mov eax, [esi+0x18C]             ; frame_state
and eax, 0x214                   ; mask created|hidden|disabled
cmp eax, 4                       ; must be exactly 0x4 (created only)
jnz return_false                 ; BAIL if hidden or disabled

; *** GET FRAME CONTEXT ***
push esi                          ; arg = Frame*
call 0x25EC0                      ; GetFrameContext-like function
mov edx, eax                      ; edx = context
add esp, 4                        ; cdecl cleanup
test edx, edx
jz return_false                   ; BAIL if context is null

; Check context's frame_state too
mov ecx, [edx+0x18C]
shr ecx, 2
test cl, 1                        ; check created bit
jz return_false

; *** BUILD kMouseAction STRUCT ON STACK ***
mov eax, [esi+0xBC]              ; frame_id (from Frame struct)
mov [ebp-0x24], eax              ; action.frame_id
mov eax, [esi+0xB8]              ; child_offset_id
mov [ebp-0x20], eax              ; action.child_offset_id
mov eax, [esi+0x1C4]             ; field_0x1C4
mov [ebp-0x0C], eax              ; action field
lea eax, [ebp-0x10]
mov [ebp-0x18], eax              ; INTERNAL POINTER to local!
mov eax, [ebp+8]                 ; ActionState from stack arg
mov [ebp-0x1C], eax              ; action.current_state

; *** CALL GWCA SendFrameUIMessage ***
lea eax, [ebp-0x24]              ; eax = &kMouseAction struct
push 0                            ; lParam = 0
push eax                          ; wParam = &kMouseAction
push 0x31                         ; msgid = kMouseClick2 (0x31)
push edx                          ; Frame CONTEXT (NOT Frame*!)
; zero remaining action fields
mov [ebp-0x14], 0
mov [ebp-0x10], 0
mov [ebp-0x08], 0
call SendFrameUIMessage           ; → +0x274D0 (GWCA wrapper)
add esp, 16                       ; cdecl: 4 args cleaned
; ... stack cookie check, ret 4
```

**CRITICAL FINDINGS:**
1. First arg to SendFrameUIMessage is **FRAME CONTEXT** (from GetFrameContext), NOT Frame*
2. Uses msgid **0x31** (kMouseClick2)
3. Reads frame_id from offset **0xBC** and child_offset_id from **0xB8** (matches our code)
4. Action struct contains an **internal pointer** to a local variable (stack-relative)
5. Also reads field_0x1C4 from Frame

### GWCA SendFrameUIMessage @ +0x274D0 (CRITICAL)
```x86
push ebp
mov ebp, esp
sub esp, 0x24
cmp dword [0x1008A3A0], 0        ; *** CHECK: hooked func ptr must be non-null ***
push ebx
jz return_false                   ; IF NULL → RETURN FALSE (not initialized!)

mov ebx, [ebp+8]                 ; first arg = frame CONTEXT
test ebx, ebx
jz return_false

; ... FNV-1a hash of msgid for hook dispatch ...

; Eventually calls the game function:
lea ecx, [ebx+0xA8]              ; ECX = context+0xA8 (for __thiscall game func)
push lParam
push wParam
push msgid
call [0x1008A3A0]                 ; call HOOKED game SendFrameUIMsg
```

**ROOT CAUSE FOUND:**
- `[0x1008A3A0]` = gwcaBase + 0x8A3A0 = the **hooked SendFrameUIMsg pointer**
- We populated +0x8A39C (original) but NOT +0x8A3A0 (hooked)!
- GWCA wrapper checks +0x8A3A0 and **returns false if it's NULL**
- ALL GWCA ButtonClick attempts silently returned false because this pointer was 0

### GetFrameContext-like function @ +0x25EC0
Called by MouseAction with Frame* as arg. Returns a "context" pointer. The context appears to be a different object than the Frame — possibly a parent frame, a frame controller, or a UI context. GWCA SendFrameUIMessage then uses context+0xA8 as the __thiscall ECX for the game's actual dispatch function.

---

## BREAKTHROUGH: Play Button Clicked Successfully (2026-03-28)

### What Worked
`test_gwca_manual_ptrs.au3` with the `+0x8A3A0` fix **successfully clicked the Play button**.
The game entered map loading (confirmed by user observation). Then crashed during/after load.

### The Fix That Worked
Write the game's SendFrameUIMsg address to **BOTH** GWCA data offsets:
- `+0x8A39C` = original function pointer
- **`+0x8A3A0` = hooked function pointer** (GWCA wrapper checks this — returns false if NULL)

### Why It Crashed
The crash after map load is likely caused by:
1. **GWCA's hook dispatch accesses uninitialized hook tables** — the SendFrameUIMessage wrapper does FNV hashing and hook dispatch at `gwcaBase + 0x854A4` area, which is all zeros since GW::Initialize was never called
2. **The rendering hook continues to fire** after map load, and GWCA's code may access invalid state
3. **Interaction between BotsHub hooks and GWCA code** in the now-active game state

### Implementation Path
The approach is:
1. Inject gwca.dll (LoadLibraryW via CreateRemoteThread)
2. Skip Scanner::Initialize and GW::Initialize entirely
3. Manually populate GWCA data section with known function pointers:
   - `+0x8A39C`: game SendFrameUIMsg (game_base + 0x2286D0)
   - `+0x8A3A0`: same address (hooked ptr, checked by GWCA wrapper)
   - `+0x8A37C`: game GetChildFrame (game_base + 0x20E2B0)
   - `+0x8A410`: game RootFrame (game_base + 0x22DC20)
   - `+0x8A3B0`: FrameArray label address
4. Call ButtonClick(frame_ptr) via rendering hook shellcode
5. After click, unload gwca.dll or stop using it

### Remaining Pointers to Populate
| Offset | Content | Needed By |
|---|---|---|
| **+0x8A3A0** | SendFrameUIMsg (hooked ptr) | GWCA SendFrameUIMessage — **CRITICAL, must be non-null** |
| +0x8A39C | SendFrameUIMsg (original) | Some code paths |
| +0x8A37C | GetChildFrame | GetFrameContext chain |
| +0x8A410 | RootFrame | GetFrameById |
| +0x8A3B0 | Frame hash table | GetFrameById |
| +0x884C0 | Stack cookie seed | MouseAction (read-only, CRT sets this) |

### Crash Prevention TODO
- Investigate the exact crash point (likely in GWCA hook dispatch at 0x854A4 area)
- Consider: write a NOP/passthrough hook at +0x8A3A0 instead of the game function directly
- Or: unload gwca.dll after click succeeds (FreeLibrary)
- Or: populate the hook dispatch table at +0x854A4 with empty/stub entries
