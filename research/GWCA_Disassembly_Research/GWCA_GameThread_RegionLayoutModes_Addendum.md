## `Gw.exe` Frame Callback Region Layout Modes Addendum

This pass continues from the reset-and-region-table note by decompiling the key layout helpers underneath `FUN_005F00F0(...)`:

- `FUN_005EFC20`
- `FUN_005F0970`
- `FUN_005319B0`

The goal was to turn the child layout strategies `0`, `1`, `2`, and `3` from anonymous switch values into something closer to named layout behaviors.

## Source artifacts

These results come from:

- [gw_decomp_region_layout_helpers_temp99.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_layout_helpers_temp99.log)
- [gw_decomp_interaction_reset_temp98.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_interaction_reset_temp98.log)

## High-level result

This pass clarifies the layout side in three useful ways:

- `FUN_005EFC20(...)` is a child-availability / measurability gate
- `FUN_005319B0(...)` is just rect-to-size reduction with validation
- `FUN_005F0970(...)` is the real recursive size resolver behind the region table

Most importantly, the layout strategy values at child record `+0x28` are now much easier to describe:

- `0` = stacked aggregate layout along one axis
- `3` = stacked aggregate layout along the orthogonal axis
- `1` = measured child-object layout via the owner/control system
- `2` = policy/virtual layout using a callback-selected dimension mask

Those names are still a little provisional, but they are much more concrete than raw integers.

## `FUN_005319B0(size, rect)`: rect-to-size reduction

This helper is as small and direct as it looks:

```cpp
if (rect.left <= rect.right && rect.top <= rect.bottom) {
    size.w = rect.right - rect.left;
    size.h = rect.bottom - rect.top;
}
else fail;
```

So this helper is simply:

- rect-to-size reduction with validity check

This matters mostly because it confirms one part of `FUN_005F00F0(...)` and `FUN_005F0970(...)`:

- several of the region-table paths are fundamentally operating by shrinking and reusing live rectangles
- then reducing them back to width/height as needed

## `FUN_005EFC20(owner_id)`: child measurability / availability gate

This helper is small but important.

It:

- checks whether the current child record is in layout mode `1`
- resolves the child object through:
  - `FUN_0060E2B0(owner_id, child_slot_id)`
- then checks that object through:
  - `FUN_0060F5C0(child)`
- if that check fails, it falls back to a record flag bit:
  - `record_flags >> 4 & 1`

Otherwise it returns `1`.

So this helper is best described as:

- child measurability / availability gate

That is a useful correction.
It is not computing geometry itself.
It is deciding whether the child should participate as a live measurable child-object or just fall back to record-defined behavior.

## Why that gate matters

This helper explains a confusing part of `FUN_005F0970(...)`.

When layout mode `1` is active, the code path first asks:

- is the referenced live child object available/measurable?

If yes:

- it measures the live child through `FUN_0060E4C0(...)`

If not:

- it effectively falls back to record-based defaults/flags

So layout mode `1` is not "just ask a child for its size."
It is:

- child-object measurement gated by availability/visibility-like state

## `FUN_005F0970(size, index, avail, mask)`: recursive size resolver

This is the most important helper in the pass.

At a high level it:

1. loads the current child record from the owner’s indexed child table
2. resolves base available width/height from `avail`
3. applies record flags:
   - proportional sizing when flag `8` is set
   - clamp-to-available behavior when flag `1` is set
   - exact/fill behavior when flag `4` is set
   - minimum-like behavior when flag `2` is set
4. subtracts record extents/insets from the available region
5. switches on record layout mode at `+0x28`

That switch is the real semantic upgrade of the pass.

## Layout mode `0`: stacked aggregate in one axis

For mode `0`, `FUN_005F0970(...)`:

- recursively visits following children
- enforces a specific child flag family `0x53`
- accumulates one dimension as a max
- conditionally advances/consumes the other dimension when child flags contain `0x180`

This is a very strong signature for:

- stacked aggregate layout along one axis

The exact axis depends on the engine’s coordinate convention, but behaviorally this is:

- "children laid out sequentially on one axis while the other axis becomes the max extent"

## Layout mode `3`: stacked aggregate in the orthogonal axis

Mode `3` is the close sibling to mode `0`.

It:

- also recursively visits following children
- enforces a different child flag family `0x2E`
- flips which dimension is accumulated as max and which one is sequentially consumed

So the cleanest description is:

- stacked aggregate layout in the orthogonal axis

Together, modes `0` and `3` now read like a pair:

- one horizontal-stack style aggregate
- one vertical-stack style aggregate

I am intentionally keeping those names a bit cautious because the coordinate convention is still inferred from behavior, but the paired-axis aggregate pattern is very clear.

## Layout mode `1`: measured live-child layout

For mode `1`, `FUN_005F0970(...)`:

- resolves the referenced live child object by slot id
- checks measurability through `FUN_005EFC20(...)`
- when allowed, measures it via:
  - `FUN_0060E4C0(&out_size, child_obj, &avail)`

This is the strongest and cleanest mode identity in the pass.

Mode `1` is best described as:

- measured live-child layout

or more explicitly:

- "query the actual referenced child object for size within the current available region"

That fits perfectly with the broader host/region-control model we’ve been building.

## Layout mode `2`: policy / virtual layout

Mode `2` is different from the others.

It does not recurse directly or resolve a live child by slot the same way.
Instead it:

- calls a virtual/policy callback through the owner object
- passes a mask derived from the child record flags
- then uses the returned mask bits to decide which dimension to set from record field `+0x04`

That makes mode `2` best described as:

- policy / virtual layout

or:

- callback-selected dimension layout

So this looks like a more abstract record type where the parent/control policy decides which dimension is meaningful.

## What the child record flags now look like

This pass does not completely decode all the flag bits, but their behavioral roles are much clearer:

- bit `8`
  - proportional / percentage-style scaling
- bit `1`
  - clamp-to-available behavior
- bit `4`
  - exact/fill dimension behavior
- bit `2`
  - minimum-like fallback behavior when exact sizing is not in force
- bits `0x180`
  - stacked-layout consume/advance behavior in modes `0` and `3`

That is enough to make the table feel like a real layout descriptor format rather than an opaque metadata blob.

## Updated interpretation of `FUN_005F00F0(...)`

With these helpers decoded, the higher-level region walker reads more clearly:

- `FUN_005F0970(...)` computes how much size a region subtree wants
- `FUN_0060FE70(...)` and friends place/shrink actual rects based on that size
- `FUN_005319B0(...)` reduces final rects back into widths/heights where needed

So the region-table layer is now best understood as:

- recursive size negotiation plus rect placement

not merely:

- recursive slot resolution

## Updated working model

The cleanest current model is now:

- host/control instance owns:
  - reusable interaction state
  - indexed child-region table
  - recursive layout resolver
- child record layout strategies:
  - `0` = stacked aggregate along one axis
  - `1` = measured live-child layout
  - `2` = policy/virtual layout
  - `3` = stacked aggregate along the orthogonal axis

That is a substantial step up from the earlier "modes 0..3 exist" wording.

## Best next step

The next best step is to keep tightening the record semantics around:

- `FUN_0060F5C0`
- `FUN_0060FE70`
- `FUN_0060FAC0`
- and the virtual callback used by layout mode `2`

That should let us answer the next important questions:

- what exact child-object state makes a mode-`1` child measurable or not?
- and what policy object / callback contract drives mode `2`?

