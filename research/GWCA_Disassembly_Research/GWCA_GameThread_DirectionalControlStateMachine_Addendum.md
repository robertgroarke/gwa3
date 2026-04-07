## `Gw.exe` Frame Callback Directional Control State Machine Addendum

This pass continues from the axis-semantics note by decompiling the remaining conversion helpers and the second caller of the normalized-motion classifier:

- `FUN_005AEC3B`
- `FUN_005A7290`
- `FUN_0052CC30`

The goal was to answer the next obvious question:

- are the slot `2/3` writes just low-level numeric plumbing?
- or do they sit inside a real interaction state machine that makes the host/region stack look like a live directional control?

## Source artifacts

These results come from:

- [gw_decomp_axis_followups_temp96.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_axis_followups_temp96.log)
- [gw_findcallers_0059a110_temp96.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0059a110_temp96.log)
- [gw_decomp_region_followups_temp95.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_followups_temp95.log)

## High-level result

This pass makes the interaction story much sharper.

The strongest current model is now:

- `FUN_005EDBD0(...)` is a normalized motion / drag classifier
- `FUN_005EF470(...)` uses it to feed slot `2` and slot `3` with rounded signed axis deltas through `0x5B`
- `FUN_0052CC30(...)` shows the same classifier inside a real `CtlInstance` state machine, where directional motion is promoted into owner-local channels `9` and `10`

So the slot `2/3` family is no longer best described as merely:

- "paired directional region controls"

It now looks more specifically like:

- a directional interaction sub-control family under a view/control host

I still would not hard-name it as scrollbars or camera panners yet, but it is clearly an active interaction system rather than passive geometry storage.

## `FUN_005AEC3B(value)`: float rounding pre-step

This helper is small but important.

It calls `__fd_int`-style conversion checks and, when the float is not already in a clean integer-representable state, nudges the value:

- positive values get `+1.0`
- negative values get `-1.0`

before returning the value as an x87 float result.

That makes it best described as:

- float-to-int rounding preparation helper

or more concretely:

- "adjust toward integer boundary before final integer conversion"

So the scalar writes going into slot `2/3` are not raw floats.
They are intentionally coerced into integral step values.

## `FUN_005A7290()`: x87 float-to-int finalizer

This helper consumes the x87 result from `FUN_005AEC3B(...)` and returns an `int`.

Behaviorally, it:

- handles small values by returning `0`
- extracts sign/exponent/mantissa state
- returns the converted integer when representable
- otherwise returns `-0x80000000` on failure/overflow-style cases

So the combined `FUN_0046DB60(...)` path from the previous pass is now clear:

```cpp
FUN_005AEC3B(float_value);
FUN_005A7290();
```

That means `FUN_0046DB60(...)` is effectively:

- rounded float-to-int conversion for directional deltas

This is a meaningful upgrade because it confirms the slot `2/3` writes are integral control steps, not continuous float geometry writes.

## What this does to the `0x5B` interpretation

From the previous pass we knew:

- `0x5B` was a scalar setter

Now we can say more specifically:

- in the slot `2/3` path, `0x5B` receives rounded signed integer directional deltas

So the cleanest current reading of `0x5B` on this family is:

- directional step / offset update verb

not just:

- generic scalar setter

## `FUN_0052CC30(...)`: `CtlInstance` interaction-state dispatcher

This is the biggest result in the pass.

The body is clearly an event/state dispatcher for a control instance object. It references:

- `P:\\Code\\Engine\\Controls\\CtlInstance.h`

and contains create/destroy/update handling for a concrete instance payload object.

Several cases matter for the current research thread.

### Case `9`: create instance and bootstrap interaction state

This branch:

- allocates a control-instance object
- seeds default fields
- initializes drag/state helpers via:
  - `FUN_005EF740()`
  - `FUN_005EDB70(...)`
- binds the instance to the owner/control id
- emits setup messages like:
  - `FUN_00610000(id, 0, -1)`
  - `FUN_00611340(id, 1, 0)`
  - `FUN_00610120(id, 0x45)`

So the interaction stack we are tracing is not abstract.
It lives inside a concrete control-instance implementation.

### Case `0x3D`: acquire active interaction

This branch:

- requires `instance + 0x20 == -1`
- stores the active token/id
- copies a starting position/state bundle
- seeds two state helpers:
  - `FUN_005EDF50(...)`
  - `FUN_005EDB80(...)`
- emits:
  - `FUN_006100A0(owner, 7, &local, 0)`

This looks like:

- begin interaction / capture / activation

That fits extremely well with the earlier extended-channel research where ids `7+` looked like higher-level interaction/control channels.

### Case `0x3E`: active motion update

This branch is the closest analogue to the slot `2/3` host path.

If the active token matches:

- when `instance + 0x3C == 1`, it updates one motion state path with:
  - `FUN_005EDE20(...)`
  - then emits `FUN_006100A0(owner, 9, &local, 0)`
- when `instance + 0x3C == 2`, it calls:
  - `FUN_005EDBD0(instance + 0x60, instance + 0x48, param_2 + 2)`
  - then emits `FUN_006100A0(owner, 10, &local, 0)`
  - using the scaled values:
    - `*(instance + 0x84) * *(instance + 0x7C)`
    - `*(instance + 0x88) * *(instance + 0x80)`

That is the crucial link.

It proves the normalized-motion classifier is part of a larger interaction state machine with:

- acquisition phase
- live motion update phase
- owner-local channel emission

So the host/region stack is participating in a real control-interaction protocol, not just local widget bookkeeping.

### Case `0x45`: completion / release-like path

This branch:

- checks another state predicate via `FUN_005EDEA0(...)`
- if it advances to mode `2`, emits:
  - `FUN_006100A0(owner, 9, &local, 0)`

This looks like a completion, release, or re-entry path tied to the same interaction family.

I would still label the exact lifecycle names as provisional, but the interaction structure is now obvious.

## What channels `7`, `9`, and `10` now look like

This pass does not fully rename them, but it narrows them a lot:

- channel `7`
  - interaction acquisition / capture begin
- channel `9`
  - one motion/update family
- channel `10`
  - normalized directional motion/update family

That fits neatly with the earlier extended-channel notes and gives concrete meaning to at least part of the `>= 7` protocol space.

## Updated reading of the host/region path

Combining this pass with the previous two:

- slot `1` host owns an interaction path
- slots `2` and `3` receive integral signed directional deltas through `0x5B`
- the same motion-classifier machinery also feeds owner-local channels `9` and `10` in `CtlInstance`

So the slot `2/3` controls now look less like generic stored axes and more like:

- instance-backed directional control targets
- under a host that participates in a richer capture / drag / update protocol

## One useful limit on the current conclusion

I still would not collapse this to a specific UI name like:

- scrollbar pair
- minimap panner
- camera drag control

because this pass only proves:

- directional motion classification
- integer step conversion
- slot-specific signed updates
- owner-local interaction channels

That is enough to say "directional interaction control family," but not enough yet to pick the final user-facing control name.

## `FUN_0059A110(...)` call spread

The caller search for `FUN_0059A110(...)` returned a very wide spread across the image, including:

- `FUN_005EE080(...)`
- `FUN_005EF470(...)`
- many unrelated control and engine paths

That tells us something useful even without decompiling all of them:

- `0x5B` is not unique to this one host family
- it is a broader engine/control scalar-update verb

So the semantics of `0x5B` still depend on control family context, which matches the broader opcode-reuse pattern already established in the GWCA UI/frame work.

## Updated working model

The cleanest current model is now:

- slot `1`
  - view/control host
  - owns a live interaction state machine
- slot `2`
  - first directional target
  - accepts rounded signed integer updates via `0x5B`
- slot `3`
  - second directional target
  - accepts rounded signed integer updates via `0x5B`
- owner-local channels `7`, `9`, `10`
  - capture / directional update family for the same interaction stack

That is a stronger and more behaviorally grounded model than any of the earlier notes.

## Best next step

The next best step is to name the remaining state-machine helpers:

- `FUN_005EDF50`
- `FUN_005EDB80`
- `FUN_005EDE20`
- `FUN_005EDEA0`
- `FUN_005EFFC0`
- `FUN_0052D190`

That should let us turn the current provisional lifecycle:

- capture
- update
- completion

into a properly named control-interaction state machine.

