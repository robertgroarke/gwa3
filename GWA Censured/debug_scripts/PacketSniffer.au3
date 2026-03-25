#include "lib/GWA2.au3"

Global $PAGE_EXECUTE_READWRITE = 0x40

Func SniffPackets()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    
    If $index == -1 Then
        ConsoleWrite("Character '" & $targetName & "' not found." & @CRLF)
        Return
    EndIf
    
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $packetSendFunc = GetValue('PacketSendFunction')
    If $packetSendFunc == 0 Then
        ConsoleWrite("Error: PacketSendFunction is 0" & @CRLF)
        Return
    EndIf
    
    ConsoleWrite("Hooking PacketSendFunction at: " & Hex($packetSendFunc) & @CRLF)
    
    ; 1. Allocate Log Buffer (64KB)
    Local $logAlloc = DllCall("kernel32.dll", "ptr", "VirtualAllocEx", "handle", $kernel_handle, "ptr", 0, "int", 0x10000, "int", 0x1000, "int", $PAGE_EXECUTE_READWRITE)
    Local $logBuf = $logAlloc[0]
    
    ; 2. Allocate Trampoline Memory
    Local $hookAlloc = DllCall("kernel32.dll", "ptr", "VirtualAllocEx", "handle", $kernel_handle, "ptr", 0, "int", 0x1000, "int", 0x1000, "int", $PAGE_EXECUTE_READWRITE)
    Local $hookMem = $hookAlloc[0]
    
    ; 3. Construct ASM for Hook
    ; [ESP+40] = Size, [ESP+44] = Buffer
    ; Log Format: [TotalSize(4)] [DataSize(4)] [Data...]
    ; We need a pointer to current write offset. Let's use start of LogBuf as 'WriteOffset'.
    
    ; Initialize WriteOffset to 4 (skip counter)
    MemoryWrite($logBuf, 4, 'dword')
    
    Local $asm = ""
    $asm &= "60" ; pushad
    
    ; Load Buffer and Size
    $asm &= "8B4C2428" ; mov ecx, [esp+40] (Size)
    $asm &= "8B74242C" ; mov esi, [esp+44] (Buffer)
    
    ; Check if size is reasonable (0 < Size < 1000)
    $asm &= "83F900" ; cmp ecx, 0
    $asm &= "7E30"   ; jle Skip (to popad)
    $asm &= "81F9E8030000" ; cmp ecx, 1000
    $asm &= "7F28"   ; jg Skip
    
    ; Load WriteOffset
    $asm &= "A1" & SwapEndian(Hex($logBuf, 8)) ; mov eax, [LogBuf]
    $asm &= "05" & SwapEndian(Hex($logBuf, 8)) ; add eax, LogBuf (Absolute address)
    
    ; Check bounds (Wrap at 0x10000 - 1000)
    ; For simplicity, just reset if > 0xF000
    $asm &= "81F800F00000" ; cmp eax, F000
    ; ... skip logic for circular buffer to keep it simple, user just needs ONE packet.
    
    ; Write Size
    $asm &= "8908" ; mov [eax], ecx
    $asm &= "83C004" ; add eax, 4
    
    ; Copy Data (ESI to EAX)
    $asm &= "8BFE" ; mov edi, esi (Src is ESI, Buffer) -> Wait, rep movsb uses ESI(Src) EDI(Dest)
    $asm &= "8BF8" ; mov edi, eax (Dest)
    $asm &= "F3A4" ; rep movsb
    
    ; Update WriteOffset
    $asm &= "2905" & SwapEndian(Hex($logBuf, 8)) ; sub [LogBuf], LogBuf (Get offset) -> No.
    ; New EAX is absolute address. Subtract LogBuf to get offset.
    $asm &= "2D" & SwapEndian(Hex($logBuf, 8)) ; sub eax, LogBuf
    $asm &= "A3" & SwapEndian(Hex($logBuf, 8)) ; mov [LogBuf], eax
    
    $asm &= "61" ; popad (Label Skip) -> Need to calculate jump
    
    ; Replaced Instructions
    $asm &= "55"       ; push ebp
    $asm &= "8BEC"     ; mov ebp, esp
    $asm &= "83EC50"   ; sub esp, 50h
    
    ; Jump Back
    Local $retAddr = $packetSendFunc + 6
    $asm &= "68" & SwapEndian(Hex($retAddr, 8)) ; push retAddr
    $asm &= "C3"       ; ret
    
    ; Write generic hook code (without logic for now to ensure no crash)
    ; Actually, let's write the full logic.
    ; Fix jumps:
    ; jle Skip (7E 30) -> Need to measure bytes.
    ; The logic above is risky if offset calculation is wrong.
    
    ; Safe Version: Just log the Size for now? No, need data.
    ; I'll rely on AutoIt loop to read "LastPacketPointer".
    ; Instead of complex buffer, just write to "LastPacket" area (1024 bytes).
    ; Structure: [Size] [Data...]
    ; ASM: Copy [Buffer] to [HookMem+1000].
    
    $asm = ""
    $asm &= "60" ; pushad
    
    ; Load Size
    $asm &= "8B4C2428" ; mov ecx, [esp+40] (Size)
    ; Load Buffer
    $asm &= "8B74242C" ; mov esi, [esp+44] (Buffer)
    
    ; Limit copy to 256 bytes
    $asm &= "83F9FF" ; cmp ecx, 255
    $asm &= "7E05"   ; jle ok
    $asm &= "B9FF000000" ; mov ecx, 255
    ; ok:
    
    ; Dest = HookMem + 0x800
    Local $dest = $hookMem + 0x800
    $asm &= "BF" & SwapEndian(Hex($dest + 4, 8)) ; mov edi, dest+4
    
    ; Save Size at dest
    $asm &= "890D" & SwapEndian(Hex($dest, 8)) ; mov [dest], ecx
    
    ; Copy
    $asm &= "F3A4" ; rep movsb
    
    $asm &= "61" ; popad
    
    ; Orig Instructions
    $asm &= "558BEC83EC50"
    
    ; Jump Return
    $asm &= "68" & SwapEndian(Hex($retAddr, 8))
    $asm &= "C3"
    
    ; Write ASM to HookMem
    WriteBinary($asm, $hookMem)
    
    ; 4. Inject JMP at PacketSendFunction
    ; E9 [Offset] + 90
    Local $relOffset = $hookMem - ($packetSendFunc + 5)
    Local $patch = "E9" & SwapEndian(Hex($relOffset, 8)) & "90"
    WriteBinary($patch, $packetSendFunc)
    
    ConsoleWrite("Hook Injected. Listening for packets... (Press ESC or create stop_sniff.txt to stop)" & @CRLF)
    Local $logFile = @ScriptDir & "\sniff_log.txt"
    Local $hFile = FileOpen($logFile, 2)
    ConsoleWrite("Logging to: " & $logFile & @CRLF)
    
    Local $lastSize = 0
    Local $lastContent = ""
    
    While 1
        If FileExists("stop_sniff.txt") Then ExitLoop
        
        Local $size = MemoryRead($dest, 'dword')
        If $size > 0 And $size < 1000 Then
            Local $sData = ""
            For $i = 0 To $size - 1
                $sData &= Hex(MemoryRead($dest + 4 + $i, 'byte'), 2) & " "
            Next
            
            If $sData <> $lastContent Then
                ConsoleWrite("Packet Found: Size=" & $size & " Data=" & $sData & @CRLF)
                FileWrite($hFile, "Packet Found: Size=" & $size & " Data=" & $sData & @CRLF)
                FileFlush($hFile)
                $lastContent = $sData
                ; Clear size to detect new
                MemoryWrite($dest, 0, 'dword')
            EndIf
        EndIf
        Sleep(10)
    WEnd
    
    ConsoleWrite("Restoring original code..." & @CRLF)
    WriteBinary("558BEC83EC50", $packetSendFunc)
    FileClose($hFile)
    
    ; Free memory? (Leak is acceptable for debug script)
EndFunc

SniffPackets()
