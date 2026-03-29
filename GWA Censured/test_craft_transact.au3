#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_craft_transact.au3
;
; Craft items by calling the game's TransactItem function directly via
; shellcode on the command queue. No GWCA, no UI frame clicks.
;
; From GWCA MerchantMgr.cpp, the game's TransactItem is cdecl:
;   TransactItem(type, gold_give, give{count,*ids,*qtys}, gold_recv, recv{count,*ids,*qtys})
;
; Scan pattern: "\x85\xFF\x74\x1D\x8B\x4D\x14\xEB\x08" at offset -0x7F
;
; TransactionType::CrafterBuy = 3
; =============================================================================

Global Const $MAP_EMBARK_BEACH = 857

ConsoleWrite("=== Native TransactItem Craft Test ===" & @CRLF)

; --- Find DISCOPANIC or launch fresh ---
ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
	ConsoleWrite("  Client " & $i & ": PID=" & $game_clients[$i][0] & " '" & $game_clients[$i][1] & "'" & @CRLF)
	If StringInStr($game_clients[$i][1], "D I S C O P A N I C") Or StringInStr($game_clients[$i][1], "DISCOPANIC") Then
		$targetIdx = $i
	EndIf
Next

; If not found by title, use the last client (newest launched)
If $targetIdx = -1 Then
	$targetIdx = $game_clients[0][0]
	ConsoleWrite("Using last client " & $targetIdx & " (PID=" & $game_clients[$targetIdx][0] & ")" & @CRLF)
EndIf

If $targetIdx = -1 Then
	ConsoleWrite("DISCOPANIC not found. Launching..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
	If $accIdx = -1 Then
		ConsoleWrite("ERROR: Account not found" & @CRLF)
		Exit 1
	EndIf
	Local $launchResult = GWLauncher_LaunchAccount($accounts, $accIdx)
	If $launchResult = 0 Then
		ConsoleWrite("ERROR: Launch failed" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("Launched PID=" & $launchResult[0] & ", waiting 55s..." & @CRLF)
	Sleep(55000)
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

; --- Scan for TransactItem function ---
ConsoleWrite("Scanning for TransactItem function..." & @CRLF)
Local $textStart = $pe_sections_ranges[0][0]
Local $textEnd = $pe_sections_ranges[0][1]
Local $textSize = $textEnd - $textStart

; Pattern: 85 FF 74 1D 8B 4D 14 EB 08
Local $pattern[9] = [0x85, 0xFF, 0x74, 0x1D, 0x8B, 0x4D, 0x14, 0xEB, 0x08]
Local $transactFunc = 0

Local $chunkSize = 65536
For $offset = 0 To $textSize - 16 Step $chunkSize
	Local $readSize = $chunkSize + 16
	If $offset + $readSize > $textSize Then $readSize = $textSize - $offset
	Local $chunk = DllStructCreate('byte[' & $readSize & ']')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
		'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)

	For $j = 1 To $readSize - 9
		Local $match = True
		For $k = 0 To 8
			If DllStructGetData($chunk, 1, $j + $k) <> $pattern[$k] Then
				$match = False
				ExitLoop
			EndIf
		Next
		If $match Then
			; Function start is at pattern_addr - 0x7F
			$transactFunc = ($textStart + $offset + ($j - 1)) - 0x7F
			ExitLoop 2
		EndIf
	Next
Next

If $transactFunc = 0 Then
	ConsoleWrite("ERROR: TransactItem function not found" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("TransactItem at 0x" & Hex($transactFunc) & @CRLF)

; Verify first bytes
Local $funcBytes = DllStructCreate('byte[8]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($transactFunc), _
	'ptr', DllStructGetPtr($funcBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)
Local $hex = ""
For $b = 1 To 8
	$hex &= Hex(DllStructGetData($funcBytes, 1, $b), 2) & " "
Next
ConsoleWrite("First bytes: " & $hex & @CRLF)

; Also check the BotsHub Transaction label for comparison
Local $botshubTransaction = Int(GetLabel('Transaction'))
ConsoleWrite("BotsHub Transaction label: 0x" & Hex($botshubTransaction) & @CRLF)

; --- Wait for character to be in-game ---
ConsoleWrite("Waiting for character to load..." & @CRLF)
WinActivate($gwHWnd)

Local $loadWait = TimerInit()
Local $clickedPlay = False
While GetMyID() = 0
	Sleep(1000)
	WinActivate($gwHWnd)  ; keep window focused for rendering hook

	; Check for char select after initial load
	If Not $clickedPlay And TimerDiff($loadWait) > 5000 Then
		If IsAtCharSelect() Then
			ConsoleWrite("At char select. Clicking Play..." & @CRLF)
			ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
			$clickedPlay = True
			Sleep(5000)
		EndIf
	EndIf

	If TimerDiff($loadWait) > 120000 Then
		ConsoleWrite("ERROR: Character load timeout (MyID=" & GetMyID() & " charsel=" & IsAtCharSelect() & ")" & @CRLF)
		Exit 1
	EndIf
WEnd
Sleep(5000)  ; settle time after map load
ConsoleWrite("In game! MyID=" & GetMyID() & " MapID=" & GetMapID() & @CRLF)

; --- Travel to Embark Beach ---
If GetMapID() <> $MAP_EMBARK_BEACH Then
	ConsoleWrite("Traveling to Embark Beach..." & @CRLF)
	TravelToOutpost($MAP_EMBARK_BEACH)
	WaitMapLoading($MAP_EMBARK_BEACH, 30000)
	Sleep(5000)
EndIf
ConsoleWrite("At Embark Beach. MapID=" & GetMapID() & @CRLF)

; --- Walk to Eyja and open dialog ---
ConsoleWrite("Going to Eyja..." & @CRLF)
If Not GoToConsumableTrader("Eyja") Then
	ConsoleWrite("ERROR: Could not reach Eyja" & @CRLF)
	Exit 1
EndIf
Sleep(2000)
ConsoleWrite("Dialog opened with Eyja" & @CRLF)

; --- Wait for merchant items to populate ---
ConsoleWrite("Waiting for merchant items..." & @CRLF)
Local $merchWait = TimerInit()
Local $merchantBase = 0
Local $merchantSize = 0
While $merchantSize = 0
	Sleep(500)
	$merchantBase = GetMerchantItemsBase()
	$merchantSize = GetMerchantItemsSize()
	If TimerDiff($merchWait) > 10000 Then ExitLoop
WEnd
ConsoleWrite("Merchant items: base=0x" & Hex($merchantBase) & " size=" & $merchantSize & @CRLF)

If $merchantBase = 0 Or $merchantSize = 0 Then
	ConsoleWrite("ERROR: Merchant items not available" & @CRLF)
	Exit 1
EndIf

; --- Find Grail of Might in merchant list ---
Local $grailModelID = 24861  ; Grail of Might

; Find grail item ID in merchant list using BotsHub's item pointer chain
Local $grailItemID = 0
Local $base_ptr = MemoryRead($processHandle, GetLabel('BasePointer'), 'dword')
For $i = 0 To $merchantSize - 1
	Local $mItemID = MemoryRead($processHandle, $merchantBase + 4 * $i, 'dword')
	If $mItemID > 0 Then
		; Resolve item ptr through chain: [base][0x18][0x40][0xB8][itemID*4]
		Local $ptr1 = MemoryRead($processHandle, $base_ptr, 'dword')
		If $ptr1 > 0x10000 Then
			Local $ptr2 = MemoryRead($processHandle, $ptr1 + 0x18, 'dword')
			If $ptr2 > 0x10000 Then
				Local $ptr3 = MemoryRead($processHandle, $ptr2 + 0x40, 'dword')
				If $ptr3 > 0x10000 Then
					Local $ptr4 = MemoryRead($processHandle, $ptr3 + 0xB8, 'dword')
					If $ptr4 > 0x10000 Then
						Local $itemPtr = MemoryRead($processHandle, $ptr4 + 4 * $mItemID, 'dword')
						If $itemPtr > 0x10000 Then
							Local $modelID = MemoryRead($processHandle, $itemPtr + 0x2C, 'dword')
							If $modelID = $grailModelID Then
								$grailItemID = $mItemID
								ConsoleWrite("Found Grail: itemID=" & $grailItemID & " index=" & $i & @CRLF)
								ExitLoop
							EndIf
						EndIf
					EndIf
				EndIf
			EndIf
		EndIf
	EndIf
Next

If $grailItemID = 0 Then
	ConsoleWrite("ERROR: Grail of Might not in merchant list" & @CRLF)
	Exit 1
EndIf

; --- Find material stacks in inventory ---
; Grail of Might: 50 Iron Ingots (ModelID=945) + 50 Glittering Dust (ModelID=929)
ConsoleWrite("Finding materials..." & @CRLF)
Local $ironModelID = 945
Local $dustModelID = 929

; Count materials (use the lib function available in our includes)
Local $ironCount = _GWA2_CountItemInBagsByModelID($ironModelID)
Local $dustCount = _GWA2_CountItemInBagsByModelID($dustModelID)
ConsoleWrite("Iron Ingots: " & $ironCount & " (need 50)" & @CRLF)
ConsoleWrite("Glittering Dust: " & $dustCount & " (need 50)" & @CRLF)

If $ironCount < 50 Or $dustCount < 50 Then
	ConsoleWrite("WARNING: Not enough materials — will attempt anyway to test function call" & @CRLF)
EndIf

; Get item IDs for material stacks (may be 0 if not in inventory)
Local $ironItemID = GetItemIDFromModelID($ironModelID)
Local $dustItemID = GetItemIDFromModelID($dustModelID)
ConsoleWrite("Iron ItemID=" & $ironItemID & " Dust ItemID=" & $dustItemID & @CRLF)
; If no materials, use dummy IDs (call will fail but won't crash)
If $ironItemID = 0 Then $ironItemID = 1
If $dustItemID = 0 Then $dustItemID = 2

; --- Build TransactItem call ---
; cdecl: TransactItem(type=3, gold_give=250, give={2, &ids, &qtys}, gold_recv=0, recv={1, &recvId, &recvQty})
; Struct layout on stack (cdecl, all args pushed right-to-left):
;   push recv.item_quantities ptr
;   push recv.item_ids ptr
;   push recv.item_count (1)
;   push gold_recv (0)
;   push give.item_quantities ptr
;   push give.item_ids ptr
;   push give.item_count (2)
;   push gold_give (250)
;   push type (3)
;   call TransactItem
;   add esp, 36  (9 args * 4 bytes... wait)
;
; Actually TransactionInfo is a struct of 3 fields = 12 bytes.
; Cdecl passes structs BY VALUE on the stack. So the actual stack layout is:
;   type (4 bytes)
;   gold_give (4 bytes)
;   give.item_count (4 bytes)
;   give.item_ids (4 bytes)
;   give.item_quantities (4 bytes)
;   gold_recv (4 bytes)
;   recv.item_count (4 bytes)
;   recv.item_ids (4 bytes)
;   recv.item_quantities (4 bytes)
; Total: 36 bytes on stack

Local $goldBefore = GetGoldCharacter()
ConsoleWrite(@CRLF & "=== CRAFTING ===" & @CRLF)
ConsoleWrite("Gold before: " & $goldBefore & @CRLF)

; Allocate game memory for data arrays
Local $dataMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
	'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 128, _
	'dword', 0x1000, 'dword', 0x40)
Local $dataAddr = Int($dataMem[0])

; Layout in data memory:
; +0x00: give_ids[2] = {ironItemID, dustItemID}
; +0x08: give_qtys[2] = {50, 50}
; +0x10: recv_ids[1] = {grailItemID}
; +0x14: recv_qtys[1] = {1}
; +0x18: TransactItem func ptr
; +0x1C: shellcode (32 bytes)

Local $giveIdsAddr = $dataAddr + 0x00
Local $giveQtysAddr = $dataAddr + 0x08
Local $recvIdsAddr = $dataAddr + 0x10
Local $recvQtysAddr = $dataAddr + 0x14
Local $funcPtrAddr = $dataAddr + 0x18
Local $shellcodeAddr = $dataAddr + 0x1C

; Write data arrays
Local $giveIds = DllStructCreate('dword[2]')
DllStructSetData($giveIds, 1, $ironItemID, 1)
DllStructSetData($giveIds, 1, $dustItemID, 2)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($giveIdsAddr), _
	'ptr', DllStructGetPtr($giveIds), 'ulong_ptr', 8, 'ulong_ptr*', 0)

Local $giveQtys = DllStructCreate('dword[2]')
DllStructSetData($giveQtys, 1, 50, 1)
DllStructSetData($giveQtys, 1, 50, 2)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($giveQtysAddr), _
	'ptr', DllStructGetPtr($giveQtys), 'ulong_ptr', 8, 'ulong_ptr*', 0)

Local $recvIds = DllStructCreate('dword[1]')
DllStructSetData($recvIds, 1, $grailItemID, 1)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($recvIdsAddr), _
	'ptr', DllStructGetPtr($recvIds), 'ulong_ptr', 4, 'ulong_ptr*', 0)

Local $recvQtys = DllStructCreate('dword[1]')
DllStructSetData($recvQtys, 1, 1, 1)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($recvQtysAddr), _
	'ptr', DllStructGetPtr($recvQtys), 'ulong_ptr', 4, 'ulong_ptr*', 0)

; Write func ptr
MemoryWrite($processHandle, $funcPtrAddr, $transactFunc, 'dword')

; Build shellcode: push all 9 dwords in reverse, call, cleanup, ret
Local $sc = DllStructCreate('byte[64]')
Local $p = 1

; push recv.item_quantities ptr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $recvQtysAddr)
$p += 4
; push recv.item_ids ptr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $recvIdsAddr)
$p += 4
; push recv.item_count = 1
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x01, $p)
$p += 1
; push gold_recv = 0
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x00, $p)
$p += 1
; push give.item_quantities ptr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $giveQtysAddr)
$p += 4
; push give.item_ids ptr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $giveIdsAddr)
$p += 4
; push give.item_count = 2
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x02, $p)
$p += 1
; push gold_give = 250
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, 250)
$p += 4
; push type = 3 (CrafterBuy)
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x03, $p)
$p += 1
; call [funcPtrAddr]
DllStructSetData($sc, 1, 0xFF, $p)
$p += 1
DllStructSetData($sc, 1, 0x15, $p)
$p += 1
_WriteLE32($sc, $p, $funcPtrAddr)
$p += 4
; add esp, 36
DllStructSetData($sc, 1, 0x83, $p)
$p += 1
DllStructSetData($sc, 1, 0xC4, $p)
$p += 1
DllStructSetData($sc, 1, 0x24, $p)
$p += 1
; ret
DllStructSetData($sc, 1, 0xC3, $p)

ConsoleWrite("Shellcode: " & $p & " bytes at 0x" & Hex($shellcodeAddr) & @CRLF)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($shellcodeAddr), _
	'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

; Queue via command queue
$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $shellcodeAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

ConsoleWrite("Craft command queued. Waiting..." & @CRLF)
Sleep(3000)

Local $goldAfter = GetGoldCharacter()
ConsoleWrite("Gold after: " & $goldAfter & @CRLF)
ConsoleWrite("Gold change: " & ($goldAfter - $goldBefore) & @CRLF)

If $goldAfter < $goldBefore Then
	ConsoleWrite("*** CRAFT SUCCESSFUL! Gold decreased by " & ($goldBefore - $goldAfter) & " ***" & @CRLF)
Else
	ConsoleWrite("Gold unchanged — craft may have failed" & @CRLF)
EndIf

; Cleanup
DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
	'handle', $processHandle, 'ptr', Ptr($dataAddr), 'ulong_ptr', 0, 'dword', 0x8000)

ConsoleWrite("=== DONE ===" & @CRLF)
