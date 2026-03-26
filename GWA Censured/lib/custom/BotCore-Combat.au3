#include-once
; =============================================================================
; BotCore-Combat.au3 — Combat State Container, State Readers & Target Selection
;
; KF-017: Defines the combat state globals (SkillBarCache, SkillbarSlot,
;         BestTargetPtr) with corrected sizing.
; KF-018: Target selection helpers (GetBestTargetPtr, GetBestMeleeTarget,
;         GetLowestAlly, GetNoHexEnemy, GetBalledEnchantedEnemy,
;         GetMostBalledCastingEnemy, GetBestTargetBySkillSlot).
; KF-019: Ally/summon targeting helpers (GetNearestSpiritPtrToAgent,
;         GetNearestMinionPtrToAgent, GetNearestDeadAllyPtrToAgent,
;         NeedEchoAlly, MostCondsAllyPtr, MostHexedAllyPtr).
; KF-020: Extracts combat state reader functions from Froggy_HM_v1.6.au3.
;
; Extracted from Froggy_HM_v1.6.au3 as part of the function extraction project.
; No #include needed — all dependencies resolve through Froggy_Includes.au3.
;
; See CONVENTIONS.md for naming and state management rules.
; =============================================================================

; =============================================================================
; Section 1: SkillBarCache Column Enum
; =============================================================================
; 23 members (indices 0..22). The original Froggy code re-declared the cache
; as [9][20] at line 1302, causing an OOB bug for indices 20-22. Fixed here
; by declaring the cache with [9][23] — matching the enum count exactly.

Global Enum $all = 0, _         ; 0  — skill ID (all slots)
            $ptr, _             ; 1  — pointer to skill data struct
            $energyreq, _      ; 2  — energy cost
            $adrereq, _        ; 3  — adrenaline cost
            $type, _           ; 4  — skill type ID
            $target, _         ; 5  — target type byte
            $hexes, _          ; 6  — hex spell skill ID (or "")
            $pressure, _       ; 7  — pressure skill ID (or "")
            $bind, _           ; 8  — binding ritual skill ID (or "")
            $speedBoost, _     ; 9  — speed boost skill ID (or "")
            $survive, _        ; 10 — survival skill ID (or "")
            $attackskill, _    ; 11 — attack skill ID (or "")
            $heal, _           ; 12 — heal skill ID (or "")
            $prot, _           ; 13 — protection skill ID (or "")
            $bond, _           ; 14 — bond skill ID (or "")
            $condremove, _     ; 15 — condition removal skill ID (or "")
            $hexremove, _      ; 16 — hex removal skill ID (or "")
            $enchantremove, _  ; 17 — enchant removal skill ID (or "")
            $rupt, _           ; 18 — interrupt skill ID (or "")
            $hardrupt, _       ; 19 — hard interrupt skill ID (or "")
            $precast, _        ; 20 — precast skill ID (or "")
            $chantsnshouts, _  ; 21 — chant/shout skill ID (or "")
            $echoes           ; 22 — echo/refrain skill ID (or "")

; =============================================================================
; Section 2: Combat State Globals
; =============================================================================
; These arrays remain as bare globals (not inside a Scripting.Dictionary)
; because they are accessed on every frame of the combat hot-path. Dictionary
; key lookups add measurable overhead in AutoIt for indexed array access.
;
; $Skillbar[9]           — DELETED (dead code per KF-004 audit)
; $PressureSpiritSkills  — DELETED (write-only / dead code per KF-004 audit)

; --- SkillBarCache: 9 skill slots x 23 attribute columns ---
; BUG FIX: Froggy re-declared this as [9][20] at line 1302, truncating
; columns 20-22 ($precast, $chantsnshouts, $echoes). Fixed to [9][23].
Global $SkillBarCache[9][23]

; --- SkillbarSlot: maps skill ID -> slot index ---
; Original declared as [3500], then re-declared as [10000]. Using 3500
; which covers the full GW skill ID range (max ~3400).
Global $SkillbarSlot[3500]

; --- Best target pointer (used as side-effect global by targeting functions) ---
Global $BestTargetPtr = 0

; --- Scalar combat state (in state container per CONVENTIONS.md) ---
Global $g_CombatState = ObjCreate("Scripting.Dictionary")

; =============================================================================
; Section 3: State Initialization
; =============================================================================

Func _Combat_InitState()
    $g_CombatState("BestTargetPtr") = 0

    ; Callback slots (see CONVENTIONS.md Section 4)
    $g_CombatState("OnFightStart") = ""
    $g_CombatState("OnFightEnd") = ""
EndFunc

; Initialize state at load time
_Combat_InitState()

; =============================================================================
; Section 4: BestTargetPtr Accessor
; =============================================================================
; $BestTargetPtr was a bare global mutated as a side-effect by query functions.
; It now lives in $g_CombatState("BestTargetPtr"). These accessors provide
; a clean read/write interface while remaining compatible with existing code
; that assigned $BestTargetPtr directly.
;
; NOTE: Callers in Froggy that read/write $BestTargetPtr will need to be
; migrated to use these accessors (or the dictionary key directly) when the
; combat functions are extracted in later KF tasks.

Func GetBestTarget()
    Return $g_CombatState("BestTargetPtr")
EndFunc

Func SetBestTarget($aPtr)
    $g_CombatState("BestTargetPtr") = $aPtr
EndFunc

; =============================================================================
; Section 5: Combat State Reader Functions
; =============================================================================
; Extracted from Froggy_HM_v1.6.au3. These are pure read-only queries that
; inspect game state without side effects.

; -----------------------------------------------------------------------------
; Wipe() — Wipe Detection
; Returns True if every party member is dead (full wipe).
;
; @param $aPartyPtr  Array from GetParty() (default: current party)
; @return            True if all party members are dead
; -----------------------------------------------------------------------------
Func Wipe($aPartyPtr = GetParty())
    Local $lDeadCount = 0
    Local $lPartySize = UBound($aPartyPtr) - 1
    For $i = 1 To $lPartySize
        If GetIsDead($aPartyPtr[$i]) Then $lDeadCount += 1
    Next
    If $lDeadCount = $lPartySize Then Return True
    Return False
EndFunc

; -----------------------------------------------------------------------------
; GetPartyHealth() — Party Health Average
; Returns the average HP fraction (0.0–1.0) across all party members.
;
; @param $aParty  Array from GetParty() (default: current party)
; @return         Average HP fraction
; -----------------------------------------------------------------------------
Func GetPartyHealth($aParty = GetParty())
    Local $lHealth = 0
    For $i = 1 To $aParty[0]
        $lHealth += GetHP($aParty[$i])
    Next
    Return $lHealth / $aParty[0]
EndFunc

; -----------------------------------------------------------------------------
; GetNumberOfEnemies() — Enemy Count in Range
; Counts living enemies within the specified compass range of the player.
;
; @param $aRange  Distance threshold (default: 1200, roughly aggro bubble)
; @return         Number of living enemies in range
; -----------------------------------------------------------------------------
Func GetNumberOfEnemies($aRange = 1200)
    Local $lCount = 0
    Local $lAgentArray = GetAgentArray(0xDB) ; 0xDB = All living NPCs
    If Not IsArray($lAgentArray) Or UBound($lAgentArray) == 0 Then Return 0
    For $i = 0 To UBound($lAgentArray) - 1
        If GetIsDead($lAgentArray[$i]) Then ContinueLoop
        If GetDistance(GetMyAgent(), $lAgentArray[$i]) < $aRange Then $lCount += 1
    Next
    Return $lCount
EndFunc

; -----------------------------------------------------------------------------
; IsKnocked() — Knockdown Detection
; Returns True if the specified agent is currently knocked down.
;
; @param $aAgent  Agent ID (default: -2 = player)
; @return         True if knocked down
; -----------------------------------------------------------------------------
Func IsKnocked($aAgent = -2)
    Return GetIsKnocked($aAgent)
EndFunc

; -----------------------------------------------------------------------------
; AgentHasEffect() — Effect Check on Any Agent
; Checks whether the specified agent (currently player only) has a particular
; skill effect active.
;
; NOTE: GetEffect() expects a hero index (0=player, 1-7=heroes), not an
; agent ID. This function currently only supports checking player effects
; (hero index 0). Extend when hero effect checking is needed.
;
; @param $aSkillID   The skill ID to check for
; @param $aAgentID   Agent ID (default: -2 = player; currently ignored)
; @return            True if the effect is active
; -----------------------------------------------------------------------------
Func AgentHasEffect($aSkillID, $aAgentID = -2)
    ; For simplicity, if checking player (-2), use hero index 0
    ; This function currently only supports checking player effects
    Local $lHeroIndex = 0  ; Always check player for now
    Local $lEffect = GetEffect($aSkillID, $lHeroIndex)
    ; GetEffect returns Null if effect not found, or DllStruct if found
    Return ($lEffect <> Null And Not IsArray($lEffect))
EndFunc

; =============================================================================
; Section 6: Target Selection (KF-018)
; =============================================================================
; Extracted from Froggy_HM_v1.6.au3 lines 1310-1603. These functions select
; the best enemy target based on range, filters (casting, hex, enchant), and
; skill slot requirements. Several are thin wrappers around GetBestTargetPtr
; with different filter combinations.
;
; GLOBALS READ:  $SkillBarCache, $SkillbarSlot
; GLOBALS WRITE: $BestTargetPtr (via direct assignment in GetBestTargetBySkillSlot)
;
; NOTE: GetBestTargetBySkillSlot still writes $BestTargetPtr as a bare global
; side-effect for compatibility with Fight()/UseSkills()/CanAttack() in Froggy.
; Migration to SetBestTarget() will happen when those callers are extracted.

; -----------------------------------------------------------------------------
; GetBestTargetPtr() — Best Enemy Target with Filters
; Returns the nearest living enemy agent within range, optionally filtered by
; casting state, hex presence, and enchantment presence.
;
; @param $aRange      Distance threshold (default: 1350)
; @param $casting     If True, only return enemies currently casting
; @param $nohex       If True, only return enemies without hexes
; @param $enchanted   If True, only return enemies with enchantments
; @return             Agent struct of best target, or 0 if none found
; -----------------------------------------------------------------------------
Func GetBestTargetPtr($aRange = 1350, $casting = False, $nohex = False, $enchanted = False)
    Local $lAgentArray = GetAgentArray(0xDB) ; 0xDB = All living NPCs
    If Not IsArray($lAgentArray) Then Return 0

    Local $lBestDist = 99999
    Local $lBestPtr = 0
    Local $lMe = GetAgentByID(-2)
    Local $lEnemiesInRange = 0

    For $i = 0 To UBound($lAgentArray) - 1
        Local $lAgent = $lAgentArray[$i]
        If GetIsDead($lAgent) Then ContinueLoop

        ; CRITICAL: Filter for enemies only (Allegiance = 3 = FOE)
        Local $lAllegiance = DllStructGetData($lAgent, 'Allegiance')
        If $lAllegiance <> 3 Then ContinueLoop  ; Skip non-enemies

        Local $lDist = GetDistance(GetMyAgent(), $lAgent)
        If $lDist > $aRange Then ContinueLoop
        $lEnemiesInRange += 1
        Local $lID = DllStructGetData($lAgent, 'ID')

        ; Apply filters
        If $casting And Not GetIsCasting($lID) Then ContinueLoop
        If $nohex And GetHasHex($lID) Then ContinueLoop
        If $enchanted And Not GetHasEnchantment($lID) Then ContinueLoop

        If $lDist < $lBestDist Then
            $lBestDist = $lDist
            $lBestPtr = $lAgent
        EndIf
    Next
    Return $lBestPtr
EndFunc

; -----------------------------------------------------------------------------
; GetBestMeleeTarget() — Nearest Enemy in Melee Range
; Convenience wrapper: calls GetBestTargetPtr with short range (250).
;
; @param $aRange  Distance threshold (default: 250, melee range)
; @return         Agent struct of nearest melee-range enemy, or 0
; -----------------------------------------------------------------------------
Func GetBestMeleeTarget($aRange = 250)
    Return GetBestTargetPtr($aRange) ; Simplified
EndFunc

; -----------------------------------------------------------------------------
; GetNoHexEnemy() — Nearest Enemy Without Hexes
; Returns the nearest enemy that does not currently have a hex on it.
;
; @param $aRange  Distance threshold (default: 1320)
; @return         Agent struct, or 0 if none found
; -----------------------------------------------------------------------------
Func GetNoHexEnemy($aRange = 1320)
    Return GetBestTargetPtr($aRange, False, True)
EndFunc

; -----------------------------------------------------------------------------
; GetBalledEnchantedEnemy() — Nearest Enchanted Enemy
; Returns the nearest enemy that has an enchantment active.
;
; @param $aRange  Distance threshold
; @return         Agent struct, or 0 if none found
; -----------------------------------------------------------------------------
Func GetBalledEnchantedEnemy($aRange)
    Return GetBestTargetPtr($aRange, False, False, True)
EndFunc

; -----------------------------------------------------------------------------
; GetMostBalledCastingEnemy() — Nearest Casting Enemy
; Returns the nearest enemy currently casting a spell (for interrupts).
;
; @param $aRange  Distance threshold (default: 1320)
; @return         Agent struct, or 0 if none found
; -----------------------------------------------------------------------------
Func GetMostBalledCastingEnemy($aRange = 1320)
    Return GetBestTargetPtr($aRange, True)
EndFunc

; -----------------------------------------------------------------------------
; GetLowestAlly() — Lowest Health Ally
; Scans all living allied agents and returns the one with the lowest HP
; fraction. Optionally excludes the player.
;
; @param $excludeself  If True, skip the player agent
; @return              Agent struct of lowest-HP ally, or 0 if none
; -----------------------------------------------------------------------------
Func GetLowestAlly($excludeself = False)
    Local $lLowestally = 0, $lLowestHP = 1.0
    Local $lAgentArray = GetAgentArray(0xDB)
    For $i = 0 To UBound($lAgentArray) - 1
        If DllStructGetData($lAgentArray[$i], 'Allegiance') <> 1 Then ContinueLoop
        If DllStructGetData($lAgentArray[$i], 'HP') <= 0 Then ContinueLoop
        If $excludeself And ID($lAgentArray[$i]) = ID(-2) Then ContinueLoop
        Local $lHP = GetHP($lAgentArray[$i])
        If $lHP < $lLowestHP Then
            $lLowestally = $lAgentArray[$i]
            $lLowestHP = $lHP
        EndIf
    Next
    Return $lLowestally
EndFunc

; -----------------------------------------------------------------------------
; GetBestTargetBySkillSlot() — Target Selection by Skill Slot Properties
; Reads the $target column of SkillBarCache for the given slot and dispatches
; to the appropriate targeting function based on target type code:
;   0 = self, 1 = spirit/minion, 3 = ally, 4 = other ally,
;   5 = enemy, 6 = dead ally, 14 = minion
;
; SIDE EFFECT: Writes $BestTargetPtr (bare global) for compatibility with
; Fight()/UseSkills()/CanAttack() in Froggy. Will migrate to SetBestTarget()
; when those callers are extracted.
;
; @param $aSkillSlot   Skill bar slot (1-8)
; @param $aAggroRange  Distance threshold (default: 1320)
; @return              Agent struct of selected target, or 0
; -----------------------------------------------------------------------------
Func GetBestTargetBySkillSlot($aSkillSlot, $aAggroRange = 1320)
    Local $MyPtr = GetAgentByID(-2)
    Local $targetType = $SkillBarCache[$aSkillSlot][$target]
    Switch $targetType
        Case 0  ; self
            If $SkillBarCache[$aSkillSlot][$type] == $Ward And GetDistance(GetMyAgent(), GetNearestEnemyToAgent(GetMyAgent())) > $aAggroRange Then Return False
            $BestTargetPtr = $MyPtr
        Case 1  ; spirit, minion
            $BestTargetPtr = GetNearestSpiritPtrToAgent()
        Case 3  ; ally
            If $SkillBarCache[$aSkillSlot][$condremove] <> "" Then
                $BestTargetPtr = MostCondsAllyPtr()
            ElseIf $SkillBarCache[$aSkillSlot][$hexremove] <> "" Then
                $BestTargetPtr = MostHexedAllyPtr()
            ElseIf $SkillBarCache[$aSkillSlot][$precast] <> "" Then
                $BestTargetPtr = $MyPtr
            ElseIf $SkillBarCache[$aSkillSlot][$survive] <> "" Then
                $BestTargetPtr = $MyPtr
            ElseIf $SkillBarCache[$aSkillSlot][$echoes] <> "" Then
                $BestTargetPtr = NeedEchoAlly($SkillBarCache[$aSkillSlot][$echoes])
            Else
                $BestTargetPtr = GetLowestAlly()
            EndIf
        Case 4  ; other ally
            If $SkillBarCache[$aSkillSlot][$condremove] <> "" Then
                $BestTargetPtr = MostCondsAllyPtr(True)
            ElseIf $SkillBarCache[$aSkillSlot][$hexremove] <> "" Then
                $BestTargetPtr = MostHexedAllyPtr(True)
            ElseIf $SkillBarCache[$aSkillSlot][$precast] <> "" Then
                $BestTargetPtr = $MyPtr
            ElseIf $SkillBarCache[$aSkillSlot][$survive] <> "" Then
                $BestTargetPtr = $MyPtr
            ElseIf $SkillBarCache[$aSkillSlot][$echoes] <> "" Then
                $BestTargetPtr = NeedEchoAlly($SkillBarCache[$aSkillSlot][$echoes])
            Else
                $BestTargetPtr = GetLowestAlly(True)
            EndIf
        Case 5  ; enemy
            If $SkillBarCache[$aSkillSlot][$hexes] <> "" Then
                $BestTargetPtr = GetNoHexEnemy($aAggroRange)
                If $BestTargetPtr = 0 Then $BestTargetPtr = GetBestTargetPtr($aAggroRange)
            ElseIf $SkillBarCache[$aSkillSlot][$enchantremove] <> "" Then
                $BestTargetPtr = GetBalledEnchantedEnemy($aAggroRange)
            ElseIf $SkillBarCache[$aSkillSlot][$attackskill] <> "" Then
                $BestTargetPtr = GetBestMeleeTarget()
            ElseIf $SkillBarCache[$aSkillSlot][$rupt] <> "" Then
                $BestTargetPtr = GetMostBalledCastingEnemy($aAggroRange)
            Else
                $BestTargetPtr = GetBestTargetPtr($aAggroRange)
            EndIf
        Case 6  ; dead ally
            $BestTargetPtr = GetNearestDeadAllyPtrToAgent()
        Case 14 ; spirit, minion
            $BestTargetPtr = GetNearestMinionPtrToAgent()
    EndSwitch

    If $BestTargetPtr <> 0 Then Return $BestTargetPtr
    Return 0
EndFunc

; =============================================================================
; Section 7: Ally/Summon Targeting (KF-019)
; =============================================================================
; Extracted from Froggy_HM_v1.6.au3 lines 1349-1627. These functions find
; specific ally/summon targets for support skills (resurrect, condition/hex
; removal, echo application, spirit/minion targeting).
;
; GLOBALS READ: None (use GWA2 API calls)

; -----------------------------------------------------------------------------
; GetNearestSpiritPtrToAgent() — Nearest Spirit
; Returns the nearest NPC spirit to the specified agent.
; NOTE: Simplified placeholder — uses GetNearestNPCToCoords which may not
; filter specifically for spirits. Refine when spirit type ID is available.
;
; @param $aAgent  Agent ID (default: -2 = player)
; @return         Agent struct, or result of GetNearestNPCToCoords
; -----------------------------------------------------------------------------
Func GetNearestSpiritPtrToAgent($aAgent = -2)
    ; Simplified implementation
    Return GetNearestNPCToCoords(GetX($aAgent), GetY($aAgent)) ; Placeholder
EndFunc

; -----------------------------------------------------------------------------
; GetNearestMinionPtrToAgent() — Nearest Minion
; Returns the nearest minion to the specified agent.
; NOTE: Placeholder — always returns 0. Implement when minion allegiance/type
; filtering is available in GWA2.
;
; @param $aAgent  Agent ID (default: -2 = player)
; @return         0 (placeholder)
; -----------------------------------------------------------------------------
Func GetNearestMinionPtrToAgent($aAgent = -2)
    Return 0 ; Placeholder
EndFunc

; -----------------------------------------------------------------------------
; GetNearestDeadAllyPtrToAgent() — Nearest Dead Ally
; Scans the party array and returns the first dead member found.
; Used for resurrection skill targeting.
;
; @param $aAgent  Agent ID (default: -2 = player; currently unused)
; @return         Agent struct of first dead party member, or 0
; -----------------------------------------------------------------------------
Func GetNearestDeadAllyPtrToAgent($aAgent = -2)
    Local $party = GetParty()
    For $i = 0 To UBound($party) - 1
        If GetIsDead($party[$i]) Then Return $party[$i]
    Next
    Return 0
EndFunc

; -----------------------------------------------------------------------------
; NeedEchoAlly() — Ally Needing Echo/Refrain
; Returns an ally that needs the specified echo/refrain skill applied.
; NOTE: Simplified placeholder — always returns the player agent. Implement
; proper echo effect checking when multi-hero effect queries are available.
;
; @param $aSkillID  The echo/refrain skill ID to check for
; @return           Agent struct (currently always player)
; -----------------------------------------------------------------------------
Func NeedEchoAlly($aSkillID)
    Return GetAgentByID(-2) ; Simplified
EndFunc

; -----------------------------------------------------------------------------
; MostCondsAllyPtr() — Ally with Most Conditions
; Iterates party members and returns the one with the most conditions.
; NOTE: Simplified placeholder — iterates heroes but does not actually count
; conditions (complex effect checking not yet implemented). Returns the last
; hero ID visited.
;
; @param $excludeself  If True, skip hero index 0 (player)
; @return              Hero agent ID (placeholder logic)
; -----------------------------------------------------------------------------
Func MostCondsAllyPtr($excludeself = False)
    Local $MostConditionedAlly = 0
    Local $lMostConditions = 0
    For $aHeroNumber = 0 To GetPartySize() - 1
        If $excludeself = True And $aHeroNumber = 0 Then ContinueLoop
        ; Logic simplified: assume random ally if complex effect checking fails
        $MostConditionedAlly = GetHeroID($aHeroNumber)
    Next
    Return $MostConditionedAlly ; Placeholder
EndFunc

; -----------------------------------------------------------------------------
; MostHexedAllyPtr() — Ally with Most Hexes
; Iterates party members and returns the one with the most hexes.
; NOTE: Simplified placeholder — iterates heroes but does not actually count
; hexes (complex effect checking not yet implemented). Returns the last
; hero ID visited.
;
; @param $excludeself  If True, skip hero index 0 (player)
; @return              Hero agent ID (placeholder logic)
; -----------------------------------------------------------------------------
Func MostHexedAllyPtr($excludeself = False)
    Local $lMostHexedally = 0
    For $aHeroNumber = 0 To GetPartySize() - 1
        If $excludeself = True And $aHeroNumber = 0 Then ContinueLoop
        $lMostHexedally = GetHeroID($aHeroNumber)
    Next
    Return $lMostHexedally ; Placeholder
EndFunc
; =============================================================================
; BotCore-Combat-CastEngine.au3.tmp
;
; TEMPORARY FILE -- merge into BotCore-Combat.au3 after both agents complete.
;
; KF-021: Skillbar caching (CacheSkillBar)
; KF-022: Cast/use decision engine (CanCast, CanAttack, CanUse)
; KF-023: Active combat execution (UseSkillSmart, UseSkills, Fight)
;
; Extracted from Froggy_HM_v1.6.au3 lines 1368-1659.
; MemoryRead -> MemRead conversion: NOT NEEDED (source already uses MemRead).
;
; No #include needed -- all dependencies resolve through Froggy_Includes.au3.
; =============================================================================

; =============================================================================
; GLOBALS DOCUMENTATION
; =============================================================================
;
; GLOBALS READ:
;   $SkillBarCache[9][23]  -- skill bar cache array (declared in BotCore-Combat.au3 Section 1-2)
;   $SkillbarSlot[3500]    -- maps skill ID -> slot index (declared in BotCore-Combat.au3 Section 2)
;   $BestTargetPtr         -- current best target (bare global, still used for compat)
;   Skill type constants   -- $Hex, $Spell, $Enchantment, $Well, $Ward, $ItemSpell,
;                             $WeaponSpell, $Attack, $Ritual, $Signet, $Glyph, $Shout,
;                             $Preparation, $Trap, $Chant, $EchoRefrain, $Disguise
;                             (from Skill_Types.au3)
;   Skill ID constants     -- $Diversion, $Visions_of_Regret, $Backfire, $Soul_Leech,
;                             $Mistrust, $Mark_of_Subversion, $Spiteful_Spirit,
;                             $Ineptitude, $Clumsiness, $Wandering_Eye, $Well_of_Silence,
;                             $Ignorance, $Quickening_Zephyr, $Shadow_Form,
;                             $Glyph_of_Swiftness, $Shroud_Of_Distress,
;                             $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick,
;                             $Shadow_Refuge, $Heart_of_Shadow, $Mystic_Regeneration,
;                             $Shield_of_Judgment, $Shielding_Hands, $Shield_of_Absorption,
;                             $Finish_Him, $I_Am_Unstoppable,
;                             $Summon_Spirits_Kurzick, $Summon_Spirits_Luxon
;                             (from Skill_IDs.au3)
;   Cache column enum      -- $all, $ptr, $energyreq, $adrereq, $type, $target,
;                             $hexes, $pressure, $bind, $survive, $attackskill,
;                             $heal, $bond, $condremove, $hexremove, $enchantremove,
;                             $precast, $chantsnshouts, $echoes, $rupt, $skilltype
;                             (from BotCore-Combat.au3 Section 1)
;   PvP skill ID variants -- $Visions_of_Regret_PvP, $Mistrust_PvP, $Wandering_Eye_PvP
;
; GLOBALS WRITTEN:
;   $SkillBarCache[9][23]  -- populated by CacheSkillBar()
;   $SkillbarSlot[3500]    -- populated by CacheSkillBar()
;   $BestTargetPtr         -- set by CanAttack() as side-effect
;
; EXTERNAL FUNCTION DEPENDENCIES (not in this file):
;   From BotCore-Combat.au3:
;     Wipe(), IsKnocked(), AgentHasEffect(), GetNumberOfEnemies(),
;     GetBestTargetBySkillSlot(), GetBestTargetPtr()
;   From GWA2/botshub:
;     GetMapLoading(), Disconnected(), IsRecharged(), GetEnergy(), GetHP(),
;     GetAdrenaline(), GetSkillbarSkillRecharge(), GetEffectTimeRemaining(),
;     GetSkillbarSkillID(), GetSkillPtr(), MemRead(), UseSkill(),
;     ChangeTarget(), GetIsDead(), Attack(), CancelAll(), Move(),
;     GetMyID(), ID(), X(), Y(), GetAgentByID(), DllStructGetData()
;   From Froggy (not yet extracted):
;     GetNearestEnemyDistance(), PickupLootEx(), Out()
;   From BotCore-SkillRules.au3:
;     IsHexSpell(), IsPressureSkill(), IsSurvivalSkill(), IsAttackSkill(),
;     IsHealSkill(), IsBondSkill(), IsCondRemoveSkill(), IsHexRemoveSkill(),
;     IsEnchantRemoveSkill(), IsPrecastSkill(), IsChantSkill(), IsShoutSkill(),
;     IsEchoRefrainskill(), IsBindingSkill()
; =============================================================================


; #############################################################################
; KF-021: SKILLBAR CACHING
; #############################################################################

; -----------------------------------------------------------------------------
; CacheSkillBar() -- Populate SkillBarCache and SkillbarSlot Arrays
;
; Reads each skill slot (1-8) from the live skillbar, resolves the skill data
; pointer, and populates both the $SkillBarCache[slot][column] matrix and the
; reverse-lookup $SkillbarSlot[skillID] = slotNumber array.
;
; For each slot, reads from memory:
;   - energy cost   (offset +28, 'long')
;   - adrenaline    (offset +56, 'dword')
;   - skill type    (offset +12, 'long')
;   - target type   (offset +49, 'byte')
;
; Then classifies the skill using the Is*Skill() functions from
; BotCore-SkillRules.au3 and stores the skill ID in the appropriate
; cache column (hexes, pressure, survive, attackskill, heal, bond,
; condremove, hexremove, enchantremove, precast, chantsnshouts, echoes, bind).
;
; GLOBALS READ:   (none -- populates from GWA2 API)
; GLOBALS WRITTEN: $SkillBarCache, $SkillbarSlot
;
; @return  True on completion
; -----------------------------------------------------------------------------
Func CacheSkillBar()
	Out("Mapping your skill bar")
	Sleep(200)
	For $i = 1 To 8
		Local $aSkillID = GetSkillbarSkillID($i)
		If $aSkillID = 0 Then ContinueLoop
		$SkillbarSlot[$aSkillID] = $i
		$SkillBarCache[$i][$all] = $aSkillID
		$SkillBarCache[$i][$ptr] = GetSkillPtr($aSkillID)
		$SkillBarCache[$i][$energyreq] = MemRead($SkillBarCache[$i][$ptr] + 28, 'long')
		$SkillBarCache[$i][$adrereq] = MemRead($SkillBarCache[$i][$ptr] + 56, 'dword')
		$SkillBarCache[$i][$type] = MemRead($SkillBarCache[$i][$ptr] + 12, "long")
		$SkillBarCache[$i][$target] = MemRead($SkillBarCache[$i][$ptr] + 49, "byte")

		If IsHexSpell($aSkillID) Then $SkillBarCache[$i][$hexes] = $aSkillID
		If IsPressureSkill($aSkillID) Then $SkillBarCache[$i][$pressure] = $aSkillID
		If IsSurvivalSkill($aSkillID) Then $SkillBarCache[$i][$survive] = $aSkillID
		If IsAttackSkill($aSkillID) Then $SkillBarCache[$i][$attackskill] = $aSkillID
		If IsHealSkill($aSkillID) Then $SkillBarCache[$i][$heal] = $aSkillID
		If IsBondSkill($aSkillID) Then $SkillBarCache[$i][$bond] = $aSkillID
		If IsCondRemoveSkill($aSkillID) Then $SkillBarCache[$i][$condremove] = $aSkillID
		If IsHexRemoveSkill($aSkillID) Then $SkillBarCache[$i][$hexremove] = $aSkillID
		If IsEnchantRemoveSkill($aSkillID) Then $SkillBarCache[$i][$enchantremove] = $aSkillID
		If IsPrecastSkill($aSkillID) Then $SkillBarCache[$i][$precast] = $aSkillID
		If IsChantSkill($aSkillID) Or IsShoutSkill($aSkillID) Then $SkillBarCache[$i][$chantsnshouts] = $aSkillID
		If IsEchoRefrainskill($aSkillID) Then $SkillBarCache[$i][$echoes] = $aSkillID
	Next
	Out("Mapping your skill bar - completed")
	Return True
EndFunc


; #############################################################################
; KF-022: CAST / USE DECISION ENGINE
; #############################################################################

; -----------------------------------------------------------------------------
; CanCast($aSkillSlot) -- Check If Skill Can Be Cast
;
; Validates that the game is in a castable state (explorable area, not dead,
; not knocked down, not wiped, skill recharged) and then checks for debuffs
; that would prevent the skill type from being used (e.g. Diversion blocks
; spells, Ineptitude blocks attacks, Ignorance blocks signets, etc.).
;
; If $aSkillSlot = 0, treats the skill type as $Attack (basic attack check).
;
; GLOBALS READ:  $SkillBarCache (column $type)
; GLOBALS WRITTEN: (none)
;
; @param $aSkillSlot  Skill bar slot (1-8), or 0 for basic attack check
; @return             True if the skill/attack can be cast
; -----------------------------------------------------------------------------
Func CanCast($aSkillSlot = 0)
	If GetMapLoading() == 2 Then Disconnected()
	If GetMapLoading() <> 1 Then Return False  ; Can only cast in explorable areas
	If IsKnocked() Or GetIsDead(-2) Or Wipe() = 1 Then Return False
	If $aSkillSlot <> 0 And Not IsRecharged($aSkillSlot) Then Return False
	Local $aType = $SkillBarCache[$aSkillSlot][$type]
	If $aSkillSlot = 0 Then $aType = $Attack

	Switch $aType
		Case $Hex, $Spell, $Enchantment, $Well, $Ward, $ItemSpell, $WeaponSpell
			If AgentHasEffect($Diversion) <> 0 Then Return False
			If AgentHasEffect($Visions_of_Regret) <> 0 Then Return False
			If AgentHasEffect($Visions_of_Regret_PvP) <> 0 Then Return False
			If AgentHasEffect($Backfire) <> 0 Then Return False
			If AgentHasEffect($Soul_Leech) <> 0 Then Return False
			If AgentHasEffect($Mistrust) <> 0 Then Return False
			If AgentHasEffect($Mistrust_PvP) <> 0 Then Return False
			If AgentHasEffect($Mark_of_Subversion) <> 0 Then Return False
			If AgentHasEffect($Spiteful_Spirit) <> 0 Then Return False
		Case $Attack
			If AgentHasEffect($Ineptitude) + AgentHasEffect($Clumsiness) + AgentHasEffect($Spiteful_Spirit) + AgentHasEffect($Wandering_Eye) + AgentHasEffect($Wandering_Eye_PvP) <> 0 Then
				Out("Can't Attack")
				Return False
			EndIf
		Case $Ritual, $Signet, $Glyph, $Shout, $Preparation, $Trap, $Chant, $EchoRefrain, $Disguise
			If AgentHasEffect($Diversion) Then Return False
		Case $Shout, $Chant
			If AgentHasEffect($Well_of_Silence) Then Return False
		Case $Signet
			If AgentHasEffect($Ignorance) Then Return False
	EndSwitch
	Return True
EndFunc

; -----------------------------------------------------------------------------
; CanAttack($aRange) -- Check If Player Can Attack an Enemy
;
; Finds the best enemy target in range via GetBestTargetPtr() and stores it
; in $BestTargetPtr (side-effect). Then checks CanCast() for basic attack
; ability (slot 0 = attack type).
;
; GLOBALS READ:   (none directly -- delegates to GetBestTargetPtr, CanCast)
; GLOBALS WRITTEN: $BestTargetPtr (via GetBestTargetPtr assignment)
;
; @param $aRange  Distance threshold (default: 1320)
; @return         True if an attackable enemy exists in range
; -----------------------------------------------------------------------------
Func CanAttack($aRange = 1320)
	$BestTargetPtr = GetBestTargetPtr($aRange)
	If $BestTargetPtr = 0 Then Return False
	If CanCast() Then Return True
	Return False
EndFunc

; -----------------------------------------------------------------------------
; CanUse($aSkillSlot, $aAggroRange) -- Full Skill Usability Check
;
; Comprehensive check combining:
;   1. CanCast() -- game state and debuff checks
;   2. GetBestTargetBySkillSlot() -- valid target exists
;   3. IsRecharged() -- skill not on cooldown
;   4. Energy/adrenaline sufficiency (with Quickening Zephyr +30% cost)
;   5. Skill-specific conditions:
;      - Binding rituals: enemies must be in range
;      - Survival skills: HP/effect thresholds (Shadow Form timing, HP gates)
;      - Pressure skills: Finish Him requires target HP < 45%
;      - Heal skills: lowest ally must be below 80% HP
;
; GLOBALS READ:  $SkillBarCache (multiple columns), $BestTargetPtr
; GLOBALS WRITTEN: $BestTargetPtr (via GetBestTargetBySkillSlot side-effect)
;
; @param $aSkillSlot   Skill bar slot (1-8)
; @param $aAggroRange  Distance threshold (default: 1320)
; @return              True if the skill can and should be used now
; -----------------------------------------------------------------------------
Func CanUse($aSkillSlot, $aAggroRange = 1320)
	Local $ZephyrEffect = $SkillBarCache[$aSkillSlot][$energyreq] * 30 / 100
	Local $ZephyrAddition = $SkillBarCache[$aSkillSlot][$energyreq] + $ZephyrEffect

	If $aSkillSlot = "" Then Return
	If Not CanCast($aSkillSlot) Then Return False
	If GetBestTargetBySkillSlot($aSkillSlot, $aAggroRange) = 0 Then Return False
	If Not IsRecharged($aSkillSlot) Then Return False
	If GetEnergy(-2) < $SkillBarCache[$aSkillSlot][$energyreq] Then Return False
	If AgentHasEffect($Quickening_Zephyr, -2) And GetEnergy(-2) < $ZephyrAddition Then Return False
	If $SkillBarCache[$aSkillSlot][$adrereq] <> 0 And GetAdrenaline($aSkillSlot) < $SkillBarCache[$aSkillSlot][$adrereq] Then Return False

;~ BINDING RITUALS
	If $SkillBarCache[$aSkillSlot][$bind] <> "" Then
		Switch $SkillBarCache[$aSkillSlot][$bind]
			Case $Summon_Spirits_Kurzick, $Summon_Spirits_Luxon
				If GetNumberOfEnemies($aAggroRange) = 0 Then Return False
		EndSwitch
	EndIf

;~ SURVIVAL SKILLS
	If $SkillBarCache[$aSkillSlot][$survive] <> "" Then
		Switch $SkillBarCache[$aSkillSlot][$survive]
			Case $I_Am_Unstoppable
				If GetEffectTimeRemaining($Shadow_Form) > 5000 And Not GetIsKnocked(-2) Then Return False
			Case $Glyph_of_Swiftness
				If GetEffectTimeRemaining($Shadow_Form) > 5000 Then Return False
				If GetSkillbarSkillRecharge($SkillbarSlot[$Shadow_Form]) > 5000 Then Return False
			Case $Shadow_Form
				If GetEffectTimeRemaining($Shadow_Form) > 5000 Then Return False
				If GetEffectTimeRemaining($Glyph_of_Swiftness) = 0 Then Return False
			Case $Shroud_Of_Distress
				If GetHP(-2) > 0.9 Then Return False
				If GetEffectTimeRemaining($Shroud_Of_Distress) > 5000 Then Return False
			Case $Shadow_Sanctuary_Luxon, $Shadow_Sanctuary_Kurzick, $Shadow_Refuge
				If GetHP(-2) > 0.7 Then Return False
			Case $Heart_of_Shadow
				If GetHP(-2) > 0.5 Then Return False
			Case $Mystic_Regeneration
				If GetEffectTimeRemaining($Mystic_Regeneration) < 4000 Then Return True
			Case $Shield_of_Judgment
				If GetEffectTimeRemaining($Shielding_Hands) < 4500 And GetEffectTimeRemaining($Shield_of_Absorption) < 4500 Then Return False
		EndSwitch
	EndIf

;~ PRESSURE SKILLS
	If $SkillBarCache[$aSkillSlot][$pressure] <> "" Then
		Switch $SkillBarCache[$aSkillSlot][$pressure]
			Case $Finish_Him
				If DllStructGetData(GetAgentByID(ID($BestTargetPtr)), 'Health') > 0.45 Then Return False
		EndSwitch
	EndIf

;~ HEAL SKILLS
	If $SkillBarCache[$aSkillSlot][$heal] <> "" Then
		$lowestally = GetLowestAlly()
		If IsHealSkill($SkillBarCache[$aSkillSlot][$heal]) And GetHP($lowestally) > 0.8 Then Return False
	EndIf

	Return True
EndFunc


; #############################################################################
; KF-023: ACTIVE COMBAT EXECUTION
; #############################################################################

; -----------------------------------------------------------------------------
; UseSkillSmart($aSkillSlot, $aTarget, $aTimeout, $aSkillbarPtr)
;     Smart Skill Usage with Target Selection
;
; Fires a skill at the specified target, then polls until the skill is no
; longer castable (recharging) or the timeout expires. Handles:
;   - Dead target/player early exit
;   - Energy depletion early exit
;   - Target change before casting
;   - Aftercast delay (reads float at skill ptr + 64, converts to ms)
;
; GLOBALS READ:  $SkillBarCache (column $energyreq), $SkillbarSlot
; GLOBALS WRITTEN: (none)
;
; @param $aSkillSlot    Skill bar slot (1-8)
; @param $aTarget       Target agent (default: -2 = player)
; @param $aTimeout      Max wait in ms (default: 6000)
; @param $aSkillbarPtr  Unused (legacy parameter)
; @return               True on successful cast, empty otherwise
; -----------------------------------------------------------------------------
Func UseSkillSmart($aSkillSlot, $aTarget = -2, $aTimeout = 6000, $aSkillbarPtr = 0)
	Local $lDeadlock = TimerInit(), $lAgentID = ID($aTarget)
	If $lAgentID = 0 Or GetIsDead(-2) Then Return
	If $lAgentID <> GetMyID() Then ChangeTarget($aTarget)
	UseSkill($aSkillSlot, $aTarget)
	Do
		Sleep(50)
		If GetIsDead($aTarget) Then Return
		If GetEnergy(-2) < $SkillBarCache[$aSkillSlot][$energyreq] Then Return
	Until Not CanCast($aSkillSlot) Or TimerDiff($lDeadlock) > $aTimeout
	Sleep(MemRead(GetSkillPtr($SkillbarSlot[$aSkillSlot]) + 64, "float") * 1000) ; Aftercast
	Return True
EndFunc

; -----------------------------------------------------------------------------
; UseSkills($aAggroRange, $skilltype) -- Iterate Skillbar and Use Available Skills
;
; Loops through skill slots 1-8. For each slot:
;   1. Checks for death/wipe/map-loading bail-out
;   2. Skips slots where the $skilltype column is empty
;   3. Calls CanUse() for full usability check
;   4. Calls UseSkillSmart() with $BestTargetPtr (set by CanUse -> GetBestTargetBySkillSlot)
;   5. Exits early if enemies leave aggro range
;
; GLOBALS READ:  $SkillBarCache (column $skilltype), $BestTargetPtr
; GLOBALS WRITTEN: (none directly -- delegates to CanUse/UseSkillSmart)
;
; @param $aAggroRange  Distance threshold (default: 1000)
; @param $skilltype    Cache column index to filter by (default: $all)
; -----------------------------------------------------------------------------
Func UseSkills($aAggroRange = 1000, $skilltype = $all)
	For $aSkillSlot = 1 To 8
		If GetIsDead(-2) Or Wipe() = 1 Or GetMapLoading() == 2 Then ExitLoop
		If $SkillBarCache[$aSkillSlot][$skilltype] = "" Then ContinueLoop
		If CanUse($aSkillSlot, $aAggroRange) Then UseSkillSmart($aSkillSlot, $BestTargetPtr)
		If GetNearestEnemyDistance() > $aAggroRange Then Return
	Next
EndFunc

; -----------------------------------------------------------------------------
; Fight($aAggroRange, $careful) -- Main Combat Loop
;
; Outer combat loop that runs until enemies leave range, player dies, party
; wipes, or a 4-minute safety timeout expires. Each iteration:
;   1. (careful mode) Cancels current action
;   2. Checks for attackable enemies via CanAttack()
;   3. Initiates basic attack on $BestTargetPtr
;   4. (careful mode) Moves toward target
;   5. Calls UseSkills() to fire all available skills
;   6. Checks if nearest enemy is still in range
; After the loop, picks up loot within 3000 range.
;
; GLOBALS READ:  $BestTargetPtr (set by CanAttack -> GetBestTargetPtr)
; GLOBALS WRITTEN: (none directly -- delegates to CanAttack which sets $BestTargetPtr)
;
; EXTERNAL DEPS (not yet extracted):
;   GetNearestEnemyDistance() -- Froggy_HM_v1.6.au3 line 853
;   PickupLootEx()           -- Froggy_HM_v1.6.au3 line 901
;   Out()                    -- logging function
;
; @param $aAggroRange  Distance threshold (default: 1000)
; @param $careful      If True, cancel actions and move toward target each tick
; -----------------------------------------------------------------------------
Func Fight($aAggroRange = 1000, $careful = False)
	Out("Fighting enemies")
	Local $TimerToGetOut = TimerInit()
	Local $nearDist = 0
	Do
		If $careful Then CancelAll()
		Local $canAtk = CanAttack($aAggroRange)
		If $canAtk Then
			Attack($BestTargetPtr, True)
		EndIf
		Sleep(100)
		If $careful Then
			Move(X($BestTargetPtr), Y($BestTargetPtr))
			Sleep(300)
		EndIf
		UseSkills($aAggroRange, $all)
		$nearDist = GetNearestEnemyDistance()
	Until $nearDist > $aAggroRange Or GetIsDead(-2) Or Wipe() Or TimerDiff($TimerToGetOut) > 240000
	PickupLootEx(3000)
EndFunc
