## `Gw.exe` Frame Callback Helper Cluster Addendum

This pass continues directly from the recovered game callback:

- `FUN_006117E0`

which GWCA reaches via:

- `FindAssertion("FrApi.cpp", "renderElapsed >= 0")`

The next question was:

- what do the key helpers under that callback actually do?

The short answer is that the callback is not one monolithic render function. It is a dispatcher over several distinct time-driven subsystems:

- a context-driven expiry/update walker
- a larger float-time scheduler/animation walker
- a couple of state/event flush helpers
- lightweight event emission wrappers

## Decompiled helper set

The helper bundle was decompiled from the matching game image:

- [gw_decomp_callback_helpers_temp58.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_helpers_temp58.log)

The functions recovered were:

- `FUN_0061DEC0`
- `FUN_006193B0`
- `FUN_00634B40`
- `FUN_006287D0`
- `FUN_00615E20`
- `FUN_00618DD0`
- `FUN_00616CE0`

## The most important helper: `FUN_00616CE0(float dt)`

This is the clearest evidence that the callback cluster is driven by elapsed time and not just one raw render pass.

`FUN_00616CE0(float param_1)`:

- takes the clamped elapsed value from the parent callback
- repeatedly processes several global intrusive-list style registries
- advances timers by `param_1`
- computes interpolated values from:
  - start value
  - end value
  - current elapsed
  - total duration
- writes current interpolated state into `pfVar2[0xb]`
- removes completed entries from their list(s)
- toggles a half-second cadence state via `_DAT_00BD01B4`

The most telling behaviors are:

1. a half-second toggle:
   - `_DAT_00BD01B4 += dt`
   - when it exceeds `0.5`, it flips `DAT_00BD01B8`
   - and optionally calls `FUN_00617AC0(...)`

2. a major timed-entry list:
   - entries track total duration, elapsed, and value ranges
   - current value is interpolated each frame
   - a derived float is written into `entry[0xb]`
   - completed entries are removed

3. a second short-lived timed list:
   - entries accumulate elapsed time
   - a threshold around `0.25` triggers cleanup/removal

That makes this function best understood as a general:

- frame-time scheduler
- interpolation/animation updater
- transient timer cleanup pass

inside the larger frame callback.

This is a meaningful refinement of the previous model. GWCA is not just hitching onto some “render loop” in the abstract. It is hooking a callback that definitely runs time-based engine maintenance work.

## `FUN_0061DEC0(int context_value)`

This helper is the strongest context-driven pass in the cluster.

It:

- stamps `DAT_00BD09B0` from `FUN_0046B290()`
- walks a registry rooted at `DAT_00BD02B0`
- adds `param_1` into `entry[3]`
- removes entries once they pass `0xF9`
- records one entry field into `DAT_00BD08D0[...]`
- optionally calls `FUN_0061FCB0(iVar1, 0)` on removal
- ends by calling `FUN_0061AD80(param_1)`

This does **not** look like a pure render helper.

It looks more like:

- context-lifetime tracking
- expiration/retirement of registered objects
- and a follow-up subsystem tick keyed by the forwarded callback context value

The important connection to the parent callback is:

- `FUN_006117E0` calls `FUN_0061DEC0(*param_2)`

So the second callback argument preserved by GWCA is feeding a real game-side update pass, not just being forwarded passively.

## `FUN_00615E20(dt)`

This helper is a compact gate-and-dispatch wrapper around another time-based subsystem:

- if `DAT_00BB54C0 == 0`
  - it calls initialization/reset style helpers
  - clears several globals
- otherwise, if `DAT_00BB54F0 != 0`, it skips the init path
- then it always calls:
  - `FUN_00615E90(dt)`

So this one is not the heavy logic itself. It is a mode gate that ensures a subsystem is initialized, then advances it with the frame delta.

That makes it another good sign that `FUN_006117E0` is above several specialized per-frame subsystems rather than being a single-purpose render routine.

## `FUN_00618DD0()`

This one is tiny but useful:

```cpp
if ((-1 < DAT_00BD029C) && (DAT_00BD02A4 != 0)) {
    FUN_006327E0(0, 1, &DAT_00BD02A4, 1);
}
```

So it is essentially:

- a conditional event/state flush
- gated on two globals

It looks like a narrow auxiliary commit step rather than a core update loop.

## `FUN_006193B0()`

This helper is another small state-commit/cleanup pass:

- gated on `DAT_00BD02A4 == 0` and `DAT_00BD029C >= 0`
- asserts a pointer at `DAT_00BD028C + 0x0C`
- if `*(base + 0x10) != 0`, it calls:
  - `FUN_00634020(0, *(base + 0x0C), base)`
- then clears `*(base + 0x10)`

That pattern looks like:

- “if there is a pending dirty/update flag, flush it once and clear the flag”

So again, `FUN_006117E0` appears to orchestrate multiple subsystem-maintenance passes, not one self-contained renderer.

## `FUN_00634B40(id_or_default)`

This helper is not time-driven. It looks like an object/state fetch and flush helper:

- optionally resolves an object by id/tag `0x67726476`
- validates object type
- checks fields around `+0x30` and `+0x1E4`
- emits a small packet/object if pending state exists
- calls `FUN_00634FC0()`
- clears `+0x1E4` on success

This makes it look like:

- a queued state submission / flush path

The parent callback calls it with:

- `FUN_00634B40(0)`

which strongly suggests “flush the default/current object state if needed”.

## `FUN_006287D0(code, payload, aux)`

This is a tiny event wrapper:

```cpp
iVar1 = FUN_0048F830(&param_1);
if (iVar1 != 0) {
    FUN_00628570(param_2, param_3);
}
```

In the parent callback, it is used with codes:

- `0x44`
- `0x52`
- `0x53`

So `FUN_006117E0` is clearly bracketing its inner update work with internal event/message notifications. This is another sign we are inside a dispatcher-style engine callback.

## What the helper cluster says about the parent callback

Putting the cluster together:

- `FUN_006117E0` gates on elapsed-time validity
- normalizes/clamps the frame delta
- then fans out into:
  - event emission
  - time-driven scheduler/interpolator updates
  - context-driven lifetime/expiry processing
  - state flushes/commits
  - optional capture/screenshot handling

That means the best current reading of the parent callback is:

- not “the renderer”
- not merely “a thread tick”
- but a **frame/update dispatcher over several engine subsystems**

This helps explain why GWCA chose this seam for `GameThread`:

- it is frequent
- it is clearly in the safe engine update context
- and it is already where the game itself processes several kinds of queued/temporal work

## Strongest refined conclusion

After this pass, the strongest model is:

1. GWCA hooks a real `FrApi.cpp` frame/update callback
2. that callback is a dispatcher, not a leaf function
3. the dispatcher advances multiple internal subsystems each frame
4. GWCA drains its own deferred queue at the front of that callback, then replays the original dispatcher

So GWCA’s `GameThread` abstraction is effectively:

- “run this before the engine’s ordinary per-frame maintenance work”

That is a sharper and more useful statement than the earlier generic “safe game-thread callback”.

## Best next step

The next logical reverse step is to keep tracing the highest-signal children:

- `FUN_00615E90(dt)` from the gated wrapper `FUN_00615E20`
- `FUN_0061AD80(context)` from `FUN_0061DEC0`
- `FUN_00617AC0(...)` / `FUN_00617EC0(...)` / `FUN_00617150()` from the timed scheduler in `FUN_00616CE0`

Those should tell us what concrete engine domains these timed registries belong to:

- animation
- UI transitions
- effect timers
- entity state expiry
- or some mix of them

## Supporting artifacts

- [GWCA_GameThread_GwCallback_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_GameThread_GwCallback_Addendum.md)
- [gw_decomp_callback_helpers_temp58.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_helpers_temp58.log)
- [gw_disasm_00616ce0_temp55.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_disasm_00616ce0_temp55.log)
