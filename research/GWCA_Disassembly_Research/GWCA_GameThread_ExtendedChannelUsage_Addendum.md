## `Gw.exe` Frame Callback Extended Channel Usage Addendum

This pass continues from the channel-liveness note by deepening two unresolved edges:

1. the broader traversal helper used alongside channel `0`
2. concrete uses of the generic extended-channel entrypoint `FUN_006100A0(...)`

The main targets were:

- `FUN_0062D250`
- and a sample of real `FUN_006100A0(...)` callers:
  - `FUN_004A6500`
  - `FUN_004A6770`
  - `FUN_00508060`
  - `FUN_005719B0`

## Source artifacts

These results come from:

- [gw_decomp_extended_followup_temp76.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_extended_followup_temp76.log)
- [gw_findcallers_006100a0_temp74.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_006100a0_temp74.log)

## High-level result

This pass gives us two important upgrades.

First:

- `FUN_0062D250(...)` is not just another lookup wrapper
- it is a traversal helper that returns a related owner and reports whether a key relation field changed

Second:

- the generic extended-channel entrypoint is not only “used somewhere”
- we now have concrete live ids in hand:
  - `7`
  - `8`
  - `9`
  - `10`

So the extended channel range is not abstract. At least four extended owner-local channels are demonstrably live in the sampled caller set.

## `FUN_0062D250(type_id, owner, out_changed)`: traversal with change reporting

This helper is the missing partner to the earlier `FUN_00615700(...)` traversal.

Its behavior is:

- require `owner != 0`
- require `out_changed != 0`
- call:
  - `FUN_0062D5C0(type_id, owner + 0x128)`
- if a matching subobject exists:
  - set:
    - `*out_changed = (owner[0x150] != subobject[0x28])`
  - return the containing owner base:
    - `subobject - 0x128`
- otherwise:
  - `*out_changed = 0`
  - return `0`

That is a stronger role than the earlier rough guess.

It means the broader low-end traversal family now looks like:

- `type_id 0` = root of the broader owner family
- `type_id 2` = owner-relative continuation/resolution within that family
- `FUN_0062D250(2, owner, &changed)` = next related owner plus “did relation-state change?” flag

So the low-end channels split more cleanly now:

- `0 -> 2` = broad relation traversal with change reporting
- `1 -> 5` = narrower traversal/selection family

That is an important structural distinction.

## Sampled `FUN_006100A0(...)` callers: extended ids are concrete and live

The sampled callers show that the generic extended entrypoint is not just a catch-all API nobody uses. It is carrying specific live channel ids.

### `FUN_004A6500(...)`

This helper switches on a mode field in `param_1[1]`.

The most important path is:

- when `param_1[1] == 0x31`
- and `param_2[2] == 0x0B`
- call:
  - `FUN_006100A0(*param_1, 7, 0, 0)`
- then possibly:
  - `FUN_0060D890(*param_1)`

So extended channel `7` is definitely live.

There is also another path where:

- `param_1[1] == 9`
- and a new owner/frame is created through `FUN_0060D300(...)`

which ties this helper back into the setup/activation side of the already-mapped channel family.

### `FUN_004A6770(...)`

This sibling helper gives an even clearer small map of the extended ids.

When:

- `param_1[1] == 0x31`

it dispatches:

- if `param_2[2] == 0x0B`
  - `FUN_006100A0(*param_1, 8, 0, 0)`
- else if `param_2[2] == 8`
  - `FUN_006100A0(*param_1, 7, 0, 0)`

So we now know:

- `7` is live
- `8` is live

and they are closely related in at least one input/control path.

### `FUN_00508060()`

This one is especially useful because it exercises multiple extended ids together.

It calls:

- `FUN_006100A0(owner, 7, 0, &local_10)`

Then, depending on state, it may call:

- `FUN_006100A0(owner, 9, 0, &local_8)`
- `FUN_006100A0(owner, 8, 0, &local_c)`
- `FUN_006100A0(owner, 10, local_8, 0)`

So from one sampled caller alone we get direct evidence that:

- `7`
- `8`
- `9`
- `10`

are all live extended channel ids in this build.

Even better, they appear to form a little mini-protocol:

- `7` first
- possibly `9`
- then `8`
- then `10`

That strongly suggests the extended range contains its own structured sub-lifecycle, not just unrelated miscellaneous ids.

### `FUN_005719B0(param)`

This helper confirms the same cluster from another angle.

For one case it does:

- `FUN_006100A0(owner, 7, arg, 0)`
- `FUN_006100A0(owner, 8, 0, 0)`

For other cases it does:

- only `FUN_006100A0(owner, 8, 0, 0)`

So `7` and `8` again look like a paired or sequential part of one extended-channel family.

## What this does to the channel map

Before this pass, the best safe reading was:

- channels `>= 7` are definitely live
- but we did not yet know which ids were actually used in practice

After this pass, the live extended range is no longer abstract.

### Confirmed live extended ids

- `7`
- `8`
- `9`
- `10`

### Confirmed structural hints

- `7` and `8` are tightly paired across multiple callers
- `9` and `10` appear in at least one multi-step sequence with `7` and `8`
- these are not random singletons; they behave like a related extended-channel cluster

That means the owner-channel framework likely has:

- low-end traversal channels
- mid-range lifecycle channels
- and a higher extended interaction/control cluster

## Updated channel framework

With this pass added, the strongest current model is:

### Low-end traversal family

- `0` = broad root traversal
- `2` = continuation/resolution within that broad family, used via `FUN_0062D250(...)`
- `1` = narrower root/current traversal head
- `5` = sibling/iterator continuation for the `1` family

### Mid-range lifecycle family

- `2` = setup/enter
- `3` = activation/rebind/drain
- `4` = relation/layout reevaluation
- `6` = message-coupled follow-up for the `0x3D` family

### Extended live cluster

- `7`
- `8`
- `9`
- `10`

with at least one observed sequence:

- `7 -> 9 -> 8 -> 10`

The exact names are still unresolved, but the existence of that cluster is now source-backed rather than speculative.

## Best current interpretation

The strongest safe reading after this pass is:

- the frame/relation framework is not just a small fixed lifecycle around owner activation
- it also exposes a higher extended-channel layer with its own live multi-step interaction patterns

So the reverse-engineered seam is now best described as:

- **a layered owner/relation graph with traversal channels, lifecycle channels, and extended interaction channels**

That is substantially richer than a normal “UI callback” model.

## Best next step

The next best reverse step is to stop widening the map and instead name the extended cluster.

The highest-yield next targets are:

- decompile one or two stronger frame-adjacent callers from `gw_findcallers_006100a0_temp74.log`
- especially those that clearly pass:
  - `7`
  - `8`
  - `9`
  - `10`
- and, if useful, trace a companion helper near:
  - `FUN_00610160`
  - `FUN_00610370`
  - `FUN_0060F610`

That should tell us whether the extended ids correspond to things like:

- focus acquisition
- selection commit
- enter/leave modal state
- or another structured UI-control protocol layered above the relation owner system
