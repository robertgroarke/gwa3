## `Gw.exe` Frame Callback Reset And Region Table Addendum

This pass continues from the interaction-lifecycle note by decompiling the reset helpers and the owner-managed table walker that the control instance uses during commit:

- `FUN_005EDEE0`
- `FUN_005EDB70`
- `FUN_005F00F0`
- `FUN_005EF740`

The goal was to close two remaining gaps:

- what exactly is reset when the interaction ends?
- what kind of region/slot table is the instance validating and committing against?

## Source artifacts

These results come from:

- [gw_decomp_interaction_reset_temp98.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_interaction_reset_temp98.log)
- [gw_decomp_interaction_helpers_temp97.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_interaction_helpers_temp97.log)

## High-level result

This pass simplifies one part of the model and deepens another.

The reset side is straightforward:

- `FUN_005EDEE0(...)` only clears the planar state mode field
- `FUN_005EDB70(...)` only clears the directional state mode field

The interesting complexity is in `FUN_005F00F0(...)`, which is not a generic array scan.
It is a recursive owner-managed region/slot layout walker that:

- indexes a per-owner child table
- interprets child metadata flags and mode fields
- computes/clamps child bounds
- recurses into nested children
- and updates the caller’s current slot index

So the control instance is not just checking a flat list.
It is validating against a real nested region layout structure.

## `FUN_005EDEE0(state)`: planar state mode clear

This helper is as small as it looks:

```cpp
*(state + 0x18) = 0;
```

That means the planar interaction state keeps its whole record in place and only clears the mode/armed-active field at release.

So the best description is:

- planar state mode clear

not:

- full planar state reset

That is useful because it implies the stored coordinates are preserved across release until the next acquire path overwrites them.

## `FUN_005EDB70(state)`: directional state mode clear

This helper is even simpler:

```cpp
*state = 0;
```

Given how `FUN_005EDB80(...)` seeds the record, this means the first word is acting as the directional-state mode field.

So this is best described as:

- directional state mode clear

Again, that is a mode reset, not a full memory wipe.

## What this says about interaction release

Combined with `FUN_0052D190(...)`, the release path now looks like:

- emit end signal
- clear planar mode
- clear directional mode
- clear active token

That is a crisp state-machine shutdown, not a destructive object teardown.

So the interaction family is behaving more like:

- reusable instance-local state machine

than:

- fire-and-forget transient drag object

## `FUN_005EF740()`: control-instance subobject initializer

This helper seeds a small embedded object:

- zeroes fields at offsets `+4`, `+8`, `+0x0C`, `+0x10`
- sets field `+0x14` to `0x40`
- installs vtable `PTR_FUN_00A1F2E4`

So this looks like:

- embedded control-instance subobject initializer

or more specifically:

- table/region-manager style subobject bootstrap

because the initialized object is exactly what later supplies the `in_ECX` table fields used by `FUN_005EFFC0(...)` and `FUN_005F00F0(...)`.

## `FUN_005F00F0(index, rect, size, mask)`: recursive region-table walker

This is the main result of the pass.

At a high level, this helper:

1. validates the incoming current index and owner table bounds
2. loads the current child record from:
   - `in_ECX[2] + index * 4`
3. reads several record fields:
   - geometry / min-size-like values at `+0x08`, `+0x0C`
   - extents or offsets at `+0x14..+0x20`
   - flag byte at `+0x10`
   - mode/type at `+0x24`
   - layout strategy at `+0x28`
4. applies percentage/fill constraints when record flag `8` is set
5. applies clamp-to-parent behavior when record flag `1` is set
6. uses `FUN_0060FE70(...)`, `FUN_0060FAC0(...)`, and `FUN_005F0970(...)` to compute the child rectangle under different alignment/layout strategies
7. recursively descends into subsequent children when layout mode is:
   - `0`
   - `1`
   - `2`
   - `3`
8. updates the caller’s current index with the child it resolved
9. writes the final rectangle back into the caller’s `rect`

That is much stronger than "search selected slot."

This is clearly:

- recursive region-table walker and layout resolver

## What the region table now looks like

From `FUN_005F00F0(...)`, the owner-side structure now looks like a real indexed child table:

- `in_ECX[1]`
  - owner/control id used in child resolution
- `in_ECX[2]`
  - pointer to child-record pointer array
- `in_ECX[4]`
  - child count

Each child record appears to include:

- a child slot/id at `+0x04`
- size-like values at `+0x08 / +0x0C`
- a flag byte at `+0x10`
- rect/inset-style values at `+0x14..+0x20`
- a mode field at `+0x24`
- a layout strategy at `+0x28`

This is much more like:

- indexed region-layout descriptor table

than:

- arbitrary slot metadata

## Why `FUN_005EFFC0(...)` mattered

The previous pass showed `FUN_005EFFC0(...)` calling `FUN_005F00F0(...)` and then asserting that the returned index matched the currently selected/active slot.

Now we can say what that really means:

- the control instance validates a proposed rectangle against the recursive region-layout table
- then ensures the resolved table index matches the active/selected region entry

So the interaction family is not merely tracking pointer capture.
It is actively reconciling the live interaction against a nested region-layout structure.

## How this sharpens the slot `2/3` interpretation

This pass still does not give the final user-facing name of slots `2` and `3`, but it strengthens the current reading in a very specific way:

- the directional updates are part of a reusable instance-local state machine
- that state machine resolves and validates against a recursive region-layout table
- the slot/region family therefore behaves like genuine interactive subregions inside a structured control host

That makes a passive interpretation even less likely.

The stack now looks more like:

- host control
- instance-local interaction state
- nested region-layout table
- directional subregion targets

## Updated working model

The cleanest current model is now:

- `FUN_005EDF50 / FUN_005EDB80`
  - initialize the two interaction-state records
- `FUN_005EDE20 / FUN_005EDBD0 / FUN_005EDEA0`
  - promote those records into active motion modes
- `FUN_0052D190`
  - emits end signals and clears only the mode/token fields
- `FUN_005EF740`
  - initializes the embedded region-table manager object
- `FUN_005F00F0`
  - resolves nested child-region layout and advances the active index

So the host/region family is now best described as:

- an instance-backed interactive region system on top of a recursive child-region layout table

## Best next step

The next best step is to identify the remaining region-table and layout helpers that `FUN_005F00F0(...)` leans on most heavily:

- `FUN_005EFC20`
- `FUN_005F0970`
- `FUN_005319B0`
- plus the child-mode helpers around record field `+0x24`

That should let us answer the next important question:

- what do child layout modes `0`, `1`, `2`, and `3` actually mean inside this interactive region table?

