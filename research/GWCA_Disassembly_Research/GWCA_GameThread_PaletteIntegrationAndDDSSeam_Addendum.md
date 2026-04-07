## `Gw.exe` Frame Callback Palette Integration And DDS Seam Addendum

This pass followed the next palette-side integration helpers:

- `FUN_006A0330`
- `FUN_006A0280`
- `FUN_006A01D0`
- `FUN_006A0630`
- `FUN_0069E870`

The goal was to determine:

- how the palette-side helpers tie back into the broader image-transfer family
- whether the same shared `ImgMem` sizing logic reappears here
- and whether the palette/conversion path is connected to a concrete import/export seam

## Source artifacts

These results come from:

- [gw_decomp_imgpal_integration_temp131.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgpal_integration_temp131.log)

## High-level result

This pass gave two strong upgrades:

1. the palette-side cluster is not floating on its own
   - it is tied directly into a DDS-oriented import/export seam
2. the same shared `FUN_0068CC20(...)` level-size logic is reused here too
   - especially around DXT-family formats

The best current decomposition is:

- `FUN_006A0330(...)`
  - DDS-header parser/validator that maps DDS pixel-format state into the engine's internal format ids
- `FUN_006A0630(...)`
  - DDS-header/buffer builder for internal image data, especially DXT-family outputs
- `FUN_006A0280(...)`
  - six-bank/pointer-table side size walk over shared level-size logic
- `FUN_006A01D0(...)`
  - single-bank side size walk over shared level-size logic
- `FUN_0069E870(...)`
  - palette/conversion-side payload builder for DXT-family and related formats

So the current best model is:

- the `ImgMem` / `ImgPal` family is not just internal scratch machinery
- it participates in real DDS-oriented import/export and conversion flows

## `FUN_006A0330(...)`: DDS parser / internal-format mapper

This helper is the strongest anchor in the pass.

### What it does

Its behavior is:

- require at least `0x80` bytes
- validate the magic:
  - `0x20534444` = `"DDS "`
- validate expected DDS header sizes and flag combinations
- inspect the DDS pixel-format fields
- map them into the engine's internal format ids:
  - uncompressed layouts through:
    - `FUN_0068AD90(...)`
  - compressed FourCC cases:
    - `DXT1` -> `0x0F`
    - `DXT2` -> `0x10`
    - `DXT3` -> `0x11`
    - `DXT4` -> `0x12`
    - `DXT5` -> `0x13`
    - `DXTA` -> `0x14`
    - `DXTL` -> `0x15`
    - `DXTN` -> `0x16`
- extract:
  - width
  - height
  - mip/level count
  - an auxiliary mode value in `*param_6`
- use:
  - `FUN_0068AE30(...)`
  - `FUN_0068B010(...)`
  - `FUN_0068CC20(...)`
  to ensure the declared DDS payload actually fits within the buffer
- clamp the level count through:
  - `FUN_0068CBB0(...)`

### Best interpretation

The strongest reading is:

- `FUN_006A0330(...)` is a DDS parser/validator that converts DDS header state into the engine's internal format and level-layout model

That is a major anchor for the whole reverse chain, because it shows the same image-format subsystem we have been mapping is used directly at a concrete file-format boundary.

## `FUN_006A0630(...)`: DDS builder / exporter side

This helper is the clean inverse anchor to `FUN_006A0330(...)`.

### What it does

Its behavior is:

- require a format whose flags support the path
- compute total payload bytes across all levels using:
  - `FUN_0068CC20(...)`
- allocate/write a DDS-like header
- write:
  - width
  - height
  - mip count
  - linear size / top-level size
- set DDS pixel-format fields based on the internal format:
  - DXT1..DXTN mappings back to FourCCs
- set additional flags and mode bits from internal capability flags
- for each level:
  - account for payload size through:
    - `FUN_0068CC20(...)`
  - advance output cursor

### Best interpretation

The cleanest reading is:

- `FUN_006A0630(...)` is a DDS export/header-builder path for the internal image-format family

So the DDS seam is now effectively bi-directional in the notes:

- parse/import side
- build/export side

## `FUN_006A0280(...)`: six-bank style level walk

This helper is smaller, but it matters because it reuses the shared level-sizer.

### What it does

Its behavior is:

- resolve bytes-per-block and block dimensions through:
  - `FUN_0068AE30(...)`
  - `FUN_0068B010(...)`
- iterate six entries from an external bank/pointer table
- for each bank:
  - walk every active level
  - advance by:
    - `FUN_0068CC20(...)`

### Best interpretation

- `FUN_006A0280(...)` is a six-bank side helper that walks the same shared per-level storage layout used by the `ImgMem` family

That reinforces the earlier six-bank model instead of leaving it isolated to one constructor.

## `FUN_006A01D0(...)`: single-bank side level walk

This helper is the simpler counterpart.

### What it does

Its behavior is:

- require `param_7 == 0`
- resolve bytes-per-block and block dimensions
- iterate each active level
- advance by:
  - `FUN_0068CC20(...)`

### Best interpretation

- `FUN_006A01D0(...)` is the corresponding single-bank side walk over the same shared level-layout logic

So the palette-side cluster really is mirroring the single-bank vs multi-bank split we already saw on the `ImgMem` side.

## `FUN_0069E870(...)`: palette/conversion payload builder

This helper is the most complex in the pass, but the broad role is now pretty readable.

### What it does

Its behavior is:

- require formats with capability bit `1`
- reject overly large dimensions or zero level counts
- resolve bytes-per-block through:
  - `FUN_0068AE30(...)`
- compute total payload size using:
  - `FUN_0068CC20(...)`
  - but with a local block geometry forced to `4 x 4`
- write an output header-like structure including:
  - width
  - height
  - FourCC for DXT-family formats
- derive internal mode bits from format flags
- allocate temporary bitmask storage for block occupancy/selection
- then, per level:
  - walk source payload blocks
  - maintain selection bitmasks
  - route into specialized helpers depending on the format:
    - `FUN_0069B720(...)` for `0x10` / `0x11`
    - `FUN_0069CC40(...)` in one conditional path
    - `FUN_0069BCD0(...)` for `0x12` / `0x13` / `0x14` / `0x15`
    - `FUN_0069C3F0(...)` when additional mode bits are active
- compact the produced block data into the output stream

### Best interpretation

The strongest safe reading is:

- `FUN_0069E870(...)` is a palette/conversion-side payload builder for DXT-family and related compressed output formats

It is not just “use palette lookup somewhere.”
It is actively constructing compressed-format payload data across levels.

That makes the temporary `ImgPal` path much easier to place:

- it supports blockwise conversion/selection work used in these compressed-format builders

## Updated subsystem model

With this pass included, the current image side now looks like:

### Shared image-layout core

- internal format flags
- block dimensions
- bytes-per-block
- level count / depth
- per-level byte sizing

### `ImgMem` family

- single-bank layout
- six-bank layout
- validation
- live add-next-level path

### `ImgPal` / conversion family

- candidate-record arena
- squared-distance table
- nearest-match bucket structures
- compact lookup table
- compressed/palette conversion support

### DDS seam

- parse/import:
  - `FUN_006A0330(...)`
- build/export:
  - `FUN_006A0630(...)`

So the palette-side path is no longer just a possible side channel.
It is part of a real DDS/compressed-format conversion seam using the same shared image-format substrate.

## Best next step

The strongest next targets are now the specialized block-conversion helpers called from `FUN_0069E870(...)`:

- `FUN_0069B720`
- `FUN_0069CC40`
- `FUN_0069BCD0`
- `FUN_0069C3F0`

That should let us say:

- which compressed-format families each helper corresponds to
- what the palette/bitmask logic is doing block-by-block
- and whether the six-bank mode maps cleanly onto a specific DXT conversion family or only a broader multi-plane compressed path
