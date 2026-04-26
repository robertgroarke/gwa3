; click_trade_offer_item.au3
;
; Automated: finds GW window, clicks first inventory slot to offer it to trade.
; Assumes trade window is already open.
; No human interaction required.

#RequireAdmin
#include <ScreenCapture.au3>

; Find the Guild Wars window
Local $hwnd = WinGetHandle("[CLASS:ArenaNet_Dx_Window_Class]")
If @error Then
    ConsoleWrite("ERROR: Guild Wars window not found" & @CRLF)
    Exit 1
EndIf

; Activate and wait
WinActivate($hwnd)
Sleep(1000)

; Get client area position
Local $pos = WinGetPos($hwnd)
If @error Then
    ConsoleWrite("ERROR: Could not get window position" & @CRLF)
    Exit 1
EndIf

Local $winX = $pos[0]
Local $winY = $pos[1]
Local $winW = $pos[2]
Local $winH = $pos[3]

ConsoleWrite("GW window: x=" & $winX & " y=" & $winY & " w=" & $winW & " h=" & $winH & @CRLF)

; Take a screenshot for debugging
Local $screenshotPath = @ScriptDir & "\trade_offer_screenshot.bmp"
_ScreenCapture_CaptureWnd($screenshotPath, $hwnd)
ConsoleWrite("Screenshot saved: " & $screenshotPath & @CRLF)

; In Guild Wars, when a trade window is open, the inventory (backpack)
; is visible in the right side panel. The first inventory slot is
; approximately at these relative positions within the GW window:
;
; For typical GW resolutions:
;   Inventory panel right edge is at ~97% of window width
;   First slot row starts at ~14-18% of window height
;   Each slot is roughly 35-40 pixels apart
;   First slot X is at ~80-85% of window width
;
; We'll try multiple positions to find a clickable inventory item.

; Strategy: try several inventory slot positions
; The inventory grid in GW is typically 5 columns x rows
; Starting position for first backpack slot (approximate):
; Updated from screenshot analysis: inventory is at BOTTOM-RIGHT
; First item slot at roughly x=78%, y=72% of window area
; Slot spacing ~33px at 1676x1066 resolution
Local $invStartX = $winX + Int($winW * 0.78)
Local $invStartY = $winY + Int($winH * 0.72)
Local $slotSpaceX = Int($winW * 0.020)
Local $slotSpaceY = Int($winH * 0.031)

ConsoleWrite("Inventory grid start: " & $invStartX & "," & $invStartY & @CRLF)
ConsoleWrite("Slot spacing: " & $slotSpaceX & "x" & $slotSpaceY & @CRLF)

; Try clicking first few inventory slots with double-click
; Double-clicking an item in inventory while trade is open offers it
For $row = 0 To 2
    For $col = 0 To 4
        Local $clickX = $invStartX + ($col * $slotSpaceX)
        Local $clickY = $invStartY + ($row * $slotSpaceY)
        ConsoleWrite("Trying slot [" & $row & "," & $col & "] at " & $clickX & "," & $clickY & @CRLF)
        MouseClick("left", $clickX, $clickY, 2, 3)
        Sleep(500)
    Next
Next

ConsoleWrite("Done clicking inventory slots." & @CRLF)
Sleep(1000)

; Take another screenshot to see result
Local $screenshotAfter = @ScriptDir & "\trade_offer_screenshot_after.bmp"
_ScreenCapture_CaptureWnd($screenshotAfter, $hwnd)
ConsoleWrite("After-screenshot saved: " & $screenshotAfter & @CRLF)

ConsoleWrite("COMPLETE - check gwa3 log for [PACKET-TAP] entries" & @CRLF)
