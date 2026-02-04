#include <ButtonConstants.au3>
#include <EditConstants.au3>
#include <GUIConstantsEx.au3>
#include <GuiStatusBar.au3>
#include <GuiEdit.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>
#include <ScrollBarsConstants.au3>
#include <Date.au3>
#include <String.au3>
#include <Array.au3>
#include <Misc.au3>

AutoItSetOption("GUIOnEventMode", 1)	; Opt("GUIOnEventMode", True)

Global Const $INI_PATH 		= @ScriptDir & "\Settings.ini"

; #Region Settings



;Fix const u cannot change
Global Const $AUTOIT_GUI_HEADER_SIZE 	= 26	;The thing where title is and minimize button etc. You can change it with tricks etc but not worth caring about it.
Global Const $AUTOIT_GUI_MENUBAR_SIZE 	= 20	;the thing just beneath the header. You can also change it with tricks but its really not worth caring about it.
Global Const $AUTOIT_GUI_EDITBOX_Y_FIX 	= 1		;You need to add +1 on Y-Pos to make it even with other controls...

;Main
Global Const $GUI_WIDTH				= 390
Global Const $GUI_HEIGHT			= 450 ;312
Global Const $GUI_FONTSIZE 			= 9			;standard fontsize
Global Const $GUI_BORDERSIZE 		= 8			;just the space needed to be on the left, right, top, bottom GUI
Global Const $GUI_CONTROL_SPACE 	= 4			;just the space needed to be on the left, right, top, bottom GUI
Global Const $GUI_STATUSBAR_WIDTH	= [135, 265, -1]	;110 Width for the 'Run Time: 00:00:00' and -1 to determine the second party has infinite space (end of GUI width)

;Buttons
Global Const $GUI_BUTTON_WIDTH 		= 140		;Make every button the same size (use multiplier for bigger/smaller buttons, so resizing will be easy
Global Const $GUI_BUTTON_HEIGHT 	= 25

;Labels
Global Const $GUI_LABEL_WIDTH 		= 120		;Make every button the same size (use multiplier for bigger/smaller buttons, so resizing will be easy
Global Const $GUI_LABEL_HEIGHT 		= 20

Global Const $GUI_CHECKBOX_WIDTH 	= 110
Global Const $GUI_CHECKBOX_HEIGHT 	= 16

#EndRegion

; #Region GUI Vars
;Main
Global $GUI = 0									;handle to the main window

;Statusbar
Global $GUI_hStatusBar 							= 0						;Buttom Statusbar for the time preview
Global $GUI_RunCounter 							= 0						;Run counter
Global $GUI_FailCounter 						= 0						;Fail counter

;Menu
Global $GUI_idMenuFile							= 0			;The Main Menu, this cannot handle any events, so we re creating sub menus below
Global $GUI_idMenuFile_idSettings 				= 0			;Settings
Global $GUI_idMenuFile_idModSettings			= 0			;Mod Settings
Global $GUI_idMenuFile_idOpenDir				= 0			;Open Dir
Global $GUI_idMenuFile_idExit					= 0			;Exit

;Buttons
Global $GUI_idButtonStart						= 0
Global $GUI_idButtonStop						= 0
Global $GUI_idButtonResume						= 0
Global $GUI_idButtonStart_Function				= ""	;Sub Function to call onStartPressed
Global $GUI_idButtonStop_Function				= ""	;Sub Function to call onStopPressed
Global $GUI_idButtonResume_Function				= ""	;Sub Function to call onResumePressed

;Group 1: Settings
Global $GUI_idConsole 							= 0
Global $GUI_GroupSettings 						= 0
Global $GUI_GroupSettings_CheckAddHeroes			= 0
Global $Rendering								= 1
Global $GUI_GroupSettings_CheckPurge			= 0
Global $GUI_GroupSettings_CheckHM				= 0
Global $GUI_GroupSettings_CheckConsets			= 0
Global $GUI_GroupSettings_CheckScrolls			= 0
Global $GUI_GroupSettings_CheckStones			= 0
Global $GUI_GroupSettings_CheckChests			= 0
Global $GUI_GroupSettings_CheckPickupGolds		= 0
Global $GUI_GroupSettings_CheckSalvage			= 0

;Group 2: General Statistics
Global $GUI_GroupGeneralStats = 0
Global $GUI_GroupGeneralStats_lblDeldrimor 		= 0
Global $GUI_GroupGeneralStats_lblAsura				= 0
Global $GUI_GroupGeneralStats_lblNorn				= 0
Global $GUI_GroupGeneralStats_lblVanguard 		= 0
Global $GUI_GroupGeneralStats_lblLockpicks 		= 0
Global $GUI_GroupGeneralStats_lblWipes 			= 0
Global $GUI_GroupGeneralStats_lblBestRunTime	= 0
Global $GUI_GroupGeneralStats_lblAvgRunTime = 0
Global $GUI_GroupGeneralStats_lblDeldrimorVal 	= 0
Global $GUI_GroupGeneralStats_lblAsuraVal 		= 0
Global $GUI_GroupGeneralStats_lblNornVal 		= 0
Global $GUI_GroupGeneralStats_lblVanguardVal 		= 0
Global $GUI_GroupGeneralStats_lblLockpicksVal 	= 0
Global $GUI_GroupGeneralStats_lblWipesVal		= 0
Global $GUI_GroupGeneralStats_lblBestRunTimeVal	= 0
Global $GUI_GroupGeneralStats_lblAvgRunTimeVal	= 0


Global $GUI_GroupDropStatistics = 0
Global $GUI_GroupDropStatistics_lblRareSkins 	= 0
Global $GUI_GroupDropStatistics_lblGolds 		= 0
Global $GUI_GroupDropStatistics_lblLockpicks 	= 0
Global $GUI_GroupDropStatistics_lblChests 		= 0
Global $GUI_GroupDropStatistics_lblDyes 		= 0
Global $GUI_GroupDropStatistics_lblTomes 		= 0
Global $GUI_GroupDropStatistics_lblRareSkinsVal	= 0
Global $GUI_GroupDropStatistics_lblGoldsVal 	= 0
Global $GUI_GroupDropStatistics_lblLockpicksVal	= 0
Global $GUI_GroupDropStatistics_lblChestsVal 	= 0
Global $GUI_GroupDropStatistics_lblDyesVal 		= 0
Global $GUI_GroupDropStatistics_lblTomesVal	= 0
#EndRegion GUI Vars

; #Region String Extensions
Func String_GetTimeStamp($ShowSeconds)
	Local $TimeStamp = "[" & @HOUR & ":" & @MIN
	If $ShowSeconds Then $TimeStamp &= ":" & @SEC
	$TimeStamp &= "]"
	Return $TimeStamp
EndFunc
#EndRegion

; #Region GUI Helpfunctions
;Gets the information of a control related to the main GUI
;$SizeBuffer[0] = X Position
;$SizeBuffer[1] = Y Position
;$SizeBuffer[2] = Width
;$SizeBuffer[3] = Height
Func GUI_GetCtrlInfo($ControlID, $GuiHasMenu = ($GUI_idMenuFile <> 0), $GUI = $GUI)
	If $GUI = 0 Then Return SetError(1, 0)

	Local $SizeBuffer 	= WinGetPos(GUICtrlGetHandle($ControlID))
	Local $SizeGUI 		= WinGetPos($GUI)

	;I have to subract the GUI X Coords else it would get the total coords of the entire screen
	$SizeBuffer[0] -= $SizeGUI[0]
	$SizeBuffer[1] -= $SizeGUI[1]

	If GUI_HasStyle($WS_CAPTION) Then $SizeBuffer[1] -= $AUTOIT_GUI_HEADER_SIZE
	If $GuiHasMenu Then	$SizeBuffer[1] -= $AUTOIT_GUI_MENUBAR_SIZE

	Return $SizeBuffer
EndFunc

Func GUI_HasStyle($Style, $GUI = $GUI)
	Return BitAND(GUIGetStyle($GUI)[0], $Style) = $Style
EndFunc

Func GUI_IsChecked($ControlID)
	Return BitAND(GUICtrlRead($ControlID), $GUI_CHECKED) = $GUI_CHECKED
EndFunc
#EndRegion GUI Helpfunctions

; #Region Creation
#cs
; #Region Set Uskin
_Uskin_LoadDLL()
_USkin_Init(@ScriptDir & "\Skins\Royale.msstyles") ; <-- Put here your skin...
OnAutoItExitRegister(_USkin_Exit)
#EndRegion Set Uskin
#ce

Func GUI_Create()
	;Locals
	Local $temp1[4]
	Local $temp2[4]

	local $tempCtrlTop = 0
	Local Static $Title = $BOTNAME & " v" & $VERSION & " - By " & $AUTHORS[0]

	;Main
	$GUI = GUICreate($Title, $GUI_WIDTH, $GUI_HEIGHT)
	GUISetOnEvent($GUI_EVENT_CLOSE, "GUI_onExit")
	GUISetFont($GUI_FONTSIZE)
	GUISetBkColor(0xAAAAAA)

	;Menu
	$GUI_idMenuFile = GUICtrlCreateMenu("File")
	$GUI_idMenuFile_idSettings 	= GUICtrlCreateMenuItem("Settings", $GUI_idMenuFile)
	$GUI_idMenuFile_idModSettings 	= GUICtrlCreateMenuItem("Mod Settings", $GUI_idMenuFile)
	$GUI_idMenuFile_idOpenDir 	= GUICtrlCreateMenuItem("Open Dir", $GUI_idMenuFile)
	$GUI_idMenuFile_idExit 		= GUICtrlCreateMenuItem("Exit"    , $GUI_idMenuFile)
	GUICtrlSetOnEvent($GUI_idMenuFile_idSettings, "GUI_MenuCallback")
	GUICtrlSetOnEvent($GUI_idMenuFile_idModSettings, "GUI_MenuCallback")
	GUICtrlSetOnEvent($GUI_idMenuFile_idOpenDir, "GUI_MenuCallback")
	GUICtrlSetOnEvent($GUI_idMenuFile_idExit, "GUI_MenuCallback")

	; #Region Gui Grid Row 1

	;Buttons

	$GUI_idButtonStart 	= GUICtrlCreateButton("Start" , $GUI_BORDERSIZE, $GUI_BORDERSIZE, $GUI_BUTTON_WIDTH, $GUI_BUTTON_HEIGHT, -1, $WS_EX_LAYOUTRTL)
	$GUI_idButtonStop 	= GUICtrlCreateButton("Stop"  , $GUI_BORDERSIZE, $GUI_BORDERSIZE, $GUI_BUTTON_WIDTH, $GUI_BUTTON_HEIGHT, -1, $WS_EX_LAYOUTRTL)
	$GUI_idButtonResume 	= GUICtrlCreateButton("Resume", $GUI_BORDERSIZE, $GUI_BORDERSIZE, $GUI_BUTTON_WIDTH, $GUI_BUTTON_HEIGHT, -1, $WS_EX_LAYOUTRTL)
	GUICtrlSetOnEvent($GUI_idButtonStart, "GUI_ButtonCallback")
	GUICtrlSetOnEvent($GUI_idButtonStop, "GUI_ButtonCallback")
	GUICtrlSetOnEvent($GUI_idButtonResume, "GUI_ButtonCallback")
	GUI_HideButton($GUI_idButtonStop  , True)
	GUI_HideButton($GUI_idButtonResume, True)


	#EndRegion Gui Grid Row 1

	; #Region Gui Grid Row 2
	;Group 1: Fast Settings
	$tempCtrlTop = $GUI_CHECKBOX_HEIGHT + $GUI_CONTROL_SPACE

	$temp1 = GUI_GetCtrlInfo($GUI_idButtonStart)
	$GUI_GroupSettings = GUICtrlCreateGroup("Settings", $GUI_BORDERSIZE		, $temp1[1] + $temp1[3] + $GUI_CONTROL_SPACE, $GUI_LABEL_WIDTH + $GUI_BORDERSIZE * 2,  20 + $tempCtrlTop * 9, -1, $WS_EX_TRANSPARENT)
	$GUI_GroupSettings_CheckAddHeroes  	= GUICtrlCreateCheckbox("Add Heroes"	, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 0, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
;~ 	$GUI_GroupSettings_CheckPurge   	= GUICtrlCreateCheckbox("Purge"  			, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 1, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
;~ 	$GUI_GroupSettings_CheckHM			= GUICtrlCreateCheckbox("Hard Mode"			, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 2, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
	$GUI_GroupSettings_CheckConsets		= GUICtrlCreateCheckbox("Consets"			, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 3, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
;~ 	$GUI_GroupSettings_CheckScrolls 	= GUICtrlCreateCheckbox("Scrolls"			, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 4, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
	$GUI_GroupSettings_CheckStones  	= GUICtrlCreateCheckbox("Stones"		  	, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 5, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
	$GUI_GroupSettings_CheckChests  	= GUICtrlCreateCheckbox("Open Chests"		, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 6, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
	$GUI_GroupSettings_CheckPickUpGolds	= GUICtrlCreateCheckbox("Pick Up Golds"	, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 7, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
	$GUI_GroupSettings_CheckSalvage    	= GUICtrlCreateCheckbox("Auto-Salvage"	  		, $GUI_BORDERSIZE * 2, $temp1[1] + $temp1[3] + 20 + $tempCtrlTop * 8, $GUI_CHECKBOX_WIDTH, $GUI_CHECKBOX_HEIGHT)
	GUICtrlCreateGroup("", -99, -99, 1, 1)	;Closes a Group

	;GUICtrlSetOnEvent($GUI_GroupSettings_CheckRender, "ToggleRendering")
	GUICtrlSetOnEvent($GUI_GroupSettings_CheckPurge, "Purgehook")
	GUICtrlSetOnEvent($GUI_GroupSettings_CheckSalvage, "IdentifyBags")


	$temp2 = GUI_GetCtrlInfo($GUI_GroupSettings)

	$GUI_idConsole = GUICtrlCreateEdit("", _
		$GUI_BORDERSIZE + $GUI_CONTROL_SPACE + ($temp1[2] > $temp2[2] ? $temp1[2] : $temp2[2]), _
		$temp1[1] + $AUTOIT_GUI_EDITBOX_Y_FIX, _
		$GUI_WIDTH - $GUI_BORDERSIZE * 2 - $GUI_CONTROL_SPACE - ($temp1[2] > $temp2[2] ? $temp1[2] : $temp2[2]), _
		$temp1[3] + $temp2[3] + $AUTOIT_GUI_EDITBOX_Y_FIX, BitOR($ES_MULTILINE, $ES_READONLY, $ES_AUTOVSCROLL, $WS_VSCROLL))
	GUICtrlSetBkColor($GUI_idConsole, 0x000000)
	GUICtrlSetColor($GUI_idConsole, 0xFFFFFF)
	#EndRegion Gui GridRow 2

	; #Region Gui Grid Row 3
	$tempCtrlTop = $GUI_LABEL_HEIGHT

	;Group 2: General Statistics
	$GUI_GroupGeneralStats = GUICtrlCreateGroup("General Statistics", $GUI_BORDERSIZE, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE, $GUI_LABEL_WIDTH * 1.45 + $GUI_BORDERSIZE * 2, 20 + $tempCtrlTop * 8 + $GUI_CONTROL_SPACE)
    GUICtrlSetFont($GUI_GroupGeneralStats, 9, 800, 0, "Arial")

	$GUI_GroupGeneralStats_lblDeldrimor 	= GUICtrlCreateLabel("Deldrimor Pts:"		, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 0, $GUI_LABEL_WIDTH, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblAsura			= GUICtrlCreateLabel("Asura Pts:"	, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 1, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblNorn			= GUICtrlCreateLabel("Norn Pts:"	, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 2, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblVanguard			= GUICtrlCreateLabel("Vanguard Pts:"	, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 3, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblLockpicks		= GUICtrlCreateLabel("Lockpicks:"		, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 4, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblWipes	 		= GUICtrlCreateLabel("Wipes:"			, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 5, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblBestRunTime	= GUICtrlCreateLabel("Best Run Time:"	, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 6, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)
	$GUI_GroupGeneralStats_lblAvgRunTime	= GUICtrlCreateLabel("Avg Run Time:"	, $GUI_BORDERSIZE * 2, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 7, $GUI_LABEL_WIDTH * 0.75, $GUI_LABEL_HEIGHT)

	$GUI_GroupGeneralStats_lblDeldrimorVal 	= GUICtrlCreateLabel("0", 			$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 0, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblAsuraVal 		= GUICtrlCreateLabel("0", 			$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 1, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblNornVal 		= GUICtrlCreateLabel("0", 			$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 2, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblVanguardVal 		= GUICtrlCreateLabel("0", 			$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 3, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblLockpicksVal 	= GUICtrlCreateLabel("0", 			$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 4, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblWipesVal 		= GUICtrlCreateLabel("0", 			$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 5, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblBestRunTimeVal= GUICtrlCreateLabel("00:00:00", 	$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 6, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupGeneralStats_lblAvgRunTimeVal = GUICtrlCreateLabel("00:00:00", 	$GUI_BORDERSIZE * 2 + $GUI_LABEL_WIDTH * 0.75, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE + 20 + $tempCtrlTop * 7, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	GUICtrlCreateGroup("", -99, -99, 1, 1)

	;Group 3: Drop Statistics
	$temp1 = GUI_GetCtrlInfo($GUI_GroupGeneralStats)
	$GUI_GroupDropStatistics = GUICtrlCreateGroup("Drop Statistics", $temp1[0] + $temp1[2] + $GUI_CONTROL_SPACE, $temp2[1] + $temp2[3] + $GUI_CONTROL_SPACE, $GUI_LABEL_WIDTH * 1.45 + $GUI_BORDERSIZE * 2, 20 + $tempCtrlTop * 8 + $GUI_CONTROL_SPACE)
	GUICtrlSetFont($GUI_GroupDropStatistics, 9, 800, 0, "Arial")
	$temp1 = GUI_GetCtrlInfo($GUI_GroupDropStatistics)
	$GUI_GroupDropStatistics_lblRareSkins 		= GUICtrlCreateLabel("Rare Skin:"		, $temp1[0] + $GUI_CONTROL_SPACE, $temp1[1] + 20 + $tempCtrlTop * 0, $GUI_LABEL_WIDTH , $GUI_LABEL_HEIGHT)
	$GUI_GroupDropStatistics_lblGolds 			= GUICtrlCreateLabel("Gold Items:"		, $temp1[0] + $GUI_CONTROL_SPACE, $temp1[1] + 20 + $tempCtrlTop * 1, $GUI_LABEL_WIDTH , $GUI_LABEL_HEIGHT)
	$GUI_GroupDropStatistics_lblLockpicks 		= GUICtrlCreateLabel("Lockpicks:"		, $temp1[0] + $GUI_CONTROL_SPACE, $temp1[1] + 20 + $tempCtrlTop * 2, $GUI_LABEL_WIDTH , $GUI_LABEL_HEIGHT)
	$GUI_GroupDropStatistics_lblChests 			= GUICtrlCreateLabel("Chests Opened:"	, $temp1[0] + $GUI_CONTROL_SPACE, $temp1[1] + 20 + $tempCtrlTop * 3, $GUI_LABEL_WIDTH , $GUI_LABEL_HEIGHT)
	$GUI_GroupDropStatistics_lblDyes 			= GUICtrlCreateLabel("Black Dye:"		, $temp1[0] + $GUI_CONTROL_SPACE, $temp1[1] + 20 + $tempCtrlTop * 4, $GUI_LABEL_WIDTH , $GUI_LABEL_HEIGHT)
	$GUI_GroupDropStatistics_lblTomes 			= GUICtrlCreateLabel("Tomes:"			, $temp1[0] + $GUI_CONTROL_SPACE, $temp1[1] + 20 + $tempCtrlTop * 5, $GUI_LABEL_WIDTH , $GUI_LABEL_HEIGHT)
	$GUI_GroupDropStatistics_lblRareSkinsVal 	= GUICtrlCreateLabel("0", $temp1[0] + $GUI_CONTROL_SPACE + $GUI_LABEL_WIDTH * 0.75, $temp1[1] + 20 + $tempCtrlTop * 0, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupDropStatistics_lblGoldsVal 		= GUICtrlCreateLabel("0", $temp1[0] + $GUI_CONTROL_SPACE + $GUI_LABEL_WIDTH * 0.75, $temp1[1] + 20 + $tempCtrlTop * 1, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupDropStatistics_lblLockpicksVal	= GUICtrlCreateLabel("0", $temp1[0] + $GUI_CONTROL_SPACE + $GUI_LABEL_WIDTH * 0.75, $temp1[1] + 20 + $tempCtrlTop * 2, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupDropStatistics_lblChestsVal 		= GUICtrlCreateLabel("0", $temp1[0] + $GUI_CONTROL_SPACE + $GUI_LABEL_WIDTH * 0.75, $temp1[1] + 20 + $tempCtrlTop * 3, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupDropStatistics_lblDyesVal 		= GUICtrlCreateLabel("0", $temp1[0] + $GUI_CONTROL_SPACE + $GUI_LABEL_WIDTH * 0.75, $temp1[1] + 20 + $tempCtrlTop * 4, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	$GUI_GroupDropStatistics_lblTomesVal		= GUICtrlCreateLabel("0", $temp1[0] + $GUI_CONTROL_SPACE + $GUI_LABEL_WIDTH * 0.75, $temp1[1] + 20 + $tempCtrlTop * 5, $GUI_LABEL_WIDTH * 0.5, $GUI_LABEL_HEIGHT, $SS_RIGHT)
	GUICtrlCreateGroup("", -99, -99, 1, 1)
	#EndRegion Gui GridRow 2

	$temp1 = GUI_GetCtrlInfo($GUI_GroupDropStatistics, False)
	$temp2 = WinGetPos($GUI)
	WinMove($GUI, "", $temp2[0], $temp2[1], $temp2[2], $temp1[1] + $temp1[3] + $GUI_BORDERSIZE + $AUTOIT_GUI_HEADER_SIZE + 25)

    ;StatusBar
    $GUI_hStatusBar = _GUICtrlStatusBar_Create($GUI)
    _GUICtrlStatusBar_SetParts($GUI_hStatusBar, $GUI_STATUSBAR_WIDTH)
    _GUICtrlStatusBar_SetText($GUI_hStatusBar, "Runs: 0 (0% failed)", 0)
    _GUICtrlStatusBar_SetText($GUI_hStatusBar, "Run Time: 00:00:00", 1)
    _GUICtrlStatusBar_SetText($GUI_hStatusBar, "Total Time: 00:00:00", 2)


	; #Region INI
	GUI_IniCreate()
	If Not GUI_IniIsComplete() Then
		FileDelete($INI_PATH)
		Out("INI file incompleted. Recreating it.")
		GUI_IniCreate()
	EndIf

;~ 	GUICtrlSetState($GUI_GroupSettings_CheckHM, (IniRead($INI_PATH, "Settings", "Hard Mode", True)) ? $GUI_CHECKED : 0)
;~ 	GUICtrlSetState($GUI_GroupSettings_CheckConsets, (IniRead($INI_PATH, "Settings", "Consets", True)) ? $GUI_UNCHECKED : 0)
;~ 	GUICtrlSetState($GUI_GroupSettings_CheckScrolls, (IniRead($INI_PATH, "Settings", "Scrolls", True)) ? $GUI_CHECKED : 0)
;~ 	GUICtrlSetState($GUI_GroupSettings_CheckStones, (IniRead($INI_PATH, "Settings", "Stones", True)) ? $GUI_CHECKED : 0)
;~ 	GUICtrlSetState($GUI_GroupSettings_CheckChests, (IniRead($INI_PATH, "Settings", "OpenChests", True)) ? $GUI_UNCHECKED : 0)
 	GUICtrlSetState($GUI_GroupSettings_CheckAddHeroes, (IniRead($INI_PATH, "Settings", "AddHeroes", True)) ? $GUI_CHECKED : 0)
	GUICtrlSetState($GUI_GroupSettings_CheckPickUpGolds, (IniRead($INI_PATH, "Settings", "PickUpGolds", True)) ? $GUI_CHECKED : 0)
 	GUICtrlSetState($GUI_GroupSettings_CheckSalvage, (IniRead($INI_PATH, "Settings", "AutoSalvage", True)) ? $GUI_CHECKED : 0)
	#EndRegion INI

	GUISetState(@SW_SHOW)
EndFunc
#EndRegion Creation

; #Region Events
Func GUI_onExit()
	Exit
EndFunc

Func GUI_ButtonCallback()
	If Not @GUI_CtrlHandle = $GUI Then Return ;If u run multiple GUIs, sort out the GUI u dont need, to reduce unnecessary cpu usage.
	Local $idButton = @GUI_CtrlId
	Switch($idButton)
		Case $GUI_idButtonStart
			GUI_HideButton($GUI_idButtonStart, True)
			GUI_HideButton($GUI_idButtonStop, False)
			GUI_HideButton($GUI_idButtonResume, True)
			Call($GUI_idButtonStart_Function)
		Case $GUI_idButtonStop
			GUI_HideButton($GUI_idButtonStop, True)
			GUI_HideButton($GUI_idButtonResume, False)
			Call($GUI_idButtonStop_Function)
		Case $GUI_idButtonResume
			GUI_HideButton($GUI_idButtonStop, False)
			GUI_HideButton($GUI_idButtonResume, True)
			Call($GUI_idButtonResume_Function)
	EndSwitch
EndFunc

Func GUI_MenuCallback() ;Checking here for the GUI does not work. Since every GUI can only have one Menu.
	Local $idMenuItem = @GUI_CtrlId
	Switch($idMenuItem)
		Case $GUI_idMenuFile_idSettings
			Run("notepad.exe " & $INI_PATH)
		Case $GUI_idMenuFile_idModSettings
			ModSelection()
	    Case $GUI_idMenuFile_idOpenDir
			Run("Explorer.exe " & @ScriptDir)
		Case $GUI_idMenuFile_idExit
			Exit
	EndSwitch
EndFunc
#EndRegion

; #Region Getters
;Group 1: Checkbox
Func GUI_IsAddHeroesChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckAddHeroes)
EndFunc

Func GUI_IsPurgeChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckPurge)
EndFunc

Func GUI_IsHMChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckHM)
EndFunc

Func GUI_IsConsetsChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckConsets)
EndFunc

Func GUI_IsScrollsChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckScrolls)
EndFunc

Func GUI_IsStonesChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckStones)
EndFunc

Func GUI_IsChestChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckChests)
EndFunc

Func GUI_IsPickUpGoldsChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckPickupGolds)
EndFunc

Func GUI_IsSalvageChecked()
	Return GUI_IsChecked($GUI_GroupSettings_CheckSalvage)
EndFunc

;Group 2: General
Func GUI_GetDeldrimor()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblDeldrimorVal)
EndFunc

Func GUI_GetAsura()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblAsuraVal)
EndFunc

Func GUI_GetNorn()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblNornVal)
EndFunc

Func GUI_GetVanguard()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblVanguardVal)
EndFunc

Func GUI_GetLockpicks()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblLockpicksVal)
EndFunc

Func GUI_GetWipes()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblWipesVal)
EndFunc

Func GUI_GetBestRunTime()
	Return GUICtrlRead($GUI_GroupGeneralStats_lblBestRunTimeVal)
EndFunc

;Group 3: Drops
Func GUI_GetRareSkins()
	Return GUICtrlRead($GUI_GroupDropStatistics_lblRareSkinsVal)
EndFunc

Func GUI_GetGolds()
	Return GUICtrlRead($GUI_GroupDropStatistics_lblGoldsVal)
EndFunc

Func GUI_GetDroppedLockpicks()
	Return GUICtrlRead($GUI_GroupDropStatistics_lblLockpicksVal)
EndFunc

Func GUI_GetChestsOpened()
	Return GUICtrlRead($GUI_GroupDropStatistics_lblChestsVal)
EndFunc

Func GUI_GetBlackDyes()
	Return GUICtrlRead($GUI_GroupDropStatistics_lblDyesVal)
EndFunc

Func GUI_GetTomes()
	Return GUICtrlRead($GUI_GroupDropStatistics_lblTomesVal)
EndFunc


Func GUI_GetConsoleText()
	Global $GUI_idConsole
	Return GUICtrlRead($GUI_idConsole)
EndFunc

#EndRegion Getters

; #Region Commands
Func GUI_SetOnStartFunc($NewFunc = "")
	$GUI_idButtonStart_Function = $NewFunc
EndFunc

Func GUI_SetOnStopFunc($NewFunc = "")
	$GUI_idButtonStop_Function = $NewFunc
EndFunc

Func GUI_SetOnResumeFunc($NewFunc = "")
	$GUI_idButtonResume_Function = $NewFunc
EndFunc

Func GUI_HideButton($ButtonID, $Hide)
	GUICtrlSetState($ButtonID, $Hide ? $GUI_HIDE : $GUI_SHOW)		;Hiding
	GUICtrlSetState($ButtonID, $Hide ? $GUI_DISABLE : $GUI_ENABLE)	;Makes it unable to press it, or let it be pressed
EndFunc

;Group 2: General
Func GUI_SetDeldrimor($Value)
	Return GUICtrlSetData($GUI_GroupGeneralStats_lblDeldrimorVal, $Value)
EndFunc

Func GUI_SetAsura($Value)
	Return GUICtrlSetData($GUI_GroupGeneralStats_lblAsuraVal, $Value)
EndFunc

Func GUI_SetNorn($Value)
	Return GUICtrlSetData($GUI_GroupGeneralStats_lblNornVal, $Value)
EndFunc

Func GUI_SetVanguard($Value)
	Return GUICtrlSetData($GUI_GroupGeneralStats_lblVanguardVal, $Value)
EndFunc

Func GUI_SetLockpicks($Value)
  	Return GUICtrlSetData($GUI_GroupGeneralStats_lblLockpicksVal, $Value)
EndFunc

Func GUI_SetWipes($Value)
	Return GUICtrlSetData($GUI_GroupGeneralStats_lblWipesVal, $Value)
 EndFunc

;Group 3: Drops
Func GUI_SetRareSkins($Value)
	Return GUICtrlSetData($GUI_GroupDropStatistics_lblRareSkinsVal, $Value)
 EndFunc

Func GUI_SetGolds($Value)
	Return GUICtrlSetData($GUI_GroupDropStatistics_lblGoldsVal, $Value)
EndFunc

Func GUI_SetDroppedLockpicks($Value)
	Return GUICtrlSetData($GUI_GroupDropStatistics_lblLockpicksVal, $Value)
EndFunc

Func GUI_SetChestsOpened($Value)
	Return GUICtrlSetData($GUI_GroupDropStatistics_lblChestsVal, $Value)
EndFunc

Func GUI_SetBlackDyes($Value)
	Return GUICtrlSetData($GUI_GroupDropStatistics_lblDyesVal, $Value)
EndFunc

Func GUI_SetTomes($Value)
	Return GUICtrlSetData($GUI_GroupDropStatistics_lblTomesVal, $Value)
EndFunc


;Console
Func GUI_SetRunCounter($Value = -1)
    If $Value = -1 Then
        $GUI_RunCounter += 1
    Else
        $GUI_RunCounter = $Value
    EndIf
    _GUICtrlStatusBar_SetText($GUI_hStatusBar, "Runs: " & $GUI_RunCounter & " (" & Round($GUI_FailCounter/$GUI_RunCounter * 100 ,2) & "% failed)", 0)
EndFunc

Func GUI_SetFailCounter($Value = -1)
    If $Value = -1 Then
        $GUI_FailCounter += 1
    Else
        $GUI_FailCounter += 1 = $Value
    EndIf
    _GUICtrlStatusBar_SetText($GUI_hStatusBar, "Runs: " & $GUI_RunCounter & " (" & Round($GUI_FailCounter/$GUI_RunCounter * 100 ,2) & "% failed)", 0)
EndFunc

Func GUI_Print($Text, $TimeStamp = True, $ShowSeconds = False)
	If $TimeStamp Then $Text = String_GetTimeStamp($ShowSeconds) & " " & $Text
	GUICtrlSetData($GUI_idConsole, GUI_GetConsoleText() & $Text)
	_GUICtrlEdit_Scroll($GUI_idConsole, $SB_SCROLLCARET)
EndFunc

Func Out($Text, $TimeStamp = True, $ShowSeconds = False)
	Local $Out = ""
	If GUI_GetConsoleText() <> "" Then $Out = @CRLF
	If $TimeStamp Then $Out &= String_GetTimeStamp($ShowSeconds) & " "
	GUICtrlSetData($GUI_idConsole, GUI_GetConsoleText() & $Out & $Text & "")
	_GUICtrlEdit_Scroll($GUI_idConsole, $SB_SCROLLCARET)
EndFunc

Func GUI_SetRunTime($iTicks)
	Local $iHours, $iMins, $iSecs
	Local $TimeStamp = ""
	_TicksToTime($iTicks, $iHours, $iMins, $iSecs)
	If $iHours < 10 Then $TimeStamp = "0"
	$TimeStamp &= $iHours & ":"
	If $iMins < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iMins & ":"
	If $iSecs < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iSecs
	_GUICtrlStatusBar_SetText($GUI_hStatusBar, "Run Time: " & $TimeStamp, 1)
EndFunc

Func GUI_SetBestRunTime($iTicks)
	Local $iHours, $iMins, $iSecs
	Local $TimeStamp = ""
	_TicksToTime($iTicks, $iHours, $iMins, $iSecs)
	If $iHours < 10 Then $TimeStamp = "0"
	$TimeStamp &= $iHours & ":"
	If $iMins < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iMins & ":"
	If $iSecs < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iSecs
	GUICtrlSetData($GUI_GroupGeneralStats_lblBestRunTimeVal, $TimeStamp)
EndFunc

Func GUI_SetTotalTime($iTicks)
	Local $iHours, $iMins, $iSecs
	Local $TimeStamp = ""
	_TicksToTime($iTicks, $iHours, $iMins, $iSecs)
	If $iHours < 10 Then $TimeStamp = "0"
	$TimeStamp &= $iHours & ":"
	If $iMins < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iMins & ":"
	If $iSecs < 10 Then $TimeStamp &= "0"
	$TimeStamp &= $iSecs
	_GUICtrlStatusBar_SetText($GUI_hStatusBar, "Total Time: " & $TimeStamp, 2)
 EndFunc

Func GUI_SetAvgRunTime($TimeStamp)
 	GUICtrlSetData($GUI_GroupGeneralStats_lblAvgRunTimeVal, $TimeStamp)
 EndFunc

#EndRegion Commands

; #Region Ini
Global Const $ValuesToBeSaved = [ _
		[7, ""], _ ;Amount of elements in the array
		["AddHeroes", True], _
		["Consets", True], _
		["Stones", True], _
		["OpenChests", True], _
		["PickUpGolds", True], _
		["AutoSalvage", True] _
]

Func GUI_IniCreate()
	If FileExists($INI_PATH) Then Return False
	Out("Creating INI File.")
	Return IniWriteSection($INI_PATH, "Settings", $ValuesToBeSaved)
EndFunc

Func GUI_IniHasKey($Sections, $Key)
	Local $IniResult = IniReadSection($INI_PATH, "Settings")

	For $i = 1 To $ValuesToBeSaved[0][0]
		If $IniResult[$i][0] = $Key Then Return True
	Next

	Return False
EndFunc

;This func will loop through all the keys in Settings and checks if every value is in the same row
;and has the same value. If one is missing or out of order
Func GUI_IniIsComplete()
	If Not FileExists($INI_PATH) Then Return False

	Local $IniResult = IniReadSection($INI_PATH, "Settings")

	If $IniResult[0][0] <> $ValuesToBeSaved[0][0] Then Return False

	For $key = 1 To $ValuesToBeSaved[0][0]
		If Not GUI_IniHasKey("Settings", $ValuesToBeSaved[$key][0]) Then Return False
	Next

	Return True
EndFunc



Opt("GUIDataSeparatorChar", "|") ; used for arraay to ini and ini to array functions
Global $defaultSeparatorString = "<%Separator%>" 			; used for arraay to ini and ini to array functions


;~ 				Autor logicdoor
;~
;~				 $array_mods_[x][0] = Name
;~				 $array_mods_[x][1] = Modstring 1
;~ 				 $array_mods_[x][2] = Modstring 2

;~			     $array_mods_[x][3-13] = Staff (26), Wand (22), Offhand (12),  Shield (24), Axe (2), Bow (5), Hammer (15), Daggers (32), Scythe (35), Spear (36), Sword (27)
; 										stores for which weapon types the mod should be salvaged

Global $ColLabels[12] = ["Staff", "Wand",  "Focus", "Shield", "Axe", "Bow", "Hammer", "Daggers", "Scythe", "Spear", "Sword", "Runes & Insignas"]
Global $array_weaponmods_ini [133][15]
Global $array_weaponmods [133][15] = [ _
		 [ "HCT20 [Inscription]",  							2, "22500140828","" ,		 1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "HCT20 [Staff head]", 							0, "02500140828","" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _ 	; 004302500140828 (adept staff head)
		 [ "HCT20 [Focus Core]",			 				1, "02500140828","" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _ 	; F04302500140828 (focus core)
		 [ "HCT10 [Inscription]",							2, "000A0822",   "" ,		 0,  0,  0, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "HCT10 [Staff Head]",							0, "000A0822",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "HSR20 [Inscription]",							2, "00142828",   "" ,		-1, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "HSR20 [Wand Wrapping]",							1, "00142828",   "" ,		-1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "HSR10 [Inscription]",							2, "000AA823",   "" ,		-1, -1,  0, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "HSR10 [Wand Wrapping]",							1, "000AA823",   "" ,		-1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+1 Attr. (Chance 20%) [Inscription]",			2, "00143828",   "" ,	  	-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+1 Attr. (Chance 20%) [Staff Wrapping]",		1, "00143828",   "" ,	  	 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Highly salvageable",							2, "1E000826",   "" ,	  	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Improved sale value",							2, "3200F805",   "" ,	  	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Energy +5 [Inscription]" ,						2, "0500D822",   "" ,	  	-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Energy +5 [Staff Head]" ,						0, "0500D822",   "" ,	  	 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Energy +5 (HP>50%)",							2, "05320823",   "" ,	  	 0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Energy +5 (while Enchanted)",					2, "0500F822",   "" ,	  	 0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Energy +7 (HP<50%)",							2, "07321823",   "" ,	  	 0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Energy +7 (while hexed)",						2, "07002823",   "" ,	  	 0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Energy +15 (-1 energy regen)",					2, "0F00D822",   "0100C820", 0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Damage -2 (while Enchanted)",					2, "02008820",   "" ,  		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Damage -2 (while in a Stance)",					2, "0200A820",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Damage -3 (while Hexed)",						2, "03009820",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Damage -5 (Chance: 20%)",						2, "05147820",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Bleeding", 								2, "00005828",	 "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Blind",									2, "00015828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Crippled",									2, "00035828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Dazed",									2, "00075828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Deep Wound",								2, "00045828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Disease (Inscribable)",					2, "00055828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Disease", 									2, "E3017824",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Poison",									2, "00065828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "-20% Weakness",									2, "00085828",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Damage +15% (-1 energy regen)",					2, "0F003822",   "0100C820",-1, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _ ; cannot be salvaged
		 [ "Damage +15% (-1 HP regen)",						2, "0F003822",   "0100E820",-1, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _ ; cannot be salvaged
		 [ "Damage +15% (HP> 50%)",							2, "0F327822",   "" , 		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage +15% (while Enchanted)",					2, "0F006822",   "" , 		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage +15% (while in a Stance)",				2, "0F00A822",   "" , 		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage +15% (vs Hexed Foes)",					2, "0F005822",   "" , 		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage +15% (-10 AL while attacking)",			2, "0A001820",   "" , 		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage +15% (Energy -5)",						2, "0500B820",   "" , 		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage 20% (HP<50%)",							2, "14328822",   "" ,		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Damage 20% (while Hexed)",						2, "14009822",   "" ,		 0,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Ebon",											0, "000BB824",   "" ,		-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Fiery",											0, "0005B824",   "" ,		-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Icy",											0, "0003B824",   "" ,		-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Shocking",										0, "0004B824",   "" ,		-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Barbed",										0, "DE016824",   "" ,		-1, -1, -1, -1,  0,  0, -1,  0,  0,  0,  0], _
		 [ "Crippling",										0, "E1016824",   "" ,		-1, -1, -1, -1,  0,  0, -1,  0,  0,  0,  0], _
		 [ "Cruel",											0, "E2016824",   "" ,		-1, -1, -1, -1,  0, -1,  0,  0,  0,  0,  0], _
		 [ "Furious",										0, "0A00B823",   "" ,		-1, -1, -1, -1,  0, -1,  0,  0,  0,  0,  0], _
		 [ "Heavy",											0, "E601824",    "" ,		-1, -1, -1, -1,  0, -1,  0, -1,  0,  0, -1], _
		 [ "Poisonous",										0, "E4016824",   "" ,		-1, -1, -1, -1,  0,  0, -1,  0,  0,  0,  0], _
		 [ "Silencing",										0, "E5016824",   "" ,		-1, -1, -1, -1, -1,  0,  0,  0, -1,  0, -1], _
		 [ "Sundering",										0, "1414F823",   "" ,		-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Vampiric (+3)",									0, "00032825",   "" ,		-1, -1, -1, -1,  0, -1, -1,  0, -1,  0,  0], _
		 [ "Vampiric (+5)",									0, "00052825",   "" ,		-1, -1, -1, -1, -1,  0,  0, -1,  0, -1, -1], _
		 [ "Zealous",										0, "01001825",   "" ,		-1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "+ 20% (vs Charr)",								1, "00018080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Demons)",								1, "00088080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Dragons",								1, "00098080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Dwarves)",							1, "00068080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Giants)",								1, "00058080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Ogres)",								1, "000A8080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Plants)",								1, "00038080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Skeletons)",							1, "00048080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Tengu)",								1, "00078080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Trolls)",								1, "00028080",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _
		 [ "+ 20% (vs Undead)",								1, "001448A2",   "" ,		 0, -1, -1, -1,  0,  0,  0, -1, -1, -1,  0], _		; use 00008080 for +19%/+20%
		 [ "+30 HP",										1, "001E4823",   "" ,		-1, -1,  0,  1,  0,  0,  0,  0,  0,  0,  0], _  	; weapons and shields
		 [ "+30 HP (staff wrapping)",						1, "9013025001E4823", "" ,	 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _  	; DC000824B9013025001E4823 (staff wrapping)
		 [ "+30 HP (staff head)",							0, "A013025001E4823", "" , 	 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _  	; 9D0008243A013025001E4823 (hale staff head)
		 [ "+45 HP while Enchanted",						1, "002D6823",   "" ,		 0, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+45 HP while in a Stance",						1, "002D8823",   "" ,		 0, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+60 HP while Hexed",							1, "003C7823",   "" ,	 	 0, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+20% Enchantment Duration",						1, "1400B822",   "" , 		 0, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Axe Mastery +1 (20% chance)",					1, "14121824",   "" ,		-1, -1, -1, -1,  0, -1, -1, -1, -1, -1, -1], _
		 [ "Marksmanship +1 (20% chance)",					1, "14191824",   "" ,		-1, -1, -1, -1, -1,  0, -1, -1, -1, -1, -1], _
		 [ "Hammer Mastery +1 (20% chance)",				1, "14131824",   "" , 		-1, -1, -1, -1, -1, -1,  0, -1, -1, -1, -1], _
		 [ "Dagger Mastery +1 (20% chance)",				1, "141D1824",   "" ,		-1, -1, -1, -1, -1, -1, -1,  0, -1, -1, -1], _
		 [ "Scythe Mastery +1 (20% chance)",				1, "14291824",   "" ,		-1, -1, -1, -1, -1, -1, -1, -1,  0, -1, -1], _
		 [ "Spear Mastery +1 (20% chance)",					1, "14251824",   "" ,		-1, -1, -1, -1, -1, -1, -1, -1, -1,  0, -1], _
		 [ "Swordmanship +1 (20% chance)",					1, "14141824",   "" ,		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0], _
		 [ "Air Magic +1 (20% chance)",						1, "14081824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Blood Magic +1 (20% chance)",					1, "14041824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Channeling Magic +1 (20% chance)",				1, "14221824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Communing Magic +1 (20% chance)",				1, "14201824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Curse Magic +1 (20% chance)",					1, "14071824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Death Magic +1 (20% chance)",					1, "14051824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Divine Favor  +1 (20% chance)",					1, "14101824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Domination Magic +1 (20% chance)",				1, "14021824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Earth Magic +1 (20% chance)",					1, "14091824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Fire Magic +1 (20% chance)",					1, "140A1824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Healing Prayers +1 (20% chance)",				1, "140D1824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Illusion Magic +1 (20% chance)",				1, "14011824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Inspiration  +1 (20% chance)",					1, "14031824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Protection Prayers +1 (20% chance)",			1, "140F1824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Restoration Magic +1 (20% chance)",				1, "14211824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Smiting Prayers +1 (20% chance)",				1, "140E1824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Soul Reaping +1 (20% chance)",					1, "14061824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Spawning Magic +1 (20% chance)",				1, "14241824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Water Magic +1 (20% chance)",					1, "140B1824",   "" ,		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+7 armor vs Physical",							1, "07005821",   "" ,		 0, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "+7 Armor vs Elemental",							1, "07002821",   "" ,		 0, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Armor +5",										1, "05000821",   "" ,		 0, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0], _
		 [ "Armor +5 (HP> 50%)",							2, "0532A821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (HP< 50%)",							2, "0A32B821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (while Enchanted)",					2, "05009821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (while attacking)",					2, "05007821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (while casting)",						2, "05008821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (vs Elemental)",						2, "05002821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (vs Physical)",						2, "05005821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (Energy -5)",							2, "0500B820",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +5 (Health -20)",							2, "1400D820",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (while Hexed)",						2, "0A00C821",   "" ,		-1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+10 Armor vs.Undead",							2, "0A004821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _ ; cannot be salavaged
		 [ "+10 Armor vs.Charr",							2, "0A014821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _ ; cannot be salavaged
		 [ "+10 Armor vs.Trolls",							2, "0A024821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _ ; cannot be salavaged
		 [ "+10 Armor vs.Plants",							2, "0A034821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _ ; cannot be salavaged
		 [ "+10 Armor vs.Skeletons",						2, "0A044821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _ ; cannot be salavaged
		 [ "+10 Armor vs.Giants",							2, "0A054821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+10 Armor vs.Dwarves",							2, "0A064821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+10 Armor vs.Tengu",							2, "0A074821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+10 Armor vs.Demons",							2, "0A084821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+10 Armor vs.Dragons",							2, "0A094821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "+10 Armor vs.Ogres",							2, "0A0A4821",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Blunt)",							2, "0A0018A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Cold)",							2, "0A0318A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Earth)",							2, "0A0B18A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Fire)",							2, "0A0518A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Lightning)",						2, "0A0418A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Piercing)",						2, "0A0118A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1], _
		 [ "Armor +10 (vs Slashing)",						2, "0A0218A1",   "" ,		-1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1]]

		; Name, 									ModelID,Index, Modstruct, Salvage indicator
	Global $array_armormods [183][5] = [ _
		 ["Minor Critical Strikes [Assassin]", 		6324,	1, "0123E821", 1], _
		 ["Minor Dagger Mastery [Assassin]",		6324,	1, "011DE821", 0], _
		 ["Minor Deadly Arts [Assassin]", 			6324,	1, "011EE821", 0], _
		 ["Minor Shadow Arts [Assassin]", 			6324,	1, "011FE821", 0], _
		 ["Major Critical Strikes [Assassin]", 		6325,	1, "0223E8217902", 0], _
		 ["Major Dagger Mastery [Assassin]", 		6325,	1, "021DE8217902", 0], _
		 ["Major Deadly Arts [Assassin]", 			6325,	1, "021EE8217902", 0], _
		 ["Major Shadow Arts [Assassin]", 			6325,	1, "021FE8217902", 0], _
		 ["Superior Critical Strikes [Assassin]", 	6326,	1, "0323E8217B02", 0], _
		 ["Superior Dagger Mastery [Assassin]", 	6326,	1, "031DE8217B02", 0], _
		 ["Superior Deadly Arts [Assassin]", 		6326,	1, "031EE8217B02", 0], _
		 ["Superior Shadow Arts [Assassin]", 		6326,	1, "031FE8217B02", 0], _
		 ["Vanguard's Insignia [Assassin]", 		19124,	1, "DE010824", 0], _
		 ["Infiltrator's Insignia [Assassin]", 		19125,	0, "DF010824", 0], _
		 ["Saboteur's Insignia [Assassin]", 		19126,	0, "E0010824", 0], _
		 ["Nightstalker's Insignia [Assassin]", 	19127,	0, "E1010824", 0], _
		 ["Minor Earth Prayers[Dervish]", 			15545,	1, "012BE821", 1], _
		 ["Minor Mysticism[Dervish]", 				15545,	1, "012CE821", 0], _
		 ["Minor Scythe Mastery[Dervish]", 			15545,	1, "0129E821", 0], _
		 ["Minor Wind Prayers[Dervish]", 			15545,	1, "012AE821", 0], _
		 ["Major Earth Prayers[Dervish]", 			15546,	1, "022BE8210703", 0], _
		 ["Major Mysticism[Dervish]", 				15546,	1, "022CE8210703", 0], _
		 ["Major Scythe Mastery[Dervish]", 			15546,	1, "0229E8210703", 0], _
		 ["Major Wind Prayers[Dervish]", 			15546,	1, "022AE8210703", 0], _
		 ["Superior Earth Prayers[Dervish]", 		15547,	1, "032BE8210903", 0], _
		 ["Superior Mysticism[Dervish]", 			15547,	1, "032CE8210903", 0], _
		 ["Superior Scythe Mastery[Dervish]", 		15547,	1, "0329E8210903", 0], _
		 ["Superior Wind Prayers[Dervish]", 		15547,	1, "032AE8210903", 0], _
		 ["Windwalker Insignia [Dervish]", 			19163,	0, "02020824", 1], _
		 ["Forsaken Insignia [Dervish]", 			19164,	0, "03020824", 0], _
		 ["Minor Air Magic [Elementalist]", 		901,	1, "0108E821", 0], _
		 ["Minor Earth Magic [Elementalist]", 		901,	1, "0109E821", 0], _
		 ["Minor Energy Storage [Elementalist]", 	901,	1, "010CE821", 0], _
		 ["Minor Water Magic [Elementalist]", 		901,	1, "010BE821", 0], _
		 ["Minor Fire Magic [Elementalist]", 		901,	1, "010AE821", 0], _
		 ["Major Air Magic [Elementalist]", 		5554,	1, "0208E8216F01", 0], _
		 ["Major Earth Magic [Elementalist]", 		5554,	1, "0209E8216F01", 0], _
		 ["Major Energy Storage [Elementalist]", 	5554,	1, "020CE8216F01", 0], _
		 ["Major Fire Magic [Elementalist]", 		5554,	1, "020AE8216F01", 0], _
		 ["Major Water Magic [Elementalist]", 		5554,	1, "020BE8216F01", 0], _
		 ["Superior Air Magic [Elementalist]", 		5555,	1, "0308E8217B01", 1], _
		 ["Superior Earth Magic [Elementalist]", 	5555,	1, "0309E8217B01", 0], _
		 ["Superior Energy Storage [Elementalist]", 5555,	1, "030CE8217B01", 0], _
		 ["Superior Fire Magic [Elementalist]", 	5555,	1, "030AE8217B01", 1], _
		 ["Superior Water Magic [Elementalist]", 	5555,	1, "030BE8217B01", 0], _
		 ["Prismatic Insignia [Elementalist]", 		19144,	0, "F1010824", 0], _
		 ["Hydromancer Insignia [Elementalist]", 	19145,	0, "F2010824", 0], _
		 ["Geomancer Insignia [Elementalist]", 		19146,	0, "F3010824", 0], _
		 ["Pyromancer Insignia [Elementalist]", 	19147,	0, "F4010824", 0], _
		 ["Aeromancer Insignia [Elementalist]", 	19148,	0, "F5010824", 0], _
		 ["Rune of Attunement", 					898,	1, "0200D822", 0], _
		 ["Rune of Minor Vigor", 					898,	1, "C202E827", 1], _
		 ["Rune of Vitae", 							898,	1, "000A4823", 0], _
		 ["Rune of Clarity", 						5550,	1, "01087827", 1], _
		 ["Rune of Major Vigor", 					5550,	1, "C202E927", 1], _
		 ["Rune of Purity", 						5550,	1, "05067827", 0], _
		 ["Rune of Recovery", 						5550,	1, "07047827", 0], _
		 ["Rune of Restoration", 					5550,	1, "00037827", 0], _
		 ["Rune of Superior Vigor", 				5551,	1, "C202EA27", 1], _
		 ["Radiant Insignia",		 				19131,	0, "E5010824", 0], _
		 ["Survivor Insignia", 						19132,	0, "E6010824", 0], _
		 ["Stalwart Insignia", 						19133,	0, "E7010824", 0], _
		 ["Brawler's Insignia", 					19134,	0, "E8010824", 0], _
		 ["Blessed Insignia", 						19135,	0, "E9010824", 1], _
		 ["Herald's Insignia", 						19136,	0, "EA010824", 0], _
		 ["Sentry's Insignia", 						19137,	0, "EB010824", 0], _
		 ["Minor Domination Magic [Mesmer]", 		899,	1, "0102E821", 0], _
		 ["Minor Fast Casting [Mesmer]", 			899,	1, "0100E821", 0], _
		 ["Minor Illusion Magic [Mesmer]", 			899,	1, "0101E821", 0], _
		 ["Minor Inspiration Magic [Mesmer]", 		899,	1, "0103E821", 1], _
		 ["Major Domination Magic [Mesmer]", 		3612,	1, "0202E8216B01", 0], _
		 ["Major Fast Casting [Mesmer]", 			3612,	1, "0200E8216B01", 0], _
		 ["Major Illusion Magic [Mesmer]", 			3612,	1, "0201E8216B01", 0], _
		 ["Major Inspiration Magic [Mesmer]", 		3612,	1, "0203E8216B01", 0], _
		 ["Superior Domination Magic [Mesmer]", 	5549,	1, "0302E8217701", 1], _
		 ["Superior Fast Casting [Mesmer]", 		5549,	1, "0300E8217701", 0], _
		 ["Superior Illusion Magic [Mesmer]", 		5549,	1, "0301E8217701", 0], _
		 ["Superior Inspiration Magic [Mesmer]", 	5549,	1, "0303E8217701", 0], _
		 ["Artificer's Insignia [Mesmer]", 			19128,	0, "E2010824", 0], _
		 ["Prodigy's Insignia [Mesmer]", 			19129,	0, "E3010824", 1], _
		 ["Virtuoso's Insignia [Mesmer]", 			19130,	0, "E4010824", 0], _
		 ["Minor Divine Favor [Monk]", 				902,	1, "0110E821", 0], _
		 ["Minor Healing Prayers [Monk]", 			902,	1, "010DE821", 0], _
		 ["Minor Protection Prayers [Monk]", 		902,	1, "010FE821", 1], _
		 ["Minor Smiting Prayers [Monk]", 			902,	1, "010EE821", 0], _
		 ["Major Healing Prayers [Monk]", 			5556,	1, "020DE8217101", 0], _
		 ["Major Protection Prayers [Monk]", 		5556,	1, "020FE8217101", 0], _
		 ["Major Smiting Prayers [Monk]", 			5556,	1, "020EE8217101", 0], _
		 ["Major Divine Favor [Monk]", 				5556,	1, "0210E8217101", 0], _
		 ["Superior Divine Favor [Monk]", 			5557,	1, "0310E8217D01", 0], _
		 ["Superior Healing Prayers [Monk]", 		5557,	1, "030DE8217D01", 0], _
		 ["Superior Protection Prayers [Monk]", 	5557,	1, "030FE8217D01", 0], _
		 ["Superior Smiting Prayers [Monk]",		5557,	1, "030EE8217D01", 0], _
		 ["Wanderer's Insignia [Monk]", 			19149,	0, "F6010824", 0], _
		 ["Disciple's Insignia [Monk]", 			19150,	0, "F7010824", 0], _
		 ["Anchorite's Insignia [Monk]", 			19151,	0, "F8010824", 0], _
		 ["Minor Blood Magic [Necromancer]",		900,	1, "0104E821", 0], _
		 ["Minor Curses [Necromancer]", 			900,	1, "0107E821", 0], _
		 ["Minor Death Magic [Necromancer]", 		900,	1, "0105E821", 0], _
		 ["Minor Soul Reaping [Necromancer]", 		900,	1, "0106E821", 0], _
		 ["Major Blood Magic [Necromancer]",		5552,	1, "0204E8216D01", 0], _
		 ["Major Curses [Necromancer]",				5552,	1, "0207E8216D01", 0], _
		 ["Major Death Magic [Necromancer]",		5552,	1, "0205E8216D01", 0], _
		 ["Major Soul Reaping [Necromancer]", 		5552,	1, "0206E8216D01", 1], _
		 ["Superior Blood Magic [Necromancer]", 	5553,	1, "0304E8217901", 0], _
		 ["Superior Curses [Necromancer]",			5553,	1, "0307E8217901", 0], _
		 ["Superior Death Magic [Necromancer]",		5553,	1, "0305E8217901", 1], _
		 ["Superior Soul Reaping [Necromancer",		5553,	1, "0306E8217901", 0], _
		 ["Bloodstained Insignia [Necromancer]",	19138,	0, "0A020824", 0], _
		 ["Tormentor's Insignia [Necromancer]",		19139,	0, "EC010824", 1], _
		 ["Undertaker's Insignia [Necromancer]",	19140,	0, "ED010824", 0], _
		 ["Bonelace Insignia [Necromancer]",		19141,	0, "EE010824", 0], _
		 ["Minion Master's Insignia [Necromancer]",	19142,	0, "EF010824", 0], _
		 ["Blighter's Insignia [Necromancer]",		19143,	1, "F0010824", 0], _
		 ["Minor Command [Paragon]",				15548,	1, "0126E821", 0], _
		 ["Minor Leadership [Paragon]",				15548,	1, "0128E821", 0], _
		 ["Minor Motivation [Paragon]",				15548,	1, "0127E821", 0], _
		 ["Minor Spear Mastery [Paragon]",			15548,	1, "0125E821", 1], _
		 ["Major Command [Paragon]",				15549,	1, "0226E8210D03", 0], _
		 ["Major Leadership [Paragon]",				15549,	1, "0228E8210D03", 0], _
		 ["Major Motivation [Paragon]",				15549,	1, "0227E8210D03", 0], _
		 ["Major Spear Mastery [Paragon]",			15549,	1, "0225E8210D03", 0], _
		 ["Superior Command [Paragon]",				15550,	1, "0326E8210F03", 0], _
		 ["Superior Leadership [Paragon]",			15550,	1, "0328E8210F03", 0], _
		 ["Superior Motivation [Paragon]",			15550,	1, "0327E8210F03", 0], _
		 ["Superior Spear Mastery [Paragon]",		15550,	1, "0325E8210F03", 0], _
		 ["Centurion's Insignia [Paragon]",			19168,	0, "07020824", 1], _
		 ["Minor Beast Mastery [Ranger]",			904,	1, "0116E821", 0], _
		 ["Minor Expertise [Ranger]",				904,	1, "0117E821", 0], _
		 ["Minor Marksmanship [Ranger]",			904,	1, "0119E821", 0], _
		 ["Minor Wilderness Survival [Ranger]",		904,	1, "0118E821", 0], _
		 ["Major Beast Mastery [Ranger]",			5560,	1, "0216E8217501", 0], _
		 ["Major Expertise [Ranger]",				5560,	1, "0217E8217501", 0], _
		 ["Major Marksmanship [Ranger]",			5560,	1, "0219E8217501", 0], _
		 ["Major Wilderness Survival [Ranger]",		5560,	1, "0218E8217501", 0], _
		 ["Superior Beast Mastery [Ranger]",		5561,	1, "0316E8218101", 0], _
		 ["Superior Expertise [Ranger]",			5561,	1, "0317E8218101", 0], _
		 ["Superior Marksmanship [Ranger]",			5561,	1, "0319E8218101", 0], _
		 ["Superior Wilderness Survival [Ranger]",	5561,	1, "0318E8218101", 0], _
		 ["Frostbound Insignia [Ranger]",			19157,	0, "FC010824", 0], _
		 ["Earthbound Insignia [Ranger]",			19158,	0, "FD010824", 0], _
		 ["Pyrebound Insignia [Ranger]",			19159,	0, "FE010824", 0], _
		 ["Stormbound Insignia [Ranger]",			19160,	0, "FF010824", 0], _
		 ["Beastmaster's Insignia [Ranger]",		19161,	0, "00020824", 0], _
		 ["Scout's Insignia [Ranger]",				19162,	0, "01020824", 0], _
		 ["Minor Channeling Magic [Ritualist]",		6327,	1, "0122E821", 0], _
		 ["Minor Communing [Ritualist]",			6327,	1, "0120E821", 0], _
		 ["Minor Restoration Magic [Ritualist]",	6327,	1, "0121E821", 0], _
		 ["Minor Spawning Power [Ritualist]",		6327,	1, "0124E821", 0], _
		 ["Major Channeling Magic [Ritualist]",		6328,	1, "0222E8217F02", 0], _
		 ["Major Communing [Ritualist]",			6328,	1, "0220E8217F02", 0], _
		 ["Major Restoration Magic [Ritualist]",	6328,	1, "0221E8217F02", 0], _
		 ["Major Spawning Power [Ritualist]",		6328,	1, "0224E8217F02", 0], _
		 ["Superior Channeling Magic [Ritualist]",	6329,	1, "0322E8218102", 0], _
		 ["Superior Communing [Ritualist]",			6329,	1, "0320E8218102", 1], _
		 ["Superior Restoration Magic [Ritualist]",	6329,	1, "0321E8218102", 0], _
		 ["Superior Spawning Power [Ritualist]",	6329,	1, "0324E8218102", 0], _
		 ["Shaman's Insignia [Ritualist]",			19165,	0, "04020824", 1], _
		 ["Ghost Forge Insignia [Ritualist]",		19166,	0, "05020824", 0], _
		 ["Mystic's Insignia [Ritualist]",			19167,	0, "06020824", 0], _
		 ["Minor Absorption [Warrior]",				903,	0, "EA02E827", 0], _
		 ["Minor Axe Mastery [Warrior]",			903,	1, "0112E821", 0], _
		 ["Minor Hammer Mastery [Warrior]",			903,	1, "0113E821", 0], _
		 ["Minor Strength [Warrior]",				903,	1, "0111E821", 0], _
		 ["Minor Swordsmanship [Warrior]",			903,	1, "0114E821", 0], _
		 ["Minor Tactics [Warrior]",				903,	1, "0115E821", 0], _
		 ["Major Absorption [Warrior]",				5558,	1, "EA02E927", 0], _
		 ["Major Axe Mastery [Warrior]",			5558,	1, "0212E8217301", 0], _
		 ["Major Hammer Mastery [Warrior]",			5558,	1, "0213E8217301", 0], _
		 ["Major Strength [Warrior]",				5558,	1, "0211E8217301", 0], _
		 ["Major Swordsmanship [Warrior]",			5558,	1, "0214E8217301", 0], _
		 ["Major Tactics [Warrior]",				5558,	1, "0215E8217301", 0], _
		 ["Superior Axe Mastery [Warrior]",			5559,	1, "0312E8217F01", 0], _
		 ["Superior Hammer Mastery [Warrior]",		5559,	1, "0313E8217F01", 0], _
		 ["Superior Strength [Warrior]",			5559,	1, "0311E8217F01", 0], _
		 ["Superior Swordsmanship [Warrior]",		5559,	1, "0314E8217F01", 0], _
		 ["Superior Tactics [Warrior]",				5559,	1, "0315E8217F01", 0], _
		 ["Superior Absorption [Warrior]",			5559,	1, "EA02EA27", 0], _
		 ["Knight's Insignia [Warrior]",			19152,	0, "F9010824", 0], _
		 ["Lieutenant's Insignia [Warrior]",		19153,	0, "08020824", 0], _
		 ["Stonefist Insignia [Warrior]",			19154,	0, "09020824", 0], _
		 ["Dreadnought Insignia [Warrior]",			19155,	0, "FA010824", 0], _
		 ["Sentinel's Insignia [Warrior]",			19156,	0, "FB010824", 1]]

;~ 		 _ArrayDisplay ($array_armormods)

Func ModSelection()
   Global $frmSelection = GUICreate("Mod Selection", 816, 660, 500, 112)
   Global $array_checkboxes[133][15]


   GUICtrlSetFont($frmSelection, 9, -1, 0, "Arial")

   GUICtrlCreateTab(1, 0, 816, 660)
   $X_GUI = 8
   $Y_GUI = 8
   $Width_GUI = 390
   $Height_GUI = 312
   $iSpacing= 8

   If FileExists($MOD_INI_PATH) Then $array_weaponmods_ini = _iniToArray("Mod_Settings.ini", "array_weaponmods")
;  _ArrayDisplay ($Array_weaponmods_ini, "2D Display")


   GUICtrlCreateTabItem("Popular Mods")
		 Global $grpEnergy = GUICtrlCreateGroup("", 0, 14, 816, 660)
		 For $j = 4 to 14
			GUICtrlCreateLabel($ColLabels[$j-4], $X_GUI + 55 +(50 * $j), 40 , $iSpacing*6, $iSpacing*2, $ES_CENTER)
			For $i = 0 To 32
				If $j = 4 then GUICtrlCreateLabel($array_weaponmods[$i][0], $X_GUI + $iSpacing*2, 60 + (18 * $i),200, $iSpacing*2)
				$array_checkboxes[$i][$j] = GUICtrlCreateCheckbox("", $X_GUI + 72+(50 * $j), 60 + (18 * $i), $iSpacing*2, $iSpacing*2, BitOR($BS_AUTOCHECKBOX, $ES_CENTER,($array_checkboxes[$i][$j] > -1 ? $GUI_CHECKED : $GUI_UNCHECKED)))
				If $array_weaponmods[$i][$j] 		= 1 then guictrlsetstate (-1, $GUI_CHECKED)
			    If $array_weaponmods[$i][$j] 		=-1 then guictrlsetstate (-1, $GUI_DISABLE)
			    If $array_weaponmods_ini[$i][$j] 	= 1 then guictrlsetstate (-1, $GUI_CHECKED)
				Next
			 Next
   GUICtrlCreateTabItem("") ; end tabitem definition

   GUICtrlCreateTabItem("Damage")
		 Global $grpEnergy = GUICtrlCreateGroup("", 0, 14, 816, 660)
		 For $j = 4 to 14
			GUICtrlCreateLabel($ColLabels[$j-4], $X_GUI + 55 +(50 * $j),40 , $iSpacing*6, $iSpacing*2, $ES_CENTER)
			For $i = 33 To 68
				If $j = 4 then GUICtrlCreateLabel($array_weaponmods[$i][0], $X_GUI + $iSpacing*2, 60 + (16 * ($i - 33)),200, $iSpacing*2)
				$array_checkboxes[$i][$j] = GUICtrlCreateCheckbox("", $X_GUI + 72+(50 * $j), 60 + (16 * ($i - 33)), $iSpacing*2, $iSpacing*2, BitOR($BS_AUTOCHECKBOX, $ES_CENTER,($array_checkboxes[$i][$j] >-1 ? $GUI_CHECKED : $GUI_UNCHECKED)))
				If $array_weaponmods[$i][$j] = 1 then guictrlsetstate (-1, $GUI_CHECKED)
			    If $array_weaponmods[$i][$j] 		= -1 then guictrlsetstate (-1, $GUI_DISABLE)
			    If $array_weaponmods_ini[$i][$j] 	= 1 then guictrlsetstate (-1, $GUI_CHECKED)
				Next
			 Next

   GUICtrlCreateTabItem("") ; end tabitem definition

   GUICtrlCreateTabItem("HP, Echanting and Attribute")
		 Global $grpEnergy = GUICtrlCreateGroup("", 0, 14, 816, 660)
		 For $j = 4 to 14
			GUICtrlCreateLabel($ColLabels[$j-4], $X_GUI + 55 +(50 * $j), 40 , $iSpacing*6, $iSpacing*2, $ES_CENTER)
			For $i = 69 To 101
				If $j = 4 then GUICtrlCreateLabel($array_weaponmods[$i][0], $X_GUI+$iSpacing*2, 60 + (18 * ($i - 69)),200, $iSpacing*2)
				$array_checkboxes[$i][$j] = GUICtrlCreateCheckbox("", $X_GUI + 72+(50 * $j), 60 + (18 * ($i - 69)), $iSpacing*2, $iSpacing*2, BitOR($BS_AUTOCHECKBOX, $ES_CENTER,($array_checkboxes[$i][$j] >-1 ? $GUI_CHECKED : $GUI_UNCHECKED)))
			    If $array_weaponmods[$i][$j] = 1 then guictrlsetstate (-1, $GUI_CHECKED)
			    If $array_weaponmods[$i][$j] 		= -1 then guictrlsetstate (-1, $GUI_DISABLE)
			    If $array_weaponmods_ini[$i][$j] 	= 1 then guictrlsetstate (-1, $GUI_CHECKED)
				Next
			 Next

   GUICtrlCreateTabItem("") ; end tabitem definition

   GUICtrlCreateTabItem("Armor")
		 Global $grpEnergy = GUICtrlCreateGroup("", 0, 14, 816, 660)
		 For $j = 4 to 14
			GUICtrlCreateLabel($ColLabels[$j-4], $X_GUI + 55 +(50 * $j), 40 , $iSpacing*6, $iSpacing*2, $ES_CENTER)
			For $i = 102 To 132
				If $j = 4 then GUICtrlCreateLabel($array_weaponmods[$i][0], $X_GUI+$iSpacing*2, 60 + (19 * ($i - 102)),200, $iSpacing*2)
				$array_checkboxes[$i][$j] = GUICtrlCreateCheckbox("", $X_GUI + 72+(50 * $j), 60 + (19 * ($i - 102)), $iSpacing*2, $iSpacing*2, BitOR($BS_AUTOCHECKBOX, $ES_CENTER,($array_checkboxes[$i][$j] >-1 ? $GUI_CHECKED : $GUI_UNCHECKED)))
			    If $array_weaponmods[$i][$j] = 1 then guictrlsetstate (-1, $GUI_CHECKED)
			    If $array_weaponmods[$i][$j] 		= -1 then guictrlsetstate (-1, $GUI_DISABLE)
			    If $array_weaponmods_ini[$i][$j] 	= 1 then guictrlsetstate (-1, $GUI_CHECKED)
			Next
		 Next

   GUICtrlCreateTabItem("") ; end tabitem definition

   GUISetOnEvent($GUI_EVENT_CLOSE, "SpecialEvents")

   GUICtrlCreateGroup("", -99, -99, 1, 1)

   GUISetState()

EndFunc

Func SpecialEvents()

   Select
        Case @GUI_CtrlId = $GUI_EVENT_CLOSE
			For $j = 4 to 14
			   For $i = 0 to 132
				  $array_weaponmods[$i][$j] = GUICtrlRead($array_checkboxes[$i][$j])
			   Next
			Next

			_arrayToIni("Mod_Settings.ini", "Mod_Settings.ini", $array_weaponmods)

;			_ArrayDisplay($array_weaponmods_ini, "2D display")

			GUISetState (@SW_HIDE)
    EndSelect
EndFunc

; #Region Logging Wrappers
; Added to resolve missing functions

Global $WARN_ONCE_CACHE[]

; Logging functions that write to GUI console
Func Notice($Text)
    Out("[NOTICE] " & $Text)
EndFunc

Func Warn($Text)
    Out("[WARN] " & $Text)
EndFunc

Func WarnOnce($Text)
    If $WARN_ONCE_CACHE[$Text] = True Then Return
    $WARN_ONCE_CACHE[$Text] = True
    Warn($Text)
EndFunc

Func Debug($Text)
	; Simple debug wrapper
    Out("[DEBUG] " & $Text)
EndFunc

Func Info($Text)
    Out("[INFO] " & $Text)
EndFunc

Func Error($Text)
    MsgBox(16, "Error", $Text)
    Exit
EndFunc
; #EndRegion Logging Wrappers