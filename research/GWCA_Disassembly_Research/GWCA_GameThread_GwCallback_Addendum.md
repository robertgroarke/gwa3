## GWCA `GameThread` Underlying `Gw.exe` Callback Addendum

This pass takes the next logical reverse step after identifying `GameThreadModule`’s scan target strings:

- `FrApi.cpp`
- `renderElapsed >= 0`

The goal was to stop at the real game callback body inside `Gw.exe`, not just the GWCA wrapper around it.

That target now resolves cleanly.

## The assertion xref lands in one concrete game function

Using the matching game image:

- [Gw_livecopy.exe](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\Gw_livecopy.exe)

the string xref pass shows:

- `renderElapsed >= 0` lives at `0x00A20E44`
- it is referenced from:
  - `0x006118E6`
  - function `FUN_006117E0`

The relevant xref log is:

- [gw_renderelapsed_xrefs_temp49d.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_renderelapsed_xrefs_temp49d.log)

The source-file string xref pass also independently points back into the same function:

- `P:\Code\Engine\Frame\FrApi.cpp` lives at `0x00A20D94`
- `FUN_006117E0` references it multiple times

The relevant log is:

- [gw_frapi_xrefs_temp52b.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_frapi_xrefs_temp52b.log)

So the scan pair GWCA uses:

- `FindAssertion("FrApi.cpp", "renderElapsed >= 0")`

really does converge on:

- `FUN_006117E0`

inside the game image.

## Decompiled body of the game callback

Decompiling `0x006117E0` gives:

- [gw_decomp_006117e0_temp51.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_006117e0_temp51.log)

The high-value shape of the function is:

```cpp
void FUN_006117e0(int param_1, undefined4 *param_2)
{
    if (_DAT_00bd00e0 < 0.0) {
        return;
    }

    local_21c = *(int *)(param_1 + 0x10);
    if (local_21c == 0) {
        FUN_00487260(0x237);
    }

    local_224 = _DAT_00bd00e0;
    local_220 = _DAT_00bd00e4;
    FUN_006287d0(0x44, &local_224, 0);
    FUN_0062e940(_DAT_00bd00e4);

    if (DAT_00bd0100 == 0) {
        _DAT_00bd0104 = _DAT_00bd0104 + _DAT_00bd00e0;
        return;
    }

    if (_DAT_00bd0104 == 0.0) {
        local_214 = _DAT_00bd00e0;
    } else {
        local_214 = _DAT_00bd0104;
        _DAT_00bd0104 = 0.0;
    }

    if (0.0 <= local_214) {
        local_218 = 1.0;
        if (local_214 < 1.0) {
            local_218 = local_214;
        }

        FUN_0062c350();
        FUN_006263f0();
        FUN_0061dec0(*param_2);
        FUN_00634b40(0);
        local_214 = local_218;
        FUN_006287d0(0x52, &local_214, 0);
        FUN_00616ce0(local_218);
        FUN_006193b0();

        if (DAT_00bd00ec != 0) {
            local_214 = 0.0;
            FUN_00630ee0(0, 2, &local_214);
            DAT_00bd00ec = 0;
        }

        FUN_00615e20(local_218);
        FUN_00618dd0(local_218);

        if (DAT_00bd00d8 != 0) {
            // screenshot/capture path
            ...
        }

        FUN_006287d0(0x53, 0, 0);
        FUN_00632420(0);
        return;
    }

    FUN_00487260(0x252);
}
```

## What this proves about the callback

This body is strong evidence that the underlying seam is exactly what the name `GameTick` suggested:

- it consumes frame-time / elapsed-time globals
- it checks the non-negativity condition behind `renderElapsed >= 0`
- it accumulates leftover elapsed time when the engine says the update is not active
- it clamps the effective time slice into `[0.0, 1.0]`
- it runs a sequence of update/render-adjacent engine functions
- it emits internal events/notifications before and after the main update body
- it has a screenshot/capture branch

That is not a generic thread procedure.

It is a bona fide engine frame/update callback.

## Why GWCA’s detour signature has two forwarded parameters

GWCA’s detour shim for `GameThreadModule` was:

```cpp
void FUN_10019e40(undefined4 param_1, undefined4 param_2)
{
    GW::Hook::EnterHook();
    FUN_10019b00();
    (*DAT_1008a0b4)(param_1, param_2);
    GW::Hook::LeaveHook();
}
```

Now that we have the game body, that signature makes more sense:

- the hooked callback really does take two arguments
- in the decompiled game body they appear as:
  - `param_1`
  - `param_2`

And the game body uses both:

- `param_1 + 0x10` is dereferenced and asserted non-null
- `*param_2` is passed onward into `FUN_0061DEC0(*param_2)`

So GWCA’s shim is not faking an adapter signature here. It is preserving the real callback ABI of the hooked engine function.

## Strongest semantic interpretation so far

The most useful interpretation of `FUN_006117E0` is:

- a frame/update dispatcher that runs once per engine tick
- gated by current render/update elapsed-time state
- with a pair of caller-supplied context arguments
- and with the engine’s own update pipeline living underneath it

That gives a much clearer meaning to GWCA’s `GameThread` abstraction:

- it is “safe work that runs at the next frame/update callback”

not:

- “work sent to a hidden dedicated worker thread”

## Cross-check against the local workspace

This pass lines up very well with the local naming and experiments:

### Local scan naming

[BotsHub_Stubs.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\lib\custom\BotsHub_Stubs.au3) already registers:

```autoit
AddScanPattern('GameTick', '', '', 'hook', 'P:\Code\Engine\Frame\FrApi.cpp', 'renderElapsed >= 0')
```

That is now directly validated by the game image.

### Earlier char-select notes

[CharSelect_ButtonClick_Research.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\CharSelect_ButtonClick_Research.md) described the same seam as a “GameTick Hook”. That terminology is now strongly supported by the real callback body.

### Why remote-thread UI calls still failed

The old experiments where `CreateRemoteThread`-style UI actuation still failed now read more cleanly:

- GWCA is not making those operations safe by “marshalling to some OS thread”
- it is making them safe by draining work during this engine callback

So if the callback is not firing, or if the user work bypasses that callback path, the presence of GWCA alone will not magically make frame/UI calls safe.

## Most important conclusion from this pass

This is the clearest formulation so far:

1. GWCA’s `GameThreadModule` scans `FrApi.cpp` / `renderElapsed >= 0`
2. that scan resolves to `Gw.exe` function `FUN_006117E0`
3. `FUN_006117E0` is an engine frame/update callback driven by elapsed-time state
4. GWCA detours that callback and drains its deferred queue before replaying the original body

So the underlying system is:

- **game frame callback**

while the GWCA abstraction layered on top is:

- **game-thread-safe deferred execution**

## Best next reverse step

The next logical step is to keep peeling this callback cluster outward:

- decompile the nearby helper calls inside `FUN_006117E0`
- especially the ones that take the clamped elapsed value:
  - `FUN_00616CE0(local_218)`
  - `FUN_00615E20(local_218)`
  - `FUN_00618DD0(local_218)`
- and the context-driven call:
  - `FUN_0061DEC0(*param_2)`

That would tell us whether GWCA is hooking the main render/update function itself or one layer above a smaller dispatch point inside the frame system.

## Supporting artifacts

- [GWCA_GameThreadTarget_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_GameThreadTarget_Addendum.md)
- [gw_renderelapsed_xrefs_temp49d.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_renderelapsed_xrefs_temp49d.log)
- [gw_frapi_xrefs_temp52b.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_frapi_xrefs_temp52b.log)
- [gw_decomp_006117e0_temp51.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_006117e0_temp51.log)
- [Gw_livecopy.exe](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\Gw_livecopy.exe)
