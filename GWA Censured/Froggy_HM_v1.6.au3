#requireadmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\Utils.au3"
#include "lib\JSON.au3"
#include "lib\SQLite.au3"
#include "lib\SQLite.dll.au3"
InitializeGameClientData(True, True, False)

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
		Sleep(250)

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
		ZoneMap(638)
	Endif
	Sleep(1000)
	
	If($addHeroes) Then
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

		LoadSkillTemplate("OAOiAyk8gNtePuwJ00ZaNbJA", 1) ; ST Xandra remove hex
		LoadSkillTemplate("OAhjQkGZIT3BVVCPSTTODTjTciA", 2) ; BiP Olias
		LoadSkillTemplate("OAhjYoHYIPWb7wnoqKNncDzqH", 3) ; Xinrae Livia
		LoadSkillTemplate("OAljUwGpZSUBKgfBVVbh8Y7Y1YA", 4) ; MM Master P
		LoadSkillTemplate("OQhkAsC8gFKzJY6lDMd40hQG4iB", 5)	; E-Surge Gwen
		LoadSkillTemplate("OQhkAsC8gFKDNY6lDMd40hQG4iB", 6) ; Inep Norgu
		LoadSkillTemplate("OQljAkBsZSvAIg5ZkAcQsA7Y1YA", 7)	; Panic Razah

		sleep(500)

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
	
	Local $NPC = GetNearestNPCToCoords (12396, 22407)
	GoNPC($NPC)

	QuestReward($hTekksWar) ; in case for some reason we didnt find tekks after chest
	AcceptQuest($hTekksWar)

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
	Local $NPC = GetNearestNPCToCoords (14618, -17828)
	GoNPC($NPC)
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
		;BossGlow(GetMyID(), Random(2, 10, 1))
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

Func CacheSkillBar()
	Out("Mapping your skill bar")
	$PressureSpiritSkills = 0 ; (reset in case we map skills more than once)
	Sleep(200)
	For $i = 1 To 8
		$aSkillID = GetSkillbarSkillID($i)
        If $aSkillID = 0 Then ContinueLoop

        Local $skillStruct = GetSkillByID($aSkillID)
        If Not IsDllStruct($skillStruct) Then ContinueLoop

		$SkillbarSlot[$aSkillID] = $i
		$SkillBarCache[$i][$all] = $aSkillID
		$SkillBarCache[$i][$energyreq] = DllStructGetData($skillStruct, 'EnergyCost')
		$SkillBarCache[$i][$adrereq] = DllStructGetData($skillStruct, 'Adrenaline')
		$SkillBarCache[$i][$type] = DllStructGetData($skillStruct, 'Type')
		$SkillBarCache[$i][$target] = DllStructGetData($skillStruct, 'Target')

		If IsHexSpell($skillStruct) Then $SkillBarCache[$i][$hexes] = $aSkillID
		If IsPressureSkill($skillStruct) Then $SkillBarCache[$i][$pressure] = $aSkillID
		If IsBindingSkill($skillStruct) Then $SkillBarCache[$i][$bind] = $aSkillID
		If IsSpeedBoost($skillStruct) Then $SkillBarCache[$i][$speedBoost] = $aSkillID
		If IsSurvivalSkill($skillStruct) Then $SkillBarCache[$i][$survive] = $aSkillID
		If IsAttackSkill($skillStruct) Then $SkillBarCache[$i][$attackskill] = $aSkillID
		If IsHealSkill($skillStruct) Then $SkillBarCache[$i][$heal] = $aSkillID
		If IsBondSkill($skillStruct) Then $SkillBarCache[$i][$bond] = $aSkillID
		If IsCondRemoveSkill($skillStruct) Then $SkillBarCache[$i][$condremove] = $aSkillID
		If IsHexRemoveSkill($skillStruct) Then $SkillBarCache[$i][$hexremove] = $aSkillID
		If IsEnchantRemoveSkill($skillStruct) Then $SkillBarCache[$i][$enchantremove] = $aSkillID
		If IsRuptSkill($skillStruct) Then $SkillBarCache[$i][$rupt] = $aSkillID
		If IsRuptSkill($skillStruct, True) Then $SkillBarCache[$i][$hardrupt] = $aSkillID
		If IsPrecastSkill($skillStruct) Then $SkillBarCache[$i][$precast] = $aSkillID
		If IsChantSkill($skillStruct) or IsShoutSkill($skillStruct) Then $SkillBarCache[$i][$chantsnshouts] = $aSkillID
		If IsEchoRefrainskill($skillStruct) Then $SkillBarCache[$i][$echoes] = $aSkillID
		If IsPressureSpiritSkill($skillStruct) Then $PressureSpiritSkills += 1
	Next
	If $SkillbarSlot[$Signet_Of_Spirits] > 0 Then $PressureSpiritSkills += 2
	Out($PressureSpiritSkills & " pressure sprits on your skillbar")
	$Summon_Spirits = $SkillbarSlot[$Summon_Spirits_Luxon] + $SkillbarSlot[$Summon_Spirits_Kurzick]
	Out("Mapping your skill bar - completed")
	Return True
EndFunc

Func IsHexSpell($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Hex
EndFunc

Func IsConditionSpell($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Condition
EndFunc

Func IsRitualSkill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Ritual
EndFunc

Func IsWeaponSpell($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $WeaponSpell
EndFunc

Func IsEnchantmentSkill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Enchantment
EndFunc

Func IsAttackSkill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Attack
EndFunc

Func IsStanceskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Stance
EndFunc

Func IsSpellskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Spell
EndFunc

Func IsSignetskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Signet
EndFunc

Func IsWellskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Well
EndFunc

Func IsWardskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Ward
EndFunc

Func IsGlyphskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Glyph
EndFunc

Func IsShoutskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Shout
EndFunc

Func IsPreparationskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Preparation
EndFunc

Func IsTrapskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Trap
EndFunc

Func IsItemSpellskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $ItemSpell
EndFunc

Func IsChantskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Chant
EndFunc

Func IsEchoRefrainskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $EchoRefrain
EndFunc

Func IsDisguiseskill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False
    Return DllStructGetData($skillStruct, "Type") = $Disguise
EndFunc

Func IsBindingSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Ebon_Battle_Standard_of_Honor
			Return True
		Case $Agony, $Agony_PvP, $Anguish, $Anguish_PvP, _
				$Bloodsong, $Bloodsong_PvP, $Call_to_the_Spirit_Realm, $Destruction, $Destruction_PvP, _
				$Disenchantment, $Disenchantment_PvP, $Disenchantment_Togo, $Dissonance, $Dissonance_PvP, _
				$Gaze_of_Fury, $Gaze_of_Fury_PvP, $Jack_Frost, $Life, $Pain, $Pain_PvP, $Wanderlust, $Wanderlust_PvP, _
				$Displacement, $Displacement_PvP, $Earthbind, $Earthbind_PvP, $Empowerment, $Empowerment_PvP, _
				$Preservation, $Preservation_PvP, $Recovery, $Recovery_PvP, $Recuperation, $Recuperation_PvP, _
				$Rejuvenation, $Rejuvenation_PvP, $Shadowsong, $Shadowsong_PvP, $Shelter, $Shelter_PvP, _
				$Signet_of_Creation, $Signet_of_Creation_PvP, $Signet_Of_Spirits, $Signet_of_Spirits_PvP, _
				$Soothing, $Soothing_PvP, $Union, $Union_PvP, $Vampirism
			Return True
		Case $Summon_Spirits_Luxon, $Summon_Spirits_Kurzick
			Return True
		Case $Ritual_Lord, $Ritual_Lord_PvP
			Return True
		Case $Soul_Twisting
			Return True
		Case $Armor_of_Unfeeling, $Armor_of_Unfeeling_PvP, $Signet_of_Ghostly_Might, $Signet_of_Ghostly_Might_PvP
			Return True
		Case $Spiritleech_Aura
			Return True
	EndSwitch
	Return False
EndFunc

Func IsPressureSpiritSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Agony, $Agony_PvP, $Anguish, $Anguish_PvP, $Bloodsong, $Bloodsong_PvP, $Destruction, $Destruction_PvP, _
				$Disenchantment, $Disenchantment_PvP, $Dissonance, $Dissonance_PvP, $Gaze_of_Fury, $Gaze_of_Fury_PvP, _
				$Pain, $Pain_PvP, $Wanderlust, $Wanderlust_PvP, $Earthbind, $Earthbind_PvP, $Shadowsong, $Shadowsong_PvP, _
				$Signet_Of_Spirits, $Signet_of_Spirits_PvP, $Vampirism
			Return True
	EndSwitch
	Return False
EndFunc

Func IsRuptSkill($aSkill, $hardrupt = False)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False

	Local $lSkillEffect2 = DllStructGetData($skillStruct, 'Effect2')
	Local $lSkillID = DllStructGetData($skillStruct, 'ID')
	If BitAND($lSkillEffect2, 1) Then Return True

	Switch $lSkillID
		Case $Mistrust, $Mistrust_PvP, $Guilt, $Power_Drain, $Power_Flux, $Power_Leak, $Power_Leech, $Power_Lock, $Power_Return, $Power_Spike, $Shame
			If Not $hardrupt Then Return True
		Case $You_Move_Like_a_Dwarf, $Disarm, $Disrupting_Shot, $Disrupting_Throw, $Distracting_Lunge, $Distracting_Strike, $Magebane_Shot, $Concussion_Shot
			Return True
		Case $Cry_of_Pain, $Overload, $Psychic_Instability, $Psychic_Instability_PvP, $Signet_of_Clumsiness, $Simple_Thievery, $Tease
			Return True
		Case $Exhausting_Assault, $Temple_Strike, $Lyssas_Assault, $Lyssas_Haste, $Thunderclap
			Return True
		Case Else
			Return False
	EndSwitch
EndFunc

Func IsHardRuptSkill($aSkill)
	Return IsRuptSkill($aSkill, True)
EndFunc

Func IsHealSkill($aSkill)
    Local $skillStruct
    If IsDllStruct($aSkill) Then
        $skillStruct = $aSkill
    Else
        $skillStruct = GetSkillByID($aSkill)
    EndIf
    If Not IsDllStruct($skillStruct) Then Return False

	Switch(DllStructGetData($skillStruct, 'Effect2'))
		Case 2, 4, 6, 36, 38, 4102, 4096, 6144, 8196, 8198, 14336
			Return True
		Case Else
			Switch(DllStructGetData($skillStruct, 'ID'))
				Case $Healing_Hands, $Restful_Breeze, $Signet_Of_Rejuvenation, $Words_of_Comfort, $Conviction, $Faithful_Intervention, _
					$Mystic_Healing, $Mystic_Healing_PvP, $Mystic_Regeneration, $Mystic_Vigor, $Pious_Renewal, $Watchful_Intervention, _
					$Spirit_Transfer, $I_Will_Avenge_You, $I_Will_Survive, $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick, _
					$Shroud_of_Distress, $Healing_Spring, $Hexers_Vigor, $Feel_No_Pain
				Return True
			EndSwitch
	EndSwitch

	If IsPartyHealSkill($skillStruct) Then Return True
	Return False
EndFunc

Func IsPartyHealSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Divine_Healing, $Heal_Party, $Heavens_Delight, $Protective_Was_Kaolai, $Mystic_Healing, $Mystic_Healing_PvP
			Return True
	EndSwitch
	Return False
EndFunc

Func IsBondSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Balthazars_Spirit, $Essence_Bond, $Life_Barrier, $Life_Bond, $Mending, $Protective_Bond, $Purifying_Veil, $Retribution, $Strength_of_Honor, $Succor
			Return True
	EndSwitch
	Return False
EndFunc

Func IsCondRemoveSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $mend_ailment, $purge_conditions, $mending_touch, $Mend_Body_and_Soul, $Spotless_Soul
			Return True
	EndSwitch
	If IsHexAndConditionRemoveSkill($skillID) Then Return True
	Return False
EndFunc

Func IsHexRemoveSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Smite_Hex, $Divert_Hexes, $Cure_Hex, $Hex_Eater_Vortex, $Shatter_Hex, $Reverse_Hex, $Remove_Hex, $Expel_Hexes, $Inspired_Hex, $Revealed_Hex, $Spotless_Mind, $Convert_Hexes, $Deny_Hexes, _
				$Hex_Eater_Signet, $Withdraw_Hexes, $Hexbreaker_Aria, $holy_veil, $Pious_Restoration
			Return True
	EndSwitch
	If IsHexAndConditionRemoveSkill($skillID) Then Return True
	Return False
EndFunc

Func IsHexAndConditionRemoveSkill($aSkillID)
	Switch $aSkillID
		Case $Peace_and_Harmony, $Empathic_Removal, $Blessed_Light, $Contemplation_of_Purity, $Purge_Signet, $Signet_of_Removal
			Return True
	EndSwitch
	Return False
EndFunc

Func IsEnchantRemoveSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Chilblains, $Corrupt_Enchantment, $Envenom_Enchantments, $Jaundiced_Gaze, $Pain_of_Disenchantment, $Rend_Enchantments, _
				$Rip_Enchantment, $Strip_Enchantment, _
				$Air_of_Disenchantment, $Discharge_Enchantment, $Drain_Enchantment, $Feedback, $Inspired_Enchantment, $Lyssas_Balance, _
				$Mirror_of_Disenchantment, $Revealed_Enchantment, $Shatter_Enchantment, $Shatter_Storm, $Signet_of_Disenchantment, _
				$Assault_Enchantments, $Expunge_Enchantments, $Lift_Enchantment, $Shattering_Assault, $Signet_of_Twilight, _
				$Rending_Touch, $Test_of_Faith
			Return True
	EndSwitch
	Return False
EndFunc

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
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Dwarven_Stability
			Return True
		Case $Illusion_of_Haste, $Windborne_Speed, $Armor_of_Mist, $Storm_Djinns_Haste, $Rush, $Sprint, $Charge, $Bulls_Charge, $Dodge, $Escape, $Storm_Chaser, $Run_as_One, _
				$Burning_Speed, $Retreat, $Gust, $Shadow_of_Haste, $Torch_Hex, $Torch_Degeneration_Hex, $Dark_Escape, $Dash, $Zojuns_Haste, $Flame_Djinns_Haste, _
				$Enraging_Charge, $Lyssas_Haste, $Avatar_of_Balthazar, $Enchanted_Haste, $Pious_Haste, $Whirling_Charge, $Godspeed, $Make_Haste, $Fall_Back, $Incoming, $Onslaught, _
				$Featherfoot_Grace, $Harriers_Haste, $Hasty_Refrain, $Soldiers_Speed, $Drunken_Master, $Ursan_Roar, $Volfen_Pounce, $Escape_PvP, $Charging_Strike, _
				$Battle_Rage, $Natural_Stride, $Storms_Embrace, $Junundu_Tunnel, $Flee, $HYAHHHHH, $Ursan_Force, $Incoming_PvP, $Its_Just_a_Flesh_Wound, $Fall_Back_PvP, _
				$Call_of_Haste, $Call_of_Haste_PvP, $Lead_the_Way, $Fleeting_Stability, $Illusion_of_Haste_PvP, $Mindbender, $Rampage_as_One
			Return True
		Case $Heroic_Refrain, $To_the_Limit
			Return True
	EndSwitch
	Return False
EndFunc

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

Func IsSelfPrehealSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $Healing_Breeze, $Healing_Hands, $Mending, $Patient_Spirit, $Restful_Breeze, $Spirit_Bond, $Vigorous_Spirit _
				, $Conviction, $Faithful_Intervention, $Mystic_Regeneration, $Mystic_Vigor, $Pious_Renewal, $Watchful_Intervention _
				, $Feigned_Neutrality, $Shadow_Refuge, $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick, $Shroud_of_Distress _
				, $Healing_Spring, $Troll_Unguent _
				, $Blood_Renewal, $Hexers_Vigor
			Return True
	EndSwitch
	Return False
EndFunc

Func Disconnected()
	Local $lCheck = False
	Local $lDeadlock = TimerInit()
	Do
		Sleep(20)
		$lCheck = GetMapLoading() <> 2 And GetAgentExists(-2)
	Until $lCheck Or TimerDiff($lDeadlock) > 5000
	If $lCheck = False Then
		Out('Disconnected!')
		Out('Attempting to reconnect.')
		ControlSend(GetWindowHandle(), '', '', '{Enter}')
		$lDeadlock = TimerInit()
		Do
			Sleep(20)
			$lCheck = GetMapLoading() <> 2 And GetAgentExists(-2)
		Until $lCheck Or TimerDiff($lDeadlock) > 60000
		If $lCheck = False Then
			Out('Failed to Reconnect 1!')
			Out('Retrying.')
			ControlSend(GetWindowHandle(), '', '', '{Enter}')
			$lDeadlock = TimerInit()
			Do
				Sleep(20)
				$lCheck = GetMapLoading() <> 2 And GetAgentExists(-2)
			Until $lCheck Or TimerDiff($lDeadlock) > 60000
			If $lCheck = False Then
				Out('Failed to Reconnect 2!')
				Out('Retrying.')
				ControlSend(GetWindowHandle(), '', '', '{Enter}')
				$lDeadlock = TimerInit()
				Do
					Sleep(20)
					$lCheck = GetMapLoading() <> 2 And GetAgentExists(-2)
				Until $lCheck Or TimerDiff($lDeadlock) > 60000
				If $lCheck = False Then
					Out('Could not reconnect!')
					Out('Exiting.')
					EnableRendering()
					Exit 1
				EndIf
			EndIf
		EndIf
	EndIf
	Out('Reconnected!')
	Sleep(5000)
EndFunc

Func Wipe()
	If Not GetIsDead(-2) Then Return False
	Local $DeadPartyMembers = 0
	For $i = 1 To GetHeroCount()
		If GetIsDead(GetHeroID($i)) = True Then $DeadPartyMembers += 1
	Next
	If GetIsDead(-2) And (GetAvailableRezz() = 0 Or $DeadPartyMembers >= UBound(GetParty()) - 2 Or GetPartyHealth() < 0.15) Then Return True
	Return False
EndFunc

Func GetPartyHealth()
	Local $aTotalTeamHP = 0
	Local $aParty = GetParty()
	If Not IsArray($aParty) Or UBound($aParty) = 0 Then return 0
	For $i = 0 To UBound($aParty) - 1
		If GetIsDead($aParty[$i]) Then ContinueLoop
		Local $aAgent = $aParty[$i]
		Local $aAgentHP = Round(DllStructGetData($aAgent, 'HealthPercent'), 6)
		$aTotalTeamHP += $aAgentHP
	Next
    If UBound($aParty) <= 1 Then Return 0
	Local $nAverageHP = Round($aTotalTeamHP / (UBound($aParty) -1), 6)
	Return $nAverageHP
EndFunc

Func GetAvailableRezz()
	Local $aHeroRezzSkills = 0
	For $aHeroNumber = 1 To GetHeroCount()
		If GetIsDead(GetHeroID($aHeroNumber)) Then ContinueLoop
		For $aSkillSlot = 1 To 8
			Local $aSkill = GetSkillbarSkillID($aSkillSlot, $aHeroNumber)
			If IsResSkill($aSkill) Then $aHeroRezzSkills += 1
		Next
	Next
	Return $aHeroRezzSkills
EndFunc

Func IsResSkill($aSkill)
    Local $skillID = IsDllStruct($aSkill) ? DllStructGetData($aSkill, "ID") : $aSkill
	Switch $skillID
		Case $By_Urals_Hammer, $We_Shall_Return, $Death_Pact_Signet, $Eternal_Aura, $Flesh_of_My_Flesh, $Junundu_Wail, $Light_of_Dwayna, $Lively_Was_Naomei, $Rebirth, $Renew_Life, _
				$Restoration, $Restore_Life, $Resurrect, $Resurrection_Chant, $Resurrection_Signet, $Signet_of_Return, $Sunspear_Rebirth_Signet, $Unyielding_Aura, $Vengeance
			Return True
	EndSwitch
	Return False
EndFunc

Func GetPartyDefeated()
    Return GetPartyState(0x20)
EndFunc

Func WipeManagement($aWaypoints, $NearestWaypoint, $LastWaypoint)
	Out("Last waypoint - " & $aWaypoints[$LastWaypoint][3])

	Switch GetMapID()
		Case $Shards_of_Oor_Lvl1
			Switch $aWaypoints[$NearestWaypoint][3]
			Case 1 to 9
			     Return 1
			Case 10 to 14
			     Return 10
			Case 15 to 20
			    Return 14
			EndSwitch

		Case $Shards_of_Oor_Lvl2
			Switch $aWaypoints[$NearestWaypoint][3]
			   Case  1 to 3
					Return 1
			   Case  4 to 11
					Return 4
			EndSwitch

	    Case $Shards_of_Oor_Lvl3
			Switch $aWaypoints[$NearestWaypoint][3]
                Case 1 to 8
			      Return 1
			   Case 9 to 16
			      Return 9
			   Case 17 to 20
			      Return 20
			   Case 21 To 41
			     Return 23
		    	EndSwitch
			 Case $iSplarkflyMapID
		 Switch $aWaypoints[$NearestWaypoint][3]
			   Case 1 to 6
			    Return 4
			   Case 7 to 17
			    Return 7
		     	EndSwitch

		Case $Bogroot_Growths_Lvl1
			Switch $aWaypoints[$NearestWaypoint][3]
				Case 14
					Return 9
			EndSwitch

		Case $Bogroot_Growths_Lvl2
			Switch $aWaypoints[$NearestWaypoint][3]
				Case 1 to 8
					Return 1
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
	If GetMapLoading() <> 1 Or GetMapLoading() = 2 Then Return True
	If WeCanMove($aFightRange) Then Move($x, $y, $random)
	Local $TimerAggro = TimerInit()
	Do
		$aOldX = DllStructGetData(GetMyAgent(), 'X')
		$aOldY = DllStructGetData(GetMyAgent(), 'Y')
		If GetMapLoading() == 2 Then Disconnected()
		If GetNearestEnemyDistance() < $aFightRange Then Fight($aFightRange)
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
	If GetNearestEnemyDistance() < $aRange Then Return False
	Return True
EndFunc

Func Fight($aAggroRange = 1000, $careful = False)
	Out("Fighting enemies")
	Local $TimerToGetOut = TimerInit()
	Do
		If $careful Then CancelAll()
		Local $BestTarget = GetNearestEnemyToAgent(GetMyAgent())
		If IsDllStruct($BestTarget) Then
			Attack($BestTarget, True)
		EndIf
		Sleep(100)
		If $careful Then
			MoveTo(DllStructGetData($BestTarget, 'X'), DllStructGetData($BestTarget, 'Y'))
			Sleep(300)
		EndIf
	Until GetNearestEnemyDistance() > $aAggroRange Or GetIsDead(-2) Or Wipe() Or TimerDiff($TimerToGetOut) > 240000
	PickupLootEx(3000)
EndFunc

Func CheckForChest($chestrun = False)
	Local $AgentArray, $lAgent, $lExtraType
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

Func GetDwarvenBlessing($x, $y)
	Out("Getting Dwarven Blessing")
	Local $npc = GetNearestNPCToCoords($x, $y)
	GoToNPC($npc)
	TolSleep(500)
	While 1
		Dialog(0x84)
		For $i = 0 To UBound($DwarvenBuffArr) - 1
			If HasEffect($DwarvenBuffArr[$i]) Then Return Out("Dwarven Blessing Received")
		Next
		Return Out("Unable To Get Dwarven Blessing")
	WEnd
EndFunc

Func PickupLootEx($iMaxDist = 2000, $PickupTorch = False)
	Local $lAgentArray = GetAgentArray($ID_AGENT_TYPE_ITEM)
	Local $lPickupDeadlock = TimerInit()
	Local $lPickupCounter = 0
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
				Return True
			EndIf
		Case $TYPE_GOLD_COINS
            If $gPickupCoins And $lModelID = 2511 And GetGoldCharacter() + $lValue < 100000 Then Return True
		Case $TYPE_KEY
			If $lModelID = 22751 Then Return True
			If $lModelID = 25410 Or $lModelID = 25416 Then Out("Grab Dungeon Key")
			Return True

		Case $TYPE_MATERIAL_AND_ZCOINS, $TYPE_SCROLL, $TYPE_TROPHY
			Return false
		Case $TYPE_USABLE
			Switch $lModelID
			Case 21786 To 21805
					Return True
		  EndSwitch
        EndSwitch

    Switch $lRarity
		Case $RARITY_Gold
		    Return True
	EndSwitch
	Return False
EndFunc