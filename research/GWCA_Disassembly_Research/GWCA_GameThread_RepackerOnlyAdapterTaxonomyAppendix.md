# GWCA GameThread Repacker-Only Adapter Taxonomy Appendix

This appendix compresses the current repacker-only adapter work into one compact taxonomy.

It is meant to unify the results from:

- [GWCA_GameThread_RepackerOnlyAdapterCluster_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_RepackerOnlyAdapterCluster_Addendum.md)
- [GWCA_GameThread_RepackerOnlyFeederShapes_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_RepackerOnlyFeederShapes_Addendum.md)
- [GWCA_GameThread_0067E9C0_vs_0067F090_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_0067E9C0_vs_0067F090_Addendum.md)
- [GWCA_GameThread_0067DC00_vs_0067E2F0_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_0067DC00_vs_0067E2F0_Addendum.md)

## Main Result

The repacker-only side of the transfer bank is now strong enough to read as its own small taxonomy.

All currently sampled members:

- call `FUN_006A5CB0(...)`
- do **not** call `FUN_006A5F10(...)`
- build a local `16`-entry working block first
- join the shared intermediate-family seam from the repacker side only

So the cleanest current architecture is:

- full front-door intermediate workers
- repacker-only adapters
- direct materializers

not one single flat middle tier.

## Adapter Table

| Worker | Feeder Class | Target Family | Local Policy Variant | Best Current Short Name |
| --- | --- | --- | --- | --- |
| `FUN_0067DC00` | paired family-`2`-like compact records | `(2,0)` | raised / max-clamped scalar lane | family-`2` saturating repacker adapter |
| `FUN_0067E2F0` | paired family-`2`-like compact records | `(2,0)` | resident-scalar-preserving lane | family-`2` preserving repacker adapter |
| `FUN_0067E9C0` | richer `uint`-word compact feeder into opaque family `1` | `(1,0)` | source-weighted one-sided blend | family-`1` one-sided repacker adapter |
| `FUN_0067F090` | richer `uint`-word compact feeder into opaque family `1` | `(1,0)` | two-sided selector-weighted crossblend | family-`1` two-sided repacker adapter |
| `FUN_0067F7D0` | byte-packed alpha/color feeder into opaque family `1` | `(1,0)` | byte-packed scalar-plus-color bridge | family-`1` byte-packed repacker adapter |
| `FUN_00680140` | raw `ushort` texels | `(3,0)` | direct texel-to-working-block bridge | family-`3` texel repacker adapter |
| `FUN_00684100` | single compact family-`1`-like feeder | `(1,0)` | one-input canonicalizer / normalizer | family-`1` canonicalizing repacker adapter |
| `FUN_006848D0` | byte-packed scalar-plus-color feeder | `(0,1)` | one-input canonicalizing bridge into the threshold-`0` lower color-table lane | family-`0` byte-packed repacker adapter |

## Feeder Classes

### Paired Family-`2`-Like Compact Records

Functions:

- `FUN_0067DC00`
- `FUN_0067E2F0`

Reading:

- source and resident sides both look like compact family-`2`-like records
- both sides contribute color endpoints, selector state, and scalar lane data
- the sibling split lives in the scalar merge policy, not the feeder class

### Richer `uint`-Word Compact Feeder Into Opaque Family `1`

Functions:

- `FUN_0067E9C0`
- `FUN_0067F090`

Reading:

- source side is still a `uint`-word compact block
- resident side already looks like an opaque family-`1` block
- the sibling split lives in one-sided vs two-sided blend policy

### Byte-Packed Alpha/Color Feeder

Function:

- `FUN_0067F7D0`

Reading:

- source payload is byte-packed rather than plain `uint` words
- the function still targets family `(1,0)`
- this looks like a distinct feeder class, not just another policy variant of the `0067E9C0 / 0067F090` pair
- that same broad feeder class now also appears at:
  - `FUN_006848D0`
  - but with a different repack target

### Raw `ushort` Texel Feeder

Function:

- `FUN_00680140`

Reading:

- this is the furthest source-normalization case currently known
- it skips the compact-record feeder stage entirely
- and bridges directly into family `(3,0)`

### Single Compact Family-`1`-Like Feeder

Function:

- `FUN_00684100`

Reading:

- this function rebuilds local working entries from one compact feeder rather than merging source vs resident blocks
- it still repacks through `FUN_006A5CB0(..., 1, 0)`
- so it looks like a one-input canonicalizer inside the repacker-only branch

## Policy Variants

The repacker-only band is not organized by feeder class alone.

It also splits by local policy variant.

### Family `(2,0)` Policy Split

- `FUN_0067DC00`
  - source can raise the final scalar nibble up to the stronger of source/resident
- `FUN_0067E2F0`
  - source influences color blend strength
  - but the final scalar lane stays resident-driven

### Family `(1,0)` Policy Split

- `FUN_0067E9C0`
  - source-weighted one-sided blend into opaque family `1`
- `FUN_0067F090`
  - two-sided selector-weighted crossblend into opaque family `1`
- `FUN_0067F7D0`
  - distinct byte-packed feeder path rather than just another policy sibling of those two
- `FUN_00684100`
  - single-input canonicalizer / normalizer into opaque family `1`
- `FUN_006848D0`
  - one-input canonicalizing bridge into lower family `(0,1)`
  - current best reading:
    - threshold-`0` lower color-table lane rather than a new helper-backed family

So the current adapter-side model is:

- feeder class
- target family
- local policy variant

not just:

- feeder class
- target family

## Relationship To The Main Subtype Model

These still sit under the broad `source bridge worker` subtype.

But the subtype now separates cleanly into two operational branches:

- full front-door source bridge
  - example:
    - `FUN_006803F0`
- repacker-only source adapter
  - examples:
    - `FUN_0067DC00`
    - `FUN_0067E2F0`
    - `FUN_0067E9C0`
    - `FUN_0067F090`
    - `FUN_0067F7D0`
    - `FUN_00680140`
    - `FUN_00684100`
    - `FUN_006848D0`

That is a useful refinement because it explains why these workers feel related to the front-door cluster while still failing the normal `FUN_006A5F10(...)` check.

## Practical Triage Rule

For a new uncategorized worker near this band:

1. if it calls `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`
   - treat it as a full front-door worker
2. if it skips `FUN_006A5F10(...)` but calls `FUN_006A5CB0(...)`
   - treat it as a repacker-only adapter
3. then classify:
   - feeder class
   - target family
   - local policy variant

That is currently the fastest stable classification route.

## Best Next Step

The next strongest reverse step is now to test how broad the lower family-`0` lane really is:

- `FUN_006848D0`
  - `FUN_006A5CB0(..., 0, 1)`

That should show whether this threshold-`0` lower lane is a small special case or a broader mask/transparency branch under the shared lower color-table emitter.

## Supporting Artifacts

- `tools/ghidra_projects/gw_findcallers_006a5cb0_temp175.log`
- `tools/ghidra_projects/gw_decomp_repacker_only_cluster_temp177.log`
- `tools/ghidra_projects/gw_decomp_00680140_00680e20_temp176.log`
