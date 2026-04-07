## `Gw.exe` Frame Callback Mode-2 Policy Callback Addendum

This pass continues from the measurability-and-placement note by chasing the remaining high-value unknown in the region-layout system:

- the mode-`2` policy / virtual callback

To do that, I dumped the subobject vtable installed by `FUN_005EF740(...)` and decompiled its first callback target:

- `PTR_FUN_00A1F2E4`
- `FUN_005EFB10`

## Source artifacts

These results come from:

- [gw_dump_vtable_a1f2e4_temp101.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_dump_vtable_a1f2e4_temp101.log)
- [gw_decomp_mode2_callback_temp101.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_mode2_callback_temp101.log)
- [gw_decomp_layout_policy_temp100.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_layout_policy_temp100.log)

## High-level result

The important result is actually a simplification.

Mode `2` does not appear to route into a large, rich policy object with many virtual methods.
The subobject initialized by `FUN_005EF740(...)` exposes a first slot callback at:

- `0x00A1F2E4 -> FUN_005EFB10`

and that callback is tiny.

So the mode-`2` path is now best understood as:

- a mask-normalization policy shim

not:

- a deep alternative layout engine

## `PTR_FUN_00A1F2E4`: effectively a one-slot policy object

The vtable dump shows:

- `00A1F2E4 -> 005EFB10`

and then immediately falls into a string blob, not a long contiguous function-pointer table.

That means the subobject installed by `FUN_005EF740(...)` is not behaving like a large multi-method interface.
For the current reverse path, the only meaningful callback slot we can see is:

- `FUN_005EFB10`

This matches what `FUN_005F0970(...)` was doing in layout mode `2`:

```cpp
(**(code **)*in_ECX)(&param_1, mask);
```

So this is the exact policy callback used by mode `2`.

## `FUN_005EFB10(out_mask, mask)`: mask normalizer

This callback is very small:

```cpp
if (-1 < (char)mask) {
    mask &= 0xFFFFFFD1;
}
if ((mask & 0x100) == 0) {
    mask &= 0xFFFFFFAC;
}
*out_mask = mask;
```

The important point is not the precise bit constants yet.
It is the shape of the logic:

- it does not look up child objects
- it does not do geometry
- it does not recurse
- it only filters / normalizes the incoming mask

So layout mode `2` is now much more specifically:

- callback-normalized mask layout

or:

- policy-filtered dimension-mask layout

## What this does to mode `2`

Before this pass, mode `2` looked like:

- policy / virtual layout

That was accurate, but still left open the possibility that there was a large hidden policy subsystem behind it.

After this pass, the cleaner reading is:

- mode `2` asks a small policy callback to sanitize/filter the incoming mask bits
- then uses the returned mask to decide which dimension should be taken from record field `+0x04`

So mode `2` is really closer to:

- mask-filtered fixed-dimension layout

than to:

- full dynamic measurement or full custom child-object layout

## Why this is a useful correction

This matters because it narrows the remaining ambiguity dramatically.

The big unknown is no longer:

- what complicated callback object is mode `2` talking to?

It is now:

- what do the specific mask bits mean after `FUN_005EFB10(...)` filters them?

That is a much smaller and better-defined reverse target.

## Updated layout mode taxonomy

With this pass, the current layout taxonomy is:

- `0`
  - stacked aggregate layout along one axis
- `1`
  - measured live-child layout, gated by child flag `0x200`
- `2`
  - mask-filtered fixed-dimension layout through `FUN_005EFB10(...)`
- `3`
  - stacked aggregate layout along the orthogonal axis

That is the strongest mode summary so far.

## What remains unresolved in mode `2`

This pass does not yet assign human-readable names to the mask bits that survive filtering in `FUN_005EFB10(...)`.

In `FUN_005F0970(...)`, the returned mask is then interpreted through checks like:

- `(*mask & 0x0C) != 0`
- `(*mask & 0x11) != 0`

and those checks decide whether the fixed dimension from record field `+0x04` applies to one axis or the other.

So the remaining unknown is:

- whether those masks represent horizontal vs vertical selection
- or leading vs trailing edge participation
- or some closely related dimension-selection policy

## Updated working model

The cleanest current model is now:

- the host/control instance owns a recursive region-layout system
- most layout behavior is driven by record flags and shared placement helpers
- mode `2` is a narrow policy escape hatch that only filters the dimension mask before applying a fixed-size rule

That is a much more constrained and manageable design than it first appeared.

## Best next step

The next best step is to decode the post-filter mask semantics in mode `2` by tracing:

- the specific checks in `FUN_005F0970(...)`
- more callers or uses of `FUN_005EFB10(...)`
- and nearby record-flag producers that write the mode-`2` child mask field

That should let us finally rename the surviving mask bits in a human-readable way.

