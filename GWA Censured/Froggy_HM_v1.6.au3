#requireadmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\Utils.au3"
#include "lib\JSON.au3"
#include "lib\SQLite.au3"
#include "lib\SQLite.dll.au3"
#include "lib\Map_IDs.au3"
#include "lib\Skill_IDs.au3"
#include "lib\Skill_Types.au3"
#include "lib\Utils-Debugger.au3"
#include "lib\GUI_Functions.au3"
#include <GUIConstantsEx.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>

Global Const $QUEST_ID_TEKKS_WAR = 0x339
Global Const $DIALOG_ID_TEKKS_WAR_ACCEPT = 0x833901
Global Const $DIALOG_ID_TEKKS_WAR_REWARD = 0x833907

Global Const $BOTNAME = "Froggy HM"
Global Const $VERSION = "1.6"
Global Const $AUTHORS[1] = ["Bob and Gemini"]

Global $Outpost = 638 ; Gadd's Camp
Global $Language = 0 ; English
Global $Summon_Spirits = 0

Global Const $TYPE_MATERIAL_AND_ZCOINS = 11
Global Const $TYPE_TROPHY = 30
Global Const $TYPE_SCROLL = 31
Global Const $TYPE_BUNDLE = 6
Global Const $TYPE_USABLE = 9
Global Const $TYPE_DYE = 10
Global Const $TYPE_GOLD_COINS = 20
Global Const $Type_Attack = 14 ; Added missing constant
Global Const $TYPE_KEY = 18

; Logic Port
; Missing Skill IDs moved to Skill_IDs.au3
Global $CumulatedTime = 0

Global $mBasePointer

Global $BotRunning = False
Global $OpenedChestAgentIDs[1]
Global $NearestWaypoint = 0
Global $LastWaypoint = 0
Global $unlit = False
Global $iVanguardTitle, $iNornTitle, $iAsuraTitle, $iDeldrimorTitle
Global $BestTargetPtr = 0
Global $GUI_RunCounter = 0, $GUI_FailCounter = 0, $AvgRunTime = 0
Global $nBestRunTime = 999999999, $nCurrentRunTime = 0



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
		$mBasePointer = MemoryRead(GetScannedAddress('ScanBasePointer', 8))
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

Global Enum $all = 0, $ptr, $energyreq, $adrereq, $type, $target, $hexes, $pressure, $bind, $speedBoost, $survive, $attackskill, $heal, $prot, $bond, $condremove, $hexremove, $enchantremove, $rupt, $hardrupt, $precast, $chantsnshouts, $echoes
Global $SkillBarCache[9][23]
Global $SkillbarSlot[3500]
Global $PressureSpiritSkills = 0

While 1
	Sleep(200)
	While $BotRunning
		Sleep(250)

		$OpenedChestAgentIDs[0] = ""
		ReDim $OpenedChestAgentIDs[1]
		Local $currentMap = GetMapID()

		Switch $currentMap
			Case $Sparkfly_Swamp
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
	AdlibRegister("UpdateStats", 1000)
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
	If GetMapID() <> 638 Then
		Out("Incorrect map. Zoning to Gadds Encampment (638)...")
		ZoneMap(638)
	Endif
	Sleep(1000)
	
	If($addHeroes) Then
		Out("Setting up heroes...")
		LeaveParty()
		sleep(500)
		AddHero(25) ;Xandra
		AddHero(14) ;Olias
		AddHero(21) ; Livia
		AddHero(4) ; Master of Whispers
		AddHero(24) ; Gwen
		AddHero(15) ; Norgu
		AddHero(1); Razah

		sleep(500)

		Out("Loading hero skill templates...")
		LoadSkillTemplate("OAOiAyk8gNtePuwJ00ZaNbJA", 1) ; ST Xandra remove hex
		LoadSkillTemplate("OAhjQkGZIT3BVVCPSTTODTjTciA", 2) ; BiP Olias
		LoadSkillTemplate("OAhjYoHYIPWb7wnoqKNncDzqH", 3) ; Xinrae Livia
		LoadSkillTemplate("OAljUwGpZSUBKgfBVVbh8Y7Y1YA", 4) ; MM Master P
		LoadSkillTemplate("OQhkAsC8gFKzJY6lDMd40hQG4iB", 5)	; E-Surge Gwen
		LoadSkillTemplate("OQhkAsC8gFKDNY6lDMd40hQG4iB", 6) ; Inep Norgu
		LoadSkillTemplate("OQljAkBsZSvAIg5ZkAcQsA7Y1YA", 7)	; Panic Razah

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
	ClearMemory()  ; Clean up DllStructs from MoveandAggroEx before continuing

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
	AdlibRegister("CurrentRunTime", 1000)
	clearmemory()
	SetPlayerStatus(0)
	GUI_SetRunCounter()
	Out("Starting Run Num: " & $GUI_RunCounter)

	TolSleep(800)
	ClearMemory()  ; Clear memory before heavy agent operations to prevent allocation errors
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
    ClearMemory()
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
	GoNPC($NPC)
	Sleep(GetPing() + 500)  ; Wait for dialog to open
	QuestReward($QUEST_ID_TEKKS_WAR)
	Sleep(500)  ; Wait for game to process quest reward
	Dialog($DIALOG_ID_TEKKS_WAR_REWARD)
	Sleep(500)  ; Wait for dialog to process
	
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

Func MoveandAggroEx($aWaypoints)
	Local $lMapId = GetMapId()
	Local $lLastNearestWaypoint = -1
	Local $lStuckCounter = 0
	For $i = GetNearestWaypointIndex($aWaypoints) To UBound($aWaypoints) - 1 Step 1
		;BossGlow(GetMyID(), Random(2, 10, 1))
		$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
		
		; Stuck detection: if we're stuck at the same waypoint for too long, backtrack
		If $NearestWaypoint = $lLastNearestWaypoint Then
			$lStuckCounter += 1
			If $lStuckCounter >= 5 Then
				Out("STUCK DETECTED: Nearest waypoint hasn't changed in " & $lStuckCounter & " iterations. Waypoint=" & $aWaypoints[$NearestWaypoint][3])
				Out("Current target waypoint $i=" & $i & " (" & $aWaypoints[$i][3] & ")")
				Out("Backtracking to nearest waypoint and retrying...")
				$i = $NearestWaypoint - 1  ; For loop will increment, so we'll retry from NearestWaypoint
				$lStuckCounter = 0  ; Reset counter after backtracking
			EndIf
		Else
			$lStuckCounter = 0  ; Reset counter when we make progress
			$lLastNearestWaypoint = $NearestWaypoint
		EndIf
		
		Sleep(200)
		If GetMapId() <> $lMapId Then Return ; Need to get out of this MoveAggro loop if the mapid changes
		; (SoO only) unlit effect intercept - backtracks until lit effect is reapplied and resumes at light torch waypoint
		; Consets arent working - duration check is wrong or something
		;If(GUI_IsConsetsChecked() and (GetMapId() = $Bogroot_Growths_Lvl1 Or GetMapId() = $Bogroot_Growths_Lvl2)) Then UseConsets()
		;If(GUI_IsConsetsChecked() and (GetMapId() = $Bogroot_Growths_Lvl1 Or GetMapId() = $Bogroot_Growths_Lvl2)) Then UseArmor()
		;If $unlit Then $i = RekindleTorch($aWaypoints, $NearestWaypoint)
        If GetMapLoading() == 2 Then Disconnected()
		If Wipe() = 1 Then    ; wipe intercept - wait until rezz and redirect to best waypoint
			GUI_SetWipes(GUI_GetWipes() + 1)
			$LastWaypoint = $i
			Out("We wiped at " & $aWaypoints[$LastWaypoint][3] & ", waiting for rezz")
			CancelAll()    ; in case we were pulling enemies
			Local $lWipeWaitCounter = 0
			Local $lDeadlock = TimerInit()
			Do
				Sleep(500)
				$lWipeWaitCounter += 1
				; Debug: Log what we're waiting for
				If Mod($lWipeWaitCounter, 10) = 0 Then
					Out("Still waiting... Counter=" & $lWipeWaitCounter & " PlayerDead=" & GetIsDead(-2) & " PartyDefeated=" & GetPartyDefeated())
				EndIf
				If GetPartyDefeated() Or TimerDiff($lDeadlock) > 120000 Or $lWipeWaitCounter > 240 Then 
					Out("Timeout or party defeated - resigning. Counter=" & $lWipeWaitCounter & " TimerDiff=" & Round(TimerDiff($lDeadlock)))
					Return ResignAndReturn($Outpost, $Language)
				EndIf
			Until Not GetIsDead(-2)  ; Wait until PLAYER is alive, not party average HP
			Out("Player is alive! Continuing...")
			If GetMorale() < -40 Then Usedp()
			$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
			$i = WipeManagement($aWaypoints, $NearestWaypoint, $LastWaypoint)
			; Safety check: if we're too far from target waypoint, use nearest instead
			Local $DistanceToTarget = GetDistanceToPoint(GetMyAgent(), $aWaypoints[$i][0], $aWaypoints[$i][1])
			If $DistanceToTarget > 5000 Then
				Out("Target waypoint too far (" & Floor($DistanceToTarget) & "), using nearest waypoint instead")
				$i = $NearestWaypoint
			EndIf
			Out("Restarting at : " & $aWaypoints[$i][3])
		EndIf

		Out("Nearest waypoint - " & $aWaypoints[$NearestWaypoint][3])
		Out("Moving to - " & $aWaypoints[$i][3])

		Switch ($aWaypoints[$i][3])
		   	Case "Boss lock", "Chest", "Signpost"
				If Wipe() = 0 Then GoToSignpostNearXY($aWaypoints[$i][0], $aWaypoints[$i][1])
				PickupLootEx()

			Case "Lvl1 to Lvl2"
				Local $aTimer = TimerInit()
				Do
					MoveTo(7665, -19050)
					Sleep(250)
				Until WaitMapLoading($Bogroot_Growths_Lvl2) Or TimerDiff($aTimer) > 60000
			Case "Dungeon Key"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				GetDungeonKeyEx()
			Case "Dungeon Door"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				OpenDungeonDoor()
			Case "Dungeon Door Checkpoint"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
				If($NearestWaypoint <> $i) Then
					Out("Failed dungeon door, going back to waypoint i-3")
					For $j = $i-1 To $i - 3 Step -1
						If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$j][0], $aWaypoints[$j][1], $aWaypoints[$j][2])
						If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$j][0], $aWaypoints[$j][1])
					Next
					$i = GetNearestWaypointIndex($aWaypoints)
				EndIf
			Case "Quest Door Checkpoint"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
				If($NearestWaypoint <> $i) Then
					Out("Failed first door, going back to get quest")
					For $j = $i-1 To $i - 3 Step -1
						If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$j][0], $aWaypoints[$j][1], $aWaypoints[$j][2])
						If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$j][0], $aWaypoints[$j][1])
					Next
					ReverseToSparkflySwamp()
					Return
				EndIf
			Case "Boss"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				Boss()
			Case Else
				If GetMapLoading() = 1 Then
					AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				Else
					MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				EndIf
		EndSwitch
	Next
EndFunc   ;==>MoveandAggro

Func GetNearestWaypointIndex($aWaypoints)
	Local $lNearestWaypoint, $lNearestDistance = 100000000
	Local $lDistance
	For $Index = 0 To UBound($aWaypoints) - 1
		$lDistance = GetDistanceToPoint(GetMyAgent(), $aWaypoints[$Index][0], $aWaypoints[$Index][1])
		If $lDistance < $lNearestDistance Then
			$lNearestWaypoint = $Index
			$lNearestDistance = $lDistance
		EndIf
	Next
	Return $lNearestWaypoint
EndFunc

Func GetPartyDefeated()
    Return GetPartyState(0x20)
EndFunc

Func WipeManagement($aWaypoints, $NearestWaypoint, $LastWaypoint)
	Out("Last waypoint - " & $aWaypoints[$LastWaypoint][3])
	Out("Nearest waypoint index - " & $NearestWaypoint)

	; Use the waypoint index for comparisons, not the string label
	Switch GetMapID()
		Case $Shards_of_Oor_Lvl1
			Switch $NearestWaypoint
			Case 0 to 9
			     Return 0
			Case 10 to 14
			     Return 10
			Case 15 to 20
			    Return 14
			EndSwitch

		Case $Shards_of_Oor_Lvl2
			Switch $NearestWaypoint
			   Case  0 to 3
					Return 0
			   Case  4 to 11
					Return 4
			EndSwitch

	    Case $Shards_of_Oor_Lvl3
			Switch $NearestWaypoint
                Case 0 to 8
			      Return 0
			   Case 9 to 16
			      Return 9
			   Case 17 to 20
			      Return 17
			   Case 21 To 41
			     Return 21
		    	EndSwitch

		Case $Sparkfly_Swamp
			Switch $NearestWaypoint
			   Case 0 to 5
			    Return 0
			   Case 6 to 10
			    Return 6
		     	EndSwitch

		Case $Bogroot_Growths_Lvl1
			; Waypoints: 0=start, 1=Blessing, 2="1", 3=Quest Door, 4="2", 5="3", etc.
			Switch $NearestWaypoint
				Case 0 to 4
					Return 0  ; Go back to start
				Case 5 to 10
					Return 5  ; Around waypoint "3"
				Case 11 to 17
					Return 11 ; Around waypoint "11"
				Case 18 to 27
					Return 18 ; Around waypoint "18"
			EndSwitch

		Case $Bogroot_Growths_Lvl2
			; Waypoints: 0="1", 1="2", 2="3", etc.
			Switch $NearestWaypoint
				Case 0 to 3
					Return 0  ; Go back to start
				Case 4 to 10
					Return 4  ; Around waypoint "5"
				Case 11 to 20
					Return 11 ; Around waypoint "11"
				Case 21 to 35
					Return 21 ; After dungeon door
			EndSwitch

	EndSwitch
	Return $NearestWaypoint
EndFunc

Func GoToSignpostNearXY($x, $y)
    Local $signpost = GetNearestSignpostToCoords($x, $y)
    GoToSignpost($signpost)
EndFunc

Func AggroMoveToEX($x, $y, $aFightRange = 1350)

	Local $lDeadlock, $lBlocked, $aOldX, $aOldY
	Local $random = 100
	If GetMapLoading() <> 1 Then 
		Out("Debug: AggroMoveToEx Early Exit (MapLoading=" & GetMapLoading() & ")")
		Return True
	EndIf
	
	If WeCanMove($aFightRange) Then 
		Move($x, $y, $random)
	EndIf
	
	Local $TimerAggro = TimerInit()
	Do
		$aOldX = DllStructGetData(GetMyAgent(), 'X')
		$aOldY = DllStructGetData(GetMyAgent(), 'Y')
		If GetMapLoading() == 2 Then Disconnected()
		
		Local $dist = GetNearestEnemyDistance()

		
		If $dist < $aFightRange Then 
			Fight($aFightRange)
		EndIf
		
		If GUI_IsChestChecked() Then CheckForChest()
		If WeCanMove($aFightRange) Or TimerDiff($TimerAggro) > 60000 Then
			Move($x, $y, $random)
			PickupLootEx(3000)
			If DllStructGetData(GetMyAgent(), 'X') = $aOldX And DllStructGetData(GetMyAgent(), 'Y') = $aOldY Then
				$lBlocked += 1
				Move(DllStructGetData(GetMyAgent(), 'X'), DllStructGetData(GetMyAgent(), 'Y'), 500)
				Sleep(350)
				Move($x, $y, $random)
			EndIf
		EndIf
	Until GetDistanceToPoint(GetMyAgent(), $x, $y) < 250 Or $lBlocked > 30 Or TimerDiff($TimerAggro) > 240000 Or Wipe()
EndFunc

Func WeCanMove($aRange = 1200)
	Local $dist = GetNearestEnemyDistance()
	; Out("Debug: WeCanMove Check Dist=" & $dist & " Range=" & $aRange)
	If $dist < $aRange Then Return False
	Return True
EndFunc

; Fight function removed here. New Fight logic appended at the end of the script.

Func GetNearestEnemyDistance()
	Local $target = GetNearestEnemyToAgent(GetMyAgent())
	If IsDllStruct($target) Then
		Local $d = GetDistance(GetMyAgent(), $target)
		; Out("Debug: Found Enemy ID=" & DllStructGetData($target, 'ID') & " Dist=" & $d)
		Return $d
	Else
		; Out("Debug: No Enemy Found")
		Return 10000
	EndIf
EndFunc

Func CheckForChest($chestrun = False)
	Local $AgentArray, $lAgent, $lExtraType, $lType
	Local $ChestFound = False
	If GetIsDead(-2) Then Return
	$AgentArray = GetAgentArray(0x200)   ;0x200 = type: static
	Out("Looking for chests")
	For $i = 1 To $AgentArray[0]
		$lAgent = $AgentArray[$i]
		$lType = DllStructGetData($lAgent, 'Type')
		$lExtraType = DllStructGetData($lAgent, 'ExtraType')
		If $lType <> 512 Then ContinueLoop
		;If $aChestID[$lExtraType] = "" Then ContinueLoop ; This needs to be fixed with a proper map
		If _ArraySearch($OpenedChestAgentIDs, DllStructGetData($lAgent, 'ID')) <> -1 Then ContinueLoop

		_ArrayAdd($OpenedChestAgentIDs, DllStructGetData($lAgent, 'ID'))
		$ChestFound = True
		Out("Found a Chest")
		ExitLoop
	Next
	If Not $ChestFound Then Return
	Out("opening chest")
	ChangeTarget($lAgent)
	GoToSignpost($lAgent)
	OpenChest()
	Sleep(GetPing() + 500)
	$AgentArray = GetAgentArray(0x400)    ;0x400 = type: item
	If $AgentArray[0] > 0 Then ChangeTarget($AgentArray[1])
	If $chestrun = True Then
		PickupLootEx(5000)
	Else
		PickupLootEx(3500)
	EndIf
 EndFunc



Func PickupLootEx($iMaxDist = 2000, $PickupTorch = False)
	Local $lAgentArray = GetAgentArray($ID_AGENT_TYPE_ITEM)
	Local $lPickupDeadlock = TimerInit()
	Local $lPickupCounter = 0, $lDeadlock = 0
	If Not IsArray($lAgentArray) Then Return
	For $i = 1 To $lAgentArray[0]
		Local $lAgentStruct = $lAgentArray[$i]
        If Not IsDllStruct($lAgentStruct) Then ContinueLoop

		Local $lAgentID = DllStructGetData($lAgentStruct, 'ID')
		Local $lItem = GetItemByAgentID($lAgentID)
		If Not IsDllStruct($lItem) Then ContinueLoop

		Local $lOwner = DllStructGetData($lAgentStruct, 'Owner')
		If $lOwner <> 0 And $lOwner <> GetMyID() Then ContinueLoop

		If CanPickUpEx($lItem, $PickupTorch) And GetDistanceToPoint(GetMyAgent(), DllStructGetData($lAgentStruct, 'X'), DllStructGetData($lAgentStruct, 'Y')) < $iMaxDist Then
			MoveTo(DllStructGetData($lAgentStruct, 'X'), DllStructGetData($lAgentStruct, 'Y'))
			$lDeadlock = TimerInit()
			$lPickupCounter = 0
			Do
				PickUpItem($lAgentID)
				Sleep(250)
				Out("Pickup")
				$lPickupCounter += 1
			Until Not GetAgentExists($lAgentID) Or GetIsDead(-2) Or TimerDiff($lDeadlock) > 6000 or $lPickupCounter > 10
		EndIf
		If TimerDiff($lPickupDeadlock) > 120000 Then Return
	Next
EndFunc

Func CanPickUpEx($aItem, $PickupTorch = False)
    If Not IsDllStruct($aItem) Then Return False

	Local $lType = DllStructGetData($aItem, "Type")
	Local $lExtraID = DllStructGetData($aItem, "ExtraId")
	Local $lModelID = DllStructGetData($aItem, "ModelId")
	Local $lRarity = GetRarity($aItem)
	Local $lQuantity = DllStructGetData($aItem, "Quantity")
    Local $lValue = DllStructGetData($aItem, "Value")

	If CountFreeSlots() < 2 And $lType <> $TYPE_BUNDLE And $lType <> $TYPE_GOLD_COINS Then Return False
	 
	Switch $lModelID
		Case 2619, 36985 ;Unholy text from Fow, misterial commendations
			Return True
		 Case 27067, 27071, 27033, 27052, 22374 ; Blob Of Ooze, Vaettir Essence, Destroyer Core, Superb Charr Carving Kath Hammer
			Return True
		Case 2605, 2606, 501 to 503, 2566, 2607, 6102, 6104, 6531, 15564, 15565, 15867, 15869 to 15871, 17054, 17055 _
			,17075, 22781, 22782, 25410, 25413, 25416, 24628, 24582 ; General quest items: Prison key (25413), etc.
			Return True
		Case 910, 2513, 5585, 6049, 6366, 6367, 6375, 15477, 19171, 19172, 19173, 22190, 24593, 28435, 30855, 31145, 31146, 35124, 36682 _  ; alcohol
			, 15528, 15479, 19170, 21492, 21812, 22644, 30208, 31150, 35125, 36681 _  ; sweets
			, 17060, 17061, 17062, 22269, 28431, 28432, 28436, 29431, 31151, 31152, 31153, 35121 _  ; Sweet Pcons
			, 6370, 19039, 21488, 21489, 22191, 26784, 28433, 35127 _  ; DP Removal Sweets
			, 556, 18345, 21491, 37765, 21833, 28433, 28434 _ ; Special Drops
			, 935, 936	; Diamond and Onyx
		Return True
	EndSwitch

	Switch $lType
		Case $TYPE_BUNDLE
			If $PickupTorch And ($lModelID = 22342 or $lModelID = 24350) Then ; Unlit Torch, Asura Flame Staff
				Out("Grab unlit torch")
				Return True
			EndIf

		Case $TYPE_DYE
			If $lExtraID = 10 Then ; Black dye
				GUI_SetBlackDyes(GUI_GetBlackDyes() + 1)
				Return True
			EndIf
		Case $TYPE_GOLD_COINS
            If $gPickupCoins And $lModelID = 2511 And GetGoldCharacter() + $lValue < 100000 Then Return True
		Case $TYPE_KEY
			If $lModelID = 22751 Then GUI_SetDroppedLockpicks(GUI_GetDroppedLockpicks() + 1)
			If $lModelID = 25410 Or $lModelID = 25416 Then Out("Grab Dungeon Key")
			Return True

		Case $TYPE_MATERIAL_AND_ZCOINS, $TYPE_SCROLL, $TYPE_TROPHY
			Return false
		Case $TYPE_USABLE
			Switch $lModelID
			Case 21786 To 21805 ; Tomes
					GUI_SetTomes(GUI_GetTomes() + 1)
					Return True
		  EndSwitch
        EndSwitch

    Switch $lRarity
		Case $RARITY_Gold
			GUI_SetGolds(GUI_GetGolds() + 1)
		    Return True
	EndSwitch
	Return False
EndFunc
;I have ported the script to use the new GWA2 library. Please test it and let me know if you find any issues.
;I have ported the script to use the new GWA2 library. Please test it and let me know if you find any issues.

Func AvgRunTime()
	Local $CurrentRunTime = Floor(TimerDiff($nCurrentRunTime))
	$CumulatedTime += $CurrentRunTime
	Out("Cumulated Time: " & $CumulatedTime)
	Out("Run Counter: " & $GUI_RunCounter)
	$AvgRunTime = $CumulatedTime / ($GUI_RunCounter-$GUI_FailCounter)

	Local $iHours, $iMins, $iSecs
	Local $TimeStamp = ""
	_TicksToTime($AvgRunTime, $iHours, $iMins, $iSecs)
	If $iHours < 10 Then $TimeStamp = "0"
	$TimeStamp &= $iHours & ":"
	If $iMins < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iMins & ":"
	If $iSecs < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iSecs
	Out($TimeStamp)
	Return $TimeStamp
EndFunc

Func BestRunTime()
	If TimerDiff($nCurrentRunTime) < $nBestRunTime Then $nBestRunTime = TimerDiff($nCurrentRunTime)
	Return $nBestRunTime
EndFunc

Func UpdateStats()
	GUI_SetVanguard(GetVanguardTitle() - $iVanguardTitle)
	GUI_SetNorn(GetNornTitle() - $iNornTitle)
	GUI_SetAsura(GetAsuraTitle() - $iAsuraTitle)
	GUI_SetDeldrimor(GetDeldrimorTitle() - $iDeldrimorTitle)
	GUI_SetLockpicks(GetPicksCount())
EndFunc

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

Func HasEffect($aEffectSkillID, $aHeroNumber = 0, $aHeroId = GetHeroID($aHeroNumber))
	Return GetSkillEffectPtr($aEffectSkillID, $aHeroNumber, $aHeroId) <> 0
EndFunc

Func GetSkillEffectPtr($aSkillID, $aHeroNumber = 0, $aHeroId = GetHeroID($aHeroNumber))
	Local $lOffset[4] = [0, 24, 44, 1296]
	Local $lCount = MemoryReadPtr($mBasePointer, $lOffset)
	ReDim $lOffset[5]
	$lOffset[3] = 1288
	Local $lBuffer
	For $i = 0 To $lCount[1] - 1
		$lOffset[4] = 36 * $i
		$lBuffer = MemoryReadPtr($mBasePointer, $lOffset)
		If $lBuffer[1] = $aHeroId Then
			$lOffset[4] = 28 + 36 * $i
			Local $lEffectCount = MemoryReadPtr($mBasePointer, $lOffset)
			$lOffset[4] = 20 + 36 * $i
			Local $lEffectStructAddress = MemoryReadPtr($mBasePointer, $lOffset, 'ptr')
			For $J = 0 To $lEffectCount[1] - 1
				Local $lEffectSkillID = MemoryRead($lEffectStructAddress[1] + 24 * $J, 'long')
				If $lEffectSkillID = $aSkillID Then Return Ptr($lEffectStructAddress[1] + 24 * $J)
			Next
		EndIf
	Next
	Return 0
EndFunc

Func CountFreeSlots($NumOfBags = 4)
	Local $lCount = 0
	Local $lBagPtr
	For $lBag = 1 To $NumOfBags
		$lBagPtr = GetBagPtr($lBag)
		If $lBagPtr = 0 Then ContinueLoop
		$lCount += MemoryRead($lBagPtr + 32, "long") - MemoryRead($lBagPtr + 16, "long")
	Next
	Return $lCount
EndFunc

; ============================================================
; Functions added for Resign and Return capability
; ============================================================

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
EndFunc

Func TravelTo($aMapID, $aLanguage = -1, $aRegion = -1)
	If $aLanguage = -1 Then $aLanguage = GetLanguage()
	If $aRegion = -1 Then $aRegion = GetRegion()
	
	If GetMapID() = $aMapID And GetLanguage() = $aLanguage And GetMapLoading() = 2 Then
		Out("Already at destination")
		Return True
	EndIf

	If Not IsDeclared("HEADER_PARTY_TRAVEL") Then Global Const $HEADER_PARTY_TRAVEL = 0x00B0
	
	SendPacket(0x18, $HEADER_PARTY_TRAVEL, $aMapID, $aRegion, 0, $aLanguage, False)
	WaitMapLoading($aMapID)
EndFunc

; Helper for Logic Port
; Helper for Logic Port
Func GetSkillPtr($aSkillID)
	; Return actual memory address of skill in game memory (not a copied struct)
	; This matches GWA_Logic: $mSkillBase + 160 * $aSkillID
	; 0xA4 = 164 bytes per skill (from GWA2: 0xA4 * $skillID)
	Return Ptr($skill_base_address + 0xA4 * $aSkillID)
EndFunc

; Ported Helper: GetHP (Percentage)
Func GetHP($aAgent = -2)
	Return MemoryRead(GetAgentPtr($aAgent) + 304, 'float')
EndFunc

; Ported Helper: GetIsKnocked Wrapper
Func IsKnocked($aAgent = -2)
	Return GetIsKnocked($aAgent)
EndFunc

; Ported Helper: IsPressureSpiritSkill


; Ported Helper: IsDisguiseskill


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

; Ported Helper: IsSkillType
Func IsSkillType($aSkill, $aType)
	Return DllStructGetData(GetSkillByID($aSkill), 'Type') = $aType
EndFunc

; Ported Helper: IsWeaponRange
Func IsWeaponRange($aSkill)
    Local $lRange = MemoryRead(GetSkillPtr($aSkill) + 96, "long") ; Range offset
    Return $lRange > 0
EndFunc

; Ported Helper: Wipe
Func Wipe($aPartyPtr = GetParty())
	Local $lDeadCount = 0
	Local $lPartySize = UBound($aPartyPtr) - 1
	For $i = 1 To $lPartySize
		If GetIsDead($aPartyPtr[$i]) Then $lDeadCount += 1
	Next
	If $lDeadCount = $lPartySize Then Return True
	Return False
EndFunc

; Ported Helper: GetPartyHealth
Func GetPartyHealth($aParty = GetParty())
	Local $lHealth = 0
	For $i = 1 To $aParty[0]
		$lHealth += GetHP($aParty[$i])
	Next
	Return $lHealth / $aParty[0]
EndFunc


#Region Logic_Port
; Ported Logic from GWA_Logic_Censured_NEW.au3
; Including Smart Casting, Fight, and CacheSkillBar


Global $Skillbar[9] = [0, 1, 2, 3, 4, 5, 6, 7, 8]
Global $SkillBarCache[9][20] ; Increased size for cache
Global $SkillbarSlot[10000]

; Helper Functions from Froggy (Restored for Logic)



; Implemented GetBestTargetPtr (was missing in source)
Func GetBestTargetPtr($aRange = 1350, $casting = False, $nohex = False, $enchanted = False)
    Local $lAgentArray = GetAgentArray(0xDB) ; 0xDB = All living NPCs
    If Not IsArray($lAgentArray) Then Return 0
    
    Local $lBestDist = 99999
    Local $lBestPtr = 0
    Local $lMe = GetAgentByID(-2)
    Local $lEnemiesInRange = 0
    
    For $i = 1 To $lAgentArray[0]
        Local $lAgent = $lAgentArray[$i]
        If GetIsDead($lAgent) Then ContinueLoop
        
        ; CRITICAL: Filter for enemies only (Allegiance = 3 = FOE)
        Local $lAllegiance = DllStructGetData($lAgent, 'Allegiance')
        If $lAllegiance <> 3 Then ContinueLoop  ; Skip non-enemies
        
		Local $lDist = GetDistance(GetMyAgent(), $lAgent)
		If $lDist > $aRange Then ContinueLoop
		$lEnemiesInRange += 1
		Local $lID = DllStructGetData($lAgent, 'ID')
		
		; Apply filters
		If $casting And Not GetIsCasting($lID) Then ContinueLoop
		If $nohex And GetHasHex($lID) Then ContinueLoop
		If $enchanted And Not GetHasEnchantment($lID) Then ContinueLoop
		
		If $lDist < $lBestDist Then
			$lBestDist = $lDist
			$lBestPtr = $lAgent
		EndIf
    Next
    Return $lBestPtr
EndFunc

Func GetBestMeleeTarget($aRange = 250)
	Return GetBestTargetPtr($aRange) ; Simplified
EndFunc

Func GetNearestSpiritPtrToAgent($aAgent = -2)
	; Simplified implementation
	Return GetNearestNPCToCoords(GetX($aAgent), GetY($aAgent)) ; Placeholder
EndFunc

Func GetNearestMinionPtrToAgent($aAgent = -2)
	Return 0 ; Placeholder
EndFunc

Func GetNearestDeadAllyPtrToAgent($aAgent = -2)
	; Simplified
	Local $party = GetParty()
    For $i = 0 To UBound($party) - 1
        if GetIsDead($party[$i]) Then Return $party[$i]
    Next
    Return 0
EndFunc

; Copied UseSkillEX
Func UseSkillSmart($aSkillSlot, $aTarget = -2, $aTimeout = 6000, $aSkillbarPtr = 0)
	Local $lDeadlock = TimerInit(), $lAgentID = ID($aTarget)
	If $lAgentID = 0 Or GetIsDead(-2) Then Return
	If $lAgentID <> GetMyID() Then ChangeTarget($aTarget)
	UseSkill($aSkillSlot, $aTarget)
	Do
		Sleep(50)
		If GetIsDead($aTarget) Then Return
		If GetEnergy(-2) < $SkillBarCache[$aSkillSlot][$energyreq] Then Return
	Until Not CanCast($aSkillSlot) Or TimerDiff($lDeadlock) > $aTimeout
	Sleep(MemoryRead(GetSkillPtr($SkillbarSlot[$aSkillSlot]) + 64, "float") * 1000) ; Aftercast
	Return True
EndFunc

Func Fight($aAggroRange = 1000, $careful = False) ; Fighting mechanics
	Out("Fighting enemies")
	Local $TimerToGetOut = TimerInit()
	Local $nearDist = 0
	Do
		If $careful Then CancelAll()
		Local $canAtk = CanAttack($aAggroRange)
		If $canAtk Then 
			Attack($BestTargetPtr, True)
		EndIf
		Sleep(100)
		If $careful Then
			Move(X($BestTargetPtr), Y($BestTargetPtr))
			Sleep(300)
		EndIf
		UseSkills($aAggroRange, $all)
		$nearDist = GetNearestEnemyDistance()
	Until $nearDist > $aAggroRange Or GetIsDead(-2) Or Wipe() Or TimerDiff($TimerToGetOut) > 240000
	PickupLootEx(3000)
EndFunc

Func UseSkills($aAggroRange = 1000, $skilltype = $all)
	For $aSkillSlot = 1 To 8
		If GetIsDead(-2) Or Wipe() = 1 Or GetMapLoading() == 2 Then ExitLoop
		If $SkillBarCache[$aSkillSlot][$skilltype] = "" Then ContinueLoop
		If CanUse($aSkillSlot, $aAggroRange) Then UseSkillSmart($aSkillSlot, $BestTargetPtr)
		If GetNearestEnemyDistance() > $aAggroRange Then Return
	Next
EndFunc

Func CanCast($aSkillSlot = 0)
	If GetMapLoading() == 2 Then Disconnected()
	If GetMapLoading() <> 1 Then Return False  ; Can only cast in explorable areas
	If IsKnocked() Or GetIsDead(-2) Or Wipe() = 1 Then Return False
	If $aSkillSlot <> 0 And Not IsRecharged($aSkillSlot) Then Return False
	Local $aType = $SkillBarCache[$aSkillSlot][$type]
	If $aSkillSlot = 0 Then $aType = $Attack

	Switch $aType
		Case $Hex, $Spell, $Enchantment, $Well, $Ward, $ItemSpell, $WeaponSpell
            If AgentHasEffect($Diversion) <> 0 Then Return False
            If AgentHasEffect($Visions_of_Regret) <> 0 Then Return False
            If AgentHasEffect($Visions_of_Regret_PvP) <> 0 Then Return False
            If AgentHasEffect($Backfire) <> 0 Then Return False
            If AgentHasEffect($Soul_Leech) <> 0 Then Return False
            If AgentHasEffect($Mistrust) <> 0 Then Return False
            If AgentHasEffect($Mistrust_PvP) <> 0 Then Return False
            If AgentHasEffect($Mark_of_Subversion) <> 0 Then Return False
            If AgentHasEffect($Spiteful_Spirit) <> 0 Then Return False
		Case $Attack
			If AgentHasEffect($Ineptitude) + AgentHasEffect($Clumsiness) + AgentHasEffect($Spiteful_Spirit) + AgentHasEffect($Wandering_Eye) + AgentHasEffect($Wandering_Eye_PvP) <> 0 Then
				Out("Can't Attack")
				Return False
			EndIf
		Case $Ritual, $Signet, $Glyph, $Shout, $Preparation, $Trap, $Chant, $EchoRefrain, $Disguise
			If AgentHasEffect($Diversion) Then Return False
		Case $Shout, $Chant
			If AgentHasEffect($Well_of_Silence) Then Return False
		Case $Signet
			If AgentHasEffect($Ignorance) Then Return False
	EndSwitch
	Return True
EndFunc

Func CanAttack($aRange = 1320)
	$BestTargetPtr = GetBestTargetPtr($aRange)
	If $BestTargetPtr = 0 Then Return False
	If CanCast() Then Return True
	Return False
EndFunc

Func CanUse($aSkillSlot, $aAggroRange = 1320)
	Local $ZephyrEffect = $SkillBarCache[$aSkillSlot][$energyreq] * 30 / 100
	Local $ZephyrAddition = $SkillBarCache[$aSkillSlot][$energyreq] + $ZephyrEffect

	If $aSkillSlot = "" Then Return
	If Not CanCast($aSkillSlot) Then Return False
	If GetBestTargetBySkillSlot($aSkillSlot, $aAggroRange) = 0 Then Return False
	If Not IsRecharged($aSkillSlot) Then Return False
	If GetEnergy(-2) < $SkillBarCache[$aSkillSlot][$energyreq] Then Return False
	If AgentHasEffect($Quickening_Zephyr, -2) And GetEnergy(-2) < $ZephyrAddition Then Return False
	If $SkillBarCache[$aSkillSlot][$adrereq] <> 0 And GetAdrenaline($aSkillSlot) < $SkillBarCache[$aSkillSlot][$adrereq]  Then Return False

;~ BINDING RITUALS
	If $SkillBarCache[$aSkillSlot][$bind] <> "" Then
		Switch $SkillBarCache[$aSkillSlot][$bind]     ; binding ritual skills
			Case $Summon_Spirits_Kurzick, $Summon_Spirits_Luxon
				If GetNumberOfEnemies($aAggroRange) = 0 Then Return False ; NumberOfPressureSpirits not impl
		EndSwitch
	EndIf

;~ SURVIVAL SKILLS
	If $SkillBarCache[$aSkillSlot][$survive] <> "" Then
		Switch $SkillBarCache[$aSkillSlot][$survive]
			Case $I_Am_Unstoppable
				If GetEffectTimeRemaining($Shadow_Form) > 5000 And Not GetIsKnocked(-2) Then Return False
			Case $Glyph_of_Swiftness
				If GetEffectTimeRemaining($Shadow_Form) > 5000 Then Return False
				If GetSkillbarSkillRecharge($SkillbarSlot[$Shadow_Form]) > 5000 Then Return False
			Case $Shadow_Form
				If GetEffectTimeRemaining($Shadow_Form) > 5000 Then Return False
				If GetEffectTimeRemaining($Glyph_of_Swiftness) = 0 Then Return False
			Case $Shroud_Of_Distress
				If GetHP(-2) > 0.9 Then Return False
				If GetEffectTimeRemaining($Shroud_Of_Distress) > 5000 Then Return False
			Case $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick, $Shadow_Refuge
				If GetHP(-2) > 0.7 Then Return False
			Case $Heart_of_Shadow
				If GetHP(-2) > 0.5 Then Return False
			Case $Mystic_Regeneration
				If GetEffectTimeRemaining($Mystic_Regeneration) < 4000 Then Return True
			Case $Shield_of_Judgment
				If GetEffectTimeRemaining($Shielding_Hands) < 4500 And GetEffectTimeRemaining($Shield_of_Absorption) < 4500 Then Return False

		EndSwitch
	EndIf

;~ PRESSURE SKILLS
	If $SkillBarCache[$aSkillSlot][$pressure] <> "" Then
		Switch $SkillBarCache[$aSkillSlot][$pressure]
			Case $Finish_Him
				If DllStructGetData(GetAgentByID(ID($BestTargetPtr)), 'Health') > 0.45 Then Return False
		EndSwitch
	EndIf

;~ HEAL SKILLS
	If $SkillBarCache[$aSkillSlot][$heal] <> "" Then
		$lowestally = GetLowestAlly()
		If IsHealSkill($SkillBarCache[$aSkillSlot][$heal]) And GetHP($lowestally) > 0.8 Then Return False
	EndIf

	Return True
EndFunc

Func GetBestTargetBySkillSlot($aSkillSlot, $aAggroRange = 1320)
	Local $MyPtr = GetAgentByID(-2)  ; Use GetAgentByID instead of GetAgentPtr (GWA2 doesn't handle -2)
	Local $targetType = $SkillBarCache[$aSkillSlot][$target]
	Switch $targetType
		Case 0	; self
			If $SkillBarCache[$aSkillSlot][$type] == $Ward And GetDistance(GetMyAgent(), GetNearestEnemyToAgent(GetMyAgent())) > $aAggroRange Then Return False
			$BestTargetPtr = $MyPtr
		Case 1	; spirit, minion
			$BestTargetPtr = GetNearestSpiritPtrToAgent()
		Case 3	; ally
			If $SkillBarCache[$aSkillSlot][$condremove] <> "" Then
				$BestTargetPtr = MostCondsAllyPtr()
			ElseIf $SkillBarCache[$aSkillSlot][$hexremove] <> "" Then
				$BestTargetPtr = MostHexedAllyPtr()
			ElseIf $SkillBarCache[$aSkillSlot][$precast] <> "" Then
				$BestTargetPtr = $MyPtr
			ElseIf $SkillBarCache[$aSkillSlot][$survive] <> "" Then
				$BestTargetPtr = $MyPtr
			ElseIf $SkillBarCache[$aSkillSlot][$echoes] <> "" Then
				$BestTargetPtr = NeedEchoAlly($SkillBarCache[$aSkillSlot][$echoes])
			Else
				$BestTargetPtr = GetLowestAlly()
			EndIf
		Case 4	; other ally
			If $SkillBarCache[$aSkillSlot][$condremove] <> "" Then
				$BestTargetPtr = MostCondsAllyPtr(True)
			ElseIf $SkillBarCache[$aSkillSlot][$hexremove] <> "" Then
				$BestTargetPtr = MostHexedAllyPtr(True)
			ElseIf $SkillBarCache[$aSkillSlot][$precast] <> "" Then
				$BestTargetPtr = $MyPtr
			ElseIf $SkillBarCache[$aSkillSlot][$survive] <> "" Then
				$BestTargetPtr = $MyPtr
			ElseIf $SkillBarCache[$aSkillSlot][$echoes] <> "" Then
				$BestTargetPtr = NeedEchoAlly($SkillBarCache[$aSkillSlot][$echoes])
			Else
				$BestTargetPtr = GetLowestAlly(True)
			EndIf

		Case 5	; enemy,
			If $SkillBarCache[$aSkillSlot][$hexes] <> "" Then
				$BestTargetPtr = GetNoHexEnemy($aAggroRange)
				If $BestTargetPtr = 0 Then $BestTargetPtr = GetBestTargetPtr($aAggroRange)
			ElseIf $SkillBarCache[$aSkillSlot][$enchantremove] <> "" Then
				$BestTargetPtr = GetBalledEnchantedEnemy($aAggroRange)
			ElseIf $SkillBarCache[$aSkillSlot][$attackskill] <> "" Then
				$BestTargetPtr = GetBestMeleeTarget()
			ElseIf $SkillBarCache[$aSkillSlot][$rupt] <> "" Then
				$BestTargetPtr = GetMostBalledCastingEnemy($aAggroRange)    ; finds only enemies that are casting a spell
			Else
				$BestTargetPtr = GetBestTargetPtr($aAggroRange)
			EndIf
		Case 6	; dead ally
			$BestTargetPtr = GetNearestDeadAllyPtrToAgent()
		Case 14	; spirit, minion
			$BestTargetPtr = GetNearestMinionPtrToAgent()
		EndSwitch

	If $BestTargetPtr <> 0 Then Return $BestTargetPtr
	Return 0
EndFunc

Func GetLowestAlly($excludeself = False)
	Local $lLowestally = 0, $lLowestHP = 1.0
	Local $lAgentArray = GetAgentArray(0xDB)
	For $i = 1 To $lAgentArray[0]
		If DllStructGetData($lAgentArray[$i], 'Allegiance') <> 1 Then ContinueLoop
		If DllStructGetData($lAgentArray[$i], 'HP') <= 0 Then ContinueLoop
		If $excludeself And ID($lAgentArray[$i]) = ID(-2) Then ContinueLoop
		$lHP = GetHP($lAgentArray[$i])
		If $lHP < $lLowestHP Then
			$lLowestally = $lAgentArray[$i]
			$lLowestHP = $lHP
		EndIf
	Next
	Return $lLowestally
EndFunc

Func GetNoHexEnemy($aRange = 1320)
	Return GetBestTargetPtr($aRange, False, True)
EndFunc

Func GetBalledEnchantedEnemy($aRange)
	Return GetBestTargetPtr($aRange, False, False, True)
EndFunc

Func GetMostBalledCastingEnemy($aRange = 1320)
	Return GetBestTargetPtr($aRange, True)
EndFunc

Func NeedEchoAlly($aSkillID)
	Return GetAgentByID(-2) ; Simplified
EndFunc

Func MostCondsAllyPtr($excludeself = False)
    Local $MostConditionedAlly = 0
    Local $lMostConditions = 0
    For $aHeroNumber = 0 To GetPartySize()-1
       If $excludeself = True And $aHeroNumber = 0 Then ContinueLoop
		; Logic simplified: assume random ally if complex effect checking fails
		$MostConditionedAlly = GetHeroID($aHeroNumber)
	Next
    Return $MostConditionedAlly ; Placeholder
EndFunc

Func MostHexedAllyPtr($excludeself = False)
	Local $lMostHexedally = 0
	For $aHeroNumber = 0 To GetPartySize()-1
		If $excludeself = True And $aHeroNumber = 0 Then ContinueLoop
		$lMostHexedally = GetHeroID($aHeroNumber)
	Next
	Return $lMostHexedally ; Placeholder
EndFunc

Func CacheSkillBar()
	Out("Mapping your skill bar")
	$PressureSpiritSkills = 0
	Sleep(200)
	For $i = 1 To 8
		Local $aSkillID = GetSkillbarSkillID($i)
		If $aSkillID = 0 Then ContinueLoop
		$SkillbarSlot[$aSkillID] = $i
		$SkillBarCache[$i][$all] = $aSkillID
		$SkillBarCache[$i][$ptr] = GetSkillPtr($aSkillID)
		$SkillBarCache[$i][$energyreq] = MemoryRead($SkillBarCache[$i][$ptr] + 28, 'long') ; Offset guess
		$SkillBarCache[$i][$adrereq] = MemoryRead($SkillBarCache[$i][$ptr] + 56, 'dword')
		$SkillBarCache[$i][$type] = MemoryRead($SkillBarCache[$i][$ptr] + 12, "long")
		$SkillBarCache[$i][$target] = MemoryRead($SkillBarCache[$i][$ptr] + 49, "byte")
		
		If IsHexSpell($aSkillID) Then $SkillBarCache[$i][$hexes] = $aSkillID
		If IsPressureSkill($aSkillID) Then $SkillBarCache[$i][$pressure] = $aSkillID
		If IsSurvivalSkill($aSkillID) Then $SkillBarCache[$i][$survive] = $aSkillID
		If IsAttackSkill($aSkillID) Then $SkillBarCache[$i][$attackskill] = $aSkillID
		If IsHealSkill($aSkillID) Then $SkillBarCache[$i][$heal] = $aSkillID
		If IsBondSkill($aSkillID) Then $SkillBarCache[$i][$bond] = $aSkillID
		If IsCondRemoveSkill($aSkillID) Then $SkillBarCache[$i][$condremove] = $aSkillID
		If IsHexRemoveSkill($aSkillID) Then $SkillBarCache[$i][$hexremove] = $aSkillID
		If IsEnchantRemoveSkill($aSkillID) Then $SkillBarCache[$i][$enchantremove] = $aSkillID
		If IsPrecastSkill($aSkillID) Then $SkillBarCache[$i][$precast] = $aSkillID
		If IsChantSkill($aSkillID) or IsShoutSkill($aSkillID) Then $SkillBarCache[$i][$chantsnshouts] = $aSkillID
		If IsEchoRefrainskill($aSkillID) Then $SkillBarCache[$i][$echoes] = $aSkillID
	Next
	Out("Mapping your skill bar - completed")
	Return True
EndFunc




Func IsWeaponSpell($aSkill)
    Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
    If Not IsDllStruct($skillStruct) Then Return False
	Return IsSkillType($aSkill, $Type_Attack) And IsWeaponRange($aSkill)
EndFunc

Func IsEnchantmentSkill($aSkill)
    Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Enchantment
EndFunc

Func IsAttackSkill($aSkill)
    Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Type_Attack
EndFunc

Func IsShoutSkill($aSkill)
    Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Shout
EndFunc

Func IsEchoRefrainskill($aSkill)
    Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $EchoRefrain
EndFunc



Func IsChantSkill($aSkill)
    Local $skillStruct = IsDllStruct($aSkill) ? $aSkill : GetSkillByID($aSkill)
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Chant
EndFunc

Func IsHealSkill($aSkill)
	Switch MemoryRead(GetSkillPtr($aSkill) + 32, 'long')	; Effect 2
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
EndFunc

; -------------------

Func IsPartyHealSkill($aSkill)    ; incomplete list, add based on your needs
	Switch $aSkill
		Case $Divine_Healing, $Heal_Party, $Heavens_Delight, $Protective_Was_Kaolai, $Mystic_Healing, $Mystic_Healing_PvP
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsPartyHealSkill

Func IsSelfPrehealSkill($aSkill)    ; contains only instant, non condititional, self-targeted heals
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

Func IsBondSkill($aSkill)
	Switch $aSkill
		Case $Balthazars_Spirit, $Essence_Bond, $Life_Barrier, $Life_Bond, $Mending, $Protective_Bond, $Purifying_Veil, $Retribution, $Strength_of_Honor, $Succor
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsBondSkill

;~ Tests if a skill is a condition removal skill - to be expanded
Func IsCondRemoveSkill($aSkill)
	Switch $aSkill
		Case $mend_ailment, $purge_conditions, $mending_touch, $Mend_Body_and_Soul, $Spotless_Soul
			Return True
	EndSwitch
	If IsHexAndConditionRemoveSkill($aSkill) Then Return True
	Return False
EndFunc   ;==>IsCondRemoveSkill

;~ Tests if a skill is a hex removal skill
Func IsHexRemoveSkill($aSkill)
	Switch $aSkill
		Case $Smite_Hex, $Divert_Hexes, $Cure_Hex, $Hex_Eater_Vortex, $Shatter_Hex, $Reverse_Hex, $Remove_Hex, $Expel_Hexes, $Inspired_Hex, $Revealed_Hex, $Spotless_Mind, $Convert_Hexes, $Deny_Hexes; remove hex (me and ally)
        Case $Hex_Eater_Signet, $Withdraw_Hexes, $Hexbreaker_Aria, $holy_veil, $Pious_Restoration
			Return True
	EndSwitch
	If IsHexAndConditionRemoveSkill($aSkill) Then Return True
	Return False
EndFunc   ;==>IsHexRemoveSkill

Func IsHexAndConditionRemoveSkill($aSkillID)
	Switch $aSkillID
		Case $Peace_and_Harmony, $Empathic_Removal, $Blessed_Light, $Contemplation_of_Purity, $Purge_Signet, $Signet_of_Removal
			Return True
	EndSwitch
	Return False
EndFunc   ;==>IsHexAndConditionRemoveSkill

Func IsEnchantRemoveSkill($aSkill)
	Return False
EndFunc

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

Func IsPressureSkill($aSkill)
    If IsConditionSpell($aSkill) Then Return True
    If IsHexSpell($aSkill) Then Return True
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
    Switch $skillID
        Case $Ebon_Vanguard_Assassin_Support, $Ebon_Battle_Standard_Of_Honor, $Finish_Him, $Dark_Pact
            Return True
    EndSwitch
    Return False
EndFunc

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

Func IsSurvivalSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $I_Am_Unstoppable, $Shadow_Form, $Shroud_of_Distress, $Glyph_of_Swiftness, $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick _
				, $Heart_of_Shadow, $Way_Of_The_Master, $Shadow_Refuge, $Protective_Spirit, $Shield_of_Absorption, $Shielding_Hands _
				, $Mystic_Regeneration, $Shield_of_Judgment, $Spirit_Bond, $Zealots_Fire
			Return True
	EndSwitch
	Return False
EndFunc

; Returns whether a skill is a hex spell
Func IsHexSpell($aSkill)
	If IsPtr($aSkill) <> 0 Then
		Return MemoryRead($aSkill + 12, "long") = $Hex
	ElseIf IsDllStruct($aSkill) <> 0 Then
		Return DllStructGetData($aSkill, "Type") = $Hex
	Else
		Return MemoryRead(GetSkillPtr($aSkill) + 12, "long") = $Hex
	EndIf
EndFunc   ;==>IsHexSpell

; Returns whether a skill is a condition spell
Func IsConditionSpell($aSkill)
	If IsPtr($aSkill) <> 0 Then
		Return MemoryRead($aSkill + 12, "long") = $Condition
	ElseIf IsDllStruct($aSkill) <> 0 Then
		Return DllStructGetData($aSkill, "Type") = $Condition
	Else
		Return MemoryRead(GetSkillPtr($aSkill) + 12, "long") = $Condition
	EndIf
EndFunc   ;==>IsConditionSpell

Func IsRuptSkill($aSkill, $hardrupt = False)
	Local $lSkillEffect2 = MemoryRead(GetSkillPtr($aSkill) + 32, 'long')
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

Func IsHardRuptSkill($aSkillID)
	Return IsRuptSkill($aSkillID, True)
EndFunc   ;==>IsHardRuptSkill

Func IsDisguiseskill($aSkill)
	If IsPtr($aSkill) <> 0 Then
		Return MemoryRead($aSkill + 12, "long") = $Disguise
	ElseIf IsDllStruct($aSkill) <> 0 Then
		Return DllStructGetData($aSkill, "Type") = $Disguise
	Else
		Return MemoryRead(GetSkillPtr($aSkill) + 12, "long") = $Disguise
	EndIf
EndFunc   ;==>IsDisguiseskill

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
EndFunc



; ============================
; Helper Functions for Compatibility
; ============================

Func ID($aAgent = -2)
	; Handle special values first
	If $aAgent = -2 Then Return GetMyID()
	; Handle pointers (from GetAgentPtr)
	If IsPtr($aAgent) Then
		Return MemoryRead($aAgent + 44, 'long')
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

; Ported Logic Helpers
; -------------------

Func GetHasEnchantment($aAgent)
	; Alias for GetIsEnchanted logic
	Return BitAND(MemoryRead(GetAgentPtr($aAgent) + 312, "long"), 0x0080) > 0
EndFunc

Func GetEffectsPtr($aSkillID = 0, $aHeroNumber = 0, $aHeroId = GetHeroID($aHeroNumber))
	Local $lEffectCount, $lEffectStructAddress, $lBuffer
	Local $lOffset[4] = [0, 24, 44, 1296]
	Local $lCount = MemoryReadPtr($mBasePointer, $lOffset)
	ReDim $lOffset[5]
	$lOffset[3] = 1288
	For $i = 0 To $lCount[1] - 1
		$lOffset[4] = 36 * $i
		$lBuffer = MemoryReadPtr($mBasePointer, $lOffset)
		If $lBuffer[1] = $aHeroId Then
			$lOffset[4] = 28 + 36 * $i
			$lEffectCount = MemoryReadPtr($mBasePointer, $lOffset)
			$lOffset[4] = 20 + 36 * $i
			$lEffectStructAddress = MemoryReadPtr($mBasePointer, $lOffset, 'ptr')
			If $aSkillID = 0 Then Return $lEffectStructAddress[1]
			; Logic for specific ID omitted for basic Ptr return
		EndIf
	Next
	Return 0
EndFunc

Func IsSelfHealSkill($aSkill)    ; healing skills that can be used on self
	Return IsPartyHealSkill($aSkill) And MemoryRead(GetSkillPtr($aSkill) + 49, "byte") <> 4
EndFunc

Func IsSelfOnlyHealSkill($aSkill)    ; healing skills that can be used on self
	Return IsPartyHealSkill($aSkill) And MemoryRead(GetSkillPtr($aSkill) + 49, "byte") = 0
EndFunc

Func IsHealOtherSkill($aSkill)    ; healing skills that can only be used on others
	Return IsPartyHealSkill($aSkill) And MemoryRead(GetSkillPtr($aSkill) + 49, "byte") = 4
EndFunc

Func IsHealMySelfSkill($aSkill)
	Local $lSkillType = MemoryRead(GetSkillPtr($aSkill) + 12, "long")
	Return BitAND($lSkillType, 4)
EndFunc

Func IsHealAllySkill($aSkill)
	Local $lSkillType = MemoryRead(GetSkillPtr($aSkill) + 12, "long")
	Return BitAND($lSkillType, 2)
EndFunc

Func IsHexRemovalSkill($aSkill)
	Return IsHexRemoveSkill($aSkill)
EndFunc

Func IsConditionRemovalSkill($aSkill)
	Return IsCondRemoveSkill($aSkill)
EndFunc

Func IsConditionAndHexRemovalSkill($aSkill)
	Return IsHexAndConditionRemoveSkill($aSkill)
EndFunc



Func GetNumberOfEnemies($aRange = 1200)
	Local $count = 0
	Local $lAgentArray = GetAgentArray(0xDB)
	If Not IsArray($lAgentArray) Then Return 0
	For $i = 1 To $lAgentArray[0]
		If GetIsDead($lAgentArray[$i]) Then ContinueLoop
		If GetDistance(GetMyAgent(), $lAgentArray[$i]) < $aRange Then $count += 1
	Next
	Return $count
EndFunc

Func GetAdrenaline($aSkillSlot)
    Local $aSkillbarPtr = GetSkillbarPtr()
    If $aSkillbarPtr = 0 Then Return 0
    $aSkillSlot -= 1
    Return MemoryRead($aSkillbarPtr + 4 + $aSkillSlot * 20, "long")
EndFunc




; Ported Dependency for GetAdrenaline
Func GetSkillbarPtr($aHeroNumber = 0)
    Local $lOffset[5] = [0, 24, 76, 84, 44]
    Local $lHeroCount = MemoryReadPtr($mBasePointer, $lOffset)
    Local $lOffset2[5] = [0, 24, 44, 1776, 0] ; Re-dimensioned for 5 elements
    For $i = 0 To $lHeroCount[1]
        $lOffset2[4] = $i * 188
        Local $lSkillbarStructAddress = MemoryReadPtr($mBasePointer, $lOffset2)
        If $lSkillbarStructAddress[1] = GetHeroID($aHeroNumber) Then Return $lSkillbarStructAddress[0]
    Next
    Return 0
EndFunc

; Counts Lockpicks in your inventory
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


#EndRegion Logic_Port

