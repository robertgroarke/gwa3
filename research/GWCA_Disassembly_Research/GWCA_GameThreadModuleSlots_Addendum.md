## GWCA `GameThreadModule` Slot Recovery Addendum

This pass takes the next logical step after the queue, callback-registry, lifecycle, and bootstrap work:

- stop inferring `GameThreadModule` behavior only from `GW::Initialize()`
- recover the concrete slot bodies embedded in the module record

The practical goal was to answer:

1. what do the callback pointers inside `PTR_s_GameThreadModule_100880c0` actually do?
2. do they line up with the two module-callback phases seen in `GW::Initialize()`?
3. is `GameThreadModule` a simple helper module, or a real first-class hook owner with full init/shutdown semantics?

## Starting point: the module object layout

From the already-recovered module object dump:

```text
PTR_s_GameThreadModule_100880c0:
+0x00 -> 0x10052518
+0x04 -> 0x00000000
+0x08 -> 0x100196F0
+0x0C -> 0x10019770
+0x10 -> 0x100196C0
+0x14 -> 0x10019740
+0x18 -> 0x1005252C
+0x1C -> 0x00000000
```

The interesting slots are therefore:

- `+0x08 = 0x100196F0`
- `+0x0C = 0x10019770`
- `+0x10 = 0x100196C0`
- `+0x14 = 0x10019740`

Earlier, `0x100196C0` was already known as:

- `GW::GameThread::EnableHooks`

The other three were not auto-defined as functions in a fresh Ghidra import, so this pass used direct disassembly around the addresses and then matched the bodies against named callees.

## What the four slots are

### Slot `+0x10` = `0x100196C0`

This is the already-named export:

- `GW::GameThread::EnableHooks`

Decompiled body:

```cpp
if (DAT_1008a0b8 != '\0') {
    EnterCriticalSection(&DAT_1008a098);
    Hook::EnableHooks(DAT_1008a0b0);
    LeaveCriticalSection(&DAT_1008a098);
}
```

So this module-phase callback enables the game-thread-owned detour root only if the subsystem has been initialized.

That exactly matches the earlier bootstrap picture:

- early init builds the subsystem
- post-hook phase enables the hook root

## Slot `+0x08` = `0x100196F0`

This body did not auto-promote to a named function in the fresh project, but the disassembly is clear:

```text
100196F0: EnterCriticalSection(&DAT_1008A098)
100196FB: push 0x300
10019704: push 0x10052504
10019709: push 0x100520F8
1001970E: call Scanner::FindAssertion
10019717: call Scanner::ToFunctionStart
1001971C: push &DAT_1008A0B4
10019721: push 0x10019E40
10019726: push &DAT_1008A0B0
1001972B: mov  [DAT_1008A0B0], eax
10019730: call Hook::CreateHook
10019738: mov  byte ptr [DAT_1008A0B8], 1
1001973F: ret
```

The called helpers were decompiled and identified as:

- `0x100213A0 = GW::Scanner::FindAssertion`
- `0x10021B40 = GW::Scanner::ToFunctionStart`
- `0x1001A4D0 = GW::Hook::CreateHook`
- `0x10019E40 = unnamed game-thread detour shim`

And that detour shim decompiles as:

```cpp
void FUN_10019e40(undefined4 param_1, undefined4 param_2)
{
    GW::Hook::EnterHook();
    FUN_10019b00();
    (*DAT_1008a0b4)(param_1, param_2);
    GW::Hook::LeaveHook();
}
```

So slot `+0x08` is the real **game-thread bootstrap/init** callback:

1. enter the subsystem critical section
2. use scanner assertions to find the underlying game thread target
3. normalize it to the function start
4. create a detour using `0x10019E40`
5. store:
   - hook handle at `DAT_1008A0B0`
   - original/replay pointer at `DAT_1008A0B4`
6. set initialized flag `DAT_1008A0B8 = 1`

This is stronger than the earlier bootstrap note because it proves `GameThreadModule` owns a real detour installation path, not just queue state.

## Slot `+0x14` = `0x10019740`

Again, Ghidra did not auto-name it as a function in the fresh import, but the body is short and clean:

```text
10019740: if DAT_1008A0B8 == 0 -> ret
10019749: EnterCriticalSection(&DAT_1008A098)
10019754: push DAT_1008A0B0
1001975A: call Hook::DisableHooks
10019762: LeaveCriticalSection(&DAT_1008A098)
1001976D: ret
```

This is the **post-hook disable** counterpart to `EnableHooks`.

So the two post-bootstrap lifecycle callbacks in the module record are:

- `+0x10`: enable this module’s hook root
- `+0x14`: disable this module’s hook root

That lines up with the general GWCA module model seen elsewhere.

## Slot `+0x0C` = `0x10019770`

This one is the full teardown path, and the longer disassembly removes most ambiguity:

```text
10019770: if DAT_1008A0B8 == 0 -> ret
10019779: EnterCriticalSection(&DAT_1008A098)

10019784: if DAT_1008A0B8 != 0:
10019798:   push DAT_1008A0B0
1001979E:   call Hook::DisableHooks
100197A6:   LeaveCriticalSection(&DAT_1008A098)

100197B1: call GW::GameThread::ClearCalls
100197B6: push DAT_1008A0B0
100197BC: call Hook::RemoveHook
100197C4: DAT_1008A0B8 = 0
100197CB: LeaveCriticalSection(&DAT_1008A098)
100197D6: DeleteCriticalSection(&DAT_1008A098)
100197E1: ret
```

The named callees here are:

- `0x1001A530 = GW::Hook::DisableHooks`
- `0x1001A590 = GW::Hook::RemoveHook`
- `0x10019CB0 = GW::GameThread::ClearCalls`

So slot `+0x0C` is the **full shutdown / destroy** callback:

1. acquire lock
2. disable the hook if still active
3. clear all queued and persistent game-thread calls
4. remove the hook record entirely
5. clear initialized flag
6. leave and then destroy the critical section

This is notably stronger than a “simple disable” path. It is a true subsystem teardown.

## Final mapping of the `GameThreadModule` slots

With this pass, the four concrete lifecycle slots are best understood as:

| Module offset | Address | Recovered role |
|---|---:|---|
| `+0x08` | `0x100196F0` | initialize scanner/hook state and create the game-thread detour |
| `+0x0C` | `0x10019770` | full shutdown: disable, clear calls, remove hook, destroy lock |
| `+0x10` | `0x100196C0` | enable installed hook root |
| `+0x14` | `0x10019740` | disable installed hook root |

So `GameThreadModule` is now fully consistent with GWCA’s wider module lifecycle:

- construction/init
- full teardown
- enable
- disable

## What this means architecturally

This closes an important gap in the earlier game-thread work.

The compiled build now shows that `GameThreadModule` is not merely:

- a deferred lambda queue
- a persistent callback vector
- a helper namespace used by UI/input code

It is a real first-class GWCA instrumentation subsystem with:

- scanner-driven target discovery
- a dedicated detour shim
- hook handle/original replay storage
- lock-protected lifecycle management
- full reset semantics

That puts it in the same category as the more obvious hook-bearing modules like the UI subsystem.

## Confidence and caveat

One caveat from the tooling side:

- in a fresh headless import, Ghidra auto-recognized `0x100196C0` as a function
- but did not auto-promote `0x100196F0`, `0x10019740`, or `0x10019770`

So for those three slots, the conclusions above are based on:

- direct instruction stream recovery
- named callee identification
- control-flow shape

not on a decompiler-produced C body for the outer wrapper itself.

That said, the confidence is still high because the bodies are short, linear, and heavily anchored by already-decompiled named callees.

## Supporting artifacts

- [decomp_10018e90.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10018e90.log)
- [decomp_gamethread_module_slots_temp46.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gamethread_module_slots_temp46.log)
- [disasm_100196c0_temp42.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_100196c0_temp42.log)
- [disasm_100196f0_temp43.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_100196f0_temp43.log)
- [disasm_10019740_temp44.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10019740_temp44.log)
- [disasm_10019770_long_temp47.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10019770_long_temp47.log)
- [decomp_gamethread_slot_calls_temp48.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gamethread_slot_calls_temp48.log)
- [decomp_1001a4d0.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001a4d0.log)
- [decomp_1001a530_deep.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001a530_deep.log)
- [decomp_1001a540_deep.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001a540_deep.log)
