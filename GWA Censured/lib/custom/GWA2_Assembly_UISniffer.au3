#include-once

; =============================================================================
; GWA2_Assembly_UISniffer.au3
;
; Hooks the game's internal SendUIMessage function to intercept and log
; all UI messages. Used to discover what messages the game sends when
; buttons are clicked (Play, Yes/No on dialogs, character selection, etc.)
;
; Architecture: clones the GWA2_Assembly_Chatlog.au3 pattern
;   - ASM hook trampoline saves msgid + wParam to shared memory
;   - PostMessage notifies AutoIt
;   - AutoIt callback reads and logs the message
;   - ASM-level filtering skips high-volume rendering/input messages
;
; Enable/disable at runtime via UISnifferEnable() / UISnifferDisable()
; =============================================================================

Global $g_UISnifferEnabled = False
Global $g_UISnifferLogFunc = ''
Global $g_UISniffer_MsgId_Addr = 0
Global $g_UISniffer_WParam_Addr = 0
Global $g_UISniffer_LParam_Addr = 0
Global $g_UISniffer_Counter_Addr = 0
Global $g_UISniffer_LastCounter = 0

; Extension flag — checked by GWA2_Assembly.au3 via IsDeclared
Global $g_b_UISniffer = True

; =============================================================================
; Extension Points (called by the GWA2 framework during initialization)
; =============================================================================

;~ Register additional scan patterns — UIMessage is already scanned by the framework
;~ We just need it resolved as a hook site
Func ExtendAddPattern_UISniffer()
    ; UIMessage is already scanned as 'func' type by the framework
    ; We need UIMessageStart and UIMessageReturn labels
    ; These will be set in ExtendScanner_UISniffer
EndFunc

;~ Resolve scan results into labels for the hook
Func ExtendScanner_UISniffer()
    ; UIMessage function address is already available via $scan_results['UIMessage']
    ; Set it as a hook site (start = function entry, return = entry + 5)
    Local $uiMsgAddr = $scan_results['UIMessage']
    SetLabel('UIMessageStart', Ptr($uiMsgAddr))
    SetLabel('UIMessageReturn', Ptr($uiMsgAddr + 5))

    Debug('UIMessageStart: ' & GetLabel('UIMessageStart'))
    Debug('UIMessageReturn: ' & GetLabel('UIMessageReturn'))

    SetLabel('UISnifferCallbackEvent', '0x00000502')
EndFunc

;~ Allocate shared memory data region
Func ExtendAssemblerData_UISniffer()
    _('UISnifferCallbackHandle/4')
    _('UISnifferCallbackEvent/4')
    _('UISnifferCounter/4')
    _('UISnifferEnabled/4')
    _('UISnifferMsgId/4')
    _('UISnifferWParam/4')
    _('UISnifferLParam/4')
EndFunc

;~ Assemble the hook trampoline
Func ExtendAssembler_UISniffer()
    _('UISnifferProc:')
    _('pushfd')
    _('pushad')

    ; Check if sniffer is enabled
    _('cmp dword[UISnifferEnabled],1')
    _('jnz UISnifferSkip')

    ; UIMessage is __cdecl: args on stack
    ; After pushad (32 bytes) + pushfd (4 bytes) + return addr from JMP (0, we used JMP not CALL)
    ; Actually: pushfd=4, pushad=32, + ret addr from the call that originally entered UIMessage
    ; The JMP replaces the first 5 bytes. Before our hook, the caller did:
    ;   call UIMessage  (which pushes return addr)
    ; Actually no — WriteDetour overwrites the first 5 bytes of UIMessage with JMP.
    ; So the call stack is: [caller's return addr] [msgid] [wParam] [lParam]
    ; After pushfd+pushad: ESP offset = 32+4 = 36 bytes above the original ESP
    ; Original ESP had: [ret_addr][msgid][wParam][lParam]
    ; So: msgid = [esp+36+4] = [esp+28h], wParam = [esp+2Ch], lParam = [esp+30h]
    ; Wait — the JMP doesn't push a return address. Let me reconsider.
    ;
    ; Before hook: caller does `call UIMessage` which pushes return addr on stack
    ; At UIMessage entry: stack = [ret_addr] [msgid] [wParam] [lParam]
    ; Our JMP replaces first 5 bytes, so we enter UISnifferProc with same stack
    ; After pushfd: stack = [flags] [ret_addr] [msgid] [wParam] [lParam]  (+4)
    ; After pushad: stack = [8 regs] [flags] [ret_addr] [msgid] [wParam] [lParam]  (+36)
    ; So: ret_addr = [esp+24h], msgid = [esp+28h], wParam = [esp+2Ch], lParam = [esp+30h]

    ; Read msgid
    _('mov eax,dword[esp+28]')

    ; FILTER: only capture messages >= 0x10000000 (skip low-level input/render)
    ; Use AND to check if top nibble is set
    _('test eax,eax')
    _('jz UISnifferSkip')           ; skip msgid=0
    _('cmp eax,10000000')           ; compare against 0x10000000
    _('jb UISnifferSkip')           ; skip if below (rendering/input messages)

    ; Save msgid
    _('mov dword[UISnifferMsgId],eax')

    ; Save lParam
    _('mov eax,dword[esp+30]')
    _('mov dword[UISnifferLParam],eax')

    ; Save wParam value (raw pointer or value — AutoIt reads the pointed-to data)
    _('mov eax,dword[esp+2C]')
    _('mov dword[UISnifferWParam],eax')

    _('UISnifferNoWParam:')
    ; Increment counter
    _('mov eax,dword[UISnifferCounter]')
    _('inc eax')
    _('mov dword[UISnifferCounter],eax')

    ; Notify AutoIt via PostMessage(hwnd, event, counter, msgid)
    _('push dword[UISnifferMsgId]')           ; lParam = msgid value
    _('push dword[UISnifferCounter]')         ; wParam = counter
    _('push dword[UISnifferCallbackEvent]')   ; msg = 0x502
    _('push dword[UISnifferCallbackHandle]')  ; hwnd
    _('call dword[PostMessage]')

    _('UISnifferSkip:')
    _('popad')
    _('popfd')

    ; Execute the original prologue bytes that were overwritten by the JMP
    ; These are read at runtime and patched in by MemoryWriteDetourEx
    ; We need to replicate them here. The original bytes are saved in $detours_map.
    ;
    ; For UIMessage, the prologue is typically: push ebp / mov ebp,esp / ...
    ; We'll use a NOP sled that gets patched at enable time.
    ; Actually — MemoryWriteDetourEx saves the original bytes and restores on disable.
    ; The trampoline needs to execute them. Let me use a different approach:
    ; Read the first 5 bytes at runtime and write them into a code region.

    ; PLACEHOLDER: These 5 bytes will be patched with the real prologue at enable time
    _('UISnifferOriginal:')
    _('nop')
    _('nop')
    _('nop')
    _('nop')
    _('nop')

    ; Jump back to UIMessage + 5
    _('ljmp UISnifferReturn')

    ; UISnifferReturn label is set in ExtendScanner_UISniffer
EndFunc

; =============================================================================
; Runtime Control
; =============================================================================

;~ Initialize the sniffer (called after injection is complete)
Func UISnifferInit()
    ; Create a hidden GUI window for receiving PostMessage notifications
    Local $snifferGUI = GUICreate('UISnifferGUI')
    GUIRegisterMsg(0x00000502, '_UISnifferCallback')

    ; Write the GUI handle into shared memory so the ASM hook can PostMessage to it
    MemoryWrite(GetProcessHandle(), GetLabel('UISnifferCallbackHandle'), $snifferGUI)

    ; Cache addresses for fast reading
    $g_UISniffer_MsgId_Addr = GetLabel('UISnifferMsgId')
    $g_UISniffer_WParam_Addr = GetLabel('UISnifferWParam')
    $g_UISniffer_LParam_Addr = GetLabel('UISnifferLParam')
    $g_UISniffer_Counter_Addr = GetLabel('UISnifferCounter')

    ConsoleWrite('[UISniffer] Initialized. CallbackHandle=' & $snifferGUI & @CRLF)
EndFunc

;~ Enable the sniffer hook
Func UISnifferEnable()
    If $g_UISnifferEnabled Then Return

    ; Read the original 5 bytes at UIMessageStart
    Local $processHandle = GetProcessHandle()
    Local $uiMsgStart = GetLabel('UIMessageStart')
    Local $origBytes = DllStructCreate('byte[5]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', $uiMsgStart, _
        'ptr', DllStructGetPtr($origBytes), 'ulong_ptr', 5, 'ulong_ptr*', 0)

    ; Write the original bytes into our UISnifferOriginal code region
    Local $origAddr = GetLabel('UISnifferOriginal')
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', $origAddr, _
        'ptr', DllStructGetPtr($origBytes), 'ulong_ptr', 5, 'ulong_ptr*', 0)

    ; Enable the sniffer flag in shared memory
    MemoryWrite($processHandle, GetLabel('UISnifferEnabled'), 1, 'dword')

    ; Write the JMP detour
    MemoryWriteDetourEx('UIMessageStart', 'UISnifferProc')

    $g_UISnifferEnabled = True
    ConsoleWrite('[UISniffer] ENABLED — hook active' & @CRLF)
EndFunc

;~ Disable the sniffer hook
Func UISnifferDisable()
    If Not $g_UISnifferEnabled Then Return

    ; Revert the detour (restores original bytes)
    MemoryRevertDetour('UIMessageStart')

    ; Disable the flag
    MemoryWrite(GetProcessHandle(), GetLabel('UISnifferEnabled'), 0, 'dword')

    $g_UISnifferEnabled = False
    ConsoleWrite('[UISniffer] DISABLED — hook removed' & @CRLF)
EndFunc

;~ Set callback function for UI messages
Func UISnifferSetCallback($funcName)
    $g_UISnifferLogFunc = $funcName
EndFunc

; =============================================================================
; AutoIt Callback (receives PostMessage from ASM hook)
; =============================================================================

Func _UISnifferCallback($hWnd, $iMsg, $wParam, $lParam)
    ; wParam = counter, lParam = msgid value
    Local $msgId = $lParam
    Local $counter = $wParam

    ; Skip duplicates
    If $counter = $g_UISniffer_LastCounter Then Return 0
    $g_UISniffer_LastCounter = $counter

    ; Read wParam pointer from shared memory
    Local $wParamPtr = MemoryRead(GetProcessHandle(), $g_UISniffer_WParam_Addr, 'dword')

    ; Read lParam from shared memory
    Local $lParamVal = MemoryRead(GetProcessHandle(), $g_UISniffer_LParam_Addr, 'dword')

    ; If wParam is a pointer, try to read the first 4 dwords it points to
    Local $wp0 = 0, $wp1 = 0, $wp2 = 0, $wp3 = 0
    If $wParamPtr > 0x10000 Then ; looks like a valid pointer
        $wp0 = MemoryRead(GetProcessHandle(), $wParamPtr, 'dword')
        $wp1 = MemoryRead(GetProcessHandle(), $wParamPtr + 4, 'dword')
        $wp2 = MemoryRead(GetProcessHandle(), $wParamPtr + 8, 'dword')
        $wp3 = MemoryRead(GetProcessHandle(), $wParamPtr + 12, 'dword')
    EndIf

    ; Format message
    Local $msgHex = '0x' & Hex($msgId, 8)
    Local $logLine = '[UISniffer] #' & $counter & ' MsgID=' & $msgHex & _
        ' wP=0x' & Hex($wParamPtr, 8) & ' [' & Hex($wp0,8) & ',' & Hex($wp1,8) & ',' & Hex($wp2,8) & ',' & Hex($wp3,8) & ']' & _
        ' lP=' & $lParamVal

    ; Output to console and optionally to Froggy GUI
    ConsoleWrite($logLine & @CRLF)
    If $g_UISnifferLogFunc <> '' Then Call($g_UISnifferLogFunc, $msgHex, $wp0, $wp1, $wp2, $wp3, $lParamVal)

    Return 0
EndFunc
