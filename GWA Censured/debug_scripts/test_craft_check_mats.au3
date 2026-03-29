#RequireAdmin
#include "lib\Froggy_Includes.au3"

; Quick check: what materials does DISCOPANIC have?
ScanAndUpdateGameClients()
ConsoleWrite("Clients: " & $game_clients[0][0] & @CRLF)
; Find DISCOPANIC by checking each client
Local $found = False
For $i = 1 To $game_clients[0][0]
	ConsoleWrite("  " & $i & ": PID=" & $game_clients[$i][0] & " '" & $game_clients[$i][1] & "'" & @CRLF)
Next
; Launch DISCOPANIC if not found
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
	If StringInStr($game_clients[$i][1], "D I S C O P A N I C") Then $targetIdx = $i
Next
If $targetIdx = -1 Then
	ConsoleWrite("Launching DISCOPANIC..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
	Local $lr = GWLauncher_LaunchAccount($accounts, $accIdx)
	ConsoleWrite("PID=" & $lr[0] & " waiting 55s..." & @CRLF)
	Sleep(55000)
	ScanAndUpdateGameClients()
	For $i = 1 To $game_clients[0][0]
		If $game_clients[$i][0] = $lr[0] Then
			$targetIdx = $i
			ExitLoop
		EndIf
	Next
	If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]
EndIf
SelectClient($targetIdx)
InitializeGameClientForGWA2(False)

; Handle char select
If IsAtCharSelect() Then
	ConsoleWrite("At char select. Clicking Play..." & @CRLF)
	ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
	Local $t = TimerInit()
	While GetMyID() = 0
		Sleep(1000)
		WinActivate($game_clients[$targetIdx][2])
		If TimerDiff($t) > 60000 Then ExitLoop
	WEnd
	Sleep(3000)
EndIf
Local $ph = GetProcessHandle()

ConsoleWrite("Character: " & GetCharacterName() & " Gold: " & GetGoldCharacter() & @CRLF)

; Check common crafting materials
Local $mats[8][2] = [ _
	[945, "Iron Ingot"], _
	[929, "Glittering Dust"], _
	[921, "Bone"], _
	[933, "Feather"], _
	[955, "Granite Slab"], _
	[934, "Plant Fiber"], _
	[946, "Steel Ingot"], _
	[925, "Bolt of Cloth"] _
]

ConsoleWrite("=== Inventory Materials ===" & @CRLF)
For $i = 0 To 7
	Local $count = _GWA2_CountItemInBagsByModelID($mats[$i][0])
	If $count > 0 Then ConsoleWrite("  " & $mats[$i][1] & " (ID=" & $mats[$i][0] & "): " & $count & @CRLF)
Next

; Also check merchant items if dialog is open
Local $mb = GetMerchantItemsBase()
Local $ms = GetMerchantItemsSize()
If $ms > 0 Then
	ConsoleWrite(@CRLF & "=== Merchant Items ===" & @CRLF)
	Local $bp = MemoryRead($ph, GetLabel('BasePointer'), 'dword')
	For $i = 0 To $ms - 1
		Local $mid = MemoryRead($ph, $mb + 4 * $i, 'dword')
		If $mid > 0 Then
			; Resolve model ID
			Local $p1 = MemoryRead($ph, $bp, 'dword')
			If $p1 > 0x10000 Then
				Local $p2 = MemoryRead($ph, $p1 + 0x18, 'dword')
				If $p2 > 0x10000 Then
					Local $p3 = MemoryRead($ph, $p2 + 0x40, 'dword')
					If $p3 > 0x10000 Then
						Local $p4 = MemoryRead($ph, $p3 + 0xB8, 'dword')
						If $p4 > 0x10000 Then
							Local $ip = MemoryRead($ph, $p4 + 4 * $mid, 'dword')
							If $ip > 0x10000 Then
								Local $model = MemoryRead($ph, $ip + 0x2C, 'dword')
								ConsoleWrite("  [" & $i & "] itemID=" & $mid & " modelID=" & $model & @CRLF)
							EndIf
						EndIf
					EndIf
				EndIf
			EndIf
		EndIf
	Next
EndIf
