# GWCA GameThread Worker Subtype Appendix

This appendix reorganizes the current transfer-bank branch by worker subtype instead of by family/mode.

It is meant to complement:

- [GWCA_GameThread_TransferBank_Crosswalk.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBank_Crosswalk.md)
- [GWCA_GameThread_FamilyModeAppendix.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_FamilyModeAppendix.md)

## Current Worker Subtypes

The current transfer-bank branch supports four practical worker subtypes:

1. neighborhood edit worker
2. source bridge worker
3. transform bridge worker
4. final materializer

This appendix summarizes each subtype by:

- representative functions
- source-side input shape
- family behavior
- stage behavior

## Neighborhood Edit Worker

Best current description:

- expands an intermediate compact family
- blends against local neighborhood pixels
- repacks back into the same family

Representative functions:

- `FUN_00679D70`
- `FUN_0067A6B0`
- `FUN_0067BDD0`
- `FUN_0067A210`
- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067AB50`
- `FUN_0067AFF0`
- `FUN_0067B490`
- `FUN_0067B930`
- `FUN_0067CAC0`
- `FUN_0067CF10`
- `FUN_0067D360`
- `FUN_0067D7B0`

Typical source-side input shape:

- compact family-specific block records

Family behavior:

- stays inside one intermediate family
- does not change `block_kind`
- mode affects normalization/companion interpretation

Stage behavior:

- fully inside the intermediate edit/repack plane

Best current use:

- this is the default subtype when a caller uses `FUN_006A5F10(...)` / `FUN_006A5CB0(...)` and does local 4x4 neighborhood-style blending
- `FUN_0067A6B0(...)` is now a clean family-`2`, mode-`0` example of that default shape
- current neighborhood workers also appear to split into at least two local loop skeletons, even while staying inside the same subtype

## Source Bridge Worker

Best current description:

- derives a working block from a different source encoding
- then repacks into one of the standard intermediate families

Representative functions:

- `FUN_0067DC00`
- `FUN_0067E2F0`
- `FUN_0067E9C0`
- `FUN_0067F090`
- `FUN_0067F7D0`
- `FUN_00680140`
- `FUN_00684100`
- `FUN_006848D0`
- `FUN_006803F0`

Typical source-side input shape:

- nonstandard source payloads that are not already in the normal compact-family expander shape
- current sampled cases include:
  - 16-bit source texels
  - mixed compact scalar/color payloads
  - locally decoded selector/end-point feeder forms

Family behavior:

- bridges *into* an existing family rather than staying in a like-for-like compact source family
- current known bridges:
  - `FUN_0067DC00`
    - into `block_kind 2`, mode `0`
    - uses the repacker side only
  - `FUN_0067E2F0`
    - into `block_kind 2`, mode `0`
    - uses the repacker side only
  - `FUN_0067E9C0`
    - into `block_kind 1`, mode `0`
    - uses the repacker side only
  - `FUN_0067F090`
    - into `block_kind 1`, mode `0`
    - uses the repacker side only
  - `FUN_0067F7D0`
    - into `block_kind 1`, mode `0`
    - uses the repacker side only
  - `FUN_00680140`
    - into `block_kind 3`, mode `0`
    - joins from the repacker side only
  - `FUN_00684100`
    - into `block_kind 1`, mode `0`
    - uses the repacker side only
  - `FUN_006848D0`
    - into lower-family `(0,1)`
    - uses the repacker side only
  - `FUN_006803F0`
    - into `block_kind 3`, mode `0`
    - uses the full front door

Stage behavior:

- still belongs to the intermediate edit/repack plane
- but acts as an adapter at the edge of that plane

Best current use:

- identify places where the engine normalizes a nonstandard source representation into the main intermediate-family toolkit
- the current sampled source-bridge band now includes two practical branches:
  - full front-door source bridge
    - current example:
      - `FUN_006803F0`
  - repacker-only source adapter
    - current examples:
      - `FUN_0067DC00`
      - `FUN_0067E2F0`
      - `FUN_0067E9C0`
      - `FUN_0067F090`
      - `FUN_0067F7D0`
      - `FUN_00680140`
      - `FUN_00684100`
      - `FUN_006848D0`
- the repacker-only branch is now strong enough to classify by:
  - feeder class
  - target family
  - local policy variant
- lower-family repacker targets now appear to split into:
  - opaque-style family `1`
  - threshold-`0` lower family `0`
- `FUN_006803F0(...)` still appears to reuse a neighborhood-style pointer-walk body, with the main change on the source feeder side
- `FUN_00680140(...)` is a useful edge case because it skips `FUN_006A5F10(...)`, builds the working block locally from `ushort` texels, and enters through `FUN_006A5CB0(..., 3, 0)` only

## Transform Bridge Worker

Best current description:

- expands a standard intermediate family
- applies a non-neighborhood transform across the full 16-pixel working block
- repacks back into the same family

Representative function:

- `FUN_0068C310`

Typical source-side input shape:

- compact family-`3` block stream plus external coefficient data

Family behavior:

- stays in the same intermediate family
- current known case:
  - `block_kind 3`, mode `2`

Stage behavior:

- still belongs to the intermediate edit/repack plane
- but behaves more like a transform operator than a neighborhood editor

Best current use:

- identify color-space/channel-remap or policy-driven transforms that reuse the intermediate-family seam without changing stage
- current known transform bridge `FUN_0068C310(...)` does not appear to reuse the ordinary neighborhood skeletons; it uses a distinct whole-block transform loop

## Final Materializer

Best current description:

- consumes compact upper-family payloads
- does not route back through the shared family front door
- writes final destination pixels directly

Representative functions:

- `FUN_00681860`
- `FUN_00681ED0`
- `FUN_006825A0`
- `FUN_00682980`
- `FUN_006833F0`

Typical source-side input shape:

- upper DXT-derived compact payloads

Family behavior:

- no intermediate `block_kind` classification exposed at this level
- instead the family split is output-oriented:
  - scalar-plus-color
  - alpha-only
  - scalar-weighted color
  - color-only

Stage behavior:

- belongs to the final `0x18` materialization plane

Best current use:

- identify where the engine stops editing compact intermediate block families and starts producing actual destination pixel layouts

## Quick Crosswalk By Subtype

| Subtype | Keeps Family? | Can Bridge Source Shape? | Uses `FUN_006A5F10/5CB0`? | Writes Final Pixels Directly? |
| --- | --- | --- | --- | --- |
| neighborhood edit worker | yes | no | yes | no |
| source bridge worker | bridges into a family | yes | `FUN_006A5CB0` yes, `FUN_006A5F10` optional | no |
| transform bridge worker | yes | no, but consumes extra transform data | yes | no |
| final materializer | n/a | n/a | no | yes |

## Practical Reading Order

For future reversing, the easiest classification order now looks like:

1. does the function call `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`?
2. if no:
   - check whether it still calls `FUN_006A5CB0(...)`
   - if yes, compare it against the repacker-only adapter band first, not just `FUN_00680140(...)`
   - if no, it is more likely output plane or another non-front-door subsystem
3. if yes:
   - is it neighborhood-shaped, source-bridging, or transform-bridging?
4. then classify:
   - `block_kind`
   - mode
   - exact source-side shape

That is usually a faster route than trying to infer format identity from constants first.

## Best Next Step

The subtype layer is now strong enough that the next best reverse task is no longer broad subtype discovery.

It is testing whether the same repacker-only architecture continues outside the currently mapped band or narrowing the remaining semantic labels under:

- intermediate modes `0/1/2/3`
- how broad the threshold-`0` lower-family lane is beyond `FUN_006848D0`
- tighter semantics for the lower-family `0` vs `1` split under the shared color-table emitter
