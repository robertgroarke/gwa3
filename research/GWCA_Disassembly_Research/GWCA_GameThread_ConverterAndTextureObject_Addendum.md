## `Gw.exe` Frame Callback Converter And Texture Object Addendum

This pass followed the next inner helpers below the effect-lane family:

- `FUN_00653890`
- `FUN_0066A8C0`
- `FUN_0066BA80`
- `FUN_0066BCF0`
- `FUN_006488A0`
- `FUN_00647C30`

The goal was to identify:

- how the engine classifies a code unit into a glyph/effect descriptor
- how the converter object is looked up
- how the lane/composition helpers read their packed source data
- and what concrete object type backs the fallback piece path

## Source artifacts

These results come from:

- [gw_decomp_converter_object_temp123.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_converter_object_temp123.log)

## High-level result

This pass closes a useful set of naming gaps.

The best current decomposition is:

- `FUN_00653890(...)`
  - code-unit to glyph/effect descriptor classifier with a small hot-cache layer
- `FUN_0066A8C0(...)`
  - hashed converter lookup keyed by a two-dword effect signature
- `FUN_0066BA80(...)`
  - packed nibble-stream/lane header reader
- `FUN_0066BCF0(...)`
  - byte reader over that packed lane stream
- `FUN_006488A0(...)`
  - atlas/texture-backed piece allocator and registration path
- `FUN_00647C30(...)`
  - concrete `GrTex2d` object constructor

So the text-piece pipeline is now no longer stopping at "effect-aware fragments."
It now reaches:

- glyph/effect classification
- converter-table lookup
- packed lane-data decoding
- and final texture-backed piece-object construction

## `FUN_00653890(...)`: code-unit classifier with cache promotion

This helper is the concrete classifier used by the span and subpiece iterators.

### What it does

Its flow is:

- ignore the plain space code unit `0x20`
- test a primary cached descriptor at:
  - `this + 0x10`
- if the code unit lies in that descriptor's range:
  - call:
    - `FUN_00654500(...)`
  - then resolve the result through:
    - `FUN_0066B810(...)`
- otherwise test a secondary cached descriptor at:
  - `this + 0x14`
- if that one matches:
  - call:
    - `FUN_00654500(...)`
  - swap the primary and secondary cache entries
  - then resolve through:
    - `FUN_0066B810(...)`
- otherwise scan a descriptor table rooted at:
  - `this + 0x38`
  - count-like value at `this + 0x40`
- if a table entry range matches:
  - move old primary to secondary
  - promote the found entry to primary
  - refresh through `FUN_00654500(...)`
  - resolve through `FUN_0066B810(...)`
- otherwise return `0`

### Best interpretation

The strongest reading is:

- `FUN_00653890(...)` maps a UTF-16 code unit to a glyph/effect descriptor using:
  - a two-entry hot cache
  - plus a slower range-table scan

That means the fitted text pipeline is not doing random ad hoc glyph probing.
It is walking a real cached descriptor family.

## `FUN_0066A8C0(...)`: hashed converter lookup

This helper is the missing center of the converter path that `FUN_0066A950(...)` depends on.

### What it does

Its behavior is:

- take a two-dword key from `param_1`
- hash it as:
  - `param_1[1] << 2 ^ param_1[0]`
- mask the hash with:
  - `*(this + 0x1C)`
- index a bucket table at:
  - `*(this + 0x10)`
- validate that the bucket index is within:
  - `*(this + 0x18)`
- walk a linked chain from the bucket node
- compare candidate records on:
  - stored hash
  - first dword
  - second dword
- return the matching record pointer or `0`

### Best interpretation

The cleanest reading is:

- `FUN_0066A8C0(...)` is a hashed converter/object lookup over a two-dword effect signature

That fits the caller exactly:

- `FUN_0066A950(...)` tries converter lookup
- if lookup fails it logs the missing effects mask and retries with a simpler signature

So this really does look like a proper effect-converter registry rather than one hardcoded branch table.

## `FUN_0066BA80(...)`: packed nibble/lane header reader

This helper is one of the best clues about the internal lane format.

### What it does

It:

- seeds a local stream pointer from `param_1`
- resets a nibble toggle flag
- reads three values through:
  - `FUN_0066BBA0()`
- stores:
  - one raw metric-like scalar
  - two dimensions offset by `+1`
  - a derived product
- then reads a nibble from the current byte:
  - low nibble on one phase
  - high nibble on the next phase
- stores that nibble at:
  - `this + 0x20` equivalent local slot (`in_ECX[8]` in the decompile)
- for nibble values `0` and `0xF`:
  - flips a byte using:
    - `~(&DAT_00A26ACC)[uVar5]`
- finishes with:
  - `FUN_0066BB10()`

### Best interpretation

The strongest reading is:

- `FUN_0066BA80(...)` initializes a packed lane/piece stream reader with:
  - dimensions
  - total cell count
  - and a nibble-coded lane/control token

That fits perfectly with the later composition helpers:

- `FUN_0066B4A0(...)`
  - asks it for cheap bounds
- `FUN_0066B4D0(...)`
  - asks it for per-pixel alpha bytes

So the effect-lane family is backed by a real packed stream format, not loose arrays of independent values.

## `FUN_0066BCF0(...)`: byte reader over the packed stream

This helper is short but decisive.

### What it does

It:

- returns `0` immediately if there are no remaining rows/blocks
- requires a positive remaining byte count
- writes one current byte to `*param_1`
- decrements:
  - remaining byte count
  - remaining row/block count
- if the current byte run is exhausted but more rows remain:
  - refill through:
    - `FUN_0066BB10()`
- returns `1` on success

### Best interpretation

The cleanest reading is:

- `FUN_0066BCF0(...)` is the sequential byte reader that `FUN_0066B4D0(...)` uses to pull alpha/coverage values from the packed lane stream

Together with `FUN_0066BA80(...)`, it closes the loop on how the compositor gets its source bytes.

## `FUN_006488A0(...)`: atlas-backed piece allocator and registration path

This helper is much richer than the earlier thin retain wrapper suggested.

### What it does

Its behavior is:

- validate requested dimensions against destination bounds
- compute area:
  - `width * height`
- require enough remaining atlas/storage capacity from:
  - `this + 0x20`
- allocate a placement block through:
  - `FUN_006486B0(width, height, *(this + 0x24), 0)`
- subtract consumed area from remaining capacity
- copy returned placement rect info into `param_6[0..3]`
- call:
  - `FUN_00648D40()`
- validate mode flags at:
  - `this + 0x14`
- enter graphics context through:
  - `FUN_00637DE0(*(this + 0x44))`
- copy or upload the requested data through:
  - `FUN_0064BA50(...)`
- obtain or reuse a small wrapper node:
  - either fresh under `GrTex2d.cpp`
  - or from a free-list stack at `this + 0x34 / +0x3C`
- attach:
  - owner/context pointer
  - placement block pointer
- link the wrapper into an intrusive list
- schedule a callback/finalizer through:
  - `FUN_0065A3A0(..., FUN_006490F0, wrapper)`
- return a graphics/handle object produced via:
  - `FUN_0065A290(...)`

### Best interpretation

The strongest reading is:

- `FUN_006488A0(...)` is not just a “lookup”
- it is the real atlas-backed texture-piece allocation and registration path

That sharpens the earlier fallback model:

- `FUN_0064AF80(...)` is the wrapper that requests one of these atlas-backed piece objects and retains it when successful

So the fallback path is actually a normal graphics-object construction path, not a weak emergency branch.

## `FUN_00647C30(...)`: concrete `GrTex2d` object constructor

This helper finally makes the object type explicit.

### What it does

Its construction pattern is:

- install vtable:
  - `PTR_FUN_00A24288`
- initialize intrusive/self-linked fields
- store incoming mode/config values:
  - `param_2`
  - `param_3`
- call:
  - `FUN_0046C9F0()`
- compute pixel capacity from:
  - `width * height`
- allocate a node/object through:
  - `FUN_0046CA10(0x24, s___AUNode_CIGrRectangleSheet___00bc1a30)`
- seed that node with:
  - width
  - height
  - zeroed links/fields
- initialize additional object fields:
  - remaining capacity
  - default flags/state
  - free-list counters

### Best interpretation

The cleanest reading is:

- `FUN_00647C30(...)` is the concrete `GrTex2d`-backed piece/atlas object constructor

So the earlier source anchor was not just a debug breadcrumb.
It really is the concrete graphics-object class for this fallback/piece path.

## Updated inner backend model

With this pass included, the current low-level pipeline is:

1. `FUN_00653890(...)`
   - classify UTF-16 code unit into a cached glyph/effect descriptor
2. `FUN_0066A8C0(...)`
   - find the converter/object implementation for a two-dword effect signature
3. `FUN_0066BA80(...)`
   - initialize packed lane-stream decode state
4. `FUN_0066BCF0(...)`
   - read alpha/coverage bytes from that stream
5. `FUN_0066B4D0(...)`
   - composite those bytes into a destination mask/texture buffer
6. `FUN_006488A0(...)`
   - allocate/register an atlas-backed texture piece
7. `FUN_00647C30(...)`
   - construct the underlying `GrTex2d` object when needed

So the text backend under GWCA’s hooked frame/update path is now clearly:

- descriptor-driven
- converter-driven
- packed-stream decoded
- and texture-object backed

## Best next step

The strongest next targets are the remaining glue helpers around these resolved seams:

- `FUN_0066B810`
- `FUN_00654500`
- `FUN_0066BBA0`
- `FUN_0066BB10`
- `FUN_006486B0`
- `FUN_0064BA50`
- `FUN_006490F0`

That should answer:

- what exact descriptor object `FUN_00653890(...)` is materializing
- what the packed stream header fields really mean
- how atlas placement is chosen
- and what lifecycle/finalizer semantics the texture-piece wrapper uses
