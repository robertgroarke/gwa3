
#requireadmin
#include "GWA_Logic_Censured_NEW.au3"
Initialize(CharacterSelector(), True, True, False)

Global Const $BOTNAME 			= "Froggy"
Global Const $AUTHORS 			= ["bob version | reaverseath/logicdoor"]
Global Const $VERSION 			= "1.6"
Global $aPlayerAgent			= GetAgentByID(-2)

GUI_Create()

Global $gReturnMap = $Gadds_Encampment
Global $gPickupCoins = True

Local $BotRunning 			= False

GUI_SetOnStartFunc("onStart")
GUI_SetOnStopFunc("onStop")
GUI_SetOnResumeFunc("onResume")

While 1
	Sleep(200)
	While $BotRunning
		PingSleep(250)

		$OpenedChestAgentIDs[0] = ""
		ReDim $OpenedChestAgentIDs[1]
		Switch GetMapID()
			Case $explorable
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

Func Setup($addHeroes = True)

	If GetMapID() <> 638 Then
		TravelTo(638)
	Endif
	PingSleep(1000)
	
	If($addHeroes) Then
		LeaveGroup()
	sleep(500)
	AddHero($HERO_ID_MERCENARY_3)
	;AddHero(25) ;Xandra
	AddHero(14) ;Olias
	AddHero(21) ; Livia
	AddHero(4) ; Master of Whispers
	AddHero(24) ; Gwen
	AddHero(15) ; Norgu
	;AddHero(1); Razah
	AddHero($HERO_ID_MERCENARY_2)

	sleep(500)

	LoadSkillTemplate("OAOiAyk8gNtePuwJ00ZaNbJA", 1) ; ST Xandra remove hex
	LoadSkillTemplate("OAhjQkGZIT3BVVCPSTTODTjTciA", 2) ; BiP Olias
	LoadSkillTemplate("OAhjYoHYIPWb7wnoqKNncDzqH", 3) ; Xinrae Livia
	LoadSkillTemplate("OAljUwGpZSUBKgfBVVbh8Y7Y1YA", 4) ; MM Master P
	LoadSkillTemplate("OQhkAsC8gFKzJY6lDMd40hQG4iB", 5)	; E-Surge Gwen
	LoadSkillTemplate("OQhkAsC8gFKDNY6lDMd40hQG4iB", 6) ; Inep Norgu
	LoadSkillTemplate("OQljAkBsZSvAIg5ZkAcQsA7Y1YA", 7)	; Panic Razah

	sleep(500)

		For $i = 1 To 2
			SetHeroAggression($i, 1) ;0=Fight, 1=Guard, 2=Avoid
			sleep(100)
		Next
		For $i = 3 To 6
			SetHeroAggression($i, 1) ;0=Fight, 1=Guard, 2=Avoid
			sleep(100)
		Next

		SetHeroAggression(6,1)
	EndIf

	SwitchMode(2)

	Out("Let's GO")
	MoveTo(-10018, -21892)
	MoveTo(-9550, -20400)
	TolSleep(500)
	Do
		Move(-9451, -19766)
	Until WaitMapLoading($iSplarkflyMapID)

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
	MoveTo(12396, 22407)
	Out("Accept Quest")
	
	Local $NPC = GetNearestNPCPtrToXY (12396, 22407)
	NPCHook($NPC)
	DialogHook($TekksDialog)
	QuestReward($hTekksWar) ; in case for some reason we didnt find tekks after chest
	AcceptQuest($hTekksWar)
	DialogHook(0x2AE6)
	Dialog(0x833905)
	
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

	$Me = GetAgentByID()

	If $NearestWaypoint = 0 then
	   Out("Start at "& $aWaypointsLevel2[$NearestWaypoint][3])
	   AggroMoveToEx(-11330, -5483)
	   MoveTo(-11132, -5546)
	   GetDwarvenBlessing(-11132, -5546)
    Else
	   Out("Start at " & $aWaypointsLevel2[$NearestWaypoint][3])
    Endif
	MoveandAggroEx($aWaypointsLevel2)
	
	Local $Timer = TimerInit()
	Do
		Sleep(200)
	Until WaitMapLoading($iSplarkflyMapID) Or TimerDiff($Timer) >= 360000 


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
	$BogrootChest = GetNearestSignpostToCoords(14876, -19033)
	GoToSignpost($BogrootChest)
	TolSleep(5000)
	PickupLootEx(5000)
	GoToSignpost($BogrootChest)
	TolSleep(5000)
	PickupLootEx(5000)
	MoveTo(14618, -17828)
	
	; 0x8101 dialog body
	; 0x833907 acccept quest
	Local $NPC = GetNearestNPCPtrToXY (14618, -17828)
	NPCHook($NPC, 20000)
	;Dialog($TekksDialog)
	QuestReward($hTekksWar)
	Dialog(0x833907)
	
	
	If GUI_IsSalvageChecked() = True Then Return SalvageItems()
EndFunc

Func ReverseToSparkflySwamp()
	MoveTo(14747, 480)
	sleep(500)
	
	Local $aTimer = TimerInit()
	Do
		MoveTo(14747, 480)
		Sleep(250)
	Until WaitMapLoading($iSplarkflyMapID) or TimerDiff($aTimer) > 60000
EndFunc

Func MoveandAggroEx($aWaypoints)
	Local $lMapId = GetMapId()
	For $i = GetNearestWaypointIndex($aWaypoints) To UBound($aWaypoints) - 1 Step 1
		BossGlow(ID(GetAgentByID(-2)), Random(2, 10, 1))
		$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
		Sleep(200)
		If GetMapId() <> $lMapId Then Return ; Need to get out of this MoveAggro loop if the mapid changes
		; (SoO only) unlit effect intercept - backtracks until lit effect is reapplied and resumes at light torch waypoint
		; Consets arent working - duration check is wrong or something
		;If(GUI_IsConsetsChecked() and (GetMapId() = $Bogroot_Growths_Lvl1 Or GetMapId() = $Bogroot_Growths_Lvl2)) Then UseConsets()
		;If(GUI_IsConsetsChecked() and (GetMapId() = $Bogroot_Growths_Lvl1 Or GetMapId() = $Bogroot_Growths_Lvl2)) Then UseArmor()
		If $unlit Then $i = RekindleTorch($aWaypoints, $NearestWaypoint)
        If GetMapLoading() == 2 Then Disconnected()
		If Wipe() = 1 Then    ; wipe intercept - wait until rezz and redirect to best waypoint, check for GetIsDead first to avoid checking fo wipe all the time..
			GUI_SetWipes(GUI_GetWipes() + 1)
			$LastWaypoint = $i
			Out("We wiped at " & $aWaypoints[$LastWaypoint][3] & ", waiting for rezz")
			CancelAll()    ; in case we were pulling enemie
			Do
				Sleep(500)
				If GetPartyDefeated() Then Return ResignAndReturn($Outpost, $Language)
				Until GetPartyHealth() > 0.5
				if GetMorale() < -40 then Usedp()
			$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
			$i = WipeManagement($aWaypoints, $NearestWaypoint, $LastWaypoint)    ; converts nearest waypoint to the desired waypoint based on last or nearest waypoint
			Out("Restarting at : " & $aWaypoints[$i][3])
		EndIf

		Out("Nearest waypoint - " & $aWaypoints[$NearestWaypoint][3])
		Out("Moving to - " & $aWaypoints[$i][3])

		Switch ($aWaypoints[$i][3])
		   	Case "Boss lock", "Chest", "Signpost"
				If Wipe() = 0 Then GoToSignpostNearXY($aWaypoints[$i][0], $aWaypoints[$i][1])
				PickupLootEx()
			Case "Blessing Lvl1"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				GetDwarvenBlessing(19099, 7762)	
			Case "Lvl1 to Lvl2"
				Local $aTimer = TimerInit()
				Do
					MoveTo(7665, -19050)
					Sleep(250)
				Until WaitMapLoading($Bogroot_Growths_Lvl2) Or TimerDiff($aTimer) > 60000
			Case "Dungeon Key"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				GetDungeonKeyEx()
			Case "Dungeon Door"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				OpenDungeonDoor()
			Case "Dungeon Door Checkpoint"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
				If($NearestWaypoint <> $i) Then
					Out("Failed dungeon door, going back to waypoint i-3")
					For $j = $i-1 To $i - 3 Step -1
						If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$j][0], $aWaypoints[$j][1], $aWaypoints[$j][2])
						If GetMapLoading() = 0 Then MoveTo($aWaypoints[$j][0], $aWaypoints[$j][1])
					Next
					$i = GetNearestWaypointIndex($aWaypoints)
				EndIf
			Case "Quest Door Checkpoint"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
				If($NearestWaypoint <> $i) Then
					Out("Failed first door, going back to get quest")
					For $j = $i-1 To $i - 3 Step -1
						If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$j][0], $aWaypoints[$j][1], $aWaypoints[$j][2])
						If GetMapLoading() = 0 Then MoveTo($aWaypoints[$j][0], $aWaypoints[$j][1])
					Next
					ReverseToSparkflySwamp()
					Return
				EndIf
			Case "Boss"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				Boss()
			Case Else
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() = 0 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
		EndSwitch
	Next
EndFunc   ;==>MoveandAggro