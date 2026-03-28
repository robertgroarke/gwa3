#include-once

; =============================================================================
; GWA2_FrameUI.au3
;
; Programmatic interaction with GW's frame-based UI system.
; Provides GetFrameByHash, ButtonClick, and char select helpers.
;
; Uses frame hashes (uint32) to identify UI elements.
; Known hashes from py4gw frame_aliases.json:
;   Play button:       184818986
;   Play (greyed):     41327607
;   Reconnect YES:     1398610279
;   Reconnect NO:      3600335809
;   Create button:     3372446797
;   Delete button:     3379687503
;   Log Out button:    1117342925
;   Character Frame:   828467986
; =============================================================================

; Extension flag
Global $g_b_FrameUI = True

; Frame struct offsets (from GWCA UIMgr.h, Frame is 0x1C8 bytes)
Global Const $FRAME_OFFSET_CHILD_OFFSET_ID = 0xB8
Global Const $FRAME_OFFSET_FRAME_ID = 0xBC
Global Const $FRAME_OFFSET_RELATION = 0x128
Global Const $FRAME_OFFSET_HASH_ID = 0x134  ; FrameRelation.frame_hash_id = 0x128 + 0x0C
Global Const $FRAME_OFFSET_STATE = 0x18C

; Frame state flags
Global Const $FRAME_STATE_CREATED = 0x4
Global Const $FRAME_STATE_HIDDEN = 0x200
Global Const $FRAME_STATE_DISABLED = 0x10

; Known frame hashes for char select
Global Const $FRAME_HASH_PLAY_BUTTON = 184818986
Global Const $FRAME_HASH_PLAY_GREYED = 41327607
Global Const $FRAME_HASH_RECONNECT_YES = 1398610279
Global Const $FRAME_HASH_RECONNECT_NO = 3600335809
Global Const $FRAME_HASH_CREATE_BUTTON = 3372446797
Global Const $FRAME_HASH_DELETE_BUTTON = 3379687503
Global Const $FRAME_HASH_LOGOUT_BUTTON = 1117342925
Global Const $FRAME_HASH_CHARACTER_FRAME = 828467986
Global Const $FRAME_HASH_EDIT_ACCOUNT = 1601494406

; =============================================================================
; Extension Points (called by BotsHub framework during initialization)
; =============================================================================

;~ No-op: patterns are scanned in ExtendScanner_FrameUI instead
Func ExtendAddPattern_FrameUI()
EndFunc

;~ Scan for SendFrameUIMessage game function
;~ Called during Extend_Scanner phase (after main scan completes)
Func ExtendScanner_FrameUI()
    ; SendFrameUIMessage is found by scanning for byte pattern 83 C1 DC E8
    ; At offset +3, there's a CALL instruction (E8 xx xx xx xx)
    ; We resolve the call target to get the actual function address
    ;
    ; Since we can't add patterns to the main scan (no extension point),
    ; we scan the .text section ourselves from AutoIt

    Local $processHandle = GetProcessHandle()
    Local $textStart = $pe_sections_ranges[0][0]
    Local $textEnd = $pe_sections_ranges[0][1]
    Local $textSize = $textEnd - $textStart

    ConsoleWrite('[FrameUI] Scanning for SendFrameUIMsg in .text (0x' & Hex($textStart) & '-0x' & Hex($textEnd) & ')...' & @CRLF)

    ; Pattern: 83 C1 DC E8 (add ecx, -0x24; call ...)
    ; Read in 64KB chunks
    Local $chunkSize = 65536
    Local $found = False
    For $offset = 0 To $textSize - 8 Step $chunkSize
        Local $readSize = $chunkSize + 8
        If $offset + $readSize > $textSize Then $readSize = $textSize - $offset

        Local $chunk = DllStructCreate('byte[' & $readSize & ']')
        DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
            'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
            'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)

        For $j = 1 To $readSize - 7
            If DllStructGetData($chunk, 1, $j) = 0x83 And _
               DllStructGetData($chunk, 1, $j+1) = 0xC1 And _
               DllStructGetData($chunk, 1, $j+2) = 0xDC And _
               DllStructGetData($chunk, 1, $j+3) = 0xE8 Then

                ; Found the pattern. The E8 at +3 is a CALL instruction.
                Local $callAddr = $textStart + $offset + ($j - 1) + 3
                ; Read the relative offset
                Local $relBytes = DllStructCreate('int')
                DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
                    'handle', $processHandle, 'ptr', Ptr($callAddr + 1), _
                    'ptr', DllStructGetPtr($relBytes), 'ulong_ptr', 4, 'ulong_ptr*', 0)
                Local $rel = DllStructGetData($relBytes, 1)
                Local $funcAddr = $callAddr + 5 + $rel

                SetLabel('SendFrameUIMsg', Ptr($funcAddr))
                ; NOTE: Can't write to shared memory yet — data labels not allocated until after assembly
                ; The func ptr will be written lazily in ClickFrameButton() on first use
                ConsoleWrite('[FrameUI] SendFrameUIMsg: 0x' & Hex($funcAddr) & ' (call at 0x' & Hex($callAddr) & ')' & @CRLF)
                $found = True
                ExitLoop 2
            EndIf
        Next
    Next

    If Not $found Then
        ConsoleWrite('[FrameUI] WARNING: SendFrameUIMsg not found!' & @CRLF)
    EndIf
EndFunc

;~ Allocate shared memory for frame click data
Func ExtendAssemblerData_FrameUI()
    _('FrameClickFuncPtr/4')     ; Pointer to game's SendFrameUIMessage function
    _('FrameClickFramePtr/4')    ; Frame* to click
    _('FrameClickMsgId/4')       ; UIMessage to send (0x31 = kMouseClick2)
    _('FrameClickActionPtr/4')   ; Pointer to FrameClickAction (set during init)
    _('FrameClickAction/32')     ; kMouseAction struct (8 dwords, matches GWCA MouseAction)
    _('FrameClickResult/4')      ; Result flag
EndFunc

;~ Assemble the frame click command handler
Func ExtendAssembler_FrameUI()
    ; CommandFrameClick: calls game's SendFrameUIMessage via __thiscall
    ; Game function signature: __thiscall SendFrameUIMsg(this=&frame[0xA8], msgid, wParam, lParam)
    ; ECX = frame_ptr + 0xA8 (callback array pointer)
    ; Stack: msgid, wParam (ptr to action struct), lParam (0)
    ;
    ; Command struct in EAX:
    ;   [0]  = CommandFrameClick address
    ;   [4]  = Frame* pointer
    ;   [8]  = UIMessage id (0x31 for kMouseClick2)
    ;   [12] = kMouseAction.frame_id
    ;   [16] = kMouseAction.child_offset_id
    ;   [20] = kMouseAction.action_state (0x8 = MouseClick)
    ;   [24] = kMouseAction.wparam (0)
    ;   [28] = kMouseAction.lparam (0)
    ; CommandFrameClick: calls game's SendFrameUIMessage via __thiscall
    ; ECX = frame_ptr + 0x84 (game does: ecx=frame+0xA8, add ecx,-0x24)
    ; Stack: msgid, wParam (ptr to action struct), lParam (0)
    ;
    ; Command struct:
    ;   [0]  = CommandFrameClick address
    ;   [4]  = Frame* pointer
    ;   [8]  = UIMessage id (0x31 for kMouseClick2)
    ;   [12] = kMouseAction.frame_id
    ;   [16] = kMouseAction.child_offset_id
    ;   [20] = kMouseAction.action_state (0x8 = MouseClick)
    ;   [24] = kMouseAction.wparam (0)
    ;   [28] = kMouseAction.lparam (0)
    _('CommandFrameClick:')
    ; Read from persistent shared memory (not the zeroed queue entry)
    _('mov ecx,dword[FrameClickFramePtr]')  ; ecx = Frame*
    _('add ecx,A8')                          ; ecx = frame + 0xA8 (callbacks array = this ptr)
    _('push 0')                              ; lParam = NULL
    _('push dword[FrameClickActionPtr]')     ; wParam = address of kMouseAction struct
    _('push dword[FrameClickMsgId]')         ; msgid (0x31)
    _('call dword[FrameClickFuncPtr]')       ; indirect call (__thiscall, callee cleans stack)
    ; Write 1 to FrameClickResult to confirm execution
    _('mov dword[FrameClickResult],1')
    _('ljmp CommandReturn')
EndFunc

; Cached calibrated address for CommandFrameClick
Global $g_FrameClick_CalibratedAddr = 0

;~ Calibrate the CommandFrameClick ASM address (workaround for assembler .5 offset)
;~ Scans near the label for the instruction signature: 8B 48 04 (mov ecx,[eax+4])
;~ followed by 81 C1 A8 00 00 00 (add ecx, 0xA8)
Func _CalibrateFrameClickAddr($labelAddr)
    If $g_FrameClick_CalibratedAddr <> 0 Then Return $g_FrameClick_CalibratedAddr

    Local $processHandle = GetProcessHandle()
    Local $scanBuf = DllStructCreate('byte[24]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($labelAddr - 8), _
        'ptr', DllStructGetPtr($scanBuf), 'ulong_ptr', 24, 'ulong_ptr*', 0)

    ; Look for 8B 0D (mov ecx, [imm32]) followed by 81 C1 A8 (add ecx, 0xA8)
    ; First instruction: mov ecx,dword[FrameClickFramePtr] = 8B 0D xx xx xx xx
    ; Second instruction: add ecx,A8 = 81 C1 A8 00 00 00
    For $i = 1 To 15
        If DllStructGetData($scanBuf, 1, $i) = 0x8B And _
           DllStructGetData($scanBuf, 1, $i+1) = 0x0D Then
            ; Verify: 6 bytes later should be 81 C1 A8 (add ecx, 0xA8)
            If $i + 8 <= 24 And _
               DllStructGetData($scanBuf, 1, $i+6) = 0x81 And _
               DllStructGetData($scanBuf, 1, $i+7) = 0xC1 And _
               DllStructGetData($scanBuf, 1, $i+8) = 0xA8 Then
                $g_FrameClick_CalibratedAddr = $labelAddr - 8 + ($i - 1)
                ConsoleWrite('[FrameUI] CommandFrameClick calibrated: label=0x' & Hex($labelAddr) & _
                    ' actual=0x' & Hex($g_FrameClick_CalibratedAddr) & _
                    ' offset=' & ($g_FrameClick_CalibratedAddr - $labelAddr) & @CRLF)
                Return $g_FrameClick_CalibratedAddr
            EndIf
        EndIf
    Next

    ; Fallback: use label address as-is
    ConsoleWrite('[FrameUI] WARNING: Could not calibrate CommandFrameClick, using label addr' & @CRLF)
    $g_FrameClick_CalibratedAddr = $labelAddr
    Return $labelAddr
EndFunc

; =============================================================================
; Frame Lookup Functions
; =============================================================================

;~ Find a frame by its hash value. Returns [frame_ptr, frame_id] or [0, 0] if not found.
Func GetFrameByHash($hash)
    Local $processHandle = GetProcessHandle()
    Local $frameArrayAddr = GetLabel('FrameArray')

    ; Read frame array: buffer_ptr, size
    Local $bufferPtr = MemoryRead($processHandle, $frameArrayAddr, 'dword')
    Local $arraySize = MemoryRead($processHandle, $frameArrayAddr + 4, 'dword')

    If $arraySize > 5000 Or $arraySize <= 0 Or $bufferPtr < 0x10000 Then
        Local $ret[2] = [0, 0]
        Return $ret
    EndIf

    ; Walk frames looking for matching hash at offset 0x134
    For $i = 0 To $arraySize - 1
        Local $framePtr = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
        If $framePtr = 0 Or $framePtr < 0x10000 Then ContinueLoop

        Local $frameHash = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_HASH_ID, 'dword')
        If $frameHash = $hash Then
            Local $frameId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_FRAME_ID, 'dword')
            Local $ret[2] = [$framePtr, $frameId]
            Return $ret
        EndIf
    Next

    Local $ret[2] = [0, 0]
    Return $ret
EndFunc

;~ Check if a frame exists and is visible (created + not hidden)
Func IsFrameVisible($hash)
    Local $result = GetFrameByHash($hash)
    If $result[0] = 0 Then Return False

    Local $state = MemoryRead(GetProcessHandle(), $result[0] + $FRAME_OFFSET_STATE, 'dword')
    Return (BitAND($state, $FRAME_STATE_CREATED) <> 0) And (BitAND($state, $FRAME_STATE_HIDDEN) = 0)
EndFunc

;~ Check if a frame exists (regardless of visibility)
Func FrameExists($hash)
    Local $result = GetFrameByHash($hash)
    Return ($result[0] <> 0)
EndFunc

; =============================================================================
; Button Click Functions
; =============================================================================

;~ Click a button frame by its hash. Uses SendFrameUIMessage with kMouseClick2.
;~ This replicates what GWCA ButtonFrame::Click() does internally.
Func ClickFrameButton($hash)
    Local $result = GetFrameByHash($hash)
    If $result[0] = 0 Then
        ConsoleWrite('[FrameUI] Button hash ' & $hash & ' not found' & @CRLF)
        Return False
    EndIf

    Local $framePtr = $result[0]
    Local $frameId = $result[1]
    Local $processHandle = GetProcessHandle()

    ; Check frame state
    Local $state = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_STATE, 'dword')
    If BitAND($state, $FRAME_STATE_CREATED) = 0 Then
        ConsoleWrite('[FrameUI] Button hash ' & $hash & ' not created' & @CRLF)
        Return False
    EndIf

    ; Read frame_id and child_offset_id for the mouse action packet
    Local $childOffsetId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')

    ; Build kMouseAction packet and send via CommandUIMsg
    ; kMouseClick2 = 0x31, ActionState::MouseClick = 0x8
    ; The packet struct for kMouseAction:
    ;   uint32_t frame_id
    ;   uint32_t child_offset_id
    ;   ActionState current_state (0x8 = MouseClick)
    ;   void* wparam (0)
    ;   void* lparam (0)

    ; Use SendUIMessage with kMouseClick2 (0x31) but this is a FRAME message
    ; We need SendFrameUIMessage, not SendUIMessage
    ; For now, use the CommandUIMsg which calls SendUIMessage(msgid, wParam, 0)
    ; We'll build the wParam struct in shared memory

    ; Actually, looking at the MouseAction code from gwca.dll:
    ; It pushes: 0, &packet, 0x31, frame_context_ptr and calls SendFrameUIMessage
    ; SendFrameUIMessage takes: Frame*, UIMessage, void*, void*
    ; This requires calling a game function we haven't hooked yet.

    ; ALTERNATIVE: Use the simpler approach from py4gw:
    ; PyUIManager.UIManager.button_click(frame_id)
    ; This calls GW::ButtonFrame::Click() which internally:
    ;   1. Gets frame context
    ;   2. Builds mouse action struct
    ;   3. Calls SendFrameUIMessage(context, kMouseClick2, &action, 0)

    ; Since we need SendFrameUIMessage (a game function), and we don't have it hooked yet,
    ; let's use a workaround: write the mouse action directly to memory and trigger it
    ; through the game's own processing.

    ; Actually, the SIMPLEST approach: SendUIMessage (global) with kMouseClick2
    ; does go through the frame system! Let me try that.

    ; Build the kMouseAction struct
    ; struct kMouseAction {
    ;   uint32_t frame_id;        // +0
    ;   uint32_t child_offset_id; // +4
    ;   ActionState current_state;// +8 (0x8 = MouseClick)
    ;   void* wparam;             // +12 (0)
    ;   void* lparam;             // +16 (0)
    ; };

    ; We'll write this struct to our shared data area and pass its address as wParam
    ; to SendUIMessage(0x31, &mouseAction, 0)

    ; Use CommandUIMsg: it calls SendUIMessage(msgid, wParam, 0)
    ; where wParam = &data[8] in the command struct
    ; So we write: [CommandUIMsg_addr] [0x31] [frame_id] [child_offset_id] [0x8] [0] [0]

    Local $cmdAddr = GetLabel('CommandUIMsg')
    Local $queueSize = GetLabel('QueueSize')
    Local $queueBase = GetLabel('QueueBase')

    ; Use CommandFrameClick ASM stub which calls game's SendFrameUIMessage
    ; Game function is __thiscall: ECX = &frame->callbacks (frame_ptr + 0xA8)
    ; Stack params: push lParam, push wParam, push msgid, call [SendFrameUIMsg]
    ;
    ; Command struct: [4: CommandFrameClick] [4: frame_ptr] [4: msgid]
    ;                 [20: kMouseAction = {frame_id, child_offset_id, state, wp, lp}]

    ; Lazy init: write SendFrameUIMsg function pointer and action ptr on first call
    Local $sendFrameFunc = Int(GetLabel('SendFrameUIMsg'))
    If $sendFrameFunc = 0 Or $sendFrameFunc = -1 Then
        ConsoleWrite('[FrameUI] SendFrameUIMsg not found — cannot click' & @CRLF)
        Return False
    EndIf

    Local $funcPtrAddr = GetLabel('FrameClickFuncPtr')
    Local $currentFuncPtr = MemoryRead(GetProcessHandle(), $funcPtrAddr, 'dword')
    If $currentFuncPtr = 0 Then
        MemoryWrite(GetProcessHandle(), $funcPtrAddr, $sendFrameFunc, 'dword')
        MemoryWrite(GetProcessHandle(), GetLabel('FrameClickActionPtr'), Int(GetLabel('FrameClickAction')), 'dword')
        ConsoleWrite('[FrameUI] Initialized: FuncPtr=0x' & Hex($sendFrameFunc) & _
            ' ActionPtr=0x' & Hex(Int(GetLabel('FrameClickAction'))) & @CRLF)
    EndIf

    ; Write action data to PERSISTENT shared memory (not the queue, which gets zeroed)
    ; The main loop zeroes the queue entry before dispatching the command handler,
    ; so any pointers into the queue entry would be invalid by the time the game
    ; function reads the wParam data.
    Local $processHandle = GetProcessHandle()

    ; Write Frame* to FrameClickFramePtr
    MemoryWrite($processHandle, GetLabel('FrameClickFramePtr'), Int($framePtr), 'dword')
    ; Write msgid (0x31 = kMouseClick2, used by GWCA's ButtonFrame::Click)
    MemoryWrite($processHandle, GetLabel('FrameClickMsgId'), 0x31, 'dword')
    ; Write kMouseAction struct (8 dwords = 32 bytes)
    ; Layout from GWCA MouseAction code at gwca.dll 0x100173D0:
    ;   [0x00] frame_id        (from frame+0xBC)
    ;   [0x04] child_offset_id (from frame+0xB8)
    ;   [0x08] action_state    (0x8 = MouseClick)
    ;   [0x0C] 0
    ;   [0x10] 0
    ;   [0x14] 0
    ;   [0x18] frame[0x1C4]
    ;   [0x1C] 0
    Local $actionBase = GetLabel('FrameClickAction')
    Local $field1C4 = MemoryRead($processHandle, $framePtr + 0x1C4, 'dword')
    MemoryWrite($processHandle, $actionBase + 0, $frameId, 'dword')
    MemoryWrite($processHandle, $actionBase + 4, $childOffsetId, 'dword')
    MemoryWrite($processHandle, $actionBase + 8, 0x8, 'dword')   ; MouseClick
    MemoryWrite($processHandle, $actionBase + 12, 0, 'dword')
    MemoryWrite($processHandle, $actionBase + 16, 0, 'dword')
    MemoryWrite($processHandle, $actionBase + 20, 0, 'dword')
    MemoryWrite($processHandle, $actionBase + 24, $field1C4, 'dword')
    MemoryWrite($processHandle, $actionBase + 28, 0, 'dword')

    ; Use CommandUIMsg (proven working) with kMouseClick2 (0x31)
    ; CommandUIMsg calls SendUIMessage(msgid, &data[8], 0)
    ; The global SendUIMessage should route kMouseClick2 to the frame by frame_id
    Local $field1C4 = MemoryRead($processHandle, $framePtr + 0x1C4, 'dword')
    Local $struct = DllStructCreate('dword;dword;dword;dword;dword;dword;dword;dword;dword;dword')
    DllStructSetData($struct, 1, GetLabel('CommandUIMsg'))
    DllStructSetData($struct, 2, 0x31)           ; kMouseClick2
    DllStructSetData($struct, 3, $frameId)        ; action.frame_id
    DllStructSetData($struct, 4, $childOffsetId)  ; action.child_offset_id
    DllStructSetData($struct, 5, 0x8)             ; MouseClick state
    DllStructSetData($struct, 6, 0)
    DllStructSetData($struct, 7, 0)
    DllStructSetData($struct, 8, 0)
    DllStructSetData($struct, 9, $field1C4)       ; frame[0x1C4]
    DllStructSetData($struct, 10, 0)
    Enqueue(DllStructGetPtr($struct), DllStructGetSize($struct))

    ConsoleWrite('[FrameUI] Clicked via CommandUIMsg: hash=' & $hash & ' frame_id=' & $frameId & _
        ' child_off=' & $childOffsetId & ' 1C4=0x' & Hex($field1C4) & @CRLF)
    Return True
EndFunc

; =============================================================================
; Char Select Helpers
; =============================================================================

;~ Check if we're at the character select screen
Func IsAtCharSelect()
    Return IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Or IsFrameVisible($FRAME_HASH_PLAY_GREYED)
EndFunc

;~ Check if the reconnect dialog is showing
Func IsReconnectDialogShowing()
    Return IsFrameVisible($FRAME_HASH_RECONNECT_YES)
EndFunc

;~ Press Play at character select
Func PressPlayButton()
    If Not IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
        ConsoleWrite('[FrameUI] Play button not visible' & @CRLF)
        Return False
    EndIf
    Return ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
EndFunc

;~ Dismiss reconnect dialog with Yes or No
Func DismissReconnectDialog($choice = 'no')
    If Not IsReconnectDialogShowing() Then
        ConsoleWrite('[FrameUI] No reconnect dialog showing' & @CRLF)
        Return False
    EndIf

    If $choice = 'yes' Then
        Return ClickFrameButton($FRAME_HASH_RECONNECT_YES)
    Else
        Return ClickFrameButton($FRAME_HASH_RECONNECT_NO)
    EndIf
EndFunc
