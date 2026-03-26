#include-once
; =============================================================================
; BotCore-Waypoints.au3
;
; Nearest-waypoint helper extracted from Froggy_HM_v1.6.au3 (KF-030).
; All dependencies resolve through Froggy_Includes.au3 master include.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; === Public Functions ===

; ---------------------------------------------------------------------------
; GetNearestWaypointIndex
;   Given a 2-D waypoints array ([$i][0] = X, [$i][1] = Y), returns the
;   index of the waypoint closest to the player's current position.
; ---------------------------------------------------------------------------
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
