#include "lib/GWA2.au3"
#include "lib/Utils-Debugger.au3"

Func ProbeEDIState()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then 
        ConsoleWrite("Character not found" & @CRLF)
        Return
    EndIf
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\probe_edi.txt"
    Local $hFile = FileOpen($logFile, 2)
    FileWrite($hFile, "Starting Probe on EDI Site..." & @CRLF)
    
    ; 1. Find the Anchor (RequestQuote)
    Local $baseAddr = GetScannedAddress('ScanRequestQuoteFunction', 0)
    FileWrite($hFile, "RequestQuote Base: " & Hex($baseAddr) & @CRLF)
    
    ; 2. Find '83 FF 10' (CMP EDI, 10) nearby
    ; Read 200 bytes
    Local $bytes = MemoryRead($baseAddr, 'byte[200]')
    Local $hex = Hex($bytes)
    
    Local $pattern = "83FF107614"
    Local $pos = StringInStr($hex, $pattern)
    
    If $pos == 0 Then
        FileWrite($hFile, "ERROR: CMP EDI Pattern not found!" & @CRLF)
        FileClose($hFile)
        Return
    EndIf
    
    Local $offset = ($pos - 1) / 2
    Local $hookAddr = $baseAddr + $offset
    FileWrite($hFile, "Found CMP EDI at: " & Hex($hookAddr) & " (Offset +" & Hex($offset) & ")" & @CRLF)
    
    ; 3. Install Probe Hook
    ; Overwriting: CMP EDI, 10 (83 FF 10) - 3 bytes
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
    
    ; Inc Counter
    $asm &= "FF05" & SwapEndian(Hex($hookMem + 512))
    
    ; POPAD
    $asm &= "61"
    
    ; Execute Original Logic
    ; CMP EDI, 10 -> 83 FF 10
    $asm &= "83FF10"
    
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
    
    ConsoleWrite("Probe installed at EDI Site. Monitoring..." & @CRLF)
    
    Local $lastCount = 0
    Local $probeBase = $hookMem + 512
    
    While 1
        Local $count = MemoryRead($probeBase)
        If $count <> $lastCount Then
             Local $edi = MemoryRead($probeBase + 4)
             Local $esi = MemoryRead($probeBase + 8)
             Local $ebx = MemoryRead($probeBase + 12)
             
             Local $msg = "Probe Hit! EDI=" & Hex($edi) & " ESI=" & Hex($esi) & " EBX=" & Hex($ebx) & @CRLF
             ConsoleWrite($msg) 
             $hFile = FileOpen($logFile, 1)
             FileWrite($hFile, $msg)
             
             ; Inspect memory at EBX
             Local $ebxMem = MemoryRead($ebx, 'byte[64]')
             FileWrite($hFile, "  EBX Mem: " & Hex($ebxMem) & @CRLF)
             
             FileClose($hFile)
             
             $lastCount = $count
        EndIf
        Sleep(100)
    WEnd
EndFunc

ProbeEDIState()
