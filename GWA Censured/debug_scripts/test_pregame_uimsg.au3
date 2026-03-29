#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Test PreGame UIMessages ===" & @CRLF)

; Kill any existing GW and launch fresh DISCO PANIC
ConsoleWrite("Launching DISCO PANIC..." & @CRLF)
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
Local $pid = $result[0]
ConsoleWrite("PID=" & $pid & @CRLF)

Sleep(20000)

ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    If $game_clients[$i][0] = $pid Then $targetIdx = $i
Next
If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $hWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected" & @CRLF)

; Take screenshot
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\pregame_test.png', $hWnd)

; Enable sniffer to see what happens
UISnifferEnable()

; Read UIMessage function address
Local $uiMsgFunc = GetLabel('UIMessage')
ConsoleWrite("UIMessage func: 0x" & Hex($uiMsgFunc) & @CRLF)

; We have CommandUIMsg in our framework which calls SendUIMessage(msgid, wParam, lParam)
; The framework uses a command queue: write command struct, set BasePointer flag
; Let's try sending kSetPreGameContext_Value0 (0x10000183) with chosen character index

; First, let's just try to understand the PreGame state
; From GWCA UIMessages.h:
; kSetPreGameContext_Value0 = 0x10000183, wparam = uint32_t value
; kGetPreGameContext_Value0 = 0x10000185, lparam = *uint32_t value_out

; Try reading PreGameContext
Local $preGameAddr = $pre_game_address
ConsoleWrite("pre_game_address: 0x" & Hex($preGameAddr) & @CRLF)
Local $preGamePtr = MemoryRead($processHandle, $preGameAddr, 'dword')
ConsoleWrite("PreGameContext ptr: 0x" & Hex($preGamePtr) & @CRLF)

If $preGamePtr <> 0 Then
    ; Read chosen_character_index
    Local $chosenIdx = MemoryRead($processHandle, $preGamePtr + 0x124, 'dword')
    ConsoleWrite("chosen_character_index: " & $chosenIdx & @CRLF)

    ; Read frame_id at PreGameContext offset 0x000
    Local $frameId = MemoryRead($processHandle, $preGamePtr, 'dword')
    ConsoleWrite("PreGameContext.frame_id: " & $frameId & @CRLF)

    ; Dump first 16 dwords of PreGameContext to look for dialog state
    ConsoleWrite(@CRLF & "PreGameContext first 64 bytes:" & @CRLF)
    For $off = 0 To 60 Step 4
        Local $val = MemoryRead($processHandle, $preGamePtr + $off, 'dword')
        ConsoleWrite("  +0x" & Hex($off, 3) & ": 0x" & Hex($val, 8) & " (" & $val & ")" & @CRLF)
    Next
EndIf

; Now try using CommandUIMsg to send kSetPreGameContext_Value0
; CommandUIMsg expects a struct: [4 bytes command_id] [4 bytes msgid] [N bytes data]
; But actually, let's check how CommandUIMsg works
ConsoleWrite(@CRLF & "CommandUIMsg label: 0x" & Hex(GetLabel('CommandUIMsg')) & @CRLF)

; The framework's command system:
; 1. Write command struct to queue
; 2. The ASM main loop reads the command
; 3. For CommandUIMsg: calls SendUIMessage(msgid, &data[8], 0)
;
; So to send kSetPreGameContext_Value0 (0x10000183) with wParam = chosen_index:
; We write: [CommandUIMsg_addr][0x10000183][chosen_index]
;
; But wParam for kSetPreGameContext_Value0 is just a uint32_t value, not a pointer
; CommandUIMsg pushes (eax+8) as wParam, which is a POINTER to the data
; But the message expects a VALUE, not a pointer
;
; This won't work directly because CommandUIMsg passes a pointer to the data field
; not the raw value. We'd need to modify the command handler.

; For now, let's just try calling SendUIMessage directly via DllCall
; Actually, we can't - SendUIMessage is a game function in the game process
; We need to call it through our injected ASM

; Let's try a different approach: use the existing QueueCommand mechanism
; with CommandUIMsg, passing the value in the data area

; Flush sniffer counter
Local $counterAddr = GetLabel('UISnifferCounter')
Local $lastCounter = MemoryRead($processHandle, $counterAddr, 'dword')

; Actually, the simplest test: write chosen_character_index directly via PreGameContext
; then see if there's a way to trigger Play
If $preGamePtr <> 0 Then
    ConsoleWrite(@CRLF & "=== Setting chosen_character_index to 0 ===" & @CRLF)
    MemoryWrite($processHandle, $preGamePtr + 0x124, 0, 'dword')
    Local $verify = MemoryRead($processHandle, $preGamePtr + 0x124, 'dword')
    ConsoleWrite("Verified: " & $verify & @CRLF)
    Sleep(2000)
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\pregame_after_select.png', $hWnd)
EndIf

; Check sniffer for any messages after changing character index
Local $msgIdAddr = GetLabel('UISnifferMsgId')
For $p = 1 To 30
    Sleep(100)
    Local $c = MemoryRead($processHandle, $counterAddr, 'dword')
    If $c <> $lastCounter Then
        Local $mid = MemoryRead($processHandle, $msgIdAddr, 'dword')
        ConsoleWrite("  [MSG] 0x" & Hex($mid, 8) & @CRLF)
        $lastCounter = $c
    EndIf
Next

UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
