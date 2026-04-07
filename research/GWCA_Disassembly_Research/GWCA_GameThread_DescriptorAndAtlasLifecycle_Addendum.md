## `Gw.exe` Frame Callback Descriptor And Atlas Lifecycle Addendum

This pass followed the next glue helpers around the descriptor, packed-stream, and atlas-backed piece path:

- `FUN_0066B810`
- `FUN_00654500`
- `FUN_0066BBA0`
- `FUN_0066BB10`
- `FUN_006486B0`
- `FUN_0064BA50`
- `FUN_006490F0`

The goal was to close the remaining gap between:

- code-unit classification
- packed lane decoding
- atlas placement
- upload
- and wrapper cleanup

## Source artifacts

These results come from:

- [gw_decomp_descriptor_atlas_temp124.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_descriptor_atlas_temp124.log)

## High-level result

This pass closes the low-level lifecycle pretty cleanly.

The best current decomposition is:

- `FUN_0066B810(...)`
  - descriptor-record resolver inside a hashed/range-backed glyph table
- `FUN_00654500(...)`
  - lazy range-loader/materializer for one descriptor block
- `FUN_0066BBA0(...)`
  - variable-length nibble decoder for packed lane metadata
- `FUN_0066BB10(...)`
  - packed lane run/header refill helper
- `FUN_006486B0(...)`
  - rectangle-sheet allocator / subdivision routine
- `FUN_0064BA50(...)`
  - atlas upload / blit processing path
- `FUN_006490F0(...)`
  - wrapper unlink-and-release finalizer

So the backend is now much more explicit:

- classify code unit
- materialize descriptor block on demand
- decode packed lane metadata
- allocate atlas rectangle
- upload/blit into the texture sheet
- wrap the placement in a tracked object
- unlink and release it later through a finalizer

## `FUN_0066B810(...)`: descriptor-record resolver

This helper is the direct record resolver that `FUN_00653890(...)` calls after it picks a matching range block.

### What it does

Its behavior is:

- require:
  - nonzero code unit
  - nonzero record count/state at `this + 8`
- compute a hash/bucket index from:
  - `this + 0x10`
  - `this + 0x20`
- validate the masked index against:
  - `this + 8`
- resolve an initial record pointer as:
  - `base + index * 0x34`
- compare the record's code unit at:
  - `record + 0x2C`
- if it does not match:
  - follow a chain through:
    - `record + 0x30`
  - each chained record is again:
    - `base + next_index * 0x34`
- return the matching record or `0`

### Best interpretation

The strongest reading is:

- `FUN_0066B810(...)` resolves one concrete glyph/effect descriptor record from a hashed record table whose entries are `0x34` bytes each

That is the first clean evidence that the text backend is working with real per-code-unit records rather than only high-level range buckets.

## `FUN_00654500(...)`: lazy range loader / materializer

This helper is the biggest structural upgrade in the pass.

### What it does

Its behavior is:

- inspect a state field at `*param_1`
- skip work when the range is already in state `0`, `1`, or `3`
- otherwise load backing data through:
  - `FUN_00470AD0(param_1[1], 1, 0)`
- if loading fails:
  - log:
    - `"Failed to load font range 0x%x - 0x%x"`
  - set the state to `1`
- if loading succeeds:
  - call:
    - `FUN_004706E0(...)`
    - `FUN_006413B0(...)`
- then iterate through the loaded record count
- for each entry:
  - use:
    - `FUN_0066B5B0(...)`
    - `FUN_0066B620(...)`
  - advance through the block
  - validate code-unit bounds between:
    - `param_1[4]`
    - `param_1[6]`
- then release backing data through:
  - `FUN_00470F50(...)`
  - `FUN_0046F500(...)`
- mark loader state in the owning object
- set `*param_1 = 0`
- refresh a timestamp or generation counter from:
  - `DAT_00BD89E4`

### Best interpretation

The cleanest reading is:

- `FUN_00654500(...)` is the lazy materializer for one font/glyph range block

So `FUN_00653890(...)` does not just identify a range.
It ensures the selected range block is physically loaded and decoded before `FUN_0066B810(...)` resolves the exact record.

That makes the classifier path fully coherent:

- range pick
- lazy load if needed
- exact record lookup

## `FUN_0066BBA0(...)`: variable-length nibble decoder

This helper explains the packed metadata format much better.

### What it does

It reads nibbles from the stream pointed to by `*this` with a nibble-phase flag at `this + 4`-style state.

Its decoding behavior is:

- read one nibble
- if nibble `< 8`
  - return it directly
- otherwise:
  - keep low `3` bits
  - read a second nibble
  - if second nibble `< 0xF`
    - return:
      - `low3 + second * 8`
  - otherwise read enough additional nibble/byte state to return:
      - `low3 + next * 8 + next * 0x80`

### Best interpretation

The strongest reading is:

- `FUN_0066BBA0(...)` is a compact variable-length integer decoder over the nibble stream

That fits `FUN_0066BA80(...)` perfectly, because that helper was using it to decode:

- one scalar
- two dimensions
- and a derived count/product

So the lane header format is not fixed-width.
It is a compact nibble-coded integer format.

## `FUN_0066BB10(...)`: packed run/header refill

This helper is the state machine companion to `FUN_0066BBA0(...)`.

### What it does

It branches on the current lane token at:
  - `this + 0x20` equivalent state (`in_ECX[8]` in the decompile)

Cases:

- if token is `0` or `0xF`
  - flip a cached flag byte
  - refresh run state through:
    - `FUN_0066BC70()`
- if token is `1`
  - read another nibble directly
  - map it through:
    - `DAT_00A26ACC`
  - if the mapped token is not `0` or `0xF`
    - set a one-unit run length/state
  - otherwise fall back to:
    - `FUN_0066BC70()`
- any other token:
  - assert/fail

### Best interpretation

The cleanest reading is:

- `FUN_0066BB10(...)` refills the current packed lane run/header state after a token has been consumed

Together with `FUN_0066BCF0(...)`, that gives us a coherent packed-stream model:

- `FUN_0066BA80(...)`
  - initialize stream and decode header fields
- `FUN_0066BB10(...)`
  - decode the next run/header token
- `FUN_0066BCF0(...)`
  - consume bytes from that run

## `FUN_006486B0(...)`: rectangle-sheet allocator

This helper is the real atlas placement routine.

### What it does

It works recursively over a tree of `CIGrRectangleSheet`-style nodes.

The behavior is:

- require a non-null sheet/root node
- if child nodes exist at:
  - `node + 8`
  - `node + 0xC`
  - recurse into them first
- skip used nodes marked at:
  - `node + 0x10`
- compute available width/height from:
  - `right - left`
  - `bottom - top`
- reject the node if the requested width/height will not fit
- if the requested size matches exactly:
  - optionally mark the node occupied
  - return it
- otherwise split the node by allocating two child nodes through:
  - `FUN_0046CA10(0x24, s___AUNode_CIGrRectangleSheet___00bc1a30)`
- choose horizontal or vertical split based on:
  - exact height match
  - parent/child relationship hints
  - and remaining width/height
- assign child rectangles
- recurse into the first child

### Best interpretation

The strongest reading is:

- `FUN_006486B0(...)` is a binary rectangle-sheet allocator for atlas placement

That means the fallback piece path is backed by a real texture atlas/subdivision tree rather than a flat free-list of pages.

## `FUN_0064BA50(...)`: atlas upload / blit processor

This helper is the upload path that fills the allocated atlas rectangle.

### What it does

Its behavior is:

- require a valid graphics context at:
  - `this + 0x44`
- lazily create or cache a processor object at:
  - `this + 0x10`
  - via:
    - `FUN_006903C0(...)`
- clamp the work amount against:
  - `this + 0x38`
- call the core blit/upload worker:
  - `FUN_00688A10(...)`
- if the worker reports more than one issue/state:
  - emit a `GrTex2d.cpp` diagnostic through:
    - `FUN_006359B0(..., L"ProcessBlt()", ...)`
- compute destination rectangle endpoints from:
  - requested source/destination offsets
- if a cached processor already existed:
  - finish through:
    - `FUN_00648370(...)`
- otherwise finalize through:
  - `FUN_0064C2C0()`

### Best interpretation

The cleanest reading is:

- `FUN_0064BA50(...)` is the atlas upload/blit processor for one placed piece rectangle

So `FUN_006488A0(...)` is not just allocating atlas space.
It really is:

- place rectangle
- upload/blit piece data into that rectangle
- return a wrapped graphics object

## `FUN_006490F0(...)`: wrapper finalizer

This helper closes the lifecycle on the atlas piece wrapper.

### What it does

It:

- unlinks the wrapper from its intrusive list
- branches on `param_2[3]`

If `param_2[3] == 0`:

- perform a specialized release via:
  - `FUN_005A6CFB(param_2, 0x10)`

Otherwise:

- reset the wrapper into a self-linked state
- release the associated placement/object at:
  - `param_2[2]`
  - through:
    - `FUN_00649630(...)`
- free the wrapper itself through:
  - `FUN_004781D0(&param_2)`

### Best interpretation

The strongest reading is:

- `FUN_006490F0(...)` is the wrapper unlink-and-release finalizer for atlas-backed piece objects

That closes the loop with `FUN_006488A0(...)`, which was scheduling this callback through:

- `FUN_0065A3A0(handle, FUN_006490F0, wrapper)`

So the wrapper is definitely a real owned lifecycle object, not just a raw placement token.

## Updated low-level lifecycle

With this pass included, the current low-level text-piece lifecycle is:

1. `FUN_00653890(...)`
   - choose the matching range block for a UTF-16 code unit
2. `FUN_00654500(...)`
   - lazily load/materialize that block if needed
3. `FUN_0066B810(...)`
   - resolve the exact `0x34`-byte descriptor record
4. `FUN_0066BBA0(...)` / `FUN_0066BB10(...)`
   - decode compact packed lane metadata and runs
5. `FUN_0066BCF0(...)`
   - read packed alpha/coverage bytes
6. `FUN_0066B4D0(...)`
   - composite them into a destination mask/texture buffer
7. `FUN_006486B0(...)`
   - allocate a rectangle from the texture sheet
8. `FUN_0064BA50(...)`
   - upload/blit the piece into that rectangle
9. `FUN_006488A0(...)`
   - wrap the placed/uploaded piece in a tracked object
10. `FUN_006490F0(...)`
   - unlink and release the wrapper later

This is now a full descriptor-to-atlas lifecycle, not just a partial rendering sketch.

## Best next step

The strongest next targets are the remaining helpers immediately around the resolved seams:

- `FUN_0066B5B0`
- `FUN_0066B620`
- `FUN_006413B0`
- `FUN_0066BC70`
- `FUN_006903C0`
- `FUN_00688A10`
- `FUN_00649630`

That should answer:

- how a loaded font range is unpacked into the `0x34` descriptor records
- how packed runs are expanded
- what graphics processor object `FUN_0064BA50(...)` is caching
- and how atlas placements are released back into the rectangle sheet
