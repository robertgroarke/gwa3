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

Global Const $BOTNAME = "Craft Test"
Global Const $VERSION = "1.0"
Global Const $AUTHORS[1] = ["Gene"]
Global $district_name = "Random"

Global $mBasePointer
Global $BotRunning = False

; ==============================================================================
; Standalone Crafting Test Script
; Purpose: Test crafting a Grail of Might at Eyja in Embark Beach
; ==============================================================================
Main()

Func Main()
    ; Connect to game client
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
    
    ; Create the GUI for logging
    GUI_Create()
    GUI_SetOnStartFunc("StartBot")
    GUI_SetOnStopFunc("StopBot")
    
    Out("=== Crafting Test Script ===")
    Out("Character: " & $targetChar)
    Out("Target: Craft 1 Grail of Might at Eyja (Embark Beach)")
    Out("")
    Out("INSTRUCTIONS:")
    Out("  1. Make sure you are in Embark Beach")
    Out("  2. Have 50 Iron Ingots + 50 Glittering Dust in inventory")
    Out("  3. Have at least 250 gold")
    Out("  4. Press Start to begin crafting test")
    Out("")
    
    ; Main Event Loop
    While 1
        If $BotRunning Then
            TestCraftGrail()
            $BotRunning = False
            Out("Test complete. Press Start to repeat.")
        Else
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartBot()
    $BotRunning = True
    Out("Starting crafting test...")
EndFunc

Func StopBot()
    $BotRunning = False
    Out("Stopped.")
EndFunc

; ==============================================================================
; CRAFTING TEST
; ==============================================================================
Func TestCraftGrail()
    Out("--- Crafting Test Start ---")
    
    ; Check prerequisites
    Local $mapID = GetMapID()
    Out("Current Map: " & $mapID & " (Embark Beach = " & $ID_EMBARK_BEACH & ")")
    
    If $mapID <> $ID_EMBARK_BEACH Then
        Out("WARNING: Not in Embark Beach! Attempting to travel...")
        TravelToOutpost($ID_EMBARK_BEACH)
        Sleep(5000)
    EndIf
    
    ; Check materials
    Local $ironCount = GetMaterialCount($ID_IRON_INGOT)
    Local $dustCount = GetMaterialCount($ID_PILE_OF_GLITTERING_DUST)
    Local $gold = GetGoldCharacter()
    
    Out("Iron Ingots: " & $ironCount & " (need 50)")
    Out("Glittering Dust: " & $dustCount & " (need 50)")
    Out("Gold: " & $gold & " (need 250)")
    
    If $ironCount < 50 Or $dustCount < 50 Or $gold < 250 Then
        Out("ERROR: Insufficient materials or gold!")
        Return
    EndIf
    
    ; Navigate to Eyja
    Out("Walking to Eyja (Consumable Trader)...")
    If Not GoToConsumableTrader("Eyja") Then
        Out("ERROR: Could not find Eyja!")
        Return
    EndIf
    
    Out("Merchant window should be open. Checking...")
    Sleep(1000)
    
    If Not GetIsMerchantOpen() Then
        Out("ERROR: Merchant window not open!")
        Return
    EndIf
    
    Local $merchantSize = GetMerchantItemsSize()
    Local $merchantBase = GetMerchantItemsBase()
    Out("Merchant items count: " & $merchantSize & " Base: " & $merchantBase)
    
    ; Debug: Check key function addresses
    Out("Debug: TraderFunction = " & GetValue('TraderFunction'))
    Out("Debug: CommandCraftItemEx2 = " & GetValue('CommandCraftItemEx2'))
    Out("Debug: TradeID = " & GetValue('TradeID'))
    Out("Debug: TradeID_Value = " & GetValue('TradeID_Value'))
    
    ; Debug: Check _GetMerchantItemPtrByModelId_Safe for the Grail
    Local $destPtr = _GetMerchantItemPtrByModelId_Safe($ID_GRAIL_OF_MIGHT)
    Out("Debug: Grail DestPtr from _GetMerchantItemPtrByModelId_Safe = " & $destPtr)
    
    ; Setup materials array (Grail of Might = 50 Iron Ingots + 50 Glittering Dust)
    Local $materials[2][2] = [[$ID_IRON_INGOT, 50], [$ID_PILE_OF_GLITTERING_DUST, 50]]
    
    Out("")
    Out(">>> ATTEMPTING TO CRAFT 1 GRAIL OF MIGHT <<<")
    Out("Using CraftItemSafe with TransactionFunction opcode 3")
    Out("")
    
    Local $result = CraftItemSafe($ID_GRAIL_OF_MIGHT, 1, 250, $materials)
    
    Out("CraftItemSafe returned: " & $result & " @error=" & @error & " @extended=" & @extended)
    
    ; Wait for result
    Sleep(3000)
    
    ; Check results
    Local $newIronCount = GetMaterialCount($ID_IRON_INGOT)
    Local $newDustCount = GetMaterialCount($ID_PILE_OF_GLITTERING_DUST)
    Local $newGold = GetGoldCharacter()
    Local $grailCount = GetMaterialCount($ID_GRAIL_OF_MIGHT)
    
    Out("")
    Out("--- Results ---")
    Out("Iron Ingots: " & $ironCount & " -> " & $newIronCount & " (diff: " & ($ironCount - $newIronCount) & ")")
    Out("Glittering Dust: " & $dustCount & " -> " & $newDustCount & " (diff: " & ($dustCount - $newDustCount) & ")")
    Out("Gold: " & $gold & " -> " & $newGold & " (diff: " & ($gold - $newGold) & ")")
    Out("Grails of Might in inventory: " & $grailCount)
    Out("--- Crafting Test End ---")
EndFunc
