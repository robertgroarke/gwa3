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

; Cached state
Global $g_FrameClick_CalibratedAddr = 0
Global $g_FrameClick_ShellcodeAddr = 0
Global $g_FrameClick_ActionDataAddr = 0
Global $g_GWCA_ModuleBase = 0
Global $g_GWCA_Initialized = False

; GWCA function RVA offsets (from gwca.dll binary analysis)
Global Const $GWCA_RVA_BUTTONCLICK = 0x255E0
Global Const $GWCA_RVA_CLICK = 0x16660
Global Const $GWCA_RVA_GETFRAMEBYID = 0x25CC0
; GWCA data section offsets for function pointers
Global Const $GWCA_DATA_SENDFRAME_ORIG = 0x8A39C
Global Const $GWCA_DATA_SENDFRAME_HOOK = 0x8A3A0  ; CRITICAL — wrapper returns false if NULL
Global Const $GWCA_DATA_GETCHILDFRAME = 0x8A37C
Global Const $GWCA_DATA_ROOTFRAME = 0x8A410
Global Const $GWCA_DATA_FRAMEHASHTBL = 0x8A3B0
; Game function offsets from game base (consistent across instances)
Global Const $GAME_OFF_SENDFRAMEUIMSG = 0x2286D0
Global Const $GAME_OFF_GETCHILDFRAME = 0x20E2B0
Global Const $GAME_OFF_ROOTFRAME = 0x22DC20

;~ Write a little-endian 32-bit value into a DllStruct at position $pos
Func _WriteLE32(ByRef $struct, $pos, $value)
    DllStructSetData($struct, 1, BitAND($value, 0xFF), $pos)
    DllStructSetData($struct, 1, BitAND(BitShift($value, 8), 0xFF), $pos + 1)
    DllStructSetData($struct, 1, BitAND(BitShift($value, 16), 0xFF), $pos + 2)
    DllStructSetData($struct, 1, BitAND(BitShift($value, 24), 0xFF), $pos + 3)
EndFunc

;~ Inject gwca.dll and populate its data section with game function pointers.
;~ This enables GWCA's ButtonClick to work without calling GW::Initialize
;~ (which would break our rendering hook).
Func _InitGWCAForButtonClick()
    Local $processHandle = GetProcessHandle()
    Local $gwPID = $game_clients[$game_clients[0][0]][0]
    Local $gwBase = $pe_sections_ranges[0][0] - 0x1000

    ; Check if gwca.dll is already loaded
    $g_GWCA_ModuleBase = _FindModule($gwPID, "gwca.dll")
    If $g_GWCA_ModuleBase = 0 Then
        ; Inject gwca.dll
        ConsoleWrite('[FrameUI] Injecting gwca.dll...' & @CRLF)
        Local $dllPath = @ScriptDir & "\..\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
        If Not FileExists($dllPath) Then
            $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
        EndIf
        Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
        Local $pathLen = BinaryLen($dllPathW)

        Local $rp = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
            'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
            'dword', 0x1000, 'dword', 0x04)
        If Not IsArray($rp) Or $rp[0] = 0 Then Return False

        Local $pb = DllStructCreate('byte[' & $pathLen & ']')
        DllStructSetData($pb, 1, $dllPathW)
        DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
            'handle', $processHandle, 'ptr', $rp[0], _
            'ptr', DllStructGetPtr($pb), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)

        Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
        Local $ll = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')
        Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
            'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
            'ptr', $ll[0], 'ptr', $rp[0], 'dword', 0, 'dword*', 0)
        If Not IsArray($th) Or $th[0] = 0 Then Return False

        DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 10000)
        Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $th[0], 'dword*', 0)
        $g_GWCA_ModuleBase = $ec[2]
        DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
        DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
            'handle', $processHandle, 'ptr', $rp[0], 'ulong_ptr', 0, 'dword', 0x8000)

        If $g_GWCA_ModuleBase = 0 Then
            ConsoleWrite('[FrameUI] ERROR: gwca.dll injection failed' & @CRLF)
            Return False
        EndIf
    EndIf
    ConsoleWrite('[FrameUI] gwca.dll at 0x' & Hex($g_GWCA_ModuleBase) & @CRLF)

    ; Populate GWCA data section with game function pointers
    ; DO NOT call GW::Initialize (it breaks our rendering hook)
    Local $sendFrameAddr = $gwBase + $GAME_OFF_SENDFRAMEUIMSG
    Local $getChildAddr = $gwBase + $GAME_OFF_GETCHILDFRAME
    Local $rootFrameAddr = $gwBase + $GAME_OFF_ROOTFRAME
    Local $frameArrayAddr = Int(GetLabel('FrameArray'))

    ; CRITICAL: write to BOTH original (+0x8A39C) AND hooked (+0x8A3A0) pointers
    ; GWCA's SendFrameUIMessage wrapper checks +0x8A3A0 and returns false if NULL
    MemoryWrite($processHandle, $g_GWCA_ModuleBase + $GWCA_DATA_SENDFRAME_ORIG, $sendFrameAddr, 'dword')
    MemoryWrite($processHandle, $g_GWCA_ModuleBase + $GWCA_DATA_SENDFRAME_HOOK, $sendFrameAddr, 'dword')
    MemoryWrite($processHandle, $g_GWCA_ModuleBase + $GWCA_DATA_GETCHILDFRAME, $getChildAddr, 'dword')
    MemoryWrite($processHandle, $g_GWCA_ModuleBase + $GWCA_DATA_ROOTFRAME, $rootFrameAddr, 'dword')
    MemoryWrite($processHandle, $g_GWCA_ModuleBase + $GWCA_DATA_FRAMEHASHTBL, $frameArrayAddr, 'dword')

    ConsoleWrite('[FrameUI] GWCA data populated (SendFrame=0x' & Hex($sendFrameAddr) & ')' & @CRLF)
    $g_GWCA_Initialized = True
    Return True
EndFunc

;~ Find a loaded module by name in a process. Returns base address or 0.
Func _FindModule($pid, $name)
    Local $sn = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
    If Not IsArray($sn) Or $sn[0] = -1 Then Return 0
    Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;' & _
        'dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
    DllStructSetData($me, 'dwSize', DllStructGetSize($me))
    Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $sn[0], 'struct*', $me)
    While IsArray($r) And $r[0]
        If StringLower(DllStructGetData($me, 'szModule')) = StringLower($name) Then
            Local $base = Int(DllStructGetData($me, 'modBaseAddr'))
            DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
            Return $base
        EndIf
        $r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $sn[0], 'struct*', $me)
    WEnd
    DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
    Return 0
EndFunc

;~ Get the frame context — replicates the function at gwca+0x25EC0
;~ (called by MouseAction, NOT the exported GetFrameContext at +0x25D90).
;~ Returns [frame+0x128] - 0x128 = the PARENT FRAME pointer.
;~ This is the context that SendFrameUIMsg needs: ECX = context + 0xA8.
Func _GetFrameContext($framePtr)
    Local $ph = GetProcessHandle()
    Local $relation = MemoryRead($ph, $framePtr + 0x128, 'dword')  ; FrameRelation*
    If $relation = 0 Or $relation < 0x10000 Then Return 0
    Return $relation - 0x128  ; parent Frame* = relation_ptr - offset_of_relation_in_Frame
EndFunc

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
Func ClickFrameButton_OLD($hash)
    ; OLD implementation — replaced by clean version below
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

    ; Write shellcode directly into the FrameClickAction data region (RWX memory)
    ; Data labels have integer offsets (no .5 error), so the address is exact.
    ; The shellcode calls SendFrameUIMsg(__thiscall) with the right parameters.
    ;
    ; Shellcode (at FrameClickAction address):
    ;   mov ecx, [FrameClickFramePtr]    ; 8B 0D <addr>
    ;   add ecx, 0xA8                    ; 81 C1 A8 00 00 00
    ;   push 0                           ; 6A 00
    ;   push FrameClickActionPtr_value   ; FF 35 <addr>  (push [addr] = push wParam ptr)
    ;   push 0x31                        ; 6A 31
    ;   call [FrameClickFuncPtr]         ; FF 15 <addr>
    ;   jmp CommandReturn                ; E9 <rel32>

    ; First, we need to write the kMouseAction data to a separate location
    ; since FrameClickAction itself will hold shellcode now.
    ; Use FrameClickResult + 4 as the action data location (we have space)

    ; Actually, let's use a simpler approach: write action data BEFORE the shellcode
    ; Put action data at FrameClickFramePtr+8 to FrameClickFramePtr+28 (FrameClickMsgId area)
    ; Then put shellcode at FrameClickAction

    ; Hmm, this is getting messy. Let me just write raw shellcode bytes into
    ; the FrameClickAction region and point the queue at it.

    Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))
    Local $framePtrAddr = Int(GetLabel('FrameClickFramePtr'))
    Local $actionAddr = Int(GetLabel('FrameClickAction'))
    Local $cmdReturnLabel = Int(GetLabel('CommandReturn'))

    ; Write the action struct data into FrameClickMsgId area (before FrameClickAction)
    ; We'll use FrameClickActionPtr as the action data pointer
    Local $actionDataAddr = Int(GetLabel('FrameClickActionPtr'))  ; reuse as data storage
    ; Actually, FrameClickActionPtr is only 4 bytes. Let me use FrameClickResult area
    ; which is 4 bytes at the end. Not enough.

    ; Better: write action data into the end of the 32-byte FrameClickAction region
    ; Shellcode takes ~25 bytes, action data at actionAddr+25 (7 bytes of space left)
    ; Not enough for 20 bytes of action data.

    ; Simplest: allocate another data region or use the existing FrameClickResult space.
    ; But we only have 4 bytes there.

    ; OK, completely different approach: use FrameClickAction (32 bytes) for the action data
    ; and write shellcode to FrameClickResult area (4 bytes is too small)

    ; The REAL simplest approach: just make the shellcode read everything from shared memory
    ; and call the game function. Write the shellcode once during init, then just
    ; update the shared memory values and queue the shellcode address each time.

    ; One-time shellcode init
    If $g_FrameClick_ShellcodeAddr = 0 Then
        ; Build shellcode bytes
        ; The shellcode region is FrameClickAction (32 bytes, RWX)
        ; Action data goes to a separate 20-byte block we carve from the 32 bytes
        ; Shellcode: ~23 bytes, leaves 9 bytes unused

        ; Actually: write action data to shared memory FIRST (FrameClickFramePtr+8..+28)
        ; Then shellcode at FrameClickAction reads from fixed addresses

        ; Let me just use VirtualAllocEx for a clean 64-byte region
        Local $shellMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
            'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 64, _
            'dword', 0x1000, 'dword', 0x40)
        If Not IsArray($shellMem) Or $shellMem[0] = 0 Then
            ConsoleWrite('[FrameUI] VirtualAllocEx for shellcode failed' & @CRLF)
            Return False
        EndIf
        $g_FrameClick_ShellcodeAddr = Int($shellMem[0])
        $g_FrameClick_ActionDataAddr = $g_FrameClick_ShellcodeAddr + 32  ; action data at +32

        ; Build shellcode that reads from shared memory
        Local $sc = DllStructCreate('byte[32]')
        Local $p = 1

        ; mov ecx, [FrameClickFramePtr]  = 8B 0D <le32>
        DllStructSetData($sc, 1, 0x8B, $p)
        $p += 1
        DllStructSetData($sc, 1, 0x0D, $p)
        $p += 1
        _WriteLE32($sc, $p, $framePtrAddr)
        $p += 4

        ; add ecx, 0xA8  = 81 C1 A8 00 00 00
        DllStructSetData($sc, 1, 0x81, $p)
        $p += 1
        DllStructSetData($sc, 1, 0xC1, $p)
        $p += 1
        _WriteLE32($sc, $p, 0xA8)
        $p += 4

        ; push 0  = 6A 00
        DllStructSetData($sc, 1, 0x6A, $p)
        $p += 1
        DllStructSetData($sc, 1, 0x00, $p)
        $p += 1

        ; push <actionDataAddr>  = 68 <le32>
        DllStructSetData($sc, 1, 0x68, $p)
        $p += 1
        _WriteLE32($sc, $p, $g_FrameClick_ActionDataAddr)
        $p += 4

        ; push 0x31  = 6A 31
        DllStructSetData($sc, 1, 0x6A, $p)
        $p += 1
        DllStructSetData($sc, 1, 0x31, $p)
        $p += 1

        ; call [FrameClickFuncPtr]  = FF 15 <le32>
        DllStructSetData($sc, 1, 0xFF, $p)
        $p += 1
        DllStructSetData($sc, 1, 0x15, $p)
        $p += 1
        _WriteLE32($sc, $p, Int(GetLabel('FrameClickFuncPtr')))
        $p += 4

        ; jmp CommandReturn  = E9 <rel32>
        ; But CommandReturn label has .5 error... use calibrated address
        ; Actually, RegularFlow section of MainProc runs AFTER the handler returns
        ; via ljmp CommandReturn. The handler should just RET or JMP to CommandReturn.
        ; BUT: we entered via jmp ebx (no return address on stack), so we can't RET.
        ; We need to JMP to CommandReturn.
        ; CommandReturn label has .5 offset. We need to calibrate it too.
        ; For now, just use the raw label value (it's close enough or use NOP sled)

        ; Actually, I realize: the other commands all use `ljmp CommandReturn`
        ; which resolves during assembly. Since our shellcode is NOT in the assembly,
        ; we need to compute the jump target ourselves.

        ; CommandReturn is in the MainProc code. Its label should have the same .5 offset
        ; as all other code labels. But since we're jumping FROM allocated memory
        ; (not from the assembly region), the relative offset calculation is different.

        ; Just use an absolute JMP: FF 25 <addr> where addr points to CommandReturn address
        ; But we don't have the calibrated CommandReturn address...

        ; Simplest: just RET from the shellcode. The main loop did `jmp ebx` (no CALL),
        ; so there's no return address. We need to somehow return to the main loop.
        ; The main loop after processing: goes to MainExit.

        ; Actually, looking at how existing commands work: they use `ljmp CommandReturn`
        ; which is a JMP (not RET). CommandReturn then:
        ;   mov ecx,[SavedIndex]
        ;   mov edx,[QueueCounter]
        ;   ...
        ;   jmp MainExit

        ; For our shellcode, we need to jump there. But we don't have the address.
        ; UNLESS we store it in shared memory.

        ; Store CommandReturn address in a known location
        ; Use FrameClickResult (4 bytes) to store the calibrated CommandReturn addr
        ; ... but CommandReturn also has the .5 offset issue

        ; FOR NOW: just do a tight infinite-avoidance by jumping to MainExit
        ; which the main loop eventually reaches anyway. OR:
        ; Store the return address in shared memory before enqueuing.

        ; Actually, the simplest: ret 0 — the main loop pushes nothing, so
        ; the stack frame from before the main loop gets popped... that would crash.

        ; OK let me try: don't jump anywhere, just crash gracefully...
        ; NO. Let me store CommandReturn address.

        ; Write CommandReturn address to FrameClickResult
        ; CommandReturn has .5 offset. Use calibration on it.
        Local $crLabel = Int(GetLabel('CommandReturn'))
        ; CommandReturn starts with: mov ecx,[SavedIndex] = 8B 0D <addr>
        ; Scan near the label for this signature
        Local $crBuf = DllStructCreate('byte[16]')
        DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
            'handle', $processHandle, 'ptr', Ptr($crLabel - 8), _
            'ptr', DllStructGetPtr($crBuf), 'ulong_ptr', 16, 'ulong_ptr*', 0)
        Local $crActual = $crLabel
        For $ci = 1 To 13
            If DllStructGetData($crBuf, 1, $ci) = 0x8B And _
               DllStructGetData($crBuf, 1, $ci+1) = 0x0D Then
                $crActual = $crLabel - 8 + ($ci - 1)
                ExitLoop
            EndIf
        Next
        MemoryWrite($processHandle, GetLabel('FrameClickResult'), $crActual, 'dword')

        ; Now add JMP to CommandReturn via [FrameClickResult]
        DllStructSetData($sc, 1, 0xFF, $p)
        $p += 1
        DllStructSetData($sc, 1, 0x25, $p)  ; JMP [imm32]
        $p += 1
        _WriteLE32($sc, $p, Int(GetLabel('FrameClickResult')))
        $p += 4

        ; Write shellcode to allocated memory
        DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
            'handle', $processHandle, 'ptr', Ptr($g_FrameClick_ShellcodeAddr), _
            'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p - 1, 'ulong_ptr*', 0)

        ConsoleWrite('[FrameUI] Shellcode at 0x' & Hex($g_FrameClick_ShellcodeAddr) & _
            ' ActionData at 0x' & Hex($g_FrameClick_ActionDataAddr) & _
            ' CommandReturn at 0x' & Hex($crActual) & @CRLF)
    EndIf

    ; Write Frame* to shared memory
    MemoryWrite($processHandle, GetLabel('FrameClickFramePtr'), Int($framePtr), 'dword')

    ; Click = MouseDown (0x6) then MouseUp (0x7) — two separate calls
    ; Send MouseDown first
    Local $actionData = DllStructCreate('dword;dword;dword;dword;dword')
    DllStructSetData($actionData, 1, $frameId)
    DllStructSetData($actionData, 2, $childOffsetId)
    DllStructSetData($actionData, 3, 0x6)   ; MouseDown
    DllStructSetData($actionData, 4, 0)
    DllStructSetData($actionData, 5, 0)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($g_FrameClick_ActionDataAddr), _
        'ptr', DllStructGetPtr($actionData), 'ulong_ptr', 20, 'ulong_ptr*', 0)

    Local $struct = DllStructCreate('dword;dword')
    DllStructSetData($struct, 1, $g_FrameClick_ShellcodeAddr)
    DllStructSetData($struct, 2, 0)
    Enqueue(DllStructGetPtr($struct), DllStructGetSize($struct))
    Sleep(100)

    ; Send MouseUp
    DllStructSetData($actionData, 3, 0x7)   ; MouseUp
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($g_FrameClick_ActionDataAddr), _
        'ptr', DllStructGetPtr($actionData), 'ulong_ptr', 20, 'ulong_ptr*', 0)
    Enqueue(DllStructGetPtr($struct), DllStructGetSize($struct))

    ConsoleWrite('[FrameUI] Clicked (MouseDown+Up): hash=' & $hash & ' frame_id=' & $frameId & @CRLF)
    Return True
EndFunc

;~ Click a frame button by hash. NATIVE implementation — no gwca.dll needed.
;~ Replicates GWCA's ButtonClick chain:
;~   1. GetFrameContext(Frame*) — pure memory reads, returns context pointer
;~   2. SendFrameUIMsg(ECX=context+0xA8, msgid=0x31, wParam=&action, lParam=0)
;~ Sends MouseDown then MouseUp via two shellcode calls through the rendering hook.
Func ClickFrameButton($hash)
    Local $result = GetFrameByHash($hash)
    If $result[0] = 0 Then
        ConsoleWrite('[FrameUI] Button hash ' & $hash & ' not found' & @CRLF)
        Return False
    EndIf

    Local $framePtr = Int($result[0])
    Local $frameId = $result[1]
    Local $processHandle = GetProcessHandle()
    Local $childOffsetId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')

    ; Check frame state
    Local $state = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_STATE, 'dword')
    If BitAND($state, $FRAME_STATE_CREATED) = 0 Then
        ConsoleWrite('[FrameUI] Button not created' & @CRLF)
        Return False
    EndIf

        ; Get frame context — parent frame via [frame+0x128] - 0x128
    Local $context = _GetFrameContext($framePtr)
    If $context = 0 Then
        ConsoleWrite('[FrameUI] GetFrameContext returned NULL' & @CRLF)
        Return False
    EndIf
    ConsoleWrite('[FrameUI] Context=0x' & Hex($context) & ' for frame ' & $frameId & @CRLF)

    ; Get SendFrameUIMsg game function address
    Local $sendFrameFunc = Int(GetLabel('SendFrameUIMsg'))
    If $sendFrameFunc = 0 Or $sendFrameFunc = -1 Then
        ConsoleWrite('[FrameUI] SendFrameUIMsg not found' & @CRLF)
        Return False
    EndIf

    ; Allocate shellcode + action data memory (one-time)
    If $g_FrameClick_ShellcodeAddr = 0 Then
        Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
            'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 64, _
            'dword', 0x1000, 'dword', 0x40)
        If Not IsArray($mem) Or $mem[0] = 0 Then Return False
        $g_FrameClick_ShellcodeAddr = Int($mem[0])
        $g_FrameClick_ActionDataAddr = $g_FrameClick_ShellcodeAddr + 32
        ConsoleWrite('[FrameUI] Shellcode at 0x' & Hex($g_FrameClick_ShellcodeAddr) & @CRLF)
    EndIf

    ; GWCA sends a SINGLE MouseUp (0x7) for ButtonClick — NOT MouseDown+MouseUp.
    ; The game has a dedicated MouseClick (0x8) action state for complete clicks,
    ; but GWCA's ButtonClick uses MouseUp. Sending MouseDown first corrupts UI state.
    ;
    ; ECX note: scanner pattern "83 C1 DC E8" = "add ecx,-0x24; call <target>".
    ; We call <target> directly, so we must pre-apply the -0x24 adjustment.
    ; thisPtr = context + 0xA8 - 0x24 = context + 0x84
    ;
    ; Shellcode (called via RenderingModProc's "call dword[SavedIndex]"):
    ;   mov ecx, <context + 0xA8>   ; B9 <le32>     (5 bytes) __thiscall this
    ;   push 0                      ; 6A 00          (2 bytes) lParam
    ;   push <actionDataAddr>       ; 68 <le32>      (5 bytes) wParam
    ;   push <msgid>                ; 6A 31          (2 bytes) kMouseClick2
    ;   call <sendFrameFunc>        ; E8 <rel32>     (5 bytes)
    ;   ret                         ; C3             (1 byte)
    ;                                         Total: 20 bytes

    ; Use context + 0xA8 (callbacks pointer) as ECX — matches GWCA ButtonFrame::Click
    ; The -0x24 adjustment (for the scanner's "add ecx,-0x24" pattern) was incorrect:
    ; SendFrameUIMsg expects the callbacks pointer directly, not adjusted.
    Local $thisPtr = $context + 0xA8

    ; Single MouseUp (0x7) — matches GWCA ButtonClick behavior
    Local $actionState = 0x7

    ; Write kMouseAction struct to persistent memory
    Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
    DllStructSetData($ad, 1, $frameId)
    DllStructSetData($ad, 2, $childOffsetId)
    DllStructSetData($ad, 3, $actionState)
    DllStructSetData($ad, 4, 0)
    DllStructSetData($ad, 5, 0)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($g_FrameClick_ActionDataAddr), _
        'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

    ; Build shellcode (ends with ret — called via RenderingModProc's "call dword[RenderCmdPtr]")
    Local $sc = DllStructCreate('byte[20]')
    Local $p = 1
    ; mov ecx, thisPtr
    DllStructSetData($sc, 1, 0xB9, $p)
    $p += 1
    _WriteLE32($sc, $p, $thisPtr)
    $p += 4
    ; push 0
    DllStructSetData($sc, 1, 0x6A, $p)
    $p += 1
    DllStructSetData($sc, 1, 0x00, $p)
    $p += 1
    ; push actionDataAddr
    DllStructSetData($sc, 1, 0x68, $p)
    $p += 1
    _WriteLE32($sc, $p, $g_FrameClick_ActionDataAddr)
    $p += 4
    ; push 0x31 (kMouseClick2)
    DllStructSetData($sc, 1, 0x6A, $p)
    $p += 1
    DllStructSetData($sc, 1, 0x31, $p)
    $p += 1
    ; call sendFrameFunc
    DllStructSetData($sc, 1, 0xE8, $p)
    $p += 1
    _WriteLE32($sc, $p, $sendFrameFunc - ($g_FrameClick_ShellcodeAddr + $p - 1 + 4))
    $p += 4
    ; ret
    DllStructSetData($sc, 1, 0xC3, $p)

    ; Write shellcode to memory
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($g_FrameClick_ShellcodeAddr), _
        'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

    ; Queue for RenderingModProc to execute on the game thread.
    ; The rendering hook processes queued commands at char select (MapIsLoaded=0).
    Local $cmd = DllStructCreate('dword;dword')
    DllStructSetData($cmd, 1, $g_FrameClick_ShellcodeAddr)
    DllStructSetData($cmd, 2, 0)
    Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

    ConsoleWrite('[FrameUI] Clicked hash=' & $hash & ' frame_id=' & $frameId & @CRLF)

    ; Wait for game to process
    Sleep(500)

    Return True
EndFunc

; =============================================================================
; Child Frame Navigation
; =============================================================================

;~ Find all direct children of a frame by scanning FrameArray for matching parent pointers.
;~ Returns array of child Frame pointers sorted by child_offset_id.
Func GetChildFrames($framePtr)
    Local $processHandle = GetProcessHandle()
    Local $parentRelAddr = Int($framePtr) + 0x128  ; Address of parent's FrameRelation

    Local $frameArrayAddr = Int(GetLabel('FrameArray'))
    Local $bufferPtr = MemoryRead($processHandle, $frameArrayAddr, 'dword')
    Local $arraySize = MemoryRead($processHandle, $frameArrayAddr + 4, 'dword')
    If $arraySize <= 0 Or $arraySize > 5000 Then
        Local $empty[0]
        Return $empty
    EndIf

    ; Collect children
    Local $children[64][2]  ; [framePtr, childOffsetId] — max 64 children
    Local $count = 0

    For $i = 0 To $arraySize - 1
        Local $fp = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
        If $fp = 0 Or $fp < 0x10000 Then ContinueLoop

        Local $parentPtr = MemoryRead($processHandle, $fp + 0x128, 'dword')
        If $parentPtr = $parentRelAddr Then
            Local $childOff = MemoryRead($processHandle, $fp + 0xB8, 'dword')
            If $count < 64 Then
                $children[$count][0] = $fp
                $children[$count][1] = $childOff
                $count += 1
            EndIf
        EndIf
    Next

    ; Sort by child_offset_id and return frame pointers
    ; Simple insertion sort
    For $i = 1 To $count - 1
        Local $key0 = $children[$i][0]
        Local $key1 = $children[$i][1]
        Local $j = $i - 1
        While $j >= 0 And $children[$j][1] > $key1
            $children[$j + 1][0] = $children[$j][0]
            $children[$j + 1][1] = $children[$j][1]
            $j -= 1
        WEnd
        $children[$j + 1][0] = $key0
        $children[$j + 1][1] = $key1
    Next

    Local $result[$count]
    For $i = 0 To $count - 1
        $result[$i] = $children[$i][0]
    Next
    Return $result
EndFunc

;~ Navigate a child path (e.g. "0,0,6") from a parent frame.
;~ Each index selects the Nth child (sorted by child_offset_id).
;~ Returns the final child Frame pointer, or 0 on failure.
Func NavigateFramePath($framePtr, $path)
    Local $parts = StringSplit($path, ",")
    Local $current = Int($framePtr)

    For $p = 1 To $parts[0]
        Local $targetIdx = Int($parts[$p])
        Local $kids = GetChildFrames($current)
        If $targetIdx >= UBound($kids) Then
            ConsoleWrite('[FrameUI] Path error at step ' & $p & ': index ' & $targetIdx & _
                ' but only ' & UBound($kids) & ' children' & @CRLF)
            Return 0
        EndIf
        $current = Int($kids[$targetIdx])
    Next

    Return $current
EndFunc

;~ Click a frame by its pointer (not hash). Works in-game via CommandFrameClick queue.
;~ Uses the existing FrameClick shared memory + CommandFrameClick ASM stub.
Func ClickFrameByPtr($framePtr)
    Local $processHandle = GetProcessHandle()
    Local $fp = Int($framePtr)

    Local $frameId = MemoryRead($processHandle, $fp + 0xBC, 'dword')
    Local $childOffsetId = MemoryRead($processHandle, $fp + 0xB8, 'dword')

    ; Get context (parent frame) — CommandFrameClick adds 0xA8 to FrameClickFramePtr
    Local $context = _GetFrameContext($fp)
    If $context = 0 Then
        ConsoleWrite('[FrameUI] ClickFrameByPtr: GetFrameContext returned NULL' & @CRLF)
        Return False
    EndIf

    ; Get SendFrameUIMsg function address
    Local $sendFrameFunc = Int(GetLabel('SendFrameUIMsg'))
    If $sendFrameFunc = 0 Or $sendFrameFunc = -1 Then
        ConsoleWrite('[FrameUI] ClickFrameByPtr: SendFrameUIMsg not found' & @CRLF)
        Return False
    EndIf

    ; Write to shared memory labels used by CommandFrameClick ASM
    Local $funcPtrAddr = Int(GetLabel('FrameClickFuncPtr'))
    Local $framePtrAddr = Int(GetLabel('FrameClickFramePtr'))
    Local $msgIdAddr = Int(GetLabel('FrameClickMsgId'))
    Local $actionPtrAddr = Int(GetLabel('FrameClickActionPtr'))
    Local $actionAddr = Int(GetLabel('FrameClickAction'))
    Local $resultAddr = Int(GetLabel('FrameClickResult'))

    ; Write SendFrameUIMsg function pointer
    MemoryWrite($processHandle, $funcPtrAddr, $sendFrameFunc, 'dword')
    ; Write context pointer (CommandFrameClick adds 0xA8)
    MemoryWrite($processHandle, $framePtrAddr, $context, 'dword')
    ; Write message ID (0x31 = kMouseClick2)
    MemoryWrite($processHandle, $msgIdAddr, 0x31, 'dword')
    ; Write pointer to action struct
    MemoryWrite($processHandle, $actionPtrAddr, $actionAddr, 'dword')

    ; Write kMouseAction struct: [frame_id, child_offset_id, action_state, 0, 0, 0, 0, 0]
    Local $action = DllStructCreate('dword;dword;dword;dword;dword;dword;dword;dword')
    DllStructSetData($action, 1, $frameId)
    DllStructSetData($action, 2, $childOffsetId)
    DllStructSetData($action, 3, 0x7)  ; MouseUp (single click, matches GWCA)
    DllStructSetData($action, 4, 0)
    DllStructSetData($action, 5, 0)
    DllStructSetData($action, 6, 0)
    DllStructSetData($action, 7, 0)
    DllStructSetData($action, 8, 0)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($actionAddr), _
        'ptr', DllStructGetPtr($action), 'ulong_ptr', 32, 'ulong_ptr*', 0)

    ; Clear result flag
    MemoryWrite($processHandle, $resultAddr, 0, 'dword')

    ; Enqueue CommandFrameClick (use calibrated address for .5 offset bug)
    Local $cmdAddr = _CalibrateFrameClickAddr(Int(GetLabel('CommandFrameClick')))
    Local $cmd = DllStructCreate('dword;dword')
    DllStructSetData($cmd, 1, $cmdAddr)
    DllStructSetData($cmd, 2, 0)
    Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

    ConsoleWrite('[FrameUI] ClickFrameByPtr: fid=' & $frameId & ' childOff=' & $childOffsetId & @CRLF)

    ; Wait for execution
    Sleep(500)
    Local $result = MemoryRead($processHandle, $resultAddr, 'dword')
    ConsoleWrite('[FrameUI] ClickFrameByPtr result: ' & $result & @CRLF)
    Return ($result = 1)
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

;~ Press Play at character select — uses mouse click (GW ignores synthetic input)
Func PressPlayButton()
    If Not IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
        ConsoleWrite('[FrameUI] Play button not visible' & @CRLF)
        Return False
    EndIf
    Return ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
EndFunc

Func PressPlayButton_MOUSE()
    If Not IsFrameVisible($FRAME_HASH_PLAY_BUTTON) Then
        ConsoleWrite('[FrameUI] Play button not visible' & @CRLF)
        Return False
    EndIf

    Local $hWnd = GetWindowHandle()
    If $hWnd = 0 Then Return False

    WinActivate($hWnd)
    Sleep(300)
    Local $pos = WinGetPos($hWnd)
    If Not IsArray($pos) Then Return False

    Local $playX = $pos[0] + Int($pos[2] * 0.78)
    Local $playY = $pos[1] + Int($pos[3] * 0.96)
    ConsoleWrite('[FrameUI] Clicking Play at ' & $playX & ',' & $playY & @CRLF)
    MouseClick('left', $playX, $playY, 1, 3)
    Return True
EndFunc

;~ Dismiss reconnect dialog with Yes or No using mouse click
Func DismissReconnectDialog($choice = 'no')
    If Not IsReconnectDialogShowing() Then
        ConsoleWrite('[FrameUI] No reconnect dialog showing' & @CRLF)
        Return False
    EndIf

    If $choice = 'yes' Then
        ConsoleWrite('[FrameUI] Clicking reconnect YES' & @CRLF)
        Return ClickFrameButton($FRAME_HASH_RECONNECT_YES)
    Else
        ConsoleWrite('[FrameUI] Clicking reconnect NO' & @CRLF)
        Return ClickFrameButton($FRAME_HASH_RECONNECT_NO)
    EndIf
EndFunc
