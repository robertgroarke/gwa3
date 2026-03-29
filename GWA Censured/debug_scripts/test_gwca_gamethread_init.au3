#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_gamethread_init.au3
;
; Research: What if GWCA's GW::Initialize() MUST run on the game thread?
;
; The previous approach ran GW::Initialize via CreateRemoteThread (random OS
; thread). GWCA hooks game functions by patching code — if those patches expect
; to be called from the game thread context, initialization on a remote thread
; could leave hooks in a broken state.
;
; This test:
;   1. Injects gwca.dll via CreateRemoteThread (LoadLibraryW — safe)
;   2. Calls Scanner::Initialize via CreateRemoteThread (safe — just reads memory)
;   3. Calls GW::Initialize via RENDERING HOOK (game thread!)
;   4. Waits, then tries ButtonFrame::Click via rendering hook
; =============================================================================

ConsoleWrite("=== GWCA Game Thread Init Test ===" & @CRLF)
ConsoleWrite("Time: " & @YEAR & "-" & @MON & "-" & @MDAY & " " & @HOUR & ":" & @MIN & ":" & @SEC & @CRLF)

; --- Connect ---
ScanAndUpdateGameClients()
Local $targetIdx = 1
For $i = 1 To $game_clients[0][0]
	If StringInStr($game_clients[$i][1], "B E A S T R I T") Or StringInStr($game_clients[$i][1], "BEASTRIT") Then
		$targetIdx = $i
		ExitLoop
	EndIf
Next

If $game_clients[0][0] = 0 Then
	ConsoleWrite("ERROR: No GW clients" & @CRLF)
	Exit 1
EndIf

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[$targetIdx][0]
ConsoleWrite("Connected PID=" & $gwPID & @CRLF)

; --- Check if gwca already loaded ---
Local $gwcaBase = 0
Local $hSnap = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $gwPID)
If IsArray($hSnap) And $hSnap[0] <> -1 Then
	Local $modEntry = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($modEntry, 'dwSize', DllStructGetSize($modEntry))
	Local $ret = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $hSnap[0], 'struct*', $modEntry)
	While IsArray($ret) And $ret[0]
		If StringLower(DllStructGetData($modEntry, 'szModule')) = "gwca.dll" Then
			$gwcaBase = Int(DllStructGetData($modEntry, 'modBaseAddr'))
			ExitLoop
		EndIf
		$ret = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $hSnap[0], 'struct*', $modEntry)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $hSnap[0])
EndIf

; --- Inject if needed ---
If $gwcaBase = 0 Then
	ConsoleWrite("Injecting gwca.dll..." & @CRLF)
	Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
	Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
	Local $pathLen = BinaryLen($dllPathW)

	Local $remotePath = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
		'dword', 0x1000, 'dword', 0x04)
	Local $pathBuf = DllStructCreate('byte[' & $pathLen & ']')
	DllStructSetData($pathBuf, 1, $dllPathW)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', $remotePath[0], _
		'ptr', DllStructGetPtr($pathBuf), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)

	Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
	Local $loadLib = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')
	Local $thread = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', $loadLib[0], 'ptr', $remotePath[0], 'dword', 0, 'dword*', 0)
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread[0], 'dword', 10000)
	Local $exitCode = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $thread[0], 'dword*', 0)
	$gwcaBase = $exitCode[2]
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread[0])
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $remotePath[0], 'ulong_ptr', 0, 'dword', 0x8000)

	If $gwcaBase = 0 Then
		ConsoleWrite("ERROR: injection failed" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & @CRLF)

	; Scanner::Initialize via remote thread (safe — just reads memory)
	Local $gwImageBase = $pe_sections_ranges[0][0] - 0x1000
	Local $scannerInit = $gwcaBase + 0x21850
	Local $t2 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', Ptr($scannerInit), 'ptr', Ptr($gwImageBase), 'dword', 0, 'dword*', 0)
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t2[0], 'dword', 10000)
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t2[0])
	ConsoleWrite("Scanner initialized" & @CRLF)
Else
	ConsoleWrite("gwca.dll already loaded at 0x" & Hex($gwcaBase) & @CRLF)
EndIf

; Save base for other tests
FileDelete(@ScriptDir & '\tests\gwca_module_base.txt')
FileWrite(@ScriptDir & '\tests\gwca_module_base.txt', "0x" & Hex($gwcaBase))

; --- Step: Call GW::Initialize via rendering hook (GAME THREAD) ---
ConsoleWrite(@CRLF & "=== Calling GW::Initialize on GAME THREAD ===" & @CRLF)

Local $gwInit = $gwcaBase + 0x18E90
ConsoleWrite("GW::Initialize at 0x" & Hex($gwInit) & @CRLF)

; Check if already initialized by reading a data pointer
Local $origSendFrame = MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')
ConsoleWrite("Pre-init SendFrameUIMsg ptr: 0x" & Hex($origSendFrame) & @CRLF)

If $origSendFrame <> 0 Then
	ConsoleWrite("GWCA appears already initialized (func ptrs populated). Skipping init." & @CRLF)
Else
	; Build shellcode: call GW::Initialize() then ret
	; GW::Initialize takes no args, returns void
	Local $initSc = _AllocRWX(16)
	Local $sc = DllStructCreate('byte[16]')
	Local $p = 1

	; call GW::Initialize (E8 rel32)
	DllStructSetData($sc, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($sc, $p, $gwInit - ($initSc + $p - 1 + 4))
	$p += 4

	; ret
	DllStructSetData($sc, 1, 0xC3, $p)

	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($initSc), _
		'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

	; Queue via rendering hook
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	Local $cmd = DllStructCreate('dword;dword')
	DllStructSetData($cmd, 1, $initSc)
	DllStructSetData($cmd, 2, 0)
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

	ConsoleWrite("GW::Initialize queued. Waiting 5s..." & @CRLF)
	Sleep(5000)

	; Verify initialization
	Local $postInitSendFrame = MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')
	ConsoleWrite("Post-init SendFrameUIMsg ptr: 0x" & Hex($postInitSendFrame) & @CRLF)

	If $postInitSendFrame <> 0 And $postInitSendFrame <> $origSendFrame Then
		ConsoleWrite("GW::Initialize completed on game thread!" & @CRLF)
	Else
		ConsoleWrite("WARNING: Init may not have completed (pointer unchanged)" & @CRLF)
	EndIf
EndIf

; --- Read all GWCA ptrs to verify ---
ConsoleWrite(@CRLF & "=== GWCA State After Game-Thread Init ===" & @CRLF)
Local $ptrs[5][2] = [ _
	[0x8A39C, "Orig SendFrameUIMsg"], _
	[0x8A3A0, "Hooked SendFrameUIMsg"], _
	[0x8A37C, "GetChildFrame"], _
	[0x8A410, "RootFrame"], _
	[0x8A3B0, "Frame hash table"] _
]
For $i = 0 To 4
	ConsoleWrite("  " & $ptrs[$i][1] & ": 0x" & Hex(MemoryRead($processHandle, $gwcaBase + $ptrs[$i][0], 'dword')) & @CRLF)
Next

; --- Now try ButtonFrame::Click on game thread ---
ConsoleWrite(@CRLF & "=== Attempting ButtonFrame::Click After Game-Thread Init ===" & @CRLF)

Local $playFrame = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $playFrame[0] = 0 Then
	ConsoleWrite("Play button not found" & @CRLF)
	Exit 1
EndIf
Local $framePtr = Int($playFrame[0])
ConsoleWrite("Play button at 0x" & Hex($framePtr) & " id=" & $playFrame[1] & @CRLF)

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_gt_before.png', $game_clients[$targetIdx][2])

; Try ButtonFrame::Click (__thiscall: ecx=Frame*)
Local $clickSc = _AllocRWX(16)
Local $sc2 = DllStructCreate('byte[16]')
Local $p = 1

; mov ecx, frame_ptr
DllStructSetData($sc2, 1, 0xB9, $p)
$p += 1
_WriteLE32($sc2, $p, $framePtr)
$p += 4

; call ButtonFrame::Click
Local $clickFunc = $gwcaBase + 0x16660
DllStructSetData($sc2, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc2, $p, $clickFunc - ($clickSc + $p - 1 + 4))
$p += 4

; ret
DllStructSetData($sc2, 1, 0xC3, $p)

DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($clickSc), _
	'ptr', DllStructGetPtr($sc2), 'ulong_ptr', $p, 'ulong_ptr*', 0)

$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $cmd2 = DllStructCreate('dword;dword')
DllStructSetData($cmd2, 1, $clickSc)
DllStructSetData($cmd2, 2, 0)
Enqueue(DllStructGetPtr($cmd2), DllStructGetSize($cmd2))

ConsoleWrite("ButtonFrame::Click queued. Waiting 3s..." & @CRLF)
Sleep(3000)

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_gt_after.png', $game_clients[$targetIdx][2])

Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

If $status <> 0 Then
	ConsoleWrite("*** SUCCESS: Map loading — Play button clicked! ***" & @CRLF)
Else
	ConsoleWrite("Still at char select." & @CRLF)
	ConsoleWrite("Game still alive: ")
	Local $exitCheck = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', _
		'handle', $processHandle, 'dword*', 0)
	If IsArray($exitCheck) And $exitCheck[2] = 259 Then
		ConsoleWrite("Yes" & @CRLF)
	Else
		ConsoleWrite("CRASHED (exit=" & $exitCheck[2] & ")" & @CRLF)
	EndIf
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)

; --- Helper ---
Func _AllocRWX($size)
	Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $size, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($mem) Or $mem[0] = 0 Then Return 0
	Return Int($mem[0])
EndFunc
