#RequireAdmin
#include "lib\Froggy_Includes.au3"

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)

Local $processHandle = GetProcessHandle()

; Get calibrated CommandFrameClick address
Local $labelAddr = Int(GetLabel('CommandFrameClick'))
ConsoleWrite("CommandFrameClick label = 0x" & Hex($labelAddr) & @CRLF)
Local $actualAddr = _CalibrateFrameClickAddr($labelAddr)

; Dump 40 bytes at the actual address
ConsoleWrite("CommandFrameClick at 0x" & Hex($actualAddr) & ":" & @CRLF)
Local $buf = DllStructCreate('byte[40]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($actualAddr), _
    'ptr', DllStructGetPtr($buf), 'ulong_ptr', 40, 'ulong_ptr*', 0)

For $row = 0 To 2
    Local $hex = "  +" & Hex($row*16, 2) & ": "
    For $col = 1 To 16
        If $row*16+$col <= 40 Then
            $hex &= Hex(DllStructGetData($buf, 1, $row*16+$col), 2) & " "
        EndIf
    Next
    ConsoleWrite($hex & @CRLF)
Next

Local $funcPtr = MemoryRead($processHandle, GetLabel('FrameClickFuncPtr'), 'dword')
ConsoleWrite(@CRLF & "FrameClickFuncPtr = 0x" & Hex($funcPtr) & @CRLF)
Local $actionPtr = MemoryRead($processHandle, GetLabel('FrameClickActionPtr'), 'dword')
ConsoleWrite("FrameClickActionPtr = 0x" & Hex($actionPtr) & @CRLF)
ConsoleWrite("FrameClickAction label = 0x" & Hex(Int(GetLabel('FrameClickAction'))) & @CRLF)
ConsoleWrite("SendFrameUIMsg label = 0x" & Hex(Int(GetLabel('SendFrameUIMsg'))) & @CRLF)
