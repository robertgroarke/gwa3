#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_inject_research.au3
;
; Research: Inject gwca.dll into BEASTRIT's GW client, verify initialization,
; dump all relevant GWCA function pointers and frame state at char select.
;
; Goal: Build a solid foundation for understanding what GWCA exposes and
; whether its ButtonClick/ButtonFrame::Click functions can be used.
; =============================================================================

ConsoleWrite("=== GWCA Injection Research (BEASTRIT) ===" & @CRLF)
ConsoleWrite("Time: " & @YEAR & "-" & @MON & "-" & @MDAY & " " & @HOUR & ":" & @MIN & ":" & @SEC & @CRLF)

; --- Step 0: Find or launch BEASTRIT ---
ScanAndUpdateGameClients()
Local $targetIdx = -1

; Check if BEASTRIT is already running
For $i = 1 To $game_clients[0][0]
	Local $title = $game_clients[$i][1]
	ConsoleWrite("  Client " & $i & ": PID=" & $game_clients[$i][0] & " title='" & $title & "'" & @CRLF)
	If StringInStr($title, "B E A S T R I T") Or StringInStr($title, "BEASTRIT") Then
		$targetIdx = $i
	EndIf
Next

If $targetIdx = -1 Then
	; Launch BEASTRIT
	ConsoleWrite("BEASTRIT not running, launching..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	If $accIdx = -1 Then
		ConsoleWrite("ERROR: BEASTRIT account not found in Accounts.json" & @CRLF)
		Exit 1
	EndIf

	Local $result = GWLauncher_LaunchAccount($accounts, $accIdx)
	If $result = 0 Then
		ConsoleWrite("ERROR: Failed to launch" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("Launched PID=" & $result[0] & ", waiting 35s for char select..." & @CRLF)
	Sleep(35000)

	ScanAndUpdateGameClients()
	For $i = 1 To $game_clients[0][0]
		If $game_clients[$i][0] = $result[0] Then
			$targetIdx = $i
			ExitLoop
		EndIf
	Next
EndIf

If $targetIdx = -1 Then
	; Fallback: use first client
	If $game_clients[0][0] > 0 Then
		$targetIdx = 1
		ConsoleWrite("WARNING: BEASTRIT not found by name, using client 1" & @CRLF)
	Else
		ConsoleWrite("ERROR: No GW clients found" & @CRLF)
		Exit 1
	EndIf
EndIf

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[$targetIdx][0]
ConsoleWrite(@CRLF & "Connected to client " & $targetIdx & " PID=" & $gwPID & @CRLF)

; --- Step 1: Check char select state ---
ConsoleWrite(@CRLF & "=== Frame State at Char Select ===" & @CRLF)
ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("IsReconnectDialogShowing: " & IsReconnectDialogShowing() & @CRLF)

; Dump all known char select frame hashes
Local $frameHashes[8][2] = [ _
	[$FRAME_HASH_PLAY_BUTTON, "Play Button"], _
	[$FRAME_HASH_PLAY_GREYED, "Play Greyed"], _
	[$FRAME_HASH_RECONNECT_YES, "Reconnect YES"], _
	[$FRAME_HASH_RECONNECT_NO, "Reconnect NO"], _
	[$FRAME_HASH_CREATE_BUTTON, "Create Button"], _
	[$FRAME_HASH_DELETE_BUTTON, "Delete Button"], _
	[$FRAME_HASH_LOGOUT_BUTTON, "Logout Button"], _
	[$FRAME_HASH_CHARACTER_FRAME, "Character Frame"] _
]

For $i = 0 To 7
	Local $fr = GetFrameByHash($frameHashes[$i][0])
	If $fr[0] <> 0 Then
		Local $state = MemoryRead($processHandle, $fr[0] + $FRAME_OFFSET_STATE, 'dword')
		Local $childOff = MemoryRead($processHandle, $fr[0] + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
		Local $created = (BitAND($state, $FRAME_STATE_CREATED) <> 0)
		Local $hidden = (BitAND($state, $FRAME_STATE_HIDDEN) <> 0)
		Local $disabled = (BitAND($state, $FRAME_STATE_DISABLED) <> 0)
		ConsoleWrite("  " & $frameHashes[$i][1] & ": ptr=0x" & Hex($fr[0]) & " id=" & $fr[1] & _
			" childOff=" & $childOff & " state=0x" & Hex($state) & _
			" (created=" & $created & " hidden=" & $hidden & " disabled=" & $disabled & ")" & @CRLF)

		; Dump callback array
		Local $cbBuffer = MemoryRead($processHandle, $fr[0] + 0xA8, 'dword')
		Local $cbSize = MemoryRead($processHandle, $fr[0] + 0xAC, 'dword')
		Local $cbCap = MemoryRead($processHandle, $fr[0] + 0xB0, 'dword')
		ConsoleWrite("    callbacks: buffer=0x" & Hex($cbBuffer) & " size=" & $cbSize & " cap=" & $cbCap & @CRLF)
		If $cbSize > 0 And $cbSize < 20 And $cbBuffer > 0x10000 Then
			For $cb = 0 To $cbSize - 1
				Local $cbEntry = MemoryRead($processHandle, $cbBuffer + ($cb * 4), 'dword')
				ConsoleWrite("    callback[" & $cb & "] = 0x" & Hex($cbEntry) & @CRLF)
			Next
		EndIf

		; Dump field at 0x1C4 (used in GWCA MouseAction struct)
		Local $f1C4 = MemoryRead($processHandle, $fr[0] + 0x1C4, 'dword')
		ConsoleWrite("    field_0x1C4 = 0x" & Hex($f1C4) & @CRLF)
	Else
		ConsoleWrite("  " & $frameHashes[$i][1] & ": NOT FOUND" & @CRLF)
	EndIf
Next

; --- Step 2: Inject gwca.dll ---
ConsoleWrite(@CRLF & "=== Injecting gwca.dll ===" & @CRLF)

; Check if gwca.dll is already loaded by scanning process modules
Local $gwcaModule = 0
Local $hSnap = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $gwPID)
If IsArray($hSnap) And $hSnap[0] <> -1 Then
	Local $modEntry = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($modEntry, 'dwSize', DllStructGetSize($modEntry))
	Local $ret = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $hSnap[0], 'struct*', $modEntry)
	While IsArray($ret) And $ret[0]
		Local $modName = DllStructGetData($modEntry, 'szModule')
		If StringLower($modName) = "gwca.dll" Then
			$gwcaModule = Int(DllStructGetData($modEntry, 'modBaseAddr'))
			ConsoleWrite("gwca.dll already loaded at 0x" & Hex($gwcaModule) & @CRLF)
			ExitLoop
		EndIf
		$ret = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $hSnap[0], 'struct*', $modEntry)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $hSnap[0])
EndIf

If $gwcaModule = 0 Then
	; Inject fresh
	Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"

	; Write DLL path as wide string
	Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
	Local $pathLen = BinaryLen($dllPathW)
	Local $remotePath = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
		'dword', 0x1000, 'dword', 0x04)
	If Not IsArray($remotePath) Or $remotePath[0] = 0 Then
		ConsoleWrite("VirtualAllocEx for path failed" & @CRLF)
		Exit 1
	EndIf

	Local $pathBuf = DllStructCreate('byte[' & $pathLen & ']')
	DllStructSetData($pathBuf, 1, $dllPathW)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', $remotePath[0], _
		'ptr', DllStructGetPtr($pathBuf), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)

	; LoadLibraryW
	Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
	Local $loadLib = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')

	Local $thread = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', $loadLib[0], 'ptr', $remotePath[0], 'dword', 0, 'dword*', 0)
	If Not IsArray($thread) Or $thread[0] = 0 Then
		ConsoleWrite("CreateRemoteThread failed" & @CRLF)
		Exit 1
	EndIf

	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread[0], 'dword', 10000)
	Local $exitCode = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $thread[0], 'dword*', 0)
	$gwcaModule = $exitCode[2]
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread[0])

	If $gwcaModule = 0 Then
		ConsoleWrite("ERROR: gwca.dll failed to load" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("gwca.dll loaded at 0x" & Hex($gwcaModule) & @CRLF)

	; --- Step 3: Initialize Scanner ---
	Local $gwImageBase = $pe_sections_ranges[0][0] - 0x1000
	Local $scannerInit = $gwcaModule + 0x21850
	ConsoleWrite("GW image base: 0x" & Hex($gwImageBase) & @CRLF)
	ConsoleWrite("Scanner::Initialize at 0x" & Hex($scannerInit) & @CRLF)

	Local $t2 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', Ptr($scannerInit), 'ptr', Ptr($gwImageBase), 'dword', 0, 'dword*', 0)
	If IsArray($t2) And $t2[0] <> 0 Then
		DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t2[0], 'dword', 10000)
		DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t2[0])
		ConsoleWrite("Scanner initialized" & @CRLF)
	Else
		ConsoleWrite("Scanner init FAILED" & @CRLF)
	EndIf

	; --- Step 4: GW::Initialize ---
	Local $gwInit = $gwcaModule + 0x18E90
	ConsoleWrite("GW::Initialize at 0x" & Hex($gwInit) & @CRLF)

	Local $t3 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', Ptr($gwInit), 'ptr', 0, 'dword', 0, 'dword*', 0)
	If IsArray($t3) And $t3[0] <> 0 Then
		DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t3[0], 'dword', 15000)
		DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t3[0])
		ConsoleWrite("GW::Initialize completed" & @CRLF)
	Else
		ConsoleWrite("GW::Initialize FAILED" & @CRLF)
	EndIf

	; Free the path memory
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $remotePath[0], 'ulong_ptr', 0, 'dword', 0x8000)
EndIf

; --- Step 5: Read GWCA data section to verify initialization ---
ConsoleWrite(@CRLF & "=== GWCA Data Section Dump ===" & @CRLF)

; Known offsets from gwca.dll data section (relative to module base)
Local $gwcaPtrs[10][2] = [ _
	[0x8A39C, "Original SendFrameUIMsg"], _
	[0x8A3A0, "Hooked SendFrameUIMsg"], _
	[0x8A37C, "GetChildFrame"], _
	[0x8A410, "RootFrame func"], _
	[0x8A3D0, "SetWindowVisible"], _
	[0x8A394, "SetWindowPosition"], _
	[0x8A3B0, "Frame hash table"], _
	[0x880F0, "Hash seed (ESI)"], _
	[0x880F4, "Hash seed (EAX)"], _
	[0x880F8, "Hash seed 2"] _
]

For $i = 0 To 9
	Local $val = MemoryRead($processHandle, $gwcaModule + $gwcaPtrs[$i][0], 'dword')
	ConsoleWrite("  " & $gwcaPtrs[$i][1] & " [+" & Hex($gwcaPtrs[$i][0]) & "]: 0x" & Hex($val) & @CRLF)
Next

; --- Step 6: GWCA export function addresses ---
ConsoleWrite(@CRLF & "=== GWCA Export Addresses ===" & @CRLF)
Local $gwcaExports[6][2] = [ _
	[0x16660, "ButtonFrame::Click"], _
	[0x173D0, "ButtonFrame::MouseAction"], _
	[0x255E0, "ButtonClick"], _
	[0x259A0, "GetChildFrame (GWCA)"], _
	[0x25CC0, "GetFrameById"], _
	[0x25D30, "GetFrameByLabel"] _
]

For $i = 0 To 5
	Local $addr = $gwcaModule + $gwcaExports[$i][0]
	; Read first 8 bytes of the function to verify it exists
	Local $funcBytes = DllStructCreate('byte[8]')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($funcBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)
	Local $hex = ""
	For $b = 1 To 8
		$hex &= Hex(DllStructGetData($funcBytes, 1, $b), 2) & " "
	Next
	ConsoleWrite("  " & $gwcaExports[$i][1] & " @ 0x" & Hex($addr) & ": " & $hex & @CRLF)
Next

; --- Step 7: Dump Play button frame details for click research ---
ConsoleWrite(@CRLF & "=== Play Button Deep Dump ===" & @CRLF)
Local $playFrame = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $playFrame[0] <> 0 Then
	Local $fp = $playFrame[0]
	ConsoleWrite("Frame ptr: 0x" & Hex($fp) & @CRLF)
	ConsoleWrite("Frame ID: " & $playFrame[1] & @CRLF)

	; Dump entire frame struct in 16-byte chunks (0x1C8 bytes total)
	ConsoleWrite(@CRLF & "Frame struct hex dump (0x1C8 bytes):" & @CRLF)
	For $off = 0 To 0x1C0 Step 0x10
		Local $chunk = DllStructCreate('byte[16]')
		DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($fp + $off), _
			'ptr', DllStructGetPtr($chunk), 'ulong_ptr', 16, 'ulong_ptr*', 0)
		Local $hex = Hex($off, 3) & ": "
		For $b = 1 To 16
			$hex &= Hex(DllStructGetData($chunk, 1, $b), 2) & " "
		Next
		ConsoleWrite("  " & $hex & @CRLF)
	Next

	; Read the callbacks array entries in detail
	Local $cbBuf = MemoryRead($processHandle, $fp + 0xA8, 'dword')
	Local $cbSize = MemoryRead($processHandle, $fp + 0xAC, 'dword')
	ConsoleWrite(@CRLF & "Callback array buffer=0x" & Hex($cbBuf) & " size=" & $cbSize & @CRLF)

	; The callbacks array might be an array of structs, not just function pointers
	; Let's dump more bytes per entry to understand the structure
	If $cbSize > 0 And $cbSize < 50 And $cbBuf > 0x10000 Then
		; Read raw callback buffer (up to 256 bytes)
		Local $rawSize = $cbSize * 16  ; assume up to 16 bytes per entry
		If $rawSize > 256 Then $rawSize = 256
		Local $rawBuf = DllStructCreate('byte[' & $rawSize & ']')
		DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($cbBuf), _
			'ptr', DllStructGetPtr($rawBuf), 'ulong_ptr', $rawSize, 'ulong_ptr*', 0)
		ConsoleWrite("Raw callback buffer (" & $rawSize & " bytes):" & @CRLF)
		For $off = 0 To $rawSize - 1 Step 16
			Local $hex = Hex($off, 3) & ": "
			For $b = 1 To 16
				If $off + $b <= $rawSize Then
					$hex &= Hex(DllStructGetData($rawBuf, 1, $off + $b), 2) & " "
				EndIf
			Next
			ConsoleWrite("  " & $hex & @CRLF)
		Next
	EndIf
Else
	ConsoleWrite("Play button NOT FOUND at char select" & @CRLF)
EndIf

; --- Step 8: Check rendering hook state ---
ConsoleWrite(@CRLF & "=== Rendering Hook State ===" & @CRLF)
Local $queueCounter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $queueBase = Int(GetLabel('QueueBase'))
ConsoleWrite("QueueCounter: " & $queueCounter & @CRLF)
ConsoleWrite("QueueBase: 0x" & Hex($queueBase) & @CRLF)
ConsoleWrite("StatusCode: " & MemoryRead($processHandle, GetLabel('StatusCode'), 'dword') & @CRLF)

; Verify rendering hook is running by watching queue counter changes
; (if rendering is active, the counter should be stable when nothing is queued)
ConsoleWrite("Verifying rendering hook is active..." & @CRLF)
Local $testShellcode = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
	'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 16, _
	'dword', 0x1000, 'dword', 0x40)
If IsArray($testShellcode) And $testShellcode[0] <> 0 Then
	; Write a simple "ret" shellcode — just returns immediately (NOP test)
	Local $nop = DllStructCreate('byte[1]')
	DllStructSetData($nop, 1, 0xC3, 1)  ; RET
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', $testShellcode[0], _
		'ptr', DllStructGetPtr($nop), 'ulong_ptr', 1, 'ulong_ptr*', 0)

	; Queue the NOP
	Local $cmd = DllStructCreate('dword;dword')
	DllStructSetData($cmd, 1, Int($testShellcode[0]))
	DllStructSetData($cmd, 2, 0)
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
	Sleep(500)

	; Check if queue counter advanced
	Local $newCounter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	If $newCounter <> $queueCounter Then
		ConsoleWrite("Rendering hook ACTIVE (counter " & $queueCounter & " -> " & $newCounter & ")" & @CRLF)
	Else
		ConsoleWrite("WARNING: Rendering hook may NOT be active (counter unchanged)" & @CRLF)
	EndIf

	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $testShellcode[0], 'ulong_ptr', 0, 'dword', 0x8000)
EndIf

; Screenshot
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_research_state.png', $game_clients[$targetIdx][2])

; --- Save gwca module base for the click test ---
; Write to a temp file so test_gwca_click_play.au3 can read it
FileDelete(@ScriptDir & '\tests\gwca_module_base.txt')
FileWrite(@ScriptDir & '\tests\gwca_module_base.txt', "0x" & Hex($gwcaModule))
ConsoleWrite(@CRLF & "GWCA module base saved to tests\gwca_module_base.txt: 0x" & Hex($gwcaModule) & @CRLF)

ConsoleWrite(@CRLF & "=== INJECTION RESEARCH COMPLETE ===" & @CRLF)
