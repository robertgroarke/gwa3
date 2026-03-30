#include-once
#include "..\botshub\Utils-Storage.au3"
#include "Utils-Salvage.au3"

; Configuration for Maintenance
Global Const $MIN_FREE_SLOTS = 7               ; Minimum free inventory slots before triggering maintenance
Global Const $MIN_ID_KITS = 1                   ; Minimum Superior ID kits before restocking
Global Const $MIN_SALVAGE_KITS = 1              ; Minimum Salvage kits before restocking
Global Const $MAX_CHARACTER_GOLD = 95000        ; Max gold before depositing (100k is hard cap)
Global Const $TARGET_ID_KITS = 3                ; Target number of Superior ID kits
Global Const $TARGET_SALVAGE_KITS = 8           ; Target number of Salvage kits
Global Const $MAINTENANCE_TOWN = $ID_GADDS_CAMP
Global Const $CONSET_BUY_INTERVAL = 10           ; Buy consets every N runs
Global $g_LastConsetBuyRun = -1                   ; Run count at last conset purchase (-1 = never)

; Materials to KEEP
Global Const $KEEP_MATERIALS[] = [ _
    $ID_IRON_INGOT, _
    $ID_PILE_OF_GLITTERING_DUST, _
    $ID_BONE, _
    $ID_FEATHER, _
    $ID_GRANITE_SLAB, _
    $ID_PLANT_FIBER, _
    $ID_SCALE _
]

; Materials to SELL
Global Const $SELL_MATERIALS[] = [ _
    $ID_BOLT_OF_CLOTH, _
    $ID_TANNED_HIDE_SQUARE, _
    $ID_WOOD_PLANK _
]



; ==============================================================================
; DIAGNOSTIC CHECKS
; ==============================================================================
Func RunDiagnostics()
    Local $needsMaintenance = False
    
    ; Check inventory slots
    Local $freeSlots = CountFreeInventorySlots()
    If $freeSlots < $MIN_FREE_SLOTS Then $needsMaintenance = True
    
    ; Check ID kits
    Local $idKitCount = CountItemsByModelID($ID_SUPERIOR_IDENTIFICATION_KIT)
    If $idKitCount < $MIN_ID_KITS Then $needsMaintenance = True
    
    ; Check salvage kits
    Local $salvageKitCount = CountItemsByModelID($ID_SALVAGE_KIT)
    Local $salvageKitCorrect = CountItemsByModelID(2989) ; Basic Kit
    Local $salvageKitExpert = CountItemsByModelID($ID_EXPERT_SALVAGE_KIT)
    If ($salvageKitCount + $salvageKitCorrect + $salvageKitExpert) < $MIN_SALVAGE_KITS Then $needsMaintenance = True
    
    ; Check gold
    Local $characterGold = GetGoldCharacter()
    If $characterGold >= $MAX_CHARACTER_GOLD Then $needsMaintenance = True

    ; Check bank gold — if over 800k, should buy consets
    Local $storageGold = GetGoldStorage()
    If $storageGold >= 800000 Then $needsMaintenance = True

    Return $needsMaintenance
EndFunc

; ==============================================================================
; MAINTENANCE RUN
; ==============================================================================
Func PerformMaintenance($force = False, $buyConsumables = Default)
    If Not $force And Not RunDiagnostics() Then Return

    ; Default: buy consumables when GUI "Buy Consets" checkbox is enabled
    If $buyConsumables = Default Then $buyConsumables = GUI_IsBuyConsetsChecked()

    Out("Starting Maintenance Run... (BuyConsumables=" & $buyConsumables & ")")
    
    If GetMapID() <> $MAINTENANCE_TOWN Then
        TravelToOutpost($MAINTENANCE_TOWN)
        Sleep(2000) ; Wait for load
    EndIf
    
    ClaimSpecificItem(27036) ; Amphibian Tongues
    SalvageAmphibianTongues()
    IdentifyUnidentifiedItemsForMaintenance()
    
    If GetGoldCharacter() > 80000 Then
        GoToXunlaiChest($MAINTENANCE_TOWN)
        DepositGold(GetGoldCharacter() - 10000)
        Sleep(1000)
    EndIf
    
    SellItemsToMerchant(ShouldSellItemForMaintenance, False, $MAINTENANCE_TOWN)
    
    If HasOverallBasicMaterialsToSell() Then
        SellBasicMaterialsToMerchant(ShouldSellMaterialForMaintenance, $MAINTENANCE_TOWN)
    EndIf
    
    GoToXunlaiChest($MAINTENANCE_TOWN)
    If GetGoldCharacter() > 5000 Then DepositGold(GetGoldCharacter() - 5000)
    StoreItemsInXunlaiStorageSafe("ShouldStoreTome")

    ; Buy Consumables (Consets) every N runs, or on first maintenance if bank gold is high
    If $buyConsumables Then
        Local $runsSinceBuy = $GUI_RunCounter - $g_LastConsetBuyRun
        Local $intervalReached = ($runsSinceBuy >= $CONSET_BUY_INTERVAL)
        Local $bankRich = (GetGoldStorage() >= 800000)

        If $intervalReached Or $bankRich Then
            Out("Conset buy triggered (runs=" & $runsSinceBuy & " bank=" & GetGoldStorage() & ")")
            $g_LastConsetBuyRun = $GUI_RunCounter
            BuyConsumablesInEmbarkBeach()
        EndIf
    EndIf

    ; Buy kits LAST — after conset buying, tongue salvaging, and all selling.
    ; This ensures we leave maintenance with kits in inventory.
    BuyKitsUntilTarget()

    ; Final gold deposit — keep enough for kits + crafting overhead
    If GetMapID() <> $MAINTENANCE_TOWN Then TravelToOutpost($MAINTENANCE_TOWN)
    GoToXunlaiChest($MAINTENANCE_TOWN)
    If GetGoldCharacter() > 10000 Then DepositGold(GetGoldCharacter() - 10000)

    Out("Maintenance Complete")
EndFunc

; ==============================================================================
; HELPER FUNCTIONS



Func BuyConsumablesInEmbarkBeach()
    ; 0. Travel to Embark Beach FIRST (Center of operations)
    If GetMapID() <> $ID_EMBARK_BEACH Then
        TravelToOutpost($ID_EMBARK_BEACH)
        Sleep(4000) ; Wait for load
    EndIf

    ; Recipes (per unit):
    ; Grail (Eyja):      50 Iron    + 50 Dust    + 250g
    ; Essence (Kwat):    50 Feather + 50 Dust    + 250g
    ; Armor (Alcus):     50 Iron    + 50 Bone    + 250g
    ; Scroll (Edwin):    25 Fiber   + 25 Bone    + 250g
    ; Powerstone (Edwin):100 Granite + 100 Dust  + 1000g

    ; How many consets to craft (1 conset = 1 Grail + 1 Essence + 1 Armor)
    Local $targetSets = 5

    ; Total materials needed for $targetSets consets:
    ;   Iron:    50*sets (Grail) + 50*sets (Armor) = 100*sets
    ;   Dust:    50*sets (Grail) + 50*sets (Essence) = 100*sets
    ;   Bone:    50*sets (Armor) = 50*sets
    ;   Feather: 50*sets (Essence) = 50*sets
    Local $needIron    = $targetSets * 100
    Local $needDust    = $targetSets * 100
    Local $needBone    = $targetSets * 50
    Local $needFeather = $targetSets * 50

    Local $haveIron    = GetMaterialCount($ID_IRON_INGOT)
    Local $haveDust    = GetMaterialCount($ID_PILE_OF_GLITTERING_DUST)
    Local $haveBone    = GetMaterialCount($ID_BONE)
    Local $haveFeather = GetMaterialCount($ID_FEATHER)

    Out("Conset plan: " & $targetSets & " sets. Need Iron=" & $needIron & " Dust=" & $needDust & " Bone=" & $needBone & " Feather=" & $needFeather)
    Out("Have: Iron=" & $haveIron & " Dust=" & $haveDust & " Bone=" & $haveBone & " Feather=" & $haveFeather)

    ; Pull gold from bank so we can afford materials
    If GetGoldCharacter() < 100000 Then
        GoToXunlaiChest($ID_EMBARK_BEACH)
        WithdrawGold(100000 - GetGoldCharacter())
        Sleep(500)
        Out("Withdrew gold, now have " & GetGoldCharacter())
    EndIf

    ; 3. Buy ALL materials first, then craft at each trader
    ;    This avoids walking back and forth between traders and material trader
    Local $missingIron    = $needIron - GetMaterialCount($ID_IRON_INGOT)
    Local $missingDust    = $needDust - GetMaterialCount($ID_PILE_OF_GLITTERING_DUST)
    Local $missingBone    = $needBone - GetMaterialCount($ID_BONE)
    Local $missingFeather = $needFeather - GetMaterialCount($ID_FEATHER)

    Out("Buying: Iron=" & $missingIron & " Dust=" & $missingDust & " Bone=" & $missingBone & " Feather=" & $missingFeather)

    If $missingIron > 0 Then BuyMaterialSafe($ID_IRON_INGOT, $missingIron)
    If $missingDust > 0 Then BuyMaterialSafe($ID_PILE_OF_GLITTERING_DUST, $missingDust)
    If $missingBone > 0 Then BuyMaterialSafe($ID_BONE, $missingBone)
    If $missingFeather > 0 Then BuyMaterialSafe($ID_FEATHER, $missingFeather)

    ; 4. Craft at each trader
    If GetGoldCharacter() < 10000 Then RefuelGold($ID_EMBARK_BEACH)

    ; Grails at Eyja (50 Iron + 50 Dust each)
    Out("Crafting " & $targetSets & " Grails at Eyja...")
    If GoToConsumableTrader("Eyja") Then
        Local $grailMats[2][2] = [[$ID_IRON_INGOT, 50], [$ID_PILE_OF_GLITTERING_DUST, 50]]
        BuyConsumableChunk($ID_GRAIL_OF_MIGHT, $targetSets, 250, $grailMats)
        GoToXunlaiChest($ID_EMBARK_BEACH)
        StoreItemsInXunlaiStorageSafe("ShouldStoreMaintenanceItems")
    EndIf

    If GetGoldCharacter() < 10000 Then RefuelGold($ID_EMBARK_BEACH)

    ; Essence at Kwat (50 Feather + 50 Dust each)
    Out("Crafting " & $targetSets & " Essences at Kwat...")
    If GoToConsumableTrader("Kwat") Then
        Local $essenceMats[2][2] = [[$ID_FEATHER, 50], [$ID_PILE_OF_GLITTERING_DUST, 50]]
        BuyConsumableChunk($ID_ESSENCE_OF_CELERITY, $targetSets, 250, $essenceMats)
        GoToXunlaiChest($ID_EMBARK_BEACH)
        StoreItemsInXunlaiStorageSafe("ShouldStoreMaintenanceItems")
    EndIf

    If GetGoldCharacter() < 10000 Then RefuelGold($ID_EMBARK_BEACH)

    ; Armor at Alcus (50 Iron + 50 Bone each)
    Out("Crafting " & $targetSets & " Armors at Alcus...")
    If GoToConsumableTrader("Alcus Nailbiter") Then
        Local $armorMats[2][2] = [[$ID_IRON_INGOT, 50], [$ID_BONE, 50]]
        BuyConsumableChunk($ID_ARMOR_OF_SALVATION, $targetSets, 250, $armorMats)
        GoToXunlaiChest($ID_EMBARK_BEACH)
        StoreItemsInXunlaiStorageSafe("ShouldStoreMaintenanceItems")
    EndIf

    ; Final Clean up
    GoToXunlaiChest($ID_EMBARK_BEACH)
    If GetGoldCharacter() > 5000 Then DepositGold(GetGoldCharacter() - 5000)

    TravelToOutpost($MAINTENANCE_TOWN)
EndFunc


Func GoToRareMaterialTrader($townID)
    Return GoToMaterialTrader($townID, True)
EndFunc


Func BuyConsumableChunk($itemID, $amount, $unitCost, $materials = 0)
    If $amount <= 0 Then Return
    Out("Debug: BuyConsumableChunk ID=" & $itemID & " Amount=" & $amount & " Cost=" & $unitCost)

    ; Caller must already be at the correct trader with dialog open

    If IsArray($materials) Then
        ; Use UI frame-based crafting (CraftItemSafe packet approach is broken)
        ; Map item IDs to their position in each trader's craft list
        Local $craftIndex = _GetCraftItemIndex($itemID)
        Out("Crafting " & $amount & " items via UI (index " & $craftIndex & ")...")
        CraftConsumableByUI($craftIndex, $amount)
        Sleep(1000)
        Return
    EndIf

    Local $slot = GetMerchantItemSlot($itemID)
    Out("Debug: Slot for ID " & $itemID & " is " & $slot)
    
    If $slot == 0 Then 
        Out("Error: Could not find consumable ID " & $itemID & " at trader.")
        Return
    EndIf
    
    Out("Buying " & $amount & " items one by one...")
    For $i = 1 To $amount
        BuyItem($slot, 1, $unitCost)
        Sleep(500)
    Next
EndFunc

Func UpdateInventory()
    Out("Updating inventory state...")
    For $i = 1 To 4
        GetBag($i)
    Next
EndFunc


Func GetMerchantItemSlot($modelID)
    Local $base = GetMerchantItemsBase()
    Local $count = GetMerchantItemsSize()
    Out("Debug: MerchantBase=" & $base & " Count=" & $count)
    
    If $base = 0 Then Return 0
    
    Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 0]
    
    For $i = 1 To $count
        Local $itemID = MemRead($base + 4 * ($i - 1))
        
        If $itemID <> 0 Then
            $offsets[4] = 4 * $itemID
            Local $itemPtrData = MemReadPtr($base_address_ptr, $offsets)
            Local $itemPtr = $itemPtrData[1]
            
            If $itemPtr <> 0 Then
                Local $id = MemRead($itemPtr + 0x2C)
                Out("Debug: Slot " & $i & " ModelID=" & $id)
                If $id = $modelID Then Return $i
            EndIf
        EndIf
    Next
    Out("Debug: Item " & $modelID & " not found.")
    Return 0
EndFunc

Func GetMaterialCount($modelID)
    Local $count = 0
    For $bag = 1 To 4
        Local $bagStruct = GetBag($bag)
        If DllStructGetData($bagStruct, 'ID') == 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bagStruct, 'slots')
            Local $item = GetItemBySlot($bag, $slot)
            If IsDllStruct($item) And DllStructGetData($item, 'ModelID') == $modelID Then
                $count += DllStructGetData($item, 'Quantity')
            EndIf
        Next
    Next
    Return $count
EndFunc

Func GoToMaterialTrader($townID, $forceRare = False)
    Local $traderName = "Basic material trader"
    If $forceRare Then $traderName = "Rare material trader"

    Local $coords = NPCCoordinatesInTown($townID, $traderName)
    If $coords[0] = 0 Or $coords[0] = -1 Then
        $traderName = "Rare material trader"
        $coords = NPCCoordinatesInTown($townID, $traderName)
    EndIf

    If $coords[0] = 0 Or $coords[0] = -1 Then
        Out("Could not find Material Trader in town " & $townID)
        Return False
    EndIf

    MoveTo($coords[0], $coords[1])
    Sleep(2000)

    ; Retry NPC finding — agents may take time to load after walking
    Local $npc = Null
    For $attempt = 1 To 4
        $npc = GetNearestNPCToCoords($coords[0], $coords[1])
        If IsDllStruct($npc) Then ExitLoop
        Sleep(2000)
    Next

    If Not IsDllStruct($npc) Then
        Out("Material Trader NPC not in range at (" & $coords[0] & "," & $coords[1] & ")")
        Return False
    EndIf

    GoToNPC($npc)
    Sleep(500)
    Dialog($npc)
    Sleep(1000)
    Return True
EndFunc

Func GoToConsumableTrader($name)
    Local $coords = NPCCoordinatesInTown($ID_EMBARK_BEACH, 'Consumables trader', $name)
    
    If $coords[0] <> 0 And $coords[0] <> -1 Then
        MoveTo($coords[0], $coords[1])
        Local $npc = GetNearestNPCToCoords($coords[0], $coords[1])
        GoToNPC($npc)
        RandomSleep(500)
        Dialog($npc)
        Sleep(1000)
        Return True
    Else
        Out("Could not find Consumable Trader: " & $name)
        Return False
    EndIf
EndFunc


Func BuyMaterialIfMissing($modelID, $amountNeeded, $batchSize = 10)
    If $amountNeeded <= 0 Then Return
    Out("Buying " & $amountNeeded & " of material " & $modelID & " (Batch Size: " & $batchSize & ")")

    ; Request Quote
    If Not TraderRequest($modelID) Then
        Out("Failed to get quote for material " & $modelID)
        Return
    EndIf
    
    Sleep(500)
    Local $cost = GetTraderCostValue()
    
    If $cost > 0 Then
        ; Calculate number of packs/items to buy
        Local $packs = Ceiling($amountNeeded / $batchSize)
        Out("Requesting " & $packs & " packs from trader.")
        
        Local $bought = 0
        While $bought < $packs
            $cost = GetTraderCostValue()
            If $cost == 0 Then 
                Out("Use TraderRequest failed or cost is 0")
                ExitLoop
            EndIf
            
            If GetGoldCharacter() < $cost Then
                Out("Not enough gold to buy pack.")
                ExitLoop
            EndIf

            TraderBuy()
            $bought += 1
            Sleep(GetPing() + 200)
            
            ; Re-request quote
            If Not TraderRequest($modelID) Then ExitLoop
        WEnd
        Out("Bought " & $bought & " packs.")
    Else
        Out("Error: Trader cost is 0 or quote failed.")
    EndIf
EndFunc

Func RefuelGold($townID)
    Out("Refueling Gold...")
    GoToXunlaiChest($townID)
    WithdrawGold(100000 - GetGoldCharacter())
EndFunc

Func BuyMaterialSafe($id, $amount)
    ; All conset materials (iron, bone, dust, feather, granite, fiber) are at the basic trader.
    ; Rare trader is for ecto, ruby, sapphire etc. Only use rare for actual rare materials.
    Local $useRare = False
    If Not GoToMaterialTrader($ID_EMBARK_BEACH, $useRare) Then
        Out("Failed to reach material trader for ID " & $id)
        Return
    EndIf

    For $retry = 1 To 3
        BuyMaterialIfMissing($id, $amount, 10)
        Sleep(500)
        If GetMaterialCount($id) >= $amount Then Return
        If $retry < 3 Then Out("Retry " & $retry & " buy material " & $id)
    Next
    
    If GetGoldCharacter() < 5000 Then
        RefuelGold($ID_EMBARK_BEACH)
        GoToMaterialTrader($ID_EMBARK_BEACH, $useRare)
        ; Try one last time after refuel
        BuyMaterialIfMissing($id, $amount, 10)
    EndIf
EndFunc

; Map consumable item IDs to their position in the trader's craft list
; Each trader has items in a specific order — index 0 is the first item shown
Func _GetCraftItemIndex($itemID)
    Switch $itemID
        ; Eyja: Grail(0), Scroll(1), Star(2), PerfSalvKit(3), ArcticStone(4)
        Case $ID_GRAIL_OF_MIGHT
            Return 0
        ; Kwat: Essence(0), ...
        Case $ID_ESSENCE_OF_CELERITY
            Return 0
        ; Alcus Nailbiter: Armor(0), ...
        Case $ID_ARMOR_OF_SALVATION
            Return 0
        ; Edwin: Powerstone(0), Scroll of Res(1), ...
        Case $ID_POWERSTONE_OF_COURAGE
            Return 0
        Case $ID_SCROLL_OF_RESURRECTION
            Return 1
        Case Else
            Out("Warning: Unknown craft item ID " & $itemID & ", defaulting to index 0")
            Return 0
    EndSwitch
EndFunc

Func IsMaintenanceKit($itemID)
    Switch $itemID
        Case 2992, 2989, 235, 243 
            Return True
    EndSwitch
    If $itemID = $ID_SUPERIOR_IDENTIFICATION_KIT Then Return True
    If $itemID = $ID_EXPERT_SALVAGE_KIT Then Return True
    If $itemID = $ID_SALVAGE_KIT Then Return True
    Return False
EndFunc

Func ShouldStoreMaintenanceItems($item)
    Local $itemID = DllStructGetData($item, 'ModelID')
    ; Whitelist Consumables ONLY
    Switch $itemID
        Case $ID_GRAIL_OF_MIGHT, $ID_ESSENCE_OF_CELERITY, $ID_ARMOR_OF_SALVATION, $ID_POWERSTONE_OF_COURAGE, $ID_SCROLL_OF_RESURRECTION
            Return True
    EndSwitch
    Return False
EndFunc

Func StoreItemsInXunlaiStorageSafe($shouldStoreFunc)
    Out('Storing items (Safe Mode)')
    Local $item, $itemID


    
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        For $i = 1 To DllStructGetData($bag, 'slots')
            $item = GetItemBySlot($bagIndex, $i)
            $itemID = DllStructGetData($item, 'ModelID')
            
            If $itemID <> 0 Then
                Local $shouldStore = Call($shouldStoreFunc, $item)
                If @error Then 
                    Out("Error calling " & $shouldStoreFunc)
                EndIf
                
                If $shouldStore Then
                    Out('Storing item ' & $itemID & ' from bag ' & $bagIndex)
                    If Not StoreItemInXunlaiStorage($item) Then Return False
                    Sleep(50)
                EndIf
            EndIf
        Next
    Next
EndFunc




Func GetItemCountInStorageAndInventory($modelID)
    Local $count = 0
    ; Inventory
    $count += GetMaterialCount($modelID)
    ; Storage
    ; ... (Need Storage counting logic, usually complex due to multiple bags)
    ; For now, assume Inventory only? Or rely on `GetCountInStorage` if available.
    ; `Utils-Storage-Bot.au3` might have it.
    Return $count
EndFunc
; ==============================================================================

Func CountFreeInventorySlots()
    Local $freeSlots = 0
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        Local $totalSlots = DllStructGetData($bag, 'slots')
        For $slot = 1 To $totalSlots
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Or DllStructGetData($item, 'ModelID') = 0 Then
                $freeSlots += 1
            EndIf
        Next
    Next
    Return $freeSlots
EndFunc

Func IdentifyUnidentifiedItemsForMaintenance()
    Local $idKits = CountItemsByModelID($ID_SUPERIOR_IDENTIFICATION_KIT)
    If $idKits == 0 Then BuyKitsUntilTarget()

    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bag, 'slots')
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            
            Local $itemID = DllStructGetData($item, 'ModelID')
            If $itemID = 0 Then ContinueLoop

            If Not GetIsIdentified($item) Then
                If IsRareSkin($itemID) Then ContinueLoop
                IdentifyItem($item)
                Sleep(1000)
            EndIf
        Next
    Next
EndFunc

Func CountItemsByModelID($modelID)
    Local $count = 0
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bag, 'slots')
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            If DllStructGetData($item, 'ModelID') = $modelID Then
                $count += 1
            EndIf
        Next
    Next
    Return $count
EndFunc

Func ShouldSellMaterialForMaintenance($item)
    If Not IsBasicMaterial($item) Then Return False
    Local $itemID = DllStructGetData($item, 'ModelID')
    
    For $sellID In $SELL_MATERIALS
        If $itemID = $sellID Then Return False
    Next
    
    For $keepID In $KEEP_MATERIALS
        If $itemID = $keepID Then Return False
    Next

    Return True
EndFunc

Func HasOverallBasicMaterialsToSell()
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bag, 'slots')
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            If ShouldSellMaterialForMaintenance($item) Then Return True
        Next
    Next
    Return False
EndFunc

Func ShouldStoreTome($item)
    Return IsTome(DllStructGetData($item, 'ModelID'))
EndFunc



Func BuyKitsUntilTarget()
    Local $idKitsTarget = 3
    Local $salvageKitsTarget = 8
    
    Local $countSup = CountItemsByModelID($ID_SUPERIOR_IDENTIFICATION_KIT)
    Local $countSupAlt = CountItemsByModelID(235)
    Local $currentIDKits = $countSup + $countSupAlt
    
    Local $countSalv = CountItemsByModelID($ID_EXPERT_SALVAGE_KIT)
    Local $countSalvBasic = CountItemsByModelID($ID_SALVAGE_KIT)
    Local $countSalvAlt = CountItemsByModelID(243)
    Local $currentSalvageKits = $countSalv + $countSalvBasic + $countSalvAlt
    
    If $currentIDKits >= $idKitsTarget And $currentSalvageKits >= $salvageKitsTarget Then Return
    
    if GetMapID() <> $MAINTENANCE_TOWN Then TravelToOutpost($MAINTENANCE_TOWN)
    
    Local $coords = NPCCoordinatesInTown($MAINTENANCE_TOWN, 'Merchant')
    MoveTo($coords[0], $coords[1])
    Local $merchant = GetNearestNPCToCoords($coords[0], $coords[1])
    GoToNPC($merchant)
    RandomSleep(500)
    
    Dialog($merchant)
    Sleep(2000)
    
    Local $merchantBase = GetMerchantItemsBase()
    If $merchantBase == 0 Then Return
    
    If $currentIDKits < $idKitsTarget Then
        Local $buyCount = $idKitsTarget - $currentIDKits
        Local $pos = GetMerchantItemPosition($ID_SUPERIOR_IDENTIFICATION_KIT)
        If $pos = 0 Then $pos = GetMerchantItemPosition(235)
        
        If $pos > 0 Then 
            BuyItem($pos, $buyCount, 500)
            Sleep(1000)
        EndIf
    EndIf
    
    If $currentSalvageKits < $salvageKitsTarget Then
        Local $buyCount = $salvageKitsTarget - $currentSalvageKits
        
        Local $pos = GetMerchantItemPosition($ID_SALVAGE_KIT)
        If $pos = 0 Then $pos = GetMerchantItemPosition(2989)
        If $pos = 0 Then $pos = GetMerchantItemPosition(243)
        
        If $pos > 0 Then 
            BuyItem($pos, $buyCount, 100)
            Sleep(1000)
        Else
            $pos = GetMerchantItemPosition($ID_EXPERT_SALVAGE_KIT)
            If $pos = 0 Then $pos = GetMerchantItemPosition(2992)
            
            If $pos > 0 Then
                BuyItem($pos, $buyCount, 400)
                Sleep(1000)
            EndIf
        EndIf
    EndIf
EndFunc

Func GetMerchantItemPosition($modelID)
    Local $base = GetMerchantItemsBase()
    If $base = 0 Then Return 0
    Local $count = GetMerchantItemsSize()
    
    Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 0]
    
    For $i = 1 To $count
        Local $itemID = MemRead($base + 4 * ($i - 1))
        
        If $itemID <> 0 Then
            $offsets[4] = 4 * $itemID
            Local $itemPtrData = MemReadPtr($base_address_ptr, $offsets)
            Local $itemPtr = $itemPtrData[1]
            
            If $itemPtr <> 0 Then
                Local $id = MemRead($itemPtr + 0x2C)
                If $id = $modelID Then Return $i
            EndIf
        EndIf
    Next
    Return 0
EndFunc

Func ShouldSellItemForMaintenance($item)
    Local $itemID = DllStructGetData($item, 'ModelID')
    If IsBasicMaterial($item) Then
        For $sellID In $SELL_MATERIALS
            If $itemID = $sellID Then Return True
        Next
    EndIf

    Local $rarity = GetRarity($item)
    
    If $itemID = $ID_SUPERIOR_IDENTIFICATION_KIT Then Return False
    If $itemID = $ID_SALVAGE_KIT Then Return False
    If $itemID = $ID_EXPERT_SALVAGE_KIT Then Return False
    If $itemID = $ID_SUPERIOR_SALVAGE_KIT Then Return False
    
    If IsWeapon($item) Then
        Switch $rarity
            Case $RARITY_WHITE, $RARITY_BLUE, $RARITY_PURPLE, $RARITY_GOLD
                If Not IsRareSkinSafe($itemID) Then
                    If GetIsIdentified($item) Then
                        Return True
                    EndIf
                EndIf
        EndSwitch
    EndIf
    
    Return False
EndFunc
; Salvages all Amphibian Tongues in inventory using basic salvage kit
Func SalvageAmphibianTongues()
    Local Const $AMPHIBIAN_TONGUE_ID = 27036
    Local $totalQty = 0
    
    ; Count total tongues in inventory
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If Not IsDllStruct($bag) Or DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bag, 'slots')
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            If DllStructGetData($item, 'ModelID') = $AMPHIBIAN_TONGUE_ID Then
                $totalQty += DllStructGetData($item, 'Quantity')
            EndIf
        Next
    Next
    
    If $totalQty = 0 Then Return
    Out("Salvaging " & $totalQty & " Amphibian Tongues...")
    
    Local $kit = GetSalvageKitCompat(True, $MAINTENANCE_TOWN) ; Buy kit if needed
    If $kit = 0 Then
        Out("Warning: No salvage kit available for tongues")
        Return
    EndIf
    Local $uses = DllStructGetData($kit, 'Value') / 2
    
    For $bagIndex = 1 To 4
        Local $bag = GetBag($bagIndex)
        If Not IsDllStruct($bag) Or DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        For $slot = 1 To DllStructGetData($bag, 'slots')
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            If DllStructGetData($item, 'ModelID') <> $AMPHIBIAN_TONGUE_ID Then ContinueLoop
            
            Local $qty = DllStructGetData($item, 'Quantity')
            For $k = 1 To $qty
                SalvageItem($item, $kit)
                Sleep(GetPing() + 500)
                $uses -= 1
                If $uses < 1 Then
                    $kit = GetSalvageKitCompat(True, $MAINTENANCE_TOWN)
                    If $kit = 0 Then Return
                    $uses = DllStructGetData($kit, 'Value') / 2
                EndIf
            Next
        Next
    Next
    Out("Amphibian Tongues salvaged.")
EndFunc

; Claims a specific item type from the Unclaimed Items (Bag 7)
Func ClaimSpecificItem($targetModelID)
    Local $unclaimedBagIndex = 7
    Local $bag = GetBag($unclaimedBagIndex)
    
    If DllStructGetData($bag, 'ID') == 0 Then Return ; Bag not found or empty
    
    Local $slots = DllStructGetData($bag, 'slots')
    For $slot = 1 To $slots
        Local $item = GetItemBySlot($unclaimedBagIndex, $slot)
        If Not IsDllStruct($item) Then ContinueLoop
        
        Local $modelID = DllStructGetData($item, 'ModelID')
        If $modelID == $targetModelID Then
            ; Find empty slot in inventory (Bags 1-4)
            Local $emptySlots = FindAllEmptySlots(1, 4)
            If UBound($emptySlots) < 2 Then 
                Out("No duplicate inventory space to claim item.")
                Return
            EndIf
            
            ; Move item to first available empty slot
            Local $destBag = $emptySlots[0]
            Local $destSlot = $emptySlots[1]
            
            MoveItem($item, $destBag, $destSlot)
            Sleep(500) ; Wait for move
        EndIf
    Next
    
    CombineItemStacks($targetModelID)
EndFunc

; Combines multiple stacks of the same item in inventory (Bags 1-4)
Func CombineItemStacks($modelID)
    Local $foundStacks[30][3] ; [Bag, Slot, Quantity]
    Local $stackCount = 0
    
    ; 1. Find all partial stacks
    For $bag = 1 To 4
        Local $bagStruct = GetBag($bag)
        If DllStructGetData($bagStruct, 'ID') == 0 Then ContinueLoop
        Local $slots = DllStructGetData($bagStruct, 'slots')
        For $slot = 1 To $slots
            Local $item = GetItemBySlot($bag, $slot)
            If IsDllStruct($item) And DllStructGetData($item, 'ModelID') == $modelID Then
                Local $qty = DllStructGetData($item, 'Quantity')
                If $qty < 250 Then
                    $foundStacks[$stackCount][0] = $bag
                    $foundStacks[$stackCount][1] = $slot
                    $foundStacks[$stackCount][2] = $qty
                    $stackCount += 1
                EndIf
            EndIf
        Next
    Next
    
    If $stackCount < 2 Then Return ; Nothing to combine
    
    ; 2. Try to combine
    Out("Found " & $stackCount & " partial stacks of ID " & $modelID & ". Attempting to combine...")
    
    For $i = 0 To $stackCount - 2
        For $j = $i + 1 To $stackCount - 1
            Local $qtyI = $foundStacks[$i][2]
            Local $qtyJ = $foundStacks[$j][2]
            
            If $qtyI < 250 And $qtyJ > 0 Then
                Local $spaceInI = 250 - $qtyI
                
                ; Move J to I
                Out("Merging Bag " & $foundStacks[$j][0] & " Slot " & $foundStacks[$j][1] & " into Bag " & $foundStacks[$i][0] & " Slot " & $foundStacks[$i][1])
                MoveItem(GetItemBySlot($foundStacks[$j][0], $foundStacks[$j][1]), $foundStacks[$i][0], $foundStacks[$i][1])
                Sleep(600)
                
                ; Update tracked quantities
                If $qtyJ <= $spaceInI Then
                    ; Fully moved
                    $foundStacks[$i][2] += $qtyJ
                    $foundStacks[$j][2] = 0
                Else
                    ; Partially moved (filled I)
                    $foundStacks[$i][2] = 250
                    $foundStacks[$j][2] -= $spaceInI
                EndIf
            EndIf
            
            If $foundStacks[$i][2] >= 250 Then ExitLoop ; Stack I is full, move to next base stack
        Next
    Next
EndFunc

; ==============================================================================
; ECTO GOLD MANAGEMENT
; When bank gold exceeds the threshold, spend excess on Globs of Ectoplasm
; to avoid hitting the 1,000,000 gold storage cap.
; ==============================================================================

Global Const $ECTO_BANK_GOLD_THRESHOLD = 900000   ; Bank gold above this triggers ecto buying
Global Const $ECTO_BUY_BUDGET = 100000             ; Amount to withdraw and spend on ectos

Func BuyEctosWithExcessGold()
    Local $storageGold = GetGoldStorage()
    If $storageGold <= $ECTO_BANK_GOLD_THRESHOLD Then Return
    
    Out("Bank gold (" & $storageGold & ") exceeds " & $ECTO_BANK_GOLD_THRESHOLD & ". Buying Ectos...")
    
    ; Withdraw gold for purchasing (no need to walk to chest)
    Local $withdrawAmount = $ECTO_BUY_BUDGET
    If $storageGold < $withdrawAmount Then $withdrawAmount = $storageGold
    
    WithdrawGold($withdrawAmount)
    Sleep(1000)
    
    ; Navigate to Rare Material Trader
    Local $NPCCoords = NPCCoordinatesInTown($MAINTENANCE_TOWN, 'Rare material trader')
    MoveTo($NPCCoords[0], $NPCCoords[1])
    Local $trader = GetNearestNPCToCoords($NPCCoords[0], $NPCCoords[1])
    GoToNPC($trader)
    Sleep(1000)
    Dialog($trader)
    Sleep(1000)
    
    ; Request initial quote
    Local $ectoModelID = $ID_GLOB_OF_ECTOPLASM
    If Not TraderRequest($ectoModelID) Then
        Out("Failed to get ecto quote. Aborting ecto purchase.")
        If GetGoldCharacter() > 5000 Then DepositGold(GetGoldCharacter() - 5000)
        Return
    EndIf
    
    Local $cost = GetTraderCostValue()
    Local $bought = 0
    
    ; Buy loop: buy ectos while we can afford them
    While $cost > 0 And GetGoldCharacter() >= $cost
        TraderBuy()
        $bought += 1
        Sleep(GetPing() + 200)
        
        ; Re-request quote (price changes dynamically with supply)
        If Not TraderRequest($ectoModelID) Then ExitLoop
        $cost = GetTraderCostValue()
    WEnd
    
    Out("Bought " & $bought & " Ectos.")
    
    ; Deposit ectos — material storage first, then bank stacks
    DepositEctosToBank($ectoModelID)
    
    ; Deposit any remaining gold (no need to walk to chest)
    If GetGoldCharacter() > 5000 Then DepositGold(GetGoldCharacter() - 5000)
EndFunc

; Deposits ectos from inventory into storage.
; Priority order:
;   1. Material Storage pane (bag 6) — if not full (< 250)
;   2. Existing partial ecto stacks in bank (bags 8-16, qty < 250)
;   3. Empty bank slots
Func DepositEctosToBank($ectoModelID)
    ; Check Material Storage first (bag 6)
    Local $materialSlot = $MAP_MATERIAL_LOCATION[$ectoModelID]
    Local $materialItem = GetItemBySlot(6, $materialSlot)
    ; Material storage qty uses Equipped * 256 + Quantity for values > 255
    Local $materialQty = DllStructGetData($materialItem, 'Equipped') * 256 + DllStructGetData($materialItem, 'Quantity')
    Local $materialHasSpace = ($materialQty < 250)
    
    ; Second: find existing ecto stacks in bank (bags 8-16) with qty < 250
    Local $bankEctoSlots[20][3] ; [bagIndex, slotIndex, quantity]
    Local $bankEctoCount = 0
    
    For $bagIndex = 8 To 16
        Local $bag = GetBag($bagIndex)
        If Not IsDllStruct($bag) Or DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        Local $slots = DllStructGetData($bag, 'slots')
        For $slot = 1 To $slots
            Local $item = GetItemBySlot($bagIndex, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            If DllStructGetData($item, 'ModelID') = $ectoModelID Then
                Local $qty = DllStructGetData($item, 'Quantity')
                If $qty < 250 And $bankEctoCount < 20 Then
                    $bankEctoSlots[$bankEctoCount][0] = $bagIndex
                    $bankEctoSlots[$bankEctoCount][1] = $slot
                    $bankEctoSlots[$bankEctoCount][2] = $qty
                    $bankEctoCount += 1
                EndIf
            EndIf
        Next
    Next
    
    ; Move ectos from inventory (bags 1-4) into storage
    Local $deposited = 0
    For $invBag = 1 To 4
        Local $bag = GetBag($invBag)
        If Not IsDllStruct($bag) Or DllStructGetData($bag, 'ID') = 0 Then ContinueLoop
        Local $slots = DllStructGetData($bag, 'slots')
        For $slot = 1 To $slots
            Local $item = GetItemBySlot($invBag, $slot)
            If Not IsDllStruct($item) Then ContinueLoop
            If DllStructGetData($item, 'ModelID') <> $ectoModelID Then ContinueLoop
            
            ; Priority 1: Material Storage pane
            If $materialHasSpace Then
                MoveItem($item, 6, $materialSlot)
                Sleep(600)
                $deposited += 1
                ; Re-check material storage quantity
                $materialItem = GetItemBySlot(6, $materialSlot)
                $materialQty = DllStructGetData($materialItem, 'Equipped') * 256 + DllStructGetData($materialItem, 'Quantity')
                $materialHasSpace = ($materialQty < 250)
                ContinueLoop
            EndIf
            
            ; Priority 2: Existing partial bank stack
            Local $movedToExisting = False
            For $e = 0 To $bankEctoCount - 1
                If $bankEctoSlots[$e][2] < 250 Then
                    MoveItem($item, $bankEctoSlots[$e][0], $bankEctoSlots[$e][1])
                    Sleep(600)
                    $bankEctoSlots[$e][2] += DllStructGetData($item, 'Quantity')
                    $movedToExisting = True
                    $deposited += 1
                    ExitLoop
                EndIf
            Next
            If $movedToExisting Then ContinueLoop
            
            ; Priority 3: Empty bank slot
            Local $emptySlot = FindChestFirstEmptySlot()
            If $emptySlot[0] <> 0 Then
                MoveItem($item, $emptySlot[0], $emptySlot[1])
                Sleep(600)
                $deposited += 1
            Else
                Out("Warning: No empty bank slots for ectos!")
                ExitLoop 2
            EndIf
        Next
    Next
    
    If $deposited > 0 Then Out("Deposited " & $deposited & " ecto stack(s) to storage.")
EndFunc


; CraftItemSafe: Crafts an item using the same mechanism as the original CraftItem.
; Key insight: material arrays must be in GAME PROCESS memory (VirtualAllocEx),
; and the item index must be written to TradeID before calling CommandCraftItemEx2.
Func CraftItemSafe($modelID, $amount, $gold, $materialsArray)
    If Not IsArray($materialsArray) Then 
        Out("Error: materialsArray is not an array for " & $modelID)
        Return 0
    EndIf
    
    ; Check merchant window is open
    If Not GetIsMerchantOpen() Then
        Out("Error: Merchant window not open.")
        Return 0
    EndIf
    
    ; Find the destination item in merchant list
    Local $merchantItemsBase = GetMerchantItemsBase()
    Local $merchantItemsSize = GetMerchantItemsSize()
    If Not $merchantItemsBase Or $merchantItemsSize = 0 Then
        Out("Error: No merchant items found.")
        Return 0
    EndIf
    
    ; Find item index and destination ptr (like original CraftItem)
    Local $itemPtr = 0
    Local $destinationItemPtr = 0
    Local $itemIndex = -1
    Local $itemID = 0
    
    For $i = 0 To $merchantItemsSize - 1
        $itemID = MemRead($merchantItemsBase + 4 * $i)
        If $itemID Then
            Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 4 * $itemID]
            Local $result = MemReadPtr($base_address_ptr, $offsets)
            $itemPtr = $result[1]
            
            If $itemPtr <> 0 And MemRead($itemPtr + 0x2C) = $modelID Then
                $destinationItemPtr = $itemPtr
                $itemIndex = $i
                ExitLoop
            EndIf
        EndIf
    Next
    
    If $itemIndex = -1 Then
        Out("Error: Item " & $modelID & " not found in merchant list.")
        Return 0
    EndIf
    
    Out("Debug: Found item at index " & $itemIndex & " destPtr=" & $destinationItemPtr)
    
    ; Check materials quantity
    For $i = 0 To UBound($materialsArray) - 1
        Local $checkQuantity = _GWA2_CountItemInBagsByModelID($materialsArray[$i][0])
        If $materialsArray[$i][1] * $amount > $checkQuantity Then
            Out("Error: Insufficient material " & $materialsArray[$i][0] & " (have " & $checkQuantity & ", need " & ($materialsArray[$i][1] * $amount) & ")")
            Return 0
        EndIf
    Next
    
    ; Check gold
    If GetGoldCharacter() < $amount * $gold Then
        Out("Error: Insufficient gold (have " & GetGoldCharacter() & ", need " & ($amount * $gold) & ")")
        Return 0
    EndIf
    
    ; Build material item IDs string (like original CraftItem)
    Local $materialString = ''
    Local $materialCount = 0
    For $i = 0 To UBound($materialsArray) - 1
        $materialString &= GetItemIDFromModelID($materialsArray[$i][0]) & ';'
        $materialCount += 1
    Next
    
    Out("Debug: Material IDs string: " & $materialString & " count=" & $materialCount)
    
    ; Build arrays of material item IDs and quantities for each material type
    ; The ASM needs TWO arrays: MatIDs (inventory item IDs) and MatQtys (amounts from each stack)
    Local $giveCount = 0
    Local $giveItemIDs[32]
    Local $giveItemQtys[32]
    
    For $i = 0 To UBound($materialsArray) - 1
        Local $matModelID = $materialsArray[$i][0]
        Local $matAmountNeeded = $materialsArray[$i][1] * $amount
        
        ; Loop through all bags to find the material stacks
        For $bagIdx = 1 To 4
            Local $bag = GetBag($bagIdx)
            If Not IsDllStruct($bag) Then ContinueLoop
            For $slotIdx = 1 To DllStructGetData($bag, 'Slots')
                Local $item = GetItemBySlot($bagIdx, $slotIdx)
                If Not IsDllStruct($item) Then ContinueLoop
                
                If DllStructGetData($item, 'ModelID') == $matModelID Then
                    Local $qtyInStack = DllStructGetData($item, 'Quantity')
                    Local $takeAmount = $matAmountNeeded
                    If $qtyInStack < $takeAmount Then $takeAmount = $qtyInStack
                    
                    $giveItemIDs[$giveCount] = DllStructGetData($item, 'Id')
                    $giveItemQtys[$giveCount] = $takeAmount
                    $giveCount += 1
                    
                    $matAmountNeeded -= $takeAmount
                    If $matAmountNeeded <= 0 Then ExitLoop
                EndIf
            Next
            If $matAmountNeeded <= 0 Then ExitLoop
        Next
        
        If $matAmountNeeded > 0 Then
            Out("Error: Not enough material ModelID " & $matModelID & " in bags!")
            Return 0
        EndIf
    Next
    
    Out("Debug: Found " & $giveCount & " material stacks to give")
    
    ; Create DllStructs for material IDs and quantities
    Local $matsStruct = DllStructCreate("dword[" & $giveCount & "]")
    Local $qtysStruct = DllStructCreate("dword[" & $giveCount & "]")
    For $i = 0 To $giveCount - 1
        DllStructSetData($matsStruct, 1, $giveItemIDs[$i], $i + 1)
        DllStructSetData($qtysStruct, 1, $giveItemQtys[$i], $i + 1)
        Out("Debug: MatStack[" & $i & "] ItemID=" & $giveItemIDs[$i] & " Qty=" & $giveItemQtys[$i])
    Next
    
    ; Allocate memory INSIDE THE GAME PROCESS for both arrays
    Local $matIDsSize = $giveCount * 4
    Local $matQtysSize = $giveCount * 4
    Local $totalMemSize = $matIDsSize + $matQtysSize
    Local $processHandle = GetProcessHandle()
    Local $memoryBuffer = SafeDllCall13($kernel_handle, 'ptr', 'VirtualAllocEx', 'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $totalMemSize, 'dword', 0x1000, 'dword', 0x40)
    If $memoryBuffer = 0 Or $memoryBuffer[0] = 0 Then
        Out("Error: VirtualAllocEx failed!")
        Return 0
    EndIf
    
    ; MatIDs at offset 0, MatQtys at offset matIDsSize
    Local $matIDsGamePtr = $memoryBuffer[0]
    Local $matQtysGamePtr = $memoryBuffer[0] + $matIDsSize
    
    Out("Debug: Game memory: MatIDs at " & $matIDsGamePtr & ", MatQtys at " & $matQtysGamePtr)
    
    ; Write both arrays to game process memory
    SafeDllCall13($kernel_handle, 'int', 'WriteProcessMemory', 'int', $processHandle, 'int', $matIDsGamePtr, 'ptr', DllStructGetPtr($matsStruct), 'int', $matIDsSize, 'int', 0)
    SafeDllCall13($kernel_handle, 'int', 'WriteProcessMemory', 'int', $processHandle, 'int', $matQtysGamePtr, 'ptr', DllStructGetPtr($qtysStruct), 'int', $matQtysSize, 'int', 0)
    
    ; WRITE THE INDEX TO TradeID ADDRESS (critical for ASM to find the item)
    If $trade_id_addr <> 0 Then
        MemWrite($trade_id_value_addr, $itemIndex, 'dword')
        MemWrite($trade_id_addr, $trade_id_value_addr, 'dword')
        Out("Debug: Wrote itemIndex=" & $itemIndex & " to TradeID addr=" & $trade_id_addr)
    Else
        Out("Error: TradeID address not initialized")
        SafeDllCall11($kernel_handle, 'ptr', 'VirtualFreeEx', 'handle', $processHandle, 'ptr', $memoryBuffer[0], 'int', 0, 'dword', 0x8000)
        Return 0
    EndIf
    
    ; Populate CRAFT_ITEM_STRUCT to match CommandCraftItemEx2 ASM layout:
    ;   +0  CmdAddr              (field 1: ptr)
    ;   +4  AmountToCraft        (field 2: dword)
    ;   +8  MerchantItemID/Ptr   (field 3: dword) - original uses $destinationItemPtr
    ;   +C  TotalCost            (field 4: dword)
    ;   +10 GiveCount            (field 5: dword)
    ;   +14 MatIDsArray_PTR      (field 6: ptr)  - game process memory pointer
    ;   +18 MatQtysArray_PTR     (field 7: ptr)  - game process memory pointer
    DllStructSetData($CRAFT_ITEM_STRUCT, 1, GetValue('CommandCraftItemEx2'))
    DllStructSetData($CRAFT_ITEM_STRUCT, 2, $amount)
    DllStructSetData($CRAFT_ITEM_STRUCT, 3, $destinationItemPtr)   ; +8: Item ptr (used by ASM as &MerchantItemID for recv)
    DllStructSetData($CRAFT_ITEM_STRUCT, 4, $amount * $gold)       ; +C: TotalCost
    DllStructSetData($CRAFT_ITEM_STRUCT, 5, $giveCount)            ; +10: GiveCount (number of material stacks)
    DllStructSetData($CRAFT_ITEM_STRUCT, 6, $matIDsGamePtr)        ; +14: MatIDsArray (game process ptr)
    DllStructSetData($CRAFT_ITEM_STRUCT, 7, $matQtysGamePtr)       ; +18: MatQtysArray (game process ptr)
    
    Out("Debug: Enqueuing craft: amount=" & $amount & " destPtr=" & $destinationItemPtr & " cost=" & ($amount * $gold) & " giveCount=" & $giveCount & " matIDs=" & $matIDsGamePtr & " matQtys=" & $matQtysGamePtr)
    Enqueue($CRAFT_ITEM_STRUCT_PTR, 28) ; 7 fields = 28 bytes
    
    
    ; Wait for crafting to complete
    Local $startGold = GetGoldCharacter()
    Local $deadlock = TimerInit()
    Do
        Sleep(250)
    Until GetGoldCharacter() <> $startGold Or TimerDiff($deadlock) > 5000
    
    ; Free game process memory
    SafeDllCall11($kernel_handle, 'ptr', 'VirtualFreeEx', 'handle', $processHandle, 'ptr', $memoryBuffer[0], 'int', 0, 'dword', 0x8000)
    
    Out("Debug: CraftItemSafe finished.")
    Return True
EndFunc

; Local version: Returns ItemPtr (Address) checks 0x2C offset
Func _GetMerchantItemPtrByModelId_Safe($modelID)
    Local $base = GetMerchantItemsBase()
    Local $count = GetMerchantItemsSize()
    If $base = 0 Then Return 0
    
    Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 0]
    
    For $i = 1 To $count
        Local $itemID = MemRead($base + 4 * ($i - 1))
        
        If $itemID <> 0 Then
            $offsets[4] = 4 * $itemID
            Local $itemPtrData = MemReadPtr($base_address_ptr, $offsets)
            Local $itemPtr = $itemPtrData[1]
            
            If $itemPtr <> 0 Then
                ; Check 0x2C first (as confirmed by ScanTrader)
                If MemRead($itemPtr + 0x2C) = $modelID Then Return $itemPtr
                ; Fallback to 0x18 just in case
                If MemRead($itemPtr + 0x18) = $modelID Then Return $itemPtr
            EndIf
        EndIf
    Next
    Return 0
EndFunc
