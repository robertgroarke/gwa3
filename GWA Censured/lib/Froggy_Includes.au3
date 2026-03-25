#include-once
; =============================================================================
; Froggy HM Master Include File
;
; Structure:
;   lib/botshub/  - BotsHub upstream files (drop-in replaceable)
;   lib/custom/   - GWA Censured custom code (our additions)
; =============================================================================

; === AutoIt Standard Libraries ===
#include <GUIConstantsEx.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>

; === BotsHub Core Libraries (lib/botshub/) ===
#include 'botshub\GWA2_Assembly.au3'
#include 'botshub\GWA2.au3'
#include 'botshub\Utils.au3'
#include 'botshub\Utils-Agents.au3'
#include 'botshub\Utils-Storage.au3'
#include 'botshub\JSON.au3'

; === Custom Compatibility Layer ===
#include 'custom\GWA2_Compat.au3'

; === Custom Data Files (aliases for upstream IDs) ===
#include 'custom\Map_IDs.au3'
#include 'custom\Skill_IDs.au3'
#include 'custom\Skill_Types.au3'

; === Custom Extensions ===
#include 'custom\NPC_Coordinates.au3'
#include 'custom\GWA2_Crafting.au3'
#include 'custom\GWA2_Extensions.au3'

; === Custom Libraries ===
#include 'custom\GUI_Functions.au3'
#include 'custom\Utils-Maintenance.au3'
