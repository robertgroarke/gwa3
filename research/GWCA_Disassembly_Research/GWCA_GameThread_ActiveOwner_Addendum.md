## `Gw.exe` Frame Callback Active Owner Addendum

This pass continues from the frame-relation owner note by reversing the next follow-up helpers:

- `FUN_0062DC70`
- `FUN_00624CE0`
- `FUN_00626030`
- `FUN_00626BC0`
- `FUN_0062E5D0`
- `FUN_0060D890`

The main goal was to tighten three remaining points:

1. what the active-owner globals actually represent
2. what message `0x25` is doing
3. whether this relation layer is mostly layout, mostly selection/focus, or both

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_relation_followups_temp69.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_relation_followups_temp69.log)

## High-level result

This pass makes the active-owner side much clearer.

The strongest new points are:

- `FUN_0062DC70(owner)` is a parent-chain membership test over `+0x128`
- `FUN_00626030(owner)` is a true active-owner setter/clearer
- `FUN_0062E5D0(set_bits, clear_bits, xor_bits)` is a shared flag-fanout helper that propagates state changes into many subsystems
- `FUN_00624CE0(...)` and `FUN_00625D90()` confirm that `0x29` and `0x26` belong to the same active-owner/relation cleanup family
- `FUN_0060D890(id)` is not a tiny notifier, it is a large owner-id driven teardown/rebind path that triggers `FUN_00628A10(3, 0, 0)` and a broad refresh cascade

So the best current interpretation is now:

- this relation layer is not only layout
- it definitely includes active-owner / selection / focus behavior
- while still sharing the same frame/relation infrastructure as the layout/coordinate paths

## `FUN_0062DC70(owner)`: parent-chain membership check

This helper is small but very useful.

It walks upward through:

- `owner + 0x128`

using the same `ptr - 0x128` recovery pattern we already saw before.

At each step it checks whether the climbed owner matches:

- `this - 0x128`

and returns true if the candidate owner lies somewhere in that ancestor chain.

That means `FUN_0062DC70(...)` is best understood as:

- **is this owner in my relation-parent chain?**

not:

- a generic validator
- or a registry lookup

That is important because it makes the active-owner logic more concrete:

- the engine is not just tracking “some current owner”
- it is testing hierarchical relation ancestry

## `FUN_00626030(owner)`: active-owner setter / clearer

This is the biggest semantic win in the batch.

It manages:

- `DAT_00bd0c48`

which is now much more convincingly an active/current owner pointer.

Its behavior is:

1. capture the previous owner and its runtime id (`old_owner + 0xBC`)
2. reject no-op self-assignment
3. clear the current owner
4. if an old owner existed:
   - `FUN_0062e5d0(0, 0x40, 0)`
   - `FUN_006286d0(0x25, 0, 0)`
5. set the new owner
6. if a new owner exists:
   - `FUN_0062e5d0(0x40, 0, 0)`
   - `FUN_006286d0(0x25, 1, 0)`
7. publish the new owner id through:
   - `FUN_006287d0(0x4e, &local_c, 0)`
8. update one more downstream target with:
   - `FUN_00618f10(new_owner ? new_owner + 0x44 : 0)`

That makes `0x25` much tighter semantically.

The strongest current reading is:

- **active-owner changed**

with:

- `payload = 0` for clear
- `payload = 1` for set

So this relation layer is definitely doing selection/focus-style active-owner state, not only geometric layout.

## `FUN_0062E5D0(set_bits, clear_bits, xor_bits)`: shared flag mutation and fan-out

This helper is one of the most important infrastructure pieces in the batch.

It computes:

- `new_flags = ((old_flags & ~clear_bits) | set_bits) ^ xor_bits`

then:

- stores the new flags
- computes the changed-bit mask

and fans the change out through a whole cluster:

- `FUN_00615AA0(new_flags, delta)`
- `FUN_00617880(new_flags, delta)`
- `FUN_0061C790(new_flags, delta)`
- `FUN_006208F0(new_flags, delta)`
- `FUN_00625C70(new_flags, delta)`
- `FUN_006141E0(new_flags, delta)`

That means the relation-owner flags are not local bookkeeping only.

They are a shared state word whose changes propagate immediately into multiple subsystems.

This is one of the strongest reasons to describe the hook seam as a true frame/relation maintenance loop rather than a narrow UI helper cluster.

## `FUN_00624CE0(payload)`: relation cleanup/reset notification

This helper is short but clarifies the global cleanup side.

It:

- if `DAT_00bd0c68 != 0`, sends:
  - `FUN_00624700(0x26, payload)`
- clears `DAT_00bd0c64` / `DAT_00bd0c60`
- clears `DAT_00bd0c68`
- calls `FUN_006189f0()`
- if `DAT_00bd0c6c != 0`:
  - clear it
  - `FUN_0062e5d0(0, 0x80, 0)`
  - `FUN_006286d0(0x29, 0, 0)`
- sets bit `1` in `DAT_00bd0c3c`

This reinforces two earlier interpretations:

- `0x29` belongs to global relation/active-owner cleanup
- `0x26` is another cleanup/reset style notification in the same family

The exact distinction between `0x26` and `0x29` is still open, but neither looks like a generic layout update now.

## `FUN_0060D890(owner_id)`: owner-id driven rebind / refresh cascade

This helper is much larger than its earlier call sites suggested.

Its rough shape is:

1. require a non-zero owner id
2. validate it through the registry layer
3. resolve the owner object
4. check flag/category state through `FUN_0062E640(4)` and `FUN_0062E640(8)`
5. set owner flags via:
   - `FUN_0062e5d0(8, 0, 0)`
6. if a certain condition is not already satisfied:
   - `FUN_00628A10(3, 0, 0)`
7. while the owner remains valid:
   - `FUN_0062D220(3, 0)`
   - `FUN_0060BE80(...)`
8. then run a very broad refresh/cleanup cascade:
   - `FUN_0062CDD0()`
   - `FUN_006284E0()`
   - `FUN_0061F680()`
   - `FUN_0061B210()`
   - `FUN_006243E0()`
   - `FUN_0062F6F0()`
   - `FUN_00613C00()`
   - `FUN_0062F630()`
   - `FUN_0062EFF0()`
   - `FUN_0062E680()`
   - `FUN_0062CB00()`
   - `FUN_00629280()`
   - `FUN_00626420()`
   - `FUN_00627890()`
   - `FUN_00624180()`
   - `FUN_006210F0()`
   - `FUN_0061F200()`
   - `FUN_0061A560()`
   - `FUN_00618160()`
   - `FUN_006162C0()`
   - `FUN_00614CD0()`
   - final free/reset of the resolved object

This is not just a notifier.

It looks like:

- rebind to owner id
- raise/normalize owner-active flags
- emit owner-local phase `3`
- drain owner-associated work/items
- then refresh many dependent systems

That is very strong evidence for a focus/selection-style path.

The exact user-facing name is still unresolved, but this is much more than layout.

## `FUN_00626BC0()`: side-state clear

This one is small:

- `FUN_00627250(0, 0, 0)`
- `DAT_00bd0c90 = 0`

By itself it is modest, but in context it fits the same family:

- it is part of the teardown path called from `FUN_00625D90()`
- so it likely clears one auxiliary relation/selection side state

## What this changes about the owner-family interpretation

Before this pass, the best current description was:

- frame-relation owner object
- with layout/attachment bookkeeping
- active-owner participation

After this pass, the stronger description is:

- **frame-relation owner with active-selection/focus semantics**

because we now have:

- a concrete active-owner setter/clearer
- ancestor-chain tests
- owner-id based rebind logic
- set/clear notifications on message `0x25`
- global cleanup/reset notifications on `0x26` / `0x29`
- broad downstream refresh after owner activation

So the safest high-level wording is now:

- this relation layer spans both **layout/attachment** and **active-owner / focus / selection**

not one or the other alone.

## Updated message interpretation

This pass makes several ids sharper:

- `0x25` = active-owner changed
  - `0` on clear
  - `1` on set
- `0x26` = relation cleanup/reset style notification
- `0x29` = relation deactivation / global clear notification
- `type_id 3` in `FUN_00628A10(3, 0, 0)` = owner-activation / rebind-side phase
- `type_id 4` from the previous pass still looks like relation/layout reevaluation

That means the `FUN_00628A10(...)` family is now carrying at least two semantically different owner-local phases:

- `3` = activation/rebind side
- `4` = relation/layout reevaluation side

## Best current interpretation

The strongest safe model after this pass is:

- GWCA’s `GameThread` hook sits above Guild Wars’ frame/relation maintenance loop
- that loop includes:
  - coordinate/rect submission
  - relation attach/detach
  - active-owner set/clear
  - owner-id driven rebind/refresh
  - broad flag propagation into multiple subsystems

So this is now very clearly a rich frame/relation/focus maintenance seam, not merely a render callback and not merely a layout pass.

## Best next step

The next best reverse step is to resolve the remaining typed phase helpers and the owner-work drain path:

- `FUN_0062D220`
- `FUN_00628A10` callers by concrete `type_id`
- `FUN_00624700`
- `FUN_006287D0`
- `FUN_00618F10`

That should let us:

- name the `type_id 3` and `type_id 4` phases more confidently
- distinguish the global message ids (`0x25`, `0x26`, `0x29`) from the owner-local typed phases
- and say more precisely whether the active-owner path is closest to hover, focus, selection, or a generalized “current relation target”
