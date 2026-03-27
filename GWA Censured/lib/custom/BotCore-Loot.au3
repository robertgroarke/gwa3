#include-once
; =============================================================================
; BotCore-Loot.au3
;
; Extracted from Froggy_HM_v1.6.au3 as part of the function extraction project.
; All dependencies resolve through Froggy_Includes.au3 master include.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; === Module State ===

; Opened-chest tracking array -- reset each run by the bot main loop.
; Callers must do:
;   $g_aOpenedChestAgentIDs[0] = ""
;   ReDim $g_aOpenedChestAgentIDs[1]
; at the start of every run iteration.
Global $g_aOpenedChestAgentIDs[1]

; Whether to pick up gold coins (default True). Bots can flip this.
Global $g_bPickupCoins = True

; --- Item-type constants (numeric values from GW's item struct) ---
; These are duplicated here so BotCore-Loot is self-contained; if a
; shared Constants.au3 is created later, remove these and include that.
Global Const $LOOT_TYPE_BUNDLE              = 6
Global Const $LOOT_TYPE_USABLE              = 9
Global Const $LOOT_TYPE_DYE                 = 10
Global Const $LOOT_TYPE_MATERIAL_AND_ZCOINS = 11
Global Const $LOOT_TYPE_ATTACK              = 14
Global Const $LOOT_TYPE_KEY                 = 18
Global Const $LOOT_TYPE_GOLD_COINS          = 20
Global Const $LOOT_TYPE_TROPHY              = 30
Global Const $LOOT_TYPE_SCROLL              = 31

; --- Rarity enum (values match GW rarity model IDs) ---
Global Enum $LOOT_RARITY_White = 2621, _
            $LOOT_RARITY_Blue  = 2623, _
            $LOOT_RARITY_Gold  = 2624, _
            $LOOT_RARITY_Purple = 2626, _
            $LOOT_RARITY_Green = 2627

; === Public Functions ===

; ---------------------------------------------------------------------------
; CountFreeSlots  --  counts free inventory slots across bags
; Dependencies: GetBagPtr(), MemRead()
; ---------------------------------------------------------------------------
Func CountFreeSlots($NumOfBags = 4)
	Local $lCount = 0
	Local $lBagPtr
	For $lBag = 1 To $NumOfBags
		$lBagPtr = GetBagPtr($lBag)
		If $lBagPtr = 0 Then ContinueLoop
		$lCount += MemRead($lBagPtr + 32, "long") - MemRead($lBagPtr + 16, "long")
	Next
	Return $lCount
EndFunc   ;==>CountFreeSlots

; ---------------------------------------------------------------------------
; GetPicksCount  --  counts Lockpicks in personal inventory (ModelID 22751)
; Dependencies: GetBag(), GetItemBySlot(), DllStructGetData()
; ---------------------------------------------------------------------------
Func GetPicksCount()
	Local $AmountPicks = 0
	Local $aBag
	Local $aItem
	Local $i
	For $i = 1 To 4 ; Count in personal inventory bags only
		$aBag = GetBag($i)
		For $j = 1 To DllStructGetData($aBag, "Slots")
			$aItem = GetItemBySlot($aBag, $j)
			If DllStructGetData($aItem, "ModelID") == 22751 Then
				$AmountPicks += DllStructGetData($aItem, "Quantity")
			Else
				ContinueLoop
			EndIf
		Next
	Next
	Return $AmountPicks
EndFunc   ;==>GetPicksCount


; #########################################################################
; === Loot Policy (KF-025) ===
; #########################################################################
;
; The loot filtering rules embedded in CanPickUpEx() are:
;
; 1. ALWAYS pick up (regardless of type/rarity):
;    - Specific quest/trophy model IDs hard-coded in the first Switch block
;      (Unholy Text, Mysterious Commendations, Blob of Ooze, Vaettir Essence,
;       Destroyer Core, Superb Charr Carving, Kath Hammer, Prison Keys,
;       general quest items, alcohol, sweets, consumables, special drops,
;       Diamonds, Onyx Gemstones).
;
; 2. TYPE-based rules (second Switch block):
;    - BUNDLE:  only if $PickupTorch is True AND model is Unlit Torch (22342)
;               or Asura Flame Staff (24350).
;    - DYE:     only Black Dye (ExtraID 10). Updates GUI counter.
;    - GOLD_COINS: only if $g_bPickupCoins is True, model 2511,
;                  and character gold < 100 000.
;    - KEY:     always pick up. If Lockpick (22751), update GUI counter.
;    - MATERIAL_AND_ZCOINS, SCROLL, TROPHY: skip (Return False).
;    - USABLE:  only Tomes (model 21786..21805). Updates GUI counter.
;
; 3. RARITY fallback (third Switch):
;    - Gold-rarity items are always picked up. Updates GUI counter.
;
; 4. Everything else: skip (Return False).
;
; 5. Inventory guard: if fewer than 2 free slots, only BUNDLE and
;    GOLD_COINS are picked up (other items are skipped).
;


; #########################################################################
; === Pickup Predicate (KF-026) ===
; #########################################################################
;
; ---------------------------------------------------------------------------
; CanPickUpEx  --  decides whether a ground item should be picked up
;
; Globals used:
;   $g_bPickupCoins  (read)  -- whether to pick up gold coins
;
; Dependencies:
;   CountFreeSlots(), GetRarity(), GetGoldCharacter(),
;   GUI_SetBlackDyes/GUI_GetBlackDyes, GUI_SetDroppedLockpicks/GUI_GetDroppedLockpicks,
;   GUI_SetTomes/GUI_GetTomes, GUI_SetGolds/GUI_GetGolds,
;   DllStructGetData(), Out()
;
; NOTE: This function contains Froggy-specific pickup policy.
;       Sections are marked with "; FROGGY-SPECIFIC:" comments.
; ---------------------------------------------------------------------------
Func CanPickUpEx($aItem, $PickupTorch = False)
	If Not IsDllStruct($aItem) Then Return False

	Local $lType     = DllStructGetData($aItem, "Type")
	Local $lExtraID  = DllStructGetData($aItem, "ExtraId")
	Local $lModelID  = DllStructGetData($aItem, "ModelId")
	Local $lRarity   = GetRarity($aItem)
	Local $lQuantity = DllStructGetData($aItem, "Quantity")
	Local $lValue    = DllStructGetData($aItem, "Value")

	; --- Inventory guard: need at least 2 free slots (bundles & coins exempt) ---
	If CountFreeSlots() < 2 And $lType <> $LOOT_TYPE_BUNDLE And $lType <> $LOOT_TYPE_GOLD_COINS Then Return False

	; --- Always-pickup model IDs (quest items, consumables, materials) ---
	; FROGGY-SPECIFIC: These model-ID lists are tuned for Froggy farm routes.
	; Other bots should review / replace these lists.
	Switch $lModelID
		Case 2619, 36985 ; Unholy Text (FoW), Mysterious Commendations
			Return True
		Case 27067, 27071, 27033, 27052, 22374 ; Blob Of Ooze, Vaettir Essence, Destroyer Core, Superb Charr Carving, Kath Hammer
			Return True
		Case 2605, 2606, 501 To 503, 2566, 2607, 6102, 6104, 6531, 15564, 15565, 15867, 15869 To 15871, 17054, 17055 _
				, 17075, 22781, 22782, 25410, 25413, 25416, 24628, 24582 ; General quest items: Prison key (25413), etc.
			Return True
		; FROGGY-SPECIFIC: alcohol, sweets, pcons, DP-removal sweets, special drops, Diamond & Onyx
		Case 910, 2513, 5585, 6049, 6366, 6367, 6375, 15477, 19171, 19172, 19173, 22190, 24593, 28435, 30855, 31145, 31146, 35124, 36682 _  ; alcohol
				, 15528, 15479, 19170, 21492, 21812, 22644, 30208, 31150, 35125, 36681 _  ; sweets
				, 17060, 17061, 17062, 22269, 28431, 28432, 28436, 29431, 31151, 31152, 31153, 35121 _  ; sweet pcons
				, 6370, 19039, 21488, 21489, 22191, 26784, 28433, 35127 _  ; DP removal sweets
				, 556, 18345, 21491, 37765, 21833, 28433, 28434 _  ; special drops
				, 935, 936 ; Diamond and Onyx
			Return True
	EndSwitch

	; --- Type-based rules ---
	Switch $lType
		Case $LOOT_TYPE_BUNDLE
			; FROGGY-SPECIFIC: torch pickup for Bogroot/dungeon runs
			If $PickupTorch And ($lModelID = 22342 Or $lModelID = 24350) Then ; Unlit Torch, Asura Flame Staff
				Out("Grab unlit torch")
				Return True
			EndIf

		Case $LOOT_TYPE_DYE
			If $lExtraID = 10 Then ; Black dye
				GUI_SetBlackDyes(GUI_GetBlackDyes() + 1)
				Return True
			EndIf

		Case $LOOT_TYPE_GOLD_COINS
			If $g_bPickupCoins And $lModelID = 2511 And GetGoldCharacter() + $lValue < 100000 Then Return True

		Case $LOOT_TYPE_KEY
			If $lModelID = 22751 Then GUI_SetDroppedLockpicks(GUI_GetDroppedLockpicks() + 1)
			; FROGGY-SPECIFIC: dungeon key log messages
			If $lModelID = 25410 Or $lModelID = 25416 Then Out("Grab Dungeon Key")
			Return True

		Case $LOOT_TYPE_MATERIAL_AND_ZCOINS, $LOOT_TYPE_SCROLL, $LOOT_TYPE_TROPHY
			Return False

		Case $LOOT_TYPE_USABLE
			Switch $lModelID
				Case 21786 To 21805 ; Tomes
					GUI_SetTomes(GUI_GetTomes() + 1)
					Return True
			EndSwitch
	EndSwitch

	; --- Rarity fallback ---
	Switch $lRarity
		Case $LOOT_RARITY_Gold
			GUI_SetGolds(GUI_GetGolds() + 1)
			Return True
	EndSwitch

	Return False
EndFunc   ;==>CanPickUpEx


; #########################################################################
; === Loot Pickup Engine (KF-027) ===
; #########################################################################
;
; ---------------------------------------------------------------------------
; PickupLootEx  --  iterates ground items and picks up qualifying ones
;
; Globals used:
;   (none directly -- delegates to CanPickUpEx which reads $g_bPickupCoins)
;
; Dependencies:
;   GetAgentArray(), GetItemByAgentID(), GetMyID(), GetDistanceToPoint(),
;   GetMyAgent(), MoveTo(), PickUpItem(), GetAgentExists(), GetIsDead(),
;   CanPickUpEx(), DllStructGetData(), IsDllStruct(), Out(), Sleep(),
;   TimerInit(), TimerDiff()
;
; $iMaxDist -- maximum distance (game units) to walk for an item
; $PickupTorch -- passed through to CanPickUpEx for bundle filtering
; ---------------------------------------------------------------------------
Func PickupLootEx($iMaxDist = 2000, $PickupTorch = False)
	Local $lAgentArray = GetAgentArray($ID_AGENT_TYPE_ITEM)
	Local $lPickupDeadlock = TimerInit()
	Local $lPickupCounter = 0, $lDeadlock = 0
	If Not IsArray($lAgentArray) Or UBound($lAgentArray) == 0 Then Return
	For $i = 0 To UBound($lAgentArray) - 1
		Local $lAgentStruct = $lAgentArray[$i]
		If Not IsDllStruct($lAgentStruct) Then ContinueLoop

		Local $lAgentID = DllStructGetData($lAgentStruct, 'ID')
		Local $lItem = GetItemByAgentID($lAgentID)
		If Not IsDllStruct($lItem) Then ContinueLoop

		Local $lOwner = DllStructGetData($lAgentStruct, 'Owner')
		If $lOwner <> 0 And $lOwner <> GetMyID() Then ContinueLoop

		If CanPickUpEx($lItem, $PickupTorch) And GetDistanceToPoint(GetMyAgent(), DllStructGetData($lAgentStruct, 'X'), DllStructGetData($lAgentStruct, 'Y')) < $iMaxDist Then
			MoveTo(DllStructGetData($lAgentStruct, 'X'), DllStructGetData($lAgentStruct, 'Y'))
			$lDeadlock = TimerInit()
			$lPickupCounter = 0
			Do
				PickUpItem($lItem)
				Sleep(250)
				Out("Pickup")
				$lPickupCounter += 1
			Until Not GetAgentExists($lAgentID) Or GetIsDead(-2) Or TimerDiff($lDeadlock) > 6000 Or $lPickupCounter > 10
		EndIf
		; Global deadlock: abort if looting takes longer than 2 minutes total
		If TimerDiff($lPickupDeadlock) > 120000 Then Return
	Next
EndFunc   ;==>PickupLootEx


; #########################################################################
; === Chest Detection (KF-028) ===
; #########################################################################
;
; ---------------------------------------------------------------------------
; CheckForChest  --  scans for unopened chests, opens the first one found,
;                    then picks up the loot.
;
; Globals used:
;   $g_aOpenedChestAgentIDs  (read/write) -- tracks already-opened chests
;
; Dependencies:
;   GetIsDead(), GetAgentArray(), DllStructGetData(), _ArraySearch(),
;   _ArrayAdd(), ChangeTarget(), GoToSignpost(), OpenChest(), GetPing(),
;   PickupLootEx(), Out(), Sleep()
;
; $chestrun -- if True, uses a wider pickup radius (5000 vs 3500)
; ---------------------------------------------------------------------------
Func CheckForChest($chestrun = False)
	Local $AgentArray, $lAgent, $lExtraType, $lType
	Local $ChestFound = False
	If GetIsDead(-2) Then Return
	$AgentArray = GetAgentArray(0x200)   ; 0x200 = type: static (signpost/chest)
	Out("Looking for chests")
	For $i = 0 To UBound($AgentArray) - 1
		$lAgent = $AgentArray[$i]
		$lType = DllStructGetData($lAgent, 'Type')
		$lExtraType = DllStructGetData($lAgent, 'ExtraType')
		If $lType <> 512 Then ContinueLoop
		If _ArraySearch($g_aOpenedChestAgentIDs, DllStructGetData($lAgent, 'ID')) <> -1 Then ContinueLoop

		_ArrayAdd($g_aOpenedChestAgentIDs, DllStructGetData($lAgent, 'ID'))
		$ChestFound = True
		Out("Found a Chest")
		ExitLoop
	Next
	If Not $ChestFound Then Return
	Out("opening chest")
	ChangeTarget($lAgent)
	GoToSignpost($lAgent)
	OpenChest()
	Sleep(GetPing() + 500)
	$AgentArray = GetAgentArray(0x400)    ; 0x400 = type: item
	If UBound($AgentArray) > 0 Then ChangeTarget($AgentArray[0])
	; FROGGY-SPECIFIC: pickup radius values (5000 for chest runs, 3500 otherwise)
	If $chestrun = True Then
		PickupLootEx(5000)
	Else
		PickupLootEx(3500)
	EndIf
EndFunc   ;==>CheckForChest
