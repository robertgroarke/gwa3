# GWCA GameThread Transfer Variant Matrix Appendix

This appendix consolidates the installed transfer-registry triples into one readable variant matrix.

The key question was:

- what does the third selector field actually do in the transfer-bank installs?

The strongest current answer is:

- for the symmetric `(fmt, fmt, variant)` tier, the third field is a real worker-variant selector inside one format family
- but the registry is not single-tiered, so some formats also expose heavy workers through asymmetric registration planes

That means the selector is real, but it must be read together with the whole key triple rather than in isolation.

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_StaticInstalledCallbackMatrixAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_StaticInstalledCallbackMatrixAppendix.md)
- [GWCA_GameThread_TransferBankKernelSlices_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBankKernelSlices_Addendum.md)
- [GWCA_GameThread_TransferBankRoleSplit_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBankRoleSplit_Addendum.md)
- [GWCA_GameThread_TransferVariantCrosswalk_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferVariantCrosswalk_Addendum.md)
- [GWCA_GameThread_AsymmetricTransferTier_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_AsymmetricTransferTier_Addendum.md)

## Symmetric Tier Matrix

The currently recovered symmetric installs are:

| triple | bank slice | target | current role |
| --- | --- | --- | --- |
| `(0x11, 0x11, 0x1)` | `0x00A26C14` | `FUN_006743A0` | full compressed-family worker |
| `(0x11, 0x11, 0x3)` | `0x00A26C18` | `FUN_00674B20` | sibling full worker |
| `(0x12, 0x12, 0x1)` | `0x00A26C1C` | `FUN_00675260` | full compressed-family worker |
| `(0x12, 0x12, 0x3)` | `0x00A26C20` | `FUN_00675E80` | sibling full worker |
| `(0x13, 0x13, 0x1)` | `0x00A26C24` | `FUN_00676A40` | full compressed-family worker |
| `(0x13, 0x13, 0x3)` | `0x00A26C28` | `FUN_006775A0` | sibling full worker |
| `(0x13, 0x13, 0x5)` | `0x00A26C2C` | `FUN_006780B0` | third sibling full worker |
| `(0x14, 0x14, 0x1)` | `0x00A26C30` | `FUN_00678A70` | support-layer size helper |
| `(0x15, 0x15, 0x1)` | `0x00A26C34` | `FUN_00678AE0` | support-layer size helper |

## What the Third Selector Means

The best current symmetric-tier reading is:

### `0x11`

- `variant = 1`
  - `FUN_006743A0`
- `variant = 3`
  - `FUN_00674B20`

Both are heavy RGB565/interpolation workers ending through `FUN_006A5CB0(..., 2, 0)`.
So `1` and `3` are real sibling worker variants here.

### `0x12`

- `variant = 1`
  - `FUN_00675260`
- `variant = 3`
  - `FUN_00675E80`

Both are again full compressed-family workers.
So `1` and `3` are real sibling worker variants inside the `0x12` family too.

### `0x13`

- `variant = 1`
  - `FUN_00676A40`
- `variant = 3`
  - `FUN_006775A0`
- `variant = 5`
  - `FUN_006780B0`

This is the clearest proof that the third selector is a genuine worker-variant axis.
`0x13` has at least three heavy siblings in the same family.

### `0x14` and `0x15`

At this symmetric bank position:

- `0x14, variant = 1`
  - lands on `FUN_00678A70`
- `0x15, variant = 1`
  - lands on `FUN_00678AE0`

These are support-layer size helpers, not the heavy worker bodies seen for `0x11 / 0x12 / 0x13`.

So the right conclusion is not:

- the selector stops mattering

It is:

- the symmetric tier itself is not flat across all formats

## Why `0x14` / `0x15` Are Not Missing Families

The asymmetric tiers matter here.

### Asymmetric tier A: `(fmt, 0, 1)`

Recovered installs:

| triple | bank slice | target |
| --- | --- | --- |
| `(0x12, 0x0, 0x1)` | `0x00A26C74` | `FUN_0067AB50` |
| `(0x13, 0x0, 0x1)` | `0x00A26C78` | `FUN_0067AFF0` |
| `(0x14, 0x0, 0x1)` | `0x00A26C7C` | `FUN_0067B490` |
| `(0x15, 0x0, 0x1)` | `0x00A26C80` | `FUN_0067B930` |

These are substantial worker registrations, not support helpers.

So `0x14` and `0x15` do have heavy-worker registrations.
They simply do not show up as heavy workers in the same symmetric window where `0x12` and `0x13` do.

### Asymmetric tier B: `(0x18, fmt, 0)`

Recovered installs:

| triple | bank slice | target |
| --- | --- | --- |
| `(0x18, 0x12, 0x0)` | `0x00A26CCC` | `FUN_00681860` |
| `(0x18, 0x13, 0x0)` | `0x00A26CD0` | `FUN_00681ED0` |
| `(0x18, 0x14, 0x0)` | `0x00A26CD4` | `FUN_006825A0` |
| `(0x18, 0x15, 0x0)` | `0x00A26CD8` | `FUN_00682980` |
| `(0x18, 0x16, 0x0)` | `0x00A26CDC` | `FUN_006833F0` |

This is a second strong sign that the registry key is choosing both:

- format family
- and stage/tool layer

not just one flat family id.

## Best Current Matrix Reading

The cleanest current model is:

- symmetric `(fmt, fmt, variant)` tier
  - where `variant = 1 / 3 / 5` is a real worker-variant selector
  - especially clear for `0x11 / 0x12 / 0x13`
- asymmetric `(fmt, 0, 1)` tier
  - one-sided heavy-worker registrations
- asymmetric `(0x18, fmt, 0)` tier
  - later staged registrations into another method slice

So the third selector field is meaningful, but the whole triple must be read as:

- family / stage key
- family / stage key
- local variant selector

rather than assuming the third field alone explains everything.

## Practical Classifier

When a new installed transfer triple appears, the fastest current classifier is:

1. if it is `(fmt, fmt, variant)`
   - treat it as the symmetric family tier
2. if `fmt` is `0x11 / 0x12 / 0x13`
   - expect `variant` to map onto real heavy sibling workers
3. if `fmt` is `0x14 / 0x15` in the symmetric tier
   - do not assume support-only globally; check the asymmetric tiers too
4. if it is `(fmt, 0, 1)`
   - treat it as the one-sided heavy-worker tier
5. if it is `(0x18, fmt, 0)`
   - treat it as the later stage-keyed tier

## Best Next Step

The strongest next reverse step is to tighten the meaning of the heavy sibling variants inside one family, and the best family for that is still:

- `0x13`

because it already exposes the clearest three-way split:

- `variant = 1` -> `FUN_00676A40`
- `variant = 3` -> `FUN_006775A0`
- `variant = 5` -> `FUN_006780B0`

That should let the next pass say what `1 / 3 / 5` mean concretely in terms of:

- packing mode
- companion-plane use
- or another per-family materialization choice.
