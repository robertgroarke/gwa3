#include-once
; No #include needed — all dependencies resolve through Froggy_Includes.au3

; ==================================================================================================
; GWA2_Extensions.au3
;
; Custom GWA Censured functions with NO upstream BotsHub equivalent.
; Functions that upstream also defines (IsPlayerDead, UseSkillEx, etc.)
; have been removed — upstream's versions are used instead.
;
; Remaining custom functions:
;   - GetIsMerchantOpen, GetItemIDFromModelID, GetMerchantItemPtrByModelId
;   - _GWA2_GetAlmostInRangeOfAgent, _GWA2_GetInventoryItemPtrByModelId,
;     _GWA2_CountItemInBagsByModelID
;   - GetMapLoading, GetLoggedIn (no upstream equivalent)
;   - Extend_Write, Extend_AssemblerWriteDetour (extensibility stubs)
;
; Updated 2026-03-25 for Phase 4b (upstream file adoption)
; ==================================================================================================


; ==================================================================================================
; Merchant / Trading Helpers
; ==================================================================================================

;~ Check if merchant window is open (by checking if merchant items are populated)
Func GetIsMerchantOpen()
	Return GetMerchantItemsSize() > 0
EndFunc

;~ Find an item with the provided modelId in your inventory and return its itemID
Func GetItemIDFromModelID($modelID)
	For $i = 1 To $bags_count
		For $j = 1 To DllStructGetData(GetBag($i), 'slots')
			Local $item = GetItemBySlot($i, $j)
			If DllStructGetData($item, 'ModelId') == $modelID Then Return DllStructGetData($item, 'Id')
		Next
	Next
EndFunc

; GetMerchantItemPtrByModelID — provided by botshub/GWA2.au3


; ==================================================================================================
; Internal Helpers (prefixed with _GWA2_)
; ==================================================================================================

Func _GWA2_GetAlmostInRangeOfAgent($targetAgent, $proximity = ($RANGE_SPELLCAST + 100))
	Local $distance = GetDistance(GetMyAgent(), $targetAgent)
	If $distance > $proximity Then
		MoveTo(DllStructGetData($targetAgent, 'X'), DllStructGetData($targetAgent, 'Y'))
		Do
			Sleep(100)
			$distance = GetDistance(GetMyAgent(), $targetAgent)
		Until $distance <= $proximity
	EndIf
EndFunc

Func _GWA2_GetInventoryItemPtrByModelId($modelID)
	Local $bagPtr, $slots, $itemPtr
	For $i = 1 To 4 ; Backpack, Belt Pouch, Bag 1, Bag 2
		$bagPtr = GetBagPtr($i)
		If $bagPtr Then
			$slots = GetMaxSlots($bagPtr)
			For $j = 1 To $slots
				$itemPtr = GetItemPtrBySlot($bagPtr, $j)
				If $itemPtr Then
					; ModelID is at offset 44 (long)
					If MemRead($itemPtr + 44, 'long') == $modelID Then Return $itemPtr
				EndIf
			Next
		EndIf
	Next
	Return 0
EndFunc

Func _GWA2_CountItemInBagsByModelID($modelID)
	Local $count = 0
	Local $bagPtr, $slots, $itemPtr
	For $i = 1 To 4
		$bagPtr = GetBagPtr($i)
		If $bagPtr Then
			$slots = GetMaxSlots($bagPtr)
			For $j = 1 To $slots
				$itemPtr = GetItemPtrBySlot($bagPtr, $j)
				If $itemPtr Then
					If MemRead($itemPtr + 44, 'long') == $modelID Then
						; Quantity is at offset 76 (short)
						$count += MemRead($itemPtr + 76, 'short')
					EndIf
				EndIf
			Next
		EndIf
	Next
	Return $count
EndFunc


; ==================================================================================================
; Extensibility Stubs
; ==================================================================================================

Func Extend_Write()
	; Placeholder if missing
EndFunc

Func Extend_AssemblerWriteDetour()
	; Placeholder if missing
EndFunc


; ==================================================================================================
; Custom Game State Functions (no upstream equivalent)
; ==================================================================================================

;~ Returns the map loading state:
;~   0 = Outpost, 1 = Explorable area, 2 = Loading
Func GetMapLoading()
	Return MemRead($instance_info_ptr)
EndFunc

;~ Returns True if logged into a character, False at character select
Func GetLoggedIn()
	Local $myID = GetMyID()
	If $myID <= 0 Then Return False
	Return GetAgentExists($myID)
EndFunc
