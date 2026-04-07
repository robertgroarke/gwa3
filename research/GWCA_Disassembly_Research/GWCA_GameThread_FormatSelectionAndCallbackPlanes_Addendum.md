## `Gw.exe` Frame Callback Format Selection And Callback Planes Addendum

This pass followed the next helpers around descriptor-table growth, format descriptors, callback-plane selection, and remainder-fill behavior:

- `FUN_0066B760`
- `FUN_00688930`
- `FUN_006887E0`
- `FUN_00689D40`
- `FUN_0068B010`
- `FUN_0068AE30`
- `FUN_00689E90`

The goal was to explain:

- how the `0x34` descriptor table grows
- how the `GrTex2d` upload path describes source/destination formats
- how the large blit worker chooses one callback plane or two
- and how remainder regions are filled

## Source artifacts

These results come from:

- [gw_decomp_format_selection_temp126.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_format_selection_temp126.log)

## High-level result

This pass makes the upload-selection layer much more concrete.

The best current decomposition is:

- `FUN_0066B760(...)`
  - descriptor-table grow/realloc helper for `0x34` records
- `FUN_00689E90(...)`
  - per-format capability-flag lookup
- `FUN_0068AE30(...)`
  - per-format bytes-per-block/element lookup
- `FUN_0068B010(...)`
  - per-format block-dimension lookup
- `FUN_006887E0(...)`
  - per-level/mip offset-table generator
- `FUN_00688930(...)`
  - primary/secondary callback-plane selector
- `FUN_00689D40(...)`
  - remainder-region filler for the tiled blit worker

So the `GrTex2d` upload path is now much clearer:

- describe format through static tables
- decide whether one callback plane is enough or two are needed
- precompute per-level offsets
- run the main tiled transfer
- and fill leftover regions with a dedicated helper

## `FUN_0066B760(...)`: descriptor-table grow helper

This helper closes the remaining descriptor-table ownership question.

### What it does

Its behavior is:

- take requested capacity `param_1`
- compare against current capacity at:
  - `this + 4`
- if more space is needed:
  - choose a new capacity through:
    - `FUN_00473540(param_1, current_capacity, this + 0x0C)`
- if the active record count clamp `param_2` is below current count:
  - lower current count
- if capacity actually changed:
  - realloc backing storage through:
    - `FUN_004735B0(old_ptr, new_capacity * 0x34)`
  - guard against overlap with the active record region
  - reset active span through:
    - `FUN_0046D790(active_count * 0x34)`
  - free old storage through:
    - `FUN_0047EF60()`
- install the new capacity and pointer
- if current count is below `param_2`:
  - raise it to `param_2`

### Best interpretation

The cleanest reading is:

- `FUN_0066B760(...)` is the dedicated grow/realloc policy for the hashed `0x34` descriptor-record table

That completes the descriptor-table story:

- `FUN_00654500(...)`
  - loads one range block
- `FUN_0066B620(...)`
  - inserts records
- `FUN_0066B760(...)`
  - grows and maintains the backing table

## `FUN_00689E90(...)`: format capability flags

This helper is tiny but important.

### What it does

It:

- validates a format id `<= 0x1A`
- returns one byte from:
  - `DAT_00A27340[format_id]`

### Best interpretation

The strongest reading is:

- `FUN_00689E90(...)` returns the capability/feature flags for one graphics format id

That fits its callers:

- `FUN_006903C0(...)`
  - checks bit `8`
- `FUN_00688A10(...)`
  - checks bits like `8`, `0x10`, and `0x20`

So this byte table is the central format-capability map for the upload engine.

## `FUN_0068AE30(...)`: bytes-per-block / element-size lookup

This helper is also simple:

- validate format id `<= 0x1A`
- return the first dword from:
  - `DAT_00A271F8 + format_id * 0x0C`

### Best interpretation

The cleanest reading is:

- `FUN_0068AE30(...)` returns the bytes-per-block or bytes-per-element value for one format id

That matches how callers use it:

- `FUN_006887E0(...)`
  - multiplies by block size and aligned dimensions
- `FUN_00688A10(...)`
  - converts tiled dimensions into byte offsets

## `FUN_0068B010(...)`: block-dimension lookup

This helper fills a two-dword output pair.

### What it does

It:

- validates format id `<= 0x1A`
- checks a flag bit from:
  - `DAT_00A27340[format_id]`
- if the bit is set:
  - write:
    - width block size = `4`
    - height block size = `4`
- otherwise:
  - write:
    - width block size = `1`
    - height block size = `1`

### Best interpretation

The strongest reading is:

- `FUN_0068B010(...)` returns the format’s block width and block height

So together:

- `FUN_00689E90(...)`
  - flags
- `FUN_0068AE30(...)`
  - bytes per block/element
- `FUN_0068B010(...)`
  - block dimensions

form the core static format-descriptor layer for the upload path.

## `FUN_006887E0(...)`: per-level offset-table generator

This helper precomputes offsets across repeated downscaled levels.

### What it does

Its behavior is:

- get bytes-per-block through:
  - `FUN_0068AE30(format)`
- get block dimensions through:
  - `FUN_0068B010(...)`
- iterate `param_3` levels
- for each level:
  - align the current width upward to the format block width
  - multiply by block width/height scaling and bytes-per-block
  - shift by `>> 3`
  - store the result into the output array
  - halve the width for the next level, with floor-at-one behavior

### Best interpretation

The cleanest reading is:

- `FUN_006887E0(...)` builds a per-level or per-mip row/plane offset table for a given format and starting width

That explains why `FUN_00688A10(...)` uses it when no explicit tables are supplied.

## `FUN_00688930(...)`: callback-plane selector

This helper is the missing high-level selector for the two callback slots inside the blit worker.

### What it does

Its behavior is:

- derive a local condition from:
  - source format
  - destination format
  - mode bits
  - a same-surface/same-state boolean
- in the simple case:
  - call:
    - `FUN_00688890(...)`
  - store its result into `*param_5`
  - set `*param_6 = 0`
- otherwise:
  - call `FUN_00688890(...)` twice:
    - once with source format disabled
    - once with destination format disabled
  - store the two returned callbacks in:
    - `*param_5`
    - `*param_6`
- require both callbacks to be nonzero in the dual-plane path

### Best interpretation

The strongest reading is:

- `FUN_00688930(...)` chooses whether the upload worker can run through a single callback plane or must split into two staged callback planes

That matches `FUN_00688A10(...)` exactly:

- when one callback plane exists:
  - invoke only `*local_e8`
- when a second plane exists:
  - stage through `*local_e8`
  - then feed that result through `*local_108`

So the dual-callback branch is real staged conversion, not just error handling.

## `FUN_00689D40(...)`: remainder-region filler

This helper is the cleanup path for regions the main tiled worker leaves behind.

### What it does

Its behavior is:

- require power-of-two source dimensions
- require a proper non-empty destination rectangle
- require a nonzero level/index parameter
- compute:
  - source/destination offsets
  - local width/height
  - scaled dimensions based on the current level
- then call:
  - `FUN_0069AB50(...)`
  - with a small local rect/size package

### Best interpretation

The cleanest reading is:

- `FUN_00689D40(...)` fills or processes a remainder rectangle for one level of the tiled transfer

That lines up with `FUN_00688A10(...)`, which calls it repeatedly for uncovered edges and tails after the main callback-driven work.

## Updated upload-selection model

With this pass included, the current upload path is:

1. static format descriptor layer
   - `FUN_00689E90(...)` = capability flags
   - `FUN_0068AE30(...)` = bytes per block/element
   - `FUN_0068B010(...)` = block dimensions
2. callback selection
   - `FUN_00688930(...)`
   - one callback plane when possible
   - two staged planes when needed
3. level/offset preparation
   - `FUN_006887E0(...)`
4. tiled transfer core
   - `FUN_00688A10(...)`
5. remainder filling
   - `FUN_00689D40(...)`

That makes the `GrTex2d` upload path much less opaque.
It is clearly a format-aware, callback-selected staged transfer system.

## Best next step

The strongest next targets are:

- `FUN_00688890`
- `FUN_0069AB50`
- `FUN_006a1610`
- `FUN_006a16e0`
- `FUN_006902C0`
- optionally the static tables around:
  - `DAT_00A271F8`
  - `DAT_00A27340`

That should let us:

- name the actual callback implementations selected for each format path
- describe what the remainder filler really does
- and identify the temporary handle/resource lifecycle that the two-plane worker uses
