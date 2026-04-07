# GWCA GameThread DXT Format Crosswalk Addendum

This pass is a synthesis step rather than a brand-new deep helper dive. The goal is to connect three strands that were already individually strong:

1. the DDS import/export seam from [GWCA_GameThread_PaletteIntegrationAndDDSSeam_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_PaletteIntegrationAndDDSSeam_Addendum.md)
2. the block-state and compact-family helpers from:
   - [GWCA_GameThread_BlockStateHelpers_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_BlockStateHelpers_Addendum.md)
   - [GWCA_GameThread_CompressedBlockOrchestration_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CompressedBlockOrchestration_Addendum.md)
   - [GWCA_GameThread_SiblingDecodeFamilies_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_SiblingDecodeFamilies_Addendum.md)
3. the static format-descriptor layer from [GWCA_GameThread_FormatSelectionAndCallbackPlanes_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_FormatSelectionAndCallbackPlanes_Addendum.md)

The result is a much cleaner crosswalk for the internal format ids `0x0F..0x16`.

## 1. DDS FourCC to internal format id

From the parser in `FUN_006A0330(...)`, the compressed DDS mapping is:

- `DXT1` -> `0x0F`
- `DXT2` -> `0x10`
- `DXT3` -> `0x11`
- `DXT4` -> `0x12`
- `DXT5` -> `0x13`
- `DXTA` -> `0x14`
- `DXTL` -> `0x15`
- `DXTN` -> `0x16`

That means the remaining internal-family work can now be phrased in external names instead of only numeric ids.

## 2. What is now solid versus still inferred

What is binary-strong at this point:

- `FUN_0069DD70(...)` is a BC1 / DXT1-style color-block decoder
- `FUN_0069DCE0(...)` is a DXT5 / BC3-style alpha interpolant decoder
- `FUN_0069E1C0(...)` is the parent compact-family orchestration routine
- the compact-family decoders are:
  - `FUN_0069DA70(...)` for flag `1`
  - `FUN_0069CFE0(...)` for flag `2`
  - `FUN_0069D320(...)` for flag `4`
  - `FUN_0069D660(...)` for flag `8`
- `FUN_0069D8F0(...)` is a special post-pass only for formats `0x10 / 0x11` at `256 x 256`

What is still partly inferred:

- the exact external meaning of the sentinel flag-`1` family
- whether `DXTA`, `DXTL`, and `DXTN` correspond exactly to conventional external names or to engine-local variants layered over known BC/DXT styles

So the table below is a confidence-aware crosswalk rather than a claim of absolute one-to-one identity for every subpath.

## 3. Internal format crosswalk

### `0x0F` = `DXT1`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- `FUN_0069D660(...)` receives a boolean `param_3 == 0x0F`
- `FUN_0069DD70(...)` is BC1 / DXT1-style color-block decode
- `FUN_006A66C0(...)` builds a BC1-like representative color block

Best interpretation:

- `DXT1` is the anchor format for the BC1-like color-block model in this cluster
- it participates in the broader flag-`8` representative-block family
- and likely defines the baseline color-block semantics that several sibling compact families reuse or emulate

Confidence:

- high

### `0x10` = `DXT2`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- `FUN_0069E1C0(...)` gates flag `2` specifically to `0x10 / 0x11`
- `FUN_0069D8F0(...)` post-pass is also limited to `0x10 / 0x11` and only under `256 x 256`

Best interpretation:

- `DXT2` belongs to the format pair that uses the flag-`2` compact family
- that family is the 4-bit-seeded compact decoder `FUN_0069CFE0(...)`
- and this pair also carries the special tiled/swizzled post-pass at one specific size

Confidence:

- high on the format id
- medium-high on the meaning of the compact-family path

### `0x11` = `DXT3`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- shares the same flag-`2` gate and `FUN_0069D8F0(...)` post-pass with `0x10`

Best interpretation:

- `DXT3` shares the same compact-family machinery as `DXT2` in this engine
- specifically the flag-`2` 4-bit-seeded path
- with the same special post-layout transform for the `256 x 256` case

This grouping makes sense architecturally even before exact payload naming, because DXT2 and DXT3 are very close relatives.

Confidence:

- high

### `0x12` = `DXT4`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- `FUN_0069E1C0(...)` gates flag `4` to `0x12 / 0x13 / 0x14 / 0x15`
- `FUN_0069D320(...)` is the flag-`4` 8-bit-seeded compact-family decoder
- `FUN_0069DCE0(...)` is a strong DXT5/BC3-style alpha interpolant helper in the same broader cluster

Best interpretation:

- `DXT4` belongs to the upper compressed-family group that uses the flag-`4` 8-bit-seeded path
- and likely sits closer to the alpha-interpolated side of the family than `DXT2 / DXT3`

Confidence:

- high on id
- medium-high on family role

### `0x13` = `DXT5`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- shares the same flag-`4` gate as `0x12 / 0x14 / 0x15`
- the helper `FUN_0069DCE0(...)` is strongly DXT5-style in structure

Best interpretation:

- `DXT5` is the clearest member of the upper family
- it is very likely one of the primary consumers of the alpha-interpolation helper cluster
- and belongs to the flag-`4` 8-bit-seeded compact-family group in `FUN_0069E1C0(...)`

Confidence:

- high

### `0x14` = `DXTA`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- grouped with `0x12 / 0x13 / 0x15` in the flag-`4` path

Best interpretation:

- `DXTA` is treated by the engine as a sibling of the `DXT4 / DXT5 / DXTL` family
- and uses the same flag-`4` 8-bit-seeded compact path

What remains unclear is whether `DXTA` here corresponds to a standard external convention or to an engine-specific variant that still reuses the same compressed-family substrate.

Confidence:

- high on grouping
- medium on exact external semantics

### `0x15` = `DXTL`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- grouped with `0x12 / 0x13 / 0x14` for flag `4`
- treated specially in `FUN_0069E1C0(...)` fallback layout

Best interpretation:

- `DXTL` is in the upper compressed-family group
- but unlike the others, it triggers special fallback-copy behavior in the parent rebuild routine

So this id is especially likely to be an engine-local or format-local variant whose block layout differs enough to warrant special fallback handling even though it shares the same compact-family decoder.

Confidence:

- high on group membership
- medium on exact external meaning

### `0x16` = `DXTN`

Evidence:

- mapped directly by `FUN_006A0330(...)`
- appears in the DDS-facing format-id map
- but has not yet surfaced in the same level of compact-family-specific decode work as `0x10..0x15`

Best interpretation:

- `DXTN` is present in the format system and DDS seam
- but its exact relationship to the currently reversed compact families remains less explicit than the others

This is the main holdout in the current crosswalk.

Confidence:

- high on id
- low-to-medium on exact decode-family placement

## 4. Compact-family crosswalk

The compact-family side now reads as:

- flag `1`
  - `FUN_0069DA70(...)`
  - emits a fixed sentinel block:
    - `0xFFFFFFFE`
    - `0xFFFFFFFF`
  - likely a degenerate or reserved compressed-block case

- flag `2`
  - `FUN_0069CFE0(...)`
  - 4-bit seed expanded into a compact local template
  - used specifically by:
    - `DXT2`
    - `DXT3`

- flag `4`
  - `FUN_0069D320(...)`
  - 8-bit seed expanded into a compact local template
  - used specifically by:
    - `DXT4`
    - `DXT5`
    - `DXTA`
    - `DXTL`

- flag `8`
  - `FUN_0069D660(...)`
  - representative-block family synthesized through `FUN_006A66C0(...)`
  - broader capability-gated family
  - clearly tied to BC1 / DXT1-style compressed color logic

- chunk flag `0x10`
  - `FUN_0069D8F0(...)`
  - special block swizzle/post-pass
  - only for:
    - `DXT2`
    - `DXT3`
  - and only in the `256 x 256` case

## 5. Strongest current interpretation

The cleanest current model is:

- `DXT1`
  - anchors the BC1-like color-block logic
- `DXT2 / DXT3`
  - form one compact-family pair using flag `2`
  - plus a size-specific post-swizzle
- `DXT4 / DXT5 / DXTA / DXTL`
  - form the upper compact-family group using flag `4`
  - with DXT5-style alpha interpolation visible in the helper cluster
- `DXTN`
  - is definitely part of the DDS / internal format map
  - but still needs another pass to place confidently inside the compact-family decode graph

That is a much stronger state than before this pass. The format ids no longer float independently of the decode families.

## 6. Best next step

The strongest next move is to finish the one remaining weak spot:

- trace `DXTN` / `0x16` specific callers and gates
- and inspect more `FUN_0069E1C0(...)` neighbors / callers to see where `0x16` diverges from the already-mapped `0x10..0x15` families

The best concrete targets are:

- more callers around the DDS seam:
  - `FUN_006A0330(...)`
  - `FUN_006A0630(...)`
- more format-capability users of:
  - `FUN_00689E90(...)`
- and parent/neighbor functions around:
  - `FUN_0069E1C0(...)`
  - `FUN_0069E870(...)`

That should let the next pass either place `DXTN` cleanly or prove it follows a distinct compressed-family branch.
