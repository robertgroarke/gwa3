#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_click_stable.au3
;
; Stable version: click Play button, then immediately neutralize GWCA to
; prevent crash from uninitialized hook dispatch tables.
;
; Strategy: shellcode does ButtonClick THEN zeros +0x8A3A0 to prevent
; any subsequent GWCA calls from entering the crash-prone hook dispatch.
; After the shellcode completes, we FreeLibrary gwca.dll from the host.
; =============================================================================

ConsoleWrite("=== Stable GWCA Click Test ===" & @CRLF)

; Launch or find client
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("Launching BEASTRIT..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	Local $result = GWLauncher_LaunchAccount($accounts, $accIdx)
	ConsoleWrite("PID=" & $result[0] & ", waiting 40s..." & @CRLF)
	Sleep(40000)
	ScanAndUpdateGameClients()
EndIf

SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[$game_clients[0][0]][0]
Local $gwHWnd = $game_clients[$game_clients[0][0]][2]
ConsoleWrite("PID=" & $gwPID & @CRLF)

WinActivate($gwHWnd)
Sleep(2000)

; Verify rendering hook
Local $marker = _Alloc(4)
MemoryWrite($processHandle, $marker, 0xAAAA, 'dword')
Local $nsc = _Alloc(16)
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
_WQ($nsc, $nb, $p)
Sleep(1500)
If MemoryRead($processHandle, $marker, 'dword') <> 0xBBBB Then
	ConsoleWrite("ERROR: Rendering hook not active" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Rendering hook: OK" & @CRLF)
ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)

; Get Play button
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $fp = Int($pf[0])
ConsoleWrite("Play button: 0x" & Hex($fp) & " id=" & $pf[1] & @CRLF)

; --- Inject GWCA ---
Local $gwcaBase = _FindModule($gwPID, "gwca.dll")
If $gwcaBase = 0 Then
	ConsoleWrite("Injecting gwca.dll..." & @CRLF)
	$gwcaBase = _InjectDLL("c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll")
	If $gwcaBase = 0 Then
		ConsoleWrite("ERROR: injection failed" & @CRLF)
		Exit 1
	EndIf
EndIf
ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & @CRLF)

; --- Populate ONLY the essential data pointers ---
Local $gwBase = $pe_sections_ranges[0][0] - 0x1000
Local $sendFrameAddr = $gwBase + 0x2286D0

; The critical pointer: +0x8A3A0 (GWCA wrapper checks this)
MemoryWrite($processHandle, $gwcaBase + 0x8A3A0, $sendFrameAddr, 'dword')
; Also +0x8A39C (original, used by some paths)
MemoryWrite($processHandle, $gwcaBase + 0x8A39C, $sendFrameAddr, 'dword')
; GetChildFrame, RootFrame, FrameHashTable
MemoryWrite($processHandle, $gwcaBase + 0x8A37C, $gwBase + 0x20E2B0, 'dword')
MemoryWrite($processHandle, $gwcaBase + 0x8A410, $gwBase + 0x22DC20, 'dword')
MemoryWrite($processHandle, $gwcaBase + 0x8A3B0, Int(GetLabel('FrameArray')), 'dword')
ConsoleWrite("GWCA data populated" & @CRLF)

; Verify hook still works after injection
MemoryWrite($processHandle, $marker, 0xCCCC, 'dword')
Local $vsc = _Alloc(16)
Local $vb = DllStructCreate('byte[16]')
$p = 1
DllStructSetData($vb, 1, 0xC7, $p)
$p += 1
DllStructSetData($vb, 1, 0x05, $p)
$p += 1
_WriteLE32($vb, $p, $marker)
$p += 4
_WriteLE32($vb, $p, 0xDDDD)
$p += 4
DllStructSetData($vb, 1, 0xC3, $p)
_WQ($vsc, $vb, $p)
Sleep(1500)
If MemoryRead($processHandle, $marker, 'dword') <> 0xDDDD Then
	ConsoleWrite("ERROR: Hook broken after GWCA" & @CRLF)
	Exit 1
EndIf

; --- Build shellcode: ButtonClick + neutralize GWCA ---
; Shellcode:
;   push frame_ptr
;   call ButtonClick
;   add esp, 4
;   ; Neutralize: zero +0x8A3A0 to prevent crash on subsequent GWCA calls
;   mov dword [gwcaBase+0x8A3A0], 0
;   ret
ConsoleWrite(@CRLF & "=== Clicking Play Button ===" & @CRLF)
Local $sc = _Alloc(64)
Local $buf = DllStructCreate('byte[48]')
$p = 1

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

; add esp, 4 (cdecl cleanup)
DllStructSetData($buf, 1, 0x83, $p)
$p += 1
DllStructSetData($buf, 1, 0xC4, $p)
$p += 1
DllStructSetData($buf, 1, 0x04, $p)
$p += 1

; Store return value to marker for diagnosis
; movzx eax, al
DllStructSetData($buf, 1, 0x0F, $p)
$p += 1
DllStructSetData($buf, 1, 0xB6, $p)
$p += 1
DllStructSetData($buf, 1, 0xC0, $p)
$p += 1
; mov [marker], eax
DllStructSetData($buf, 1, 0xA3, $p)
$p += 1
_WriteLE32($buf, $p, $marker)
$p += 4

; *** NEUTRALIZE GWCA: zero +0x8A3A0 ***
; mov dword [gwcaBase+0x8A3A0], 0
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

; Write marker to 0 first
MemoryWrite($processHandle, $marker, 0, 'dword')

; Write + queue shellcode
_WQ($sc, $buf, $p)
ConsoleWrite("Click shellcode queued. Waiting..." & @CRLF)

; Monitor for map loading
For $wait = 1 To 20
	Sleep(1000)
	Local $retVal = MemoryRead($processHandle, $marker, 'dword')
	Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
	Local $hookVal = MemoryRead($processHandle, $gwcaBase + 0x8A3A0, 'dword')

	; Check if game crashed
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
	Local $alive = (IsArray($ec) And $ec[2] = 259)

	ConsoleWrite("  t=" & $wait & "s: retval=" & $retVal & " status=" & $status & _
		" +0x8A3A0=0x" & Hex($hookVal) & " alive=" & $alive & @CRLF)

	If Not $alive Then
		ConsoleWrite("*** GAME CRASHED at t=" & $wait & "s ***" & @CRLF)
		ExitLoop
	EndIf

	If $status <> 0 Then
		ConsoleWrite("*** MAP LOADING! StatusCode=" & $status & " ***" & @CRLF)

		; Now try to FreeLibrary gwca.dll to fully clean up
		ConsoleWrite("Attempting FreeLibrary gwca.dll..." & @CRLF)
		Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
		Local $freeLib = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'FreeLibrary')
		Local $freeThread = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
			'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
			'ptr', $freeLib[0], 'ptr', Ptr($gwcaBase), 'dword', 0, 'dword*', 0)
		If IsArray($freeThread) And $freeThread[0] <> 0 Then
			DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $freeThread[0], 'dword', 5000)
			DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $freeThread[0])
			ConsoleWrite("FreeLibrary called" & @CRLF)
		EndIf

		; Continue monitoring
		For $wait2 = 1 To 30
			Sleep(1000)
			$ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
			$alive = (IsArray($ec) And $ec[2] = 259)
			$status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
			ConsoleWrite("  loading t=" & $wait2 & "s: status=" & $status & " alive=" & $alive & @CRLF)
			If Not $alive Then
				ConsoleWrite("*** CRASHED DURING LOADING at t=" & $wait2 & "s ***" & @CRLF)
				ExitLoop
			EndIf
			If $status = 0 And $wait2 > 5 Then
				ConsoleWrite("*** STATUS RESET — may have loaded or returned to char select ***" & @CRLF)
				ExitLoop
			EndIf
		Next
		ExitLoop
	EndIf
Next

; Final state
$ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
ConsoleWrite("Final: alive=" & (IsArray($ec) And $ec[2] = 259) & @CRLF)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_stable_after.png', $gwHWnd)
ConsoleWrite("=== DONE ===" & @CRLF)

; =============================================================================
Func _Alloc($sz)
	Local $m = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $sz, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($m) Or $m[0] = 0 Then Return 0
	Return Int($m[0])
EndFunc

Func _WQ($addr, ByRef $b, $len)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($b), 'ulong_ptr', $len, 'ulong_ptr*', 0)
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	Local $c = DllStructCreate('dword;dword')
	DllStructSetData($c, 1, $addr)
	DllStructSetData($c, 2, 0)
	Enqueue(DllStructGetPtr($c), DllStructGetSize($c))
EndFunc

Func _FindModule($pid, $name)
	Local $sn = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
	If Not IsArray($sn) Or $sn[0] = -1 Then Return 0
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
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

Func _InjectDLL($dllPath)
	Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
	Local $pathLen = BinaryLen($dllPathW)
	Local $rp = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
		'dword', 0x1000, 'dword', 0x04)
	If Not IsArray($rp) Or $rp[0] = 0 Then Return 0
	Local $pb = DllStructCreate('byte[' & $pathLen & ']')
	DllStructSetData($pb, 1, $dllPathW)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', $rp[0], _
		'ptr', DllStructGetPtr($pb), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)
	Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
	Local $ll = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')
	Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', $ll[0], 'ptr', $rp[0], 'dword', 0, 'dword*', 0)
	If Not IsArray($th) Or $th[0] = 0 Then Return 0
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 10000)
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $th[0], 'dword*', 0)
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $rp[0], 'ulong_ptr', 0, 'dword', 0x8000)
	Return $ec[2]
EndFunc
