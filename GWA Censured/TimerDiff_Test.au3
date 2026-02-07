; ==========================================
; TimerDiff Test Script (Standalone)
; Tests if TimerDiff() returns expected values
; No dependencies on bot libraries
; Outputs to log file
; ==========================================

Local $logFile = @ScriptDir & "\TimerDiff_Test_Results.txt"
Local $output = ""

$output &= "=== TimerDiff Test Started ===" & @CRLF
$output &= "Testing AutoIt's TimerInit() and TimerDiff() functions" & @CRLF
$output &= "Test run at: " & @YEAR & "-" & @MON & "-" & @MDAY & " " & @HOUR & ":" & @MIN & ":" & @SEC & @CRLF
$output &= "" & @CRLF

; Test 1: Basic timing test
$output &= "Test 1: Sleep 1000ms, expect ~1000ms difference" & @CRLF
Local $timer1 = TimerInit()
Sleep(1000)
Local $diff1 = TimerDiff($timer1)
$output &= "  Result: " & Round($diff1) & "ms (expected ~1000ms)" & @CRLF
If $diff1 > 900 And $diff1 < 1200 Then
    $output &= "  PASS: Within expected range" & @CRLF
Else
    $output &= "  FAIL: Outside expected range!" & @CRLF
EndIf
$output &= "" & @CRLF

; Test 2: Short timing test
$output &= "Test 2: Sleep 500ms, expect ~500ms difference" & @CRLF
Local $timer2 = TimerInit()
Sleep(500)
Local $diff2 = TimerDiff($timer2)
$output &= "  Result: " & Round($diff2) & "ms (expected ~500ms)" & @CRLF
If $diff2 > 400 And $diff2 < 700 Then
    $output &= "  PASS: Within expected range" & @CRLF
Else
    $output &= "  FAIL: Outside expected range!" & @CRLF
EndIf
$output &= "" & @CRLF

; Test 3: Simulated wipe wait loop (what happens in wipe recovery)
$output &= "Test 3: Simulated wipe wait loop (5 iterations, 500ms each)" & @CRLF
Local $lDeadlock = TimerInit()
Local $lWipeWaitCounter = 0
Local $loopExitReason = ""

Do
    Sleep(500)
    $lWipeWaitCounter += 1
    $output &= "  Iteration " & $lWipeWaitCounter & ": TimerDiff=" & Round(TimerDiff($lDeadlock)) & "ms, Counter=" & $lWipeWaitCounter & @CRLF
    
    ; Check exit conditions (same as wipe recovery code)
    If TimerDiff($lDeadlock) > 120000 Then
        $loopExitReason = "TimerDiff > 120000"
        ExitLoop
    EndIf
    If $lWipeWaitCounter > 240 Then
        $loopExitReason = "Counter > 240"
        ExitLoop
    EndIf
Until $lWipeWaitCounter >= 5 ; Exit early for test

If $loopExitReason = "" Then $loopExitReason = "Counter reached 5 (test limit)"
$output &= "  Loop exited because: " & $loopExitReason & @CRLF
$output &= "  Final TimerDiff: " & Round(TimerDiff($lDeadlock)) & "ms (expected ~2500ms)" & @CRLF
$output &= "" & @CRLF

; Test 4: Test the 5-second timeout
$output &= "Test 4: Check if 5-second timeout works" & @CRLF
Local $timer4 = TimerInit()
Local $iterations = 0
While TimerDiff($timer4) < 5000  ; 5 seconds
    Sleep(500)
    $iterations += 1
WEnd
Local $finalTime4 = TimerDiff($timer4)
$output &= "  Loop ran for " & $iterations & " iterations over " & Round($finalTime4) & "ms" & @CRLF
If $finalTime4 > 4900 And $finalTime4 < 6000 Then
    $output &= "  PASS: Timeout worked correctly" & @CRLF
Else
    $output &= "  FAIL: Timeout did not work as expected!" & @CRLF
EndIf
$output &= "" & @CRLF

$output &= "=== TimerDiff Test Complete ===" & @CRLF
$output &= "If all tests PASS, TimerDiff() is working correctly." & @CRLF
$output &= "If any tests FAIL, there may be an issue with timing functions." & @CRLF

; Write to file
FileWrite($logFile, $output)

MsgBox(0, "TimerDiff Test", "Test complete! Results saved to:" & @CRLF & @CRLF & $logFile)
ShellExecute($logFile)  ; Open the log file automatically
