# GWCA GameThread Encoder Stage Bit Correspondence Addendum

This pass closes the last obvious ambiguity in the compressed stage-bit model:

- which encoder helper sets which serialized stage bit

The relevant evidence was already present across the earlier encoder-family and parent-gating notes, so this pass is a consolidation/proof note rather than a fresh helper sweep.

## Source Artifacts

These results come from the already recovered encoder-side notes:

- [GWCA_GameThread_DXTBlockConverterFamilies_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTBlockConverterFamilies_Addendum.md)
- [GWCA_GameThread_ParentBranchGating_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ParentBranchGating_Addendum.md)
- [GWCA_GameThread_CompressedStageBitAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CompressedStageBitAppendix.md)

## Main Result

The encoder-side bit correspondence is now strong enough to state directly.

The four compact encoder helpers under `FUN_0069E870(...)` map to serialized stage bits exactly as follows:

| encoder helper | writes | decode-side inverse | current family reading |
| --- | --- | --- | --- |
| `FUN_0069CC40(...)` | `*param_1 |= 1` | `FUN_0069DA70(...)` | structural trivial/sentinel stage |
| `FUN_0069B720(...)` | `*param_1 |= 2` | `FUN_0069CFE0(...)` | nibble-seeded compact stage |
| `FUN_0069BCD0(...)` | `*param_1 |= 4` | `FUN_0069D320(...)` | richer selector/alpha/intensity stage |
| `FUN_0069C3F0(...)` | `*param_1 |= 8` | `FUN_0069D660(...)` | representative/dominant-value stage |

So the stage-bit taxonomy is no longer only a decoder-side inference.

It is now supported at both ends:

- encoder helpers set the exact bit
- decoder helpers read the exact bit

That makes the staged shell much more secure as a recovered architecture model.

## Direct Encoder-Side Evidence

### `FUN_0069CC40(...)` -> bit `1`

The earlier encoder-family note already captured the decisive line:

- `FUN_0069CC40(...)`
  - sets:
    - `*param_1 |= 1`

Its accepted family was the narrow structural trivial/sentinel family, which matches the current decode-side reading of bit `1` as:

- `FUN_0069DA70(...)`
  - narrow sentinel-block filler

So bit `1` is now cleanly paired:

- encode:
  - `FUN_0069CC40(...)`
- decode:
  - `FUN_0069DA70(...)`

### `FUN_0069B720(...)` -> bit `2`

The same encoder note recorded:

- `FUN_0069B720(...)`
  - sets:
    - `*param_1 |= 2`

That helper was already described as the trivial nibble-block RLE encoder, which aligns well with the current decode-side bit-`2` family:

- `FUN_0069CFE0(...)`
  - nibble-seeded compact block family

So bit `2` is now cleanly paired:

- encode:
  - `FUN_0069B720(...)`
- decode:
  - `FUN_0069CFE0(...)`

### `FUN_0069BCD0(...)` -> bit `4`

The richer selector-family encoder note already captured:

- `FUN_0069BCD0(...)`
  - sets:
    - `*param_1 |= 4`

That helper was the richer patterned selector/intensity/alpha-family prepass, which lines up with the current decode-side bit-`4` family:

- `FUN_0069D320(...)`
  - byte-seeded upper compact family

So bit `4` is now cleanly paired:

- encode:
  - `FUN_0069BCD0(...)`
- decode:
  - `FUN_0069D320(...)`

### `FUN_0069C3F0(...)` -> bit `8`

The dominant-family encoder note already captured:

- `FUN_0069C3F0(...)`
  - sets:
    - `*param_1 |= 8`

That helper was the richer dominant-value / representative-value stage, which lines up with the current decode-side bit-`8` family:

- `FUN_0069D660(...)`
  - representative-block replay stage

So bit `8` is now cleanly paired:

- encode:
  - `FUN_0069C3F0(...)`
- decode:
  - `FUN_0069D660(...)`

## Why the Correspondence Is Credible

This is not just “same number on both sides.”

The pairing is structurally credible for all four bits:

- bit `1`
  - narrow trivial structural family on encode
  - narrow trivial filler family on decode
- bit `2`
  - nibble-oriented compact family on encode
  - nibble-seeded compact family on decode
- bit `4`
  - richer selector/intensity family on encode
  - richer byte-seeded upper family on decode
- bit `8`
  - dominant/representative-value family on encode
  - representative-block replay family on decode

So the pairing holds both:

- numerically
- and behaviorally

That is the strongest kind of correspondence we can ask for at this level of recovery.

## Parent-Side Gating with Bits in Mind

Once the helper-to-bit mapping is fixed, the parent branch note becomes even cleaner.

### `DXT1 / 0x0F`

- stage `1`:
  - `FUN_0069CC40(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

### `DXT2 / 0x10`

- stage `2`:
  - `FUN_0069B720(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

### `DXT3 / 0x11`

- stage `2`:
  - `FUN_0069B720(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

### `DXT4 / 0x12`

- stage `4`:
  - `FUN_0069BCD0(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

### `DXT5 / 0x13`

- stage `4`:
  - `FUN_0069BCD0(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

### `DXTA / 0x14`

- stage `4`:
  - `FUN_0069BCD0(...)`
- no stage `8`

### `DXTL / 0x15`

- stage `4`:
  - `FUN_0069BCD0(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

### `DXTN / 0x16`

- stage `1`:
  - `FUN_0069CC40(...)`
- stage `8`:
  - `FUN_0069C3F0(...)`

That means the current per-format stage map can now be written directly in bits rather than only in helper names.

## Updated Stage-Bit Taxonomy

The strongest current compressed-family taxonomy is now:

- stage bit `1`
  - structural trivial/sentinel compact family
- stage bit `2`
  - nibble-seeded compact family
- stage bit `4`
  - richer selector/intensity-family compact stage
- stage bit `8`
  - representative/dominant-value compact stage

And the shell now reads as:

- parent encoder chooses one or more compact stages
- helper sets stage bit(s)
- stage word is serialized per level
- parent decoder reads that stage word
- sibling decode helper replays the claimed family

That is a complete enough stage-bit loop to treat the shell as recovered at the architecture level.

## What This Changes

Before this pass, the stage-bit appendix still had one deliberate caution:

- exact encoder-helper to stage-bit correspondence was not yet frozen in one place

After this pass, that gap is closed.

The compressed shell now has:

- exact decoder-side bit mapping
- exact encoder-side bit mapping
- and explicit parent-side per-format gating

So the remaining uncertainty is no longer:

- which helper owns which bit

It is now narrower:

- whether every engine-local family can be mapped cleanly to one named external compressed subtype

## Best Next Step

The next best step is to turn this from a helper/bit taxonomy into a final format matrix.

The strongest follow-up is:

1. add a compact per-format appendix or table for:
   - `DXT1`
   - `DXT2`
   - `DXT3`
   - `DXT4`
   - `DXT5`
   - `DXTA`
   - `DXTL`
   - `DXTN`
2. express each row as:
   - capability word
   - encoder stage bits
   - decoder stage bits
   - notable post-pass / exception

That would make the compressed-family branch readable end-to-end the same way the high-band and emitted-segment branches now are.
