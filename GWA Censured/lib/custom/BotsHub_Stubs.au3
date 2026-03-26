#include-once
#include <Math.au3>

; =============================================================================
; BotsHub_Stubs.au3
;
; Stubs and compatibility shims for BotsHub framework globals and functions
; that upstream files expect but our GWA Censured project doesn't provide.
;
; These allow upstream botshub/ files to load without the full BotsHub
; application framework (GUI, config caching, etc.).
;
; Created 2026-03-25 for Phase 4b
; =============================================================================

; === Run Status Constants ===
Global Const $SUCCESS = True
Global Const $FAIL = False
Global Const $PAUSE = 2

; === Globals expected by upstream Utils.au3 / Utils-Storage.au3 ===
Global $run_timer = TimerInit()
Global $run_options_cache[]                    ; Empty map
Global $inventory_management_cache[]           ; Empty map
Global $bags_count = 4
Global $salvageKit = 0
Global $bagEmptySlots = 0
Global $district_name = 'International'

; === Globals expected by upstream Utils-Items_Modstructs.au3 ===
Global $valuableModsByOSWeaponType[]           ; Empty map
Global $weaponModsByType[]                     ; Empty map
Global $weaponInscriptionsByType[]             ; Empty map

; === Globals expected by upstream GWA2.au3 ===
Global $ATTRIBUTES_BY_PROFESSION_MAP[]         ; Empty map
Global $WEAPONS_MAX_DAMAGE_PER_LEVEL[]         ; Empty map

; === Map ID aliases — these are set AFTER botshub loads (see bottom of file) ===
; $ID_GADDS_CAMP, $ID_PLANT_FIBER defined at bottom after upstream IDs are available

; === GUI/INI paths ===
Global $MOD_INI_PATH = @ScriptDir & '\Mod_Settings.ini'

; === Logging functions (upstream has these in BotsHub-GUI.au3) ===
; Our versions are defined in Utils-Debugger.au3 (custom/) or GUI_Functions.au3
; Only stub here if they're not defined elsewhere in the include chain
If Not IsDeclared('__LOGGING_DEFINED') Then
	Global $__LOGGING_DEFINED = True
	; These are already defined in our custom Utils-Debugger.au3
	; Only define if not already present (IsDeclared check at call time)
EndIf

; === Compatibility aliases for renamed upstream functions ===

;~ GetValue was renamed to GetLabel in upstream
Func GetValue($key)
	Return GetLabel($key)
EndFunc

;~ InitializeGameClientData wraps the upstream split init sequence
Func InitializeGameClientData($changeTitle = True, $initUseStringLog = False, $inituse_event_system = True)
	Return InitializeClient($changeTitle, $initUseStringLog, $inituse_event_system)
EndFunc

; === Function stubs for BotsHub features we don't use ===

Func _iniToArray($sIni, $sSection = '')
	Local $a[0]
	Return $a
EndFunc

Func _arrayToIni($sIni = '', $sSection = '', $aData = Null)
	Return ''
EndFunc

Func GetAllChecked($cache = Null, $path = '', $depth = 1, $startDepth = 1)
	Local $a[0]
	Return $a
EndFunc

Func ResetBotsSetups()
	; No-op: BotsHub team setup not used
EndFunc

Func Purgehook()
	; No-op: BotsHub hook system not used
EndFunc

Func TolSleep($ms)
	Sleep($ms)
EndFunc

;~ Compat wrapper: our code passes $townID but upstream GetSalvageKit only takes $buyKit
Func GetSalvageKitCompat($buyKit = True, $townID = 0)
	Return GetSalvageKit($buyKit)
EndFunc

Func IdentifyBags()
	; No-op: handled by our maintenance system
EndFunc

; === Logging functions ===
; Upstream defines these in BotsHub-GUI.au3 which we don't use.
; Our GUI_Functions.au3 defines Out() and GUI_Print().
; These provide the standard logging API.

Func Info($msg)
	ConsoleWrite('[INFO] ' & $msg & @CRLF)
EndFunc

Func Warn($msg)
	ConsoleWrite('[WARN] ' & $msg & @CRLF)
EndFunc

Func Error($msg)
	ConsoleWrite('[ERROR] ' & $msg & @CRLF)
EndFunc

Func Debug($msg)
	ConsoleWrite('[DEBUG] ' & $msg & @CRLF)
EndFunc

Func Notice($msg)
	ConsoleWrite('[NOTICE] ' & $msg & @CRLF)
EndFunc

Func WarnOnce($msg)
	ConsoleWrite('[WARN] ' & $msg & @CRLF)
EndFunc

; === GWA2 API aliases (functions renamed in upstream) ===

Func GetIsIdentified($item)
	Return IsIdentified($item)
EndFunc

Func GetLanguage()
	Return GetDisplayLanguage()
EndFunc

Func GetSkillbarSkillRecharge($skillSlot, $heroIndex = 0)
	; Upstream uses IsRecharged() which returns True/False
	; Old API returned the recharge timer value
	; This is a best-effort shim — callers checking == 0 will work
	Return IsRecharged($skillSlot, $heroIndex) ? 0 : 1
EndFunc

Func GetPlayerStatus()
	; Old GWA2 function — return player status from agent struct
	Local $me = GetMyAgent()
	If Not IsDllStruct($me) Then Return 0
	Return DllStructGetData($me, 'TypeMap')
EndFunc

Func GetMaxSlots($bag)
	If Not IsDllStruct($bag) Then Return 0
	Return DllStructGetData($bag, 'Slots')
EndFunc

Func ZoneMap($mapID, $district = 0)
	; Upstream renamed to CommandMoveMap or similar
	; Use Travel as fallback
	TravelToOutpost($mapID, $district)
EndFunc

Func IsRareSkin($modelID)
	; Check against our rare skin array
	If $modelID < 0 Or $modelID >= UBound($aRareSkin) Then Return False
	Return $aRareSkin[$modelID] <> ''
EndFunc

Func InitializeClient($changeTitle = True, $initUseStringLog = False, $initUseEventSystem = True)
	Return InitializeGameClientForGWA2($changeTitle)
EndFunc

; === Function aliases for renamed upstream functions ===

Func GetProcessID()
	Return GetPID()
EndFunc

; === Extension stubs (for optional modules not loaded) ===
; These are no-ops unless an extension module overrides them

Func Extend_Scanner()
EndFunc

Func Extend_AssemblerData()
EndFunc

Func Extend_Assembler()
EndFunc
