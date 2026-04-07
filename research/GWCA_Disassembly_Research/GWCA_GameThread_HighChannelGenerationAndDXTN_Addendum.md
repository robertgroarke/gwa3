# GWCA GameThread High Channel Generation And DXTN Addendum

This pass continues the concrete 30-function queue with the third batch:

- `FUN_005319B0`
- `FUN_00610F60`
- `FUN_0060CE70`
- `FUN_005F8810`
- `FUN_0069CC40`
- `FUN_0069C3F0`
- `FUN_0069F7F0`
- `FUN_006903C0`
- `FUN_006A19D0`
- `FUN_006A20A0`

The point of this batch was to test two different seams:

1. the text/generation helpers beneath the `CtlTextMl` worker family
2. the compressed-family branch around `DXTN` and the `0x200` capability bit

## Main Result

Batch 3 produced two important corrections.

First:

- `FUN_00610F60(...)`
- `FUN_0060CE70(...)`

are **not** the raw row/record producer seam we hoped for.

Instead:

- `FUN_00610F60(...)`
  - is a generated-extent measurer over `FUN_00614220(...)`
- `FUN_0060CE70(...)`
  - is a generated-handle/child commit path over the same generator

So the row-builder seam is still one layer deeper than this batch’s initial guess.

Second:

- `FUN_005F8810(...)`

turns out to be much more valuable than expected, because it reveals concrete use of:

- typed channel `7`
- typed channel `8`
- typed channel `9`

through:

- `FUN_006100A0(...)`

That extends the typed-channel map well beyond the lifecycle band recovered in batches 1 and 2.

On the compressed side, the batch keeps reinforcing the same boundary:

- low capability bit `0x8`
  - is generic pipeline behavior
- extra `0x200`
  - still looks compressed-family-specific

and the new pair:

- `FUN_0069CC40(...)`
- `FUN_0069C3F0(...)`

looks like the actual `DXTN`-side compact-family encoder branch rather than a broad image-pipeline mode.

## Text / Generation Side

### `FUN_005319B0(...)`: rect-to-size reducer

Fresh decompilation is tiny and decisive:

- validate `left <= right` and `top <= bottom`
- emit:
  - width = `right - left`
  - height = `bottom - top`

So this is exactly:

- rect-to-size reduction with validity check

This is the small glue helper the region/layout notes already implied.

### `FUN_00610F60(...)`: generated extent measurer, not raw record producer

This is the biggest correction from batch 3.

Fresh decompilation shows:

- validate owner and input
- optionally seed a local descriptor block from `param_9`
- force one descriptor flag bit on
- validate owner through:
  - `FUN_00628800(...)`
- call:
  - `FUN_00614220(...)`
- read the returned rectangle from the descriptor
- reduce it to width/height

So `FUN_00610F60(...)` is best described as:

- generated extent measurer over the owner-local generation path

not:

- raw row-array builder

### `FUN_0060CE70(...)`: generated output committer

Fresh decompilation shows a matching pattern:

- validate owner and input
- optionally use a supplied descriptor or a local temporary one
- call:
  - `FUN_00614220(...)`
- compare produced count against the original count
- if new output was generated:
  - forward it through:
    - `FUN_006163E0(...)`

So this is best described as:

- generated output / handle commit path

not:

- alternate raw row producer

Taken together:

- `FUN_00610F60(...)`
  - asks “how big is the generated output?”
- `FUN_0060CE70(...)`
  - asks “commit the newly generated output”

### `FUN_005F8810(...)`: concrete callback that exposes typed channels `7`, `8`, `9`

This is the biggest payoff in the whole batch.

The function is a concrete control callback with many message cases.

It:

- routes per-control behavior
- calls:
  - `thunk_FUN_00627BC0(...)`
  - `FUN_0060D300(...)`
  - `FUN_00610F60(...)`
  - `FUN_006100A0(...)`
- and handles multiple concrete message ids

The most important new results are the high typed-channel uses.

#### Typed channel `7`

Used from several cases including:

- `0x20`
- `0x24`
- `0x2E`
- `0x56`

through:

- `FUN_006100A0(owner, 7, ..., ...)`

The safe current reading is:

- high-band state-toggle / engagement channel

because these cases are all tied to flag transitions and owner-state toggling.

#### Typed channel `8`

Used from:

- case `0x25`

through:

- `FUN_006100A0(owner, 8, param_2, 0)`

The safest current reading is:

- high-band payload-bearing state-change / commit channel

#### Typed channel `9`

Used from:

- case `0x3A`

through:

- `FUN_006100A0(owner, 9, local_14, 0)`

The surrounding code updates text/data storage before dispatching this channel, so the safest current reading is:

- high-band content/update notification channel

### What this changes about the typed-channel map

After batch 3, the owner-local typed-channel plane is no longer just:

- `1` preflight
- `2` setup
- `3` activation
- `4` relation/layout
- `6` registration

It now clearly includes a higher band:

- `7`
- `8`
- `9`

used by a concrete control callback.

So the channel system now looks like:

- lower lifecycle/relation band
- higher control/content band

That is a real architectural shift.

## Compressed-Family Side

### `FUN_0069CC40(...)`: compact run encoder for the `DXTN` branch

Fresh decompilation shows:

- iterate candidate blocks/skipped blocks under a mask
- classify entries by a two-state test involving:
  - sentinel `-1`
  - ordered endpoint comparison
- build short run codes using:
  - `DAT_00A27B38`
  - `DAT_00A27B39`
- emit them bit-packed into the output stream
- set:
  - `*param_1 |= 1`

So this looks like:

- a compact run encoder for a one-bit/special-case lane inside the `DXTN` family branch

### `FUN_0069C3F0(...)`: dominant-value / special-mask encoder companion

Fresh decompilation shows a richer companion path:

- collect candidate values through:
  - `FUN_0069DD70(...)`
- sort / bucket them
- find the dominant repeated value
- canonicalize through:
  - `FUN_006A66C0(...)`
- emit another run-coded mask/value stream
- set:
  - `*param_1 |= 8`

So this looks like:

- the richer dominant-value / special-mask encoder companion to `FUN_0069CC40(...)`

The safest current combined reading is:

- `FUN_0069CC40(...)`
  - compact boolean/special run lane
- `FUN_0069C3F0(...)`
  - dominant-value / selector-like companion lane

That is exactly the kind of pair we would expect on the `DXTN`-specific branch opened by the `0x200` capability bit.

### `FUN_0069F7F0(...)`, `FUN_006903C0(...)`, `FUN_006A19D0(...)`, `FUN_006A20A0(...)`

The generic pipeline side stays consistent under fresh decompilation.

#### `FUN_0069F7F0(...)`

- BMP-like parser / internal-format mapper
- checks:
  - `FUN_00689E90(format) & 8`
- does not use:
  - `0x200`

#### `FUN_006903C0(...)`

- cached processor/front-door constructor
- uses:
  - `FUN_00689E90(format) & 8`
- does not use:
  - `0x200`

#### `FUN_006A19D0(...)`

- palettized/export helper
- for 8-bit-style formats:
  - copies supplied palette when `(flags & 8) != 0`
  - synthesizes grayscale-like palette otherwise

#### `FUN_006A20A0(...)`

- another export/build helper
- uses:
  - `(flags & 8)` to decide whether a `0x300` palette block is present
- does not expose a broader `0x200` pipeline mode

So batch 3 continues to reinforce the same boundary:

- bit `0x8`
  - generic palette/aux-plane pipeline semantics
- bit `0x200`
  - still compressed-family specific

## Best Current Interpretation

After batch 3, the best current model is:

### Typed channels

- lower band:
  - lifecycle / relation / registration
- higher band:
  - concrete control/content notifications through channels `7`, `8`, `9`

### Text / generation

- `FUN_00610F60(...)`
  - extent measurer over generated output
- `FUN_0060CE70(...)`
  - generated output committer
- raw row/record production seam:
  - still deeper than this batch

### Compressed-family branch

- `DXTN`'s extra `0x200`
  - still best explained as a compact-family branch selector
- `FUN_0069CC40(...)` / `FUN_0069C3F0(...)`
  - now look like the concrete encoder pair on that branch

## What This Batch Corrected

The most useful correction is:

- the third-batch “builder” targets were one layer above the missing raw builder seam

That is still good news, because it narrows the search:

- the real data-production layer is now more likely beneath:
  - `FUN_00614220(...)`
  - or the helpers it uses

not beneath:

- `FUN_00610F60(...)`
- `FUN_0060CE70(...)`

## Best Next Step

The strongest next move after batches 2 and 3 is now:

1. follow the high typed-channel band from:
   - `FUN_005F8810(...)`
   - especially channels `7`, `8`, `9`
2. chase the generation seam one layer deeper under:
   - `FUN_00614220(...)`
3. continue the compressed-family branch around:
   - `FUN_0069CC40(...)`
   - `FUN_0069C3F0(...)`
   - `FUN_0069E870(...)`
   - `FUN_0069E1C0(...)`

That should answer:

- what the high-band typed channels actually mean
- where generated row/control output is really produced
- and what exact compact-family concept the `DXTN` branch represents

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_batch3_queue_temp184.log`
