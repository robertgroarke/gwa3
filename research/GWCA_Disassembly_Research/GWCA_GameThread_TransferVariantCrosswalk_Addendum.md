# GWCA GameThread Transfer Variant Crosswalk Addendum

This pass lines the installed `(format, format, variant)` triples up against the concrete transfer-bank slices so we can stop talking about the bank abstractly.

## Main result

The transfer registry crosswalk is now concrete enough to expose an important asymmetry:

- `0x12` and `0x13` land on full compressed-family worker kernels
- `0x14` and `0x15` land on narrower support slices at this bank position

So the bank is not just “format family A, format family B, format family C” in a flat way. The static installer is wiring some format ids directly onto different layers of the same toolkit.

## Installed triples -> bank slice

From the static thunk run:

- `(0x12, 0x12, 0x1) -> 0x00A26C1C -> FUN_00675260`
- `(0x12, 0x12, 0x3) -> 0x00A26C20 -> FUN_00675E80`

- `(0x13, 0x13, 0x1) -> 0x00A26C24 -> FUN_00676A40`
- `(0x13, 0x13, 0x3) -> 0x00A26C28 -> FUN_006775A0`
- `(0x13, 0x13, 0x5) -> 0x00A26C2C -> FUN_006780B0`

- `(0x14, 0x14, 0x1) -> 0x00A26C30 -> FUN_00678A70`
- `(0x15, 0x15, 0x1) -> 0x00A26C34 -> FUN_00678AE0`

## What that means by family

### `0x12`

`0x12` has at least two sibling full-worker variants:

- `FUN_00675260`
- `FUN_00675E80`

Both are heavy row/block kernels:

- explicit block traversal
- RGB565 expansion via `DAT_00A2C6C8` and `DAT_00A2C948`
- interpolation via `DAT_00A283D8`
- temporary `16`-entry block buffers
- final materialization through `FUN_006A5CB0(...)`

So `variant = 1` and `variant = 3` are real worker variants inside the `0x12` family, not just bookkeeping noise.

### `0x13`

`0x13` has an even richer sibling family:

- `FUN_00676A40`
- `FUN_006775A0`
- `FUN_006780B0`

These are still on the same heavy compressed-family seam, but they visibly diverge in local packing/materialization strategy:

- all still use the same family tables and `FUN_006A5CB0(...)`
- but the tail calls and local packing state differ
- `FUN_006780B0` especially looks like the most distinct member of the three

So `0x13` is the clearest proof so far that the third selector field (`1 / 3 / 5`) is choosing concrete worker variants inside one family.

### `0x14`

This is where the bank gets interesting.

`(0x14, 0x14, 0x1)` does **not** land on another full block reconstructor. It lands on:

- `FUN_00678A70`

And that body is a compact aligned-size / row-count helper:

- computes aligned widths from `param_8`
- compares two incoming extents
- calls `FUN_0046D790(...)` once or once-per-row

So at this bank position, `0x14` is wired onto a support slice, not a heavyweight reconstruction kernel.

### `0x15`

`(0x15, 0x15, 0x1)` behaves the same way:

- `0x00A26C34 -> FUN_00678AE0`

This is the sibling size helper with a different width multiplier:

- same basic shape as `FUN_00678A70`
- different aligned-width calculation

So `0x15` also lands on the support layer at this point in the bank.

## Best current interpretation

The clearest working model is:

- `0x12` and `0x13`
  - map cleanly onto full compressed-family worker variants in this slice range
- `0x14` and `0x15`
  - map onto narrower support-layer entries in this same bank window

That does **not** necessarily mean `0x14` and `0x15` lack full worker bodies overall. It means the static registration points we have recovered for those exact triples land on support slices here.

This is consistent with the broader bank-role split from the last pass:

- earlier/middle bank entries = full family kernels
- later entries = sizing/materialization helpers

## Why this matters

This is the first time the static triple map has shown us something structurally non-flat:

- same registry
- same bank
- but not every format id attaches at the same layer of abstraction

That’s a strong hint that the installed triples are selecting not only a format family, but also the stage of the transfer pipeline that the record is meant to expose.

## Best next step

The next highest-value move is to widen the crosswalk around `0x14` and `0x15` instead of only walking forward in the bank:

- inspect nearby one-sided triples and non-symmetric installs involving `0x12`, `0x13`, `0x14`, and `0x15`
- especially the later static thunks around `0x0045CA24`, `0x0045CB74`, and `0x0045CE42`

That should tell us whether `0x14` and `0x15` have their full-worker entries elsewhere in the registry, with the currently recovered installs representing only support-stage registrations.
