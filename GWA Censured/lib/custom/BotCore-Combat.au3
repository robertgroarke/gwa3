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
