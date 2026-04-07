## `Gw.exe` Frame Callback Range Unpack And Atlas Release Addendum

This pass followed the next helpers around range unpacking, packed-run expansion, cached blit processing, and atlas release:

- `FUN_0066B5B0`
- `FUN_0066B620`
- `FUN_006413B0`
- `FUN_0066BC70`
- `FUN_006903C0`
- `FUN_00688A10`
- `FUN_00649630`

The goal was to close the remaining low-level seams behind:

- lazy font-range materialization
- packed run decoding
- cached `GrTex2d` blit processing
- and rectangle-sheet release

## Source artifacts

These results come from:

- [gw_decomp_range_unpack_blt_release_temp125.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_range_unpack_blt_release_temp125.log)

## High-level result

This pass closes several important "glue" gaps.

The best current decomposition is:

- `FUN_0066B5B0(...)`
  - range-block probe over the packed source stream
- `FUN_0066B620(...)`
  - insert-or-chain one `0x34` glyph/effect descriptor record
- `FUN_006413B0(...)`
  - buffer ownership/reset helper for range materialization
- `FUN_0066BC70(...)`
  - compact run-length decoder for the packed lane stream
- `FUN_006903C0(...)`
  - cached graphics/blit processor constructor
- `FUN_00688A10(...)`
  - the large validated tiled blit/transfer worker
- `FUN_00649630(...)`
  - rectangle-sheet release and parent-collapse helper

So the backend is now substantially more explicit:

- load range block
- reset or resize decode buffer
- probe stream extents
- insert concrete descriptor records into the hashed table
- decode packed runs
- dispatch through a cached graphics processor
- and eventually release the occupied atlas rectangle back into the sheet

## `FUN_0066B5B0(...)`: packed range-block probe

This helper is small but very useful in context.

### What it does

Its behavior is:

- store a range/index parameter into local decode state
- seed the current stream pointer from:
  - `param_2`
- initialize packed decode through:
  - `FUN_0066BA80(param_2 + param_1)`
- reset scan state via:
  - `FUN_0066BDD0(0xFFFFFFFF)`
- ask the stream where it ended through:
  - `FUN_0066BCE0()`
- return:
  - `(end_ptr - start_ptr) - param_1`

### Best interpretation

The cleanest reading is:

- `FUN_0066B5B0(...)` probes one packed range block to determine how many source bytes/units it occupies

That fits `FUN_00654500(...)` exactly, because that loader needed a per-entry consumed-size value while walking a materialized range block.

## `FUN_0066B620(...)`: descriptor-table inserter

This helper is the concrete record writer behind lazy range materialization.

### What it does

Its behavior is:

- require a nonzero code unit
- lazily allocate/resize the backing descriptor table through:
  - `FUN_0066B760(...)`
- compute bucket index:
  - `mask & code_unit`
- if the bucket head is already occupied:
  - grow the table again
  - copy the existing `0x34`-byte record to the end
  - write the chain index into:
    - `record + 0x30`
- copy `0x2C` bytes from `param_2` into the chosen record
- write the code unit at:
  - `record + 0x2C`

### Best interpretation

The strongest reading is:

- `FUN_0066B620(...)` inserts one concrete `0x34` glyph/effect descriptor record into the hashed table that `FUN_0066B810(...)` later queries

That means the lazy range-loader path is now structurally closed:

- stream-walk packed range data
- fill temporary record fields
- insert final `0x34` records into the hashed descriptor table

## `FUN_006413B0(...)`: decode-buffer ownership/reset helper

This helper is not text-specific on its face, but in this path it controls the loaded range buffer.

### What it does

Its behavior is:

- clear current active-span state if needed
- if requested size differs from current capacity:
  - allocate/resize through:
    - `FUN_004735B0(...)`
  - reject overlapping copy scenarios
  - reset previous active subspan through:
    - `FUN_0046D790(...)`
  - free old backing storage through:
    - `FUN_0047EF60()`
- install new capacity and backing pointer
- validate no overlap with the source block
- mark the new active span via:
  - `FUN_0046D790(param_2)`

### Best interpretation

The cleanest reading is:

- `FUN_006413B0(...)` owns the backing decode buffer for the currently materialized range block

So `FUN_00654500(...)` does not decode in place out of the loader’s original resource.
It explicitly stages the loaded range into a managed decode buffer first.

## `FUN_0066BC70(...)`: compact run-length decoder

This helper closes the packed-run side nicely.

### What it does

Its behavior is:

- read one nibble from the current stream state
- initialize run length as:
  - `nibble + 1`
- while nibble is `0xF`:
  - read another nibble
  - accumulate that nibble into the run length
- return final run length

### Best interpretation

The strongest reading is:

- `FUN_0066BC70(...)` is the run-length expander used by the packed lane state machine

That fits:

- `FUN_0066BB10(...)`
  - decide which token/run form applies
- `FUN_0066BC70(...)`
  - expand the run length
- `FUN_0066BCF0(...)`
  - consume bytes inside that run

So the lane format is now very clearly:

- nibble-coded metadata
- plus compact extended run lengths

## `FUN_006903C0(...)`: cached blit/graphics processor constructor

This helper is the small front door that `FUN_0064BA50(...)` uses when it has no cached processor object yet.

### What it does

Its behavior is:

- validate processor type id:
  - must be `<= 0x1A`
- optionally query capability flags through:
  - `FUN_00689E90(...)`
- if the target does not support a feature bit:
  - null the optional out pointer
- build two local descriptor objects through:
  - `FUN_0068B010(...)`
  - `FUN_0068AE30(...)`
- finish construction through:
  - `FUN_006902C0(...)`

### Best interpretation

The cleanest reading is:

- `FUN_006903C0(...)` constructs or configures the cached graphics/blit processor object used by `FUN_0064BA50(...)`

So the upload path is not directly talking to the atlas object alone.
It goes through a reusable processor configured from source/destination format descriptors.

## `FUN_00688A10(...)`: validated tiled blit/transfer worker

This helper is very large, but the top-level structure is clear enough now.

### What it does

Its behavior is:

- validate source/destination format capabilities through:
  - `FUN_00689E90(...)`
- require power-of-two source/destination dimensions
- validate source/destination rectangles and bounds
- derive format geometry through:
  - `FUN_0068B010(...)`
  - `FUN_0068AE30(...)`
  - repeated `FUN_0046DCF0()` calls
- normalize widths/heights into shifted/tiled space
- choose worker callbacks through:
  - `FUN_00688930(...)`
- optionally allocate an intermediate staging buffer
- if no explicit swizzle tables are provided:
  - build them through:
    - `FUN_006887E0(...)`
- optionally acquire an extra graphics handle through:
  - `FUN_006A1610(...)`
- iterate over tile/mip-like work units
- invoke one or two callback planes:
  - primary callback at `*local_e8`
  - optional secondary callback at `*local_108`
- fill uncovered remainder regions through:
  - `FUN_00689D40(...)`
- release temporary graphics handle through:
  - `FUN_006A16E0(...)`
- optionally return a small status/count in `param_15`

### Best interpretation

The strongest safe reading is:

- `FUN_00688A10(...)` is the validated tiled blit/transfer engine used by `GrTex2d` uploads

It looks broader than a trivial memcpy or single-surface blit.
It is doing:

- format validation
- tile/level geometry normalization
- callback-selected transfer execution
- and remainder handling

That matches `FUN_0064BA50(...)` very well as the real upload core.

## `FUN_00649630(...)`: atlas rectangle release and parent collapse

This helper closes the rectangle-sheet lifecycle.

### What it does

Its behavior is:

- require a valid `CIGrRectangleSheet` node signature:
  - `0xC4EE1C4E`
- require no live child links in the node itself
- if the node is currently marked occupied:
  - add its area back into a global/owner free-area counter
- clear its occupied flag
- walk upward to the parent
- if both sibling children are now empty leaves:
  - free both child nodes through:
    - `FUN_0046CAF0(..., 0x24)`
  - clear parent child pointers
  - continue collapsing upward
- otherwise stop

### Best interpretation

The cleanest reading is:

- `FUN_00649630(...)` releases an occupied atlas rectangle back into the rectangle sheet and collapses redundant subdivision nodes upward

That is exactly the missing release-side counterpart to `FUN_006486B0(...)`.

## Updated lifecycle

With this pass included, the current low-level lifecycle is:

1. `FUN_00654500(...)`
   - load and stage one font/glyph range block
2. `FUN_006413B0(...)`
   - prepare the decode buffer for that block
3. `FUN_0066B5B0(...)`
   - probe each packed entry’s consumed size
4. `FUN_0066B620(...)`
   - insert concrete `0x34` records into the hashed descriptor table
5. `FUN_0066BBA0(...)` / `FUN_0066BC70(...)` / `FUN_0066BB10(...)`
   - decode compact lane metadata and run lengths
6. `FUN_006486B0(...)`
   - allocate a rectangle from the atlas sheet
7. `FUN_006903C0(...)`
   - construct/configure the cached blit processor
8. `FUN_00688A10(...)`
   - execute the validated tiled upload/blit
9. `FUN_006488A0(...)`
   - wrap the placed/uploaded piece
10. `FUN_006490F0(...)`
    - unlink wrapper and dispatch release
11. `FUN_00649630(...)`
    - return the rectangle to the sheet and collapse empty branches

This is now a nearly complete descriptor-to-atlas-to-release loop.

## Best next step

The strongest next targets are the remaining descriptor-construction and upload-selection helpers:

- `FUN_0066B760`
- `FUN_00688930`
- `FUN_006887E0`
- `FUN_00689D40`
- `FUN_0068B010`
- `FUN_0068AE30`
- `FUN_00689E90`

That should let us:

- name the exact descriptor-table allocator/grow policy
- identify how the blit worker selects its callback planes
- and describe the source/destination format descriptors instead of only their wrapper roles
