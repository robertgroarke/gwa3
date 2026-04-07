# GWCA GameThread Selector/Alpha Prepass Addendum

This pass goes one level deeper into the richer stage-1 family behind:

- `FUN_0069BCD0(...)`

The goal was to tighten three pieces that were still provisional in the last note:

- what `FUN_0069DCE0(...)` is actually decoding
- what `FUN_0069EE40(...)` is actually validating
- what the tiny class tables at `DAT_00A27BCC` / `DAT_00A27BD8` are doing

The result is that the richer stage-1 family now looks much more specifically like a selector/alpha-style prepass, not just a generic "scalar-family" peel.

## 1. `FUN_0069DCE0(...)`: DXT5-style alpha interpolation decoder

The decompile is now clean:

```cpp
uint FUN_0069dce0(uint *param_1)
{
  uint uVar1;
  uint uVar2;
  uint uVar3;
  
  uVar1 = *param_1;
  uVar2 = uVar1 & 0xff;
  uVar3 = uVar1 >> 8 & 0xff;
  uVar1 = uVar1 >> 0x10 & 7;
  if (uVar1 != 0) {
    if (uVar1 == 1) {
      return uVar3;
    }
    if (uVar3 < uVar2) {
      return ((8 - uVar1) * uVar2 + (uVar1 - 1) * uVar3 + 3) / 7;
    }
    if (5 < uVar1) {
      return -(uint)(uVar1 != 6) & 0xff;
    }
    uVar2 = ((6 - uVar1) * uVar2 + (uVar1 - 1) * uVar3 + 2) / 5;
  }
  return uVar2;
}
```

This is the same shape we already recognized earlier in the broader compressed-block seam:

- two endpoint bytes
- one 3-bit selector
- 7-step interpolation when `endpoint1 < endpoint0`
- 5-step interpolation plus the `0` / `255` special cases when `endpoint1 >= endpoint0`

So for this prepass, we no longer need to speak vaguely about "scalar-family decoding."

`FUN_0069DCE0(...)` is best understood as a DXT5-style alpha-value decoder over a compact block-local alpha representation.

## 2. `FUN_0069EE40(...)`: legal selector-family validator

The companion validator is also now explicit:

```cpp
undefined4 FUN_0069ee40(int param_1)
{
  int iVar1;
  
  iVar1 = *(int *)(param_1 + 4);
  if ((*(short *)(param_1 + 2) == (short)((uint)iVar1 >> 8)) &&
     (((((iVar1 == 0 || (iVar1 == 0x24924924)) || (iVar1 == 0x49249249)) ||
       ((iVar1 == 0x6db6db6d || (iVar1 == -0x6db6db6e)))) ||
      ((iVar1 == -0x4924924a || ((iVar1 == -0x24924925 || (iVar1 == -1)))))))) {
    return 1;
  }
  return 0;
}
```

The accepted `iVar1` patterns are exactly the eight repeated 3-bit selector families we had inferred before:

- `0x00000000`
- `0x24924924`
- `0x49249249`
- `0x6DB6DB6D`
- `0x92492492`
- `0xB6DB6DB6`
- `0xDB6DB6DB`
- `0xFFFFFFFF`

The negative constants in the decompile are just signed views of the upper four values.

So `FUN_0069EE40(...)` is not a generic sanity check. It is a precise validator for "whole block uses one repeated 3-bit selector value."

That is exactly the kind of structure a DXT5-style alpha prepass would want:

- one alpha endpoint pair
- one uniform selector family across the whole block

## 3. What `FUN_0069BCD0(...)` is really peeling

With those two helpers in hand, the stage-1 family gets a much tighter reading.

`FUN_0069BCD0(...)` does **not** seem to be peeling arbitrary selector-regular blocks.

It is peeling blocks that satisfy:

1. the record contains a legal repeated 3-bit selector family
2. that family decodes to one alpha/intensity-like scalar through `FUN_0069DCE0(...)`
3. that scalar is either:
   - `0`
   - or the learned dominant decoded byte for this run

That makes the classification logic inside `FUN_0069BCD0(...)` much easier to describe:

- class `0` = block rejected / no compact match
- class `1` = accepted block whose decoded scalar is `0`
- class `2` = accepted block whose decoded scalar equals the learned dominant byte

So the richer stage-1 family now looks much more like:

- a repeated-selector alpha/intensity peel

rather than a generic selector/value clustering pass.

## 4. Tiny class tables at `DAT_00A27BCC` / `DAT_00A27BD8`

I dumped both tables directly.

`DAT_00A27BCC`:

- `0x00A27BCC -> 1`
- `0x00A27BD0 -> 2`
- `0x00A27BD4 -> 2`

`DAT_00A27BD8`:

- `0x00A27BD8 -> 0`
- `0x00A27BDC -> 2`
- `0x00A27BE0 -> 3`

The next dwords after that roll immediately into string data (`"blocCount"`), so the meaningful table is only three entries long.

That lines up cleanly with the three-stage classification in `FUN_0069BCD0(...)`:

- class `0`
- class `1`
- class `2`

The simplest interpretation is:

- first table = bit widths for each class code
- second table = bit patterns for each class code

So the emitted class coding is:

- class `0` -> width `1`, bits `0`
- class `1` -> width `2`, bits `2`
- class `2` -> width `2`, bits `3`

That is exactly the sort of tiny prefix-ish code you would expect for:

- miss / fallback
- explicit zero
- dominant nonzero representative

## 5. Best current interpretation of the richer stage-1 family

Putting it all together, the best current description of `FUN_0069BCD0(...)` is:

- it peels blocks whose selector field is one uniform repeated 3-bit pattern
- interprets that pattern with a DXT5-style alpha decoder
- learns the dominant decoded nonzero value
- then emits a compact run/class stream with a tiny 3-class codebook

That is a much tighter result than the previous note.

We can now describe the stage-1 split this way:

- `FUN_0069CC40(...)` = trivial structural peel
- `FUN_0069BCD0(...)` = repeated-selector alpha/intensity peel

Both still feed into the shared heavier second-stage family afterward.

## 6. What this says about the upper-family formats

This also sharpens the earlier family grouping:

- `DXT4 / DXT5 / DXTA / DXTL` use the richer stage-1 family
- and that stage-1 family is now most naturally explained as an alpha/intensity-oriented compact peel

That is a better fit for the earlier format crosswalk than the older vague phrasing.

It suggests that the upper group shares not just "more complex compact families," but specifically:

- a meaningful repeated-selector scalar channel worth peeling before the dominant-color stage

## 7. Best next step

The strongest next move is to check how tightly that alpha/intensity reading is coupled to the upper format ids:

- revisit the exact `FUN_0069E870(...)` branch conditions around `FUN_0069BCD0(...)`
- compare them against the `DXT4 / DXT5 / DXTA / DXTL` ids one more time
- and, if useful, sample one caller-side builder that feeds this family

That should let us say whether this prepass is best named:

- a DXT5-style alpha peel
- an intensity-channel peel
- or a more engine-local hybrid of the two
