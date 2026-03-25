#include-once
; =============================================================================
; Froggy HM Master Include File
;
; This file wires together all BotsHub core libraries and GWA Censured
; custom extensions in the correct dependency order.
;
; Structure:
;   lib/          - BotsHub core libraries (upstream, minimal modifications)
;   lib/custom/   - GWA Censured custom code (our additions)
; =============================================================================

; === AutoIt Standard Libraries ===
#include <GUIConstantsEx.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>

; === BotsHub Core Libraries (lib/) ===
#include 'GWA2_Headers.au3'
#include 'GWA2.au3'
#include 'Utils.au3'
#include 'JSON.au3'
#include 'SQLite.au3'
#include 'SQLite.dll.au3'
#include 'Utils-Debugger.au3'

; === Custom Data Files (lib/custom/) ===
#include 'custom\Map_IDs.au3'
#include 'custom\Skill_IDs.au3'
#include 'custom\Skill_Types.au3'

; === Custom Libraries (lib/custom/) ===
#include 'custom\GUI_Functions.au3'
#include 'custom\Utils-Maintenance.au3'
