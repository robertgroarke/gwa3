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

### Open Questions

1. **What additional GWCA data pointers does ButtonClick need?** — Need to read the .cpp implementation, not just headers
2. **Is SendFrameUIMsg actually the right function?** — It's also the "Action" function. Maybe it dispatches actions, not frame UI messages
3. **Does clicking Play require a network packet?** — The button handler might send a server request, not just a UI dispatch
4. **What is InteractionMessage?** — The callback signature takes this struct, not raw args. Need to understand its layout
5. **Hash seed values** — Not populated in manual test. Might be needed for GetFrameById
