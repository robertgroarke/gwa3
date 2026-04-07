# GWCA GameThread 013 Variant Boundary Appendix

This appendix narrows the remaining semantic gap around the `0x13` sibling variants by comparing them to the adjacent transfer tiers.

The current symmetric `0x13` installs are:

- `(0x13, 0x13, 0x1) -> FUN_00676A40`
- `(0x13, 0x13, 0x3) -> FUN_006775A0`
- `(0x13, 0x13, 0x5) -> FUN_006780B0`

The question was whether these variants are already exposing:

- one-sided reconstruction stages
- or direct output/materialization stages

The strongest current answer is:

- no
- those roles already live in the adjacent asymmetric `(0x13, 0, 1)` tier and the later `(0x18, 0x13, 0)` tier

So the symmetric `1 / 3 / 5` split should now be read much more narrowly as:

- local heavy-worker variants inside one `0x13` family

not:

- stage-plane selectors by themselves

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_013VariantSemanticsAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_013VariantSemanticsAppendix.md)
- [GWCA_GameThread_AsymmetricTierVsStage18_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_AsymmetricTierVsStage18_Addendum.md)
- [GWCA_GameThread_Stage18OutputTier_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_Stage18OutputTier_Addendum.md)
- [GWCA_GameThread_Stage18PerFormatCrosswalk_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_Stage18PerFormatCrosswalk_Addendum.md)

## The Three `0x13` Layers

For `0x13`, the registry now exposes three distinct planes:

### Symmetric heavy-worker tier

- `(0x13, 0x13, 0x1) -> FUN_00676A40`
- `(0x13, 0x13, 0x3) -> FUN_006775A0`
- `(0x13, 0x13, 0x5) -> FUN_006780B0`

These are the sibling variants we are trying to tighten.

### One-sided reconstruction tier

- `(0x13, 0x0, 0x1) -> FUN_0067AFF0`

This is already better explained as:

- one-sided mode-`0` reconstruction stage

not as another symmetric sibling.

### Direct output tier

- `(0x18, 0x13, 0x0) -> FUN_00681ED0`

This is already better explained as:

- direct `DXT5`-family output/materialization

not as another symmetric sibling either.

## What This Closes

This comparison closes one important ambiguity.

The symmetric `0x13` variants are not best read as:

- the one-sided reconstruction tier in disguise
- or the direct-output tier in disguise

Those jobs already have separate installed records.

So the symmetric `1 / 3 / 5` axis is now bounded more tightly:

- it is an intra-family worker-variant split inside the same heavy symmetric tier

That is a useful reduction in uncertainty even before the exact per-variant labels are known.

## Why This Matters

Without this comparison, it was still too tempting to overread the symmetric `1 / 3 / 5` variants as if one of them might secretly be:

- the alpha-only lane
- the output lane
- or the one-sided reconstruction lane

The current cross-tier evidence argues against that.

Those roles already have their own installed selectors:

- one-sided reconstruction:
  - `(0x13, 0x0, 0x1)`
- direct output:
  - `(0x18, 0x13, 0x0)`

So the remaining `1 / 3 / 5` problem is narrower and more local than before.

## Best Current Reading

The safest current interpretation is:

- `FUN_00676A40(...)`, `FUN_006775A0(...)`, and `FUN_006780B0(...)`
  - are three local heavy-worker forms inside the same symmetric `0x13` family
- `FUN_0067AFF0(...)`
  - is the one-sided reconstruction-stage worker for `0x13`
- `FUN_00681ED0(...)`
  - is the direct output/materialization worker for `0x13`

That means the remaining semantic gap is now specifically about:

- local packing / materialization policy *within* the symmetric heavy-worker tier

not about the larger stage/tool-family architecture.

## What Still Needs Fresh Work

The exact meaning of `1 / 3 / 5` is still not frozen.

The strongest remaining open questions are:

- whether the three symmetric workers differ by:
  - one-plane vs two-plane use
  - packer mode
  - companion-stream handling
  - or another local materialization policy

But those are now clearly lower-level sibling questions, not registry-plane questions.

## Best Next Step

The strongest next reverse step is still the same focused decompilation pass:

- `FUN_00676A40(...)`
- `FUN_006775A0(...)`
- `FUN_006780B0(...)`

with the specific goal of extracting:

- exact `FUN_006A5CB0(...)` call signatures
- whether any recovered path only packs one plane or both
- and whether the most distinct worker `FUN_006780B0(...)` corresponds to a special companion/materialization policy

That should be enough to replace the current bounded reading:

- “local heavy-worker variants”

with:

- real per-variant semantic labels.
