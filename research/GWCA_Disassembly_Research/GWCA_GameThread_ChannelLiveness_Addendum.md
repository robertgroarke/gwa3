## `Gw.exe` Frame Callback Channel Liveness Addendum

This pass continues from the extended-channel note by answering two remaining structural questions:

1. is the generic `type_id >= 7` entrypoint actually used in this build?
2. are the lower unresolved channels, especially `0` and `5`, real live paths or just resolver scaffolding?

The main targets were:

- callers of `FUN_006100A0`
- callers of `FUN_0062D220`
- a small batch of representative `FUN_0062D220` callers:
  - `FUN_0062A890`
  - `FUN_0061B8B0`
  - `FUN_00615700`
  - `FUN_006177D0`

## Source artifacts

These results come from:

- [gw_findcallers_006100a0_temp74.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_006100a0_temp74.log)
- [gw_findcallers_0062d220_temp74.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0062d220_temp74.log)
- [gw_decomp_typed_lookup_callers_temp75.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_typed_lookup_callers_temp75.log)

## High-level result

This pass closes an important ambiguity.

The strongest new points are:

- the generic `type_id >= 7` entrypoint is definitely live in this build
  - [gw_findcallers_006100a0_temp74.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_006100a0_temp74.log) reports `TOTAL_REFS=679`
- channel `0` is live
- channel `5` is live
- and channels `1` and `5` form a real traversal/iteration family in multiple places

So the owner-local channel map now clearly includes more than:

- `1`
- `2`
- `3`
- `4`
- `6`

The previously unresolved lower channels are not dead scaffolding.

## `FUN_006100A0(...)`: extended channels are genuinely used

We already knew from the earlier decompile that `FUN_006100A0(owner_id, type_id, arg3, arg4)`:

- requires `owner_id != 0`
- requires `type_id >= 7`
- validates the owner
- calls `FUN_00628A10(type_id, arg3, arg4)`

The missing question was whether anything actually uses it.

The caller map answers that very strongly:

- `TOTAL_REFS=679`

That is far too many for dormant scaffolding.

So the safest current conclusion is:

- the extended owner-local channel range `>= 7` is genuinely live in this build

We do not yet know what those higher channels mean, but we now know they are not hypothetical.

## `FUN_0062D220(...)` caller map: lower channels are also live

The caller map for `FUN_0062D220(...)` is also broader than the earlier channel notes implied:

- `TOTAL_REFS=25`

and includes, among others:

- `FUN_0060D890`
- `FUN_0060BE80`
- `FUN_0060DAA0`
- `FUN_0062A890`
- `FUN_0061B8B0`
- `FUN_00615700`
- `FUN_006177D0`

That made it worth decompiling a few representative callers, and those bodies show the missing lower channels are real.

## Channel `5` is live: sibling/iterator traversal under channel `1`

Two of the new decompiled callers show the same pattern very clearly.

### `FUN_0062A890()`

This function:

- clears owner state with:
  - `FUN_0062E5D0(0, 0x800, 0)`
- zeros several coordinate/rect fields
- queues itself into a registry/list
- then iterates:
  - `for (i = FUN_0062D220(1, 0); i != 0; i = FUN_0062D220(5, i))`
- recursively calling itself for each result

### `FUN_006177D0()`

This one also iterates:

- `for (i = FUN_0062D220(1, 0); i != 0; i = FUN_0062D220(5, i))`

and then marks a field at:

- `this + 0x14 = -1`

before queueing/list-linking the object.

### What that means

This is the clearest proof yet that:

- `type_id 1` is not only a veto/preflight channel
- it is also the root of a traversal family
- and `type_id 5` acts as the sibling/next-step iterator within that family

So `type_id 5` is definitely live.

The safest current interpretation is:

- **channel `1` = root/current traversal head for one owner-local family**
- **channel `5` = iterate/follow next member in that same family**

That does not contradict the earlier preflight reading. It suggests that channel `1` is a concrete family head that is also the family used by the preflight path.

## Channel `0` is live: root traversal over a broader relation family

`FUN_00615700(...)` gives us the strongest live use of channel `0`.

Its behavior is:

- check some global/category flags through `FUN_0062E640(...)`
- append the incoming node/value into a global vector
- then iterate:
  - `for (i = FUN_0062D220(0, 0); i != 0; i = FUN_0062D250(2, i, &local_8))`
- skip owners with certain state bits at `owner + 400`
- recurse into itself for each traversed owner
- update an output/high-water pointer

This is enough to say:

- `type_id 0` is definitely live
- it behaves like a broader root traversal channel

It also suggests an important distinction from the `1 -> 5` family:

- channel `0` is a root/forest traversal over a broader owner family
- channel `1` plus `5` is a more specific linked/sibling traversal family

So the lower channels are not arbitrary leftovers. They already look structurally different.

## `FUN_0061B8B0(...)`: channel `1 -> 5` used for nearest/live owner selection

This helper is another useful proof that the `1 -> 5` family is live and semantically important.

It:

- starts from `FUN_0062D220(1, 0)`
- iterates via `FUN_0062D220(5, current)`
- inspects owner state bits at:
  - `owner + 0x58`
- and compares geometric values at:
  - `owner + 0x10C`
  - `owner + 0x110`

to choose the best/nearest candidate.

That means the `1 -> 5` traversal family is not just for cleanup or bookkeeping. It is also being used for geometric/best-candidate selection.

This reinforces the idea that the lower channels are part of the active relation/selection system, not dead internals.

## Updated live channel map

After this pass, the strongest current live channel family is:

### `type_id 0`

- live
- root traversal over a broader owner/relation family

### `type_id 1`

- live
- root/current traversal head for a more specific owner-local family
- also used by the earlier preflight/veto path

### `type_id 2`

- live
- owner-local setup/enter/initialization

### `type_id 3`

- live
- owner-local activation/rebind/drain

### `type_id 4`

- live
- owner-local relation/layout reevaluation

### `type_id 5`

- live
- iterator/sibling continuation for the `type_id 1` family

### `type_id 6`

- live
- owner-local follow-up for the `0x3d` activation/add message family

### `type_id >= 7`

- definitely live as an exposed extended range
- exact behaviors still unresolved

That is a much fuller and more confident picture than the earlier partial lifecycle map.

## What this changes about the typed-channel model

Before this pass, the most conservative reading was:

- maybe the lower channels are just resolver plumbing
- maybe the interesting behavior starts at `1` or `2`

That is no longer the strongest reading.

After this pass, the stronger model is:

- lower channels `0` and `1` define real root families
- channel `5` is a real traversal/continuation channel tied to `1`
- middle channels `2..4` express lifecycle-style owner behaviors
- channel `6` is a message-coupled follow-up family
- channels `>=7` are exposed and heavily referenced elsewhere

So the typed relation-channel framework is both:

- structurally recursive/traversal-oriented at the low end
- and lifecycle/behavior-oriented in the middle

That is a richer model than a simple enum of state changes.

## Best current interpretation

The strongest safe reading after this pass is:

- the GWCA-hooked frame/relation seam sits above a broad owner-channel framework
- some channels represent owner-family traversal roots/iterators
- some represent owner lifecycle phases
- some are paired to specific global message families
- and an extended range is actively used elsewhere in the binary

That means the reverse-engineered subsystem is now best understood as:

- **a relation-owner graph/channel framework**, not merely a UI event pipeline

## Best next step

The next best reverse step is to stop broadening the map and start deepening one unresolved branch.

The highest-value next targets are:

- callers of `FUN_006100A0(...)` that appear near frame/relation code rather than distant unrelated systems
- `FUN_0062D250(...)`, because `FUN_00615700(...)` uses it alongside channel `0`
- and one or two concrete `FUN_006100A0(...)` callers to see which extended channel ids are actually passed

That should let us answer:

- whether the extended channels belong to the same frame/relation framework
- and whether channels `>=7` are true continuations of the owner graph model or a separate overlay on top of it
