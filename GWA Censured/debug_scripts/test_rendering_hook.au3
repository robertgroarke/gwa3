#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Test which hooks are active at char select ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf
SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()

; Write test values to shared memory locations and check if the hooks modify them
; The framework has several hooks:
; - MainProc (MainStart) - main game loop
; - RenderingModProc (RenderingMod) - rendering
; - LoadFinishedProc (LoadFinishedStart) - map load
; - TraderProc (TraderStart) - trader
; - TradePartnerProc (TradePartnerStart) - trade partner

; Let's check if MapIsLoaded is being updated (set by RenderingModProc)
Local $mapLoadedAddr = GetLabel('MapIsLoaded')
ConsoleWrite("MapIsLoaded addr: 0x" & Hex(Int($mapLoadedAddr)) & @CRLF)

; Read it multiple times
For $i = 1 To 5
    Local $val = MemoryRead($processHandle, $mapLoadedAddr, 'dword')
    ConsoleWrite("  MapIsLoaded = " & $val & @CRLF)
    Sleep(500)
Next

; Check DisableRendering
Local $disableRendAddr = GetLabel('DisableRendering')
Local $disableVal = MemoryRead($processHandle, $disableRendAddr, 'dword')
ConsoleWrite("DisableRendering = " & $disableVal & @CRLF)

; The simplest test: write a command to the queue and use CreateRemoteThread
; to call the game function directly, bypassing the hook system entirely.
; This IS on a different thread but for simple functions it might work.

; Actually, let's just check if the RenderingMod hook is active by seeing
; if the JMP is installed at the RenderingMod address
Local $rendModAddr = Int(GetLabel('RenderingMod'))
ConsoleWrite(@CRLF & "RenderingMod hook at 0x" & Hex($rendModAddr) & @CRLF)
Local $rendBytes = DllStructCreate('byte[8]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($rendModAddr), _
    'ptr', DllStructGetPtr($rendBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)
Local $hex = ""
For $b = 1 To 8
    $hex &= Hex(DllStructGetData($rendBytes, 1, $b), 2) & " "
Next
ConsoleWrite("  Bytes: " & $hex & @CRLF)
If DllStructGetData($rendBytes, 1, 1) = 0xE9 Then
    ConsoleWrite("  JMP hook IS installed" & @CRLF)
Else
    ConsoleWrite("  NO JMP hook (not hooked)" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
