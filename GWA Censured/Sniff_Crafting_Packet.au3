#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\GWA2_ID.au3"
#include "lib\Utils.au3"
#include <GUIConstantsEx.au3>
#include "lib\GUI_Functions.au3"
#include <Memory.au3>

Global Const $BOTNAME = "Packet Sniffer"
Global Const $VERSION = "1.6 (Capture Restored)"
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
    GUI_SetOnStartFunc("StartSniffer")
    GUI_SetOnStopFunc("StopSniffer")
    
    Out("=== Packet Sniffer V1.6 ===")
    Out("Click Start to install Capture hook.")
    Out("Run Crafting Debugger separately.")
    
    OnAutoItExitRegister("CleanupHook")
    
    While 1
        If $BotRunning Then
            CheckCapturedData()
        Else
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartSniffer()
    If $HookInstalled Then Return
    InstallHook()
    $BotRunning = True
    Out("Capture Hook Installed.")
EndFunc

Func StopSniffer()
    $BotRunning = False
    CleanupHook()
    Out("Hook Removed.")
EndFunc

Func CheckCapturedData()
    If $DataAddress == 0 Then Return
    
    Local $flag = MemoryRead($DataAddress, 'dword')
    If $flag == 1 Then
        ; Data Captured!
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
             Local $vtable = MemoryRead($eax, 'dword')
             Out("EAX VTable: " & Hex($vtable))
             ; Maybe dump bytes at EAX
             Local $bytes = MemoryRead($eax, 'byte[16]')
             Out("EAX Bytes: " & String($bytes))
        EndIf
        
        ; Reset Flag
        MemoryWrite($DataAddress, 0, 'dword')
    EndIf
    Sleep(10)
EndFunc

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
    
    ; Allocate Data Buffer: [Flag, EAX, ECX, EDX, Stack[0], ... Stack[4]]
    $DataAddress = MemAlloc(64)
    MemoryWrite($DataAddress, 0, 'dword') ; Clear Flag
    
    $HookAddress = MemAlloc(256)
    
    ; Save original bytes BEFORE patching
    $OriginalBytes = MemoryRead($PacketSendFuncAddr, 'byte[6]')
    Out("Original bytes: " & $OriginalBytes)
    
    ; --- Build capture block first to measure its length ---
    Local $captureAsm = ""
    ; Write Flag = 1
    $captureAsm &= "C705" & ReorderBytes(Hex($DataAddress, 8)) & "01000000"
    ; EAX from pushad stack
    $captureAsm &= "8B442420"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 4, 8))
    ; ECX
    $captureAsm &= "8B44241C"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 8, 8))
    ; EDX
    $captureAsm &= "8B442418"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 12, 8))
    ; Arg0-Arg4 from original stack
    $captureAsm &= "8B442428"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 16, 8))
    $captureAsm &= "8B44242C"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 20, 8))
    $captureAsm &= "8B442430"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 24, 8))
    $captureAsm &= "8B442434"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 28, 8))
    $captureAsm &= "8B442438"
    $captureAsm &= "A3" & ReorderBytes(Hex($DataAddress + 32, 8))
    
    Local $captureLen = StringLen($captureAsm) / 2
    Out("Capture block size: " & $captureLen & " bytes")
    
    ; --- Build the full hook ---
    Local $asm = ""
    ; Save Registers
    $asm &= "60"                ; pushad
    $asm &= "9C"                ; pushfd
    
    ; Check if Flag is 0 (Ready to capture)
    $asm &= "A1" & ReorderBytes(Hex($DataAddress, 8)) ; mov eax, [DataAddress]
    $asm &= "85C0"              ; test eax, eax
    $asm &= "75" & Hex($captureLen, 2) ; jnz skip_capture (dynamic offset)
    
    ; Capture code
    $asm &= $captureAsm
    
    ; skip_capture label target - Restore
    $asm &= "9D"                ; popfd
    $asm &= "61"                ; popad
    
    ; Replay ACTUAL original prologue bytes (not hardcoded)
    Local $origHex = StringTrimLeft(String($OriginalBytes), 2) ; Remove 0x prefix
    $asm &= $origHex
    
    ; Write hook to allocated memory
    MemoryWrite($HookAddress, '0x' & $asm, 'byte[]')
    Local $codeSize = StringLen($asm) / 2
    
    ; Jump back to PacketSendFunction + 6
    Local $jumpBackSrc = $HookAddress + $codeSize
    Local $jumpBackTarget = $PacketSendFuncAddr + 6
    Local $relOffset = $jumpBackTarget - ($jumpBackSrc + 5)
    Local $jmpBackInstr = "E9" & ReorderBytes(Hex($relOffset, 8))
    WriteMemVerbose($jumpBackSrc, Binary('0x' & $jmpBackInstr))
    
    ; Patch PacketSendFunction entry with jump to hook
    Local $oldProtect = SetProtection($PacketSendFuncAddr, 16, 0x40)
    
    If $oldProtect <> 0 Then
        Local $relHook = $HookAddress - ($PacketSendFuncAddr + 5)
        Local $hookInstr = "E9" & ReorderBytes(Hex($relHook, 8))
        $hookInstr &= "90" ; 1 NOP
        
        WriteMemVerbose($PacketSendFuncAddr, Binary('0x' & $hookInstr))
        SetProtection($PacketSendFuncAddr, 16, $oldProtect)
        $HookInstalled = True
    EndIf
    
    Out("Hook Installed at " & Hex($HookAddress) & " (jnz offset=" & $captureLen & ")")
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
