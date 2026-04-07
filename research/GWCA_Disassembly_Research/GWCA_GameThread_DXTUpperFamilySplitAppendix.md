# GWCA GameThread DXT Upper Family Split Appendix

This appendix consolidates the current answer to the remaining upper-family question:

- why does `DXTA / 0x14` join the richer upper compressed family at stage `4`
- but then drop out before the shared stage `8` companion path?

The result is now strong enough to state plainly:

- `DXTA` keeps the upper family's repeated-selector scalar/alpha stream
- `DXTA` does not carry the companion BC1-style color-block stream

So `DXTA` is no longer best treated as:

- an unexplained odd format

It is better treated as:

- the scalar/alpha-only member of the upper DXT-family branch

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_DXTA_PartialMembership_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTA_PartialMembership_Addendum.md)
- [GWCA_GameThread_BuilderSerializationSplit_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_BuilderSerializationSplit_Addendum.md)
- [GWCA_GameThread_BlockSemanticBridge_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_BlockSemanticBridge_Addendum.md)
- [GWCA_GameThread_DXTFormatMatrixAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTFormatMatrixAppendix.md)

## Upper-Family Membership

The upper family currently splits into:

### Stage-`4` membership

- `DXT4 / 0x12`
- `DXT5 / 0x13`
- `DXTA / 0x14`
- `DXTL / 0x15`

These all take:

- encoder stage `4` through `FUN_0069BCD0(...)`
- decoder stage `4` through `FUN_0069D320(...)`

### Stage-`8` membership

- `DXT4 / 0x12`
- `DXT5 / 0x13`
- `DXTL / 0x15`

These also take:

- encoder stage `8` through `FUN_0069C3F0(...)`
- decoder stage `8` through `FUN_0069D660(...)`

### Odd member

- `DXTA / 0x14`

`DXTA` joins stage `4` but not stage `8`.

## Exact Capability Split

The dividing line is the capability trait behind:

- `FUN_00689E90(format) & 0x210`

Current recovered capability words:

- `DXT4` -> `0xB1`
- `DXT5` -> `0xB1`
- `DXTA` -> `0xA1`
- `DXTL` -> `0x11`

So:

- `DXT4`, `DXT5`, `DXTL`
  - `capability & 0x210 != 0`
- `DXTA`
  - `capability & 0x210 == 0`

That is why `DXTA` drops out before the shared stage-`8` family on both sides of the shell.

## Stream-Level Meaning

The stream interpretation is now stable enough to write directly.

### Primary stream

The always-present upper-family side is:

- repeated-selector scalar/alpha-style block stream

Its current semantic anchors are:

- `FUN_0069DCE0(...)`
- `FUN_0069EE40(...)`
- `FUN_0069CFE0(...)`
- `FUN_0069D320(...)`

This is the side `DXTA` keeps.

### Companion stream

The stage-`8` side is:

- BC1-style color-block stream

Its current semantic anchors are:

- `FUN_0069DD70(...)`
- `FUN_006A66C0(...)`
- `FUN_0069DA70(...)`
- `FUN_0069D660(...)`
- `FUN_0069C3F0(...)`

This is the side gated by `0x210`, and the side `DXTA` lacks.

## Builder-Side Consequence

On the builder side in `FUN_0069E870(...)`, the split is not only helper selection.

It is visible in final serialized payload layout.

The builder:

- always has the primary-side record path available for the upper-family stage-`4` branch
- only serializes the appended companion two-word-per-block payload when:
  - `capability & 0x210 != 0`

That means:

### `DXTA`

- serializes the primary scalar/alpha-side payload
- does not serialize the appended companion BC1-style block payload

### `DXT4`, `DXT5`, `DXTL`

- serialize the primary scalar/alpha-side payload
- also serialize the appended companion BC1-style block payload

So the `DXTA` divergence is visible in final output layout, not only in internal branch shape.

## Decoder-Side Consequence

On the decoder side in `FUN_0069E1C0(...)`, the same split is mirrored.

The decoder:

- rebuilds the stage-`4` upper-family side for:
  - `DXT4`
  - `DXT5`
  - `DXTA`
  - `DXTL`
- rebuilds the stage-`8` companion side only when:
  - `FUN_00689E90(format) & 0x210 != 0`

So the mirrored decode-side reading is:

### `DXTA`

- yes to upper-family stage `4`
- no to companion stage `8`

### `DXT4`, `DXT5`, `DXTL`

- yes to upper-family stage `4`
- yes to companion stage `8`

That makes the split symmetric across:

- builder
- serialized payload
- decoder

## Best Current Reading

The safest strong statement now is:

- `DXTA` is the upper-family scalar/alpha-only variant
- `DXT4`, `DXT5`, and `DXTL` are the upper-family dual-stream variants

That is a stronger and cleaner result than the earlier partial-membership wording alone, because it is now backed by:

1. exact stage membership
2. exact capability gating
3. final serialized layout
4. stream-semantic anchor helpers

## Practical Classifier

When a new helper or caller touches the upper DXT-family branch, the fastest current classifier is:

1. if it is only on the stage-`4` / repeated-selector scalar side:
   - it can still be a `DXTA` path
2. if it requires the stage-`8` / BC1-style companion side:
   - it excludes `DXTA`
3. if it tests the capability trait behind `& 0x210`:
   - it is probably the actual upper-family split hinge

That should make future `DXTA`-related discoveries much faster to place.

## Best Next Step

The remaining question is no longer the family split itself.

It is the first concrete downstream consumer that uses the reconstructed pair as:

- scalar/alpha-side block stream
- plus BC1-style companion color stream

So the next best target is:

- a caller or downstream helper above `FUN_0069E1C0(...)` that reads both reconstructed planes together

That should let the next pass move from:

- stable stream identities

to:

- the exact engine role of the paired upper-family block layout.
