## `Gw.exe` Frame Callback Mode-2 Mask Families Addendum

This pass continues from the mode-`2` policy-callback note by tightening the semantics of the filtered mask bits that survive into `FUN_005F0970(...)`.

The key inputs are:

- `FUN_005EFB10(out_mask, mask)`
- the mode-`2` branch inside `FUN_005F0970(...)`

## Source artifacts

These results come from:

- [gw_decomp_mode2_callback_temp101.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_mode2_callback_temp101.log)
- [gw_decomp_region_layout_helpers_temp99.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_layout_helpers_temp99.log)
- [gw_findcallers_005efb10_temp102.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_005efb10_temp102.log)

## High-level result

This pass upgrades mode `2` from:

- mask-filtered fixed-dimension layout

to something more specific:

- mask-filtered fixed-dimension layout with two axis families

The important point is that the post-filter checks in `FUN_005F0970(...)` are not arbitrary:

- `0x0C` selects one dimension family
- `0x11` selects the other dimension family

So the mode-`2` callback is effectively deciding which axis family is allowed to consume the fixed dimension stored at child record `+0x04`.

## The callback again

`FUN_005EFB10(...)` is:

```cpp
if ((char)mask >= 0) {
    mask &= 0xFFFFFFD1;
}
if ((mask & 0x100) == 0) {
    mask &= 0xFFFFFFAC;
}
*out_mask = mask;
```

The key is what survives after those two filters.

## What `FUN_005F0970(...)` does with the filtered mask

In mode `2`, the code is:

```cpp
filtered = callback(mask);
if ((filtered & 0x0C) == 0) {
    if ((filtered & 0x11) != 0) {
        out.h = record[+0x04];
    }
} else {
    out.w = record[+0x04];
}
```

So whatever the individual low bits mean in the broader engine, this layout path uses them in two grouped families:

- `0x0C` => first-dimension selector
- `0x11` => second-dimension selector

The safest interpretation is:

- `0x0C` = width-axis family
- `0x11` = height-axis family

because `out.w` and `out.h` are exactly what the branch writes.

## How the callback partitions the mask families

The callback’s two filters create four behavior buckets:

### Case 1: non-negative mask, `0x100` clear

First filter keeps only low-byte bits:

- `0x01`
- `0x10`
- `0x40`
- `0x80`

Second filter then keeps only the intersection with:

- `0x04`
- `0x08`
- `0x20`
- `0x80`

So the low-byte intersection is effectively:

- `0x80`

Meaning:

- neither `0x0C` nor `0x11` survives

For mode `2`, that means:

- no fixed width
- no fixed height

So this is effectively a neutral/no-dimension-selection bucket.

### Case 2: non-negative mask, `0x100` set

Only the first filter applies, so the low surviving bits are:

- `0x01`
- `0x10`
- `0x40`
- `0x80`

Now:

- `0x11` can survive
- `0x0C` cannot

So for mode `2`, this bucket selects:

- second-dimension family only

That strongly supports:

- height-axis family

### Case 3: negative mask, `0x100` clear

The first filter does not apply.
The second filter keeps low-byte bits:

- `0x04`
- `0x08`
- `0x20`
- `0x80`

Now:

- `0x0C` can survive
- `0x11` cannot

So for mode `2`, this bucket selects:

- first-dimension family only

That strongly supports:

- width-axis family

### Case 4: negative mask, `0x100` set

Neither filter removes the low selector families.

So:

- `0x0C` can survive
- `0x11` can survive

For mode `2`, this is the only bucket where both dimension families remain potentially available.

## What this means for mode `2`

This gives the cleanest current description yet:

- mode `2` stores one fixed dimension at child record `+0x04`
- a compact policy callback chooses which axis family may consume that dimension
- the selection is controlled by:
  - sign-like state in the incoming mask
  - the presence or absence of bit `0x100`

So mode `2` is no longer just:

- callback-selected dimension layout

It is more specifically:

- fixed-dimension layout with callback-filtered width/height family selection

## What we can now say without overreaching

Binary-supported claims:

- `0x0C` is the branch family that writes the first output dimension
- `0x11` is the branch family that writes the second output dimension
- the callback gates those families using the sign of the incoming mask and bit `0x100`

Still not safe to claim yet:

- exact user-facing meaning of each individual bit
- whether the input sign is truly semantic sign or just a packed flag convention
- whether `0x100` means "allow height family", "secondary axis", "vertical", or some more abstract policy label

So the current interpretation should stay at the family level, not the single-bit level.

## Updated layout taxonomy

With this pass, the mode summary is now:

- `0`
  - stacked aggregate layout along one axis
- `1`
  - measured live-child layout, gated by child flag `0x200`
- `2`
  - fixed-dimension layout with callback-filtered width/height family selection
- `3`
  - stacked aggregate layout along the orthogonal axis

That is the strongest mode-`2` description so far.

## One useful negative result

The caller search for `FUN_005EFB10(...)` found only the vtable slot reference.
That matters because it confirms this callback is not reused all over the image as a generic utility.

So this mask-filtering behavior really is part of the specific subobject/type used by the control-instance region table, not a widespread engine helper.

## Best next step

The next best step is to find where the mode-`2` child record mask at `+0x24` is produced or initialized.

That should tell us:

- what the incoming sign-like state actually encodes
- what bit `0x100` means in the producer’s own logic
- and whether those width/height families correspond to horizontal/vertical, primary/secondary, or another naming scheme

