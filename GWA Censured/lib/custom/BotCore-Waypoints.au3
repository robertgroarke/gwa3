#include-once
; =============================================================================
; BotCore-Waypoints.au3
;
; Waypoint traversal engine extracted from Froggy_HM_v1.6.au3.
; Contains: nearest-waypoint lookup (KF-030), single-step movement (KF-031),
;           main waypoint runner loop (KF-032), and wipe management (KF-033).
;
; All dependencies resolve through Froggy_Includes.au3 master include.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; =============================================================================
; === Nearest Waypoint (KF-030) ===============================================
; =============================================================================

; ---------------------------------------------------------------------------
; GetNearestWaypointIndex
;   Given a 2-D waypoints array ([$i][0] = X, [$i][1] = Y), returns the
;   index of the waypoint closest to the player's current position.
;
; Globals read : (none)
; Globals written: (none)
; ---------------------------------------------------------------------------
;~ Navigate to the nearest signpost at given coordinates
Func GoToSignpostNearXY($x, $y)
	Local $signpost = GetNearestSignpostToCoords($x, $y)
	GoToSignpost($signpost)
EndFunc

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
EndFunc   ;==>GetNearestWaypointIndex

; =============================================================================
; === Movement Step Engine (KF-031) ===========================================
; =============================================================================

; ---------------------------------------------------------------------------
; AggroMoveToEX
;   Moves to a single waypoint ($x, $y) while fighting any enemies within
;   $aFightRange, looting, and checking for chests. Handles blocked-path
;   detection and deadlock timeouts.
;
; Globals read : (none directly -- delegates to Fight, PickupLootEx, etc.)
; Globals written: (none directly)
;
; External calls:
;   GetMapLoading, Move, WeCanMove, GetNearestEnemyDistance, Fight,
;   GUI_IsChestChecked, CheckForChest, PickupLootEx, GetDistanceToPoint,
;   GetMyAgent, Wipe, Disconnected, Out
; ---------------------------------------------------------------------------
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
EndFunc   ;==>AggroMoveToEX

; ---------------------------------------------------------------------------
; WeCanMove
;   Returns True if no enemies are within $aRange of the player.
;
; Globals read : (none)
; Globals written: (none)
; ---------------------------------------------------------------------------
Func WeCanMove($aRange = 1200)
	Local $dist = GetNearestEnemyDistance()
	Out("Debug: WeCanMove Dist=" & Round($dist) & " Range=" & $aRange & " CanMove=" & ($dist > $aRange))
	If $dist < $aRange Then Return False
	Return True
EndFunc   ;==>WeCanMove

; ---------------------------------------------------------------------------
; GetNearestEnemyDistance
;   Returns the distance to the nearest enemy, or 10000 if none found.
;
; Globals read : (none)
; Globals written: (none)
; ---------------------------------------------------------------------------
Func GetNearestEnemyDistance()
	Local $target = GetNearestEnemyToAgent(GetMyAgent())
	If IsDllStruct($target) Then
		Local $d = GetDistance(GetMyAgent(), $target)
		Out("Debug: Found Enemy ID=" & DllStructGetData($target, 'ID') & " Dist=" & Round($d))
		Return $d
	Else
		Out("Debug: No Enemy Found (GetNearestEnemyToAgent returned non-struct)")
		Return 10000
	EndIf
EndFunc   ;==>GetNearestEnemyDistance

; =============================================================================
; === Waypoint Runner (KF-032) ================================================
; =============================================================================

; ---------------------------------------------------------------------------
; MoveandAggroEx
;   Main waypoint traversal loop. Walks through a 2-D waypoint array where:
;     [$i][0] = X coordinate
;     [$i][1] = Y coordinate
;     [$i][2] = fight range (passed to AggroMoveToEX)
;     [$i][3] = waypoint label (string)
;
;   The label drives special-case handling via a Switch block. Many of the
;   Case branches are Froggy / Bogroot-specific and are marked below.
;
;   Includes: stuck detection with backtracking, wipe handling with
;   resurrection wait, morale check, and WipeManagement-based restart index.
;
; Globals read : $NearestWaypoint, $LastWaypoint, $Outpost, $Language,
;                $bRunFailed, $Bogroot_Growths_Lvl2
; Globals written: $NearestWaypoint, $LastWaypoint, $bRunFailed
;
; External calls (non-trivial):
;   GetNearestWaypointIndex, AggroMoveToEX, WipeManagement, Wipe,
;   GetIsDead, GetPartyDefeated, GetMorale, Usedp, CancelAll,
;   ResignAndReturn, Disconnected, GUI_SetWipes, GUI_GetWipes,
;   GoToSignpostNearXY, PickupLootEx, GetDungeonKeyEx, OpenDungeonDoor,
;   Boss, ReverseToSparkflySwamp, WaitMapLoading, MoveTo, Out
; ---------------------------------------------------------------------------
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
				ContinueLoop ; Restart loop with new index
			EndIf
		Else
			$lStuckCounter = 0  ; Reset counter when we make progress
			$lLastNearestWaypoint = $NearestWaypoint
		EndIf

		Sleep(200)
		If GetMapId() <> $lMapId Then Return ; Need to get out of this MoveAggro loop if the mapid changes
		; FROGGY-SPECIFIC: (SoO only) unlit effect intercept -- commented out in source
		;If(GUI_IsConsetsChecked() and (GetMapId() = $Bogroot_Growths_Lvl1 Or GetMapId() = $Bogroot_Growths_Lvl2)) Then UseConsets()
		;If(GUI_IsConsetsChecked() and (GetMapId() = $Bogroot_Growths_Lvl1 Or GetMapId() = $Bogroot_Growths_Lvl2)) Then UseArmor()
		;If $unlit Then $i = RekindleTorch($aWaypoints, $NearestWaypoint)
		If GetMapLoading() == 2 Then Disconnected()

		; --- Wipe intercept: wait for resurrection and redirect to best waypoint ---
		If Wipe() = 1 Then
			GUI_SetWipes(GUI_GetWipes() + 1)
			$LastWaypoint = $i

			Local $wpName = "Unknown"
			If $LastWaypoint >= 0 And $LastWaypoint < UBound($aWaypoints) Then
				$wpName = $aWaypoints[$LastWaypoint][3]
			EndIf
			Out("We wiped at " & $wpName & ", waiting for rezz")
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
			; FROGGY-SPECIFIC: "Boss lock", "Chest", "Signpost" -- uses GoToSignpostNearXY
			Case "Boss lock", "Chest", "Signpost"
				If Wipe() = 0 Then GoToSignpostNearXY($aWaypoints[$i][0], $aWaypoints[$i][1])
				PickupLootEx()

			; FROGGY-SPECIFIC: Bogroot level transition
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

			; FROGGY-SPECIFIC: Quest Door Checkpoint with ReverseToSparkflySwamp fallback
			Case "Quest Door Checkpoint"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				$NearestWaypoint = GetNearestWaypointIndex($aWaypoints)
				If($NearestWaypoint <> $i) Then
					Out("Failed first door, going back to get quest")
					$bRunFailed = True  ; FROGGY-SPECIFIC: Mark this run as failed (don't count for best time)
					For $j = $i-1 To $i - 3 Step -1
						If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$j][0], $aWaypoints[$j][1], $aWaypoints[$j][2])
						If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$j][0], $aWaypoints[$j][1])
					Next
					ReverseToSparkflySwamp() ; FROGGY-SPECIFIC: Bogroot abort route
					Return
				EndIf

			; FROGGY-SPECIFIC: Boss encounter handler
			Case "Boss"
				If GetMapLoading() = 1 Then AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				If GetMapLoading() <> 1 Then MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				Boss()

			Case Else
				Local $mapState = GetMapLoading()
				Out("Debug: Case Else waypoint " & $i & " MapLoading=" & $mapState & " FightRange=" & $aWaypoints[$i][2])
				If $mapState = 1 Then
					AggroMoveToEX($aWaypoints[$i][0], $aWaypoints[$i][1], $aWaypoints[$i][2])
				Else
					Out("Debug: SKIPPING AggroMoveToEX (MapLoading<>1)")
					MoveTo($aWaypoints[$i][0], $aWaypoints[$i][1])
				EndIf
		EndSwitch
	Next
EndFunc   ;==>MoveandAggroEx

; ---------------------------------------------------------------------------
; GetPartyDefeated
;   Returns True if the party is in the defeated state (flag 0x20).
;
; Globals read : (none)
; Globals written: (none)
; ---------------------------------------------------------------------------
Func GetPartyDefeated()
	Return GetPartyState(0x20)
EndFunc   ;==>GetPartyDefeated

; =============================================================================
; === Wipe Management (KF-033) ================================================
; =============================================================================

; ---------------------------------------------------------------------------
; WipeManagement
;   After a wipe, determines the best waypoint index to restart from based
;   on the current map and the nearest waypoint.
;
;   Currently contains FROGGY-SPECIFIC map-id-to-checkpoint tables for:
;     - Shards of Orr Lvl 1/2/3
;     - Sparkfly Swamp
;     - Bogroot Growths Lvl 1/2
;
;   Future: refactor into a callback / data-table pattern so each bot
;   supplies its own restart-point map without editing this function.
;
; Globals read : $Shards_of_Oor_Lvl1, $Shards_of_Oor_Lvl2,
;                $Shards_of_Oor_Lvl3, $Sparkfly_Swamp,
;                $Bogroot_Growths_Lvl1, $Bogroot_Growths_Lvl2
; Globals written: (none)
;
; Params:
;   $aWaypoints      - the waypoint array (used only for logging)
;   $NearestWaypoint - index of the nearest waypoint after resurrection
;   $LastWaypoint    - index of the waypoint we were at when we wiped
;
; Returns: waypoint index to restart traversal from
; ---------------------------------------------------------------------------
Func WipeManagement($aWaypoints, $NearestWaypoint, $LastWaypoint)
	Out("Last waypoint - " & $aWaypoints[$LastWaypoint][3])
	Out("Nearest waypoint index - " & $NearestWaypoint)

	; FROGGY-SPECIFIC: Map-based checkpoint tables
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
EndFunc   ;==>WipeManagement
