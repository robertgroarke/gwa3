#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\GWA2_ID.au3"
#include "lib\Utils.au3"
#include "lib\Utils-Storage-Bot.au3"
#include "lib\Map_IDs.au3"
#include <GUIConstantsEx.au3>

; ==============================================================================
; Long Run Maintenance Test Script
; Purpose: Test maintenance functions for extended bot runs
; ==============================================================================

; Configuration
Global Const $MIN_FREE_SLOTS = 7               ; Minimum free inventory slots before triggering maintenance
Global Const $MIN_ID_KITS = 1                   ; Minimum Superior ID kits before restocking
Global Const $MIN_SALVAGE_KITS = 1              ; Minimum Salvage kits before restocking
Global Const $MAX_CHARACTER_GOLD = 95000        ; Max gold before depositing (100k is hard cap)
Global Const $TARGET_ID_KITS = 3                ; Target number of Superior ID kits
Global Const $TARGET_SALVAGE_KITS = 8           ; Target number of Salvage kits

; Materials to KEEP (used for consumable crafting at Embark Beach)
; Iron - Grail of Might, Armor of Salvation
; Dust - Grail of Might, Essence of Celerity
; Bones - Scroll of Resurrection, Armor of Salvation
; Feathers - Essence of Celerity
; Granite - Powerstone of Courage
; Plant Fiber - Scroll of Resurrection
; Scales - (keep for potential future use)
; ALL RARE MATERIALS - never sell to merchant
Global Const $KEEP_MATERIALS[] = [ _
    $ID_IRON_INGOT, _
    $ID_PILE_OF_GLITTERING_DUST, _
    $ID_BONE, _
    $ID_FEATHER, _
    $ID_GRANITE_SLAB, _
    $ID_PLANT_FIBER, _
    $ID_SCALE _
]

; Materials to SELL (useless for consumable crafting)
Global Const $SELL_MATERIALS[] = [ _
    $ID_BOLT_OF_CLOTH, _
    $ID_TANNED_HIDE_SQUARE, _
    $ID_WOOD_PLANK _
]

; Town for maintenance run (Gadd's Encampment has all needed NPCs)
Global Const $MAINTENANCE_TOWN = $ID_GADDS_CAMP

; ==============================================================================
; TEST SCRIPT MAIN
; ==============================================================================
Main()

Func Main()
    Out("=== Long Run Maintenance Test Script ===")
    Out("")
    
    ; Check if currently in-game
    If Not GUIInitialize(GetCurrentInstancePID(), True, True, False, True) Then
        MsgBox(0, "Error", "Could not initialize GWA2. Make sure Guild Wars is running.")
        Exit
    EndIf
    
    Out("Running maintenance checks...")
    Out("")
    
    ; Run diagnostic checks
    Local $needsMaintenance = RunDiagnostics()
    
    Out("")
    If $needsMaintenance Then
        Out("*** MAINTENANCE RUN NEEDED ***")
        Out("")
        
        ; Ask user if they want to proceed
        Local $result = MsgBox(4, "Maintenance Required", "Maintenance run is needed. Proceed?")
        If $result = 6 Then ; Yes
            Out("Starting maintenance run...")
            DoMaintenanceRun()
            Out("Maintenance run complete!")
        Else
            Out("Maintenance run cancelled by user.")
        EndIf
    Else
        Out("No maintenance needed at this time.")
    EndIf
    
    Out("")
    Out("=== Test Complete ===")
EndFunc

; ==============================================================================
; DIAGNOSTIC CHECKS
; ==============================================================================
Func RunDiagnostics()
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

; ==============================================================================
; NEEDS MAINTENANCE CHECK (for integration with main bot)
; ==============================================================================
Func NeedsMaintenanceRun()
    ; Check inventory slots
    If CountFreeInventorySlots() < $MIN_FREE_SLOTS Then Return True
    
    ; Check ID kits
    If CountItemsByModelID($ID_SUPERIOR_IDENTIFICATION_KIT) < $MIN_ID_KITS Then Return True
    
    ; Check salvage kits  
    If CountItemsByModelID($ID_SALVAGE_KIT) < $MIN_SALVAGE_KITS Then Return True
    
    ; Check gold
    If GetGoldCharacter() >= $MAX_CHARACTER_GOLD Then Return True
    
    Return False
EndFunc

; ==============================================================================
; MAINTENANCE RUN
; ==============================================================================
Func DoMaintenanceRun()
    Out("Step 1: Traveling to Gadd's Encampment")
    ; TODO: TravelToOutpost($MAINTENANCE_TOWN)
    
    Out("Step 2: Selling items to merchant")
    ; Sell gold weapons that aren't rare skins and aren't material salvageable
    ; TODO: SellItemsToMerchant(ShouldSellItemForMaintenance)
    
    Out("Step 3: Selling unwanted materials to material trader")
    ; Only sell cloth, hides, wood planks - keep all crafting mats
    ; TODO: SellBasicMaterialsToMerchant(ShouldSellMaterialForMaintenance)
    
    Out("Step 4: Buying kits from merchant")
    ; Buy Superior ID kits until we have 3, Salvage kits until we have 8
    ; TODO: BuyKitsUntilTarget()
    
    Out("Step 5: Opening Xunlai chest")
    ; TODO: GoToXunlaiChest()
    
    Out("Step 6: Depositing gold/platinum")
    ; Deposit all but ~5000g for kit purchases
    ; TODO: DepositGold(GetGoldCharacter() - 5000)
    
    Out("Step 7: Depositing tomes")
    ; TODO: StoreItemsInXunlaiStorage(IsTome)
    
    Out("Step 8: Grabbing amphibian tongues from unclaimed items")
    ; TODO: GrabAmphibianTongues()
    
    Out("Step 9: Returning to Bogroot Growths")
    ; TODO: TravelToOutpost($ID_BOGROOT_GROWTHS)
EndFunc

; ==============================================================================
; HELPER FUNCTIONS
; ==============================================================================

; Count free inventory slots across bags 1-4
Func CountFreeInventorySlots()
    Local $freeSlots = 0
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        Local $totalSlots = DllStructGetData($bag, 'slots')
        Local $usedSlots = DllStructGetData($bag, 'ItemCount')
        $freeSlots += ($totalSlots - $usedSlots)
    Next
    Return $freeSlots
EndFunc

; Count items by model ID across bags 1-4
Func CountItemsByModelID($modelID)
    Local $count = 0
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bag, 'slots')
            Local $item = GetItemBySlot($bagIndex, $slot)
            If DllStructGetData($item, 'ModelID') = $modelID Then
                $count += 1
            EndIf
        Next
    Next
    Return $count
EndFunc

; Check if material should be sold (return True if in SELL_MATERIALS list)
Func ShouldSellMaterialForMaintenance($item)
    If Not IsBasicMaterial($item) Then Return False
    Local $itemID = DllStructGetData($item, 'ModelID')
    For $sellID In $SELL_MATERIALS
        If $itemID = $sellID Then Return True
    Next
    Return False
EndFunc

; Check if item should be sold to merchant
Func ShouldSellItemForMaintenance($item)
    Local $itemID = DllStructGetData($item, 'ModelID')
    Local $rarity = GetRarity($item)
    
    ; Don't sell kits
    If $itemID = $ID_SUPERIOR_IDENTIFICATION_KIT Then Return False
    If $itemID = $ID_SALVAGE_KIT Then Return False
    If $itemID = $ID_EXPERT_SALVAGE_KIT Then Return False
    If $itemID = $ID_SUPERIOR_SALVAGE_KIT Then Return False
    
    ; Sell gold weapons that are not rare skins
    If IsWeapon($item) And $rarity = $RARITY_GOLD Then
        If _ArraySearch($aRareSkins, $itemID) = -1 Then
            ; Not a rare skin, can sell
            If GetIsIdentified($item) And Not DllStructGetData($item, 'IsMaterialSalvageable') Then
                Return True
            EndIf
        EndIf
    EndIf
    
    Return False
EndFunc

; Simple output function
Func Out($msg)
    ConsoleWrite($msg & @CRLF)
EndFunc
