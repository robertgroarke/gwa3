## `Gw.exe` Frame Callback Mode-2 Producer Boundary Addendum

This pass followed the next obvious lead from the mode-`2` mask-family note:

- trace the `CtlInstance` cluster around `FUN_005EF740(...)`
- look for nearby constructors that might initialize the child-record mask field at `+0x24`

The important result is a correction of scope rather than a final producer.

## Source artifacts

These results come from:

- [gw_findcallers_005ef740_temp103.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_005ef740_temp103.log)
- [gw_decomp_ctlinstance_cluster_temp104.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_ctlinstance_cluster_temp104.log)
- [gw_decomp_mode2_callback_temp101.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_mode2_callback_temp101.log)

## High-level result

The `CtlInstance` subobject initialized by `FUN_005EF740(...)` is widely reused across many controls.

That means:

- `FUN_005EF740(...)` is not the narrow mode-`2` producer seam

More importantly, the nearby constructor bodies show a crucial address-space boundary:

- fields at `instance + 0x20` and `instance + 0x24` inside `CtlInstance` are instance-state fields
- they are not the same thing as the child-record metadata fields at record `+0x24` and record `+0x28` used by the region-table layout engine

So the producer search must move away from the `CtlInstance` instance body and back toward the actual child-record builders used by the region table.

## `FUN_005EF740(...)` caller fanout: reused subobject, not special-case seam

The caller search for `FUN_005EF740(...)` came back very broad.

That means this subobject bootstrap is used by many controls and control families, not just the one directional host/region system we have been focused on.

So while the subobject is still relevant to understanding the common `CtlInstance` substrate, it is not a good singular anchor for:

- "where does the mode-`2` region-record mask get initialized?"

## `FUN_0052D3C0(...)`: nearby `CtlInstance` subclass / specialization

This function is clearly another `CtlInstance`-style dispatcher.

Important constructor-side behavior in case `9`:

- allocates `CtlInstance.h` object
- runs `FUN_005EF740()`
- seeds a few instance fields
- calls `FUN_0052D970(...)`
- then calls the first vtable slot

Useful event-side behavior:

- case `0x37` builds a region and places children in slots `4`, `5`, and `6` using:
  - `FUN_0060FAC0(...)`
  - `FUN_0060FE70(...)`
  - `FUN_0060E2B0(...)`
- case `0x45` resolves slot `1` and sends `0x5E`

So this is a real nearby specialization, but the fields it writes in the instance body are not the region-record mask fields we were after.

## `FUN_005267D0(...)`: another nearby `CtlInstance` specialization

This function is also a `CtlInstance`-style dispatcher.

In its constructor case `9`, it does:

- allocate `CtlInstance.h` object
- run `FUN_005EF740()`
- seed:
  - `puVar2[8] = 0x11`
  - `puVar2[9] = 0`
- call `FUN_00526A50(...)`

At first glance this looks tempting because `puVar2[9]` is an object offset `+0x24`.

But that is the key correction:

- this `+0x24` is on the `CtlInstance` object body
- not on the region child-record structure inside the region table

So it does not answer the mode-`2` record-mask producer question directly.

## Why this distinction matters

We now have two different `+0x24` fields in play:

### `CtlInstance` object body `+0x24`

From the earlier interaction-lifecycle work, the `CtlInstance` body uses:

- `+0x20` as active token/state
- `+0x24` as the start of planar motion state

That is interaction state.

### child-record `+0x24`

From `FUN_005F00F0(...)` and `FUN_005F0970(...)`, the region-table child record uses:

- `+0x24` as the mode-`2` mask / record flag field
- `+0x28` as the layout strategy field

That is layout-record metadata.

Those are different structures.

This pass is useful because it makes that boundary explicit and prevents us from conflating them.

## What this means for the mode-`2` producer search

The current best conclusion is:

- the mask-normalization callback is understood
- the mask-family behavior is understood
- but the producer of the child-record mask field is not in the nearby `CtlInstance` instance-state constructors we just checked

So the producer search must shift to one of:

- child-record allocation/build helpers used by the recursive region table
- builder functions that populate the child-record array behind `in_ECX[2]`
- or higher-level control-specific region-table assembly code that creates records before interaction ever starts

## One useful side finding

Even though this pass did not recover the producer, `FUN_0052D3C0(...)` does reinforce the broader structural model:

- the same `CtlInstance` substrate is reused across multiple concrete controls
- those specializations lay out and drive different slot families with the same common interaction and placement primitives

So the directional host/region stack we’ve been tracing is part of a broader reusable control-instance framework, not a one-off UI type.

## Updated working model

The cleanest current model is now:

- `CtlInstance` object body
  - owns active token and interaction state
  - reused across many control specializations
- region child-record table
  - owns layout mode at `+0x28`
  - owns mode-`2` mask/flag field at record `+0x24`
  - is distinct from the `CtlInstance` body

That separation is now important enough to treat as a first-class correction in the research.

## Best next step

The next best step is to stop following `CtlInstance` body constructors and instead target the region-record builders directly.

Best candidates:

- functions that populate the child-record pointer array behind `in_ECX[2]`
- helpers near `FUN_005F00F0(...)` / `FUN_005F0970(...)` that allocate or append child records
- control-specific setup bodies like the `0x37` region-layout builders that may assemble the child-record table before interaction starts

That should finally expose where child-record `+0x24` and `+0x28` are written at creation time.

