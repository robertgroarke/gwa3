#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_craft_frame_discovery.au3
;
; Full pipeline: Launch BEASTRIT → Click Play → Travel to Embark Beach →
; Walk to Eyja [Consumables] → Open dialog → Dump all visible frames
; to discover craft UI frame hashes.
;
; If already in-game, skips launch/play steps.
; =============================================================================

ConsoleWrite("=== Craft Frame Discovery ===" & @CRLF)

; --- Connect to running client or launch fresh ---
ScanAndUpdateGameClients()
Local $needsLaunch = ($game_clients[0][0] = 0)

If $needsLaunch Then
	ConsoleWrite("No GW client. Launching BEASTRIT..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	Local $launchResult = GWLauncher_LaunchAccount($accounts, $accIdx)
	ConsoleWrite("PID=" & $launchResult[0] & ", waiting 40s for char select..." & @CRLF)
	Sleep(40000)
	ScanAndUpdateGameClients()
EndIf

SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwHWnd = $game_clients[$game_clients[0][0]][2]
ConsoleWrite("Connected PID=" & $game_clients[$game_clients[0][0]][0] & @CRLF)
WinActivate($gwHWnd)
Sleep(1000)

; --- If at char select, click Play ---
If IsAtCharSelect() Then
	ConsoleWrite("At char select. Clicking Play..." & @CRLF)

	; Initialize GWCA and click
	If Not ClickFrameButton($FRAME_HASH_PLAY_BUTTON) Then
		ConsoleWrite("ERROR: Failed to click Play" & @CRLF)
		Exit 1
	EndIf

	; Wait for map to load
	ConsoleWrite("Waiting for map load..." & @CRLF)
	Local $loadStart = TimerInit()
	While GetMyID() = 0 Or GetMaxAgents() = 0
		Sleep(500)
		If TimerDiff($loadStart) > 60000 Then
			ConsoleWrite("ERROR: Map load timeout" & @CRLF)
			Exit 1
		EndIf
	WEnd
	Sleep(3000)  ; extra settle time
	ConsoleWrite("In game! MyID=" & GetMyID() & " MapID=" & GetMapID() & @CRLF)
EndIf

; --- Travel to Embark Beach if not already there ---
Local $MAP_EMBARK_BEACH = 857
If GetMapID() <> $MAP_EMBARK_BEACH Then
	ConsoleWrite("Traveling to Embark Beach (map " & $MAP_EMBARK_BEACH & ")..." & @CRLF)
	TravelToOutpost($MAP_EMBARK_BEACH)
	WaitMapLoading($MAP_EMBARK_BEACH, 30000)
	Sleep(3000)
EndIf
ConsoleWrite("At Embark Beach. MapID=" & GetMapID() & @CRLF)

; --- Walk to Eyja and open dialog ---
ConsoleWrite("Walking to Eyja [Consumables]..." & @CRLF)

; Eyja coordinates in Embark Beach: (3336, 627)
Local $eyjaX = 3336
Local $eyjaY = 627
MoveTo($eyjaX, $eyjaY)
Sleep(1000)

; Find Eyja NPC
Local $eyjaAgent = GetNearestNPCToCoords($eyjaX, $eyjaY)
If $eyjaAgent = 0 Then
	ConsoleWrite("ERROR: Eyja NPC not found near coords" & @CRLF)
	; Try broader search
	ConsoleWrite("Trying MoveTo closer..." & @CRLF)
	MoveTo($eyjaX, $eyjaY)
	Sleep(2000)
	$eyjaAgent = GetNearestNPCToCoords($eyjaX, $eyjaY)
EndIf

If $eyjaAgent = 0 Then
	ConsoleWrite("ERROR: Still can't find Eyja" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Found Eyja agent=" & $eyjaAgent & @CRLF)

; Walk to and interact
GoToNPC($eyjaAgent)
Sleep(1000)

; Screenshot before dialog
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_before_dialog.png', $gwHWnd)

; Open dialog
ConsoleWrite("Opening dialog..." & @CRLF)
Dialog($eyjaAgent)
Sleep(2000)

; Screenshot with dialog open
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_dialog_open.png', $gwHWnd)

; =============================================================================
; FRAME DISCOVERY: Dump all visible frames
; =============================================================================
ConsoleWrite(@CRLF & "=== FRAME DUMP (with dialog open) ===" & @CRLF)

Local $frameArrayAddr = GetLabel('FrameArray')
Local $bufferPtr = MemoryRead($processHandle, $frameArrayAddr, 'dword')
Local $arraySize = MemoryRead($processHandle, $frameArrayAddr + 4, 'dword')

ConsoleWrite("FrameArray: buffer=0x" & Hex($bufferPtr) & " size=" & $arraySize & @CRLF)

If $arraySize > 5000 Or $arraySize <= 0 Or $bufferPtr < 0x10000 Then
	ConsoleWrite("ERROR: Invalid frame array" & @CRLF)
	Exit 1
EndIf

; Dump ALL visible frames with details
Local $visibleCount = 0
Local $output = ""
For $i = 0 To $arraySize - 1
	Local $framePtr = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
	If $framePtr = 0 Or $framePtr < 0x10000 Then ContinueLoop

	Local $state = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_STATE, 'dword')
	Local $created = (BitAND($state, $FRAME_STATE_CREATED) <> 0)
	Local $hidden = (BitAND($state, $FRAME_STATE_HIDDEN) <> 0)

	; Only show created + visible frames
	If Not $created Or $hidden Then ContinueLoop

	Local $frameHash = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_HASH_ID, 'dword')
	Local $frameId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_FRAME_ID, 'dword')
	Local $childOff = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
	Local $disabled = (BitAND($state, $FRAME_STATE_DISABLED) <> 0)

	; Read callbacks
	Local $cbBuf = MemoryRead($processHandle, $framePtr + 0xA8, 'dword')
	Local $cbSize = MemoryRead($processHandle, $framePtr + 0xAC, 'dword')
	Local $cb0 = 0
	If $cbSize > 0 And $cbBuf > 0x10000 Then
		$cb0 = MemoryRead($processHandle, $cbBuf, 'dword')
	EndIf

	; Read parent relation to understand hierarchy
	Local $relParent = MemoryRead($processHandle, $framePtr + 0x128, 'dword')
	Local $parentId = -1
	If $relParent > 0x10000 Then
		; FrameRelation is at frame+0x128. Parent FrameRelation at [frame+0x128].
		; Parent Frame = parentRelation - 0x128
		Local $parentFrame = $relParent - 0x128
		If $parentFrame > 0x10000 Then
			$parentId = MemoryRead($processHandle, $parentFrame + $FRAME_OFFSET_FRAME_ID, 'dword')
		EndIf
	EndIf

	; Read first 20 chars of frame text/label if available (at offset 0x0)
	; Frame field1_0x0 is a wchar label pointer sometimes
	Local $f0 = MemoryRead($processHandle, $framePtr, 'dword')

	$output &= "  id=" & $frameId & " hash=" & $frameHash & " childOff=" & $childOff & _
		" parent=" & $parentId & " disabled=" & $disabled & _
		" cbs=" & $cbSize & " cb0=0x" & Hex($cb0) & @CRLF
	$visibleCount += 1
Next

ConsoleWrite("Found " & $visibleCount & " visible frames:" & @CRLF)
ConsoleWrite($output)

; =============================================================================
; Now try to identify known frames and look for craft-specific ones
; =============================================================================
ConsoleWrite(@CRLF & "=== KNOWN FRAME CHECK ===" & @CRLF)

; Check known merchant frames
Local $knownFrames[3][2] = [ _
	[3613855137, "Merchant"], _
	[1532320307, "Merchant Buy Button"], _
	[684387150, "Salvage Window"] _
]

For $i = 0 To 2
	Local $fr = GetFrameByHash($knownFrames[$i][0])
	If $fr[0] <> 0 Then
		Local $vis = IsFrameVisible($knownFrames[$i][0])
		ConsoleWrite("  " & $knownFrames[$i][1] & ": FOUND (id=" & $fr[1] & " visible=" & $vis & ")" & @CRLF)
	Else
		ConsoleWrite("  " & $knownFrames[$i][1] & ": not found" & @CRLF)
	EndIf
Next

; =============================================================================
; Save full dump to file for analysis
; =============================================================================
Local $dumpFile = @ScriptDir & '\tests\craft_frame_dump.txt'
FileDelete($dumpFile)

Local $fullDump = "=== Frame Dump at Eyja Craft Dialog ===" & @CRLF
$fullDump &= "Time: " & @YEAR & "-" & @MON & "-" & @MDAY & " " & @HOUR & ":" & @MIN & @CRLF
$fullDump &= "Total visible: " & $visibleCount & @CRLF & @CRLF

; Re-dump with more detail to file
For $i = 0 To $arraySize - 1
	Local $framePtr = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
	If $framePtr = 0 Or $framePtr < 0x10000 Then ContinueLoop

	Local $state = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_STATE, 'dword')
	If BitAND($state, $FRAME_STATE_CREATED) = 0 Then ContinueLoop

	Local $frameHash = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_HASH_ID, 'dword')
	Local $frameId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_FRAME_ID, 'dword')
	Local $childOff = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
	Local $hidden = (BitAND($state, $FRAME_STATE_HIDDEN) <> 0)
	Local $disabled = (BitAND($state, $FRAME_STATE_DISABLED) <> 0)
	Local $cbBuf = MemoryRead($processHandle, $framePtr + 0xA8, 'dword')
	Local $cbSize = MemoryRead($processHandle, $framePtr + 0xAC, 'dword')

	Local $relParent = MemoryRead($processHandle, $framePtr + 0x128, 'dword')
	Local $parentId = -1
	If $relParent > 0x10000 Then
		Local $pf = $relParent - 0x128
		If $pf > 0x10000 Then $parentId = MemoryRead($processHandle, $pf + $FRAME_OFFSET_FRAME_ID, 'dword')
	EndIf

	$fullDump &= "id=" & $frameId & " hash=" & $frameHash & " childOff=" & $childOff & _
		" parent=" & $parentId & " state=0x" & Hex($state) & _
		" vis=" & (Not $hidden) & " dis=" & $disabled & " cbs=" & $cbSize & @CRLF
Next

FileWrite($dumpFile, $fullDump)
ConsoleWrite(@CRLF & "Full dump saved to tests\craft_frame_dump.txt" & @CRLF)

ConsoleWrite(@CRLF & "=== DONE ===" & @CRLF)
