## `Gw.exe` Frame Callback Segment-To-Object Pipeline Addendum

This pass followed the next backend targets:

- `FUN_00653D40`
- `FUN_00613750`
- `FUN_00652500`
- `FUN_00653C40`
- `FUN_00653BA0`

The goal was to close the remaining gap in the backend pipeline:

- how a UTF-16 segment is measured
- how break opportunities are classified
- how the final concrete emitted object is created

## Source artifacts

These results come from:

- [gw_decomp_measure_construct_temp117.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_measure_construct_temp117.log)
- [gw_findcallers_00653d40_temp118.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00653d40_temp118.log)

## High-level result

This pass closes the main pipeline cleanly:

- `FUN_00653D40(...)` is the width-fit measurer
- `FUN_00613750(...)` is the break-opportunity classifier
- `FUN_00652500(...)` is the real concrete object constructor
- `FUN_00653A60(...)` is the wrapper that turns that object into an `HGrModel` handle

It also tightened the policy side:

- `FUN_00653C40(...)` creates the default font object backing the cache layer
- `FUN_00653BA0(...)` creates an alternate/default font handle depending on a mode argument

So the “segment to emitted handle” story is now substantially complete.

## `FUN_00653D40(...)`: width-fit measurer

The caller map is extremely tight:

- only `FUN_00613DC0(...)` calls it

That matches the behavior we inferred last pass.

### What it does

Its inputs are roughly:

- owner/context root
- UTF-16 text pointer
- UTF-16 length
- width scaling factor
- available width
- style/font selector
- an integer out-count
- optional float out-measured-width

The flow is:

1. resolve required font/owner context through:
   - `FUN_0046F9B0(param_1, 0x6772666F)`
2. validate:
   - non-null text
   - positive available width
   - at least one output sink (`param_7` or `param_8`)
3. derive a count-like threshold:
   - `(available_width * 0.5 + font_metric * scale) / available_width`
   - rounded through `FUN_005A7310()`
4. call:
   - `FUN_006530A0(&local_c, text_begin, text_end, fitted_count, style, out_count_slot)`
5. optionally back-compute the actual measured width into `param_8`

So the cleanest interpretation is:

- `FUN_00653D40(...)` converts width/scale/font context into a maximum code-unit span
- then asks `FUN_006530A0(...)` how many UTF-16 code units fit
- then optionally translates the fit count back into actual width

That makes it the real width-fit measurer for line segmentation.

## `FUN_00613750(...)`: break-opportunity classifier

This helper is now very clear.

### Base behavior

It returns `true` when the current UTF-16 boundary is considered a valid break point.

Fast rules:

- if `param_1 == param_2`
  - break if current code unit `< 0x21`
- otherwise:
  - break if current code unit `< 0x21`
  - or if current code unit is in a classification set from `FUN_00614180(...)`
  - or if previous code unit is in that classification set

### Table-driven behavior

If those quick conditions hold, it then binary-searches two static code-point tables:

- `DAT_00BB53F8 .. DAT_00BB5486`
- `DAT_00BB5488 .. DAT_00BB54B6`

The final meaning is:

- if current or previous code unit falls into certain protected table ranges, return `false`
- otherwise return `true`

So this is not generic “is whitespace?”
It is a Unicode-ish line-break boundary classifier with exception tables.

That strongly validates the earlier interpretation of `FUN_00613DC0(...)`:

- fit width first
- then back up to a legal break boundary

## `FUN_00652500(...)`: concrete emitted object constructor

This function is the biggest payoff in the pass.

It is clearly a real constructor pipeline, not just a tiny allocator.

### Setup phase

It:

- tokenizes/measures input text with:
  - `FUN_006530A0(...)`
- loads incoming rect and style state from `param_4`
- calls:
  - `FUN_00669800(...)`
  - `FUN_00669DF0(...)`

Those appear to derive layout slices / rows / metric partitions for the segment.

### Per-slice allocation phase

It allocates an array of per-slice objects:

- through `FUN_006903C0(...)`
- one entry per computed slice/row

Then it iterates a generated stream through:

- `FUN_006541A0(...)`
- `FUN_00669D90(...)`
- `FUN_00669DE0(...)`
- `FUN_0066A950(...)`

That is a real staged construction pipeline:

- enumerate segment pieces
- query row/slice info
- build piece objects into the per-slice array

### Final object assembly phase

After the per-slice work, it allocates final arrays and computes per-slice widths through:

- `FUN_006536B0(...)`

Then it stores those computed values back alongside the per-slice array.

So the strongest current interpretation is:

- `FUN_00652500(...)` constructs a composed rendered-text object made of one or more sub-piece objects/slices

It is not “just make one glyph” and not “just one row.”
It is a compound object builder for the chosen UTF-16 segment.

That fits perfectly with `FUN_00653A60(...)` immediately asking it for an `HGrModel` handle.

## `FUN_00653A60(...)` in light of this pass

With `FUN_00652500(...)` decoded, `FUN_00653A60(...)` is now much clearer:

- build a composed rendered object from the chosen segment
- obtain current runtime context
- query `"HGrModel"` via tag `0x67726D64`
- release the temporary object reference
- return the model handle

So the produced handle is the render/model handle for a composed text-segment object.

That is the cleanest statement of the backend output we’ve had so far.

## `FUN_00653C40(...)` and `FUN_00653BA0(...)`: font handle creators

These two functions explain the policy-cache helpers from the previous pass.

### `FUN_00653C40(...)`

This function:

- builds a default font descriptor:
  - `0x30`
  - `0xFF00`
  - `0xFFFF`
- gathers additional state through:
  - `FUN_0066AA80(...)`
- constructs a font object through:
  - `FUN_00651E20(...)`
- finalizes temp resources through:
  - `FUN_00652300(...)`
- queries `"HGrFont"` via:
  - `FUN_0046F7B0(..., "HGrFont", 0x6772666F, ...)`

So this is a default font-handle constructor.

### `FUN_00653BA0(...)`

This is a simpler variant:

- if `param_1 == 0`
  - use default descriptor fields `0x30 / 0xFF00 / 0xFFFF`
- else
  - use `0xFFFFFFFF`
- then build font object with:
  - `FUN_00651E20(...)`
- extract `"HGrFont"` handle

So `FUN_00653BA0(...)` is an alternate/default font-handle resolver used by the cache layer.

This is why the policy cache in `FUN_00612C20(...)` could derive font/style values without being the renderer itself.

## What this pass changes

Before this pass, we had the producer engine but not the full lower pipeline.

After this pass, the pipeline is much clearer:

- `FUN_00653D40(...)`
  - measure how much text fits
- `FUN_00613750(...)`
  - decide whether a boundary is a legal break point
- `FUN_00613DC0(...)`
  - combine those two to choose the next segment
- `FUN_00652500(...)`
  - construct the composed rendered object for that segment
- `FUN_00653A60(...)`
  - extract and return the segment’s `HGrModel` handle

So the core “text segment to render handle” path is now materially recovered.

## Updated working model

The cleanest current backend model is now:

- policy/default font:
  - `FUN_00613F40`
  - `FUN_00612C20`
  - `FUN_00653C40`
  - `FUN_00653BA0`
- measure and segment:
  - `FUN_00653D40`
  - `FUN_00613750`
  - `FUN_00613DC0`
- build emitted object:
  - `FUN_00652500`
- extract render/model handle:
  - `FUN_00653A60`
- consume and register handles:
  - `FUN_006163E0`

That is the strongest end-to-end decomposition yet for this branch of the reverse work.

## Best next step

The next best step is to decompile the inner construction helpers beneath `FUN_00652500(...)`:

- `FUN_006530A0`
- `FUN_006541A0`
- `FUN_0066A950`
- `FUN_006536B0`
- `FUN_00669800`
- `FUN_00669DF0`

Why these are now the right targets:

- `FUN_006530A0(...)` appears to be the actual fit-count / text-span enumerator
- `FUN_006541A0(...)` looks like the segment-piece iterator
- `FUN_0066A950(...)` appears to construct each per-piece object
- `FUN_006536B0(...)` computes the final per-piece width/value written into the assembled object
- `FUN_00669800(...)` and `FUN_00669DF0(...)` set up the slice/layout partitioning that drives the whole constructor

That should let the next pass answer the remaining deep question:

- what exact sub-piece objects make up one composed rendered text segment before it becomes an `HGrModel`.*** End Patch
