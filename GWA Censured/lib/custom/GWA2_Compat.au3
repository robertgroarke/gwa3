#include-once

; =============================================================================
; GWA2_Compat.au3
;
; Compatibility wrappers for memory read/write functions. Upstream BotsHub
; uses MemoryRead($processHandle, $address, $type). These wrappers provide
; the simpler MemRead($address, $type) for use in custom/ files.
; =============================================================================

Func MemRead($address, $type = 'dword')
	Return MemoryRead(GetProcessHandle(), $address, $type)
EndFunc

Func MemWrite($address, $data, $type = 'dword')
	MemoryWrite(GetProcessHandle(), $address, $data, $type)
EndFunc

Func MemReadPtr($address, $offset, $type = 'dword')
	Return MemoryReadPtr(GetProcessHandle(), $address, $offset, $type)
EndFunc

; Wrapper for ClearMemory (upstream requires $processHandle arg)
Func ClearMem()
	ClearMemory(GetProcessHandle())
EndFunc
