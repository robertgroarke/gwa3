# GWCA GameThread Asymmetric Tier Vs Stage-18 Addendum

This pass compares the earlier one-sided transfer tier against the newly mapped `0x18` output tier.

The one-sided installs are:

- `(0x12, 0, 1) -> FUN_0067AB50`
- `(0x13, 0, 1) -> FUN_0067AFF0`
- `(0x14, 0, 1) -> FUN_0067B490`
- `(0x15, 0, 1) -> FUN_0067B930`

The `0x18` installs are:

- `(0x18, 0x12, 0) -> FUN_00681860`
- `(0x18, 0x13, 0) -> FUN_00681ED0`
- `(0x18, 0x14, 0) -> FUN_006825A0`
- `(0x18, 0x15, 0) -> FUN_00682980`
- `(0x18, 0x16, 0) -> FUN_006833F0`

## Main Result

The strongest conclusion from this pass is:

- the one-sided `(fmt, 0, 1)` tier does **not** look like a duplicate of the `0x18` output tier
- it looks like an earlier **neighborhood/reconstruction stage**
- the `0x18` tier still looks like the later **direct materialization/output stage**

So the two banks appear to be adjacent stages in the same transfer pipeline, not competing implementations of the same job.

## One-Sided Tier Structure

All four one-sided workers share the same overall skeleton.

They:

- read an existing destination neighborhood from `param_5`
- call `FUN_006A5F10(local_68, local_8c, 3, mode)`
- blend or rebuild four local neighborhood entries using `DAT_00A283D8`
- call `FUN_006A5CB0(local_28, &local_94, local_68, 3, mode)`
- and then write the produced compact results back to the output stream

The mode split is explicit:

- `FUN_0067AB50 -> FUN_006A5F10(..., 3, 1)` and `FUN_006A5CB0(..., 3, 1)`
- `FUN_0067AFF0 -> FUN_006A5F10(..., 3, 0)` and `FUN_006A5CB0(..., 3, 0)`
- `FUN_0067B490 -> FUN_006A5F10(..., 3, 3)` and `FUN_006A5CB0(..., 3, 3)`
- `FUN_0067B930 -> FUN_006A5F10(..., 3, 2)` and `FUN_006A5CB0(..., 3, 2)`

That is a much cleaner signal than the static installs alone: the one-sided tier is dispatching into a shared mode-based worker pair, not performing direct format-shaped pixel output itself.

## Why This Is Not The Same As Stage `0x18`

The difference from the `0x18` family is sharp.

The `0x18` workers:

- decode compressed-family block payloads directly,
- build per-block ladders and color entries locally,
- and write final destination pixels directly in format-shaped layouts

Examples:

- `FUN_00681860` / `FUN_00681ED0`: direct packed 32-bit output
- `FUN_006825A0`: alpha-only white-RGB output
- `FUN_00682980`: scalar-weighted color output
- `FUN_006833F0`: direct 24-bit color output

By contrast, the one-sided tier:

- operates around a local 4-entry neighborhood buffer,
- merges source and existing values through interpolation tables,
- and hands the real packing/emission job to `FUN_006A5F10` and `FUN_006A5CB0`

So its role is much closer to:

- pre-output reconstruction
- neighborhood-conditioned repacking
- or intermediate block-to-stage conversion

than to final destination materialization.

## Best Current Per-Format Split

The safest current interpretation is:

- `(0x12, 0, 1)` = one-sided mode-1 reconstruction stage
- `(0x13, 0, 1)` = one-sided mode-0 reconstruction stage
- `(0x14, 0, 1)` = one-sided mode-3 reconstruction stage
- `(0x15, 0, 1)` = one-sided mode-2 reconstruction stage

and then later:

- `(0x18, 0x12, 0)` = direct `DXT4`-family output
- `(0x18, 0x13, 0)` = direct `DXT5`-family output
- `(0x18, 0x14, 0)` = direct `DXTA` alpha-only output
- `(0x18, 0x15, 0)` = direct `DXTL` scalar-weighted output
- `(0x18, 0x16, 0)` = direct `DXTN` color-only output

That makes the selector dimensions look more meaningful:

- one selector chooses a format family
- another chooses a stage/tool plane
- and the one-sided tier plus `0x18` tier are different stages inside that plane system

## Architectural Meaning

This comparison is the strongest evidence so far that the transfer registry is a staged toolkit rather than a flat callback table.

At minimum, we now have:

1. symmetric format-family slices
2. one-sided mode-driven reconstruction slices
3. `0x18` direct output/materialization slices

So the installed callback record is likely selecting:

- not only **which format**
- but **which phase of handling that format**

## Next Step

The best next reverse step is to go one layer deeper into the shared mode workers:

- `FUN_006A5F10`
- `FUN_006A5CB0`

That should tell us exactly what the one-sided modes `0`, `1`, `2`, and `3` mean, and whether they correspond cleanly to:

- alpha-only
- alpha-plus-color
- luminance/intensity-weighted
- or another intermediate block arrangement

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_transfer_bank_tail2_temp164.log`
- `tools/ghidra_projects/gw_decomp_stage18_tier_temp165.log`
