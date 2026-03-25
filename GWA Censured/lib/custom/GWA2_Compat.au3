#include-once

; =============================================================================
; GWA2_Compat.au3
;
; Compatibility wrappers for memory read/write functions. These provide
; the GWA Censured calling convention (no $processHandle first arg)
; while delegating to whatever MemoryRead implementation is active.
;
; When upstream BotsHub files are adopted (Phase 4), the upstream
; MemoryRead($processHandle, $address, $type) becomes the canonical
; version. These wrappers let custom code continue using the simpler
; signature without modification.
;
; Usage in custom/ files:
;   MemRead($address, $type)         instead of MemoryRead($address, $type)
;   MemWrite($address, $data, $type) instead of MemoryWrite($address, $data, $type)
;   MemReadPtr($address, $offset, $type) instead of MemoryReadPtr(...)
;
; Created 2026-03-25
; =============================================================================

; Wrapper: reads memory using current process handle (no handle arg needed)
Func MemRead($address, $type = 'dword')
	Return MemoryRead($address, $type)
EndFunc

; Wrapper: writes memory using current process handle
Func MemWrite($address, $data, $type = 'dword')
	MemoryWrite($address, $data, $type)
EndFunc

; Wrapper: reads memory through pointer chain using current process handle
Func MemReadPtr($address, $offset, $type = 'dword')
	Return MemoryReadPtr($address, $offset, $type)
EndFunc
