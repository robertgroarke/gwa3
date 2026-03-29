#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Dump SendFrameUIMsg function ===" & @CRLF)

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
Local $ph = GetProcessHandle()

Local $func = Int(GetLabel('SendFrameUIMsg'))
ConsoleWrite("SendFrameUIMsg at 0x" & Hex($func) & @CRLF)

; Read first 128 bytes of the function
Local $buf = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($func), _
    'ptr', DllStructGetPtr($buf), 'ulong_ptr', 128, 'ulong_ptr*', 0)

ConsoleWrite(@CRLF & "Function bytes:" & @CRLF)
For $row = 0 To 7
    Local $hex = "  +" & Hex($row * 16, 2) & ": "
    For $col = 1 To 16
        $hex &= Hex(DllStructGetData($buf, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hex & @CRLF)
Next

; Decode: look for CMP/TEST instructions that check global state
ConsoleWrite(@CRLF & "Looking for state checks:" & @CRLF)
For $i = 1 To 120
    Local $b = DllStructGetData($buf, 1, $i)

    ; Look for comparisons with global addresses (83 3D, 39 xx, A1, 8B 0D, etc.)
    If $b = 0xA1 Then ; mov eax, [imm32]
        Local $addr = 0
        For $j = 1 To 4
            $addr += DllStructGetData($buf, 1, $i + $j) * (256 ^ ($j - 1))
        Next
        ; Read the value at that address
        Local $val = MemoryRead($ph, $addr, 'dword')
        ConsoleWrite("  +" & ($i-1) & ": mov eax, [0x" & Hex($addr) & "] = " & $val & @CRLF)
    ElseIf $b = 0x83 And $i + 6 <= 128 Then ; cmp/test with imm8
        Local $modrm = DllStructGetData($buf, 1, $i + 1)
        If $modrm = 0x3D Then ; cmp [imm32], imm8
            Local $addr2 = 0
            For $j = 1 To 4
                $addr2 += DllStructGetData($buf, 1, $i + 1 + $j) * (256 ^ ($j - 1))
            Next
            Local $imm = DllStructGetData($buf, 1, $i + 6)
            Local $val2 = MemoryRead($ph, $addr2, 'dword')
            ConsoleWrite("  +" & ($i-1) & ": cmp [0x" & Hex($addr2) & "], " & $imm & " (current=" & $val2 & ")" & @CRLF)
        EndIf
    ElseIf $b = 0x8B And DllStructGetData($buf, 1, $i + 1) = 0x0D Then ; mov ecx, [imm32]
        Local $addr3 = 0
        For $j = 1 To 4
            $addr3 += DllStructGetData($buf, 1, $i + 2 + $j - 1) * (256 ^ ($j - 1))
        Next
        Local $val3 = MemoryRead($ph, $addr3, 'dword')
        ConsoleWrite("  +" & ($i-1) & ": mov ecx, [0x" & Hex($addr3) & "] = 0x" & Hex($val3) & @CRLF)
    EndIf
Next

ConsoleWrite("=== DONE ===" & @CRLF)
