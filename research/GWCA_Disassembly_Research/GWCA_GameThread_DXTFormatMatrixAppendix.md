# GWCA GameThread DXT Format Matrix Appendix

This appendix consolidates the compressed-format work into one end-to-end matrix.

The goal is to make each `DXT*` row readable in one pass:

- internal format id
- capability word
- encoder stage bits and helpers
- decoder stage bits and helpers
- notable exception or post-pass

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This matrix is assembled from:

- [GWCA_GameThread_DXTFormatCrosswalk_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTFormatCrosswalk_Addendum.md)
- [GWCA_GameThread_ParentBranchGating_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ParentBranchGating_Addendum.md)
- [GWCA_GameThread_CompressedStageBitAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CompressedStageBitAppendix.md)
- [GWCA_GameThread_EncoderStageBitCorrespondence_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_EncoderStageBitCorrespondence_Addendum.md)

## Helper / Bit Key

The current closed helper-to-bit correspondence is:

| bit | encoder helper | decoder helper | current reading |
| --- | --- | --- | --- |
| `1` | `FUN_0069CC40(...)` | `FUN_0069DA70(...)` | structural trivial / sentinel compact stage |
| `2` | `FUN_0069B720(...)` | `FUN_0069CFE0(...)` | nibble-seeded compact stage |
| `4` | `FUN_0069BCD0(...)` | `FUN_0069D320(...)` | richer selector / alpha / intensity compact stage |
| `8` | `FUN_0069C3F0(...)` | `FUN_0069D660(...)` | representative / dominant-value compact stage |

## Per-Format Matrix

| format | id | capability | encoder bits | encoder helpers | decoder bits | decoder helpers | notable exception / note |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `DXT1` | `0x0F` | `0x71` | `1 + 8` | `FUN_0069CC40(...)`, `FUN_0069C3F0(...)` | `1 + 8` | `FUN_0069DA70(...)`, `FUN_0069D660(...)` | anchor BC1-like color-block member |
| `DXT2` | `0x10` | `0xB1` | `2 + 8` | `FUN_0069B720(...)`, `FUN_0069C3F0(...)` | `2 + 8` | `FUN_0069CFE0(...)`, `FUN_0069D660(...)` | `FUN_0069D8F0(...)` special post-pass at `256 x 256` |
| `DXT3` | `0x11` | `0xB1` | `2 + 8` | `FUN_0069B720(...)`, `FUN_0069C3F0(...)` | `2 + 8` | `FUN_0069CFE0(...)`, `FUN_0069D660(...)` | `FUN_0069D8F0(...)` special post-pass at `256 x 256` |
| `DXT4` | `0x12` | `0xB1` | `4 + 8` | `FUN_0069BCD0(...)`, `FUN_0069C3F0(...)` | `4 + 8` | `FUN_0069D320(...)`, `FUN_0069D660(...)` | upper richer selector / alpha-family member |
| `DXT5` | `0x13` | `0xB1` | `4 + 8` | `FUN_0069BCD0(...)`, `FUN_0069C3F0(...)` | `4 + 8` | `FUN_0069D320(...)`, `FUN_0069D660(...)` | strongest DXT5-style alpha-family fit |
| `DXTA` | `0x14` | `0xA1` | `4` | `FUN_0069BCD0(...)` | `4` | `FUN_0069D320(...)` | odd upper-family member: joins stage `4`, skips stage `8` |
| `DXTL` | `0x15` | `0x11` | `4 + 8` | `FUN_0069BCD0(...)`, `FUN_0069C3F0(...)` | `4 + 8` | `FUN_0069D320(...)`, `FUN_0069D660(...)` | special fallback-layout handling in parent shell |
| `DXTN` | `0x16` | `0x201` | `1 + 8` | `FUN_0069CC40(...)`, `FUN_0069C3F0(...)` | `1 + 8` | `FUN_0069DA70(...)`, `FUN_0069D660(...)` | shared-shell member, not a stray side branch |

## Stable Format Families

The strongest current grouping is:

- `DXT1`, `DXTN`
  - stage bits `1 + 8`
  - structural/sentinel stage plus representative-value stage
- `DXT2`, `DXT3`
  - stage bits `2 + 8`
  - nibble-seeded compact stage plus representative-value stage
  - plus the `FUN_0069D8F0(...)` `256 x 256` post-pass
- `DXT4`, `DXT5`, `DXTL`
  - stage bits `4 + 8`
  - richer selector-family stage plus representative-value stage
- `DXTA`
  - stage bit `4` only
  - upper-family sibling that drops out before the shared stage-`8` path

## Why This Matrix Matters

This closes a few lingering ambiguities at once.

First, the compressed shell is now readable in the same direction for every format:

- DDS / FourCC-facing format id
- parent capability word
- encoder helper selection
- serialized stage bits
- decoder helper replay

Second, it makes the outliers much easier to reason about:

- `DXTA` is not an unknown extra family
  - it is the upper stage-`4` sibling that does not take stage `8`
- `DXTN` is not an orphan id
  - it is a real `1 + 8` shared-shell member alongside `DXT1`

Third, it turns the helper notes into something directly usable for future reverse passes.

If a new caller or side helper mentions one of:

- `0x0F`
- `0x10`
- `0x11`
- `0x12`
- `0x13`
- `0x14`
- `0x15`
- `0x16`

we can now place it immediately against:

- capability word
- expected stage bits
- expected helper family
- known exception behavior

## Best Next Step

The best remaining codec-side question is now narrower than before.

It is no longer:

- which helper owns which bit
- or which format belongs to which shell family

It is now:

- why `DXTA / 0x14` exits after stage `4`
- and whether that reflects an alpha-only lane, an intensity-only lane, or another engine-local upper-family specialization

So the strongest next compressed-side target is still the `DXTA` divergence inside:

- `FUN_0069E870(...)`
- `FUN_0069E1C0(...)`
- and nearby `FUN_00689E90(...)` capability consumers
