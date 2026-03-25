#include-once
#include '..\GWA2.au3'
#include 'UsefulMods.au3'
#include 'RareSkins.au3'

;~ Local constants to avoid circular dependency issues with GWA2_ID.au3 include order
Global Const $__RARITY_GREEN			= 2627

Global Const $__TYPE_SALVAGE			= 0
Global Const $__TYPE_AXE				= 2
Global Const $__TYPE_BOW				= 5
Global Const $__TYPE_RUNE_AND_MOD		= 8
Global Const $__TYPE_USABLE				= 9
Global Const $__TYPE_DYE				= 10
Global Const $__TYPE_MATERIAL			= 11
Global Const $__TYPE_OFFHAND			= 12
Global Const $__TYPE_HAMMER				= 15
Global Const $__TYPE_KEY				= 18
Global Const $__TYPE_WAND				= 22
Global Const $__TYPE_SHIELD				= 24
Global Const $__TYPE_STAFF				= 26
Global Const $__TYPE_SWORD				= 27
Global Const $__TYPE_KIT				= 29
Global Const $__TYPE_DAGGER				= 32
Global Const $__TYPE_UPGRADE			= 8		; Same as RUNE_AND_MOD
Global Const $__TYPE_ARMOR_SALVAGE		= 0		; Same as SALVAGE
Global Const $__TYPE_SCYTHE				= 35
Global Const $__TYPE_SPEAR				= 36

Global Const $TYPE_ID [12] = [$__TYPE_STAFF, $__TYPE_WAND, $__TYPE_OFFHAND, $__TYPE_SHIELD, $__TYPE_AXE, $__TYPE_BOW, $__TYPE_HAMMER, $__TYPE_DAGGER, $__TYPE_SCYTHE, $__TYPE_SPEAR, $__TYPE_SWORD, $__TYPE_SALVAGE]

;~ Wrappers to match reference logic using GWA2 infrastructure

Func StartSalvage($aItem, $useRareKit = False)
	Local $kit
	If $useRareKit Then
		; Try to find high tier kits
		$kit = GetItemByModelID(5899) ; Superior
		If $kit = 0 Then $kit = GetItemByModelID(2992) ; Expert
		If $kit = 0 Then $kit = GetSalvageKit(False) ; Fallback
	Else
		$kit = GetSalvageKit(False)
	EndIf

	If $kit = 0 Then 
		Out("Debug: No salvage kit found!")
		Return False
	EndIf
	
	StartSalvageWithKit($aItem, $kit)
	Return True
EndFunc

; Note: SalvageMaterials and SalvageMod are already in GWA2.au3





;~ Ported Functions: CanSell & HasUsefulMod

Func CanSell($aItem)
	Local $lType = MemRead(GetItemPtr($aItem) + 32, "byte")
	Local $lModelID = MemRead(GetItemPtr($aItem) + 44, "long")
	Local $lQuantity = GetQuantity($aItem), $lValue = GetItemValue($aItem)
	Local $lReq = GetItemReq($aItem), $lDmg = GetItemMaxDmg($aItem), $lRarity = GetRarity($aItem)
	
	; Blacklist specific valuable trophies by Model ID
	Switch $lModelID
		Case 27033  ; Destroyer Core
			Return False
		Case 27036  ; Amphibian Tongue
			Return False
		Case 27052  ; Superb Charr Carving
			Return False
		Case 27067  ; Blob of Ooze
			Return False
		Case 27071  ; Vaettir Essence
			Return False
	EndSwitch
	
	If $lRarity = $__RARITY_GREEN Then Return False
	
	Switch $lType
		Case $__TYPE_UPGRADE, $__TYPE_USABLE, $__TYPE_KIT, $__TYPE_DYE, $__TYPE_KEY
			Return False
		Case $__TYPE_MATERIAL
			Switch $lModelID
                Case 925 to 928, 940 to 943, 946, 953, 954
                    Return True
                Case 921 To 956, 6532, 6533
                    Return False
			EndSwitch
		Case $__TYPE_ARMOR_SALVAGE
			Return True
		Case $__TYPE_SHIELD
			If $lReq = 9 And $lDmg = 16 And IsRareSkinSafe($lModelID) Then Return False	; Req9 Shields
			If $lReq = 8 And $lDmg = 16 Then Return False	; Req8 Shields
			If $lReq = 7 And $lDmg = 15 Then Return False	; Req7 Shields
			If $lReq = 6 And $lDmg = 14 Then Return False	; Req6 Shields
			If $lReq = 5 And $lDmg = 13 Then Return False	; Req5 Shields
			If $lReq = 4 And $lDmg = 12 Then Return False	; Req4 Shields
		Case $__TYPE_OFFHAND
			If $lReq = 9 And $lDmg = 12 And IsRareSkinSafe($lModelID) Then Return False
			If $lReq = 8 And $lDmg = 12 Then Return False 	; Req8 Offhand
		Case $__TYPE_SWORD
			If $lReq = 9 And $lDmg = 22 And IsRareSkinSafe($lModelID) Then Return False
			If $lReq = 8 And $lDmg = 22 Then Return False	; Req8 Sword
	EndSwitch

	If IsRareSkinSafe($lModelID) Then
		Return False
	EndIf

	Return True
 EndFunc

Func HasUsefulMod($aItem)
	; Note: Removed INI check for simplicity, using extracted array
	Local $array_weaponmods_ini = $array_weaponmods

	Local $aModStruct = GetModStruct($aItem)
	Local $atype = MemRead(GetItemPtr($aItem) + 32, "byte")

	Switch $atype
		Case 0
			; Out("Type Salvage")
			For $i = 0 To UBound($array_armormods) - 1
				If $array_armormods[$i][4] = 1 And StringInStr($aModStruct, $array_armormods[$i][3]) > 0 Then
					Out("Item has useful mod " & $array_armormods[$i][0])
					SetExtended($array_armormods[$i][2])
					Return True
				EndIf
			Next

		Case 2, 5, 12, 15, 22, 24, 26, 27, 32, 35, 36
			; Type Weapon
			For $j = 0 To 10
				For $i = 0 To UBound($array_weaponmods) - 1
					If $atype = $TYPE_ID[$j] And StringInStr($aModStruct, $array_weaponmods_ini[$i][2]) > 0 And $array_weaponmods_ini[$i][$j + 4] = 1 Then ; if type is weapon type and modstruct corresponds and the relevant weapon type is marked as a 1
						Out("Item has useful mod " & $array_weaponmods[$i][0] & " with index " & $array_weaponmods_ini[$i][1])
						SetExtended($array_weaponmods_ini[$i][1])
						Return True
					EndIf
				Next
			Next
	EndSwitch

	Return False

EndFunc

Func GetIsIDed($aItem)
	Return BitAND(MemRead(GetItemPtr($aItem) + 40, 'long'), 1) > 0
EndFunc   ;==>GetIsIDed

;~ Missing Helpers from GWA Logic

Func GetItemPtr($aItem)
	If IsDllStruct($aItem) Then Return DllStructGetPtr($aItem)
	If IsPtr($aItem) Then Return $aItem
	If IsInt($aItem) Then Return $aItem
	Return 0
EndFunc

Func GetQuantity($aItem)
	Return MemRead(GetItemPtr($aItem) + 76, 'short')
EndFunc

Func GetItemValue($aItem)
	Return MemRead(GetItemPtr($aItem) + 36, "short")
EndFunc

Func IsRareSkinSafe($modelID)
    If $modelID >= 0 And $modelID < UBound($aRareSkin) Then
        Return $aRareSkin[$modelID] <> ""
    EndIf
    Return False
EndFunc


