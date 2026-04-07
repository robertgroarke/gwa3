## `Gw.exe` Frame Callback Aggregate Grid And Inherited State Addendum

This pass continues from the default-sources note by decompiling the aggregate child-measurement helpers and checking the immediate caller set of the inherited-state helper:

- `FUN_006194F0`
- `FUN_00620E90`
- `FUN_00621480`
- `FUN_006225F0`
- `FUN_00622550`
- callers of `FUN_00613400`

The goal was to answer two narrow questions:

- how the child/subcomponent aggregate measurement is actually combined
- what we can safely say about the inherited parent field copied from `+0x194`

## Source artifacts

These results come from:

- [gw_decomp_aggregate_helpers_temp89.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_aggregate_helpers_temp89.log)
- [gw_findcallers_00613400_temp89.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00613400_temp89.log)
- [gw_decomp_inherited_state_callers_temp90.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_inherited_state_callers_temp90.log)

## High-level result

This pass makes the aggregate fallback measurement much more concrete.

The strongest current model is:

- `FUN_00620E90(...)` seeds a nine-cell measurement grid around the requested width/height
- children contribute into that grid through virtual calls
- `FUN_00621480(...)` combines row/column triplets using one of two dimension functions
- `FUN_006225F0(...)` handles horizontal cell span
- `FUN_00622550(...)` handles vertical cell span
- center-bearing cells are treated specially using object fields at:
  - `+0xA0`
  - `+0xA4`

So the aggregate fallback is not “sum all children.”
It is a real nine-region layout-grid combine pass.

On the inherited-state side, the most important refinement is:

- `FUN_00613400(parent)` has only three caller sites in this explored build
- two of them are constructor paths:
  - `FUN_0060D300`
  - `FUN_0060BD30`
- the third is a root/control bootstrap path:
  - `FUN_0060EF50`

So the inherited `+0x194` field is strongly tied to control construction/bootstrap, not a general runtime helper used everywhere.

## `FUN_006194F0(out_size)`: table-backed default size source

This helper is the lower-level source behind `FUN_00629E10(...)`.

It does:

- if `*this == 0xFFFFFFFF`, return `(0, 0)`
- bounds-check the index against:
  - `DAT_00BB5530`
- read a `0x18`-byte record at:
  - `DAT_00BB5528 + index * 0x18`
- return:
  - `*(ushort *)(record + 0x14)`
  - `*(ushort *)(record + 0x16)`

So the default-size chain is now fully concrete:

- intrinsic short defaults at `+0x50/+0x52`
- else table-backed defaults from a global `0x18`-byte descriptor array

That is much better than the earlier generic “fallback descriptor helper” wording.

## `FUN_00620E90(size)`: seed a nine-cell measurement grid

This helper is the key to understanding the aggregate child measurement.

It initializes a local structure with:

- the requested width/height
- half-width / half-height
- corner, edge, and center anchor points

The layout is easiest to read as a 3x3 grid:

- top-left
- top-center
- top-right
- mid-left
- center
- mid-right
- bottom-left
- bottom-center
- bottom-right

The evidence for that interpretation is strong:

- the helper writes repeated quartets of floats
- many values are either:
  - `0`
  - `width`
  - `height`
  - `width * 0.5`
  - `height * 0.5`

So `FUN_00620E90(...)` is not just zeroing scratch space.
It is seeding a nine-region measurement/placement frame.

## `FUN_00621480(grid, a, b, c, fn, accum)`: combine a three-cell strip

This helper combines one strip of the nine-cell grid at a time.

Its behavior is:

- examine cells `a`, `b`, and `c`
- skip empty/degenerate cells
- compute contributions via the supplied callback `fn`
- if the middle cell exists:
  - take the max of outer-cell spans
  - add the middle-cell span
- otherwise:
  - sum the two available outer-cell spans
- add spacing derived from:
  - `FUN_0060E810(0)`
- update `*accum` if the new result is larger

This is very strong evidence that the aggregate pass is combining:

- left + center + right

or

- top + middle + bottom

triplets across a 3x3 grid.

That is much richer than a flat “sum child widths” fallback.

## `FUN_006225F0(grid, cell)`: horizontal span measure with center-aware special cases

This callback computes a span for one horizontal cell.

For ordinary cells it returns:

- `x2 - x1`

But for cells:

- `0`
- `3`
- `6`

it uses the object field:

- `*(float *)(grid + 0xA0) * 0.5`

and returns a mirrored/doubled span around that center.

That means these three cells form a special vertical strip, almost certainly the:

- left column

or another center-sensitive horizontal family.

The important takeaway is:

- the grid combine logic is center-aware, not just linear

## `FUN_00622550(grid, cell)`: vertical span measure with center-aware special cases

This is the vertical counterpart.

For ordinary cells it returns:

- `y2 - y1`

But for cells:

- `4`
- `3`
- `5`

it uses:

- `*(float *)(grid + 0xA4) * 0.5`

and again computes a mirrored/doubled span.

So the aggregate measurement grid uses:

- one center-sensitive horizontal grouping
- one center-sensitive vertical grouping

This is exactly the kind of logic you would expect from a layout engine that supports:

- symmetric center cells
- left/right or top/bottom balancing

## What this says about `FUN_00622760(...)`

The previous note established that `FUN_00622760(...)`:

- initializes a local structure
- gathers child contributions
- combines them with repeated `FUN_00621480(...)` calls

With the helper bodies now in hand, that aggregate pass is much clearer.

It is effectively doing:

- three horizontal strip combines
- three vertical strip combines

over a nine-cell grid, using different span rules for edge/center cells.

So the fallback measurement stage is best described as:

- a nine-region aggregate layout solver

not:

- a simple child bounding-box union

## `FUN_00613400(parent)`: inherited state is construction-scoped

The body was already known from the previous pass:

- if `parent != 0`
  - copy `*(parent + 0x194)` into the new/current object
- else
  - write `0`

What is new in this pass is the caller set.

`FindCallers` reports only three references:

- `FUN_0060D300`
- `FUN_0060BD30`
- `FUN_0060EF50`

That matters because it narrows the interpretation substantially.

This field copy is not a generic late-runtime synchronization helper.
It is a construction/bootstrap-time inheritance seam.

## `FUN_0060BD30`: constructor twin confirms the inheritance path

This helper is effectively a constructor twin of `FUN_0060D300(...)`.

It runs the same core sequence:

- `FUN_00614CB0()`
- `FUN_006161A0()`
- `FUN_00618020()`
- `FUN_00619400()`
- `FUN_0061A4F0()`
- `FUN_0061EEF0()`
- `FUN_006240D0()`
- `FUN_00627740(type)`
- `FUN_00629190()`
- `FUN_0062C650(parent, type, arg6)`
- `FUN_0062E520()`
- write style flags at `+0x190`
- `FUN_00613400(parent)`
- `FUN_00627D70(callback, payload)`
- `FUN_0062E5D0(4,0,0)`
- `FUN_00628A10(2,0,0)`
- `FUN_006286D0(10,0,0)`

So the inherited `+0x194` field is definitely part of normal control-node construction, not just an odd one-off path.

## `FUN_0060EF50`: root/bootstrap path also seeds the same inherited slot

This function is broader and more bootstrap-like, but it still contains the same constructor core:

- `FUN_006240D0()`
- `FUN_00627740(0)`
- `FUN_00629190()`
- `FUN_0062C650(0,0,0)`
- `FUN_0062E520()`
- `*(obj + 0x190) = 0`
- `FUN_00613400(0)`
- `FUN_0060BBD0()`
- `FUN_0062EFE0()`
- `FUN_0062F580()`
- `FUN_0062E5D0(4,0,0)`

That means the inherited field slot is part of the same control-object skeleton even for the root/bootstrap object, where it is explicitly zeroed via:

- `FUN_00613400(0)`

So we can now say confidently:

- the `+0x194`-derived field is a constructor-time inherited control state slot

even if we still do not know its final semantic name.

## Strongest current model after this pass

At this point the cleanest architecture picture is:

- default sizing comes from:
  - intrinsic per-object shorts
  - global table-backed defaults
  - nine-cell aggregate child measurement
- aggregate child measurement is center-aware and grid-based
- construction seeds:
  - runtime id
  - relation/hash metadata
  - callback/handler state
  - embedded subobject init
  - inherited parent state from `+0x194`

That is a fairly complete control construction and measurement story.

## Best next step

The highest-yield next pass is to stop widening the constructor/measure story and instead name the remaining control-class helpers that appear in every constructor:

- `FUN_00629190`
- `FUN_0062E520`
- `FUN_0060BBD0`
- `FUN_0062EFE0`
- `FUN_0062F580`

Those are now the main anonymous pieces left in the core control-node creation chain. If we recover them, we should be able to say what class/state layers every constructed frame/control node always gets before slot children and layout logic begin.  
