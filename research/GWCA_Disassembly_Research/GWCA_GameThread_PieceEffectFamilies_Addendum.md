## `Gw.exe` Frame Callback Piece Effect Families Addendum

This pass followed the next inner helpers under the composed text-segment builder:

- `FUN_00669CB0`
- `FUN_00669FA0`
- `FUN_0066B130`
- `FUN_0066B200`
- `FUN_0066B4D0`
- `FUN_0066B4A0`
- `FUN_0064AF80`
- `FUN_0064AFB0`

The goal was to identify what kind of concrete piece/effect families sit underneath the fitted text-span iterator and how those pieces reach texture-backed output.

## Source artifacts

These results come from:

- [gw_decomp_piece_effects_temp121.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_piece_effects_temp121.log)

## High-level result

This pass makes the subpiece layer much less abstract.

The best current decomposition is:

- `FUN_00669CB0(...)`
  - range/window planner for one generated piece span
- `FUN_00669FA0(...)`
  - tiny metric seed fetch from a nested piece object
- `FUN_0066B130(...)`
  - left/base offset planner across up to four effect lanes
- `FUN_0066B200(...)`
  - right-edge/final width planner across the same effect lanes
- `FUN_0066B4D0(...)`
  - alpha-mask compositor / blitter for one lane or piece fragment
- `FUN_0066B4A0(...)`
  - lightweight bounds query companion for the same lane system
- `FUN_0064AF80(...)`
  - fallback piece lookup/retain helper
- `FUN_0064AFB0(...)`
  - texture-backed fallback piece creator

So the emitted subpieces are no longer best described as generic "render fragments."
They now look like effect-aware textured glyph pieces with:

- a planned visible range
- multi-lane extent tracking
- alpha-mask composition
- and a texture-backed fallback path

## `FUN_00669CB0(...)`: range/window planner

This helper computes the usable span window for the current piece state.

### What it does

Its behavior is:

- optionally writes a base metric pair from:
  - `*in_ECX`
  - `in_ECX[1]`
- computes a start position from:
  - `in_ECX[5] + (*in_ECX - 1) * param_1`
- derives an end position from:
  - `start + (*in_ECX - 1)`
- clamps against:
  - `in_ECX[7]`
- if the visible width changes:
  - rounds it up to a power of two through:
    - `FUN_0046DCF0()`
  - enforces a minimum of `4`
- optionally writes a four-int span:
  - start X
  - start Y
  - end X
  - end Y
- optionally writes the inclusive end index

### Best interpretation

The cleanest reading is:

- `FUN_00669CB0(...)` prepares the visible/usable subrange for the current piece iterator state
- and also recommends a power-of-two sized allocation width when the caller asks for one

That makes it a good fit for:

- per-piece texture window planning
- or clipped glyph/effect lane extraction

## `FUN_00669FA0(...)`: tiny nested metric fetch

This helper is very small:

- calls:
  - `FUN_0046D7D0()`
- returns:
  - `**(undefined4 **)(this + 0x0C)`

The best current interpretation is:

- it fetches a current seed metric or width-like scalar from a nested piece object at `this + 0x0C`

It matters mainly because `FUN_006541A0(...)` uses it right before it begins per-piece iteration.

## `FUN_0066B130(...)`: left/base offset planner across effect lanes

This helper is one of the most informative in the pass.

### Fast path

If:

- `*(param_1 + 0x20) == 0`
- and `param_3 == 0`

then it behaves like a simple baseline advance:

- compare `*in_ECX` against `in_ECX[1]`
- take the larger
- add `param_2`
- store and return the result

### Multi-lane path

Otherwise it treats `param_1` as a lane descriptor family:

- loops over four lanes
- each lane has:
  - an offset at `param_1 + lane*4`
  - a span/size at `param_1 + 0x10 + lane*4`
- updates a shared base position in `*in_ECX`
- compares against lane-specific minimum constraints in:
  - `in_ECX[1]`
  - `in_ECX[2]`
  - `in_ECX[3]`
  - `in_ECX[4]`
- tracks the largest `offset + span`
- divides that aggregate by `3`
- uses the result to force the base position forward if any lane constraint requires it

### Best interpretation

The strongest reading is:

- `FUN_0066B130(...)` computes the left/base advance for a glyph piece while accounting for up to four effect lanes

That lane structure is a strong hint of outline/shadow/glow-like expansion families rather than one plain glyph box.

## `FUN_0066B200(...)`: right-edge / final width planner

This helper is the natural companion to `FUN_0066B130(...)`.

### What it does

It:

- seeds a running width from:
  - `*in_ECX + param_2`
- if multi-lane mode is active:
  - iterates four lanes
  - computes each lane’s final extent from:
    - lane base offset
    - lane span
  - updates per-lane maxima in:
    - `in_ECX[1]`
    - `in_ECX[2]`
    - `in_ECX[3]`
    - `in_ECX[4]`
  - returns the maximum final width
- otherwise:
  - uses lane zero only
  - mirrors that width into all four stored maxima
  - returns it

### Best interpretation

The cleanest reading is:

- `FUN_0066B200(...)` computes the right edge / final width after a glyph or effect piece has been placed

So `FUN_0066B130(...)` and `FUN_0066B200(...)` together look like:

- left/base planner
- right/final extent planner

for a shared four-lane effect descriptor.

## `FUN_0066B4D0(...)`: alpha-mask compositor

This helper is the most concrete signal that the piece system is texture/mask backed.

### What it does

The structure is:

- call:
  - `FUN_0066BA80(*in_ECX + param_2)`
- derive a local clipped rectangle from that result and from `param_4`
- compute:
  - width delta
  - height delta
- if the region is non-empty:
  - derive a destination byte pointer inside:
    - `param_1`
  - step rows with stride:
    - `param_3`
  - walk source positions through:
    - `FUN_0066BDD0(...)`
  - fetch one alpha-like byte per source position through:
    - `FUN_0066BCF0(...)`
  - merge into destination as:
    - `dst = max(dst, src_alpha)`

### Best interpretation

The cleanest reading is:

- `FUN_0066B4D0(...)` composites one glyph/effect lane into a destination alpha bitmap or temporary texture buffer using max coverage

That is exactly the kind of inner loop you would expect for:

- outline or shadow accumulation
- monochrome glyph coverage masks
- temporary atlas/piece texture construction

## `FUN_0066B4A0(...)`: lightweight bounds query companion

This helper is much smaller:

- call:
  - `FUN_0066BA80(*in_ECX + param_2)`
- copy two local dwords out to `param_1[0..1]`

The best interpretation is:

- it queries lane/piece-local bounds or metrics without performing the full composition that `FUN_0066B4D0(...)` does

So it looks like a cheap metadata sibling for the compositor.

## `FUN_0064AF80(...)`: fallback piece lookup/retain

This helper is a tiny wrapper, but it reveals the ownership model:

- call:
  - `FUN_006488A0(...)`
- if a piece object is returned:
  - increment refcount at `returned + 4` under a lock

The strongest reading is:

- `FUN_0064AF80(...)` looks up or reuses an existing fallback/render piece object and retains it if found

That fits what `FUN_006536B0(...)` was doing:

- try existing piece first
- allocate a fallback piece only when no reusable one matches

## `FUN_0064AFB0(...)`: texture-backed fallback piece creator

This helper is also small, but its anchor is very strong:

- asserts or tags:
  - `P:\\Code\\Engine\\Gr\\GrTex2d.cpp`
- then calls:
  - `FUN_00647C30(param_1, param_2, param_3)`

The best interpretation is:

- `FUN_0064AFB0(...)` creates or initializes a `GrTex2d`-backed fallback piece object

So the fallback path is not abstract caching.
It is specifically a texture-backed graphics object path.

## Updated backend model

With this pass included, the current backend picture is:

1. `FUN_00653D40(...)`
   - measure how much text fits
2. `FUN_00613750(...)`
   - find a legal break boundary
3. `FUN_006530A0(...)`
   - measure the fitted span against glyph/effect metrics
4. `FUN_006541A0(...)`
   - iterate subpieces for that fitted span
5. `FUN_0066B130(...)` / `FUN_0066B200(...)`
   - compute per-piece left/right extents across up to four effect lanes
6. `FUN_0066B4D0(...)`
   - compose lane coverage into a destination alpha buffer
7. `FUN_0064AF80(...)` / `FUN_0064AFB0(...)`
   - reuse or create texture-backed fallback pieces
8. `FUN_00652500(...)`
   - assemble the composed rendered segment object
9. `FUN_00653A60(...)`
   - expose that object as an `HGrModel`

That means the backend under GWCA’s hooked frame callback is now clearly building real rendered text/image pieces, not just abstract layout tokens.

## Best next step

The next strongest targets are now the remaining inner helpers that should name the actual effect families and converter/object model:

- `FUN_00653890`
- `FUN_0066A8C0`
- `FUN_0066BA80`
- `FUN_0066BCF0`
- `FUN_006488A0`
- `FUN_00647C30`

That should answer:

- what effect families the four lanes actually represent
- how the converter is selected
- what exact object type sits behind the fallback piece path
