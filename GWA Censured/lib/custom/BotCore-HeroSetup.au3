#include-once
; =============================================================================
; BotCore-HeroSetup.au3
;
; Generic hero configuration loader. Reads hero ID + skill template pairs
; from text files in the hero_configs/ directory.
;
; Config file format (one hero per line):
;   HeroID, SkillTemplateCode
;   ; Comments start with semicolon
;
; Extracted from Froggy_HM_v1.6.au3 (KF-037)
; =============================================================================

; === Public Functions ===

;~ Load hero team from a config file
;~ @param $sConfigName - name of config file (without .txt extension)
;~ @return True on success, False on failure
Func LoadHeroConfigFromFile($sConfigName)
	Local $sFilePath = @ScriptDir & "\hero_configs\" & $sConfigName & ".txt"

	If Not FileExists($sFilePath) Then
		Out("ERROR: Hero config file not found: " & $sFilePath)
		Return False
	EndIf

	Out("Loading hero config: " & $sConfigName)

	; First pass: add all heroes
	Local $hFile = FileOpen($sFilePath, 0)
	If $hFile = -1 Then
		Out("ERROR: Could not open config file")
		Return False
	EndIf

	Local $iHeroSlot = 1
	While 1
		Local $sLine = FileReadLine($hFile)
		If @error Then ExitLoop

		$sLine = StringStripWS($sLine, 3)
		If $sLine = "" Or StringLeft($sLine, 1) = ";" Then ContinueLoop

		Local $iCommentPos = StringInStr($sLine, ";")
		If $iCommentPos > 0 Then
			$sLine = StringStripWS(StringLeft($sLine, $iCommentPos - 1), 2)
		EndIf

		Local $aParts = StringSplit($sLine, ",")
		If $aParts[0] >= 2 Then
			Local $iHeroID = Int(StringStripWS($aParts[1], 3))
			AddHero($iHeroID)
			Sleep(100)
		EndIf
	WEnd
	FileClose($hFile)

	Sleep(500)
	Out("Loading skill templates...")

	; Second pass: load skill templates (now that all heroes are added)
	$hFile = FileOpen($sFilePath, 0)
	$iHeroSlot = 1
	While 1
		Local $sLine2 = FileReadLine($hFile)
		If @error Then ExitLoop

		$sLine2 = StringStripWS($sLine2, 3)
		If $sLine2 = "" Or StringLeft($sLine2, 1) = ";" Then ContinueLoop

		Local $iCommentPos2 = StringInStr($sLine2, ";")
		If $iCommentPos2 > 0 Then
			$sLine2 = StringStripWS(StringLeft($sLine2, $iCommentPos2 - 1), 2)
		EndIf

		Local $aParts2 = StringSplit($sLine2, ",")
		If $aParts2[0] >= 2 Then
			Local $sSkillTemplate2 = StringStripWS($aParts2[2], 3)
			; Use lightweight skill-only load to avoid rate-limit disconnect
			; Full LoadSkillTemplate sends 50+ packets per hero (attributes)
			_LoadSkillBarOnly($sSkillTemplate2, $iHeroSlot)
			$iHeroSlot += 1
			Sleep(1000)
		EndIf
	WEnd
	FileClose($hFile)

	Out("Hero config loaded successfully")
	Return True
EndFunc

;~ Lightweight skill template loader — loads ONLY the skillbar, skips attributes
;~ This avoids sending 50+ attribute packets per hero which triggers rate-limit disconnects
Func _LoadSkillBarOnly($buildTemplate, $heroIndex = 0)
	Local $buildTemplateChars = StringSplit($buildTemplate, '')
	_ArrayDelete($buildTemplateChars, 0)

	$buildTemplate = ''
	For $character In $buildTemplateChars
		$buildTemplate &= Base64ToBin64($character)
	Next

	; Parse header
	Local $templateType = Bin64ToDec(StringLeft($buildTemplate, 4))
	$buildTemplate = StringTrimLeft($buildTemplate, 4)
	If $templateType <> 14 Then Return False

	Local $versionNumber = Bin64ToDec(StringLeft($buildTemplate, 4))
	$buildTemplate = StringTrimLeft($buildTemplate, 4)

	Local $professionBits = Bin64ToDec(StringLeft($buildTemplate, 2)) + 4
	$buildTemplate = StringTrimLeft($buildTemplate, 2)

	; Skip professions
	$buildTemplate = StringTrimLeft($buildTemplate, $professionBits) ; primary
	$buildTemplate = StringTrimLeft($buildTemplate, $professionBits) ; secondary

	; Skip attributes
	Local $attributesCount = Bin64ToDec(StringLeft($buildTemplate, 4))
	$buildTemplate = StringTrimLeft($buildTemplate, 4)
	Local $attributesBits = Bin64ToDec(StringLeft($buildTemplate, 4)) + 4
	$buildTemplate = StringTrimLeft($buildTemplate, 4)
	$buildTemplate = StringTrimLeft($buildTemplate, $attributesCount * ($attributesBits + 4))

	; Parse skills
	Local $skillsBits = Bin64ToDec(StringLeft($buildTemplate, 4)) + 8
	$buildTemplate = StringTrimLeft($buildTemplate, 4)

	Local $skills[8]
	For $i = 0 To 7
		$skills[$i] = Bin64ToDec(StringLeft($buildTemplate, $skillsBits))
		$buildTemplate = StringTrimLeft($buildTemplate, $skillsBits)
	Next

	; Load ONLY the skillbar — no attribute changes
	LoadSkillBar($skills[0], $skills[1], $skills[2], $skills[3], $skills[4], $skills[5], $skills[6], $skills[7], $heroIndex)
EndFunc
