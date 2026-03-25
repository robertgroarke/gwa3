#RequireAdmin
#Region Directives
#AutoIt3Wrapper_UseX64=n
#EndRegion

#include "lib\GWA2.au3"
#include <Array.au3>

Global $targetName = "L I L B I S C U I T"
; The Level 1 Pointer found by the previous scan
Global $startAddr = 0x2CE373C4 
Global $logFile = @ScriptDir & "\pointer_chain_trace.txt"
Global $hFile = FileOpen($logFile, 2)

Func _Log($msg)
    ConsoleWrite($msg & @CRLF)
    FileWrite($hFile, $msg & @CRLF)
EndFunc

Func _Main()
    _Log("Starting Pointer Chain Trace...")
    _Log("Target Level 1 Pointer: " & Hex(Int($startAddr)))
    
    ScanAndUpdateGameClients()
    Local $index = -1
    For $i = 1 To $game_clients[0][0]
        If $game_clients[$i][3] == $targetName Then
            $index = $i
            ExitLoop
        EndIf
    Next
    
    If $index == -1 Then
        _Log("Critical: Character '" & $targetName & "' not found! Defaulting to first client.")
        $index = 1
    EndIf
    
    SelectClient($index)
    InitializeGameClientData()
    _Log("Attached to client: " & GetCharacterName())
    
    ; Start the chain climb
    Local $currentLevel = 1
    Local $targets[1]
    $targets[0] = $startAddr
    
    ; We loop for a max depth, e.g., 5 levels
    For $level = 2 To 6
        _Log("--------------------------------------------------")
        _Log("Scanning Level " & $level & " (Looking for pointers to Level " & ($level - 1) & ")...")
        
        Local $nextTargets[1] = [0]
        Local $foundStatic = False
        
        ; For each target in the current list, scan for pointers to it
        For $t = 0 To UBound($targets) - 1
            Local $tgt = $targets[$t]
            Local $hexTgt = _IntToHex(Int($tgt))
            _Log("  Searching for: " & Hex(Int($tgt)) & "...")
            
            Local $matches = _ScanMemoryForValue($hexTgt)
            _Log("  -> Found " & (UBound($matches) - 1) & " matches.")
            
            For $m = 1 To UBound($matches) - 1
                Local $ptrAddr = $matches[$m]
                _Log("     -> Pointer at: " & Hex(Int($ptrAddr)))
                
                ; CHECK FOR STATIC BASE
                ; GW executable is usually around 0x00400000 - 0x00A00000
                If Int($ptrAddr) >= 0x00400000 And Int($ptrAddr) < 0x00A00000 Then
                    _Log("     !!! FOUND STATIC BASE POINTER !!!")
                    _Log("     Address: " & Hex(Int($ptrAddr)))
                    _Log("     Offset from 0x00400000: " & Hex(Int($ptrAddr) - 0x00400000))
                    $foundStatic = True
                Else
                     ; Add to next targets for deeper scan
                     _ArrayAdd($nextTargets, $ptrAddr)
                EndIf
            Next
        Next
        
        If $foundStatic Then
            _Log("Static Base Found! Trace complete.")
            MsgBox(0, "Success", "Static Base Found! Check log.")
            ExitLoop
        EndIf
        
        If UBound($nextTargets) <= 1 Then
            _Log("Dead end. No pointers found to this level.")
            ExitLoop
        EndIf
        
        ; Prune duplicates and set targets for next level
        ; (Simple array copy for now, duplicates minimal usually)
        Local $cleanTargets[UBound($nextTargets)-1]
        For $k = 1 To UBound($nextTargets)-1
            $cleanTargets[$k-1] = $nextTargets[$k]
        Next
        $targets = $cleanTargets
        
    Next
    
    FileClose($hFile)
    MsgBox(0, "Done", "Trace Complete. Check log.")
EndFunc

Func _IntToHex($int)
    Local $hex = Hex(Int($int), 8) ; 4 bytes
    Local $s = ""
    $s &= StringMid($hex, 7, 2)
    $s &= StringMid($hex, 5, 2)
    $s &= StringMid($hex, 3, 2)
    $s &= StringMid($hex, 1, 2)
    Return $s
EndFunc

; Custom Memory Scanner (Pattern Based)
Func _ScanMemoryForValue($hexString)
    Local $matches[1] = [0]
    Local $processHandle = GetProcessHandle()
    Local $currentSearchAddress = 0x00000000
    Local $memoryInfos = SafeDllStructCreate($MEMORY_INFO_STRUCT_TEMPLATE)
    Local $maxAddress = 0x7FFFFFFF ; Full User Space
    
    While $currentSearchAddress < $maxAddress
        SafeDllCall11($kernel_handle, 'int', 'VirtualQueryEx', 'int', $processHandle, 'int', $currentSearchAddress, 'ptr', DllStructGetPtr($memoryInfos), 'int', DllStructGetSize($memoryInfos))
        Local $regionSize = DllStructGetData($memoryInfos, 'RegionSize')
        Local $state = DllStructGetData($memoryInfos, 'State')
        Local $protect = DllStructGetData($memoryInfos, 'Protect')

        ; If memory is committed and readable/writable (usually data is RW)
        If $regionSize > 0 And $state = 0x1000 And BitAND($protect, 0x100) = 0 And (BitAND($protect, 0x04) Or BitAND($protect, 0x40)) Then ; PAGE_READWRITE
             Local $buffer = SafeDllStructCreate('byte[' & $regionSize & ']')
             SafeDllCall13($kernel_handle, 'int', 'ReadProcessMemory', 'int', $processHandle, 'int', $currentSearchAddress, 'ptr', DllStructGetPtr($buffer), 'int', DllStructGetSize($buffer), 'int', 0)
             Local $binData = DllStructGetData($buffer, 1)
             Local $hexData = Hex($binData)
             
             Local $pos = 0
             While 1
                $pos = StringInStr($hexData, $hexString, 0, 1, $pos + 1)
                If $pos == 0 Then ExitLoop
                
                ; Offset in bytes = (Pos - 1) / 2
                Local $offset = ($pos - 1) / 2
                Local $addr = $currentSearchAddress + $offset
                _ArrayAdd($matches, $addr)
             WEnd
        EndIf
        
        $currentSearchAddress += $regionSize
        If $regionSize == 0 Then $currentSearchAddress += 4096 ; Safety
    WEnd
    
    Return $matches
EndFunc

_Main()
