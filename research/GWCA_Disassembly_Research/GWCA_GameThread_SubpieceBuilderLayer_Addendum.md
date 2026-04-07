## `Gw.exe` Frame Callback Subpiece Builder Layer Addendum

This pass followed the next helpers under `FUN_00652500(...)`:

- `FUN_006530A0`
- `FUN_006541A0`
- `FUN_0066A950`
- `FUN_006536B0`
- `FUN_00669800`
- `FUN_00669DF0`

The goal was to identify what the composed rendered text object is made of before it becomes an `HGrModel`.

## Source artifacts

These results come from:

- [gw_decomp_subpiece_layer_temp119.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_subpiece_layer_temp119.log)

## High-level result

This pass makes the construction layer much more concrete.

The best current decomposition is:

- `FUN_006530A0(...)`
  - span measurer over UTF-16 text using glyph metrics and mode flags
- `FUN_006541A0(...)`
  - per-subpiece iterator / segment enumerator over the fitted span
- `FUN_0066A950(...)`
  - converter-driven per-subpiece constructor
- `FUN_006536B0(...)`
  - piece-width query/fallback creator over constructed subpiece objects
- `FUN_00669800(...)`
  - source-rect to sprite/texture clip mapping helper
- `FUN_00669DF0(...)`
  - atlas/tiling size planner

So one composed rendered text segment is not a monolithic object.
It is built from one or more converter-generated subpieces whose layout and texture sizing are planned first.

## `FUN_006530A0(...)`: span measurer over UTF-16 text

This function is the deeper span measurer used by `FUN_00653D40(...)`.

### What it does

Its behavior is:

- initialize optional out-count to zero
- if a cached glyph state exists at `this + 8`, refresh it through:
  - `FUN_00653230()`
- seed a local spacing/advance baseline from:
  - `this + 0x2C`
- optionally add one unit when `(param_5 & 1) != 0`
- initialize glyph metrics through:
  - `FUN_0066B0B0()`
- scan UTF-16 code units from `param_2` to `param_3`

For each code unit:

- special-case underscore `'_'` when `(param_5 & 2) != 0`
  - with a one-shot toggle behavior
- otherwise classify the code unit through:
  - `FUN_00653890(...)`
- when classification succeeds:
  - resolve glyph/effect data
  - call:
    - `FUN_0066B130(...)`
    - `FUN_0066B200(...)`
- stop when the accumulated width would exceed `param_4`

On success it writes:

- `*param_1 = fitted_width`
- `param_1[1] = this->field_0x20`

So the cleanest reading is:

- `FUN_006530A0(...)` measures how many code units fit into a width budget and what width they consume

That fits exactly with the wrapper chain we recovered earlier:

- `FUN_00653D40(...)`
  - computes a budget
- `FUN_006530A0(...)`
  - counts how many UTF-16 code units fit inside it

## `FUN_006541A0(...)`: subpiece iterator over the fitted span

This function is larger, but the structure is now clear enough.

### What it consumes

It takes:

- current iterator state
- a width/metric pair block
- the current UTF-16 pointer and end pointer
- mutable text pointer / state outputs
- flag bits
- optional min/max sinks

### What it does

The function:

- seeds spacing state from `this + 0x2C`
- calls:
  - `FUN_00669CB0(...)`
  - `FUN_00669FA0(...)`
- refreshes glyph metric state through:
  - `FUN_0066B090(...)`
- iterates UTF-16 text from the current pointer to the fitted end
- again handles underscore `'_'` specially when `(param_7 & 2) != 0`
- classifies code units through:
  - `FUN_00653890(...)`
- computes piece-local spans and offsets using:
  - `FUN_0066B130(...)`
  - `FUN_0066B4D0(...)`
  - `FUN_0066B4A0(...)`

At the end it clamps and exports:

- lower bound into `param_8`
- upper bound into `param_9`

and returns `1` on success.

So this is best understood as the iterator that breaks the fitted span into one or more renderable subpieces with per-piece bounds.

That is why `FUN_00652500(...)` calls it in a loop.

## `FUN_0066A950(...)`: per-subpiece constructor via converter object

This is the clearest “piece builder” in the pass.

### Converter selection

It starts by masking the incoming effect/style bits:

- `effects = flags & 0x7D`

Then it resolves a converter through:

- `FUN_0066A8C0(&local_c)`

If no converter exists for those effects:

- it logs:
  - `FontConvert(): No converter available for effects 0x%x`
- retries with effect mask `0`

That is strong evidence this function is dispatching to effect-specific glyph/subpiece conversion implementations.

### Piece construction

Then, for each requested subpiece:

- it computes downscaled source and destination widths/heights by shifting with the loop index
- calls the converter vtable:

```cpp
(**(code **)*param_7)(dst, width, src, &dims, param_8);
```

So `FUN_0066A950(...)` is not choosing text segments.
It is taking already identified subpieces and building concrete converted render pieces for them.

That is the strongest subpiece-constructor result in the pass.

## `FUN_006536B0(...)`: width query / fallback object creation

This helper walks the already-constructed subpiece list:

- `this + 0x2C`
- count at `this + 0x34`

It first tries:

- `FUN_0064AF80(existing_piece, ...)`

for every piece, returning immediately if one succeeds.

If none do, it creates a new fallback piece:

- `FUN_0064AFB0(&local_1c, 2, 0x20)`

then appends it to the same piece list and calls:

- `FUN_0064AF80(new_piece, ...)`

So this function is best described as:

- "ensure there is a subpiece capable of answering this width/value query"

That makes it a fallback width-query creator over the built piece list, not part of initial text segmentation.

## `FUN_00669800(...)`: source rect to clipped sprite/texture mapping

This helper seeds a small structure with:

- type/shape markers set to `4`
- zeroed clipping fields

Then, when non-zero source dimensions exist, it computes clipped source extents against the destination rect:

- left clip
- right clip
- top clip
- bottom clip

using helpers like:

- `FUN_0046DC80(...)`

If the resulting clipped extents collapse, it zeroes the clip state and finalizes through:

- `FUN_00669A90(1)`

So this is the rect-to-clipped-sprite mapping helper used before actual per-piece construction.

That fits with `FUN_00652500(...)` calling it before the subpiece loop.

## `FUN_00669DF0(...)`: atlas / tiling size planner

This function computes output grid and texture/atlas sizes from current builder state.

Key behaviors:

- derive a piece-count-like value from `this + 0x1C` and `this + 0x14`
- derive a packing ratio from `0x100 / *(this + 4)`
- normalize intermediate sizes to powers of two with:
  - `FUN_0046DCF0()`
- choose:
  - number of rows/pages
  - primary texture width/height
  - backing storage sizes
  - final slice count bounds

Outputs:

- `*param_1`
  - page/count-like value
- `*param_2`, `param_2[1]`
  - chosen width/height
- `*param_3`, `param_3[1]`
  - backing texture/storage dimensions
- optional:
  - `param_4`
  - `param_5`
    receive clamped count bounds

So this is best described as the texture/atlas planner beneath the composed text-object builder.

That explains why `FUN_00652500(...)` uses it before allocating its per-slice arrays.

## What this pass changes

Before this pass, we knew one text segment became one composed object, but not what that object was made of.

After this pass, the picture is sharper:

- the segment is first measured as a width-fit span
- that span is then iterated into renderable subpieces
- each subpiece is built by an effect-specific converter
- clipping and atlas sizing are planned separately
- a fallback piece list exists for later width/value queries

So the composed text object is not just “text plus a font.”
It is a managed collection of converter-built subpieces with clipping and atlas planning around them.

## Updated working model

The cleanest current construction stack is now:

- `FUN_006530A0(...)`
  - measure how much text fits
- `FUN_00613DC0(...)`
  - choose legal break boundary
- `FUN_006541A0(...)`
  - iterate the fitted span into renderable subpieces
- `FUN_00669800(...)`
  - compute clipped source/destination mapping
- `FUN_00669DF0(...)`
  - plan atlas / storage dimensions
- `FUN_0066A950(...)`
  - build concrete converted subpieces
- `FUN_006536B0(...)`
  - answer later width/value queries over the built piece list
- `FUN_00652500(...)`
  - assemble the final composed object from those parts

That is the strongest “inside `FUN_00652500`” model we have recovered so far.

## Best next step

The next best step is to decompile the remaining inner helpers that appear to own the real piece enumeration and conversion details:

- `FUN_00669CB0`
- `FUN_00669FA0`
- `FUN_0066B130`
- `FUN_0066B200`
- `FUN_0066B4D0`
- `FUN_0066B4A0`
- `FUN_0064AF80`
- `FUN_0064AFB0`

Why these are now the right targets:

- `FUN_00669CB0(...)` and `FUN_00669FA0(...)` appear to initialize the piece iterator state
- `FUN_0066B130(...)` / `FUN_0066B200(...)` appear to translate classified code points into width/effect spans
- `FUN_0066B4D0(...)` / `FUN_0066B4A0(...)` appear to compute piece-local offsets/metrics
- `FUN_0064AF80(...)` / `FUN_0064AFB0(...)` define the fallback width-query piece object family

That should let the next pass answer the remaining deep construction question:

- exactly what piece classes/effects are being emitted for one fitted text span before final object assembly.
