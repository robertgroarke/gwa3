#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Craft Dialog Child Frame Explorer ===" & @CRLF)

; Autolaunch BEASTRIT
If Not GWLauncher_AutoLaunchAndConnect("B E A S T R I T") Then
    ConsoleWrite("Failed to launch" & @CRLF)
    Exit
EndIf

ScanAndUpdateGameClients()
Local $clientIdx = FindClientIndexByCharacterName("B E A S T R I T")
SelectClient($clientIdx)
InitializeGameClientForGWA2(True)
Local $ph = GetProcessHandle()

ConsoleWrite("Map: " & GetMapID() & " Char: " & GetCharacterName() & @CRLF)

; Travel to Embark Beach
If GetMapID() <> 857 Then
    ConsoleWrite("Traveling to Embark Beach..." & @CRLF)
    TravelToOutpost(857)
    Sleep(10000)
EndIf

; Walk to Eyja and open dialog
ConsoleWrite("Walking to Eyja..." & @CRLF)
If Not GoToConsumableTrader("Eyja") Then
    ConsoleWrite("Failed to reach Eyja" & @CRLF)
    Exit
EndIf
ConsoleWrite("Eyja dialog open!" & @CRLF)
Sleep(2000)

; Find Merchant frame
Local $merchantFrame = GetFrameByHash(3613855137)
If $merchantFrame[0] = 0 Then
    ConsoleWrite("Merchant frame not found!" & @CRLF)
    Exit
EndIf

Local $merchantPtr = Int($merchantFrame[0])
ConsoleWrite("Merchant frame: ptr=0x" & Hex($merchantPtr, 8) & " id=" & $merchantFrame[1] & @CRLF)

; Dump the child tree using TList navigation
ConsoleWrite(@CRLF & "=== Merchant Child Tree ===" & @CRLF)
_WalkChildTree($ph, $merchantPtr, 0, "root")

ConsoleWrite(@CRLF & "=== Navigating path [0,0,6] (Collector.Exchange) ===" & @CRLF)
Local $target = _GetChildByPath($ph, $merchantPtr, "0,0,6")
If $target <> 0 Then
    Local $targetHash = MemoryRead($ph, $target + 0x134, 'dword')
    Local $targetFid = MemoryRead($ph, $target + 0xBC, 'dword')
    ConsoleWrite("Found! ptr=0x" & Hex($target, 8) & " fid=" & $targetFid & " hash=" & $targetHash & @CRLF)
Else
    ConsoleWrite("Path [0,0,6] not found" & @CRLF)
EndIf

ConsoleWrite(@CRLF & "=== DONE ===" & @CRLF)

; Walk children of a frame via TList at FrameRelation+0x10 (frame+0x138)
Func _WalkChildTree($processHandle, $framePtr, $depth, $label)
    If $depth > 6 Or $framePtr = 0 Or $framePtr < 0x10000 Then Return

    Local $hash = MemoryRead($processHandle, $framePtr + 0x134, 'dword')
    Local $fid = MemoryRead($processHandle, $framePtr + 0xBC, 'dword')
    Local $childOffId = MemoryRead($processHandle, $framePtr + 0xB8, 'dword')
    Local $visFlags = MemoryRead($processHandle, $framePtr + 0x18, 'dword')
    Local $frameType = MemoryRead($processHandle, $framePtr + 0x20, 'dword')

    Local $indent = ""
    For $i = 1 To $depth
        $indent &= "  "
    Next
    ConsoleWrite($indent & $label & ": fid=" & $fid & " hash=" & $hash & _
        " childOff=" & $childOffId & " vis=0x" & Hex($visFlags, 4) & _
        " type=0x" & Hex($frameType, 4) & @CRLF)

    ; Read children via TList<FrameRelation> siblings at frame+0x138
    ; TList contains TLink nodes. The list head is at frame+0x138.
    ; TLink: [prev_link(4), next_node(4)] = 8 bytes
    ; The list head's next_node points to the first child's FrameRelation.
    ; FrameRelation is at frame+0x128, so Frame = FrameRelation - 0x128.

    Local $listHead = $framePtr + 0x138  ; Address of TList (which is a TLink)
    Local $firstNodePtr = MemoryRead($processHandle, $listHead + 4, 'dword')  ; next_node

    If BitAND($firstNodePtr, 1) Then Return  ; Sentinel — no children

    ; Walk the linked list
    Local $childIndex = 0
    Local $currentRelation = $firstNodePtr  ; Points to child's FrameRelation
    Local $visited = 0

    While $currentRelation <> 0 And $currentRelation > 0x10000 And $visited < 50
        ; FrameRelation is at offset 0x128 in Frame, so Frame = FrameRelation - 0x128
        Local $childFrame = $currentRelation - 0x128
        If $childFrame > 0x10000 Then
            _WalkChildTree($processHandle, $childFrame, $depth + 1, "[" & $childIndex & "]")
        EndIf

        ; Move to next sibling: read the TLink at currentRelation+0x10 (siblings list)
        ; Actually, the siblings TLink is embedded IN the FrameRelation at offset 0x10
        ; TLink: [prev_link, next_node] at FrameRelation+0x10
        Local $nextNode = MemoryRead($processHandle, $currentRelation + 0x10 + 4, 'dword')  ; siblings.next_node

        If BitAND($nextNode, 1) Then ExitLoop  ; Sentinel
        If $nextNode = $currentRelation Then ExitLoop  ; Self-referencing
        If $nextNode = $firstNodePtr Then ExitLoop  ; Circular back to first

        $currentRelation = $nextNode
        $childIndex += 1
        $visited += 1
    WEnd
EndFunc

; Navigate a child path like "0,0,6" from a parent frame
Func _GetChildByPath($processHandle, $parentPtr, $path)
    Local $parts = StringSplit($path, ",")
    Local $current = $parentPtr

    For $p = 1 To $parts[0]
        Local $targetOffset = Int($parts[$p])
        Local $found = False

        ; Walk children to find the one with matching index
        Local $listHead = $current + 0x138
        Local $firstNodePtr = MemoryRead($processHandle, $listHead + 4, 'dword')
        If BitAND($firstNodePtr, 1) Then Return 0

        Local $childIndex = 0
        Local $currentRelation = $firstNodePtr

        While $currentRelation <> 0 And $currentRelation > 0x10000
            If $childIndex = $targetOffset Then
                $current = $currentRelation - 0x128
                $found = True
                ExitLoop
            EndIf

            Local $nextNode = MemoryRead($processHandle, $currentRelation + 0x10 + 4, 'dword')
            If BitAND($nextNode, 1) Or $nextNode = $currentRelation Or $nextNode = $firstNodePtr Then ExitLoop
            $currentRelation = $nextNode
            $childIndex += 1
        WEnd

        If Not $found Then Return 0
    Next

    Return $current
EndFunc
