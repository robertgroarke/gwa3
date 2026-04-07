## `Gw.exe` Frame Callback Subsystem Identity Addendum

This pass continues the helper-cluster work one step deeper and answers a more concrete question:

- what *kind* of engine subsystems are actually being advanced under the callback GWCA hooks for `GameThread`?

The highest-signal children recovered here were:

- `FUN_00615E90`
- `FUN_0061AD80`
- `FUN_00617AC0`
- `FUN_00617EC0`
- `FUN_00617150`
- `FUN_00620C40`

The key result is that the callback cluster now separates into three recognizable domains:

1. frame-cache / viewport commit work
2. a context-driven movement-or-focus update path
3. a small hierarchy-aware fade / selection / highlight cluster

## Decompiled helper bundle

All of the function bodies below come from:

- [gw_decomp_callback_helpers_temp59.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_helpers_temp59.log)

## `FUN_00615E90`: frame-cache / viewport work

This is the clearest subsystem-identity win in the whole batch.

The function iterates a table rooted at:

- `DAT_00BB54E8`
- with `DAT_00BB54F0` entries

and switches on a per-entry type:

- `case 0`
- `case 1`
- `case 2`
- `case 3`

The strongest markers are the logged error strings:

- `"FrCache: ignored invalid client viewport rect (%0.6f,%0.6f,%0.6f,%0.6f)"`
- `"FrCache: ignored invalid frame viewport rect (%0.6f,%0.6f,%0.6f,%0.6f)"`

That makes the domain explicit: this is **frame cache / viewport rect maintenance**.

Its behavior by case is roughly:

- `case 0`
  - dispatches `FUN_006327E0(...)` against a table-backed target
- `case 1`
  - checks another indexed table and then calls `FUN_0062E640()` / `FUN_006286D0()`
- `case 2`
  - fetches a client viewport rect
  - validates it
  - commits it through `FUN_00634990(0)`
- `case 3`
  - fetches a frame viewport rect
  - validates it
  - commits it through the same `FUN_00634990(0)`

Then at the end it emits one more default/full-viewport style commit:

```cpp
local_24 = 0;
local_20 = 0;
local_1c = 0x3f800000;
local_18 = 0x3f800000;
FUN_00634990();
```

So the parent frame callback is definitely advancing a real frame/layout cache path, not merely gameplay state.

That matters because it helps explain why this callback is attractive to GWCA for “safe” deferred execution:

- it is already in the engine’s frame/UI maintenance phase

## `FUN_0061AD80`: context-driven smooth update path

This one is less immediately labelable, but it has a very strong shape.

Its input is:

- the forwarded context-derived integer from `FUN_0061DEC0(param_1)`

The function converts that integer into seconds:

```cpp
local_10 = (float)param_1;
local_10 = local_10 / 1000.0;
```

Then it drives a more elaborate motion/update path with:

- `FUN_0061B9F0(local_10, &DAT_00BD02C8)`
- direction/quadrant-style handling via `_DAT_00BB55CC` and `_DAT_00BB55D0`
- fallback branches through:
  - `FUN_0061D7B0(...)`
  - `FUN_00624EE0()`
  - `FUN_00624E90()`
  - `FUN_00624E80()`
  - `FUN_0061DCD0(...)`

The most distinctive part is that it computes deltas, scales them, clamps them against bounds, and then commits normalized ratios:

- `local_28 / local_30`
- `local_24 / local_2c`

through:

- `FUN_00624F40(&local_14)`
- `FUN_00624F80(&local_38)`

It also fetches something through:

- `FUN_0060E5A0(DAT_00BD0998, &local_28, 0, &local_30, 0)`

and later calls:

- `FUN_006108E0(DAT_00BD0998, &local_14)`

So the best current interpretation is:

- a smooth movement / focus / reticle / viewport-position style updater
- driven by time
- using normalized coordinates and bounds

I am deliberately keeping that phrasing cautious, because while the math and normalization behavior are clear, the exact object being moved is not yet named by strings.

Still, it is definitely not a generic renderer helper. It is a specific, time-based spatial update routine.

## `FUN_00617AC0`, `FUN_00617EC0`, and `FUN_00617150`: hierarchy-aware fade/highlight cluster

These three helpers tie together tightly and appear to belong to the same subsystem.

### `FUN_00617150`

This is the simplest one:

- it walks upward through a hierarchy
- multiplies a running factor by `*(node + 0x30)`
- clamps the result to `[0.0, 1.0]`

That is a classic hierarchical-opacity / inherited-scale style computation.

### `FUN_00617EC0`

This one consumes a scalar and fans it across a hierarchy.

It:

- converts the input scalar into an 8-bit value:
  - `local_c = input * 255.0`
- toggles engine state bits through:
  - `FUN_0062E640(0x20000)`
  - `FUN_0062E5D0(...)`
- either:
  - iterates a local hierarchy and applies `FUN_006446B0(*item, local_c)`
  - or emits:
    - `FUN_006286D0(0x23, &param_1, 0)`
    - resolves an object via `FUN_006290B0(...)`
- then recursively walks a broader hierarchy and reapplies itself to children

That is extremely characteristic of a:

- fade
- alpha/intensity propagation
- highlight strength propagation

style helper.

### `FUN_00617AC0`

This one is smaller but fits the same family:

- if a count at `+0x24` is above `7`
- it iterates an array and calls:
  - `FUN_00645410(*item, param_1)`

That reads like a sibling “apply state to child list” helper in the same hierarchy/fade domain.

### Why this matters

These three helpers are called indirectly from the timed scheduler in `FUN_00616CE0`, which means at least part of the parent callback’s timed work is:

- visual hierarchy state propagation
- likely fades/highlights/selection emphasis

not just gameplay timers.

## `FUN_00620C40`: selection / focus toggle helper

This function is another strong subsystem clue.

It manages a global active object:

- `DAT_00BD0BE4`

and toggles engine state around it via:

- `FUN_0062E5D0(0, 0x20, 0)`
- `FUN_006286D0(0x21, 0, 0)`
- and on activation:
  - `FUN_0062E5D0(0x20, 0, 0)`
  - `FUN_006286D0(0x21, 1, 0)`

It also resolves by object field `+0xBC` through:

- `FUN_006290B0(iVar1)`

That pattern looks much more like:

- focus / active target / selected object state

than generic rendering.

This is especially interesting because `FUN_00616CE0` can call `FUN_00620C40(...)` when a timed entry completes and its flags indicate a certain mode. So the timed scheduler is capable of finishing by changing active/selected/focused state.

## What this says about the parent callback as a whole

Putting this together with the previous pass:

- `FUN_006117E0` is a frame/update dispatcher
- its children are not random
- they cover at least:
  - frame cache / viewport commit
  - smooth spatial update
  - hierarchy fade/highlight propagation
  - active/focus state toggling

So the callback GWCA hooks sits right in the middle of the game’s:

- frame/UI maintenance
- visual transition
- and state-commit layer

That is exactly the sort of place where “do this safely next frame” becomes meaningful.

This is now a much sharper statement than the earlier generic “GWCA runs on the game thread.”

The more precise version is:

- GWCA drains its deferred queue at the front of a real frame/update dispatcher that already owns viewport commits, visual transitions, and active-state propagation.

## Strongest current conclusion

After this pass, the best model is:

1. `GameThreadModule` hooks a frame/update dispatcher in `FrApi.cpp`
2. that dispatcher is above multiple concrete engine domains
3. at least three of those domains are:
   - frame cache / viewport management
   - smooth bounded movement/focus update
   - hierarchy fade/highlight/selection state
4. GWCA uses this callback as its safe deferred-execution surface

That makes the name `GameThread` slightly misleading but now fully explainable:

- it is not about threads
- it is about “safe execution during the engine’s per-frame UI/update phase”

## Best next reverse step

The next logical step is to tighten naming on the two still-inferred domains:

- the exact object/domain behind `FUN_0061AD80`
- the exact object/domain behind the fade/highlight cluster

The best candidates for the next pass are:

- `FUN_00624F40`
- `FUN_00624F80`
- `FUN_006108E0`
- `FUN_006446B0`
- `FUN_00645410`
- `FUN_006290B0`

Those should tell us whether the “smooth spatial update” is:

- camera
- cursor/reticle
- viewport anchor
- frame focus

and whether the hierarchy cluster is:

- alpha fading
- selection highlighting
- or another visual emphasis system

## Supporting artifacts

- [GWCA_GameThread_HelperCluster_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_GameThread_HelperCluster_Addendum.md)
- [gw_decomp_callback_helpers_temp59.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_helpers_temp59.log)
