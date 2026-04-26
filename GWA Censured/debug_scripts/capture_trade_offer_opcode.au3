; capture_trade_offer_opcode.au3
;
; Purpose: Automate a manual trade offer via real mouse clicks while
;          the gwa3 packet tap captures the CtoS opcodes.
;
; Prerequisites:
;   1. DISCO PANIC is launched, injected with gwa3_trade.dll (--llm), and
;      in Longeye's Ledge with the trade window already open with BLUMPKINS.
;   2. The gwa3 DLL has the packet tap enabled (s_packetTapEnabled = true).
;   3. The trade window is visible on screen.
;
; What this script does:
;   1. Finds the GW window
;   2. Brings it to the foreground
;   3. Waits for user to position the trade window so an inventory item is visible
;   4. Clicks an inventory item in the trade window to offer it
;   5. The packet tap in the DLL logs the CtoS opcode
;
; Usage:
;   - Launch DISCO with the test harness first (open_cancel test gets the trade open)
;   - Then run this script manually while the trade is open
;   - Check the gwa3_log_<pid>.txt for [PACKET-TAP] lines

#RequireAdmin

; Find the Guild Wars window
Local $hwnd = WinGetHandle("[CLASS:ArenaNet_Dx_Window_Class]")
If @error Then
    MsgBox(16, "Error", "Guild Wars window not found. Launch GW first.")
    Exit
EndIf

; Bring to foreground
WinActivate($hwnd)
WinWaitActive($hwnd, "", 5)
Sleep(500)

; Get window position
Local $pos = WinGetPos($hwnd)
If @error Then
    MsgBox(16, "Error", "Could not get GW window position.")
    Exit
EndIf

Local $winX = $pos[0]
Local $winY = $pos[1]
Local $winW = $pos[2]
Local $winH = $pos[3]

ConsoleWrite("GW window at " & $winX & "," & $winY & " size " & $winW & "x" & $winH & @CRLF)

; The trade window should be open. The inventory panel is typically on the right.
; We need to click on an inventory item to offer it.
;
; Strategy: GW's trade window has the player's offered items on the left
; and the partner's on the right. The inventory backpack is in the bottom-right.
;
; For a 1366x768 or 1920x1080 window:
; Inventory items are roughly in the right side of the screen.
; The first backpack slot is approximately at:
;   x = winW * 0.82, y = winH * 0.18 (top-right inventory area)
;
; We'll double-click an inventory item to offer it to the trade.

; Prompt user
MsgBox(64, "Instructions", _
    "This script will try to offer an inventory item to the trade." & @CRLF & _
    "Make sure:" & @CRLF & _
    "1. The trade window is OPEN with BLUMPKINS" & @CRLF & _
    "2. The inventory panel is visible" & @CRLF & _
    "3. There is at least one item in the backpack" & @CRLF & @CRLF & _
    "Click OK when ready. The script will click on the first inventory slot.")

; Calculate inventory slot position (first slot in backpack)
; These are approximate - adjust based on actual resolution
; For a typical GW window, inventory items start around:
;   Right panel starts at ~80% of window width
;   First row starts at ~15% of window height
Local $slotX = $winX + Int($winW * 0.835)
Local $slotY = $winY + Int($winH * 0.16)

ConsoleWrite("Clicking inventory slot at screen pos " & $slotX & "," & $slotY & @CRLF)

; Double-click the inventory slot to offer the item
; In GW, double-clicking an item in your inventory while a trade window is open
; will offer that item to the trade.
MouseClick("left", $slotX, $slotY, 2, 5)

ConsoleWrite("Clicked. Check the gwa3 log for [PACKET-TAP] lines." & @CRLF)
Sleep(2000)

; Also try right-clicking (some GW versions use right-click to offer)
ConsoleWrite("Also trying right-click..." & @CRLF)
MouseClick("right", $slotX, $slotY, 1, 5)
Sleep(2000)

ConsoleWrite("Done. Check the gwa3_log_*.txt for [PACKET-TAP] entries around this time." & @CRLF)
ConsoleWrite("Look for packets that are NOT hdr=0x28 (action cancel) - those are the trade offer packets." & @CRLF)
