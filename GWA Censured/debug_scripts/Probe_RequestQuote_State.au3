#include "lib/GWA2.au3"
#include "lib/Utils-Debugger.au3"

Func ProbeRequestQuoteState()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then 
        ConsoleWrite("Character not found" & @CRLF)
        Return
    EndIf
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\request_quote_probe.txt"
    Local $hFile = FileOpen($logFile, 2)
    FileWrite($hFile, "Starting Probe on RequestQuote..." & @CRLF)
    
    ; 1. Get Address
    ; ScanRequestQuoteFunction: 8B752083FE107614
    Local $startAddr = GetScannedAddress('ScanRequestQuoteFunction', 0)
    
    If $startAddr == 0 Then
        FileWrite($hFile, "ERROR: ScanRequestQuoteFunction not found." & @CRLF)
        FileClose($hFile)
        Return
    EndIf
    
    ; StartAddr points to MOV ESI, [EBP+20] (8B 75 20)
    ; We want to hook CMP ESI, 10 (83 FE 10) which is 3 bytes later.
    Local $hookAddr = $startAddr + 3
    
    FileWrite($hFile, "ScanRequestQuoteFunction: " & Hex($startAddr) & @CRLF)
    FileWrite($hFile, "Hooking at: " & Hex($hookAddr) & @CRLF)
    
    ; 2. Install Probe Hook
    ; Overwriting: CMP ESI, 10 (83 FE 10) - 3 bytes
    ;              JBE 14 (76 14) - 2 bytes
    ; Total 5 bytes.
    
    Local $processHandle = GetProcessHandle()
    Local $hookMem = SafeDllCall13($kernel_handle, 'ptr', 'VirtualAllocEx', _
            'handle', $processHandle, _
            'ptr', 0, _
            'ulong_ptr', 1024, _
            'dword', 0x1000, _
            'dword', 0x40)
    $hookMem = $hookMem[0]
    
    FileWrite($hFile, "Hook Memory Allocated: " & Hex($hookMem) & @CRLF)
    
    ; Probe Structure at $hookMem + 512
    Local $asm = ""
    ; PUSHAD
    $asm &= "60"
    
    ; Log Registers
    ; [Base+4] = EDI
    $asm &= "8BC7"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 4))

    ; [Base+8] = ESI
    $asm &= "8BC6"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 8))

    ; [Base+12] = EBX
    $asm &= "8BC3"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 12))

    ; [Base+16] = EBP
    $asm &= "8BC5"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 16))
    
    ; [Base+20] = EAX
    $asm &= "8B44241C"
    $asm &= "A3" & SwapEndian(Hex($hookMem + 512 + 20))
    
    ; Inc Counter
    $asm &= "FF05" & SwapEndian(Hex($hookMem + 512))
    
    ; POPAD
    $asm &= "61"
    
    ; Execute Original Logic
    ; CMP ESI, 10 -> 83 FE 10
    $asm &= "83FE10"
    
    ; JBE +14 (Original behavior)
    ; Target Branch Taken = HookAddr + 5 + 0x14
    Local $branchTakenAddr = $hookAddr + 5 + 0x14
    Local $branchNotTakenAddr = $hookAddr + 5
    
    ; JA Stick (Not Taken)
    ; JA branchNotTakenAddr
    $asm &= "0F87"
    Local $len = StringLen($asm) / 2
    Local $src = $hookMem + $len + 4
    Local $rel = $branchNotTakenAddr - $src
    $asm &= SwapEndian(Hex($rel, 8))
    
    ; Else JMP BranchTaken
    $asm &= "E9"
    $len = StringLen($asm) / 2
    $src = $hookMem + $len + 4
    $rel = $branchTakenAddr - $src
    $asm &= SwapEndian(Hex($rel, 8))
    
    WriteBinary($asm, $hookMem)
    
    ; Write Detour
    Local $detour = "E9" & SwapEndian(Hex($hookMem - $hookAddr - 5, 8))
    WriteBinary($detour, $hookAddr)
    
    ConsoleWrite("Probe installed at RequestQuote. Monitoring..." & @CRLF)
    
    Local $lastCount = 0
    Local $probeBase = $hookMem + 512
    
    While 1
        Local $count = MemoryRead($probeBase)
        If $count <> $lastCount Then
             Local $edi = MemoryRead($probeBase + 4)
             Local $esi = MemoryRead($probeBase + 8)
             Local $ebx = MemoryRead($probeBase + 12)
             Local $ebp = MemoryRead($probeBase + 16)
             Local $eax = MemoryRead($probeBase + 20)
             
             Local $msg = "Probe Hit! EDI=" & Hex($edi) & " ESI=" & Hex($esi) & " EBX=" & Hex($ebx) & " EBP=" & Hex($ebp) & " EAX=" & Hex($eax) & @CRLF
             ConsoleWrite($msg) 
             $hFile = FileOpen($logFile, 1)
             FileWrite($hFile, $msg)
             
             ; Inspect memory at likely pointers
             Local $ebxMem = MemoryRead($ebx, 'byte[64]')
             FileWrite($hFile, "  EBX Mem: " & Hex($ebxMem) & @CRLF)
             Local $esiMem = MemoryRead($esi, 'byte[64]')
             FileWrite($hFile, "  ESI Mem: " & Hex($esiMem) & @CRLF)
             Local $eaxMem = MemoryRead($eax, 'byte[64]')
             FileWrite($hFile, "  EAX Mem: " & Hex($eaxMem) & @CRLF)
             
             FileClose($hFile)
             
             $lastCount = $count
        EndIf
        Sleep(100)
    WEnd
EndFunc

ProbeRequestQuoteState()
