#requireadmin
#include "lib\Froggy_Includes.au3"

Global Const $QUEST_ID_TEKKS_WAR = 0x339
Global Const $DIALOG_ID_TEKKS_WAR_REWARD = 0x833907
; $DIALOG_ID_TEKKS_WAR_ACCEPT removed (dead — unused)
; $HERO_ID_MERCENARY_1/2/3 removed (dead — never referenced)

Global Const $BOTNAME = "Froggy HM"
Global Const $VERSION = "1.6"
Global Const $AUTHORS[1] = ["Bob and Gemini"]

Global $Outpost = 638 ; Gadd's Camp
Global $Language = 0 ; English
; $Summon_Spirits removed (dead — never read)

Global Const $TYPE_MATERIAL_AND_ZCOINS = 11
Global Const $TYPE_TROPHY = 30
Global Const $TYPE_SCROLL = 31
Global Const $TYPE_BUNDLE = 6
Global Const $TYPE_USABLE = 9
Global Const $TYPE_DYE = 10
Global Const $TYPE_GOLD_COINS = 20
Global Const $Type_Attack = 14 ; Added missing constant
Global Const $TYPE_KEY = 18

; $CumulatedTime moved to BotCore-RunStats.au3

Global $mBasePointer

Global $BotRunning = False
Global $OpenedChestAgentIDs[1]
Global $NearestWaypoint = 0
Global $LastWaypoint = 0
; $unlit removed (dead — torch logic commented out)
Global $iVanguardTitle, $iNornTitle, $iAsuraTitle, $iDeldrimorTitle
; $BestTargetPtr moved to BotCore-Combat.au3
; $GUI_RunCounter, $GUI_FailCounter, $AvgRunTime moved to BotCore-RunStats.au3
; $nBestRunTime, $nCurrentRunTime, $nTotalRunTime moved to BotCore-RunStats.au3
; $bRunFailed moved to BotCore-RunStats.au3
; $district_name removed (dead — TravelTo uses $aLanguage/$aRegion params)



ScanAndUpdateGameClients()

Global $Form1 = GUICreate("Froggy HM", 250, 150)

If Not IsArray($game_clients) Or $game_clients[0][0] = 0 Then
	MsgBox(48, "Error", "No Guild Wars clients found." & @CRLF & "Please start Guild Wars and log in to a character.")
	Exit
EndIf

Global $Combo_Label = GUICtrlCreateLabel("Select Character:", 20, 20, 100, 20)
Global $Char_Combo = GUICtrlCreateCombo("", 20, 40, 210, 25)
Global $Launch_Button = GUICtrlCreateButton("Launch", 85, 90, 80, 30)

Local $comboList = ""
For $i = 1 To $game_clients[0][0]
	$comboList &= $game_clients[$i][3] & "|"
Next
$comboList = StringTrimRight($comboList, 1)
GUICtrlSetData($Char_Combo, $comboList, $game_clients[1][3])

GUICtrlSetOnEvent($Launch_Button, "LaunchEvent")
GUISetOnEvent($GUI_EVENT_CLOSE, "CloseEvent")
GUISetState(@SW_SHOW)

Global $g_BotHasLaunched = False
While Not $g_BotHasLaunched
	Sleep(100)
WEnd

Func CloseEvent()
	Exit
EndFunc

Func LaunchEvent()
	GUICtrlSetData($Combo_Label, "Launching...")
	Global $Character_Select = GUICtrlRead($Char_Combo)
	Local $clientIndex = FindClientIndexByCharacterName($Character_Select)

	If $clientIndex > 0 Then
		SelectClient($clientIndex)
		InitializeGameClientData(True, False)
		$mBasePointer = MemRead(GetScannedAddress('ScanBasePointer', 8))
		WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())
		GUIDelete($Form1)
		$g_BotHasLaunched = True
	Else
		MsgBox(48, "Error", "Could not find a GW client with character: '" & $Character_Select & "'")
		GUICtrlSetData($Combo_Label, "Select Character:")
	EndIf
EndFunc

GUI_SetOnStartFunc("onStart")
GUI_SetOnStopFunc("onStop")
GUI_SetOnResumeFunc("onResume")
GUI_Create()

Global $gReturnMap = $Gadds_Encampment
Global $gPickupCoins = True

; Combat state enum + $SkillBarCache + $SkillbarSlot moved to BotCore-Combat.au3
; $PressureSpiritSkills removed (dead — write-only, never read)

While 1
	Sleep(200)
	While $BotRunning
		Sleep(250)

		$OpenedChestAgentIDs[0] = ""
		ReDim $OpenedChestAgentIDs[1]
		Local $currentMap = GetMapID()

		Switch $currentMap
			Case $Sparkfly_Swamp
                PerformMaintenance()
				RunToDungeon()
				Takequest0()
			Case $Bogroot_Growths_Lvl1
				BogrootLvl1()
			Case $Bogroot_Growths_Lvl2
				BogrootLvl2()
			Case Else
				Setup(GUI_IsAddHeroesChecked())
		Endswitch
	WEnd
WEnd

Func onStart()
	$BotRunning = True
	Out("Start pressed")
	$iVanguardTitle = GetVanguardTitle()
	$iNornTitle = GetNornTitle()
	$iAsuraTitle = GetAsuraTitle()
	$iDeldrimorTitle = GetDeldrimorTitle()
	;GUI_SetLockpicks(GetPicksCount())
	GUI_SetWipes(0)
	;GUI_SetGolds(0)
	;GUI_SetTomes(0)
	GUI_SetChestsOpened(0)
	$nTotalRunTime = TimerInit()  ; Start total runtime timer
	AdlibRegister("UpdateStats", 1000)
	AdlibRegister("TotalRunTime", 1000)  ; Update total runtime every second
	CacheSkillBar()
EndFunc

Func onStop()
	$BotRunning = False
EndFunc

Func onResume()
	$BotRunning = True
EndFunc




Func Setup($addHeroes = True)
	Out("Entering Setup function...")
    PerformMaintenance(True)
	If GetMapID() <> 638 Then
		Out("Incorrect map. Zoning to Gadds Encampment (638)...")
		ZoneMap(638)
	Endif
	Sleep(1000)
	
	If($addHeroes) Then
		Out("Setting up heroes...")
		LeaveParty()
		sleep(500)
		
		; Load hero configuration from selected file
		LoadHeroConfigFromFile(GUI_GetSelectedHeroConfig())

		sleep(500)

		Out("Setting hero behaviors...")
		For $i = 1 To 2
			SetHeroBehaviour($i, 1) ;0=Fight, 1=Guard, 2=Avoid
			sleep(100)
		Next
		For $i = 3 To 6
			SetHeroBehaviour($i, 1) ;0=Fight, 1=Guard, 2=Avoid
			sleep(100)
		Next

		SetHeroBehaviour(6,1)
	EndIf

	SwitchMode(2)

	Out("Setup complete. Let's GO")
	MoveTo(-10018, -21892)
	MoveTo(-9550, -20400)
	TolSleep(500)
	Do
		Move(-9451, -19766)
	Until WaitMapLoading($Sparkfly_Swamp)

EndFunc

; Load hero configuration from file
; File format: HeroID,SkillTemplateCode (one per line, comments start with ;)
Func LoadHeroConfigFromFile($sConfigName)
	Local $sFilePath = @ScriptDir & "\hero_configs\" & $sConfigName & ".txt"
	
	If Not FileExists($sFilePath) Then
		Out("ERROR: Hero config file not found: " & $sFilePath)
		Return False
	EndIf
	
	Out("Loading hero config: " & $sConfigName)
	
	Local $hFile = FileOpen($sFilePath, 0) ; Read mode
	If $hFile = -1 Then
		Out("ERROR: Could not open config file")
		Return False
	EndIf
	
	Local $iHeroSlot = 1
	While 1
		Local $sLine = FileReadLine($hFile)
		If @error Then ExitLoop
		
		; Skip empty lines and comments
		$sLine = StringStripWS($sLine, 3) ; Strip leading/trailing whitespace
		If $sLine = "" Or StringLeft($sLine, 1) = ";" Then ContinueLoop
		
		; Remove inline comments
		Local $iCommentPos = StringInStr($sLine, ";")
		If $iCommentPos > 0 Then
			$sLine = StringStripWS(StringLeft($sLine, $iCommentPos - 1), 2)
		EndIf
		
		; Parse HeroID,SkillTemplate
		Local $aParts = StringSplit($sLine, ",")
		If $aParts[0] >= 2 Then
			Local $iHeroID = Int(StringStripWS($aParts[1], 3))
			Local $sSkillTemplate = StringStripWS($aParts[2], 3)
			
			; Add hero
			AddHero($iHeroID)
			Sleep(100)
		EndIf
	WEnd
	FileClose($hFile)
	
	Sleep(500)
	Out("Loading skill templates...")
	
	; Re-read file for skill templates (now that all heroes are added)
	$hFile = FileOpen($sFilePath, 0)
	$iHeroSlot = 1
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
			Local $sSkillTemplate = StringStripWS($aParts[2], 3)
			LoadSkillTemplate($sSkillTemplate, $iHeroSlot)
			$iHeroSlot += 1
		EndIf
	WEnd
	FileClose($hFile)
	
	Out("Hero config loaded successfully")
	Return True
EndFunc



Func RunToDungeon()
	Out("Running To Bogroot")
	Local $aWaypoints[11][4] = [ _
      [-4559, -14406, 1300, "1"], _
      [-5204, -9831, 1300, "2"], _
      [-928, -8699, 1300, "3"], _
      [4200, -4897, 1500, "4"], _
      [6114, 819, 1300, "5"], _
      [9500, 2281, 1300, "6"], _
      [11570, 6120, 1200, "7"], _
      [11025, 11710, 900, "8 "], _
      [14624, 19314, 600, "9"], _
      [14650, 19417, 0, "10"], _
      [12280, 22585, 0, "11"]]
	MoveandAggroEx($aWaypoints)
	ClearMem()  ; Clean up DllStructs from MoveandAggroEx before continuing

   If $GUI_RunCounter > 0 Then
		GUI_SetAvgRunTime(AvgRunTime())
		GUI_SetBestRunTime(BestRunTime())
	 EndIf
   AdlibUnRegister("CurrentRunTime")
 EndFunc

Func TakeQuest0()
	Local  $Return1
	Local  $Return2
	Global $nCurrentRunTime = 0
	Global $nCurrentRunTime = TimerInit()
	Global $bRunFailed = False  ; Reset failure flag at start of run
	AdlibRegister("CurrentRunTime", 1000)
	ClearMem()
	SetPlayerStatus(0)
	GUI_SetRunCounter()
	Out("Starting Run Num: " & $GUI_RunCounter)

	TolSleep(800)
	ClearMem()  ; Clear memory before heavy agent operations to prevent allocation errors
	MoveTo(12396, 22407)
	Out("Accept Quest")
	
	; Add a small delay after moving to allow game state to stabilize
	Sleep(500)
	Local $NPC = GetNearestNPCToCoords(12396, 22407)
	Out("Debug: NPC IsDllStruct=" & IsDllStruct($NPC) & " ID=" & (IsDllStruct($NPC) ? DllStructGetData($NPC, 'ID') : "N/A"))
	
	If IsDllStruct($NPC) Then
		Local $npcDist = GetDistance(GetMyAgent(), $NPC)

		
		; Move closer if needed
		If $npcDist > 100 Then
			MoveTo(DllStructGetData($NPC, 'X'), DllStructGetData($NPC, 'Y'))
		EndIf
		
		GoNPC($NPC)

		Sleep(2000) ; Wait for dialog to open
	Else

	EndIf

	; Dialog sequence from My Mercs:

	QuestReward($QUEST_ID_TEKKS_WAR) ; in case for some reason we didnt find tekks after chest
	Sleep(500)
	

	AcceptQuest($QUEST_ID_TEKKS_WAR)
	Sleep(500)


	Dialog(0x2AE6)
	Sleep(500)
	

	Dialog(0x833905)
	Sleep(500)
	
	;0x8101 dialog body
	;0x833901 accept quest
	
	;dialog 0x2AE6
	;dialog 0x833905
	
    MoveTo(12228, 22677)
	sleep(500)
	MoveTo(12470, 25036)
	sleep(500)
	toggleautorun()
	Move(12968, 26219)
	sleep(1000)
	

	Local $aTimer = TimerInit()
	Do
		Move(13097, 26393)
		Sleep(250)
	Until WaitMapLoading($Bogroot_Growths_Lvl1) Or TimerDiff($aTimer) > 60000
	
EndFunc


 Func BogrootLvl1 ()
   Local $aWaypointsLevel1[28][4] = [ _
	[17026, 2168, 1200, "0"], _
	[19099, 7762, 1200, "Blessing Lvl1"], _
	[17279, 8106, 1200, "1"], _
	[14434, 8000, 1200, "Quest Door Checkpoint"], _
	[14434, 8000, 1300, "2"], _
	[10789, 6433, 1300, "3"], _
	[8101, 6800, 1200, "4"], _
	[6721, 5340, 1300, "5"], _
	[4305, 1078, 1300, "6"], _
    [757, 1110, 1200, "7"], _
    [1370, 149, 1700, "8"], _
	[672, 1105, 2000, "9"], _
	[453, 1449, 2000, "10"], _
    [504, -1973, 800, "11"], _
	[-447, -3014, 800, "12"], _
    [-1055, -4527, 1000, "13"], _
    [-1424, -6156, 1200, "14"], _
	[-475, -7511, 800, "15"], _
	[265, -8791, 1400, "16"], _
	[1061, -9443, 1400, "17"], _
	[1805, -10185, 1400, "18"], _
	[1665, -12213, 1400, "19"], _
	[3550, -16052, 1400, "20"], _
	[4941, -16181, 0, "21"], _
	[7360, -17361, 0, "22"], _
	[7552, -18776, 0, "23"], _
	[7665, -19050, 0, "24"], _
	[7665, -19050, 0, "Lvl1 to Lvl2"]]
	$NearestWaypoint = GetNearestWaypointIndex($aWaypointsLevel1)
	CacheSkillBar()
	

	
	Out("Start at " & $aWaypointsLevel1[$NearestWaypoint][3])
	MoveandAggroEx($aWaypointsLevel1)
	
Endfunc

Func BogrootLvl2 ()
	Local $aWaypointsLevel2[36][4] = [ _
	[-11386, -3871, 400, "1"], _
	[-11132, -2450, 400, "2"], _
	[-8559, 593, 300, "3"], _
	[-4110, 4484, 1200, "4"], _
    [-3747, 5068, 1200, "5"], _
    [-2597, 5775, 1000, "6"], _
    [-2618, 6383, 1100, "7"], _
    [-2770, 7571, 1000, "8"], _
    [-243, 8364, 1000, "9"], _
    [-189, 10499, 1000, "10"], _
	[37, 11449, 1400, "11"], _
	[3086, 12899, 2000, "12"], _
	[4182, 13767, 2000, "13"], _
	[7293, 9457, 2000, "14"], _
    [8150, 8143, 1500, "15"], _
	[8560, 2323, 1500, "16"], _
    [9525, -1153, 1500, "17"], _
	[12200, -6591, 1600, "18"], _
	[12200, -6591, 1600, "19"], _
    [17003, -4906, 1600, "20"], _
	[16854, -5830, 1600, "Dungeon Key"], _
	[17925, -6197, 300, "Dungeon Door"], _
	[17482, -6661, 300, "Dungeon Door Checkpoint"], _
	[18334, -8838, 0, "Boss 1"], _
	[16131, -11510, 0, "Boss 2"], _
	[19009, -12300, 0, "Boss 3"], _
	[19610, -11527, 0, "Boss 4"], _
	[18413, -13924, 0, "Boss 5"], _ 
	[14188, -15231, 0, "Boss 6"], _
	[13186, -17286, 0, "Boss 7"], _
	[14035, -17800, 0, "Boss 8"], _
	[13583, -17529, 1100, "Boss 9"], _
	[14617, -18282, 1400, "Boss 10"], _
	[15117, -18582, 1400, "Boss 11"], _
	[15117, -18582, 1400, "Boss 12"], _
	[15117, -18582, 1600, "Boss"]]
	CacheSkillBar() ; 16006, -6019 was 20
   $NearestWaypoint = GetNearestWaypointIndex($aWaypointsLevel2)

	Local $Me = GetMyAgent()


	
	Out("Start at " & $aWaypointsLevel2[$NearestWaypoint][3])
	MoveandAggroEx($aWaypointsLevel2)
	
	Local $Timer = TimerInit()
	Do
		Sleep(200)
	Until WaitMapLoading($Sparkfly_Swamp) Or TimerDiff($Timer) >= 360000 


    AdlibUnregister("CurrentRunTime")
    Sleep(500)
    ClearMem()
   AdlibUnregister("CurrentRunTime")
   TolSleep(3000)
EndFunc

Func GetDungeonKeyEx()
	ClearTarget()
	AggroMoveToEx(16854, -5830)
	PickupLootEx(18000)
	AggroMoveToEx(16854, -5830)
	PickupLootEx(18000)
	AggroMoveToEx(16854, -5830) ; Assuming this is the location where boss key is
	PickupLootEx(4000, 18000) ; double range to search for boss key
EndFunc

Func OpenDungeonDoor()
	Out("Open loop Dungeon Door")
	ClearTarget()
	Moveto(17925, -6197)
	sleep(1000)
	ClearTarget()
	ActionInteract()
	ActionInteract()
	Sleep(500)
	Moveto(17925, -6197)
	Sleep(1000)
	ClearTarget()
	ActionInteract()
	ActionInteract()
	Sleep(1000)
	ActionInteract()
	ActionInteract()
	MoveTo(17482, -6661)
EndFunc

Func Boss()
	MoveTo(14876, -19033)
	Local $BogrootChest = GetNearestSignpostToCoords(14876, -19033)
	GoToSignpost($BogrootChest)
	TolSleep(5000)
	PickupLootEx(5000)
	GoToSignpost($BogrootChest)
	TolSleep(5000)
	PickupLootEx(5000)
	MoveTo(14618, -17828)
	
	; 0x8101 dialog body
	; 0x833907 accept quest reward
	Local $NPC = GetNearestNPCToCoords (14618, -17828)
	If IsDllStruct($NPC) Then
		GoNPC($NPC)
		Sleep(2000)  ; Wait for dialog to fully open (was GetPing()+500, too short)
		
		; Try to accept the quest reward, retry if it doesn't take
		For $iRetry = 1 To 3
			Out("Attempting quest reward (attempt " & $iRetry & "/3)")
			QuestReward($QUEST_ID_TEKKS_WAR)
			Sleep(1000)  ; Wait for game to process quest reward
			Dialog($DIALOG_ID_TEKKS_WAR_REWARD)
			Sleep(1000)  ; Wait for dialog to process
			
			; If the reward was accepted, the quest state should change
			; Give it a moment and try again if needed
			If $iRetry < 3 Then
				; Re-interact with NPC in case dialog closed without accepting
				GoNPC($NPC)
				Sleep(1500)
			EndIf
		Next
	Else
		Out("WARNING: Could not find Tekk NPC near chest!")
	EndIf
	
	If GUI_IsSalvageChecked() = True Then Return SalvageItems()
EndFunc

Func ReverseToSparkflySwamp()
	MoveTo(14747, 480)
	sleep(500)
	
	Local $aTimer = TimerInit()
	Do
		MoveTo(14747, 480)
		Sleep(250)
	Until WaitMapLoading($Sparkfly_Swamp) or TimerDiff($aTimer) > 60000
EndFunc





Func GoToSignpostNearXY($x, $y)
    Local $signpost = GetNearestSignpostToCoords($x, $y)
    GoToSignpost($signpost)
EndFunc



; Fight function removed here. New Fight logic appended at the end of the script.



;I have ported the script to use the new GWA2 library. Please test it and let me know if you find any issues.






 Func usedp()
	usedp9()
	usedp9()
	usedp9()
	usedp9()
	usedp9()
endfunc

Func usedp9()
   USedp1()
   Usedp2()
   usedp3()
   usedp4()
   USedp5()
   Endfunc


Func Usedp1()
   Local $aBag
   Local $aItem
   Sleep(2)
   For $i = 1 To 4
	  $aBag = GetBag($i)
	  For $j = 1 To DllStructGetData($aBag, "Slots")
		 $aItem = GetItemBySlot($aBag, $j)
		 If DllStructGetData($aItem, "ModelID") == 26784 Then
			UseItem($aItem)
			Return True
		 EndIf
	  Next
   Next
EndFunc

Func Usedp2()
   Local $aBag
   Local $aItem
   Sleep(2)
   For $i = 1 To 4
	  $aBag = GetBag($i)
	  For $j = 1 To DllStructGetData($aBag, "Slots")
		 $aItem = GetItemBySlot($aBag, $j)
		 If DllStructGetData($aItem, "ModelID") == 22191 Then
			UseItem($aItem)
			Return True
		 EndIf
	  Next
   Next
EndFunc

Func Usedp3()
   Local $aBag
   Local $aItem
   Sleep(2)
   For $i = 1 To 4
	  $aBag = GetBag($i)
	  For $j = 1 To DllStructGetData($aBag, "Slots")
		 $aItem = GetItemBySlot($aBag, $j)
		 If DllStructGetData($aItem, "ModelID") == 21488 Then
			UseItem($aItem)
			Return True
		 EndIf
	  Next
   Next
EndFunc

Func Usedp4()
   Local $aBag
   Local $aItem
   Sleep(2)
   For $i = 1 To 4
	  $aBag = GetBag($i)
	  For $j = 1 To DllStructGetData($aBag, "Slots")
		 $aItem = GetItemBySlot($aBag, $j)
		 If DllStructGetData($aItem, "ModelID") == 21489 Then
			UseItem($aItem)
			Return True
		 EndIf
	  Next
   Next
EndFunc

Func Usedp5()
   Local $aBag
   Local $aItem
   Sleep(2)
   For $i = 1 To 4
	  $aBag = GetBag($i)
	  For $j = 1 To DllStructGetData($aBag, "Slots")
		 $aItem = GetItemBySlot($aBag, $j)
		 If DllStructGetData($aItem, "ModelID") == 6370 Then
			UseItem($aItem)
			Return True
		 EndIf
	  Next
   Next
EndFunc




; All reusable functions have been extracted to BotCore modules in lib/custom/.
; See Kanban_Froggy_Function_Extraction.md for details.

