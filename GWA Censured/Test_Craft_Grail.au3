#RequireAdmin
#include "lib\Froggy_Includes.au3"

Global Const $BOTNAME = "Craft Test"
Global Const $VERSION = "1.0"
Global Const $AUTHORS[1] = ["Test"]

ConsoleWrite("=== Full BuyConsumablesInEmbarkBeach Test ===" & @CRLF)

If Not GWLauncher_AutoLaunchAndConnect("B E A S T R I T") Then Exit
ScanAndUpdateGameClients()
SelectClient(FindClientIndexByCharacterName("B E A S T R I T"))
InitializeGameClientData(True, False)
Global $mBasePointer = MemRead(GetScannedAddress('ScanBasePointer', 8))
WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())

ConsoleWrite("Map: " & GetMapID() & " Gold: " & GetGoldCharacter() & @CRLF)

GUI_Create()
Out("=== Starting Full Maintenance Craft Test ===")

BuyConsumablesInEmbarkBeach()

Out("=== Test Complete ===")
ConsoleWrite("=== DONE ===" & @CRLF)
