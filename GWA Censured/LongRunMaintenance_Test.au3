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

Global Const $BOTNAME = "Maintenance Test"
Global Const $VERSION = "1.0"
Global Const $AUTHORS[1] = ["Gene"]
Global $district_name = "Random"

Global $mBasePointer
Global $BotRunning = False

; ==============================================================================
; Long Run Maintenance Test Script
; Purpose: Test maintenance functions for extended bot runs
; ==============================================================================

; NOTE: Configuration constants are now imported from Utils-Maintenance.au3
; NOTE: Material lists are now imported from Utils-Maintenance.au3

; ==============================================================================
; TEST SCRIPT MAIN
; ==============================================================================
Main()

Func Main()
    ; Check if currently in-game and auto-launch
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
    
    Out("=== Long Run Maintenance Test Script ===")
    Out("Character: " & $targetChar)
    Out("Ready. Press Start to begin maintenance loop.")
    Out("")
    
    ; Main Event Loop
    While 1
        If $BotRunning Then
            RunMaintenanceTest()
            ; Add a delay between cycles to avoid spamming if no maintenance is needed
            ; But keep GUI responsive (Sleep in OnEvent mode is okay, events interrupt)
            Sleep(5000) 
        Else
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartBot()
    $BotRunning = True
    Out("Bot Started. Running loop...")
EndFunc

Func StopBot()
    $BotRunning = False
    Out("Bot Stopped.")
EndFunc

; ==============================================================================
; MAINTENANCE LOGIC
; ==============================================================================
Func RunMaintenanceTest()
    Out("--- Starting Cycle ---")
    
    ; Run diagnostic checks - using local version with output
    ; Local $needsMaintenance = Test_RunDiagnostics()
    Local $needsMaintenance = True ; FORCED FOR TESTING
    
    If $needsMaintenance Then
        Out("*** MAINTENANCE RUN NEEDED (FORCED) ***")
        Out("Starting maintenance run...")
        
        ; Call the actual maintenance function from Utils-Maintenance.au3
        ; PerformMaintenance(True, True)
        
        Out("--- TESTING CONSUMABLE BUYING ONLY ---")
        buyConsumablesInEmbarkBeach()
        
        Out("Maintenance run complete!")
    Else
        Out("No maintenance needed at this time.")
    EndIf
    
    Out("Cycle complete.")
    Out("")
EndFunc

; ==============================================================================
; DIAGNOSTIC CHECKS (Local version for detailed output)
; ==============================================================================
Func Test_RunDiagnostics()
    Local $needsMaintenance = False
    
    ; Check inventory slots
    Local $freeSlots = CountFreeInventorySlots()
    Out("Free inventory slots: " & $freeSlots & " (minimum: " & $MIN_FREE_SLOTS & ")")
    If $freeSlots < $MIN_FREE_SLOTS Then
        Out("  -> LOW INVENTORY SPACE!")
        $needsMaintenance = True
    EndIf
    
    ; Check ID kits
    Local $idKitCount = CountItemsByModelID($ID_SUPERIOR_IDENTIFICATION_KIT)
    Local $idKitUses = CountRemainingKitUses($ID_SUPERIOR_IDENTIFICATION_KIT)
    Out("Superior ID Kits: " & $idKitCount & " (uses remaining: " & $idKitUses & ")")
    If $idKitCount < $MIN_ID_KITS Then
        Out("  -> NEED MORE ID KITS!")
        $needsMaintenance = True
    EndIf
    
    ; Check salvage kits
    Local $salvageKitCount = CountItemsByModelID($ID_SALVAGE_KIT)
    Local $salvageKitUses = CountRemainingKitUses($ID_SALVAGE_KIT)
    Out("Salvage Kits: " & $salvageKitCount & " (uses remaining: " & $salvageKitUses & ")")
    If $salvageKitCount < $MIN_SALVAGE_KITS Then
        Out("  -> NEED MORE SALVAGE KITS!")
        $needsMaintenance = True
    EndIf
    
    ; Check gold
    Local $characterGold = GetGoldCharacter()
    Out("Character gold: " & $characterGold & " (max threshold: " & $MAX_CHARACTER_GOLD & ")")
    If $characterGold >= $MAX_CHARACTER_GOLD Then
        Out("  -> GOLD CAP APPROACHING!")
        $needsMaintenance = True
    EndIf
    
    ; Check storage gold
    Local $storageGold = GetGoldStorage()
    Out("Storage gold: " & $storageGold & " (max: 1,000,000)")
    
    Return $needsMaintenance
EndFunc
