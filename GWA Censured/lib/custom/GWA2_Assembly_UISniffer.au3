#include-once

; =============================================================================
; GWA2_Assembly_UISniffer.au3
;
; Hooks the game's internal SendUIMessage function to intercept and log
; all UI messages. Polling-only mode (no PostMessage).
;
; Enable/disable at runtime via UISnifferEnable() / UISnifferDisable()
; =============================================================================

Global $g_UISnifferEnabled = False
Global $g_UISniffer_MsgId_Addr = 0
Global $g_UISniffer_WParam_Addr = 0
Global $g_UISniffer_LParam_Addr = 0
Global $g_UISniffer_Counter_Addr = 0
Global $g_UISniffer_LastCounter = 0
Global $g_UISniffer_OrigBytes = ''
Global $g_UISniffer_OrigByteCount = 0
Global $g_UISniffer_ActualProcAddr = 0

; Extension flag
Global $g_b_UISniffer = True

; =============================================================================
; Extension Points
; =============================================================================

Func ExtendAddPattern_UISniffer()
EndFunc

Func ExtendScanner_UISniffer()
    Local $uiMsgAddr = $scan_results['UIMessage']
    SetLabel('UIMessageStart', Ptr($uiMsgAddr))
    ; UIMessageReturn will be set dynamically in UISnifferEnable based on prologue analysis
    ; Default to +6 (most common: push ebp/mov ebp,esp/mov eax,[ebp+8] = 6 bytes)
    SetLabel('UIMessageReturn', Ptr($uiMsgAddr + 6))

    Debug('UIMessageStart: ' & GetLabel('UIMessageStart'))
    Debug('UIMessageReturn: ' & GetLabel('UIMessageReturn'))
EndFunc

Func ExtendAssemblerData_UISniffer()
    _('UISnifferCounter/4')
    _('UISnifferEnabled/4')
    _('UISnifferMsgId/4')
    _('UISnifferWParam/4')
    _('UISnifferLParam/4')
EndFunc

;~ Assemble the hook trampoline (polling-only)
;~ NOTE: The assembler has a fractional byte counter (.5) that causes label-based
;~ relative jumps (jnz, jz) to be off by ~4 bytes. We emit the correct instruction
;~ opcodes here but patch the relative offsets at enable time in UISnifferEnable().
Func ExtendAssembler_UISniffer()
    _('UISnifferProc:')
    _('pushfd')                            ; +0x00 (1 byte)
    _('pushad')                            ; +0x01 (1 byte)

    _('cmp dword[UISnifferEnabled],1')     ; +0x02 (7 bytes)
    _('jnz UISnifferSkip')                 ; +0x09 (2 bytes) — offset patched at runtime

    _('mov eax,dword[esp+28]')             ; +0x0B (4 bytes) msgid
    _('test eax,eax')                      ; +0x0F (2 bytes)
    _('jz UISnifferSkip')                  ; +0x11 (2 bytes) — offset patched at runtime

    _('mov dword[UISnifferMsgId],eax')     ; +0x13 (5 bytes)
    _('mov eax,dword[esp+2C]')             ; +0x18 (4 bytes) wParam
    _('mov dword[UISnifferWParam],eax')    ; +0x1C (5 bytes)
    _('mov eax,dword[esp+30]')             ; +0x21 (4 bytes) lParam
    _('mov dword[UISnifferLParam],eax')    ; +0x25 (5 bytes)
    _('mov eax,dword[UISnifferCounter]')   ; +0x2A (5 bytes)
    _('inc eax')                           ; +0x2F (1 byte)
    _('mov dword[UISnifferCounter],eax')   ; +0x30 (5 bytes)

    _('UISnifferSkip:')                    ; +0x35
    _('popad')                             ; +0x35 (1 byte)
    _('popfd')                             ; +0x36 (1 byte)

    ; 8 NOPs: patched with original prologue bytes at enable time
    _('UISnifferOriginal:')                ; +0x37
    _('nop')                               ; +0x37
    _('nop')                               ; +0x38
    _('nop')                               ; +0x39
    _('nop')                               ; +0x3A
    _('nop')                               ; +0x3B
    _('nop')                               ; +0x3C
    _('nop')                               ; +0x3D
    _('nop')                               ; +0x3E

    _('ljmp UIMessageReturn')              ; +0x3F (5 bytes)
EndFunc

; =============================================================================
; Runtime Control
; =============================================================================

Func UISnifferEnable()
    If $g_UISnifferEnabled Then Return

    Local $processHandle = GetProcessHandle()
    Local $uiMsgStart = GetLabel('UIMessageStart')

    ; Cache data addresses
    $g_UISniffer_MsgId_Addr = GetLabel('UISnifferMsgId')
    $g_UISniffer_WParam_Addr = GetLabel('UISnifferWParam')
    $g_UISniffer_LParam_Addr = GetLabel('UISnifferLParam')
    $g_UISniffer_Counter_Addr = GetLabel('UISnifferCounter')

    ; --- Find actual trampoline start (label offset is ~4 bytes off) ---
    Local $labelAddr = GetLabel('UISnifferProc')
    Local $scanBuf = DllStructCreate('byte[16]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr(Int($labelAddr) - 8), _
        'ptr', DllStructGetPtr($scanBuf), 'ulong_ptr', 16, 'ulong_ptr*', 0)

    Local $actualStart = Int($labelAddr)
    For $i = 1 To 15
        If DllStructGetData($scanBuf, 1, $i) = 0x9C And DllStructGetData($scanBuf, 1, $i + 1) = 0x60 Then
            $actualStart = Int($labelAddr) - 8 + ($i - 1)
            ExitLoop
        EndIf
    Next
    $g_UISniffer_ActualProcAddr = $actualStart
    ConsoleWrite('[UISniffer] Actual trampoline at 0x' & Hex($actualStart) & @CRLF)

    ; --- Analyze original prologue to find instruction boundary >= 5 bytes ---
    ; Read 16 bytes at UIMessageStart
    Local $prologueBuf = DllStructCreate('byte[16]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', $uiMsgStart, _
        'ptr', DllStructGetPtr($prologueBuf), 'ulong_ptr', 16, 'ulong_ptr*', 0)

    ; Find instruction boundary: decode simple x86 prologues
    ; Common patterns: 55 (push ebp, 1), 8BEC (mov ebp,esp, 2), 8B45xx (mov eax,[ebp+x], 3)
    ; 83F8xx (cmp eax,x, 3), 56 (push esi, 1), 57 (push edi, 1), 53 (push ebx, 1)
    ; 81ECxxxx (sub esp,imm32, 6), 83ECxx (sub esp,imm8, 3)
    Local $boundary = 0
    Local $pos = 0
    While $pos < 16
        Local $b = DllStructGetData($prologueBuf, 1, $pos + 1)
        Switch $b
            Case 0x50 To 0x57  ; push reg
                $pos += 1
            Case 0x58 To 0x5F  ; pop reg
                $pos += 1
            Case 0x8B  ; mov r32, r/m32
                Local $modrm = DllStructGetData($prologueBuf, 1, $pos + 2)
                Local $mod = BitShift($modrm, 6)
                Switch $mod
                    Case 0  ; [reg] or [disp32]
                        Local $rm = BitAND($modrm, 7)
                        If $rm = 5 Then
                            $pos += 6  ; [disp32]
                        ElseIf $rm = 4 Then
                            $pos += 3  ; [SIB]
                        Else
                            $pos += 2
                        EndIf
                    Case 1  ; [reg+disp8]
                        $pos += 3
                    Case 2  ; [reg+disp32]
                        $pos += 6
                    Case 3  ; reg,reg
                        $pos += 2
                EndSwitch
            Case 0x83  ; arith r/m32, imm8
                $pos += 3
            Case 0x81  ; arith r/m32, imm32
                $pos += 6
            Case 0x89  ; mov r/m32, r32
                $pos += 2  ; approximate
            Case 0x90  ; nop
                $pos += 1
            Case Else
                $pos += 1  ; fallback
        EndSwitch

        If $pos >= 5 Then
            $boundary = $pos
            ExitLoop
        EndIf
    WEnd

    If $boundary < 5 Then $boundary = 6  ; safe default
    $g_UISniffer_OrigByteCount = $boundary
    ConsoleWrite('[UISniffer] Prologue boundary at ' & $boundary & ' bytes' & @CRLF)

    ; Save original bytes
    Local $origBytes = DllStructCreate('byte[' & $boundary & ']')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', $uiMsgStart, _
        'ptr', DllStructGetPtr($origBytes), 'ulong_ptr', $boundary, 'ulong_ptr*', 0)

    $g_UISniffer_OrigBytes = ''
    For $i = 1 To $boundary
        $g_UISniffer_OrigBytes &= Hex(DllStructGetData($origBytes, 1, $i), 2)
    Next
    ConsoleWrite('[UISniffer] Original ' & $boundary & ' bytes: ' & $g_UISniffer_OrigBytes & @CRLF)

    ; --- Patch original bytes into UISnifferOriginal region ---
    ; Use relative offset from UISnifferProc to UISnifferOriginal (exact, .5 cancels)
    Local $relOffset = Int(GetLabel('UISnifferOriginal')) - Int($labelAddr)
    Local $actualOrigAddr = $actualStart + $relOffset
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($actualOrigAddr), _
        'ptr', DllStructGetPtr($origBytes), 'ulong_ptr', $boundary, 'ulong_ptr*', 0)

    ; --- Patch the ljmp target to UIMessage + boundary (not +5 or +6) ---
    ; The ljmp is at UISnifferOriginal + 8 NOPs = actualOrigAddr + 8
    Local $ljmpAddr = $actualOrigAddr + 8
    ; JMP E9 [rel32]: target = ljmpAddr + 5 + rel32, want target = uiMsgStart + boundary
    Local $ljmpTarget = Int($uiMsgStart) + $boundary
    Local $ljmpOffset = $ljmpTarget - ($ljmpAddr + 5)
    ; Write the E9 + rel32
    Local $ljmpBytes = DllStructCreate('byte[5]')
    DllStructSetData($ljmpBytes, 1, 0xE9, 1)
    DllStructSetData($ljmpBytes, 1, BitAND($ljmpOffset, 0xFF), 2)
    DllStructSetData($ljmpBytes, 1, BitAND(BitShift($ljmpOffset, 8), 0xFF), 3)
    DllStructSetData($ljmpBytes, 1, BitAND(BitShift($ljmpOffset, 16), 0xFF), 4)
    DllStructSetData($ljmpBytes, 1, BitAND(BitShift($ljmpOffset, 24), 0xFF), 5)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($ljmpAddr), _
        'ptr', DllStructGetPtr($ljmpBytes), 'ulong_ptr', 5, 'ulong_ptr*', 0)
    ConsoleWrite('[UISniffer] Patched ljmp at 0x' & Hex($ljmpAddr) & ' -> 0x' & Hex($ljmpTarget) & @CRLF)

    ; --- Patch jnz/jz offsets to correct values ---
    ; UISnifferSkip (popad) is at actualStart + 0x35
    ; jnz at actualStart + 0x09: offset should be 0x35 - 0x0B = 0x2A
    ; jz  at actualStart + 0x11: offset should be 0x35 - 0x13 = 0x22
    Local $patchByte = DllStructCreate('byte[1]')

    DllStructSetData($patchByte, 1, 0x2A, 1)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($actualStart + 0x0A), _
        'ptr', DllStructGetPtr($patchByte), 'ulong_ptr', 1, 'ulong_ptr*', 0)

    DllStructSetData($patchByte, 1, 0x22, 1)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($actualStart + 0x12), _
        'ptr', DllStructGetPtr($patchByte), 'ulong_ptr', 1, 'ulong_ptr*', 0)
    ConsoleWrite('[UISniffer] Patched jnz/jz offsets' & @CRLF)

    ; --- Enable the sniffer flag ---
    MemoryWrite($processHandle, GetLabel('UISnifferEnabled'), 1, 'dword')

    ; --- Install the JMP detour at UIMessageStart ---
    Local $jmpOffset = $actualStart - Int($uiMsgStart) - 5
    WriteBinary($processHandle, 'E9' & SwapEndian(Hex($jmpOffset)), $uiMsgStart)

    $g_UISnifferEnabled = True
    ConsoleWrite('[UISniffer] ENABLED - hook active' & @CRLF)
EndFunc

Func UISnifferDisable()
    If Not $g_UISnifferEnabled Then Return

    ; Restore original bytes at UIMessageStart
    WriteBinary(GetProcessHandle(), $g_UISniffer_OrigBytes, GetLabel('UIMessageStart'))

    ; Disable the flag
    MemoryWrite(GetProcessHandle(), GetLabel('UISnifferEnabled'), 0, 'dword')

    $g_UISnifferEnabled = False
    ConsoleWrite('[UISniffer] DISABLED - hook removed' & @CRLF)
EndFunc
