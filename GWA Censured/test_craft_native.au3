#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_craft_native.au3
;
; Click UI frames using the NATIVE approach — NO gwca.dll.
; Uses the discovery that:
;   context = [frame+0x128] - 0x128  (parent frame)
;   ECX = context + 0xA8
;   Call game SendFrameUIMsg(ECX, 0x31, &kMouseAction, 0)
;
; This works via BotsHub's command queue which is active in-game.
; =============================================================================

Global Const $FRAME_HASH_CRAFT_BUTTON = 835947118
Global Const $FRAME_HASH_GOODBYE_BUTTON = 3068881268
Global Const $FRAME_HASH_MERCHANT_WINDOW = 3613855137
Global Const $FRAME_HASH_ITEM_ROW = 1852904459
Global Const $MAP_EMBARK_BEACH = 857

ConsoleWrite("=== Native Frame Click Test ===" & @CRLF)

; --- Connect to existing client or launch fresh ---
ScanAndUpdateGameClients()
Local $targetIdx = -1

; Try to find BEASTRIT by name or use newest client without gwca
For $i = 1 To $game_clients[0][0]
	ConsoleWrite("  Client " & $i & ": PID=" & $game_clients[$i][0] & " title='" & $game_clients[$i][1] & "'" & @CRLF)
	If StringInStr($game_clients[$i][1], "B E A S T R I T") Or StringInStr($game_clients[$i][1], "BEASTRIT") Then
		$targetIdx = $i
	EndIf
Next

If $targetIdx = -1 And $game_clients[0][0] > 0 Then
	$targetIdx = $game_clients[0][0]
	ConsoleWrite("Using last client as fallback" & @CRLF)
EndIf

If $targetIdx = -1 Then
	ConsoleWrite("No GW client. Launching BEASTRIT..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	Local $launchResult = GWLauncher_LaunchAccount($accounts, $accIdx)
	ConsoleWrite("PID=" & $launchResult[0] & ", waiting 40s..." & @CRLF)
	Sleep(40000)
	ScanAndUpdateGameClients()
	For $i = 1 To $game_clients[0][0]
		If $game_clients[$i][0] = $launchResult[0] Then
			$targetIdx = $i
			ExitLoop
		EndIf
	Next
	If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]
EndIf

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwHWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected PID=" & $game_clients[$targetIdx][0] & @CRLF)
WinActivate($gwHWnd)
Sleep(1000)

; --- Handle char select first (needs GWCA for Play click) ---
If IsAtCharSelect() Then
	ConsoleWrite("At char select. Clicking Play via GWCA..." & @CRLF)
	ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
	Local $lt = TimerInit()
	While GetMyID() = 0 Or GetMaxAgents() = 0
		Sleep(500)
		If TimerDiff($lt) > 60000 Then
			ConsoleWrite("ERROR: Map load timeout" & @CRLF)
			Exit 1
		EndIf
	WEnd
	Sleep(5000)  ; extra settle time for hooks to stabilize
	ConsoleWrite("In game! MapID=" & GetMapID() & @CRLF)
EndIf

; --- NOW verify command queue (in-game, MainProc should be active) ---
ConsoleWrite("Testing command queue..." & @CRLF)
Local $marker = _AllocRWX(4)
MemoryWrite($processHandle, $marker, 0xAAAA, 'dword')
Local $nopSc = _AllocRWX(16)
Local $nb = DllStructCreate('byte[16]')
Local $p = 1
DllStructSetData($nb, 1, 0xC7, $p)
$p += 1
DllStructSetData($nb, 1, 0x05, $p)
$p += 1
_WriteLE32($nb, $p, $marker)
$p += 4
_WriteLE32($nb, $p, 0xBBBB)
$p += 4
DllStructSetData($nb, 1, 0xC3, $p)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($nopSc), _
	'ptr', DllStructGetPtr($nb), 'ulong_ptr', $p, 'ulong_ptr*', 0)
$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $nopSc)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
Sleep(2000)
If MemoryRead($processHandle, $marker, 'dword') <> 0xBBBB Then
	ConsoleWrite("WARNING: Command queue not active — GWCA may have broken hooks" & @CRLF)
	ConsoleWrite("Trying to re-initialize hooks..." & @CRLF)
	; Re-run InitializeGameClientForGWA2 to re-inject hooks
	InitializeGameClientForGWA2(False)
	$processHandle = GetProcessHandle()
	Sleep(2000)
	MemoryWrite($processHandle, $marker, 0xAAAA, 'dword')
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	DllStructSetData($cmd, 1, $nopSc)
	DllStructSetData($cmd, 2, 0)
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
	Sleep(2000)
	If MemoryRead($processHandle, $marker, 'dword') <> 0xBBBB Then
		ConsoleWrite("ERROR: Command queue STILL broken after reinit" & @CRLF)
		Exit 1
	EndIf
EndIf
ConsoleWrite("Command queue: OK" & @CRLF)
If GetMapID() <> $MAP_EMBARK_BEACH Then
	ConsoleWrite("Traveling to Embark Beach..." & @CRLF)
	TravelToOutpost($MAP_EMBARK_BEACH)
	WaitMapLoading($MAP_EMBARK_BEACH, 30000)
	Sleep(3000)
EndIf
ConsoleWrite("At Embark Beach." & @CRLF)

; --- Open Eyja dialog ---
If Not IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW) Then
	ConsoleWrite("Walking to Eyja..." & @CRLF)
	MoveTo(3336, 627)
	Sleep(2000)
	; Try wider radius to find Eyja
	Local $eyja = GetNearestNPCToCoords(3336, 627)
	If $eyja = 0 Then
		ConsoleWrite("Not found at default coords. Trying GoToConsumableTrader..." & @CRLF)
		GoToConsumableTrader("Eyja")
		Sleep(2000)
		; Re-check merchant window
		If IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW) Then
			ConsoleWrite("Merchant opened via GoToConsumableTrader!" & @CRLF)
			$eyja = 1  ; dummy value, dialog is open
		Else
			ConsoleWrite("ERROR: Still can't find Eyja" & @CRLF)
			Exit 1
		EndIf
	EndIf
	GoToNPC($eyja)
	Sleep(1000)
	Dialog($eyja)
	Sleep(2000)
EndIf
ConsoleWrite("Merchant window: " & IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW) & @CRLF)
ConsoleWrite("Craft button: " & IsFrameVisible($FRAME_HASH_CRAFT_BUTTON) & @CRLF)

; --- Native frame click function ---
; Click item row first, then Craft button
Local $goldBefore = GetGoldCharacter()
ConsoleWrite("Gold before: " & $goldBefore & @CRLF)

; Select first item
ConsoleWrite(@CRLF & "=== Selecting item ===" & @CRLF)
_NativeFrameClick($FRAME_HASH_ITEM_ROW)
Sleep(1000)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_native_selected.png', $gwHWnd)

; Click Craft
ConsoleWrite(@CRLF & "=== Clicking Craft ===" & @CRLF)
_NativeFrameClick($FRAME_HASH_CRAFT_BUTTON)
Sleep(2000)

Local $goldAfter = GetGoldCharacter()
ConsoleWrite("Gold after: " & $goldAfter & " (change: " & ($goldAfter - $goldBefore) & ")" & @CRLF)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_native_after.png', $gwHWnd)

If $goldAfter < $goldBefore Then
	ConsoleWrite("*** CRAFT SUCCESSFUL! ***" & @CRLF)
Else
	ConsoleWrite("Gold unchanged. Trying Goodbye to verify click works..." & @CRLF)
	_NativeFrameClick($FRAME_HASH_GOODBYE_BUTTON)
	Sleep(1000)
	Local $merchantStillOpen = IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW)
	ConsoleWrite("Merchant still open after Goodbye: " & $merchantStillOpen & @CRLF)
	If Not $merchantStillOpen Then
		ConsoleWrite("*** GOODBYE WORKED — clicks ARE working, Craft hash may be wrong ***" & @CRLF)
	EndIf
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)

; =============================================================================
; Native frame click: no GWCA, uses game's SendFrameUIMsg directly
; context = [frame+0x128] - 0x128 (parent frame)
; ECX = context+0xA8, call SendFrameUIMsg(0x31, &action, 0)
; =============================================================================
Func _NativeFrameClick($hash)
	Local $ph = GetProcessHandle()
	Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))

	; Find frame
	Local $fr = GetFrameByHash($hash)
	If $fr[0] = 0 Then
		ConsoleWrite('[Native] Hash ' & $hash & ' not found' & @CRLF)
		Return False
	EndIf
	Local $fp = Int($fr[0])
	Local $fid = $fr[1]
	Local $coff = MemoryRead($ph, $fp + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')

	; Get PARENT frame (context)
	Local $parentRel = MemoryRead($ph, $fp + 0x128, 'dword')
	If $parentRel < 0x10000 Then
		ConsoleWrite('[Native] No parent relation' & @CRLF)
		Return False
	EndIf
	Local $context = $parentRel - 0x128  ; parent Frame*
	ConsoleWrite('[Native] Frame=0x' & Hex($fp) & ' id=' & $fid & ' context=0x' & Hex($context) & @CRLF)

	; Allocate memory for shellcode + action data
	Local $mem = _AllocRWX(128)
	If $mem = 0 Then Return False
	Local $scAddr = $mem
	Local $actionAddr = $mem + 64   ; kMouseAction at +64
	Local $funcPtrAddr = $mem + 96  ; func ptr storage at +96

	; Write SendFrameUIMsg address
	MemoryWrite($ph, $funcPtrAddr, $sendFunc, 'dword')

	; Do MouseDown (6) then MouseUp (7)
	For $actionState = 6 To 7
		; Write kMouseAction struct: {frame_id, child_offset_id, action_state, 0, 0}
		Local $ad = DllStructCreate('dword[5]')
		DllStructSetData($ad, 1, $fid, 1)
		DllStructSetData($ad, 1, $coff, 2)
		DllStructSetData($ad, 1, $actionState, 3)
		DllStructSetData($ad, 1, 0, 4)
		DllStructSetData($ad, 1, 0, 5)
		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $ph, 'ptr', Ptr($actionAddr), _
			'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

		; Build shellcode:
		;   mov ecx, context+0xA8     ; B9 <le32>
		;   push 0                     ; lParam
		;   push actionAddr            ; wParam = &kMouseAction
		;   push 0x31                  ; msgid = kMouseClick2
		;   call [funcPtrAddr]         ; call SendFrameUIMsg (__thiscall)
		;   ret
		Local $buf = DllStructCreate('byte[32]')
		$p = 1
		; mov ecx, context+0xA8
		DllStructSetData($buf, 1, 0xB9, $p)
		$p += 1
		_WriteLE32($buf, $p, $context + 0xA8)
		$p += 4
		; push 0
		DllStructSetData($buf, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($buf, 1, 0x00, $p)
		$p += 1
		; push actionAddr
		DllStructSetData($buf, 1, 0x68, $p)
		$p += 1
		_WriteLE32($buf, $p, $actionAddr)
		$p += 4
		; push 0x31
		DllStructSetData($buf, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($buf, 1, 0x31, $p)
		$p += 1
		; call [funcPtrAddr]
		DllStructSetData($buf, 1, 0xFF, $p)
		$p += 1
		DllStructSetData($buf, 1, 0x15, $p)
		$p += 1
		_WriteLE32($buf, $p, $funcPtrAddr)
		$p += 4
		; ret
		DllStructSetData($buf, 1, 0xC3, $p)

		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $ph, 'ptr', Ptr($scAddr), _
			'ptr', DllStructGetPtr($buf), 'ulong_ptr', $p, 'ulong_ptr*', 0)

		; Queue
		$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
		Local $c = DllStructCreate('dword;dword')
		DllStructSetData($c, 1, $scAddr)
		DllStructSetData($c, 2, 0)
		Enqueue(DllStructGetPtr($c), DllStructGetSize($c))
		Sleep(100)
	Next

	ConsoleWrite('[Native] MouseDown+Up queued for hash=' & $hash & @CRLF)
	Return True
EndFunc

Func _AllocRWX($sz)
	Local $m = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $sz, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($m) Or $m[0] = 0 Then Return 0
	Return Int($m[0])
EndFunc
