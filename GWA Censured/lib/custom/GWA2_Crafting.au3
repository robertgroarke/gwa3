#include-once

; =============================================================================
; GWA2_Crafting.au3
;
; Custom crafting system for GWA Censured. This code hooks into GWA2.au3
; via the extension point pattern (IsDeclared checks).
;
; NOTE: This crafting system has never worked properly. It is extracted
; here for clean separation from BotsHub code. Fix attempts should be
; made in this file, not in GWA2.au3.
;
; Hook points in GWA2.au3 (3 one-line additions):
;   CreateData():                If IsDeclared('g_CraftingExtension') Then Extend_CraftingData()
;   CreateCommands():            If IsDeclared('g_CraftingExtension') Then Extend_CraftingCommands()
;   InitializeGameClientData():  If IsDeclared('g_CraftingExtension') Then Extend_CraftingInit()
;
; Extracted 2026-03-25
; =============================================================================

; Extension flag — GWA2.au3 checks for this via IsDeclared()
Global $g_CraftingExtension = True

; Crafting globals
Global $CRAFT_ITEM_STRUCT = DllStructCreate("ptr;dword;dword;dword;dword;ptr;ptr")
Global $CRAFT_ITEM_STRUCT_PTR = DllStructGetPtr($CRAFT_ITEM_STRUCT)
Global $CRAFT_MATS_STRUCT_MEMORY, $CRAFT_QTYS_STRUCT_MEMORY
Global $trade_id_addr = 0, $trade_id_value_addr = 0

; =============================================================================
; Hook: Called from CreateData() to add crafting memory slots
; =============================================================================
Func Extend_CraftingData()
	_('TradeID/4')
	_('TradeID_Value/4')
EndFunc

; =============================================================================
; Hook: Called from CreateCommands() to add crafting ASM procedures
; =============================================================================
Func Extend_CraftingCommands()
	; CommandCraftItemEx: Direct CraftItemFunction call
	_('CommandCraftItemEx:')
	_('lea edx,dword[eax+8]')
	_('push edx')
	_('mov ecx,dword[TradeID]')
	_('mov ecx,dword[ecx]')
	_('push ecx')
	_('mov ebx,dword[eax+4]')
	_('push ebx')
	_('mov eax,dword[BuyItemBase]')
	_('push eax')
	_('call CraftItemFunction')
	_('add esp,10')
	_('ljmp CommandReturn')

	; CommandCraftItemEx2: TransactionFunction opcode 3 implementation
	; CRAFT_ITEM_STRUCT layout:
	;   +0  CmdAddr            +14  MatIDsArray_PTR
	;   +4  AmountToCraft      +18  MatQtysArray_PTR
	;   +8  MerchantItemID
	;   +C  TotalCost
	;   +10 GiveCount
	_('CommandCraftItemEx2:')
	_('mov ecx,dword[eax+C]')   ; ecx = TotalCost
	_('mov edx,eax')
	_('add edx,4')
	_('push edx')               ; Arg9: &AmountToCraft
	_('mov edx,eax')
	_('add edx,8')
	_('push edx')               ; Arg8: &MerchantItemID
	_('push 1')                 ; Arg7: RecvCount
	_('push 0')                 ; Arg6: RecvGold
	_('add eax,C')
	_('mov edx,dword[eax+C]')   ; Arg5: MatQtysArray_PTR
	_('push edx')
	_('mov edx,dword[eax+8]')   ; Arg4: MatIDsArray_PTR
	_('push edx')
	_('mov edx,dword[eax+4]')   ; Arg3: GiveCount
	_('push edx')
	_('push ecx')               ; Arg2: TotalCost
	_('push 3')                 ; Arg1: CrafterBuy opcode
	_('call TransactionFunction')
	_('add esp,24')
	_('ljmp CommandReturn')

	; CommandRequestCraftQuote: Request crafting price quote (opcode 3)
	_('CommandRequestCraftQuote:')
	_('mov esi,eax')
	_('add esi,4')
	_('push esi')               ; recv.item_ids
	_('push 1')                 ; recv.item_count
	_('push 0')                 ; recv.unknown
	_('push 0')                 ; give.item_ids
	_('push 0')                 ; give.item_count
	_('push 0')                 ; give.unknown
	_('push 0')                 ; arg2
	_('push 3')                 ; type = CrafterBuy
	_('mov ecx,0')
	_('mov edx,2')
	_('call RequestQuoteFunction')
	_('add esp,20')
	_('ljmp CommandReturn')

	; CommandCraftExecute: Execute crafting transaction (opcode 3)
	_('CommandCraftExecute:')
	_('mov edx,eax')
	_('add edx,C')
	_('push edx')               ; Arg9: &Amount
	_('mov ecx,eax')
	_('add ecx,4')
	_('push ecx')               ; Arg8: &ItemID
	_('push 1')                 ; Arg7: recv count
	_('push 0')                 ; Arg6: gold_recv
	_('push 0')                 ; Arg5: give quantities
	_('push 0')                 ; Arg4: give item_ids
	_('push 0')                 ; Arg3: give count
	_('mov edx,dword[eax+8]')
	_('push edx')               ; Arg2: gold_give (Cost)
	_('push 3')                 ; Arg1: CrafterBuy
	_('call TransactionFunction')
	_('add esp,24')
	_('ljmp CommandReturn')
EndFunc

; =============================================================================
; Hook: Called from InitializeGameClientData() after ModifyMemory()
; =============================================================================
Func Extend_CraftingInit()
	; Verify TradeID was properly initialized
	If GetValue('TradeID') = 0 Then
		Debug("Warning: TradeID is 0 after ModifyMemory. Retrying...")
		Sleep(500)
		ModifyMemory()
	EndIf
	$trade_id_addr = GetValue('TradeID')
	$trade_id_value_addr = GetValue('TradeID_Value')
EndFunc

; =============================================================================
; CraftItem — Main crafting function (NON-FUNCTIONAL — needs fixing)
; =============================================================================
Func CraftItem($modelID, $amount, $gold, ByRef $materialsArray)
	; Sanity checks
	If Not GetIsMerchantOpen() Then Return SetError(1)

	Local $merchantItemsBase = GetMerchantItemsBase()
	If Not $merchantItemsBase Then Return SetError(2)
	Local $merchantItemsSize = GetMerchantItemsSize()

	Local $itemPtr = 0
	Local $destinationItemPtr = 0
	Local $itemIndex = -1
	Local $itemID = 0

	For $i = 0 To $merchantItemsSize - 1
		$itemID = MemoryRead($merchantItemsBase + 4 * $i)
		If ($itemID) Then
			Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 4 * $itemID]
			Local $result = MemoryReadPtr($base_address_ptr, $offsets)
			$itemPtr = $result[1]

			If $itemPtr <> 0 And MemoryRead($itemPtr + 0x2C) = $modelID Then
				$destinationItemPtr = $itemPtr
				$itemIndex = $i
				ExitLoop
			EndIf
		EndIf
	Next

	If $itemIndex = -1 Then Return SetError(3)

	; Check materials
	Local $sourceItemPtr = _GWA2_GetInventoryItemPtrByModelId($materialsArray[0][0])
	If ((Not $sourceItemPtr) Or (MemoryRead($sourceItemPtr + 0x4B) < $materialsArray[0][1])) Then Return 0

	Local $materialString = ''
	Local $materialCount = 0
	If IsArray($materialsArray) = 0 Then Return 0
	Local $materialsArraySize = UBound($materialsArray) - 1
	For $i = $materialsArraySize To 0 Step -1
		Local $checkQuantity = _GWA2_CountItemInBagsByModelID($materialsArray[$i][0])
		If $materialsArray[$i][1] * $amount > $checkQuantity Then
			Return SetExtended($materialsArray[$i][1] * $amount - $checkQuantity, $materialsArray[$i][0])
		EndIf
	Next
	Local $currentGold = GetGoldCharacter()

	For $i = 0 To $materialsArraySize
		$materialString &= GetItemIDFromModelID($materialsArray[$i][0]) & ';'
		Debug($materialString)
		$materialCount += 1
	Next

	Local $craftingMaterialType = 'dword'
	For $i = 1 To $materialCount - 1
		$craftingMaterialType &= ';dword'
	Next
	Local $craftingMaterialStruct = SafeDllStructCreate($craftingMaterialType)
	Local $craftingMaterialStructPtr = DllStructGetPtr($craftingMaterialStruct)
	For $i = 1 To $materialCount
		Local $size = StringInStr($materialString, ';')
		DllStructSetData($craftingMaterialStruct, $i, StringLeft($materialString, $size - 1))
		$materialString = StringTrimLeft($materialString, $size)
	Next

	Local $memorySize = $materialCount * 4
	Local $processHandle = GetProcessHandle()
	Local $memoryBuffer = SafeDllCall13($kernel_handle, 'ptr', 'VirtualAllocEx', 'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $memorySize, 'dword', 0x1000, 'dword', 0x40)
	If $memoryBuffer = 0 Or $memoryBuffer[0] = 0 Then Return 0

	SafeDllCall13($kernel_handle, 'int', 'WriteProcessMemory', 'int', $processHandle, 'int', $memoryBuffer[0], 'ptr', $craftingMaterialStructPtr, 'int', $memorySize, 'int', 0)

	; Write the index to TradeID address
	If $trade_id_addr <> 0 Then
		MemoryWrite($trade_id_value_addr, $itemIndex, 'dword')
		MemoryWrite($trade_id_addr, $trade_id_value_addr, 'dword')
	Else
		Debug("Error: TradeID address not initialized")
		Return 0
	EndIf

	DllStructSetData($CRAFT_ITEM_STRUCT, 1, GetValue('CommandCraftItemEx2'))
	DllStructSetData($CRAFT_ITEM_STRUCT, 2, $amount)
	DllStructSetData($CRAFT_ITEM_STRUCT, 3, $destinationItemPtr)
	DllStructSetData($CRAFT_ITEM_STRUCT, 4, $memoryBuffer[0])
	DllStructSetData($CRAFT_ITEM_STRUCT, 5, $materialCount)
	DllStructSetData($CRAFT_ITEM_STRUCT, 6, $amount * $gold)
	Enqueue($CRAFT_ITEM_STRUCT_PTR, 24)

	Local $deadlock = TimerInit()
	Local $currentAmount
	Do
		Sleep(250)
		$currentAmount = _GWA2_CountItemInBagsByModelID($materialsArray[0][0])
	Until $currentAmount <> $checkQuantity Or $currentGold <> GetGoldCharacter() Or TimerDiff($deadlock) > 5000

	SafeDllCall11($kernel_handle, 'ptr', 'VirtualFreeEx', 'handle', $processHandle, 'ptr', $memoryBuffer[0], 'int', 0, 'dword', 0x8000)

	Return SetExtended($checkQuantity - $currentAmount - $materialsArray[0][1] * $amount, True)
EndFunc
