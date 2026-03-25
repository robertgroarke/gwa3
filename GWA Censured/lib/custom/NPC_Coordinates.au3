#include-once

; =============================================================================
; NPC_Coordinates.au3
;
; Custom NPC coordinate data for GWA Censured. Contains coordinates for
; towns not in upstream BotsHub (Gadd's Encampment, Embark Beach traders,
; Xunlai chest locations) plus the full NPCCoordinatesInTown function
; which merges upstream and custom coordinates.
;
; Also contains GoToXunlaiChest() which is a custom utility function.
;
; Extracted 2026-03-25
; =============================================================================

;~ Move to Xunlai Chest and open it
Func GoToXunlaiChest($town = $ID_EYE_OF_THE_NORTH)
	TravelToOutpost($town)
	Info('Moving to Xunlai Chest')
	UseCitySpeedBoost()

	Local $NPCCoordinates = NPCCoordinatesInTown($town, 'Xunlai chest')
	MoveTo($NPCCoordinates[0], $NPCCoordinates[1])

	Local $chest = GetNearestNPCToCoords($NPCCoordinates[0], $NPCCoordinates[1])
	GoToNPC($chest)
	RandomSleep(500)
EndFunc

;~ Returns coordinates [X, Y] of an NPC in a given town
;~ Supports: Merchant, Basic material trader, Rare material trader,
;~           Consumables trader (Embark Beach), Xunlai chest
Func NPCCoordinatesInTown($town = $ID_EYE_OF_THE_NORTH, $type = 'Merchant', $name = '')
	Local $coordinates[2] = [-1, -1]
	Switch $type
		Case 'Merchant'
			Switch $town
				Case $ID_EMBARK_BEACH
					$coordinates[0] = 2233
					$coordinates[1] = -2009
				Case $ID_EYE_OF_THE_NORTH
					$coordinates[0] = -2700
					$coordinates[1] = 1075
				Case $ID_GADDS_CAMP
					$coordinates[0] = -8374
					$coordinates[1] = -22491
				Case Else
					Warn('For provided town coordinates of that NPC aren''t mapped yet')
			EndSwitch
		Case 'Basic material trader'
			Switch $town
				Case $ID_EMBARK_BEACH
					$coordinates[0] = 2933
					$coordinates[1] = -2236
				Case $ID_EYE_OF_THE_NORTH
					$coordinates[0] = -1850
					$coordinates[1] = 875
				Case $ID_GADDS_CAMP
					$coordinates[0] = -9097
					$coordinates[1] = -23353
				Case Else
					Warn('For provided town coordinates of that NPC aren''t mapped yet')
			EndSwitch
		Case 'Rare material trader'
			Switch $town
				Case $ID_EMBARK_BEACH
					$coordinates[0] = 2865
					$coordinates[1] = -2406
				Case $ID_EYE_OF_THE_NORTH
					$coordinates[0] = -2100
					$coordinates[1] = 1125
				Case $ID_GADDS_CAMP
					$coordinates[0] = -9136
					$coordinates[1] = -23153
				Case Else
					Warn('For provided town coordinates of that NPC aren''t mapped yet')
			EndSwitch
		Case 'Consumables trader'
			Switch $town
				Case $ID_EMBARK_BEACH
					Switch $name
						Case 'Edwin'
							$coordinates[0] = 3515
							$coordinates[1] = 369
						Case 'Kwat'
							$coordinates[0] = 3596
							$coordinates[1] = 107
						Case 'Alcus Nailbiter'
							$coordinates[0] = 3704
							$coordinates[1] = -163
						Case 'Eyja', ''
							$coordinates[0] = 3336
							$coordinates[1] = 627
						Case Else
							Warn('Unknown Consumables Trader name: ' & $name)
							$coordinates[0] = 3336
							$coordinates[1] = 627
					EndSwitch
				Case Else
					Warn('For provided town coordinates of that NPC aren''t mapped yet')
			EndSwitch
		Case 'Xunlai chest'
			Switch $town
				Case $ID_EMBARK_BEACH
					$coordinates[0] = 2283
					$coordinates[1] = -2134
				Case $ID_GADDS_CAMP
					$coordinates[0] = -10481
					$coordinates[1] = -22787
				Case Else
					Warn('For provided town coordinates of that NPC aren''t mapped yet')
			EndSwitch
		Case Else
			Warn('Wrong NPC type provided')
	EndSwitch
	Return $coordinates
EndFunc
