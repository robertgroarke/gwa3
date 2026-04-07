# GWCA GameThread Converter Glue Layer Addendum

This pass continues from the converter/texture-object note by following the next glue helpers directly underneath the recovered descriptor-driven text-piece pipeline:

- `FUN_0066B810`
- `FUN_00654500`
- `FUN_0066BBA0`
- `FUN_0066BB10`
- `FUN_006486B0`
- `FUN_0064BA50`
- `FUN_006490F0`

The goal was to close the last obvious naming gap in this branch:

- how cached descriptors are resolved
- how font-range data is loaded on demand
- how the packed lane stream encodes its control values
- how atlas placement is actually chosen
- and how the resulting texture-piece wrapper is cleaned up

## Source Artifacts

These results come from:

- [gw_decomp_converter_glue_temp191.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_converter_glue_temp191.log)

## Main Result

This pass turns the converter/object path from:

- descriptor-driven
- packed-stream decoded
- texture-object backed

into something even more concrete.

The best current decomposition is:

- `FUN_0066B810(...)`
  - hashed glyph-descriptor record lookup by code unit
- `FUN_00654500(...)`
  - lazy font-range loader and descriptor-table population pass
- `FUN_0066BBA0(...)`
  - variable-length packed integer decoder over the nibble stream
- `FUN_0066BB10(...)`
  - packed lane-state refresh / refill helper
- `FUN_006486B0(...)`
  - rectangle-sheet splitter / placement finder
- `FUN_0064BA50(...)`
  - `GrTex2d` blit/upload processor over the chosen placement
- `FUN_006490F0(...)`
  - wrapper unlink / finalizer for the registered piece node

So the pipeline now reads like:

- resolve descriptor
- lazily load missing range
- decode packed lane metrics
- find or split atlas space
- upload/blit into `GrTex2d`
- register wrapper
- later unlink and release it cleanly

## `FUN_0066B810(...)`: hashed glyph-descriptor lookup

This helper is much cleaner than it first looked from its callers.

Its behavior is:

- require:
  - non-zero code unit
  - non-empty table count
- hash the code unit using:
  - `in_ECX[4] & code_unit`
- index a base table of `0x34`-byte records
- compare the stored code unit at:
  - record `+ 0x2C`
- if it does not match:
  - follow a chain index through:
    - record `+ 0x30`
- return the matching record pointer or `0`

So this is best described as:

- hashed glyph-descriptor record lookup by code unit

That sharpens the earlier `FUN_00653890(...)` note:

- `FUN_00653890(...)` chooses the active descriptor range and cache slot
- `FUN_0066B810(...)` resolves the concrete glyph-descriptor record inside that range

## `FUN_00654500(...)`: lazy font-range loader and table population

This helper is one of the most useful in the pass because it explains why the classifier cache can miss and later recover.

Its behavior is:

- if the range state is already:
  - `0`
  - `1`
  - or `3`
  - do not rebuild it
- otherwise:
  - request a range/resource through:
    - `FUN_00470AD0(param_1[1], 1, 0)`
- on failure:
  - log:
    - `Failed to load font range 0x%x - 0x%x`
  - mark state `1`
- on success:
  - fetch the loaded range payload through:
    - `FUN_004706E0(...)`
  - pass it into:
    - `FUN_006413B0(...)`
  - iterate code units from:
    - low bound at `param_1 + 4`
    - up to high bound at `param_1 + 6`
  - for each step:
    - advance by:
      - `FUN_0066B5B0(...)`
    - and populate local descriptor data through:
      - `FUN_0066B620(...)`
  - then release the source handles through:
    - `FUN_00470F50(...)`
    - `FUN_0046F500(...)`
  - mark the parent as loaded
  - set local range state to `0`
- finally stamp:
  - `param_1[5] = DAT_00BD89E4`

So the cleanest current reading is:

- `FUN_00654500(...)` lazily loads one font/code-unit range and populates the descriptor table for it

That matters because it means the text-piece system is not holding all descriptor data eagerly.
It resolves descriptor ranges on demand as code units are encountered.

## `FUN_0066BBA0(...)`: variable-length packed integer decoder

This helper turns out to be more expressive than a simple nibble reader.

Its behavior is:

- read one nibble from the current stream position
- if that nibble is `< 8`
  - return it directly
- otherwise:
  - keep low `3` bits
  - read a second nibble
  - if that nibble is `< 0xF`
    - return:
      - `low + second * 8`
  - otherwise:
    - read one more byte worth of nibble material
    - return:
      - `low + next * 8 + next_next * 0x80`

So this is best described as:

- variable-length packed integer decoder over the lane nibble stream

That is a strong upgrade over the earlier “packed nibble/lane header” wording, because now the scale is visible:

- tiny values inline in one nibble
- medium values in two nibbles
- larger values in an extended third stage

This is exactly the kind of compact integer coding you would expect for:

- run lengths
- span counts
- packed lane metrics
- or small coverage/dimension fields

## `FUN_0066BB10(...)`: lane-state refresh / refill helper

This helper now reads as the control partner to `FUN_0066BBA0(...)`.

Its behavior is:

- inspect the current lane token in:
  - `in_ECX[8]`
- if the token is:
  - `0`
  - or `0xF`
  - flip a local byte flag
  - refill through:
    - `FUN_0066BC70()`
  - store the new run/count in:
    - `in_ECX[6]`
- if the token is `1`
  - read the next nibble
  - map it through:
    - `DAT_00A26ACC`
  - store the mapped byte into the lane-state slot
  - if that mapped nibble is not `0` and not `0xF`
    - set run/count to `1`
  - otherwise refill through:
    - `FUN_0066BC70()`
- any other token hard-fails

So this helper is best described as:

- packed lane-state refresh / refill helper

That means the stream format is not just “a bunch of alpha bytes.”
It has:

- control tokens
- mapped lane-state bytes
- and refillable run/count state

That strengthens the earlier reading that the lane family is a small packed control language, not only raw coverage samples.

## `FUN_006486B0(...)`: rectangle-sheet splitter and placement finder

This helper is the main placement result in the pass.

Its structure is recursive and very telling.

It:

- walks child rectangle nodes through:
  - `node + 8`
  - `node + 0x0C`
- recursively searches for a child that can hold:
  - requested width
  - requested height
- rejects nodes that are already occupied
- accepts exact fits immediately
- otherwise allocates two new `CIGrRectangleSheet` child nodes
- splits the current rectangle either:
  - vertically first
  - or horizontally first
  - depending on which dimension matches and whether the current node is the preferred edge child
- then recurses into the first new child

So the cleanest reading is:

- `FUN_006486B0(...)` is a binary rectangle-sheet splitter and placement finder for atlas allocation

That is much stronger than the earlier “placement block” wording.
It is now clearly a real atlas-packing tree over rectangle-sheet nodes.

## `FUN_0064BA50(...)`: `GrTex2d` blit/upload processor

We only need the upper part of the decompile here to tighten the role.

Its visible behavior is:

- require a live graphics/context object at:
  - `this + 0x44`
- lazily allocate a temporary processing surface through:
  - `FUN_006903C0(...)`
  - when `this + 0x10 == 0`
- clamp the requested width against:
  - `this + 0x38`
- call the main processing worker:
  - `FUN_00688A10(...)`
  - with:
    - source data
    - destination geometry
    - working surface state
    - clipped destination bounds
- if more than one pass/result comes back:
  - log through:
    - `ProcessBlt()` in `GrTex2d.cpp`
- if a cached temporary surface already existed:
  - finalize the touched rectangle through:
    - `FUN_00648370(...)`
- otherwise finish through:
  - `FUN_0064C2C0()`

So the safest current reading is:

- `FUN_0064BA50(...)` is the `GrTex2d` blit/upload processor that renders or copies the prepared piece data into the chosen atlas rectangle

That matches the surrounding pipeline perfectly:

- `FUN_006486B0(...)` chooses rectangle-sheet placement
- `FUN_0064BA50(...)` performs the actual blit/upload into that placement

## `FUN_006490F0(...)`: registered wrapper unlink and destructor path

This helper closes the lifecycle side of the piece wrapper.

Its behavior is:

- unlink the node from its intrusive list by patching the previous/next links
- if a small mode/count field at `param_2[3]` is zero:
  - emit:
    - `FUN_005A6CFB(param_2, 0x10)`
  - and return
- otherwise:
  - normalize the self-links
  - release the owned piece/object through:
    - `FUN_00649630(param_2[2])`
  - then destroy the wrapper allocation through:
    - `FUN_004781D0(&param_2)`

So the cleanest reading is:

- `FUN_006490F0(...)` is the registered wrapper unlink / finalizer for one atlas-backed piece node

That is exactly what `FUN_006488A0(...)` needed after registering the callback/finalizer.

## What This Changes

Before this pass, the converter/object branch was already clear in broad strokes, but a few glue points were still fuzzy.

After this pass, the remaining glue is much more concrete:

- descriptor resolution is:
  - hashed by code unit
- missing descriptor ranges are:
  - lazily loaded and populated on demand
- lane headers are:
  - variable-length packed integers with token/refill state
- atlas placement is:
  - recursive rectangle-sheet splitting
- upload is:
  - a real `GrTex2d` blit/process pass
- wrapper teardown is:
  - explicit unlink plus owned-object release

So the recovered text-piece backend is now close to end-to-end.

## Updated Working Model

The cleanest current low-level stack is now:

1. `FUN_00653890(...)`
   - choose active descriptor range
2. `FUN_00654500(...)`
   - lazily load missing font/code-unit range
3. `FUN_0066B810(...)`
   - resolve concrete glyph-descriptor record
4. `FUN_0066A8C0(...)`
   - resolve converter by effect signature
5. `FUN_0066BBA0(...)` / `FUN_0066BB10(...)`
   - decode packed lane integers and refresh lane state
6. `FUN_0066B4D0(...)`
   - compose lane coverage into destination buffer
7. `FUN_006486B0(...)`
   - choose/split atlas rectangle
8. `FUN_0064BA50(...)`
   - blit/upload piece data into `GrTex2d`
9. `FUN_006488A0(...)`
   - register the atlas-backed piece wrapper
10. `FUN_006490F0(...)`
    - later unlink and release that wrapper

That is the strongest end-to-end model recovered so far for the emitted piece path under `FUN_00653A60(...)`.

## Best Next Step

The next best reverse step is no longer more random widening under this branch.

The strongest next move is either:

1. a compact appendix that summarizes the full emitted-segment pipeline from:
   - `FUN_00613DC0(...)`
   - through:
     - `FUN_00653A60(...)`
   - down to:
     - `FUN_006490F0(...)`
2. or a pivot to the still-open codec seam:
   - exact stage/helper correspondence under:
     - `FUN_0069E870(...)`
     - `FUN_0069E1C0(...)`
