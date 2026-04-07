# GWCA GameThread 013 Variant Tail Patterns Addendum

This addendum closes the most immediate open question around the symmetric `0x13` transfer-family workers:

- `(0x13, 0x13, 0x1) -> FUN_00676A40`
- `(0x13, 0x13, 0x3) -> FUN_006775A0`
- `(0x13, 0x13, 0x5) -> FUN_006780B0`

The new result is simple but useful:

- all three workers tail through `FUN_006A5CB0(..., 3, 0)`

So the `1 / 3 / 5` split is not a visible repacker-mode split inside the symmetric `0x13` tier.
The meaningful differences sit earlier, in how each worker reconstructs its local block before the shared `(3,0)` materializer.

This was a fresh side-by-side decompilation read over the existing transfer-bank logs.

## Source Artifacts

- [gw_decomp_transfer_bank_tail_temp161.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_transfer_bank_tail_temp161.log)
- [gw_decomp_transfer_bank_temp160.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_transfer_bank_temp160.log)
- [GWCA_GameThread_013VariantSemanticsAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_013VariantSemanticsAppendix.md)
- [GWCA_GameThread_013VariantBoundaryAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_013VariantBoundaryAppendix.md)

## Tail Attribution

The exact symmetric-`0x13` tails are now:

| triple | worker | tail call |
| --- | --- | --- |
| `(0x13, 0x13, 0x1)` | `FUN_00676A40` | `FUN_006A5CB0(local_18, &local_a4, local_58, 3, 0)` |
| `(0x13, 0x13, 0x3)` | `FUN_006775A0` | `FUN_006A5CB0(local_18, &local_a0, local_58, 3, 0)` |
| `(0x13, 0x13, 0x5)` | `FUN_006780B0` | `FUN_006A5CB0(local_58, &local_104, local_48, 3, 0)` |

So the earlier visible `FUN_006A5CB0(..., 3, 1)` path in the same log was not one of these three workers.
That `(..., 3, 1)` path belongs to the preceding `FUN_00675E80` block instead.

## What Stays Shared

All three siblings still look like true heavy middle-bank workers:

- same broad transfer-family layer
- same RGB565 / interpolation-table machinery
- same local `16`-entry working-block rebuild
- same final `(3,0)` repacker family

That is a stronger closure than the earlier cautious read because it removes the easiest remaining false split.

## What Actually Differs

The variant axis now reads more locally:

### `FUN_00676A40`

This is still the cleanest baseline sibling.

Its reconstruction path uses two interpolated source arrays before the shared `(3,0)` tail:

- `local_e4[4]`
- `local_104[4]`

and then merges those through paired lookup tables into `local_58[16]`.

### `FUN_006775A0`

This stays on the same final tail as `FUN_00676A40(...)`, but the setup is already narrower and more asymmetric.

The visible local shape is:

- one explicit interpolated array `local_100[4]`
- companion lookup tables `local_c0[4]` and `local_b0[4]`
- rebuilt output in `local_58[16]`

So `variant = 3` still looks like a sibling local reconstruction policy, not a new materializer mode.

### `FUN_006780B0`

This remains the structurally distinct sibling, but the distinction is now clearly pre-pack.

The clearest differences are:

- it builds an expanded `local_d0[9]` scalar ladder rather than a small four-entry interpolant
- it writes directly into a compact `local_48[4]` block form before the same `(3,0)` materializer
- it still uses the same broad table family, but with a more specialized per-cell reconstruction loop

So `variant = 5` is not a different tail family.
It is the most transformed local block-builder feeding the same tail family.

## Updated Reading Of `1 / 3 / 5`

The strongest current interpretation is now:

- `1 / 3 / 5` do not select different `FUN_006A5CB0(...)` modes
- `1 / 3 / 5` do not separate one-sided vs direct-output tiers
- `1 / 3 / 5` select sibling local reconstruction/materialization policies inside one symmetric `0x13` heavy-worker family

That is a tighter and safer claim than the earlier wording about possible tail-argument differences.

## Practical Classifier

For symmetric `0x13` workers:

1. if the install triple is `(0x13, 0x13, 0x1)`
   - treat it as the baseline two-interpolant local block builder into shared tail `(3,0)`
2. if the install triple is `(0x13, 0x13, 0x3)`
   - treat it as the narrower asymmetric local block builder into shared tail `(3,0)`
3. if the install triple is `(0x13, 0x13, 0x5)`
   - treat it as the expanded-ladder local block builder into shared tail `(3,0)`
4. do not use tail mode itself to distinguish these three

## Best Next Step

The strongest next reverse step is to compare one adjacent sibling family with the same method:

- `FUN_006743A0(...)` vs `FUN_00674B20(...)`
- or `FUN_00675260(...)` vs `FUN_00675E80(...)`

That would tell us whether this new rule generalizes:

- symmetric-family selector variants mostly keep the same `FUN_006A5CB0(...)` tail
- and the real split lives in the pre-pack local reconstruction policy
