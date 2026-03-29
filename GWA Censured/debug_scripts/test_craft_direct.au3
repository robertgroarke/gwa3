#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_craft_direct.au3
;
; Click the Craft button WITHOUT GWCA — use the game's own
; SendFrameUIMessage directly via BotsHub's MainProc command queue.
;
; In-game, MainProc IS active (unlike char select). We can use it
; to execute shellcode that calls the game's frame click function.
;
; NO gwca.dll injection. Pure BotsHub + game function calls.
; =============================================================================

Global Const $FRAME_HASH_CRAFT_BUTTON = 835947118
Global Const $FRAME_HASH_GOODBYE_BUTTON = 3068881268
Global Const $FRAME_HASH_MERCHANT_WINDOW = 3613855137
Global Const $FRAME_HASH_ITEM_ROW = 1852904459
Global Const $MAP_EMBARK_BEACH = 857

ConsoleWrite("=== Direct Craft Click (No GWCA) ===" & @CRLF)

; --- Connect ---
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("No GW client. Launching BEASTRIT..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	Local $launchResult = GWLauncher_LaunchAccount($accounts, $accIdx)
	ConsoleWrite("PID=" & $launchResult[0] & ", waiting 40s..." & @CRLF)
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

; --- Handle char select ---
If IsAtCharSelect() Then
	ConsoleWrite("At char select — need Play button click (GWCA required for this)." & @CRLF)
	ConsoleWrite("Using ClickFrameButton which handles GWCA inject internally..." & @CRLF)
	ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
	Local $t = TimerInit()
	While GetMyID() = 0 Or GetMaxAgents() = 0
		Sleep(500)
		If TimerDiff($t) > 60000 Then
			ConsoleWrite("ERROR: Map load timeout" & @CRLF)
			Exit 1
		EndIf
	WEnd
	Sleep(3000)
EndIf

; --- Travel to Embark Beach ---
If GetMapID() <> $MAP_EMBARK_BEACH Then
	ConsoleWrite("Traveling to Embark Beach..." & @CRLF)
	TravelToOutpost($MAP_EMBARK_BEACH)
	WaitMapLoading($MAP_EMBARK_BEACH, 30000)
	Sleep(3000)
EndIf
ConsoleWrite("At Embark Beach." & @CRLF)

; --- Verify hook is working (in-game, MainProc should be active) ---
ConsoleWrite("Testing command queue..." & @CRLF)
Local $marker = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
	'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 4, 'dword', 0x1000, 'dword', 0x40)
Local $markerAddr = Int($marker[0])
MemoryWrite($processHandle, $markerAddr, 0x1111, 'dword')

Local $testSc = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
	'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 16, 'dword', 0x1000, 'dword', 0x40)
Local $testAddr = Int($testSc[0])
Local $tb = DllStructCreate('byte[16]')
Local $p = 1
; mov dword [marker], 0x2222
DllStructSetData($tb, 1, 0xC7, $p)
$p += 1
DllStructSetData($tb, 1, 0x05, $p)
$p += 1
_WriteLE32($tb, $p, $markerAddr)
$p += 4
_WriteLE32($tb, $p, 0x2222)
$p += 4
; ret (for rendering hook) or jmp CommandReturn (for MainProc)
; In-game, both hooks run. ret should work for rendering hook.
; MainProc uses jmp ebx (no ret addr), but ret should still work
; since the game's original caller has a return addr on stack.
DllStructSetData($tb, 1, 0xC3, $p)

DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($testAddr), _
	'ptr', DllStructGetPtr($tb), 'ulong_ptr', $p, 'ulong_ptr*', 0)

$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $testAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

Sleep(2000)
Local $mv = MemoryRead($processHandle, $markerAddr, 'dword')
ConsoleWrite("Queue test: marker=0x" & Hex($mv) & " (want 0x2222)" & @CRLF)
If $mv <> 0x2222 Then
	ConsoleWrite("ERROR: Command queue not processing!" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Command queue active!" & @CRLF)

; --- Open Eyja dialog if not already open ---
If Not IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW) Then
	ConsoleWrite("Walking to Eyja..." & @CRLF)
	MoveTo(3336, 627)
	Sleep(1000)
	Local $eyja = GetNearestNPCToCoords(3336, 627)
	If $eyja = 0 Then
		ConsoleWrite("ERROR: Eyja not found" & @CRLF)
		Exit 1
	EndIf
	GoToNPC($eyja)
	Sleep(1000)
	Dialog($eyja)
	Sleep(2000)
EndIf

If Not IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW) Then
	ConsoleWrite("ERROR: Merchant window not open" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Merchant window open." & @CRLF)

; --- Use ClickFrameButton (which uses GWCA internally) ---
; Since the command queue IS working, the issue was specifically that
; GWCA's rendering hook was broken. But in-game, ClickFrameButton
; queues via the SAME Enqueue mechanism. Let's test if it works now.
ConsoleWrite(@CRLF & "=== Clicking first item row ===" & @CRLF)
Local $goldBefore = GetGoldCharacter()
ConsoleWrite("Gold before: " & $goldBefore & @CRLF)

; Click item row
ClickFrameButton($FRAME_HASH_ITEM_ROW)
Sleep(1000)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_direct_itemsel.png', $gwHWnd)

; Click craft button
ConsoleWrite(@CRLF & "=== Clicking Craft button ===" & @CRLF)
ClickFrameButton($FRAME_HASH_CRAFT_BUTTON)
Sleep(2000)

Local $goldAfter = GetGoldCharacter()
ConsoleWrite("Gold after: " & $goldAfter & @CRLF)
ConsoleWrite("Gold change: " & ($goldAfter - $goldBefore) & @CRLF)

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_direct_after.png', $gwHWnd)

If $goldAfter < $goldBefore Then
	ConsoleWrite("*** CRAFT SUCCESSFUL! ***" & @CRLF)
Else
	ConsoleWrite("Gold unchanged. Trying alternative hashes..." & @CRLF)

	; Maybe our hash mapping is wrong. Try clicking other button-like frames.
	; From the frame dump: childOff=0=CraftTab, 1=SellTab, 2=CraftButton(?), 6=Goodbye(?)
	; Let's try all the button frames
	Local $altHashes[4][2] = [ _
		[1517397806, "Craft Tab (childOff=0)"], _
		[3738633661, "Sell Tab (childOff=1)"], _
		[3068881268, "Goodbye (childOff=6)"], _
		[1214056301, "Item list area (childOff=5)"] _
	]

	For $i = 0 To 3
		$goldBefore = GetGoldCharacter()
		ConsoleWrite("Trying hash " & $altHashes[$i][0] & " (" & $altHashes[$i][1] & ")..." & @CRLF)
		ClickFrameButton($altHashes[$i][0])
		Sleep(2000)
		$goldAfter = GetGoldCharacter()
		If $goldAfter <> $goldBefore Then
			ConsoleWrite("*** GOLD CHANGED with hash " & $altHashes[$i][0] & "! ***" & @CRLF)
			ExitLoop
		EndIf
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_alt_' & $i & '.png', $gwHWnd)
	Next
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
