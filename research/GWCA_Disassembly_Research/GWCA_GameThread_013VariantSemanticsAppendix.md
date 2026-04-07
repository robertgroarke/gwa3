# GWCA GameThread 013 Variant Semantics Appendix

This appendix tightens the current meaning of the `0x13` transfer-family sibling variants.

The current installed symmetric registrations are:

- `(0x13, 0x13, 0x1) -> FUN_00676A40`
- `(0x13, 0x13, 0x3) -> FUN_006775A0`
- `(0x13, 0x13, 0x5) -> FUN_006780B0`

So `0x13` is the clearest current proof that the third selector field is a real worker-variant axis.

This note does not claim the final exact external meaning of `1 / 3 / 5`.
It freezes the strongest current structural reading and separates that from the still-open semantic gap.

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_TransferVariantMatrixAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferVariantMatrixAppendix.md)
- [GWCA_GameThread_TransferVariantCrosswalk_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferVariantCrosswalk_Addendum.md)
- [GWCA_GameThread_TransferBankRoleSplit_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBankRoleSplit_Addendum.md)
- [GWCA_GameThread_TransferBankKernelSlices_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBankKernelSlices_Addendum.md)
- [GWCA_GameThread_013VariantTailPatterns_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_013VariantTailPatterns_Addendum.md)

## Stable Facts

What is already strong:

- all three `0x13` variants land on heavy compressed-family workers
- all three stay in the same broad middle-bank family layer
- all three reuse the same familiar compressed-image machinery:
  - RGB565 expansion tables
  - interpolation tables
  - local `16`-entry block buffers
  - tail materialization through `FUN_006A5CB0(...)`

So the current question is no longer:

- are these real variants?

That part is already closed.

The narrower question is:

- what kind of variant axis `1 / 3 / 5` actually encodes inside the `0x13` family?

## Installed Crosswalk

The current installed crosswalk is:

| triple | bank slice | target | current reading |
| --- | --- | --- | --- |
| `(0x13, 0x13, 0x1)` | `0x00A26C24` | `FUN_00676A40` | baseline heavy sibling |
| `(0x13, 0x13, 0x3)` | `0x00A26C28` | `FUN_006775A0` | second heavy sibling |
| `(0x13, 0x13, 0x5)` | `0x00A26C2C` | `FUN_006780B0` | most distinct heavy sibling |

## What Separates Them Structurally

### `variant = 1` -> `FUN_00676A40`

This is the cleanest current baseline inside the `0x13` family.

What is strongest here:

- still a full compressed-family worker
- same table-heavy block reconstruction style
- tail materialization through:
  - `FUN_006A5CB0(..., 3, 0)`

So `variant = 1` is the best current baseline form for `0x13`.

### `variant = 3` -> `FUN_006775A0`

This stays on the same broad family seam as `FUN_00676A40(...)`.

What is strongest here:

- still a full worker, not a support helper
- same family tables and buffer shape
- local reconstruction state differs from the baseline
- tail materialization still stays on:
  - `FUN_006A5CB0(..., 3, 0)`

So `variant = 3` is best treated as:

- a sibling local reconstruction / packing variant over the same `0x13` family

### `variant = 5` -> `FUN_006780B0`

This is the most distinct member of the three.

What is strongest here:

- still clearly belongs to the same heavy middle-bank family layer
- still reuses the same broad compressed-family machinery
- still lands on:
  - `FUN_006A5CB0(..., 3, 0)`
- but local layout / reconstruction diverges the most from the earlier two

So `variant = 5` is best treated as:

- the furthest-shifted local worker form inside the `0x13` family

## Best Current Reading of the Variant Axis

The safest current interpretation is:

- `1 / 3 / 5` are not different registries
- `1 / 3 / 5` are not just bookkeeping noise
- `1 / 3 / 5` are sibling worker variants inside one `0x13` transfer family

And the strongest current subtype of that statement is:

- the axis is probably about local reconstruction / packing policy ahead of a shared materializer

This is stronger than a generic "mode" label because we already have visible differences in:

- local interpolant and lookup-table shape
- local layout shape
- and how distinct `FUN_006780B0(...)` looks from the earlier siblings even though all three still land on `FUN_006A5CB0(..., 3, 0)`

## What We Should Not Overclaim Yet

The current notes are not strong enough yet to freeze a more specific claim like:

- `variant = 1` = scalar-only
- `variant = 3` = scalar-plus-companion
- `variant = 5` = special post-pack path

That may end up being true, but it is not yet the strongest source-anchored statement.

The present evidence is still better summarized as:

- same family
- same broad worker type
- different local packing / reconstruction strategy

## Relationship to the Broader DXT Family Work

This is still useful even at that cautious level.

Elsewhere in the DXT-family recovery we already established real split axes like:

- scalar/alpha-side only
- scalar/alpha plus companion BC1-style side
- later staged output tiers

So the `0x13` worker-variant split now looks very likely to be another layer of that same general pattern:

- family semantics fixed
- worker-local materialization policy varied

That is exactly the kind of lower-level distinction we have already seen in:

- neighborhood worker skeletons
- repacker-only policy variants
- and asymmetric transfer tiers

## Practical Classifier

For the `0x13` family specifically, the fastest current classifier is:

1. if the install triple is `(0x13, 0x13, 0x1)`
   - treat it as the baseline symmetric heavy worker
2. if the install triple is `(0x13, 0x13, 0x3)`
   - treat it as the second symmetric heavy worker variant
3. if the install triple is `(0x13, 0x13, 0x5)`
   - treat it as the most distinct symmetric heavy worker variant
4. do not treat `1 / 3 / 5` as final semantic names yet
   - treat them as concrete local worker-variant selectors

## Best Next Step

The strongest next reverse step is to compare one adjacent sibling family with the same method:

- `FUN_006743A0(...)` vs `FUN_00674B20(...)`
- or `FUN_00675260(...)` vs `FUN_00675E80(...)`

For `0x13`, the tail-argument part is now closed:

- all three symmetric siblings use `FUN_006A5CB0(..., 3, 0)`

So the next pass should answer:

- whether this shared-tail pattern repeats in neighboring symmetric families
- whether any variant writes only one recovered plane or both
- and whether the remaining local record-layout differences line up with the scalar-only vs companion-bearing split we already know from the codec seam
