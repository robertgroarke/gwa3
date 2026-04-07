## `Gw.exe` Frame Callback Typed Phase Lookup Addendum

This pass continues from the phase-taxonomy note by reversing the owner-local typed-phase lookup helper itself and correlating it with concrete callers of `FUN_00628A10(...)`.

The main targets were:

- `FUN_0062D5C0`
- callers of `FUN_00628A10`

## Source artifacts

These results come from:

- [gw_decomp_phase_lookup_temp71.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_phase_lookup_temp71.log)
- [gw_findcallers_00628a10_temp71.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00628a10_temp71.log)

## High-level result

This pass finally makes the typed phases feel like a real subsystem family.

The strongest new points are:

- `FUN_0062D5C0(type_id, seed_ptr)` is a multi-case resolver over several embedded relation tables/heads
- `FUN_0062D220(type_id, owner)` is just a wrapper that normalizes the returned subobject back to the containing owner base
- `FUN_00628A10(...)` is called from a wider set of paths than just the ones already traced:
  - `FUN_00624900`
  - `FUN_0060D890`
  - `FUN_0062A980`
  - plus a cluster of still-unresolved helpers

So the owner-local phase system is now best described as:

- **typed relation-subobject lookup + gated owner-local notification**

not just:

- “call phase 3” or “call phase 4”

## `FUN_0062D5C0(type_id, seed_ptr)`: typed relation-subobject resolver

This helper is the missing structural core beneath `FUN_0062D220(...)`.

Its overall shape is:

- switch on `type_id`
- inspect one of several owner-local table/head fields
- walk linked/embedded relation records
- skip inactive nodes
- return the first live matching subobject

Then `FUN_0062D220(...)` simply converts that returned interior pointer back to the containing owner by subtracting `0x128`.

That means typed phases are not abstract event numbers by themselves. Each one corresponds to a different relation-subobject family inside the owner.

### Recovered cases

The visible cases in this pass are:

- `case 0`
- `case 1`
- `case 2`
- `case 3`
- `case 4`
- `case 5`

Each one walks a different owner-local anchor.

### `case 0`

This case starts from:

- `this + 0x24`

and repeatedly follows links through the structure at:

- `this + 0x1C`

until it finds a live node (`*node != 0`) or hits an inactive/terminal marker.

### `case 1`

This case starts from:

- `this + 0x18`

and walks through the structure rooted by:

- `this + 0x10`

again searching for the first live subobject.

### `case 2`

This case requires a non-zero seed pointer and indexes through:

- `this + 0x1C`

using that seed.

So `case 2` is not just “type 2 event.” It is:

- a seeded lookup into one specific owner-local relation table family

### `case 3`

This case walks from:

- `**(this + 0x14) + 4`

and follows links through:

- `this + 0x10`

until it reaches a live node.

This is the most relevant one so far because we already saw:

- `FUN_0060D890(...)` use `FUN_00628A10(3, 0, 0)`
- then repeatedly call `FUN_0062D220(3, 0)`

So the best current interpretation is:

- `type_id 3` resolves one specific owner-local relation subobject family tied to activation/rebind work

### `case 4`

This case requires a non-zero seed pointer and walks through:

- `this + 0x1C`

but by first dereferencing through:

- `**(this + 0x1C + seed)`

That is structurally distinct from `case 2`, even though both touch the `+0x1C` family.

This is also the phase we already saw from:

- `FUN_0062A980() -> FUN_00628A10(4, 0, 0)`

So the best current interpretation remains:

- `type_id 4` belongs to relation/layout reevaluation

but now with stronger structural backing:

- it is resolving a particular seeded subobject family under the owner

### `case 5`

This case requires a non-zero seed pointer and walks the family rooted at:

- `this + 0x10`

That gives us at least three distinct owner-local relation-table groups in play:

- one rooted around `+0x24`
- one rooted around `+0x18`
- one or more seeded families around `+0x1C`
- one or more seeded families around `+0x10`

That is a much richer model than “a couple hard-coded phases.”

## What `FUN_0062D220(...)` now means more precisely

With `FUN_0062D5C0(...)` recovered, the wrapper at `FUN_0062D220(type_id, owner)` is now easier to phrase:

- start from `owner + 0x128` if an owner is provided
- resolve the first matching live typed relation subobject
- return its containing owner base

So `FUN_0062D220(...)` is effectively:

- **find the owner participating in typed relation phase N**

That is an important upgrade over the earlier “owner lookup helper” wording.

## `FUN_00628A10(...)` caller set

The caller map for `FUN_00628A10(...)` now includes:

- [FUN_00624900](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00628a10_temp71.log) at `00624BDC`
- [FUN_0060D890](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00628a10_temp71.log) at `0060D961`
- [FUN_0062A980](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00628a10_temp71.log) at `0062AA62`
- plus several unresolved callers:
  - `FUN_0060D300`
  - `FUN_0060BE80`
  - `FUN_0060BCB0`
  - `FUN_0060BD30`
  - `FUN_0062F7E0`
  - `FUN_0060DAA0`
  - `FUN_006100A0`
  - `FUN_0060C3A3`

This matters because it shows the typed-phase system is not narrowly confined to just:

- coordinate submission
- owner activation
- owner layout reevaluation

It is used by a larger cluster of relation/owner helpers.

So the phase family is probably broader than the currently named `3` and `4` paths.

## What this changes about typed phases

Before this pass, the best current model was:

- `type_id 3` = activation/rebind side
- `type_id 4` = relation/layout reevaluation side

That remains directionally good, but it was still too “behavioral.”

After this pass, the stronger model is:

- each `type_id` selects a different owner-local relation-subobject family
- `FUN_00628A10(type_id, ...)` then runs the gated owner-local notification path against that family

So the typed phases are now better described as:

- **owner-local relation channels**

rather than just abstract numbered phases.

### Current strongest readings

- `type_id 3`
  - owner-local activation/rebind relation channel
  - used by `FUN_0060D890(...)`
- `type_id 4`
  - owner-local relation/layout reevaluation channel
  - used by `FUN_0062A980(...)`

And there are clearly more channels available through the shared resolver.

## Best current interpretation

The strongest safe model after this pass is:

- the frame/relation maintenance seam contains:
  - global relation message broadcasts
  - owner-local typed relation channels
  - gated forwarding/callback wrappers
- the typed channels are backed by real embedded relation-subobject families, not by a flat enum alone

That means the system is deeper and more object-structured than the earlier “message bus” phrasing suggested.

## Best next step

The next best reverse step is to decompile the highest-yield unresolved `FUN_00628A10(...)` callers, especially:

- `FUN_0060D300`
- `FUN_0060BE80`
- `FUN_0060BCB0`
- `FUN_0060BD30`

Those are likely to reveal:

- more concrete `type_id` usages
- whether `type_id 3` and `type_id 4` are really “selection” and “layout”
- and whether other owner-local relation channels exist for hover, drag, anchor, or similar frame behaviors
