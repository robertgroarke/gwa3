#include "lib/GWA2.au3"

; We need to write a small ASM hook to capture packets
; and log the return address (Caller) and the Opcode.

Global $HOOK_SIZE = 1024
Global $memStruct = SafeDllStructCreate('byte[' & $HOOK_SIZE & ']')
Global $memPtr = DllStructGetPtr($memStruct)

Func PacketLogger()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then 
        ConsoleWrite("Character not found" & @CRLF)
        Return
    EndIf
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\packet_log.txt"
    Local $hFile = FileOpen($logFile, 2)
    FileWrite($hFile, "Packet Logger Started..." & @CRLF)
    FileClose($hFile)
    
    Local $packetSendAddr = GetScannedAddress('ScanPacketSendFunction', -0x5)
    If $packetSendAddr == 0 Then
        ConsoleWrite("PacketSendFunction not found." & @CRLF)
        Return
    EndIf
    
    ; Target Hook Location: PacketSendAddr + 6 (The instruction C7 47 54 00 00 00 00)
    ; Previous attempt at +5 split the JNS instruction (79 0D) relative to offset +6?
    ; Bytes: CD FF 85 F6 79 0D [C7 47 54 00 00 00 00]
    ; Index: AA AB AC AD AE AF  B0
    ; Start (AA) + 6 = B0.
    
    Local $hookTarget = $packetSendAddr + 6
    FileWrite($hFile, "Hooking at: " & Hex($hookTarget) & @CRLF)
    
    ; Allocate memory for hook
    Local $processHandle = GetProcessHandle()
    Local $hookMem = SafeDllCall13($kernel_handle, 'ptr', 'VirtualAllocEx', _
            'handle', $processHandle, _
            'ptr', 0, _
            'ulong_ptr', 1024, _
            'dword', 0x1000, _
            'dword', 0x40)
    $hookMem = $hookMem[0]
    
    ; Log structure at $hookMem + 512
    ; [0] = Update Counter
    ; [4] = EAX
    ; [8] = EBX
    ; [12] = ECX
    ; [16] = EDX
    ; [20] = ESI
    ; [24] = EDI
    ; [28] = ESP (Original)
    ; [32] = Stack Top (Return Addr?)
    ; [36] = Stack + 4
    ; [40] = Stack + 8
    
    Local $asm = ""
    ; PUSHAD (Save all regs)
    $asm &= "60"
    
    ; Log Registers to Memory
    ; MOV EAX, [ESP+28] -> EAX (PUSHAD pushes 8 regs * 4 = 32 bytes. EAX is the 8th? No.)
    ; PUSHAD order: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI.
    ; Stack grows down.
    ; ESP -> EDI
    ; ESP+4 -> ESI
    ; ESP+8 -> EBP
    ; ESP+12 -> ESP (Original)
    ; ESP+16 -> EBX
    ; ESP+20 -> EDX
    ; ESP+24 -> ECX
    ; ESP+28 -> EAX
    
    ; We want to read various registers to see args usually passed in registers (fastcall)
    
    ; Let's simpler logging. Just update the counter if we hit it.
    ; And log the first few stack items from the ORIGINAL stack.
    ; Original ESP is at ESP+32 (after PUSHAD).
    
    ; MOV ESI, [ESP+32] -> Original Stack Pointer
    $asm &= "8B742420"
    
    ; Log Stack[0] (Return Address?) -> [LogBase+32]
    ; MOV EAX, [ESI]
    $asm &= "8B06"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 32))
    
    ; Log Stack[1] -> [LogBase+36]
    ; MOV EAX, [ESI+4]
    $asm &= "8B4604"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 36))
    
    ; Log Stack[2] -> [LogBase+40]
    ; MOV EAX, [ESI+8]
    $asm &= "8B4608"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 40))
    
    ; Log EAX (from PUSHAD stack) -> [LogBase+4]
    ; MOV EAX, [ESP+28]
    $asm &= "8B44241C"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 4))

    ; Log EBX (from PUSHAD stack) -> [LogBase+8]
    ; MOV EAX, [ESP+16]
    $asm &= "8B442410"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 8))

    ; Log EDX (from PUSHAD stack) -> [LogBase+16]
    ; MOV EAX, [ESP+20]
    $asm &= "8B442414"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 16))

    ; Increment Counter
    $asm &= "FF05" & SwapEndian(Hex($hookMem + 512))
    
    ; POPAD
    $asm &= "61"
    
    ; EXECUTE ORIGINAL INSTRUCTION
    ; MOV DWORD PTR [EDI+0x54], 0
    ; C7 47 54 00 00 00 00 (7 bytes)
    $asm &= "C7475400000000"
    
    ; JUMP BACK to $hookTarget + 7 (Start of next instruction)
    ; JMP Rel32
    ; Dest = $hookTarget + 7
    ; Src = $hookMem + Len + 5
    Local $len = StringLen($asm) / 2
    $asm &= "E9"
    Local $src = $hookMem + $len + 5
    Local $dest = $hookTarget + 7
    Local $rel = $dest - $src
    $asm &= SwapEndian(Hex($rel, 8))
    
    WriteBinary($asm, $hookMem)
    
    ; Write Detour at $hookTarget (Offset +6)
    ; JMP $hookMem (5 bytes)
    ; NOP NOP (2 bytes) -> Total 7 bytes overwritten to cover the 7-byte MOV instruction
    Local $detour = "E9" & SwapEndian(Hex($hookMem - $hookTarget - 5, 8))
    $detour &= "9090"
    
    WriteBinary($detour, $hookTarget)
    
    ConsoleWrite("Safe Hook installed. Monitoring..." & @CRLF)
    
    Local $lastCount = 0
    Local $logBase = $hookMem + 512
    
    While 1
        Local $count = MemoryRead($logBase)
        If $count <> $lastCount Then
             Local $eax_val = MemoryRead($logBase + 4)
             Local $ebx_val = MemoryRead($logBase + 8)
             Local $edx_val = MemoryRead($logBase + 16)
             Local $stack0 = MemoryRead($logBase + 32)
             Local $stack1 = MemoryRead($logBase + 36)
             Local $stack2 = MemoryRead($logBase + 40)
             
             Local $msg = "Packet Hit! EAX=" & Hex($eax_val) & " EBX=" & Hex($ebx_val) & " EDX=" & Hex($edx_val) & _
                          " Stack[0]=" & Hex($stack0) & " Stack[1]=" & Hex($stack1) & " Stack[2]=" & Hex($stack2) & @CRLF
             ConsoleWrite($msg)
             $hFile = FileOpen($logFile, 1)
             FileWrite($hFile, $msg)
             FileClose($hFile)
             
             $lastCount = $count
        EndIf
        Sleep(50)
    WEnd
EndFunc

PacketLogger()
