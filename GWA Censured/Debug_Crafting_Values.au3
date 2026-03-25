#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\GWA2_ID.au3"
#include "lib\Utils.au3"
#include <GUIConstantsEx.au3>
#include "lib\GUI_Functions.au3"
#include <Memory.au3>

Global Const $BOTNAME = "Craft+Sniff Debugger"
Global Const $VERSION = "0.3 (Merged)"
Global Const $AUTHORS[1] = ["Antigravity"]

Global $BotRunning = False
Global $HookInstalled = False
Global $OriginalBytes
Global $HookAddress
Global $DataAddress
Global $PacketSendFuncAddr

Main()

Func Main()
    ScanAndUpdateGameClients()
    Local $targetChar = "L I L B I S C U I T"
    Local $clientIndex = FindClientIndexByCharacterName($targetChar)
    
    If $clientIndex > 0 Then
        SelectClient($clientIndex)
        InitializeGameClientData(True, False)
        WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())
    Else
        MsgBox(48, "Error", "Character '" & $targetChar & "' not found!")
        Exit
    EndIf
    
    GUI_Create()
    GUI_SetOnStartFunc("StartBot")
    GUI_SetOnStopFunc("StopBot")
    
    Out("=== Craft+Sniff Debugger V0.3 ===")
    Out("MERGED SCRIPT to avoid crashes.")
    Out("1. Click Start to Install Hook.")
    Out("2. Click Start AGAIN to Craft.")
    
    OnAutoItExitRegister("CleanupHook")
    
    While 1
        If $BotRunning Then
            If Not $HookInstalled Then
                InstallHook()
                $BotRunning = False ; Stop after install
                Out("Hook Installed. Click Start again to Craft.")
            Else
                TestCrafting()
                $BotRunning = False ; Stop after craft attempt
            EndIf
            
            GUI_HideButton($GUI_idButtonStart, False)
            GUI_HideButton($GUI_idButtonStop, True)
        Else
            CheckCapturedData()
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartBot()
    $BotRunning = True
EndFunc

Func StopBot()
    $BotRunning = False
    CleanupHook()
    Out("Stopped/Hook Removed.")
EndFunc

Func checkCapturedData()
    If $DataAddress == 0 Then Return
    
    Local $flag = MemoryRead($DataAddress, 'dword')
    If $flag == 1 Then
        Out("--- PACKET SEND TRIGGERED ---")
        
        Local $eax = MemoryRead($DataAddress + 4, 'dword')
        Local $ecx = MemoryRead($DataAddress + 8, 'dword')
        Local $edx = MemoryRead($DataAddress + 12, 'dword')
        Out(StringFormat("EAX: %08X, ECX: %08X, EDX: %08X", $eax, $ecx, $edx))
        
        Out("Stack Args:")
        For $i = 0 To 4
            Local $arg = MemoryRead($DataAddress + 16 + ($i * 4), 'dword')
            Out(StringFormat("Arg[%d]: %08X", $i, $arg))
        Next
        
         ; Inspect EAX (Packet Object?) if valid pointer
        If $eax > 0x10000 Then
             Local $bytes = MemoryRead($eax, 'byte[16]')
             Out("EAX Bytes: " & String($bytes))
        EndIf
        
        MemoryWrite($DataAddress, 0, 'dword')
    EndIf
EndFunc

Func TestCrafting()
    Out("Attempting to craft Grail...")
    
    Local $grailID = 24861
    Local $mats[2][2]
    $mats[0][0] = $ID_IRON_INGOT
    $mats[0][1] = 10
    $mats[1][0] = $ID_PILE_OF_GLITTERING_DUST
    $mats[1][1] = 10
    
    Local $res = CraftItemSafe($grailID, 1, 100, $mats)
    
    If @error Then
        Out("Craft Failed! Error: " & @error & " Ext: " & @extended)
    Else
        Out("Craft Function Returned Success (Check Game!)")
    EndIf
EndFunc

; --- Sniffer Functions ---

Func SetProtection($address, $size, $protect)
    Local $oldProtect = DllStructCreate("dword")
    Local $ret = SafeDllCall13($kernel_handle, 'bool', 'VirtualProtectEx', 'handle', GetProcessHandle(), 'ptr', $address, 'ulong_ptr', $size, 'dword', $protect, 'ptr', DllStructGetPtr($oldProtect))
    If @error Or Not $ret[0] Then
        Out("VirtualProtectEx Failed! Error: " & @error)
        Return 0
    EndIf
    Return DllStructGetData($oldProtect, 1)
EndFunc

Func WriteMemVerbose($address, $dataBytes)
    Local $buffer = SafeDllStructCreate('byte[' & BinaryLen($dataBytes) & ']')
    DllStructSetData($buffer, 1, $dataBytes)
    Local $ret = SafeDllCall13($kernel_handle, 'int', 'WriteProcessMemory', 'int', GetProcessHandle(), 'ptr', $address, 'ptr', DllStructGetPtr($buffer), 'int', DllStructGetSize($buffer), 'int', 0)
    If @error Or Not $ret[0] Then
         Out("WriteMemory Failed at " & Hex($address))
    EndIf
EndFunc

Func InstallHook()
    $PacketSendFuncAddr = GetValue('PacketSendFunction')
    If $PacketSendFuncAddr == 0 Then
        Out("Error: PacketSendFunction not found.")
        Return
    EndIf
    
    $HookAddress = MemAlloc(128)
    
    Local $asm = ""
    ; Prologue (Replicate 6 bytes: 55 8B EC 83 EC 50)
    $asm &= "55"                ; push ebp
    $asm &= "8BEC"              ; mov ebp, esp
    $asm &= "83EC50"            ; sub esp, 0x50
    
    ; NULL HOOK: Just JMP Back.
    
    ; Jump Back to PacketSendFunction + 6
    MemoryWrite($HookAddress, '0x' & $asm, 'byte[]')
    Local $codeSize = StringLen($asm) / 2
    
    Local $jumpBackSrc = $HookAddress + $codeSize
    Local $jumpBackTarget = $PacketSendFuncAddr + 6
    Local $relOffset = $jumpBackTarget - ($jumpBackSrc + 5)
    Local $jmpBackInstr = "E9" & ReorderBytes(Hex($relOffset, 8))
    WriteMemVerbose($jumpBackSrc, Binary('0x' & $jmpBackInstr))
    
    $OriginalBytes = MemoryRead($PacketSendFuncAddr, 'byte[6]')
    
    Local $oldProtect = SetProtection($PacketSendFuncAddr, 16, 0x40)
    
    If $oldProtect <> 0 Then
        Local $relHook = $HookAddress - ($PacketSendFuncAddr + 5)
        Local $hookInstr = "E9" & ReorderBytes(Hex($relHook, 8))
        $hookInstr &= "90" ; 1 NOP
        
        WriteMemVerbose($PacketSendFuncAddr, Binary('0x' & $hookInstr))
        SetProtection($PacketSendFuncAddr, 16, $oldProtect)
        $HookInstalled = True
    EndIf
    
    Out("Null Hook Installed at " & Hex($HookAddress))
EndFunc

Func CleanupHook()
    If Not $HookInstalled Then Return
    If $OriginalBytes Then
        Local $oldProtect = SetProtection($PacketSendFuncAddr, 16, 0x40)
        WriteMemVerbose($PacketSendFuncAddr, $OriginalBytes)
        SetProtection($PacketSendFuncAddr, 16, $oldProtect)
    EndIf
    MemFree($HookAddress)
    MemFree($DataAddress)
    $HookInstalled = False
EndFunc

Func MemAlloc($size)
    Local $mem = SafeDllCall13($kernel_handle, 'ptr', 'VirtualAllocEx', 'handle', GetProcessHandle(), 'ptr', 0, 'ulong_ptr', $size, 'dword', 0x1000, 'dword', 0x40)
    Return $mem[0]
EndFunc

Func MemFree($ptr)
    SafeDllCall13($kernel_handle, 'bool', 'VirtualFreeEx', 'handle', GetProcessHandle(), 'ptr', $ptr, 'ulong_ptr', 0, 'dword', 0x8000)
EndFunc

Func ReorderBytes($sHex)
    Local $sRes = ""
    For $i = StringLen($sHex) - 1 To 0 Step -2
        $sRes &= StringMid($sHex, $i, 2)
    Next
    Return $sRes
EndFunc

; Copied from Utils-Maintenance.au3 with GWA2 Patch Logic
Func CraftItemSafe($modelID, $amount, $gold, ByRef $materialsArray)
	If Not GetIsMerchantOpen() Then Return SetError(1)
	
	Local $merchantItemsBase = GetMerchantItemsBase()
	If Not $merchantItemsBase Then Return SetError(2)
	Local $merchantItemsSize = GetMerchantItemsSize()
	
	Local $itemPtr = 0
	Local $destinationItemPtr = 0
	Local $itemIndex = -1
	Local $itemID = 0
	Local $destinationItemGUID = 0
	
	For $i = 0 To $merchantItemsSize - 1
		$itemID = MemoryRead($merchantItemsBase + 4 * $i)
		If ($itemID) Then
			Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 4 * $itemID]
			Local $result = MemoryReadPtr($base_address_ptr, $offsets)
			Local $ptr = $result[1]
			
			If $ptr <> 0 And MemoryRead($ptr + 0x2C) = $modelID Then
				$destinationItemPtr = $ptr
                $destinationItemGUID = $itemID
				$itemIndex = $i
				ExitLoop
			EndIf
		EndIf
	Next
	
	If $itemIndex = -1 Then Return SetError(3)
    Out("Item Found at Index: " & $itemIndex)

	Local $materialString = ''
	Local $materialCount = 0
	Local $materialsArraySize = UBound($materialsArray) - 1
    
	For $i = 0 To $materialsArraySize
		$materialString &= GetItemIDFromModelID($materialsArray[$i][0]) & ';' & $materialsArray[$i][1] & ';'
		$materialCount += 2
	Next

	Local $craftingMaterialType = 'dword'
	For $i = 1 To $materialCount - 1
		$craftingMaterialType &= ';dword'
	Next
	Local $craftingMaterialStruct = SafeDllStructCreate($craftingMaterialType)
	Local $craftingMaterialStructPtr = DllStructGetPtr($craftingMaterialStruct)
	For $i = 1 To $materialCount
		Local $size = StringInStr($materialString, ';')
		DllStructSetData($craftingMaterialStruct, $i, StringLeft($materialString, $size - 1))
		$materialString = StringTrimLeft($materialString, $size)
	Next
	
	Local $memorySize = $materialCount * 4
	Local $processHandle = GetProcessHandle()
	Local $memoryBuffer = SafeDllCall13($kernel_handle, 'ptr', 'VirtualAllocEx', 'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $memorySize, 'dword', 0x1000, 'dword', 0x40)
	If $memoryBuffer = 0 Or $memoryBuffer[0] = 0 Then Return 0
	
	SafeDllCall13($kernel_handle, 'int', 'WriteProcessMemory', 'int', $processHandle, 'int', $memoryBuffer[0], 'ptr', $craftingMaterialStructPtr, 'int', $memorySize, 'int', 0)
	
	If $trade_id_addr <> 0 Then
        MemoryWrite($trade_id_value_addr, $itemIndex, 'dword')
        MemoryWrite($trade_id_addr, $trade_id_value_addr, 'dword')
	EndIf

	DllStructSetData($CRAFT_ITEM_STRUCT, 1, GetValue('CommandCraftItemEx2'))
	DllStructSetData($CRAFT_ITEM_STRUCT, 2, $amount)
	DllStructSetData($CRAFT_ITEM_STRUCT, 3, $destinationItemGUID) ; Offset 8: ID (GUID)
	DllStructSetData($CRAFT_ITEM_STRUCT, 4, $memoryBuffer[0])
	DllStructSetData($CRAFT_ITEM_STRUCT, 5, $materialCount)
	DllStructSetData($CRAFT_ITEM_STRUCT, 6, $amount * $gold)
	
    Out("Calling Enqueue with CommandCraftItemEx2")
	Enqueue($CRAFT_ITEM_STRUCT_PTR, 24)
	
	Sleep(1000)
	SafeDllCall11($kernel_handle, 'ptr', 'VirtualFreeEx', 'handle', $processHandle, 'ptr', $memoryBuffer[0], 'int', 0, 'dword', 0x8000)
	
	Return 1
EndFunc

Func _GetMerchantItemPtrByModelId_Safe($modelID)
    Local $merchantItemsBase = GetMerchantItemsBase()
    Local $merchantItemsSize = GetMerchantItemsSize()
    For $i = 0 To $merchantItemsSize - 1
        Local $itemID = MemoryRead($merchantItemsBase + 4 * $i)
        If $itemID Then
            Local $itemPtr = MemoryRead(MemoryRead($base_address_ptr) + 0x18, 'ptr')
            Local $offsets[5] = [0, 0x18, 0x40, 0xB8, 4 * $itemID]
            Local $result = MemoryReadPtr($base_address_ptr, $offsets)
            $itemPtr = $result[1]
            If $itemPtr <> 0 And MemoryRead($itemPtr + 0x2C) = $modelID Then
                Return $itemPtr
            EndIf
        EndIf
    Next
    Return 0
EndFunc
