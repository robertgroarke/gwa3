## `Gw.exe` Frame Callback Measurability And Placement Addendum

This pass continues from the region-layout-modes note by decompiling the next helper layer under the recursive region table:

- `FUN_0060F5C0`
- `FUN_0060FE70`
- `FUN_0060FAC0`

The goal was to close two concrete questions:

- what exact child state makes a mode-`1` child measurable?
- how are placement masks actually applied to the working rectangle?

## Source artifacts

These results come from:

- [gw_decomp_layout_policy_temp100.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_layout_policy_temp100.log)
- [gw_decomp_region_layout_helpers_temp99.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_layout_helpers_temp99.log)

## High-level result

This pass sharpens the layout model in three useful ways:

- `FUN_0060F5C0(...)` shows that a mode-`1` child stops being measurable when owner flag `0x200` is set
- `FUN_0060FE70(...)` is a simple masked edge-adjustment helper over the working rectangle
- `FUN_0060FAC0(...)` is just a thin wrapper into the already-known child placement engine `FUN_0060FB00(...)`

So the remaining big unknown is no longer the placement math.
It is the mode-`2` virtual/policy callback.

## `FUN_0060F5C0(child)`: measurability blocked by owner flag `0x200`

This helper is straightforward:

```cpp
validate(child);
return FUN_0062E640(0x200) == 0;
```

That means the mode-`1` measurability gate from `FUN_005EFC20(...)` now has a much more concrete interpretation:

- if the resolved live child has owner flag `0x200` set, it is not measurable
- otherwise it is measurable

Given the other reverse work in this cluster, flag `0x200` already looked like a visibility/enable-style control bit in some frame-side helpers.
So the cleanest current description is:

- mode-`1` child measurement is blocked by child flag `0x200`

I would phrase that as:

- hidden/disabled/not-participating child

rather than assign a stricter label yet.

## What this does to layout mode `1`

The previous pass called mode `1`:

- measured live-child layout

This pass lets us tighten that to:

- measured live-child layout, gated by child flag `0x200`

So mode `1` now clearly means:

- resolve live child by slot
- if child participates, measure it
- otherwise fall back to record-defined sizing behavior

That is a cleaner and more defensible model.

## `FUN_0060FE70(value, mask, rect)`: masked edge adjustment helper

This helper directly mutates the working rectangle using mask bits:

- bit `1`
  - increases `top` by `value`, clamped to `bottom`
- bit `4`
  - increases `left` by `value`, clamped to `right`
- bit `8`
  - decreases `right` by `value`, clamped to `left`
- bit `0x10`
  - decreases `bottom` by `value`, clamped to `top`

So this helper is best described as:

- masked edge adjustment / inset helper

That gives a much clearer semantic interpretation to all the earlier callers:

- they are not doing abstract "placement math"
- they are trimming or offsetting the active rectangle from specific edges

## Why `FUN_0060FE70(...)` matters

This helper makes the region-table code easier to read at a glance.

When `FUN_005F00F0(...)` or `FUN_005F0970(...)` calls `FUN_0060FE70(...)`, it is doing one of a few concrete things:

- consume space from the left
- consume space from the top
- consume space from the right
- consume space from the bottom

That strongly supports the earlier interpretation of modes `0` and `3` as orthogonal stacked aggregates.
The stacked layouts are literally shrinking the remaining rect from different edges as children are placed.

## `FUN_0060FAC0(...)`: thin placement wrapper

This helper turns out to be very small:

```cpp
FUN_0060FB00(owner_or_child, mask, rect, aux, 1);
```

So it is best described as:

- thin wrapper into the general placement engine

The main takeaway is architectural:

- there is no new hidden layout subsystem here
- the region-table layer is still leaning on the same generic placement engine we already mapped

That means the structure is now clearer:

- `FUN_005F0970(...)` decides desired size
- `FUN_0060FE70(...)` trims/adjusts the active rect
- `FUN_0060FAC0(...)` delegates final policy placement to `FUN_0060FB00(...)`

## Updated view of the region-table pipeline

After this pass, the pipeline under the region table reads as:

1. choose whether a live child participates
   - `FUN_005EFC20(...)`
   - `FUN_0060F5C0(...)`
2. compute desired size
   - `FUN_005F0970(...)`
3. mutate the active rectangle from specific edges
   - `FUN_0060FE70(...)`
4. hand off to generic placement logic
   - `FUN_0060FAC0(...)`
   - `FUN_0060FB00(...)`

That is a much more concrete pipeline than the earlier "recursive size negotiation plus placement" phrasing.

## What is still unresolved

This pass makes one thing stand out more clearly:

- mode `2` is now the real remaining semantic blind spot

Because:

- measurability for mode `1` is understood
- placement masks and rect trimming are understood
- stacked aggregate modes `0` and `3` are mostly understood

So the highest-value unanswered question is:

- what exact callback contract is mode `2` using when it asks the owner for a dimension mask?

## Updated working model

The cleanest current model is now:

- mode `0`
  - stacked aggregate layout along one axis
- mode `1`
  - measured live-child layout, suppressed when child flag `0x200` is set
- mode `2`
  - policy/virtual layout through owner callback
- mode `3`
  - stacked aggregate layout along the orthogonal axis

And the supporting primitives are now clearer:

- `FUN_0060F5C0(...)`
  - child participation/measurability test
- `FUN_0060FE70(...)`
  - edge-based rect shrink/adjust
- `FUN_0060FAC0(...)`
  - wrapper into generic placement

## Best next step

The next best step is to chase the mode-`2` callback path directly:

- identify the virtual callback used in `FUN_005F0970(...)` case `2`
- find its concrete implementations/callers
- and decode the returned mask semantics

That should finally tell us what kind of policy-driven child record mode `2` actually represents.

