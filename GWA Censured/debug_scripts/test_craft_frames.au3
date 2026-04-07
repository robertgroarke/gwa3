#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Child Frame Walker ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then Exit
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Find Merchant frame
Local $mf = GetFrameByHash(3613855137)
If $mf[0] = 0 Then
    ConsoleWrite("Merchant frame not found" & @CRLF)
    Exit
EndIf
Local $fp = Int($mf[0])
ConsoleWrite("Merchant frame: 0x" & Hex($fp, 8) & " fid=" & $mf[1] & @CRLF)

; TList<FrameRelation> siblings is at FrameRelation+0x10 = frame+0x138
; TList layout: [offset(4), TLink.prev_link(4), TLink.next_node(4)]
; TLink.next_node points to the child FrameRelation (or has bit 0 set as sentinel)
; FrameRelation is at frame+0x128, so Frame = FrameRelation_ptr - 0x128

; The TList.offset field tells us the offset of the TLink within the child structure
; This lets us find the next sibling's TLink from the child's FrameRelation

ConsoleWrite(@CRLF & "=== Walking children ===" & @CRLF)
_WalkChildren($ph, $fp, 0, "")

ConsoleWrite(@CRLF & "=== Navigating [0,0,6] ===" & @CRLF)
Local $result = _NavPath($ph, $fp, "0,0,6")
If $result <> 0 Then
    ConsoleWrite("Path [0,0,6] -> Frame 0x" & Hex($result, 8) & _
        " fid=" & MemoryRead($ph, $result + 0xBC, 'dword') & _
        " hash=" & MemoryRead($ph, $result + 0x134, 'dword') & @CRLF)
EndIf

ConsoleWrite(@CRLF & "=== DONE ===" & @CRLF)

Func _GetChildren($ph, $framePtr)
    ; Returns array of child Frame pointers
    Local $children[0]

    ; TList at frame+0x138: [offset(4), prev_link(4), next_node(4)]
    Local $tlOffset = MemoryRead($ph, $framePtr + 0x138, 'dword')  ; offset of TLink in child
    Local $firstChild = MemoryRead($ph, $framePtr + 0x140, 'dword')  ; next_node = first child

    If $firstChild = 0 Or $firstChild < 0x10000 Or BitAND($firstChild, 1) Then
        Return $children
    EndIf

    ; The first child pointer from next_node is a FrameRelation*
    ; Frame = FrameRelation - 0x128
    Local $current = $firstChild
    Local $listHeadAddr = $framePtr + 0x138  ; Address of this TList
    Local $count = 0

    While $current <> 0 And $current > 0x10000 And Not BitAND($current, 1) And $count < 50
        ; This FrameRelation* -> Frame
        Local $childFrame = $current - 0x128
        ReDim $children[$count + 1]
        $children[$count] = $childFrame
        $count += 1

        ; Find next sibling: the child's FrameRelation has its own TLink
        ; at FrameRelation + $tlOffset. That TLink's next_node is the next sibling.
        Local $childTLink = $current + $tlOffset
        Local $nextSibling = MemoryRead($ph, $childTLink + 4, 'dword')  ; TLink.next_node

        If $nextSibling = 0 Or $nextSibling < 0x10000 Or BitAND($nextSibling, 1) Then ExitLoop
        If $nextSibling = $firstChild Then ExitLoop  ; Circular
        $current = $nextSibling
    WEnd

    Return $children
EndFunc

Func _WalkChildren($ph, $framePtr, $depth, $prefix)
    If $depth > 5 Or $framePtr = 0 Or $framePtr < 0x10000 Then Return

    Local $fid = MemoryRead($ph, $framePtr + 0xBC, 'dword')
    Local $hash = MemoryRead($ph, $framePtr + 0x134, 'dword')
    Local $childOff = MemoryRead($ph, $framePtr + 0xB8, 'dword')

    Local $indent = ""
    For $i = 1 To $depth
        $indent &= "  "
    Next
    ConsoleWrite($indent & $prefix & "fid=" & $fid & " hash=" & $hash & " childOff=" & $childOff & @CRLF)

    Local $kids = _GetChildren($ph, $framePtr)
    For $k = 0 To UBound($kids) - 1
        _WalkChildren($ph, $kids[$k], $depth + 1, "[" & $k & "] ")
    Next
EndFunc

Func _NavPath($ph, $framePtr, $path)
    Local $parts = StringSplit($path, ",")
    Local $cur = $framePtr
    For $p = 1 To $parts[0]
        Local $idx = Int($parts[$p])
        Local $kids = _GetChildren($ph, $cur)
        If $idx >= UBound($kids) Then
            ConsoleWrite("Path error: child " & $idx & " not found (only " & UBound($kids) & " children)" & @CRLF)
            Return 0
        EndIf
        $cur = $kids[$idx]
    Next
    Return $cur
EndFunc
