#include-once
#include '..\GWA2.au3'

; ==================================================================================================
; GWA2_Extensions.au3
;
; Custom extension functions extracted from GWA2.au3 (COA1 additions).
; These are helper/wrapper functions added on top of the base GWA2 library
; for convenience: dead checks, skill usage with timing, merchant helpers,
; attribute clearing, disconnect recovery, inventory utilities, and
; placeholder stubs for extensibility hooks.
;
; Extracted 2026-03-25
; ==================================================================================================


; ==================================================================================================
; Player / Hero Status
; ==================================================================================================

;~ Check if player is dead
Func IsPlayerDead()
	Return BitAND(DllStructGetData(GetMyAgent(), 'Effects'), 0x0010) > 0
EndFunc

;~ Check if hero is dead
Func IsHeroDead($heroID)
	; Note: Input is HeroID, but Utils implementation took HeroIndex.
    ; GWA2 usage usually passes ID. Let's check Utils implementation again.
    ; Utils: GetAgentById(GetHeroID($heroIndex)) -> implies input was Index (1-7)
    ; GWA2 Callers:
    ;   CommandHeroUseSkill($heroNumber, ...) -> passes $heroNumber
    ; My placeholder: IsHeroDead($heroID)

    ; If the usage in GWA2 is `IsHeroDead($heroNumber)`, then I should use the Utils implementation logic.
    ; Let's assume input is Index/Number for now based on Utils.
	Return BitAND(DllStructGetData(GetAgentById(GetHeroID($heroID)), 'Effects'), 0x0010) > 0
EndFunc


; ==================================================================================================
; Cast-Time Modifier
; ==================================================================================================

Func GetCastTimeModifier($effects, $usedSkill)
	Local $skillID = DllStructGetData($usedSkill, 'ID')
	Local $effectID = 0
	Local $castTime = 1
	For $effect in $effects
		$effectID = DllStructGetData($effect, 'EffectId')
		Switch $effectID
			; consumables effects
			Case $ID_ESSENCE_OF_CELERITY_EFFECT
				$castTime = 0.80 * $castTime
			Case $ID_PIE_INDUCED_ECSTASY
				$castTime = 0.85 * $castTime
			Case $ID_RED_ROCK_CANDY_RUSH
				$castTime = 0.75 * $castTime
			Case $ID_BLUE_ROCK_CANDY_RUSH
				$castTime = 0.80 * $castTime
			Case $ID_GREEN_ROCK_CANDY_RUSH
				$castTime = 0.85 * $castTime
			; skills shortening cast time
			Case $ID_DEADLY_PARADOX
				If $skillID == $ID_SHADOW_FORM Then $castTime = 0.667 * $castTime
			Case $ID_GLYPH_OF_SACRIFICE, $ID_GLYPH_OF_ESSENCE, $ID_SIGNET_OF_MYSTIC_SPEED
				$castTime = 0
			Case $ID_MINDBENDER
				$castTime = 0.80 * $castTime
			Case $ID_TIME_WARD, $ID_OVER_THE_LIMIT
				Local $attributeLevel = DllStructGetData($effect, 'AttributeLevel')
				; Below equation converts attribute level of Time Ward or Over the Limit effect into shorter cast time, e.g. 80% for attribute levels 14,15,16
				Local $castTimeReduction = 1 - ((15 + Floor(($attributeLevel + 1) / 3)) / 100)
				$castTime = $castTimeReduction * $castTime
			; hexes lengthening cast time
			Case $ID_ARCANE_CONUNDRUM, $ID_MIGRAINE, $ID_STOLEN_SPEED, $ID_SHARED_BURDEN, $ID_FRUSTRATION, $ID_CONFUSING_IMAGES
				$castTime = 2 * $castTime
			Case $ID_SUM_OF_ALL_FEARS
				$castTime = 1.5 * $castTime
			; other effects
			Case $ID_DAZED
				$castTime = 2 * $castTime
		EndSwitch
	Next
	Return $castTime
EndFunc


; ==================================================================================================
; Skill Usage (Player)
; ==================================================================================================

;~ Use a skill and wait for it to be done, but skipping calculation of precise cast time, without effects modifiers for optimization
;~ If no target is provided then skill is used on self
;~ Returns True if skill usage was successful, False otherwise
Func UseSkillEx($skillSlot, $target = Null)
	If IsPlayerDead() Or Not IsRecharged($skillSlot) Then Return False

	Local $skill = GetSkillByID(GetSkillbarSkillID($skillSlot))
	Local $energy = StringReplace(StringReplace(StringReplace(StringMid(DllStructGetData($skill, 'Unknown4'), 6, 1), 'C', '25'), 'B', '15'), 'A', '10')
	If GetEnergy() < $energy Then Return False
	Local $castTime = DllStructGetData($skill, 'Activation') * 1000
	Local $aftercast = DllStructGetData($skill, 'Aftercast') * 1000
	; Random delay make us wait at least 2 loops before checking for recharge, to avoid issues with very low cast times
	Local $approximateCastTime = $castTime + $aftercast + Random(75, 125)
	UseSkill($skillSlot, $target)
	Local $castTimer = TimerInit()
	; wait until skill starts recharging or time for skill to be activated has elapsed
	Do
		Sleep(50)
	Until Not IsRecharged($skillSlot) Or ($approximateCastTime > 0 And TimerDiff($castTimer) > $approximateCastTime)
	Return True
EndFunc


;~ Use a skill and wait for it to be done, with calculation of all effects modifiers to wait exact cast time
;~ If no target is provided then skill is used on self
;~ Returns True if skill usage was successful, False otherwise
Func UseSkillTimed($skillSlot, $target = Null)
	If IsPlayerDead() Or Not IsRecharged($skillSlot) Then Return False

	Local $skill = GetSkillByID(GetSkillbarSkillID($skillSlot))
	Local $energy = StringReplace(StringReplace(StringReplace(StringMid(DllStructGetData($skill, 'Unknown4'), 6, 1), 'C', '25'), 'B', '15'), 'A', '10')
	If GetEnergy() < $energy Then Return False
	Local $castTime = DllStructGetData($skill, 'Activation') * 1000
	Local $aftercast = DllStructGetData($skill, 'Aftercast') * 1000
	; taking into account skill activation time modifiers
	Local $effects = GetEffect(0)
	; get cast time modifier, default is 1, but effects can influence it
	Local $castTimeModifier = GetCastTimeModifier($effects, $skill)
	Local $fullCastTime = $castTimeModifier * $castTime + $aftercast + GetPing()

	; when player casts a skill on target that is beyond cast range then trying to get close to target first to not count time on the run
	If $target <> Null And GetDistance(GetMyAgent(), $target) > ($RANGE_SPELLCAST + 100) Then _GWA2_GetAlmostInRangeOfAgent($target)
	UseSkill($skillSlot, $target)
	Local $castTimer = TimerInit()
	; wait until skill starts recharging or time for skill to be fully activated has elapsed
	Do
		Sleep(50 + GetPing())
	Until (Not IsRecharged($skillSlot)) Or ($fullCastTime < TimerDiff($castTimer))
	Return True
EndFunc


; ==================================================================================================
; Skill Usage (Hero)
; ==================================================================================================

;~ Order a hero to use a skill and wait for it to be done, but skipping calculation of precise cast time, without effects modifiers for optimization
;~ If no target is provided then skill is used on hero who uses the skill
;~ Returns True if skill usage was successful, False otherwise
Func UseHeroSkillEx($heroIndex, $skillSlot, $target = Null)
	If IsHeroDead($heroIndex) Or Not IsRecharged($skillSlot, $heroIndex) Then Return False

	Local $skill = GetSkillByID(GetSkillbarSkillID($skillSlot, $heroIndex))
	Local $energy = StringReplace(StringReplace(StringReplace(StringMid(DllStructGetData($skill, 'Unknown4'), 6, 1), 'C', '25'), 'B', '15'), 'A', '10')
	If GetEnergy(GetAgentById(GetHeroID($heroIndex))) < $energy Then Return False
	Local $castTime = DllStructGetData($skill, 'Activation') * 1000
	Local $aftercast = DllStructGetData($skill, 'Aftercast') * 1000
	Local $approximateCastTime = $castTime + $aftercast + GetPing()

	UseHeroSkill($heroIndex, $skillSlot, $target)
	Local $castTimer = TimerInit()
	; Wait until skill starts recharging or time for skill to be activated has elapsed
	Do
		Sleep(50 + GetPing())
	Until (Not IsRecharged($skillSlot)) Or ($approximateCastTime < TimerDiff($castTimer))
	Return True
EndFunc


;~ Order a hero to use a skill and wait for it to be done, with calculation of all effects modifiers to wait exact cast time
;~ If no target is provided then skill is used on hero who uses the skill
;~ Returns True if skill usage was successful, False otherwise
Func UseHeroSkillTimed($heroIndex, $skillSlot, $target = Null)
	If IsHeroDead($heroIndex) Or Not IsRecharged($skillSlot, $heroIndex) Then Return False

	Local $skill = GetSkillByID(GetSkillbarSkillID($skillSlot, $heroIndex))
	Local $energy = StringReplace(StringReplace(StringReplace(StringMid(DllStructGetData($skill, 'Unknown4'), 6, 1), 'C', '25'), 'B', '15'), 'A', '10')
	If GetEnergy(GetAgentById(GetHeroID($heroIndex))) < $energy Then Return False
	Local $castTime = DllStructGetData($skill, 'Activation') * 1000
	Local $aftercast = DllStructGetData($skill, 'Aftercast') * 1000
	; taking into account skill activation time modifiers
	Local $effects = GetEffect(0, $heroIndex)
	; get cast time modifier, default is 1, but effects can influence it
	Local $castTimeModifier = GetCastTimeModifier($effects, $skill)
	Local $fullCastTime = $castTimeModifier * $castTime + $aftercast + GetPing()

	UseHeroSkill($heroIndex, $skillSlot, $target)
	Local $castTimer = TimerInit()
	; wait until skill starts recharging or time for skill to be fully activated has elapsed
	Do
		Sleep(50 + GetPing())
	Until (Not IsRecharged($skillSlot)) Or ($fullCastTime < TimerDiff($castTimer))
	Return True
EndFunc


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

;~ Get item from merchant corresponding to given modelID
Func GetMerchantItemPtrByModelId($modelID)
	Local $offsets[5] = [0, 0x18, 0x40, 0xB8]
	Local $merchantBaseAddress = GetMerchantItemsBase()
	Local $itemID = 0
	Local $itemPtr = 0
	For $i = 0 To GetMerchantItemsSize() -1
		$itemID = MemRead($merchantBaseAddress + 4 * $i)
		If ($itemID) Then
			$offsets[4] = 4 * $itemID
			$itemPtr = MemReadPtr($base_address_ptr, $offsets)[1]
			If (MemRead($itemPtr + 0x2C) = $modelID) Then
				Return Ptr($itemPtr)
			EndIf
		EndIf
	Next
EndFunc


; ==================================================================================================
; Attributes
; ==================================================================================================

;~ Set all attributes to 0
Func ClearAttributes($heroIndex = 0)
	Local $level
	If GetMapType() <> $ID_OUTPOST Then Return False
	For $i = 0 To UBound($ATTRIBUTES_ARRAY) - 1
		Local $attributeID = $ATTRIBUTES_ARRAY[$i]
		If GetAttributeByID($attributeID, False, $heroIndex) > 0 Then
			Do
				$level = GetAttributeByID($attributeID, False, $heroIndex)
				$deadlock = TimerInit()
				DecreaseAttribute($attributeID, $heroIndex)
				Do
					Sleep(20)
				Until $level > GetAttributeByID($attributeID, False, $heroIndex) Or TimerDiff($deadlock) > 5000
				Sleep(100)
			Until GetAttributeByID($attributeID, False, $heroIndex) == 0
		EndIf
	Next
	Return True
EndFunc


; ==================================================================================================
; Disconnect Recovery
; ==================================================================================================

Func Disconnected()
	Local $check = False
	Local $deadlock = TimerInit()
	Do
		Sleep(20)
		$check = GetMapType() <> $ID_Loading And GetAgentExists(GetMyID())
	Until $check Or TimerDiff($deadlock) > 5000
	If $check = False Then
		Error('Disconnected!')
		Error('Attempting to reconnect.')
		Local $windowHandle = GetWindowHandle()
		ControlSend($windowHandle, '', '', '{Enter}')
		$deadlock = TimerInit()
		Do
			Sleep(20)
			$check = GetMapType() <> $ID_Loading And GetAgentExists(GetMyID())
		Until $check Or TimerDiff($deadlock) > 60000
		If $check = False Then
			Error('Failed to Reconnect 1!')
			Error('Retrying.')
			ControlSend($windowHandle, '', '', '{Enter}')
			$deadlock = TimerInit()
			Do
				Sleep(20)
				$check = GetMapType() <> $ID_Loading And GetAgentExists(GetMyID())
			Until $check Or TimerDiff($deadlock) > 60000
			If $check = False Then
				Error('Failed to Reconnect 2!')
				Error('Retrying.')
				ControlSend($windowHandle, '', '', '{Enter}')
				$deadlock = TimerInit()
				Do
					Sleep(20)
					$check = GetMapType() <> $ID_Loading And GetAgentExists(GetMyID())
				Until $check Or TimerDiff($deadlock) > 60000
				If $check = False Then
					Error('Could not reconnect!')
					Error('Exiting.')
					EnableRendering()
					Exit 1
				EndIf
			EndIf
		EndIf
	EndIf
	Notice('Reconnected!')
	Sleep(5000)
EndFunc


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


; RegisterNameTo32Code, RegisterNameTo16Code, RegisterNameTo8Code
; will be added here when COA1 branch is merged into this branch.
