#include-once
; =============================================================================
; BotCore-SkillRules.au3
;
; Skill classification functions extracted from Froggy_HM_v1.6.au3.
; All dependencies resolve through Froggy_Includes.au3 master include.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================
;
; Referenced globals / constants (resolved via Skill_IDs.au3, Skill_Types.au3):
;   Skill type constants: $Type_Attack, $Enchantment, $Shout, $EchoRefrain,
;       $Chant, $Hex, $Condition, $Disguise, $Ward
;   Skill ID constants: too many to list individually -- all $SkillName style
;       constants from Skill_IDs.au3
;
; Other dependencies: GetSkillByID(), GetSkillPtr(), MemRead(), ID(),
;       DllStructGetData(), IsDllStruct(), IsPtr()
; =============================================================================


; #############################################################################
;  CORE CLASSIFICATION
;  Generic type checks and range queries used by other classifiers.
; #############################################################################

; ---------------------------------------------------------------------------
; IsSkillType  --  checks if a skill matches a given type constant
; ---------------------------------------------------------------------------
Func IsSkillType($aSkill, $aType)
	Return DllStructGetData(GetSkillByID($aSkill), 'Type') = $aType
EndFunc   ;==>IsSkillType

; ---------------------------------------------------------------------------
; IsWeaponRange  --  returns True if skill has non-zero range
; ---------------------------------------------------------------------------
Func IsWeaponRange($aSkill)
	Local $lRange = MemRead(GetSkillPtr($aSkill) + 96, "long") ; Range offset
	Return $lRange > 0
EndFunc   ;==>IsWeaponRange

; ---------------------------------------------------------------------------
; IsWeaponSpell  --  attack-type skill with weapon range
; ---------------------------------------------------------------------------
Func IsWeaponSpell($aSkill)
	Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
	If Not IsDllStruct($skillStruct) Then Return False
	Return IsSkillType($aSkill, $Type_Attack) And IsWeaponRange($aSkill)
EndFunc   ;==>IsWeaponSpell

; ---------------------------------------------------------------------------
; IsEnchantmentSkill  --  checks Enchantment type
; ---------------------------------------------------------------------------
Func IsEnchantmentSkill($aSkill)
	Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
	If Not IsDllStruct($skillStruct) Then Return False
	Return DllStructGetData($skillStruct, "Type") = $Enchantment
EndFunc   ;==>IsEnchantmentSkill

; ---------------------------------------------------------------------------
; IsAttackSkill  --  checks Attack type
; ---------------------------------------------------------------------------
Func IsAttackSkill($aSkill)
	Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
	If Not IsDllStruct($skillStruct) Then Return False
	Return DllStructGetData($skillStruct, "Type") = $Type_Attack
EndFunc   ;==>IsAttackSkill

; ---------------------------------------------------------------------------
; IsShoutSkill  --  checks Shout type
; ---------------------------------------------------------------------------
Func IsShoutSkill($aSkill)
	Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
	If Not IsDllStruct($skillStruct) Then Return False
	Return DllStructGetData($skillStruct, "Type") = $Shout
EndFunc   ;==>IsShoutSkill

; ---------------------------------------------------------------------------
; IsEchoRefrainskill  --  checks Echo/Refrain type
; ---------------------------------------------------------------------------
Func IsEchoRefrainskill($aSkill)
	Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
	If Not IsDllStruct($skillStruct) Then Return False
	Return DllStructGetData($skillStruct, "Type") = $EchoRefrain
EndFunc   ;==>IsEchoRefrainskill

; ---------------------------------------------------------------------------
; IsChantSkill  --  checks Chant type
; ---------------------------------------------------------------------------
Func IsChantSkill($aSkill)
	Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
	If Not IsDllStruct($skillStruct) Then Return False
	Return DllStructGetData($skillStruct, "Type") = $Chant
EndFunc   ;==>IsChantSkill


; #############################################################################
;  OFFENSIVE
;  Hexes, conditions, pressure, interrupts, and offensive spirit skills.
; #############################################################################

; ---------------------------------------------------------------------------
; IsHexSpell  --  checks skill type = $Hex (supports ptr, struct, or ID)
; ---------------------------------------------------------------------------
Func IsHexSpell($aSkill)
	If IsPtr($aSkill) <> 0 Then
		Return MemRead($aSkill + 12, "long") = $Hex
	ElseIf IsDllStruct($aSkill) <> 0 Then
		Return DllStructGetData($aSkill, "Type") = $Hex
	Else
		Return MemRead(GetSkillPtr($aSkill) + 12, "long") = $Hex
	EndIf
EndFunc   ;==>IsHexSpell

; ---------------------------------------------------------------------------
; IsConditionSpell  --  checks skill type = $Condition (supports ptr, struct, or ID)
; ---------------------------------------------------------------------------
Func IsConditionSpell($aSkill)
	If IsPtr($aSkill) <> 0 Then
		Return MemRead($aSkill + 12, "long") = $Condition
	ElseIf IsDllStruct($aSkill) <> 0 Then
		Return DllStructGetData($aSkill, "Type") = $Condition
	Else
		Return MemRead(GetSkillPtr($aSkill) + 12, "long") = $Condition
	EndIf
EndFunc   ;==>IsConditionSpell

; ---------------------------------------------------------------------------
; IsPressureSkill  --  conditions, hexes, and specific offensive skills
; ---------------------------------------------------------------------------
Func IsPressureSkill($aSkill)
	If IsConditionSpell($aSkill) Then Return True
	If IsHexSpell($aSkill) Then Return True
	Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Ebon_Vanguard_Assassin_Support, $Ebon_Battle_Standard_Of_Honor, $Finish_Him, $Dark_Pact
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsPressureSkill

; ---------------------------------------------------------------------------
; IsPressureSpiritSkill  --  offensive spirit skills
; ---------------------------------------------------------------------------
Func IsPressureSpiritSkill($aSkillID)
	Switch $aSkillID
		Case $Agony, $Agony_PvP, $Anguish, $Anguish_PvP, $Bloodsong, $Bloodsong_PvP, $Destruction, $Destruction_PvP
		Case $Disenchantment, $Disenchantment_PvP, $Dissonance, $Dissonance_PvP, $Gaze_of_Fury, $Gaze_of_Fury_PvP
		Case $Pain, $Pain_PvP, $Wanderlust, $Wanderlust_PvP, $Earthbind, $Earthbind_PvP, $Shadowsong, $Shadowsong_PvP
		Case $Signet_Of_Spirits, $Signet_of_Spirits_PvP, $Vampirism
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsPressureSpiritSkill

; ---------------------------------------------------------------------------
; IsRuptSkill  --  interrupt skills (hard + soft rupts)
; ---------------------------------------------------------------------------
Func IsRuptSkill($aSkill, $hardrupt = False)
	Local $lSkillEffect2 = MemRead(GetSkillPtr($aSkill) + 32, 'long')
	Local $lSkillID = ID($aSkill)
	If BitAND($lSkillEffect2, 1) Then Return True

	Switch $lSkillID	; interrupt skills that are incorrectly categorized via Effect2 or soft rupts for separation
		Case $Mistrust, $Mistrust_PvP, $Guilt, $Power_Drain, $Power_Flux, $Power_Leak, $Power_Leech, $Power_Lock, $Power_Return, $Power_Spike, $Shame    ; soft rupts
			If Not $hardrupt Then Return True
		Case $You_Move_Like_a_Dwarf, $Disarm, $Disrupting_Shot, $Disrupting_Throw, $Distracting_Lunge, $Distracting_Strike, $Magebane_Shot, $Concussion_Shot
			Return True
		Case $Cry_of_Pain, $Overload, $Psychic_Instability, $Psychic_Instability_PvP, $Signet_of_Clumsiness, $Simple_Thievery, $Tease    ; overoad/SoC lumped here for convenience
			Return True
		Case $Exhausting_Assault, $Temple_Strike, $Lyssas_Assault, $Lyssas_Haste, $Thunderclap
			Return True
		Case Else
			Return False
	EndSwitch
EndFunc   ;==>IsRuptSkill

; ---------------------------------------------------------------------------
; IsHardRuptSkill  --  wrapper for IsRuptSkill with $hardrupt = True
; ---------------------------------------------------------------------------
Func IsHardRuptSkill($aSkillID)
	Return IsRuptSkill($aSkillID, True)
EndFunc   ;==>IsHardRuptSkill


; #############################################################################
;  DEFENSIVE
;  Healing, bonds, survival, and targeting helpers.
; #############################################################################

; ---------------------------------------------------------------------------
; IsHealSkill  --  checks Effect2 flags + explicit skill ID list
; ---------------------------------------------------------------------------
Func IsHealSkill($aSkill)
	Switch MemRead(GetSkillPtr($aSkill) + 32, 'long')	; Effect 2
		Case 2, 4, 6, 36, 38, 4102, 4096, 6144, 8196, 8198, 14336
			Return True
		Case Else
			Switch ID($aSkill)	;
				Case $Healing_Hands, $Restful_Breeze, $Signet_Of_Rejuvenation, $Words_of_Comfort, $Conviction, $Faithful_Intervention _
					,$Mystic_Healing, $Mystic_Healing_PvP, $Mystic_Regeneration, $Mystic_Vigor, $Pious_Renewal, $Watchful_Intervention _
					,$Spirit_Transfer, $I_Will_Avenge_You, $I_Will_Survive, $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick _
					,$Shroud_of_Distress, $Healing_Spring, $Hexers_Vigor, $Feel_No_Pain
				Return True
			EndSwitch
	EndSwitch
	If IsPartyHealSkill($aSkill) Then Return True
	Return False
EndFunc   ;==>IsHealSkill

; ---------------------------------------------------------------------------
; IsPartyHealSkill  --  incomplete list, add based on your needs
; ---------------------------------------------------------------------------
Func IsPartyHealSkill($aSkill)
	Switch $aSkill
		Case $Divine_Healing, $Heal_Party, $Heavens_Delight, $Protective_Was_Kaolai, $Mystic_Healing, $Mystic_Healing_PvP
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsPartyHealSkill

; ---------------------------------------------------------------------------
; IsSelfPrehealSkill  --  instant, non-conditional, self-targeted heals only
; ---------------------------------------------------------------------------
Func IsSelfPrehealSkill($aSkill)
	Switch $aSkill
		Case $Healing_Breeze, $Healing_Hands, $Mending, $Patient_Spirit, $Restful_Breeze, $Spirit_Bond, $Vigorous_Spirit ; Monk Skills
		Case $Conviction, $Faithful_Intervention, $Mystic_Regeneration, $Mystic_Vigor, $Pious_Renewal, $Vital_Boon, $Watchful_Intervention ; Dervish Skills
		Case $Feigned_Neutrality, $Shadow_Refuge, $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick, $Shroud_of_Distress ; Assassin Skills
		Case $Healing_Spring, $Troll_Unguent ; Ranger Skills
		Case $Blood_Renewal, $Hexers_Vigor ; Necro Skills
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsSelfPrehealSkill

; ---------------------------------------------------------------------------
; IsSelfHealSkill  --  healing skills that can be used on self
; ---------------------------------------------------------------------------
Func IsSelfHealSkill($aSkill)
	Return IsPartyHealSkill($aSkill) And MemRead(GetSkillPtr($aSkill) + 49, "byte") <> 4
EndFunc   ;==>IsSelfHealSkill

; ---------------------------------------------------------------------------
; IsSelfOnlyHealSkill  --  healing skills usable only on self
; ---------------------------------------------------------------------------
Func IsSelfOnlyHealSkill($aSkill)
	Return IsPartyHealSkill($aSkill) And MemRead(GetSkillPtr($aSkill) + 49, "byte") = 0
EndFunc   ;==>IsSelfOnlyHealSkill

; ---------------------------------------------------------------------------
; IsHealOtherSkill  --  healing skills that can only be used on others
; ---------------------------------------------------------------------------
Func IsHealOtherSkill($aSkill)
	Return IsPartyHealSkill($aSkill) And MemRead(GetSkillPtr($aSkill) + 49, "byte") = 4
EndFunc   ;==>IsHealOtherSkill

; ---------------------------------------------------------------------------
; IsHealMySelfSkill  --  checks bit flag for self-heal targeting
; ---------------------------------------------------------------------------
Func IsHealMySelfSkill($aSkill)
	Local $lSkillType = MemRead(GetSkillPtr($aSkill) + 12, "long")
	Return BitAND($lSkillType, 4)
EndFunc   ;==>IsHealMySelfSkill

; ---------------------------------------------------------------------------
; IsHealAllySkill  --  checks bit flag for ally-heal targeting
; ---------------------------------------------------------------------------
Func IsHealAllySkill($aSkill)
	Local $lSkillType = MemRead(GetSkillPtr($aSkill) + 12, "long")
	Return BitAND($lSkillType, 2)
EndFunc   ;==>IsHealAllySkill

; ---------------------------------------------------------------------------
; IsBondSkill  --  monk bond enchantments
; ---------------------------------------------------------------------------
Func IsBondSkill($aSkill)
	Switch $aSkill
		Case $Balthazars_Spirit, $Essence_Bond, $Life_Barrier, $Life_Bond, $Mending, $Protective_Bond, $Purifying_Veil, $Retribution, $Strength_of_Honor, $Succor
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsBondSkill

; ---------------------------------------------------------------------------
; IsSurvivalSkill  --  defensive / survivability skills
; ---------------------------------------------------------------------------
Func IsSurvivalSkill($aSkill)
	Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $I_Am_Unstoppable, $Shadow_Form, $Shroud_of_Distress, $Glyph_of_Swiftness, $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick _
				, $Heart_of_Shadow, $Way_Of_The_Master, $Shadow_Refuge, $Protective_Spirit, $Shield_of_Absorption, $Shielding_Hands _
				, $Mystic_Regeneration, $Shield_of_Judgment, $Spirit_Bond, $Zealots_Fire
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsSurvivalSkill


; #############################################################################
;  SUPPORT
;  Speed boosts, precasts, bindings, and disguises.
; #############################################################################

; ---------------------------------------------------------------------------
; IsSpeedBoost  --  movement speed increase skills
; ---------------------------------------------------------------------------
Func IsSpeedBoost($aSkill)
	Switch $aSkill
		Case $Dwarven_Stability    ; used to potentiate running, to be used before stances
			Return True
		Case $Illusion_of_Haste, $Windborne_Speed, $Armor_of_Mist, $Storm_Djinns_Haste, $Rush, $Sprint, $Charge, $Bulls_Charge, $Dodge, $Escape, $Storm_Chaser, $Run_as_One
		Case $Burning_Speed, $Retreat, $Gust, $Shadow_of_Haste, $Torch_Hex, $Torch_Degeneration_Hex, $Dark_Escape, $Dash, $Zojuns_Haste, $Flame_Djinns_Haste
		Case $Enraging_Charge, $Lyssas_Haste, $Avatar_of_Balthazar, $Enchanted_Haste, $Pious_Haste, $Whirling_Charge, $Godspeed, $Make_Haste, $Fall_Back, $Incoming, $Onslaught
		Case $Featherfoot_Grace, $Harriers_Haste, $Hasty_Refrain, $Soldiers_Speed, $Drunken_Master, $Ursan_Roar, $Volfen_Pounce, $Escape_PvP, $Charging_Strike
		Case $Battle_Rage, $Natural_Stride, $Storms_Embrace, $Junundu_Tunnel, $Flee, $HYAHHHHH, $Ursan_Force, $Incoming_PvP, $Its_Just_a_Flesh_Wound, $Fall_Back_PvP
		Case $Call_of_Haste, $Call_of_Haste_PvP, $Lead_the_Way, $Fleeting_Stability, $Illusion_of_Haste_PvP, $Mindbender, $Rampage_as_One
			Return True
		Case $Heroic_Refrain, $To_the_Limit	; ensures Heroic refrain is being ramped up / maintained outside of aggro
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsSpeedBoost

; ---------------------------------------------------------------------------
; IsPrecastSkill  --  skills to use before combat (spirits, bonds, wards, etc.)
; ---------------------------------------------------------------------------
Func IsPrecastSkill($aSkill)
	If IsPressureSpiritSkill($aSkill) Then Return True
	Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Balthazars_Spirit, $Blessed_Aura, $Boon_Of_Creation
			Return True
	EndSwitch
	If IsSelfPrehealSkill($aSkill) Then Return True
	If IsDisguiseskill($aSkill) Then Return True
	If DllStructGetData(GetSkillByID($skillID), "Type") = $Ward Then Return True
	Return False
EndFunc   ;==>IsPrecastSkill

; ---------------------------------------------------------------------------
; IsBindingSkill  --  binding rituals and related spirit skills
; ---------------------------------------------------------------------------
Func IsBindingSkill($aSkillID)
	Switch $aSkillID
		Case $Ebon_Battle_Standard_of_Honor
			Return True    ; not binding ritual but grouped together for convenience purposes
		Case $Agony, $Agony_PvP, $Anguish, $Anguish_PvP
		Case $Bloodsong, $Bloodsong_PvP, $Call_to_the_Spirit_Realm, $Destruction, $Destruction_PvP
		Case $Disenchantment, $Disenchantment_PvP, $Disenchantment_Togo, $Dissonance, $Dissonance_PvP
		Case $Gaze_of_Fury, $Gaze_of_Fury_PvP, $Jack_Frost, $Life, $Pain, $Pain_PvP, $Wanderlust, $Wanderlust_PvP
		Case $Displacement, $Displacement_PvP, $Earthbind, $Earthbind_PvP, $Empowerment, $Empowerment_PvP
		Case $Preservation, $Preservation_PvP, $Recovery, $Recovery_PvP, $Recuperation, $Recuperation_PvP
		Case $Rejuvenation, $Rejuvenation_PvP, $Shadowsong, $Shadowsong_PvP, $Shelter, $Shelter_PvP
		Case $Signet_of_Creation, $Signet_of_Creation_PvP, $Signet_Of_Spirits, $Signet_of_Spirits_PvP
		Case $Soothing, $Soothing_PvP, $Union, $Union_PvP, $Vampirism
			Return True
		Case $Summon_Spirits_Luxon, $Summon_Spirits_Kurzick ; used in binding builds
			Return True
		Case $Ritual_Lord, $Ritual_Lord_PvP ; used in binding builds
			Return True
		Case $Soul_Twisting ; used in binding builds
			Return True
		Case $Armor_of_Unfeeling, $Armor_of_Unfeeling_PvP, $Signet_of_Ghostly_Might, $Signet_of_Ghostly_Might_PvP ; used in binding builds
			Return True
		Case $Spiritleech_Aura ; used in binding builds
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsBindingSkill

; ---------------------------------------------------------------------------
; IsDisguiseskill  --  checks skill type = $Disguise (supports ptr, struct, or ID)
; ---------------------------------------------------------------------------
Func IsDisguiseskill($aSkill)
	If IsPtr($aSkill) <> 0 Then
		Return MemRead($aSkill + 12, "long") = $Disguise
	ElseIf IsDllStruct($aSkill) <> 0 Then
		Return DllStructGetData($aSkill, "Type") = $Disguise
	Else
		Return MemRead(GetSkillPtr($aSkill) + 12, "long") = $Disguise
	EndIf
EndFunc   ;==>IsDisguiseskill


; #############################################################################
;  REMOVAL
;  Condition, hex, and enchantment removal skills.
; #############################################################################

; ---------------------------------------------------------------------------
; IsCondRemoveSkill  --  condition removal skills
; ---------------------------------------------------------------------------
Func IsCondRemoveSkill($aSkill)
	Switch $aSkill
		Case $mend_ailment, $purge_conditions, $mending_touch, $Mend_Body_and_Soul, $Spotless_Soul
			Return True
	EndSwitch
	If IsHexAndConditionRemoveSkill($aSkill) Then Return True
	Return False
EndFunc   ;==>IsCondRemoveSkill

; ---------------------------------------------------------------------------
; IsHexRemoveSkill  --  hex removal skills
; ---------------------------------------------------------------------------
Func IsHexRemoveSkill($aSkill)
	Switch $aSkill
		Case $Smite_Hex, $Divert_Hexes, $Cure_Hex, $Hex_Eater_Vortex, $Shatter_Hex, $Reverse_Hex, $Remove_Hex, $Expel_Hexes, $Inspired_Hex, $Revealed_Hex, $Spotless_Mind, $Convert_Hexes, $Deny_Hexes; remove hex (me and ally)
		Case $Hex_Eater_Signet, $Withdraw_Hexes, $Hexbreaker_Aria, $holy_veil, $Pious_Restoration
			Return True
	EndSwitch
	If IsHexAndConditionRemoveSkill($aSkill) Then Return True
	Return False
EndFunc   ;==>IsHexRemoveSkill

; ---------------------------------------------------------------------------
; IsHexAndConditionRemoveSkill  --  skills that remove both hexes and conditions
; ---------------------------------------------------------------------------
Func IsHexAndConditionRemoveSkill($aSkillID)
	Switch $aSkillID
		Case $Peace_and_Harmony, $Empathic_Removal, $Blessed_Light, $Contemplation_of_Purity, $Purge_Signet, $Signet_of_Removal
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsHexAndConditionRemoveSkill

; ---------------------------------------------------------------------------
; IsEnchantRemoveSkill  --  enchantment removal (stub, returns False)
; ---------------------------------------------------------------------------
Func IsEnchantRemoveSkill($aSkill)
	Return False
EndFunc   ;==>IsEnchantRemoveSkill

; ---------------------------------------------------------------------------
; IsConditionRemovalSkill  --  alias for IsCondRemoveSkill
; ---------------------------------------------------------------------------
Func IsConditionRemovalSkill($aSkill)
	Return IsCondRemoveSkill($aSkill)
EndFunc   ;==>IsConditionRemovalSkill

; ---------------------------------------------------------------------------
; IsHexRemovalSkill  --  alias for IsHexRemoveSkill
; ---------------------------------------------------------------------------
Func IsHexRemovalSkill($aSkill)
	Return IsHexRemoveSkill($aSkill)
EndFunc   ;==>IsHexRemovalSkill

; ---------------------------------------------------------------------------
; IsConditionAndHexRemovalSkill  --  alias for IsHexAndConditionRemoveSkill
; ---------------------------------------------------------------------------
Func IsConditionAndHexRemovalSkill($aSkill)
	Return IsHexAndConditionRemoveSkill($aSkill)
EndFunc   ;==>IsConditionAndHexRemovalSkill
