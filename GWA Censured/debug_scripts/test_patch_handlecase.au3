#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Patch HandleCase to Execute Commands ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf
SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
ConsoleWrite("Connected. At char select: " & IsAtCharSelect() & @CRLF)

; The HandleCase path discards commands (jmp MainExit instead of jmp ebx)
; Fix: change the "je HandleCase" at line 1525 to "je RegularFlow"
; Both HandleCase and RegularFlow labels have the .5 offset issue
; But the RELATIVE offset between them is correct (both have same error)

; Find the "je HandleCase" instruction in the injected code
; It's: 74 XX (short conditional jump)
; Located AFTER: cmp eax,0 / je HandleCase
; Which is: 83 F8 00 74 XX

; The MainProc code is at GetLabel('MainProc') area
; But we can't use labels due to .5 error
; Instead: find the instruction by scanning for the pattern

; The sequence is:
; cmp eax,0      = 83 F8 00  (but could also be 3D 00 00 00 00)
; je HandleCase  = 74 XX
; mov ebx,eax    = 8B D8
; imul ebx,ebx,7C = 6B DB 7C (or 69 DB 7C 00 00 00)

; Let's scan the injection region for this pattern
; The injection region starts at the memory interface address
Local $injectionBase = MemoryRead($processHandle, GetLabel('BasePointer'), 'dword')
; Actually, the code is in the VirtualAllocEx region, not at BasePointer
; Let me look for the instruction pattern near MainProc

; The "cmp eax,0" followed by "je" is the key sequence
; In the assembled code: _('cmp eax,0') _('je HandleCase')
; cmp eax,0 with the assembler encodes as: 83 F8 00 (3 bytes) or 3D 00 00 00 00 (5 bytes)
; Followed by: 74 XX (je short) or 0F 84 XX XX XX XX (je near)

; Actually, looking at GWA2_Assembly.au3 line 2327-2403, "cmp eax,0" might be
; encoded as 3D 00 00 00 00 (5 bytes) since the assembler has special handling

; Let's find it by searching for "je" followed by the HandleCase code signature
; HandleCase starts with: mov eax,[QueueCounter] = A1 <addr>
; RegularFlow also starts with: mov eax,[QueueCounter] = A1 <addr>

; Simplest: read the MainProc code region and find the je instruction
; The MainProc label is approximate. Let me use a different approach:
; Find the jmp MainExit at the end of HandleCase and change it

; HandleCase ends with:
; mov [QueueCounter],eax  = A3 <QueueCounter_addr>
; jmp MainExit            = EB XX or E9 XX XX XX XX

; After HandleCase, RegularFlow starts:
; mov eax,[QueueCounter]  = A1 <QueueCounter_addr>

; So the pattern is: A3 <addr> EB/E9 <offset> A1 <addr>
; Where both <addr> are the QueueCounter address

; Let's find QueueCounter label and search for this pattern
Local $qcAddr = Int(GetLabel('QueueCounter'))
ConsoleWrite("QueueCounter label = 0x" & Hex($qcAddr) & @CRLF)

; Search the injection region (around where code is)
; The code is typically near 0x08-0x0A million range
; Let me search around the RegularFlow/HandleCase area
; Use the MainStart label as anchor (it's a real game address that the framework hooks)
Local $mainStartHook = Int(GetLabel('MainStart'))
ConsoleWrite("MainStart = 0x" & Hex($mainStartHook) & @CRLF)

; Read 5 bytes at MainStart — it should be a JMP to MainProc
Local $hookBytes = DllStructCreate('byte[5]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($mainStartHook), _
    'ptr', DllStructGetPtr($hookBytes), 'ulong_ptr', 5, 'ulong_ptr*', 0)

If DllStructGetData($hookBytes, 1, 1) = 0xE9 Then
    ; Read the rel32 as a signed int
    Local $relBuf = DllStructCreate('int')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($mainStartHook + 1), _
        'ptr', DllStructGetPtr($relBuf), 'ulong_ptr', 4, 'ulong_ptr*', 0)
    Local $rel = DllStructGetData($relBuf, 1)
    Local $mainProcAddr = $mainStartHook + 5 + $rel
    ConsoleWrite("MainProc at 0x" & Hex($mainProcAddr) & @CRLF)

    ; Read 256 bytes of MainProc to find HandleCase/RegularFlow
    Local $mainCode = DllStructCreate('byte[256]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($mainProcAddr), _
        'ptr', DllStructGetPtr($mainCode), 'ulong_ptr', 256, 'ulong_ptr*', 0)

    ; Dump it
    ConsoleWrite(@CRLF & "MainProc code:" & @CRLF)
    For $row = 0 To 15
        Local $hex = "  +" & Hex($row*16, 2) & ": "
        For $col = 1 To 16
            If $row*16+$col <= 256 Then
                $hex &= Hex(DllStructGetData($mainCode, 1, $row*16+$col), 2) & " "
            EndIf
        Next
        ConsoleWrite($hex & @CRLF)
    Next

    ; Search for the HandleCase "EB 44" (jmp MainExit) in the MainProc dump
    ; It follows: A3 <QueueCounter> (5 bytes), then EB 44
    ; Find it by looking for EB 44 followed eventually by A1 <QueueCounter> (RegularFlow start)
    Local $patched = False
    For $i = 1 To 240
        If DllStructGetData($mainCode, 1, $i) = 0xEB And _
           DllStructGetData($mainCode, 1, $i+1) = 0x44 Then
            ; Found EB 44. Check if RegularFlow (A1 <QueueCounter>) follows nearby
            Local $regFlowOff = 0
            For $j = $i + 2 To $i + 20
                If $j + 4 <= 256 And DllStructGetData($mainCode, 1, $j) = 0xA1 Then
                    $regFlowOff = $j
                    ExitLoop
                EndIf
            Next
            If $regFlowOff > 0 Then
                ; Calculate new offset: from EB instruction to RegularFlow
                Local $newOffset = ($regFlowOff - 1) - ($i + 1)  ; -1 for 0-based, +1 for instruction size
                Local $patchAddr = $mainProcAddr + ($i - 1)
                ConsoleWrite("Found EB 44 at MainProc+" & ($i-1) & " (0x" & Hex($patchAddr) & ")" & @CRLF)
                ConsoleWrite("RegularFlow at MainProc+" & ($regFlowOff-1) & " new offset=" & $newOffset & @CRLF)

                ; Patch
                Local $patch = DllStructCreate('byte[2]')
                DllStructSetData($patch, 1, 0xEB, 1)
                DllStructSetData($patch, 1, $newOffset, 2)
                DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
                    'handle', $processHandle, 'ptr', Ptr($patchAddr), _
                    'ptr', DllStructGetPtr($patch), 'ulong_ptr', 2, 'ulong_ptr*', 0)
                ConsoleWrite("*** PATCHED: EB 44 -> EB " & Hex($newOffset, 2) & " ***" & @CRLF)
                $patched = True
                ExitLoop
            EndIf
        EndIf
    Next
    If Not $patched Then ConsoleWrite("WARNING: Could not find EB 44 to patch" & @CRLF)

    ; OLD search-based approach (backup):
    ; Find the "jmp MainExit" in HandleCase by looking for:
    ; A3 <QueueCounter LE bytes> then EB/E9 (jmp)
    ; QueueCounter address low bytes
    Local $qcLE = DllStructCreate('dword')
    DllStructSetData($qcLE, 1, $qcAddr)
    Local $qcB1 = DllStructGetData($qcLE, 1, 1)
    Local $qcB2 = DllStructGetData($qcLE, 1, 2)

    For $i = 1 To 240
        ; Look for: A3 <qc_b1> <qc_b2> ... followed by EB or E9
        If DllStructGetData($mainCode, 1, $i) = 0xA3 And _
           DllStructGetData($mainCode, 1, $i+1) = $qcB1 And _
           DllStructGetData($mainCode, 1, $i+2) = $qcB2 Then
            ; Found "mov [QueueCounter],eax". The jmp is 5 bytes later.
            Local $jmpByte = DllStructGetData($mainCode, 1, $i+5)
            ConsoleWrite("  Found mov [QueueCounter] at +" & ($i-1) & @CRLF)
            ConsoleWrite("    Next byte: 0x" & Hex($jmpByte, 2) & @CRLF)

            If $jmpByte = 0xEB Then ; short jmp
                Local $jmpOffset = DllStructGetData($mainCode, 1, $i+6)
                ConsoleWrite("    JMP short offset: 0x" & Hex($jmpOffset, 2) & " (target: +" & ($i+6+$jmpOffset) & ")" & @CRLF)

                ; Check if the NEXT "mov eax,[QueueCounter]" (RegularFlow) follows
                ; If there's a second A3 pattern later, the first jmp is HandleCase's exit
                Local $nextA1 = 0
                For $j = $i + 7 To 250
                    If DllStructGetData($mainCode, 1, $j) = 0xA1 And _
                       DllStructGetData($mainCode, 1, $j+1) = $qcB1 And _
                       DllStructGetData($mainCode, 1, $j+2) = $qcB2 Then
                        $nextA1 = $j
                        ConsoleWrite("    RegularFlow starts at +" & ($j-1) & @CRLF)
                        ExitLoop
                    EndIf
                Next

                If $nextA1 > 0 Then
                    ; Calculate what the jmp offset should be to reach RegularFlow
                    ; jmp is at offset (i+5)-1 = i+4 in the code
                    ; jmp instruction is 2 bytes: EB XX
                    ; target = jmp_addr + 2 + offset
                    ; We want target = nextA1 - 1 (the A1 byte)
                    ; offset = (nextA1-1) - (i+4+2) = nextA1 - i - 7
                    Local $newOffset = ($nextA1 - 1) - ($i + 4 + 2)
                    ConsoleWrite("    New offset to RegularFlow: 0x" & Hex($newOffset, 2) & @CRLF)

                    ; PATCH: change the jmp offset to go to RegularFlow instead of MainExit
                    Local $patchAddr = $mainProcAddr + ($i + 5)  ; address of the EB byte
                    Local $patchBuf = DllStructCreate('byte[2]')
                    DllStructSetData($patchBuf, 1, 0xEB, 1)
                    DllStructSetData($patchBuf, 1, $newOffset, 2)
                    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
                        'handle', $processHandle, 'ptr', Ptr($patchAddr), _
                        'ptr', DllStructGetPtr($patchBuf), 'ulong_ptr', 2, 'ulong_ptr*', 0)
                    ConsoleWrite("*** PATCHED HandleCase jmp at 0x" & Hex($patchAddr) & _
                        " offset 0x" & Hex($jmpOffset, 2) & " -> 0x" & Hex($newOffset, 2) & " ***" & @CRLF)
                    ExitLoop
                EndIf
            EndIf
        EndIf
    Next
EndIf

; Now test: queue a command and see if it executes
ConsoleWrite(@CRLF & "=== Testing command execution at char select ===" & @CRLF)

; Use the direct SendFrameUIMsg shellcode approach
Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $result[0] = 0 Then
    ConsoleWrite("Play button not found" & @CRLF)
    Exit
EndIf
Local $framePtr = Int($result[0])
Local $frameId = $result[1]
Local $childOff = MemoryRead($processHandle, $framePtr + 0xB8, 'dword')
ConsoleWrite("Play: ptr=0x" & Hex($framePtr) & " id=" & $frameId & " child=" & $childOff & @CRLF)

Local $sendFrameFunc = Int(GetLabel('SendFrameUIMsg'))
ConsoleWrite("SendFrameUIMsg = 0x" & Hex($sendFrameFunc) & @CRLF)

; Build shellcode
Local $scMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 128, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($scMem[0])
Local $actionAddr = $scAddr + 64
Local $testFlagAddr = $scAddr + 84
Local $crPtrAddr = $scAddr + 88

; Write action data (MouseDown)
Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
DllStructSetData($ad, 1, $frameId)
DllStructSetData($ad, 2, $childOff)
DllStructSetData($ad, 3, 0x6)  ; MouseDown
DllStructSetData($ad, 4, 0)
DllStructSetData($ad, 5, 0)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($actionAddr), _
    'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

; Write CommandReturn pointer and test flag
Local $crLabel = Int(GetLabel('CommandReturn'))
Local $crBuf = DllStructCreate('byte[16]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($crLabel - 8), _
    'ptr', DllStructGetPtr($crBuf), 'ulong_ptr', 16, 'ulong_ptr*', 0)
Local $crActual = $crLabel
For $ci = 1 To 13
    If DllStructGetData($crBuf, 1, $ci) = 0x8B And DllStructGetData($crBuf, 1, $ci+1) = 0x0D Then
        $crActual = $crLabel - 8 + ($ci - 1)
        ExitLoop
    EndIf
Next
MemoryWrite($processHandle, $crPtrAddr, $crActual, 'dword')
MemoryWrite($processHandle, $testFlagAddr, 0, 'dword')

; Shellcode: mov ecx,<thisPtr>; push 0; push actionAddr; push 0x31; call sendFrameFunc;
;            mov [testFlagAddr],1; jmp [crPtrAddr]
Local $thisPtr = $framePtr + 0xA8
Local $sc = DllStructCreate('byte[48]')
Local $p = 1

DllStructSetData($sc, 1, 0xB9, $p)
$p += 1
_WriteLE32($sc, $p, $thisPtr)
$p += 4
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x00, $p)
$p += 1
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $actionAddr)
$p += 4
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x31, $p)
$p += 1
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $sendFrameFunc - ($scAddr + $p - 1 + 4))
$p += 4
; mov [testFlagAddr], 1
DllStructSetData($sc, 1, 0xC7, $p)
$p += 1
DllStructSetData($sc, 1, 0x05, $p)
$p += 1
_WriteLE32($sc, $p, $testFlagAddr)
$p += 4
_WriteLE32($sc, $p, 1)
$p += 4
; jmp [crPtrAddr]
DllStructSetData($sc, 1, 0xFF, $p)
$p += 1
DllStructSetData($sc, 1, 0x25, $p)
$p += 1
_WriteLE32($sc, $p, $crPtrAddr)
$p += 4

DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p - 1, 'ulong_ptr*', 0)

; Screenshot before
Local $hWnd = $game_clients[$game_clients[0][0]][2]
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\patched_before.png', $hWnd)

; Sync and queue
$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $scAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Command queued (counter=" & $queue_counter & ")" & @CRLF)

Sleep(2000)
Local $flag = MemoryRead($processHandle, $testFlagAddr, 'dword')
ConsoleWrite("Test flag = " & $flag & @CRLF)

; If MouseDown worked, do MouseUp too
If $flag = 1 Then
    ConsoleWrite("SHELLCODE EXECUTED! Sending MouseUp..." & @CRLF)
    DllStructSetData($ad, 3, 0x7)  ; MouseUp
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($actionAddr), _
        'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)
    MemoryWrite($processHandle, $testFlagAddr, 0, 'dword')
    $queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
    Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
    Sleep(2000)
EndIf

Sleep(5000)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\patched_after.png', $hWnd)
Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)
If $status <> 0 Then
    ConsoleWrite("*** PLAY CLICKED! ***" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
