## `Gw.exe` Frame Callback Owner Relation Cluster Addendum

This pass continues directly from the owning-frame layout note by reversing the next owner-local helper cluster:

- `FUN_0062A980`
- `FUN_0062ABB0`
- `FUN_00625D90`
- `FUN_00624390`
- `FUN_0062CD10`
- `FUN_0062E530`

The goal was to answer the next identity question:

- is the recovered owner best understood as a viewport/layout frame, or something even more specific inside the engine’s frame system?

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_owner_helpers_temp68.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_owner_helpers_temp68.log)

## High-level result

This pass sharpens the owner identity substantially.

The strongest signals are:

- one helper contains explicit references to:
  - `P:\\Code\\Engine\\Frame\\FrRelation.cpp`
- owner teardown routes through:
  - `FUN_00624900(0x2e, ...)`
  - `FUN_006286d0(0x29, 0, 0)`
- owner-local flag processing maps one flag word into:
  - `0x200`
  - `0x10`
- the `+0xC0` bits at `0x100` and `0x200` trigger relation/list bookkeeping and follow-up dispatch

So the best current interpretation is no longer just “some UI container.”

It is much stronger to describe the recovered object as:

- a **real engine frame/relation owner**
- with relation/attachment bookkeeping
- dirty/update propagation
- and explicit deactivate/remove signaling

That is a more specific and more defensible label than the previous “frame/container object.”

## `FUN_0062CD10()`: explicit frame-relation helper

This is the clearest naming win in the batch.

The function checks bits in:

- `this + 0x68`

and when those bits are set it asserts/logs against:

- `P:\\Code\\Engine\\Frame\\FrRelation.cpp`

at lines `0xF7` and `0xFF`.

Then it allocates/initializes a small helper object and links it through:

- `FUN_0062CA70()`
- `FUN_00473D80(...)`
- `FUN_0062DDF0(...)`

That is enough to say, confidently, that this owner sits inside the game’s frame-relation subsystem.

The meaning of the exact relation bits is not fully named yet, but the two tested states are:

- `bit 0x20`
- `bit (1 << 10)`

with a consistency check that they are not simultaneously in one invalid combination.

So `FUN_0062CD10()` is not a generic UI helper. It is relation-specific frame maintenance.

## `FUN_0062E530()`: local flag normalization

This helper is small, but it explains part of the flag plumbing:

- if `this[1] & 1`, set `this[0] |= 0x200`
- if `this[1] & 2`, set `this[0] |= 0x10`

That means the owner-side code is normalizing one flag source into another.

The important part is not the exact labels for `0x200` and `0x10` yet. It is that the object is clearly maintaining:

- source relation/state bits
- derived dirty/behavior bits

which is exactly what you would expect in a frame/relation system.

This also helps the previous addendum:

- the owner-local callback/emit helpers are not floating around independently
- they are part of a relation-aware flag propagation path

## `FUN_00625D90()`: owner teardown / detach / deactivate path

This helper is one of the most informative in the batch because it shows the inverse lifecycle.

It begins from:

- `this - 0x94`

which is another route back into the owner family

and then:

- walks a linked/related object chain
- removes relation/list links
- frees the relation record
- calls:
  - `FUN_00624900(0x2e, &local_20, 0, 0)`

That immediately sharpens the earlier message-id interpretation.

`0x2e` is now much better described as:

- a **detach / deactivate / unregister** style message in the frame-relation path

not just a generic “clear a bit and emit.”

The rest of the function strengthens that reading:

- if the owner matches certain global active pointers, it clears them
- it triggers:
  - `FUN_00624ce0(&DAT_00bd0c20)`
  - `FUN_0062e5d0(0, 0x80, 0)`
  - `FUN_006286d0(0x29, 0, 0)`
  - `FUN_00626030(0)`
  - `FUN_00626bc0()`

That is a real teardown/deactivation path with global active-owner cleanup, not a local bookkeeping helper.

So the current safest interpretation is:

- `0x2e` = detach/unregister/deactivate
- `0x29` = follow-up global notification for that detach/clear path

The name of `0x29` is still provisional, but it is now clearly tied to teardown rather than general update.

## `FUN_00624390()`: active-owner selection / focus refresh

This helper gives the active-owner global a slightly more specific role.

It:

- computes `this - 0x94`
- checks bit `8` at `this + 0xFC`
- updates `DAT_00bd0c38`
- conditionally calls:
  - `FUN_0062DC70(...)`
  - `FUN_0060D890(owner + 0xBC)`

So this is not just cache maintenance in the abstract.

It looks more like:

- update current active/selected relation owner
- if a validity check fails, push an id-based notification using `owner + 0xBC`

That keeps reinforcing the same model:

- this owner participates in a globally tracked active frame/relation selection path

## `FUN_0062ABB0(force)` and `FUN_0062A980()`: dirty relation/list propagation

These two helpers are the concrete targets for the `+0xC0` flag bits from the previous addendum.

### `FUN_0062ABB0(force)`

This one is compact:

- optional `FUN_0062E5D0(0x2000, 0, 0)` when `force != 0`
- then list-link the current object into the registry rooted at `DAT_00bd0cd0`

So this looks like:

- mark relation/frame state dirty
- queue/link the object for later processing

### `FUN_0062A980()`

This is larger and does more than simple relinking.

It:

- recovers the owner
- if `owner + 0xC0 & 0x100`, links `owner + 0xD0` into the same registry/list
- calls:
  - `FUN_00628A10(4, 0, 0)`
- gets bounds from `FUN_0062A220(...)`
- computes width/height deltas
- calls:
  - `FUN_00619950(...)`
- and if the dimensions collapse to zero, re-links the current object into the same registry/list

This strongly suggests:

- `+0xC0 & 0x100` and `+0xC0 & 0x200` are dirty/layout/relation-update bits
- `FUN_0062A980()` is a more complete reevaluation pass, not just queue insertion

Most importantly, it shows that type id `4` sent through `FUN_00628A10(...)` belongs to the same owner-local relation/layout family.

So the second-stage dispatcher is not only for the coordinate path that first led us here. It is also used by owner-local relation/update reevaluation.

## What this does to the owner identity

Before this pass, the best description was:

- a concrete frame/container object
- with bounds, placement flags, runtime id, and callback tables

After this pass, the stronger description is:

- a **frame-relation owner object**
- with:
  - bounds
  - placement/alignment
  - relation flags
  - active-owner participation
  - queued dirty/update relinking
  - detach/unregister signaling
  - owner-local callback planes

That is a real subsystem identity improvement.

## Updated message-id interpretation

This pass does not fully rename every message id, but it narrows them significantly.

The strongest current safe readings are:

- `0x24` = activate/register/update in the relation/update path
- `0x2e` = detach/unregister/deactivate in that same path
- `0x29` = follow-up clear/deactivate notification
- `0x31` / `0x32` = owner-local gated notification phases used by relation/layout reevaluation as well as coordinate submission

And we now have one concrete type-id use:

- `FUN_00628A10(4, 0, 0)` is part of owner-local relation/layout reevaluation

So the dispatch family beneath the GWCA-hooked frame callback is even more clearly tied to the engine’s frame system than before.

## Best current interpretation

The strongest safe model after this pass is:

- GWCA’s `GameThread` hook sits above part of Guild Wars’ frame/relation maintenance loop
- one descendant branch performs coordinate/rect submission
- another descendant branch performs relation/layout dirtying, detach, and active-owner maintenance
- both use the same underlying owner-local dispatch/callback machinery

So the hook is sitting in a very rich UI/frame maintenance seam, not merely a render tick and not merely a generic scheduler.

## Best next step

The next best reverse step is to resolve the remaining relation-owner validators and active-owner helpers:

- `FUN_0062DC70`
- `FUN_00624CE0`
- `FUN_00626030`
- `FUN_00626BC0`
- `FUN_0062E5D0`
- `FUN_0060D890`

That should let us tighten:

- the exact meaning of the active-owner globals
- the semantics of `0x29`
- and whether the owner family is best thought of as a focus/selection relation layer, a layout relation layer, or both
