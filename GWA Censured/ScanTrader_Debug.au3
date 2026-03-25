#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\GWA2_ID.au3"
#include "lib\Utils.au3"
#include "lib\Utils-Storage-Bot.au3"
#include "lib\Map_IDs.au3"
#include <GUIConstantsEx.au3>
#include "lib\GUI_Functions.au3"
#include "lib\Utils-Maintenance.au3"

Global Const $BOTNAME = "Trader Scanner"
Global Const $VERSION = "1.2"
Global Const $AUTHORS[1] = ["Gene"]
Global $district_name = "Random"

Global $mBasePointer
Global $BotRunning = False

Main()

Func Main()
    ScanAndUpdateGameClients()
    Local $targetChar = "L I L B I S C U I T"
    Local $clientIndex = FindClientIndexByCharacterName($targetChar)
    
    If $clientIndex > 0 Then
        SelectClient($clientIndex)
        InitializeGameClientData(True, False)
        $mBasePointer = MemoryRead(GetScannedAddress('ScanBasePointer', 8))
        WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())
    Else
        MsgBox(48, "Error", "Character '" & $targetChar & "' not found! Please log in.")
        Exit
    EndIf
    
    GUI_Create()
    GUI_SetOnStartFunc("StartBot")
    GUI_SetOnStopFunc("StopBot")
    
    Out("=== Trader Scanner Debug Script v1.2 ===")
    Out("Character: " & $targetChar)
    Out("Ready. Press Start to scan traders.")
    Out("")
    
    While 1
        If $BotRunning Then
            RunScan()
            $BotRunning = False
            Out("Scan Complete.")
        Else
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartBot()
    $BotRunning = True
    Out("Starting Scan...")
EndFunc

Func StopBot()
    $BotRunning = False
    Out("Scan Stopped.")
EndFunc

Func RunScan()
    Out("--- Scanning Traders ---")
    
    If GetMapID() <> $ID_EMBARK_BEACH Then
        Out("Travel to Embark Beach...")
        TravelToOutpost($ID_EMBARK_BEACH)
        Sleep(4000)
    EndIf

    ScanConsumableTrader("Eyja")
    ScanConsumableTrader("Edwin")
    ScanConsumableTrader("Kwat")
    ScanConsumableTrader("Alcus Nailbiter")
    
    ScanMaterialTrader()
EndFunc

Func ScanConsumableTrader($name)
    Out(">>> Visiting Consumable Trader: " & $name & " <<<")
    If GoToConsumableTrader($name) Then
        Sleep(1000) ; Wait for window
        ScanItems($name)
    Else
        Out("Failed to reach " & $name)
    EndIf
    Out("<<< End " & $name & " >>>")
    Out("")
EndFunc

Func ScanMaterialTrader()
    Out(">>> Visiting Material Trader <<<")
    ; GoToMaterialTrader handles Basic vs Rare logic internally
    Local $startTimer
    
    ; Scan Common Material Trader
    GoToMaterialTrader($ID_EMBARK_BEACH, False)
    Sleep(1000)
    ScanItems("Material Trader (Common)")
    
    ; Scan Rare Material Trader
    GoToMaterialTrader($ID_EMBARK_BEACH, True)
    Sleep(1000)
    ScanItems("Rare Material Trader")
    
    Out("<<< End Material Trader >>>")
    Out("")
EndFunc

Func ScanItems($traderName)
    If Not GetIsMerchantWindowOpen() Then
        Out("Error: Merchant window not open for " & $traderName)
        Return
    EndIf
    
    Local $base = GetMerchantItemsBase()
    Local $count = GetMerchantItemsSize()
    Out("Items Found: " & $count)
    
    Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 0]
    
    For $i = 1 To $count
        Local $itemID = MemoryRead($base + 4 * ($i - 1))
        
        If $itemID <> 0 Then
            $offsets[4] = 4 * $itemID
            Local $itemPtrData = MemoryReadPtr($base_address_ptr, $offsets)
            Local $itemPtr = $itemPtrData[1]
            
            			If $itemPtr <> 0 Then
				; Try reading offsets. GWA2 uses 0x18 usually, but Utils uses 0x2C?
				; Let's read BOTH to be sure.
                Local $first4Bytes = MemoryRead($itemPtr)
				Local $modelID_18 = MemoryRead($itemPtr + 0x18)
				Local $modelID_2C = MemoryRead($itemPtr + 0x2C)
				Out("Slot " & $i & ": ItemID=" & $itemID & " First4Bytes=" & $first4Bytes & " ModelID_18=" & $modelID_18 & " ModelID_2C=" & $modelID_2C)
			Else
				Out("Slot " & $i & ": ItemPtr=0 for ItemID=" & $itemID)
			EndIf
        EndIf
    Next
EndFunc

Func GetIsMerchantWindowOpen()
    Return GetMerchantItemsBase() <> 0
EndFunc
