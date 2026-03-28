#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_craft_click.au3
;
; Test clicking the Craft button at Eyja's consumables dialog using GWCA.
; Uses inject → click → zero → FreeLibrary pattern for stability.
;
; Prerequisites: Character must be in-game (handles travel + dialog opening).
; =============================================================================

Global Const $FRAME_HASH_CRAFT_BUTTON = 835947118
Global Const $FRAME_HASH_GOODBYE_BUTTON = 3068881268
Global Const $FRAME_HASH_CRAFT_TAB = 1517397806
Global Const $FRAME_HASH_SELL_TAB = 3738633661
Global Const $FRAME_HASH_MERCHANT_WINDOW = 3613855137
Global Const $FRAME_HASH_ITEM_ROW = 1852904459
Global Const $MAP_EMBARK_BEACH = 857

ConsoleWrite("=== Craft Button Click Test ===" & @CRLF)

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
Local $gwPID = $game_clients[$game_clients[0][0]][0]
Local $gwHWnd = $game_clients[$game_clients[0][0]][2]
ConsoleWrite("Connected PID=" & $gwPID & @CRLF)
WinActivate($gwHWnd)
Sleep(1000)

; --- Handle char select if needed ---
If IsAtCharSelect() Then
	ConsoleWrite("At char select, clicking Play..." & @CRLF)
	_GWCA_InjectClickFree($FRAME_HASH_PLAY_BUTTON)
	Local $t = TimerInit()
	While GetMyID() = 0 Or GetMaxAgents() = 0
		Sleep(500)
		If TimerDiff($t) > 60000 Then
			ConsoleWrite("ERROR: Map load timeout" & @CRLF)
			Exit 1
		EndIf
	WEnd
	Sleep(3000)
	ConsoleWrite("In game! MapID=" & GetMapID() & @CRLF)
EndIf

; --- Travel to Embark Beach ---
If GetMapID() <> $MAP_EMBARK_BEACH Then
	ConsoleWrite("Traveling to Embark Beach..." & @CRLF)
	TravelToOutpost($MAP_EMBARK_BEACH)
	WaitMapLoading($MAP_EMBARK_BEACH, 30000)
	Sleep(3000)
EndIf
ConsoleWrite("At Embark Beach. MapID=" & GetMapID() & @CRLF)

; --- Check if merchant dialog already open ---
Local $merchantOpen = IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW)
ConsoleWrite("Merchant window already open: " & $merchantOpen & @CRLF)

If Not $merchantOpen Then
	; Walk to Eyja and open dialog
	ConsoleWrite("Walking to Eyja..." & @CRLF)
	Local $eyjaX = 3336, $eyjaY = 627
	MoveTo($eyjaX, $eyjaY)
	Sleep(1000)
	Local $eyjaAgent = GetNearestNPCToCoords($eyjaX, $eyjaY)
	If $eyjaAgent = 0 Then
		ConsoleWrite("ERROR: Eyja not found" & @CRLF)
		Exit 1
	EndIf
	GoToNPC($eyjaAgent)
	Sleep(1000)
	Dialog($eyjaAgent)
	Sleep(2000)

	$merchantOpen = IsFrameVisible($FRAME_HASH_MERCHANT_WINDOW)
	ConsoleWrite("Merchant window open after dialog: " & $merchantOpen & @CRLF)
	If Not $merchantOpen Then
		ConsoleWrite("ERROR: Merchant window didn't open" & @CRLF)
		Exit 1
	EndIf
EndIf

; --- Verify Craft button is visible ---
Local $craftVisible = IsFrameVisible($FRAME_HASH_CRAFT_BUTTON)
Local $goodbyeVisible = IsFrameVisible($FRAME_HASH_GOODBYE_BUTTON)
ConsoleWrite("Craft button visible: " & $craftVisible & @CRLF)
ConsoleWrite("Goodbye button visible: " & $goodbyeVisible & @CRLF)

If Not $craftVisible Then
	ConsoleWrite("ERROR: Craft button not visible" & @CRLF)
	Exit 1
EndIf

; --- Record pre-craft state ---
Local $goldBefore = GetGoldCharacter()
ConsoleWrite("Gold before craft: " & $goldBefore & @CRLF)

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_click_before.png', $gwHWnd)

; --- First: click the first item row to select it ---
ConsoleWrite(@CRLF & "=== STEP 1: Select first item (Grail of Might) ===" & @CRLF)
; Item rows have hash=1852904459. Find all instances and click the first one.
; From the frame dump, item rows are children of parent=960 with childOff=112,113,...
; The first visible item row entry with hash 1852904459 should be Grail of Might.
Local $itemResult = GetFrameByHash($FRAME_HASH_ITEM_ROW)
If $itemResult[0] <> 0 Then
	ConsoleWrite("Item row found: id=" & $itemResult[1] & @CRLF)
	; Click it to select
	Local $selectResult = _GWCA_InjectClickFree($FRAME_HASH_ITEM_ROW)
	ConsoleWrite("Item select result: " & $selectResult & @CRLF)
	Sleep(1000)
Else
	ConsoleWrite("WARNING: Item row not found" & @CRLF)
EndIf

; Screenshot after item select
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_item_selected.png', $gwHWnd)

; --- Now click the Craft button ---
ConsoleWrite(@CRLF & "=== STEP 2: Click Craft button ===" & @CRLF)
; Try both possible hashes — the "Craft action button" might be hash=553 or different
; Let's try our discovered hash first, then try others if gold doesn't change
Local $clickResult = _GWCA_InjectClickFree($FRAME_HASH_CRAFT_BUTTON)
ConsoleWrite("Craft click result: " & $clickResult & @CRLF)

; Wait and check result
Sleep(2000)
Local $goldAfter = GetGoldCharacter()
ConsoleWrite("Gold after craft: " & $goldAfter & @CRLF)
ConsoleWrite("Gold change: " & ($goldAfter - $goldBefore) & @CRLF)

; Screenshot after
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\craft_click_after.png', $gwHWnd)

If $goldAfter < $goldBefore Then
	ConsoleWrite("*** CRAFT SUCCESSFUL! Gold decreased by " & ($goldBefore - $goldAfter) & " ***" & @CRLF)
Else
	ConsoleWrite("Gold did not decrease — craft may not have worked" & @CRLF)
	ConsoleWrite("(Item may not be selected, or insufficient materials)" & @CRLF)
EndIf

; Check if game is still alive
Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
ConsoleWrite("Game alive: " & (IsArray($ec) And $ec[2] = 259) & @CRLF)

ConsoleWrite("=== DONE ===" & @CRLF)

; =============================================================================
; GWCA inject → click → zero → FreeLibrary pattern
; Minimizes GWCA's presence in the process to avoid stability issues.
; =============================================================================
Func _GWCA_InjectClickFree($frameHash)
	Local $ph = GetProcessHandle()
	Local $pid = $game_clients[$game_clients[0][0]][0]
	Local $gwBase = $pe_sections_ranges[0][0] - 0x1000

	; Find frame first (no GWCA needed)
	Local $fr = GetFrameByHash($frameHash)
	If $fr[0] = 0 Then
		ConsoleWrite('[CraftClick] Frame hash ' & $frameHash & ' not found' & @CRLF)
		Return False
	EndIf
	Local $fp = Int($fr[0])
	Local $state = MemoryRead($ph, $fp + $FRAME_OFFSET_STATE, 'dword')
	If BitAND($state, $FRAME_STATE_CREATED) = 0 Then
		ConsoleWrite('[CraftClick] Frame not created' & @CRLF)
		Return False
	EndIf
	ConsoleWrite('[CraftClick] Frame ptr=0x' & Hex($fp) & ' id=' & $fr[1] & @CRLF)

	; --- INJECT gwca.dll ---
	Local $gwcaBase = _FindMod($pid, "gwca.dll")
	Local $freshInject = False
	If $gwcaBase = 0 Then
		$freshInject = True
		Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
		Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
		Local $pathLen = BinaryLen($dllPathW)
		Local $rp = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
			'handle', $ph, 'ptr', 0, 'ulong_ptr', $pathLen, 'dword', 0x1000, 'dword', 0x04)
		If Not IsArray($rp) Or $rp[0] = 0 Then Return False
		Local $pb = DllStructCreate('byte[' & $pathLen & ']')
		DllStructSetData($pb, 1, $dllPathW)
		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $ph, 'ptr', $rp[0], 'ptr', DllStructGetPtr($pb), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)
		Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
		Local $ll = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')
		Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
			'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, 'ptr', $ll[0], 'ptr', $rp[0], 'dword', 0, 'dword*', 0)
		If Not IsArray($th) Or $th[0] = 0 Then Return False
		DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 10000)
		Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $th[0], 'dword*', 0)
		$gwcaBase = $ec[2]
		DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
		DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
			'handle', $ph, 'ptr', $rp[0], 'ulong_ptr', 0, 'dword', 0x8000)
		If $gwcaBase = 0 Then
			ConsoleWrite('[CraftClick] Injection failed' & @CRLF)
			Return False
		EndIf
		ConsoleWrite('[CraftClick] gwca.dll injected at 0x' & Hex($gwcaBase) & @CRLF)
	Else
		ConsoleWrite('[CraftClick] gwca.dll already at 0x' & Hex($gwcaBase) & @CRLF)
	EndIf

	; --- POPULATE data section ---
	Local $sendFrameAddr = $gwBase + 0x2286D0
	MemoryWrite($ph, $gwcaBase + 0x8A39C, $sendFrameAddr, 'dword')
	MemoryWrite($ph, $gwcaBase + 0x8A3A0, $sendFrameAddr, 'dword')  ; CRITICAL
	MemoryWrite($ph, $gwcaBase + 0x8A37C, $gwBase + 0x20E2B0, 'dword')
	MemoryWrite($ph, $gwcaBase + 0x8A410, $gwBase + 0x22DC20, 'dword')
	MemoryWrite($ph, $gwcaBase + 0x8A3B0, Int(GetLabel('FrameArray')), 'dword')

	; --- BUILD shellcode: ButtonClick + zero +0x8A3A0 ---
	Local $scAddr = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, 'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($scAddr) Or $scAddr[0] = 0 Then Return False
	Local $sc = Int($scAddr[0])

	Local $buf = DllStructCreate('byte[48]')
	Local $p = 1
	; push frame_ptr
	DllStructSetData($buf, 1, 0x68, $p)
	$p += 1
	_WriteLE32($buf, $p, $fp)
	$p += 4
	; call ButtonClick
	DllStructSetData($buf, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($buf, $p, ($gwcaBase + 0x255E0) - ($sc + $p - 1 + 4))
	$p += 4
	; add esp, 4
	DllStructSetData($buf, 1, 0x83, $p)
	$p += 1
	DllStructSetData($buf, 1, 0xC4, $p)
	$p += 1
	DllStructSetData($buf, 1, 0x04, $p)
	$p += 1
	; NEUTRALIZE: mov dword [gwca+0x8A3A0], 0
	DllStructSetData($buf, 1, 0xC7, $p)
	$p += 1
	DllStructSetData($buf, 1, 0x05, $p)
	$p += 1
	_WriteLE32($buf, $p, $gwcaBase + 0x8A3A0)
	$p += 4
	_WriteLE32($buf, $p, 0)
	$p += 4
	; ret
	DllStructSetData($buf, 1, 0xC3, $p)

	; Write shellcode
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $ph, 'ptr', Ptr($sc), 'ptr', DllStructGetPtr($buf), 'ulong_ptr', $p, 'ulong_ptr*', 0)

	; --- QUEUE via rendering hook ---
	$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
	Local $cmd = DllStructCreate('dword;dword')
	DllStructSetData($cmd, 1, $sc)
	DllStructSetData($cmd, 2, 0)
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
	ConsoleWrite('[CraftClick] Click queued, waiting...' & @CRLF)

	; Wait for execution — use a marker write to CONFIRM shellcode ran
	Local $execMarker = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $ph, 'ptr', 0, 'ulong_ptr', 4, 'dword', 0x1000, 'dword', 0x40)
	Local $markerAddr = 0
	If IsArray($execMarker) And $execMarker[0] <> 0 Then
		$markerAddr = Int($execMarker[0])
		MemoryWrite($ph, $markerAddr, 0xAAAA, 'dword')

		; Build a second shellcode that just writes 0xBBBB to marker then ret
		Local $confirmSc = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
			'handle', $ph, 'ptr', 0, 'ulong_ptr', 16, 'dword', 0x1000, 'dword', 0x40)
		If IsArray($confirmSc) And $confirmSc[0] <> 0 Then
			Local $csc = Int($confirmSc[0])
			Local $cb = DllStructCreate('byte[16]')
			Local $cp = 1
			DllStructSetData($cb, 1, 0xC7, $cp)
			$cp += 1
			DllStructSetData($cb, 1, 0x05, $cp)
			$cp += 1
			_WriteLE32($cb, $cp, $markerAddr)
			$cp += 4
			_WriteLE32($cb, $cp, 0xBBBB)
			$cp += 4
			DllStructSetData($cb, 1, 0xC3, $cp)
			DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
				'handle', $ph, 'ptr', Ptr($csc), 'ptr', DllStructGetPtr($cb), 'ulong_ptr', $cp, 'ulong_ptr*', 0)

			; Queue the confirm shellcode AFTER the click shellcode
			Sleep(200)
			$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
			Local $cmd2 = DllStructCreate('dword;dword')
			DllStructSetData($cmd2, 1, $csc)
			DllStructSetData($cmd2, 2, 0)
			Enqueue(DllStructGetPtr($cmd2), DllStructGetSize($cmd2))
		EndIf
	EndIf

	; Wait for BOTH shellcodes to execute
	Local $waitStart = TimerInit()
	Local $executed = False
	While TimerDiff($waitStart) < 8000
		Sleep(200)
		If $markerAddr <> 0 Then
			Local $mv = MemoryRead($ph, $markerAddr, 'dword')
			If $mv = 0xBBBB Then
				$executed = True
				ExitLoop
			EndIf
		EndIf
	WEnd

	If $executed Then
		ConsoleWrite('[CraftClick] Click + confirm executed successfully' & @CRLF)
	Else
		ConsoleWrite('[CraftClick] WARNING: Shellcode did NOT execute (hook inactive?)' & @CRLF)
		ConsoleWrite('[CraftClick] Marker=0x' & Hex(MemoryRead($ph, $markerAddr, 'dword')) & @CRLF)
		ConsoleWrite('[CraftClick] QueueCounter=' & MemoryRead($ph, GetLabel('QueueCounter'), 'dword') & @CRLF)
	EndIf

	; Cleanup marker memory
	If $markerAddr <> 0 Then
		DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
			'handle', $ph, 'ptr', Ptr($markerAddr), 'ulong_ptr', 0, 'dword', 0x8000)
	EndIf

	; --- DON'T FreeLibrary immediately ---
	; FreeLibrary while game code is still running causes crashes.
	; Instead, just leave gwca.dll loaded but neutered (hook ptr zeroed).
	; The DLL is harmless with zeroed ptrs — it won't intercept anything.
	ConsoleWrite('[CraftClick] gwca.dll left loaded but neutered (+0x8A3A0 zeroed)' & @CRLF)

	; Also zero the original ptr to prevent any other code paths
	MemoryWrite($ph, $gwcaBase + 0x8A39C, 0, 'dword')

	; Free shellcode memory
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $ph, 'ptr', Ptr($sc), 'ulong_ptr', 0, 'dword', 0x8000)

	Return True
EndFunc

Func _FindMod($pid, $name)
	Local $sn = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
	If Not IsArray($sn) Or $sn[0] = -1 Then Return 0
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;' & _
		'dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($me, 'dwSize', DllStructGetSize($me))
	Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $sn[0], 'struct*', $me)
	While IsArray($r) And $r[0]
		If StringLower(DllStructGetData($me, 'szModule')) = StringLower($name) Then
			Local $base = Int(DllStructGetData($me, 'modBaseAddr'))
			DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
			Return $base
		EndIf
		$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $sn[0], 'struct*', $me)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
	Return 0
EndFunc
