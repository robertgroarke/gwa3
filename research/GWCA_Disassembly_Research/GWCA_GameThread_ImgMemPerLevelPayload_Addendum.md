## `Gw.exe` Frame Callback ImgMem Per-Level Payload Addendum

This pass followed the remaining high-value `ImgMem` sizing helper:

- `FUN_0068CC20`

and checked its caller surface to avoid overfitting it to one constructor path.

## Source artifacts

These results come from:

- [gw_decomp_imgmem_level_temp129.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgmem_level_temp129.log)
- [gw_findcallers_0068cc20_temp129.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0068cc20_temp129.log)

## High-level result

This helper is the missing inner payload sizer for the `ImgMem` object.

The cleanest current reading is:

- `FUN_0068CC20(...)`
  - computes the byte size of one per-level payload block from:
    - bytes-per-block
    - block width/height
    - downscaled level dimensions
  - with explicit power-of-two alignment requirements on the block geometry

So the `ImgMem` object we mapped last pass now looks like:

- pointer table
- optional `0x400` aux block
- then one aligned byte span per active level

where each level span is sized by `FUN_0068CC20(...)`.

## `FUN_0068CC20(...)`: per-level byte-span calculator

### What it does

Its behavior is:

- downscale source width and height by:
  - `param_4`
  - with floor-at-one behavior
- require power-of-two block dimensions from `param_2[0..1]`
- align the downscaled width upward to the block width
- align the downscaled height upward to the block height
- multiply:
  - aligned width
  - aligned height
  - bytes-per-block from `param_1`
- convert the product with a final:
  - `>> 3`

### Best interpretation

The strongest reading is:

- `FUN_0068CC20(...)` returns the number of bytes required to store one level of image data for the given format/block geometry

That means the per-level `ImgMem` payload is best thought of as:

- a level-sized storage slab
- whose exact byte count depends on the format's block geometry and bytes-per-block

not an arbitrary per-level metadata record.

## Relationship to the existing `ImgMem` constructor chain

This helper now closes the remaining sizing story:

- `FUN_0068CBB0(...)`
  - decides how many levels are active
- `FUN_006901E0(...)`
  - totals the memory layout
- `FUN_00690260(...)`
  - lays out pointers to each level block
- `FUN_0068CC20(...)`
  - supplies the exact byte count for each level block

So the cached `ImgMem` processor has a pretty crisp structure now:

1. header / object body
2. level-pointer table
3. optional aux/scratch block
4. aligned per-level image-storage blocks

## Caller-surface result

The caller map is broader than the constructor path alone.

Recovered refs include:

- `FUN_006901E0`
- `FUN_00690260`
- `FUN_0064B2F0`
- `FUN_00690430`
- `FUN_00690620`
- `FUN_00690910`
- `FUN_00690AE0`
- several `006A0*` and `0069E8*` helpers

### Best interpretation

The safest takeaway is:

- `FUN_0068CC20(...)` is a general per-level storage-size helper for this image-processing cluster
- not a one-off helper unique to the single `FUN_006902C0(...)` constructor

That matters because it makes the last few notes more robust:

- we are looking at a reusable image-format/storage subsystem
- not just ad hoc text-bot rendering glue

## Updated object model

With this pass included, the current object picture is:

### `ImgMem`

- format tables:
  - capability flags
  - block dimensions
  - bytes-per-block
- level count:
  - `FUN_0068CBB0(...)`
- level byte spans:
  - `FUN_0068CC20(...)`
- layout planner:
  - `FUN_006901E0(...)`
- pointer-table initializer:
  - `FUN_00690260(...)`
- final object builder:
  - `FUN_006902C0(...)`

### `ImgPal`

- candidate arena
- squared-difference table
- bucketed candidate lists
- fallback nearest-match filling
- compact lookup-table generation

So the two-plane path is now grounded in two fairly real subsystems:

- image storage/transfer layout
- palette conversion acceleration

## Best next step

The strongest next reverse targets are now less about “what is this object?” and more about integration:

- adjacent callers like:
  - `FUN_0064B2F0`
  - `FUN_00690430`
  - `FUN_00690620`
  - `FUN_00690910`
  - `FUN_00690AE0`
- and, on the palette side, the static tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`

That should let us answer:

- which concrete image-transfer modes use the shared `ImgMem` level-layout logic
- and whether the palette-acceleration path is specifically for indexed/palettized formats or a broader color-quantization side channel
