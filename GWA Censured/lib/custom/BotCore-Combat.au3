#include-once
; =============================================================================
; BotCore-Combat.au3 — Combat State Container & State Reader Functions
;
; KF-017: Defines the combat state globals (SkillBarCache, SkillbarSlot,
;         BestTargetPtr) with corrected sizing.
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
