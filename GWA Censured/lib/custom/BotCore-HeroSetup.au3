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
			LoadSkillTemplate($sSkillTemplate2, $iHeroSlot)
			$iHeroSlot += 1
		EndIf
	WEnd
	FileClose($hFile)

	Out("Hero config loaded successfully")
	Return True
EndFunc
