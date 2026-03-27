# Plan: UIMessage Sniffer for Character Select Screen

**Goal:** Intercept GW's internal `SendUIMessage` calls to discover exactly what UI messages are sent when you click Play, select a character, or dismiss the reconnect dialog. Then use those message IDs to replicate the actions programmatically.

---

## How We'll Work Together

### Phase 1: Build the Sniffer (Claude builds, no user action needed)

1. Clone the `GWA2_Assembly_Chatlog.au3` pattern into a new `GWA2_Assembly_UISniffer.au3`
2. Hook the game's `SendUIMessage` function (already scanned as `UIMessage` label)
3. On each call, copy the `msgid` + first 128 bytes of `wParam` into a shared buffer
4. Notify AutoIt via `PostMessage` (same pattern as chatlog)
5. AutoIt-side callback reads the buffer and logs the message to the Froggy console
6. Add ASM-level filtering to only capture messages in the `0x10000000` and `0x30000000` ranges (skip rendering/mouse/frame messages)

### Phase 2: Deploy and Capture (User clicks, Claude reads)

1. Claude launches a GW client for BEASTRIT with the sniffer active
2. GW loads to character select screen
3. **User action needed:** Click these things one at a time while Claude captures:
   - Click on a different character portrait (to see character selection message)
   - Click the Play button (to see the "enter game" message)
   - If reconnect dialog appears: click Yes, then click No on next attempt
4. Claude reads the captured UIMessage IDs and wParam data from the log

### Phase 3: Implement Programmatic Actions (Claude builds)

1. Use the captured message IDs to build `GWLauncher_SelectCharacter()` and `GWLauncher_PressPlay()` functions
2. These call `CommandUIMsg` (already in our ASM framework) with the discovered IDs
3. For the reconnect dialog: use `kSendDialog` with the captured dialog IDs
4. Test the programmatic approach — no mouse/keyboard interaction needed

---

## Technical Approach

### Architecture (cloning the ChatLog pattern)

```
GWA2_Assembly_UISniffer.au3
├── ExtendAddPattern_UISniffer()     — register scan patterns (reuses existing UIMessage)
├── ExtendScanner_UISniffer()        — resolve scan results to labels
├── ExtendAssemblerData_UISniffer()  — allocate shared memory buffer
├── ExtendAssembler_UISniffer()      — assemble the hook trampoline
├── Extend_WriteDetour_UISniffer()   — activate the JMP hook
├── UISnifferEnable()                — runtime enable (write detour)
├── UISnifferDisable()               — runtime disable (revert detour)
└── UISnifferCallback()              — AutoIt GUIRegisterMsg handler
```

### Shared Memory Buffer Layout

```
UISnifferCallbackHandle  /4    ; HWND for PostMessage notification
UISnifferCallbackEvent   /4    ; Windows message ID (0x502)
UISnifferCounter         /4    ; Monotonic counter
UISnifferMsgId           /4    ; Last captured msgid (uint32)
UISnifferWParam          /128  ; Copy of wParam struct data
UISnifferLParam          /4    ; lParam value
UISnifferEnabled         /4    ; 1 = active, 0 = paused
```

### ASM Hook Trampoline (UISnifferProc)

```nasm
UISnifferProc:
    pushfd
    pushad

    ; Check if sniffer is enabled
    cmp dword [UISnifferEnabled], 1
    jne UISnifferSkip

    ; Read args from stack (above saved regs + flags + return addr = 40 bytes offset)
    mov eax, [esp+28h]     ; msgid

    ; FILTER: only capture UI-level messages (0x1000xxxx and 0x3000xxxx ranges)
    mov ecx, eax
    shr ecx, 24            ; get high byte
    cmp ecx, 10h           ; 0x10xxxxxx range
    je UISnifferCapture
    cmp ecx, 30h           ; 0x30xxxxxx range
    je UISnifferCapture
    jmp UISnifferSkip       ; skip rendering/input messages

UISnifferCapture:
    ; Save msgid
    mov [UISnifferMsgId], eax

    ; Save lParam
    mov eax, [esp+30h]
    mov [UISnifferLParam], eax

    ; Copy wParam (pointer -> first 128 bytes of pointed-to data)
    mov esi, [esp+2Ch]      ; wParam pointer
    test esi, esi
    jz UISnifferNoWParam
    lea edi, [UISnifferWParam]
    mov ecx, 32             ; 32 dwords = 128 bytes
    rep movsd

UISnifferNoWParam:
    ; Increment counter
    inc dword [UISnifferCounter]

    ; Notify AutoIt via PostMessage(hwnd, event, 0, &msgid)
    push 0
    push dword [UISnifferMsgId]
    push dword [UISnifferCallbackEvent]
    push dword [UISnifferCallbackHandle]
    call dword [PostMessage]

UISnifferSkip:
    popad
    popfd

    ; Execute overwritten prologue bytes (determined at runtime)
    <original 5+ bytes>

    ; Jump back to UIMessage + 5
    jmp UISnifferReturn
```

### AutoIt Callback

```autoit
Func UISnifferCallback($hWnd, $iMsg, $wParam, $lParam)
    ; Read the full buffer from game memory
    Local $msgId = MemRead(GetValue('UISnifferMsgId'))
    Local $wParamData = ... ; read 128 bytes
    Local $lParamVal = MemRead(GetValue('UISnifferLParam'))

    ; Format and display
    Local $msgHex = '0x' & Hex($msgId, 8)
    Local $wParamHex = '0x' & Hex(MemRead(GetValue('UISnifferWParam')), 8)
    Out('[UISniffer] MsgID=' & $msgHex & ' wParam=' & $wParamHex & ' lParam=' & $lParamVal)

    Return $GUI_RUNDEFMSG
EndFunc
```

---

## What We're Looking For

When the user clicks specific UI elements, we expect to see:

| User Action | Expected UIMessage | Notes |
|---|---|---|
| Click character portrait | Unknown — might be internal frame event | May not go through SendUIMessage |
| Click Play button | Possibly `kSendEnterMission (0x30000002)` or a login-specific message | This is the key one |
| Reconnect dialog Yes | Possibly `kSendDialog (0x30000001)` with dialog ID | Similar to NPC dialog buttons |
| Reconnect dialog No | Possibly `kSendDialog (0x30000001)` with different dialog ID | |

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| UIMessage is called thousands of times per second (rendering, input) | PostMessage flood, game lag | ASM-level filtering by msgid range (0x10xxxxxx and 0x30xxxxxx only) |
| Original prologue bytes vary between GW versions | Hook crash | Read and replicate prologue bytes at runtime, verify instruction boundary |
| Character select might not use SendUIMessage at all | Sniffer captures nothing useful | Fall back to polling PreGameContext state changes, or hook `DoAction` instead |
| wParam is a pointer that might be invalid by the time AutoIt reads it | Garbage data | Copy wParam data in the ASM hook before returning control to the game |
| Game thread blocked during PostMessage | Frame stutter | PostMessage is non-blocking (unlike SendMessage) |

---

## Estimated Effort

| Phase | Time | Who |
|---|---|---|
| Phase 1: Build sniffer | ~2-3 hours | Claude |
| Phase 2: Deploy + capture | ~15 min | User clicks, Claude reads |
| Phase 3: Implement actions | ~1 hour | Claude |
| **Total** | **~3-4 hours** | |

---

## Prerequisites

- Antigravity running as admin (for process memory access)
- One GW client at the character select screen (not in-game)
- User available to click UI elements on command during Phase 2
