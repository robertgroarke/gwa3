#include-once
; =============================================================================
; BotCore-Loot.au3
;
; Extracted from Froggy_HM_v1.6.au3 as part of the function extraction project.
; All dependencies resolve through Froggy_Includes.au3 master include.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; === Module State ===
; (globals for this module go here)

; === Public Functions ===

; ---------------------------------------------------------------------------
; CountFreeSlots  --  counts free inventory slots across bags
; Dependencies: GetBagPtr(), MemRead()
; ---------------------------------------------------------------------------
Func CountFreeSlots($NumOfBags = 4)
	Local $lCount = 0
	Local $lBagPtr
	For $lBag = 1 To $NumOfBags
		$lBagPtr = GetBagPtr($lBag)
		If $lBagPtr = 0 Then ContinueLoop
		$lCount += MemRead($lBagPtr + 32, "long") - MemRead($lBagPtr + 16, "long")
	Next
	Return $lCount
EndFunc   ;==>CountFreeSlots

; ---------------------------------------------------------------------------
; GetPicksCount  --  counts Lockpicks in personal inventory (ModelID 22751)
; Dependencies: GetBag(), GetItemBySlot(), DllStructGetData()
; ---------------------------------------------------------------------------
Func GetPicksCount()
	Local $AmountPicks = 0
	Local $aBag
	Local $aItem
	Local $i
	For $i = 1 To 4 ; Count in personal inventory bags only
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == 22751 Then
				$AmountPicks += DllStructGetData($aItem, "Quantity")
			Else
				ContinueLoop
			EndIf
		Next
	Next
	Return $AmountPicks
EndFunc   ;==>GetPicksCount
