#include-once
; =============================================================================
; BotCore-Travel.au3
;
; Extracted from Froggy_HM_v1.6.au3 (KF-011, KF-012)
;
; KF-011: Death Penalty (DP) consumable helpers
;   UseDP()        - Master function: calls UseDP_Cycle() x5 to clear full DP
;   UseDP_Cycle()  - Single pass: tries each DP consumable type once
;   UseDP_Armor()  - Armor of Salvation  (ModelID 26784)
;   UseDP_Scroll() - Scroll of Resurrection (ModelID 22191)
;   UseDP_Honeycomb() - Honeycomb           (ModelID 21488)
;   UseDP_RRC()    - Red Rock Candy         (ModelID 21489)
;   UseDP_FourLeaf() - Four-Leaf Clover     (ModelID 6370)
;
; KF-012: Travel wrappers
;   TravelTo()        - Packet-based map travel
;   ResignAndReturn() - Resign, wait for death, return to outpost or travel
;
; Dependencies (resolved via Froggy_Includes.au3):
;   GWA2 API: GetBag, GetItemBySlot, UseItem, GetMapID, GetLanguage,
;             GetRegion, GetMapLoading, Resign, ReturnToOutpost,
;             WaitMapLoading, SendPacket, GetIsDead, GetAgentPtr
;   Globals:  $Outpost, $mBasePointer
;   Helpers:  Out(), Setup(), GUI_SetWipes(), GUI_GetWipes()
;
; DP consumable item IDs:
;   26784 = Armor of Salvation
;   22191 = Scroll of Resurrection
;   21488 = Honeycomb
;   21489 = Red Rock Candy
;    6370 = Four-Leaf Clover
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; === Constants ===
Global Const $DP_MODELID_ARMOR_OF_SALVATION  = 26784
Global Const $DP_MODELID_SCROLL_RESURRECTION = 22191
Global Const $DP_MODELID_HONEYCOMB           = 21488
Global Const $DP_MODELID_RED_ROCK_CANDY      = 21489
Global Const $DP_MODELID_FOUR_LEAF_CLOVER    = 6370

; Packet header for party travel (used by TravelTo)
If Not IsDeclared("HEADER_PARTY_TRAVEL") Then Global Const $HEADER_PARTY_TRAVEL = 0x00B0

; =============================================================================
; KF-011  Death Penalty Consumable Helpers
; =============================================================================

; ---- Master function: runs 5 cycles to clear up to 60% DP ----
Func UseDP()
	UseDP_Cycle()
	UseDP_Cycle()
	UseDP_Cycle()
	UseDP_Cycle()
	UseDP_Cycle()
EndFunc   ;==>UseDP

; ---- Single cycle: tries every DP consumable type once ----
Func UseDP_Cycle()
	UseDP_Armor()
	UseDP_Scroll()
	UseDP_Honeycomb()
	UseDP_RRC()
	UseDP_FourLeaf()
EndFunc   ;==>UseDP_Cycle

; ---- Individual consumable helpers (pattern: scan bags 1-4, use first match) ----

; Armor of Salvation (ModelID 26784)
Func UseDP_Armor()
	Local $aBag, $aItem
	Sleep(2)
	For $i = 1 To 4
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == $DP_MODELID_ARMOR_OF_SALVATION Then
				UseItem($aItem)
				Return True
			EndIf
		Next
	Next
	Return False
EndFunc   ;==>UseDP_Armor

; Scroll of Resurrection (ModelID 22191)
Func UseDP_Scroll()
	Local $aBag, $aItem
	Sleep(2)
	For $i = 1 To 4
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == $DP_MODELID_SCROLL_RESURRECTION Then
				UseItem($aItem)
				Return True
			EndIf
		Next
	Next
	Return False
EndFunc   ;==>UseDP_Scroll

; Honeycomb (ModelID 21488)
Func UseDP_Honeycomb()
	Local $aBag, $aItem
	Sleep(2)
	For $i = 1 To 4
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == $DP_MODELID_HONEYCOMB Then
				UseItem($aItem)
				Return True
			EndIf
		Next
	Next
	Return False
EndFunc   ;==>UseDP_Honeycomb

; Red Rock Candy (ModelID 21489)
Func UseDP_RRC()
	Local $aBag, $aItem
	Sleep(2)
	For $i = 1 To 4
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == $DP_MODELID_RED_ROCK_CANDY Then
				UseItem($aItem)
				Return True
			EndIf
		Next
	Next
	Return False
EndFunc   ;==>UseDP_RRC

; Four-Leaf Clover (ModelID 6370)
Func UseDP_FourLeaf()
	Local $aBag, $aItem
	Sleep(2)
	For $i = 1 To 4
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == $DP_MODELID_FOUR_LEAF_CLOVER Then
				UseItem($aItem)
				Return True
			EndIf
		Next
	Next
	Return False
EndFunc   ;==>UseDP_FourLeaf

; =============================================================================
; KF-012  Travel Wrappers
; =============================================================================

; ---- Resign, wait for party wipe, then return to outpost or travel ----
; $aMapID = 0 means use ReturnToOutpost(); nonzero means TravelTo() that map.
Func ResignAndReturn($aMapID = 0, $aLanguage = -1, $aRegion = -1)
	If $aLanguage = -1 Then $aLanguage = GetLanguage()
	If $aRegion = -1 Then $aRegion = GetRegion()

	Local $targetMap = ($aMapID = 0) ? $Outpost : $aMapID

	Out("Resigning")
	Resign()

	; Update GUI stats if function exists
	GUI_SetWipes(GUI_GetWipes() + 1)

	Local $lDeadlock = TimerInit()
	Do
		Sleep(100)
	Until GetIsDead(-2) Or TimerDiff($lDeadlock) >= 5000
	Sleep(1000)

	Out("Returning To Outpost")
	If $aMapID = 0 Then
		ReturnToOutpost()
		WaitMapLoading($targetMap)
	Else
		TravelTo($aMapID, $aLanguage, $aRegion)
	EndIf
	Return Setup(False)
EndFunc   ;==>ResignAndReturn

; ---- Packet-based travel to a map ----
Func TravelTo($aMapID, $aLanguage = -1, $aRegion = -1)
	If $aLanguage = -1 Then $aLanguage = GetLanguage()
	If $aRegion = -1 Then $aRegion = GetRegion()

	If GetMapID() = $aMapID And GetLanguage() = $aLanguage And GetMapLoading() = 2 Then
		Out("Already at destination")
		Return True
	EndIf

	SendPacket(0x18, $HEADER_PARTY_TRAVEL, $aMapID, $aRegion, 0, $aLanguage, False)
	WaitMapLoading($aMapID)
EndFunc   ;==>TravelTo
