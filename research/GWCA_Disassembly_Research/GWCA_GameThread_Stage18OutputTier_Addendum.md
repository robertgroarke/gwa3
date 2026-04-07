# GWCA GameThread Stage-18 Output Tier Addendum

This pass tightens the meaning of the third transfer registry plane keyed by leading selector `0x18`.

The newest static-install evidence shows:

- `(0x18, 0x12, 0) -> 0x00A26CCC -> FUN_00681860`
- `(0x18, 0x13, 0) -> 0x00A26CD0 -> FUN_00681ED0`
- `(0x18, 0x14, 0) -> 0x00A26CD4 -> FUN_006825A0`
- `(0x18, 0x15, 0) -> 0x00A26CD8 -> FUN_00682980`
- `(0x18, 0x16, 0) -> 0x00A26CDC -> FUN_006833F0`

So the `0x18` tier is not a stray helper layer or a metadata-only registry slice. It is a real installed method family with one concrete worker per upper compressed-format id.

## Main Result

The strongest upgrade from this pass is that the `0x18` tier looks like a **direct output/materialization tier**.

Unlike some earlier transfer-bank slices that:

- rebuild intermediate block state,
- choose compact families,
- compute support-layer sizing,
- or expose narrow 16-bit/materialization helpers,

the `0x18` handlers are heavy worker bodies that decode block data and write final packed pixel output directly.

That makes selector `0x18` look like a downstream output plane in the broader transfer toolkit, not just another format-family alias.

## Worker Shape

The clearest anchor is `FUN_00681860(...)`.

Its body:

- walks rows in 4-line groups,
- builds local scalar ladders in `local_6c[]`,
- expands RGB565 endpoints through `DAT_00A2C6C8` and `DAT_00A2C948`,
- derives intermediate color values into `local_4c[]`,
- consumes packed selector streams from the source payload,
- combines scalar/alpha and color contributions,
- and writes final 32-bit packed pixels directly to destination row pointers.

The final store form is explicit:

- `uVar6 >> 0x10 & 0xff | (uVar6 & 0xff) << 0x10 | uVar6 & 0xff00ff00`

So this is not merely reconstructing a compact block record. It is materializing final destination pixels.

`FUN_00681ED0(...)` is clearly a sibling in the same family:

- same signature,
- same overall row/block loop shape,
- same local ramp/table style,
- same direct packed-output behavior.

That makes the `(0x18, fmt, 0)` slice look like a family of format-specific block-to-pixel emitters.

## Relationship To Earlier Tiers

This pass also helps separate the three transfer-bank layers we now have:

1. symmetric `(fmt, fmt, variant)` installs
   - mixed heavy workers plus support/helper slices
2. asymmetric `(fmt, 0, 1)` installs
   - heavy neighborhood/post-materialization worker tier
3. stage-keyed `(0x18, fmt, 0)` installs
   - direct packed-output/materialization tier

So selector `0x18` now reads less like a format id and more like an explicit stage/tool-family selector inside the static registration framework.

## Current Interpretation

The best current model is:

- the transfer registry is multi-plane, not flat
- one selector dimension chooses a stage/tool family
- the `0x18` family is a late-stage output/materialization plane
- its installed methods take compressed-family block payloads and emit packed destination pixels directly

That is a stronger architectural result than just naming one more helper, because it explains why the registry needed a third selector plane at all.

## Next Step

The next best reverse step is to keep treating the `0x18` bank as a real family and compare it against the earlier asymmetric one-sided tier:

- `FUN_00681860`
- `FUN_00681ED0`
- `FUN_006825A0`
- `FUN_00682980`
- `FUN_006833F0`

The shortest path to a real crosswalk is to determine:

- which exact compressed-format families each worker corresponds to,
- whether `0x18` always means direct block-to-pixel output,
- and how this output tier composes with the earlier reconstruction/support tiers.

## Supporting Artifacts

- `tools/ghidra_projects/gw_dump_a26c74_temp163.log`
- `tools/ghidra_projects/gw_decomp_stage18_tier_temp165.log`
