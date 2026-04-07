## `Gw.exe` Frame Callback Setup Flow Builders Addendum

This pass continued the producer search by decompiling the earlier setup helpers that the `CtlInstance` constructors hand off to:

- `FUN_0052D970`
- `FUN_0052DD20`
- `FUN_00526A50`

The goal was to move one step closer to the code that actually assembles the region table before interaction begins.

## Source artifacts

These results come from:

- [gw_decomp_region_builders_temp106.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_builders_temp106.log)
- [gw_decomp_ctlinstance_cluster_temp104.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_ctlinstance_cluster_temp104.log)
- [gw_decomp_005eff80_temp105.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_005eff80_temp105.log)

## High-level result

This pass still did not recover the raw child-record allocator/filler behind `in_ECX[2]`, but it did finally expose the setup flows that sit immediately above that layer.

The strongest new result is:

- these setup helpers create and measure concrete child controls through repeated `FUN_0060D300(...)`, `FUN_0060E2B0(...)`, `FUN_0060FAC0(...)`, and `FUN_0060E4C0(...)` calls
- they are clearly assembling the control tree that the later region-table logic consumes

So we are now much closer to the real producer seam, even if one more layer is still missing.

## `FUN_0052D970(data)`: control-specific child setup over slots `4/5/6`

This function is the most useful result of the pass.

It begins by creating three child controls under the current owner:

- slot `4` via:
  - `FUN_0060D300(owner, 0x80300, 4, FUN_005EA9A0, 0, 0)`
- slot `5` via:
  - `FUN_0060D300(owner, 0x80300, 5, FUN_005EA9A0, 0, 0)`
- slot `6` via:
  - `FUN_0060D300(owner, 0x300, 6, FUN_005EA9A0, 0, 0)`

and then configures those children through:

- `FUN_00610000(...)`
- `FUN_00610580(...)`
- `FUN_005EB2A0(...)`
- `FUN_006107B0(...)`

After that, it switches on the input mode and fills the children with real content.

The important point is structural:

- this is definitely part of the pre-interaction tree assembly

It is not the raw region-table builder yet, but it is one of the clearest builders above it.

### Case-specific meanings inside `FUN_0052D970(...)`

Several switch cases are especially useful:

- case `1`
  - populates slots `4/5/6`
  - creates another owner child with:
    - `FUN_0060D300(owner, 0x2000, 0, FUN_005F3550, &payload, 0)`
- case `2`
  - populates slots `4/5/6`
  - creates:
    - `FUN_0060D300(owner, 0x2000, 3, &LAB_00872A00, *obj, 0)`
- case `3`
  - populates slots `4/5/6`
  - creates:
    - `FUN_0060D300(owner, 0, 2, &LAB_0089AE80, 0, 0)`
  - then sends:
    - `FUN_00610160(child, 0x67, *obj, 0)`

That tells us these setup helpers are not merely tweaking existing nodes.
They are creating new owner-attached children with distinct type ids and callbacks.

So the likely route to the real region-record metadata is:

- inside the callbacks and helper chains attached to these newly created children

not inside the tiny `CtlInstance` bootstraps we looked at earlier.

## `FUN_0052DD20(size_request)`: measurement combiner over slots `4/5/6`

This helper is a measurement/setup consumer, not a record producer, but it is still valuable.

It:

- measures slot `4`
- measures slot `5`
- measures slot `6`
- combines those widths/heights into a final required size
- also consults:
  - `FUN_0060E3D0(owner)`
  - `FUN_0052D310(...)`

So this is effectively:

- higher-level layout size combiner for the constructed slot `4/5/6` subtree

That reinforces the current model:

- `FUN_0052D970(...)` creates/configures the subtree
- `FUN_0052DD20(...)` measures it afterward

So the control-building and control-measurement phases are clearly separated.

## `FUN_00526A50(mode)`: active child selector / replacer

This helper is another important structural clue.

It:

- compares requested mode against `instance + 0x20`
- if the mode changed and `instance + 0x24 != 0`, it refreshes the old active child through:
  - `FUN_0060D890(instance + 0x24)`
- stores the new mode
- then walks a local table of `{mode, callback, type}` triples
- when it finds a match, creates a new active child:
  - `FUN_0060D300(owner, 0x80, mode, callback, 0, 0)`
- stores that created child id at:
  - `instance + 0x24`
- and emits:
  - `FUN_00610160(owner, 0x7FFFFFFE, &type, 0)`

This is very useful for the ongoing producer search.

It means:

- `instance + 0x24` here is an active child id selector slot
- and different modes cause the helper to swap in different owner-attached children with different callbacks

So this helper sits one level above the actual child implementation callbacks that may be producing the region-table records.

### Why this matters

This is the first clear setup path we have that:

- selects a child mode
- creates a new owner child with `FUN_0060D300(...)`
- keeps its id around for later refresh/swap

That is extremely close to a true producer seam.

It does not yet show the internal record writes at child-record `+0x24/+0x28`, but it gives us concrete callback entrypoints to target next.

## What this says about the producer search

After this pass, the most likely producer path is no longer:

- generic `CtlInstance` setup

It is now:

- specific callback families passed into `FUN_0060D300(...)` from setup builders like:
  - `FUN_005EA9A0`
  - `FUN_005F3550`
  - `LAB_00872A00`
  - `LAB_0089AE80`
  - and the mode table callbacks inside `FUN_00526A50(...)`

That is a much sharper target set.

## Updated working model

The cleanest current model is now:

- generic `CtlInstance` substrate
  - owner id
  - interaction state
  - manager shell
- setup-flow builders
  - create concrete owner children via `FUN_0060D300(...)`
  - configure and measure subtrees through slots `4/5/6`
  - swap active child modes and callbacks
- lower callback layer
  - likely where the actual region-table record metadata is written

So we now know the raw builder is probably:

- not a tiny initializer
- not an interaction-time walker
- but a callback attached to one of these setup-created children

## Best next step

The next best step is to follow the setup-created callbacks directly, starting with:

- `FUN_005EA9A0`
- `FUN_005F3550`

Those are now the strongest concrete candidates for code that either:

- allocates/fills the region child-record array
- or delegates into the helper that does

