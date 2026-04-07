# GWCA GameThread Block State Helpers Addendum

This pass follows the four specialized block encoders from [GWCA_GameThread_DXTBlockConverterFamilies_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTBlockConverterFamilies_Addendum.md) down into their shared support layer:

- `FUN_0069EE40(...)`
- `FUN_0069DCE0(...)`
- `FUN_0069DD70(...)`
- `FUN_006A66C0(...)`

This is a strong upgrade over the previous pass, because the helper bodies are much more format-specific than the parent encoders alone. The cluster is no longer just “DXT-like by context.” It now contains helpers that look directly like:

- BC1 / DXT1 style color-block decoding
- DXT5-style alpha palette interpolation
- special-pattern validation over packed selector words
- and a compact representative-color block builder

So the compressed-format seam is now much tighter than before.

## 1. `FUN_0069EE40(...)`: packed selector-family validator

The decompile from the headless pass is very small:

```c
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

The key points are:

- it recognizes only eight legal values for the 32-bit selector word
- those values are structured repetition patterns:
  - `0x00000000`
  - `0x24924924`
  - `0x49249249`
  - `0x6DB6DB6D`
  - `0x92492492`
  - `0xB6DB6DB6`
  - `0xDB6DB6DB`
  - `0xFFFFFFFF`
- and it also requires a small cross-field consistency check between a 16-bit portion of the first dword and bits from the second dword

These are not generic magic numbers. They are exactly the kind of repeated packed-selector families you would expect when a compressor is looking for “whole block uses one selector class everywhere” or “whole block uses one uniform pattern family.”

That lines up very well with how `FUN_0069BCD0(...)` used it:

- validate whether a block belongs to one of a small number of compressible selector families
- then encode that whole family against a shared dominant representative value

Best current interpretation:

- `FUN_0069EE40(...)` is a selector-family validator / recognizer
- it supports the richer patterned encoders rather than doing value decoding itself

## 2. `FUN_0069DCE0(...)`: DXT5-style alpha interpolant decoder

The decompile for `FUN_0069DCE0(...)` is much more revealing:

```c
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

This is very close to a textbook DXT5 / BC3 alpha-palette decode pattern:

- two 8-bit endpoints in the low two bytes
- a 3-bit selector in bits `16..18`
- two interpolation modes:
  - 7-step interpolation when `endpoint0 > endpoint1`
  - 5-step interpolation plus explicit `0` / `255` cases when `endpoint0 <= endpoint1`

The special-case branch:

- selector `6` returns `0`
- selector `7` returns `255`

is especially strong evidence for that interpretation.

So while the broader parent encoder family was previously only “DXT-like,” this helper is now strong binary evidence that at least part of the cluster is operating over a BC3 / DXT5-style alpha block model.

## 3. `FUN_0069DD70(...)`: BC1 / DXT1-style color-block decoder

`FUN_0069DD70(...)` is similarly specific:

```c
uint FUN_0069dd70(uint *param_1)
{
  uint uVar1;
  uint uVar2;
  uint uVar3;
  uint local_1c [4];
  uint local_c;
  uint local_8;
  
  uVar1 = *param_1;
  uVar2 = uVar1 & 0xffff;
  local_c = ((&DAT_00a2c6c8)[uVar2 >> 0xb] << 10 | (&DAT_00a2c948)[uVar2 >> 5 & 0x3f]) << 10 |
            (&DAT_00a2c6c8)[uVar1 & 0x1f];
  local_8 = ((&DAT_00a2c6c8)[uVar1 >> 0x1b] << 10 | (&DAT_00a2c948)[uVar1 >> 0x15 & 0x3f]) << 10 |
            (&DAT_00a2c6c8)[uVar1 >> 0x10 & 0x1f];
  ...
  return local_1c[param_1[1] & 3];
}
```

The behavior matches a BC1 / DXT1-style color block:

- two packed 16-bit endpoints in RGB565-like form
- lookup tables expand 5-bit / 6-bit channels to fuller color precision
- four palette entries are built
- the branch on endpoint ordering chooses between the two interpolation families:
  - 4-color mode when endpoint0 > endpoint1
  - reduced mode when endpoint0 <= endpoint1
- the selector is just `param_1[1] & 3`, i.e. a 2-bit color-index choice

The returned values are packed opaque colors with `0xFF000000` ORed in where appropriate, which fits the “decode one palette entry from a BC1-like block” interpretation very well.

This is the strongest color-side anchor in the cluster:

- `FUN_0069DD70(...)` is not just compressed-texture adjacent
- it is directly doing BC1 / DXT1-style palette reconstruction from packed endpoints plus a 2-bit selector

That makes the surrounding encoders much easier to place:

- some are clearly alpha-family oriented
- some are clearly BC1-color-family oriented
- and `FUN_0069E870(...)` is very likely coordinating several such block subtypes together

## 4. `FUN_006A66C0(...)`: representative-color block builder

`FUN_006A66C0(...)` is larger but still structurally clear. It takes a representative color value, quantizes channel components down to packed endpoint space, chooses a small selector state, and writes back:

- a packed endpoint pair in `param_1[0]`
- a repeated 2-bit selector pattern in `param_1[1]`

The ending is especially informative:

```c
  uVar5 = uVar5 * 4 | uVar5;
  uVar5 = uVar5 << 4 | uVar5;
  uVar5 = uVar5 << 8 | uVar5;
  *param_1 = uVar7 << 0x10 | uVar6;
  param_1[1] = uVar5 << 0x10 | uVar5;
```

That is exactly the shape you would expect if the function is synthesizing a trivial or uniform compressed color block:

- one pair of packed endpoints
- one 2-bit selector replicated across the whole block

The quantization logic above it works channel-by-channel and looks like it is searching for a compact endpoint pair that approximates a target representative color under a BC1-style palette model.

This lines up very naturally with how `FUN_0069C3F0(...)` used it:

- pick a dominant representative value
- refine / stabilize that representative through `FUN_006A66C0(...)`
- then use that derived compact block as the basis for the encoded family

Best current interpretation:

- `FUN_006A66C0(...)` is a representative BC1-like block synthesizer from an RGB seed
- used by higher-level encoders to turn a dominant color into a compact block template

## 5. What this does to the earlier uncertainty

This helper pass materially upgrades the confidence level from the previous addendum.

Before this pass, the strongest safe wording was:

- “these look like DXT-style compressed block encoder families”

After this pass, the stronger safe wording is:

- the cluster definitely contains BC1 / DXT1-style color-block logic
- the cluster definitely contains BC3 / DXT5-style alpha interpolation logic
- and the parent encoder family is selecting among multiple specialized block encoders built on top of those primitives

That does not mean every remaining unnamed helper now has an exact external label, but it does mean the broad family identity is no longer just contextual inference.

## 6. Updated interpretation of the parent encoders

With these helpers in hand, the earlier four encoder families can be read more concretely:

- `FUN_0069B720(...)`
  - still looks like a narrow trivial-block encoder
  - probably handling a very low-entropy selector/alpha-side family
- `FUN_0069CC40(...)`
  - still looks like a sibling trivial-block encoder
  - likely tied to another compact endpoint/selector case
- `FUN_0069BCD0(...)`
  - now reads much more like a patterned selector-family encoder over BC1 / DXT1-like color blocks
  - especially because it leans on `FUN_0069EE40(...)`, `FUN_0069DCE0(...)`, and `FUN_0069DD70(...)`
- `FUN_0069C3F0(...)`
  - now reads more like a dominant-color block-family encoder that derives a compact representative block through `FUN_006A66C0(...)`

So the whole seam is starting to look like a real “choose best block-family codec” cluster, not a collection of unrelated utilities.

## 7. Remaining uncertainty

Even after this pass, a few things are still open:

- exactly which external compressed formats are grouped together under `FUN_0069E870(...)`
- whether some helpers correspond to BC2 / DXT3-style special cases in addition to BC1 / BC3-like families
- whether the bit flags `1`, `2`, `4`, and `8` map one-to-one onto named external block modes or onto internal optimization classes

But the uncertainty is narrower now. The unresolved question is no longer:

- “is this actually compressed-texture block logic?”

It is now:

- “which exact external block subfamily does each internal optimization class correspond to?”

## 8. Best next step

The strongest next move is to trace the orchestration side immediately around these helpers, especially:

- `FUN_0069D660(...)`
- more callers of `FUN_006A66C0(...)`
- and the parent `FUN_0069E870(...)` with these helper meanings in mind

That should let the research answer:

- which helper combinations are used together
- what each flag bit means in the final payload builder
- and whether the internal optimization classes line up cleanly with named BC1 / BC2 / BC3 / DXT-family block modes
