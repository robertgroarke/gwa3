#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_click_play.au3
;
; Research: Try multiple approaches to click the Play button at char select
; using GWCA functions, executed on the game thread via rendering hook.
;
; Prerequisites: Run test_gwca_inject_research.au3 first to inject gwca.dll.
; This script reads the module base from tests\gwca_module_base.txt.
;
; Approaches tested:
;   1. GWCA ButtonClick(frame_ptr) — wrapper function
;   2. GWCA ButtonFrame::Click() — __thiscall direct method
;   3. GWCA ButtonFrame::MouseAction(actionState) — granular control
;   4. GWCA SendFrameUIMessage with properly formed params
;   5. Direct callback invocation
; =============================================================================

ConsoleWrite("=== GWCA Click Play Research ===" & @CRLF)
ConsoleWrite("Time: " & @YEAR & "-" & @MON & "-" & @MDAY & " " & @HOUR & ":" & @MIN & ":" & @SEC & @CRLF)

; --- Connect to the running GW client with GWCA already injected ---
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("ERROR: No GW clients found. Run test_gwca_inject_research.au3 first." & @CRLF)
	Exit 1
EndIf

; Find BEASTRIT or use first client
Local $targetIdx = 1
For $i = 1 To $game_clients[0][0]
	If StringInStr($game_clients[$i][1], "B E A S T R I T") Or StringInStr($game_clients[$i][1], "BEASTRIT") Then
		$targetIdx = $i
		ExitLoop
	EndIf
Next

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
ConsoleWrite("Connected PID=" & $game_clients[$targetIdx][0] & @CRLF)

; --- Read GWCA module base ---
Local $gwcaBase = 0
Local $baseFile = FileRead(@ScriptDir & '\tests\gwca_module_base.txt')
If $baseFile <> '' Then
	$gwcaBase = Int($baseFile)
	ConsoleWrite("GWCA base from file: 0x" & Hex($gwcaBase) & @CRLF)
Else
	; Try to find it from loaded modules
	Local $hSnap = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $game_clients[$targetIdx][0])
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
EndIf

If $gwcaBase = 0 Then
	ConsoleWrite("ERROR: gwca.dll not found. Run test_gwca_inject_research.au3 first." & @CRLF)
	Exit 1
EndIf
ConsoleWrite("GWCA module at 0x" & Hex($gwcaBase) & @CRLF)

; --- Verify char select ---
If Not IsAtCharSelect() Then
	ConsoleWrite("WARNING: Not at char select screen!" & @CRLF)
EndIf

; --- Find Play button frame ---
Local $playFrame = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $playFrame[0] = 0 Then
	ConsoleWrite("ERROR: Play button frame not found" & @CRLF)
	Exit 1
EndIf
Local $framePtr = Int($playFrame[0])
Local $frameId = $playFrame[1]
Local $childOffsetId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
ConsoleWrite("Play button: ptr=0x" & Hex($framePtr) & " id=" & $frameId & " childOff=" & $childOffsetId & @CRLF)

; --- GWCA function addresses ---
Local $GWCA_ButtonClick = $gwcaBase + 0x255E0       ; ButtonClick(Frame*) — wrapper
Local $GWCA_ButtonFrameClick = $gwcaBase + 0x16660  ; ButtonFrame::Click() — __thiscall
Local $GWCA_MouseAction = $gwcaBase + 0x173D0       ; ButtonFrame::MouseAction(ActionState) — __thiscall
Local $GWCA_GetFrameById = $gwcaBase + 0x25CC0      ; GetFrameById(uint32) — returns Frame*
Local $GWCA_SendFrameUIMsg = $gwcaBase + 0x274D0    ; SendFrameUIMessage(Frame*, msg, wp, lp)

ConsoleWrite(@CRLF & "GWCA functions:" & @CRLF)
ConsoleWrite("  ButtonClick:        0x" & Hex($GWCA_ButtonClick) & @CRLF)
ConsoleWrite("  ButtonFrame::Click: 0x" & Hex($GWCA_ButtonFrameClick) & @CRLF)
ConsoleWrite("  MouseAction:        0x" & Hex($GWCA_MouseAction) & @CRLF)
ConsoleWrite("  GetFrameById:       0x" & Hex($GWCA_GetFrameById) & @CRLF)
ConsoleWrite("  SendFrameUIMessage: 0x" & Hex($GWCA_SendFrameUIMsg) & @CRLF)

; --- Helper: allocate RWX memory for shellcode ---
Func _AllocShellcode($size)
	Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $size, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($mem) Or $mem[0] = 0 Then Return 0
	Return Int($mem[0])
EndFunc

; --- Helper: write shellcode bytes and queue for execution ---
Func _QueueShellcode($scAddr, ByRef $scStruct, $scLen)
	; Write shellcode
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($scAddr), _
		'ptr', DllStructGetPtr($scStruct), 'ulong_ptr', $scLen, 'ulong_ptr*', 0)

	; Sync queue counter
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')

	; Queue it
	Local $cmd = DllStructCreate('dword;dword')
	DllStructSetData($cmd, 1, $scAddr)
	DllStructSetData($cmd, 2, 0)
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
EndFunc

; --- Helper: check if map started loading ---
Func _CheckMapLoading()
	Sleep(3000)
	Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
	ConsoleWrite("  StatusCode after 3s: " & $status & @CRLF)
	If $status <> 0 Then
		ConsoleWrite("  *** MAP LOADING! Play button was clicked! ***" & @CRLF)
		Return True
	Else
		ConsoleWrite("  Still at char select" & @CRLF)
		Return False
	EndIf
EndFunc

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_before.png', $game_clients[$targetIdx][2])

; =============================================================================
; APPROACH 1: GWCA ButtonClick(Frame*)
;
; ButtonClick at gwca+0x255E0 takes a Frame* argument.
; From the research doc: "Wrapper: GetFrameById + Click"
; The function might actually take a frame_id (uint32) instead of a Frame*.
; We'll try BOTH: passing frame_ptr and frame_id.
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 1a: ButtonClick(frame_ptr) ===" & @CRLF)

Local $scAddr1a = _AllocShellcode(32)
If $scAddr1a <> 0 Then
	Local $sc1a = DllStructCreate('byte[32]')
	Local $p = 1

	; push frame_ptr (the actual Frame* pointer)
	DllStructSetData($sc1a, 1, 0x68, $p)  ; push imm32
	$p += 1
	_WriteLE32($sc1a, $p, $framePtr)
	$p += 4

	; call ButtonClick (E8 rel32)
	DllStructSetData($sc1a, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($sc1a, $p, $GWCA_ButtonClick - ($scAddr1a + $p - 1 + 4))
	$p += 4

	; add esp, 4 (cdecl cleanup — assume cdecl)
	DllStructSetData($sc1a, 1, 0x83, $p)
	$p += 1
	DllStructSetData($sc1a, 1, 0xC4, $p)
	$p += 1
	DllStructSetData($sc1a, 1, 0x04, $p)
	$p += 1

	; ret (for rendering hook's call)
	DllStructSetData($sc1a, 1, 0xC3, $p)

	ConsoleWrite("  Shellcode at 0x" & Hex($scAddr1a) & " (" & $p & " bytes)" & @CRLF)
	ConsoleWrite("  Pushing frame_ptr=0x" & Hex($framePtr) & @CRLF)
	_QueueShellcode($scAddr1a, $sc1a, $p)

	If _CheckMapLoading() Then
		ConsoleWrite("SUCCESS with approach 1a!" & @CRLF)
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_1a.png', $game_clients[$targetIdx][2])
		Exit 0
	EndIf
EndIf

ConsoleWrite(@CRLF & "=== APPROACH 1b: ButtonClick(frame_id) ===" & @CRLF)

Local $scAddr1b = _AllocShellcode(32)
If $scAddr1b <> 0 Then
	Local $sc1b = DllStructCreate('byte[32]')
	Local $p = 1

	; push frame_id (uint32)
	DllStructSetData($sc1b, 1, 0x68, $p)  ; push imm32
	$p += 1
	_WriteLE32($sc1b, $p, $frameId)
	$p += 4

	; call ButtonClick
	DllStructSetData($sc1b, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($sc1b, $p, $GWCA_ButtonClick - ($scAddr1b + $p - 1 + 4))
	$p += 4

	; add esp, 4
	DllStructSetData($sc1b, 1, 0x83, $p)
	$p += 1
	DllStructSetData($sc1b, 1, 0xC4, $p)
	$p += 1
	DllStructSetData($sc1b, 1, 0x04, $p)
	$p += 1

	; ret
	DllStructSetData($sc1b, 1, 0xC3, $p)

	ConsoleWrite("  Shellcode at 0x" & Hex($scAddr1b) & " (" & $p & " bytes)" & @CRLF)
	ConsoleWrite("  Pushing frame_id=" & $frameId & @CRLF)
	_QueueShellcode($scAddr1b, $sc1b, $p)

	If _CheckMapLoading() Then
		ConsoleWrite("SUCCESS with approach 1b!" & @CRLF)
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_1b.png', $game_clients[$targetIdx][2])
		Exit 0
	EndIf
EndIf

; =============================================================================
; APPROACH 2: ButtonFrame::Click() via __thiscall
;
; ButtonFrame::Click at gwca+0x16660 is __thiscall: ECX = Frame* (this)
; Internally it calls MouseAction(0x6) then MouseAction(0x7)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 2: ButtonFrame::Click (thiscall) ===" & @CRLF)

Local $scAddr2 = _AllocShellcode(32)
If $scAddr2 <> 0 Then
	Local $sc2 = DllStructCreate('byte[32]')
	Local $p = 1

	; mov ecx, frame_ptr (__thiscall: this = Frame*)
	DllStructSetData($sc2, 1, 0xB9, $p)  ; mov ecx, imm32
	$p += 1
	_WriteLE32($sc2, $p, $framePtr)
	$p += 4

	; call ButtonFrame::Click
	DllStructSetData($sc2, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($sc2, $p, $GWCA_ButtonFrameClick - ($scAddr2 + $p - 1 + 4))
	$p += 4

	; ret
	DllStructSetData($sc2, 1, 0xC3, $p)

	ConsoleWrite("  Shellcode at 0x" & Hex($scAddr2) & " (" & $p & " bytes)" & @CRLF)
	ConsoleWrite("  ECX = frame_ptr 0x" & Hex($framePtr) & @CRLF)
	_QueueShellcode($scAddr2, $sc2, $p)

	If _CheckMapLoading() Then
		ConsoleWrite("SUCCESS with approach 2!" & @CRLF)
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_2.png', $game_clients[$targetIdx][2])
		Exit 0
	EndIf
EndIf

; =============================================================================
; APPROACH 3: ButtonFrame::MouseAction(ActionState) — individual down/up
;
; MouseAction at gwca+0x173D0 is __thiscall: ECX = Frame*
; Takes one arg: ActionState (0x6=MouseDown, 0x7=MouseUp)
; A full click = MouseAction(6) + MouseAction(7)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 3: MouseAction(6) then MouseAction(7) ===" & @CRLF)

For $actionState = 6 To 7
	Local $scAddr3 = _AllocShellcode(32)
	If $scAddr3 <> 0 Then
		Local $sc3 = DllStructCreate('byte[32]')
		Local $p = 1

		; mov ecx, frame_ptr
		DllStructSetData($sc3, 1, 0xB9, $p)
		$p += 1
		_WriteLE32($sc3, $p, $framePtr)
		$p += 4

		; push actionState
		DllStructSetData($sc3, 1, 0x6A, $p)  ; push imm8
		$p += 1
		DllStructSetData($sc3, 1, $actionState, $p)
		$p += 1

		; call MouseAction
		DllStructSetData($sc3, 1, 0xE8, $p)
		$p += 1
		_WriteLE32($sc3, $p, $GWCA_MouseAction - ($scAddr3 + $p - 1 + 4))
		$p += 4

		; ret
		DllStructSetData($sc3, 1, 0xC3, $p)

		ConsoleWrite("  MouseAction(" & $actionState & ") shellcode at 0x" & Hex($scAddr3) & @CRLF)
		_QueueShellcode($scAddr3, $sc3, $p)
		Sleep(100)  ; brief pause between down and up
	EndIf
Next

If _CheckMapLoading() Then
	ConsoleWrite("SUCCESS with approach 3!" & @CRLF)
	_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_3.png', $game_clients[$targetIdx][2])
	Exit 0
EndIf

; =============================================================================
; APPROACH 4: GWCA SendFrameUIMessage wrapper
;
; GWCA's SendFrameUIMessage at gwca+0x274D0 is a wrapper around the game func.
; It might handle hook dispatching and struct setup better than calling the
; game function directly.
; Signature: SendFrameUIMessage(Frame*, UIMessage, void* wParam, void* lParam)
;
; We'll try it with:
;   - msg=0x31 (kMouseClick2), wParam=NULL, lParam=0
;   - msg=0x22 (kMouseClick), wParam=NULL, lParam=0
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 4a: GWCA SendFrameUIMsg(frame, 0x31, NULL, 0) ===" & @CRLF)

Local $scAddr4a = _AllocShellcode(32)
If $scAddr4a <> 0 Then
	Local $sc4a = DllStructCreate('byte[32]')
	Local $p = 1

	; push 0 (lParam)
	DllStructSetData($sc4a, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($sc4a, 1, 0x00, $p)
	$p += 1

	; push 0 (wParam = NULL)
	DllStructSetData($sc4a, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($sc4a, 1, 0x00, $p)
	$p += 1

	; push 0x31 (UIMessage kMouseClick2)
	DllStructSetData($sc4a, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($sc4a, 1, 0x31, $p)
	$p += 1

	; push frame_ptr
	DllStructSetData($sc4a, 1, 0x68, $p)
	$p += 1
	_WriteLE32($sc4a, $p, $framePtr)
	$p += 4

	; call SendFrameUIMessage
	DllStructSetData($sc4a, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($sc4a, $p, $GWCA_SendFrameUIMsg - ($scAddr4a + $p - 1 + 4))
	$p += 4

	; add esp, 16 (4 args * 4 bytes, cdecl)
	DllStructSetData($sc4a, 1, 0x83, $p)
	$p += 1
	DllStructSetData($sc4a, 1, 0xC4, $p)
	$p += 1
	DllStructSetData($sc4a, 1, 0x10, $p)
	$p += 1

	; ret
	DllStructSetData($sc4a, 1, 0xC3, $p)

	ConsoleWrite("  Shellcode at 0x" & Hex($scAddr4a) & @CRLF)
	_QueueShellcode($scAddr4a, $sc4a, $p)

	If _CheckMapLoading() Then
		ConsoleWrite("SUCCESS with approach 4a!" & @CRLF)
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_4a.png', $game_clients[$targetIdx][2])
		Exit 0
	EndIf
EndIf

ConsoleWrite(@CRLF & "=== APPROACH 4b: GWCA SendFrameUIMsg(frame, 0x22, NULL, 0) ===" & @CRLF)

Local $scAddr4b = _AllocShellcode(32)
If $scAddr4b <> 0 Then
	Local $sc4b = DllStructCreate('byte[32]')
	Local $p = 1

	; push 0
	DllStructSetData($sc4b, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($sc4b, 1, 0x00, $p)
	$p += 1
	; push 0
	DllStructSetData($sc4b, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($sc4b, 1, 0x00, $p)
	$p += 1
	; push 0x22 (kMouseClick)
	DllStructSetData($sc4b, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($sc4b, 1, 0x22, $p)
	$p += 1
	; push frame_ptr
	DllStructSetData($sc4b, 1, 0x68, $p)
	$p += 1
	_WriteLE32($sc4b, $p, $framePtr)
	$p += 4
	; call SendFrameUIMessage
	DllStructSetData($sc4b, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($sc4b, $p, $GWCA_SendFrameUIMsg - ($scAddr4b + $p - 1 + 4))
	$p += 4
	; add esp, 16
	DllStructSetData($sc4b, 1, 0x83, $p)
	$p += 1
	DllStructSetData($sc4b, 1, 0xC4, $p)
	$p += 1
	DllStructSetData($sc4b, 1, 0x10, $p)
	$p += 1
	; ret
	DllStructSetData($sc4b, 1, 0xC3, $p)

	ConsoleWrite("  Shellcode at 0x" & Hex($scAddr4b) & @CRLF)
	_QueueShellcode($scAddr4b, $sc4b, $p)

	If _CheckMapLoading() Then
		ConsoleWrite("SUCCESS with approach 4b!" & @CRLF)
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_4b.png', $game_clients[$targetIdx][2])
		Exit 0
	EndIf
EndIf

; =============================================================================
; APPROACH 5: Direct callback invocation
;
; Each frame has a callbacks array at +0xA8. The first callback entry might
; be the handler function. Call it directly with the right args.
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 5: Direct callback call ===" & @CRLF)

Local $cbBuffer = MemoryRead($processHandle, $framePtr + 0xA8, 'dword')
Local $cbSize = MemoryRead($processHandle, $framePtr + 0xAC, 'dword')
ConsoleWrite("  Callbacks: buffer=0x" & Hex($cbBuffer) & " size=" & $cbSize & @CRLF)

If $cbSize > 0 And $cbBuffer > 0x10000 Then
	; Read the first callback entry
	Local $cbFunc = MemoryRead($processHandle, $cbBuffer, 'dword')
	ConsoleWrite("  callback[0] = 0x" & Hex($cbFunc) & @CRLF)

	If $cbFunc > 0x10000 Then
		; The callback might be called as: callback(frame_ptr, msgid, wParam, lParam)
		; Or it might be __thiscall with frame callbacks as this
		; Try cdecl: push 0, push 0, push 0x31, push frame_ptr, call callback

		Local $scAddr5 = _AllocShellcode(32)
		If $scAddr5 <> 0 Then
			Local $sc5 = DllStructCreate('byte[32]')
			Local $p = 1

			; push 0 (lParam)
			DllStructSetData($sc5, 1, 0x6A, $p)
			$p += 1
			DllStructSetData($sc5, 1, 0x00, $p)
			$p += 1
			; push 0 (wParam)
			DllStructSetData($sc5, 1, 0x6A, $p)
			$p += 1
			DllStructSetData($sc5, 1, 0x00, $p)
			$p += 1
			; push 0x31 (kMouseClick2)
			DllStructSetData($sc5, 1, 0x6A, $p)
			$p += 1
			DllStructSetData($sc5, 1, 0x31, $p)
			$p += 1
			; push frame_ptr
			DllStructSetData($sc5, 1, 0x68, $p)
			$p += 1
			_WriteLE32($sc5, $p, $framePtr)
			$p += 4

			; Store callback address after shellcode and call via indirect
			Local $cbStore = $scAddr5 + 28  ; store at offset 28

			; call [cbStore]
			DllStructSetData($sc5, 1, 0xFF, $p)
			$p += 1
			DllStructSetData($sc5, 1, 0x15, $p)  ; call [imm32]
			$p += 1
			_WriteLE32($sc5, $p, $cbStore)
			$p += 4

			; add esp, 16
			DllStructSetData($sc5, 1, 0x83, $p)
			$p += 1
			DllStructSetData($sc5, 1, 0xC4, $p)
			$p += 1
			DllStructSetData($sc5, 1, 0x10, $p)
			$p += 1

			; ret
			DllStructSetData($sc5, 1, 0xC3, $p)

			; Write the shellcode
			DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
				'handle', $processHandle, 'ptr', Ptr($scAddr5), _
				'ptr', DllStructGetPtr($sc5), 'ulong_ptr', $p, 'ulong_ptr*', 0)

			; Write callback address at cbStore
			MemoryWrite($processHandle, $cbStore, $cbFunc, 'dword')

			; Queue
			$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
			Local $cmd5 = DllStructCreate('dword;dword')
			DllStructSetData($cmd5, 1, $scAddr5)
			DllStructSetData($cmd5, 2, 0)
			Enqueue(DllStructGetPtr($cmd5), DllStructGetSize($cmd5))

			If _CheckMapLoading() Then
				ConsoleWrite("SUCCESS with approach 5!" & @CRLF)
				_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_success_5.png', $game_clients[$targetIdx][2])
				Exit 0
			EndIf
		EndIf
	EndIf
EndIf

; --- Final screenshot ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_after.png', $game_clients[$targetIdx][2])

ConsoleWrite(@CRLF & "=== ALL APPROACHES FAILED ===" & @CRLF)
ConsoleWrite("The game client is still at char select." & @CRLF)
ConsoleWrite("Check screenshots in tests\ for visual state." & @CRLF)
ConsoleWrite("Next steps:" & @CRLF)
ConsoleWrite("  1. Check if game crashed (PID still alive?)" & @CRLF)
ConsoleWrite("  2. Try GW::Initialize on game thread (not remote thread)" & @CRLF)
ConsoleWrite("  3. Disassemble GWCA ButtonClick to verify calling convention" & @CRLF)
ConsoleWrite("  4. Try py4gw approach (Python + GWCA)" & @CRLF)

; Check if game is still alive
Local $exitCheck = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', _
	'handle', $processHandle, 'dword*', 0)
If IsArray($exitCheck) Then
	If $exitCheck[2] = 259 Then  ; STILL_ACTIVE
		ConsoleWrite("Game process still running (good — no crash)" & @CRLF)
	Else
		ConsoleWrite("WARNING: Game process exited with code " & $exitCheck[2] & " (CRASHED?)" & @CRLF)
	EndIf
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
