#include-once
; =============================================================================
; BotCore-Effects.au3
;
; Low-level helper functions extracted from Froggy_HM_v1.6.au3.
; Covers identity/coordinate helpers, health/effect read helpers,
; and skillbar pointer helpers.
;
; All dependencies resolve through the master include chain.
; No #include directives here.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; =============================================================================
; KF-008 - Identity and coordinate helpers
; =============================================================================

Func ID($aAgent = -2)
	; Handle special values first
	If $aAgent = -2 Then Return GetMyID()
	; Handle pointers (from GetAgentPtr)
	If IsPtr($aAgent) Then
		Return MemRead($aAgent + 44, 'long')
	; Handle DllStruct (from GetAgentByID)
	ElseIf IsDllStruct($aAgent) Then
		Return DllStructGetData($aAgent, 'ID')
	; Assume it's already an ID
	Else
		Return $aAgent
	EndIf
EndFunc

Func GetX($agent)
    If IsDllStruct($agent) Then Return DllStructGetData($agent, 'X')
    Return 0
EndFunc

Func GetY($agent)
    If IsDllStruct($agent) Then Return DllStructGetData($agent, 'Y')
    Return 0
EndFunc

Func X($agent)
    Return GetX($agent)
EndFunc

Func Y($agent)
    Return GetY($agent)
EndFunc

; =============================================================================
; KF-009 - Health/effect read helpers
; =============================================================================

; Ported Helper: GetHP (Percentage)
Func GetHP($aAgent = -2)
	Return MemRead(GetAgentPtr($aAgent) + 304, 'float')
EndFunc

Func GetHasEnchantment($aAgent)
	; Alias for GetIsEnchanted logic
	Return BitAND(MemRead(GetAgentPtr($aAgent) + 312, "long"), 0x0080) > 0
EndFunc

Func HasEffect($aEffectSkillID, $aHeroNumber = 0, $aHeroId = GetHeroID($aHeroNumber))
	Return GetSkillEffectPtr($aEffectSkillID, $aHeroNumber, $aHeroId) <> 0
EndFunc

Func GetSkillEffectPtr($aSkillID, $aHeroNumber = 0, $aHeroId = GetHeroID($aHeroNumber))
	Local $lOffset[4] = [0, 24, 44, 1296]
	Local $lCount = MemReadPtr($mBasePointer, $lOffset)
	ReDim $lOffset[5]
	$lOffset[3] = 1288
	Local $lBuffer
	For $i = 0 To $lCount[1] - 1
		$lOffset[4] = 36 * $i
		$lBuffer = MemReadPtr($mBasePointer, $lOffset)
		If $lBuffer[1] = $aHeroId Then
			$lOffset[4] = 28 + 36 * $i
			Local $lEffectCount = MemReadPtr($mBasePointer, $lOffset)
			$lOffset[4] = 20 + 36 * $i
			Local $lEffectStructAddress = MemReadPtr($mBasePointer, $lOffset, 'ptr')
			For $J = 0 To $lEffectCount[1] - 1
				Local $lEffectSkillID = MemRead($lEffectStructAddress[1] + 24 * $J, 'long')
				If $lEffectSkillID = $aSkillID Then Return Ptr($lEffectStructAddress[1] + 24 * $J)
			Next
		EndIf
	Next
	Return 0
EndFunc

Func GetEffectsPtr($aSkillID = 0, $aHeroNumber = 0, $aHeroId = GetHeroID($aHeroNumber))
	Local $lEffectCount, $lEffectStructAddress, $lBuffer
	Local $lOffset[4] = [0, 24, 44, 1296]
	Local $lCount = MemReadPtr($mBasePointer, $lOffset)
	ReDim $lOffset[5]
	$lOffset[3] = 1288
	For $i = 0 To $lCount[1] - 1
		$lOffset[4] = 36 * $i
		$lBuffer = MemReadPtr($mBasePointer, $lOffset)
		If $lBuffer[1] = $aHeroId Then
			$lOffset[4] = 28 + 36 * $i
			$lEffectCount = MemReadPtr($mBasePointer, $lOffset)
			$lOffset[4] = 20 + 36 * $i
			$lEffectStructAddress = MemReadPtr($mBasePointer, $lOffset, 'ptr')
			If $aSkillID = 0 Then Return $lEffectStructAddress[1]
			; Logic for specific ID omitted for basic Ptr return
		EndIf
	Next
	Return 0
EndFunc

; Renamed Helper for Logic Port to avoid conflict
; Note: GetEffect() expects hero index (0=player, 1-7=heroes), not agent ID
; For player effects, we use hero index 0
Func AgentHasEffect($aSkillID, $aAgentID = -2)
	; For simplicity, if checking player (-2), use hero index 0
	; This function currently only supports checking player effects
	Local $heroIndex = 0  ; Always check player for now
	Local $effect = GetEffect($aSkillID, $heroIndex)
	; GetEffect returns Null if effect not found, or DllStruct if found
	Return ($effect <> Null And Not IsArray($effect))
EndFunc

; =============================================================================
; KF-010 - Skillbar pointer helpers
; =============================================================================

; Ported Dependency for GetAdrenaline
Func GetSkillbarPtr($aHeroNumber = 0)
    Local $lOffset[5] = [0, 24, 76, 84, 44]
    Local $lHeroCount = MemReadPtr($mBasePointer, $lOffset)
    Local $lOffset2[5] = [0, 24, 44, 1776, 0] ; Re-dimensioned for 5 elements
    For $i = 0 To $lHeroCount[1]
        $lOffset2[4] = $i * 188
        Local $lSkillbarStructAddress = MemReadPtr($mBasePointer, $lOffset2)
        If $lSkillbarStructAddress[1] = GetHeroID($aHeroNumber) Then Return $lSkillbarStructAddress[0]
    Next
    Return 0
EndFunc

Func GetAdrenaline($aSkillSlot)
    Local $aSkillbarPtr = GetSkillbarPtr()
    If $aSkillbarPtr = 0 Then Return 0
    $aSkillSlot -= 1
    Return MemRead($aSkillbarPtr + 4 + $aSkillSlot * 20, "long")
EndFunc
