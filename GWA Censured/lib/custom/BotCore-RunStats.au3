#include-once
; =============================================================================
; BotCore-RunStats.au3
;
; Runtime timers and run-statistics functions extracted from
; Froggy_HM_v1.6.au3 (KF-035).
; All dependencies resolve through Froggy_Includes.au3 master include.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; === Module State ===
Global $nBestRunTime    = 999999999
Global $nCurrentRunTime = 0
Global $nTotalRunTime   = 0
Global $CumulatedTime   = 0
Global $AvgRunTime      = 0
Global $bRunFailed      = False

; === Public Functions ===

; ---------------------------------------------------------------------------
; AvgRunTime
;   Calculates the average successful-run time from cumulated ticks and
;   returns a formatted HH:MM:SS string.
;   Relies on globals: $nCurrentRunTime, $CumulatedTime, $GUI_RunCounter,
;                      $GUI_FailCounter, $AvgRunTime
; ---------------------------------------------------------------------------
Func AvgRunTime()
	Local $CurrentRunTime = Floor(TimerDiff($nCurrentRunTime))
	$CumulatedTime += $CurrentRunTime
	Out("Cumulated Time: " & $CumulatedTime)
	Out("Run Counter: " & $GUI_RunCounter)
	$AvgRunTime = $CumulatedTime / ($GUI_RunCounter - $GUI_FailCounter)

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
EndFunc   ;==>AvgRunTime

; ---------------------------------------------------------------------------
; BestRunTime
;   Updates $nBestRunTime if the current run was faster and not failed.
;   Returns the best run time in milliseconds.
; ---------------------------------------------------------------------------
Func BestRunTime()
	; Only update best time if this wasn't a failed run
	If Not $bRunFailed And TimerDiff($nCurrentRunTime) < $nBestRunTime Then
		$nBestRunTime = TimerDiff($nCurrentRunTime)
	EndIf
	Return $nBestRunTime
EndFunc   ;==>BestRunTime

; ---------------------------------------------------------------------------
; UpdateStats
;   Pushes current title-track deltas and lockpick count to the GUI.
; ---------------------------------------------------------------------------
Func UpdateStats()
	GUI_SetVanguard(GetVanguardTitle() - $iVanguardTitle)
	GUI_SetNorn(GetNornTitle() - $iNornTitle)
	GUI_SetAsura(GetAsuraTitle() - $iAsuraTitle)
	GUI_SetDeldrimor(GetDeldrimorTitle() - $iDeldrimorTitle)
	GUI_SetLockpicks(GetPicksCount())
EndFunc   ;==>UpdateStats

; ---------------------------------------------------------------------------
; CurrentRunTime
;   Updates the current-run timer on the GUI status bar (Adlib callback).
; ---------------------------------------------------------------------------
Func CurrentRunTime()
	GUI_SetRunTime(TimerDiff($nCurrentRunTime))
EndFunc   ;==>CurrentRunTime

; ---------------------------------------------------------------------------
; TotalRunTime
;   Updates the total-session timer on the GUI status bar (Adlib callback).
; ---------------------------------------------------------------------------
Func TotalRunTime()
	GUI_SetTotalTime(TimerDiff($nTotalRunTime))
EndFunc   ;==>TotalRunTime
