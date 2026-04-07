# GWCA GameThread Asymmetric Transfer Tier Addendum

This pass widens the transfer crosswalk around the later one-sided and non-symmetric installer thunks.

## Main result

The later static installs do reveal another heavy-worker tier for `0x12..0x15`.

So the earlier result needs one refinement:

- `0x14` and `0x15` do land on support-layer entries in the symmetric `(fmt, fmt, variant)` bank window
- but they are **not** limited to support slices overall
- they also participate in a second, asymmetric registration tier that points at another family of heavy worker methods

That means the transfer registry really is multi-tiered.

## First asymmetric tier: `(fmt, 0, 1)`

The installer thunks around `0x0045CA20` show a one-sided family:

- `0x0045CA20`: pushes `1, 0, 0x12` then patches `-> 0x00A26C74`
- `0x0045CA50`: pushes `1, 0, 0x13` then patches `-> 0x00A26C78`
- `0x0045CA80`: pushes `1, 0, 0x14` then patches `-> 0x00A26C7C`
- `0x0045CAB0`: pushes `1, 0, 0x15` then patches `-> 0x00A26C80`

Using the same argument interpretation as earlier, these are best read as:

- `(0x12, 0x0, 0x1) -> 0x00A26C74 -> FUN_0067AB50`
- `(0x13, 0x0, 0x1) -> 0x00A26C78 -> FUN_0067AFF0`
- `(0x14, 0x0, 0x1) -> 0x00A26C7C -> FUN_0067B490`
- `(0x15, 0x0, 0x1) -> 0x00A26C80 -> FUN_0067B930`

That is the first strong confirmation that `0x14` and `0x15` do have heavy worker registrations outside the earlier symmetric window.

## What the new heavy-worker tier looks like

The first functions in this asymmetric tier are not tiny support helpers. They are again substantial block/image workers.

### `FUN_0067A6B0` / `FUN_0067AB50`

These bodies are especially helpful because they expose a new pattern:

- they call `FUN_006A5F10(...)` to materialize a local block neighborhood
- they blend/accumulate values through `DAT_00A283D8`
- they finish through `FUN_006A5CB0(...)`

That makes them look like neighborhood-based post-materialization or cross-block worker families, not the earlier direct per-record reconstructor bodies.

The visible specialization split is:

- `FUN_0067A6B0` uses `FUN_006A5F10(..., 2, 0)` and `FUN_006A5CB0(..., 2, 0)`
- `FUN_0067AB50` uses `FUN_006A5F10(..., 3, 1)` and `FUN_006A5CB0(..., 3, 1)`

So even inside this asymmetric tier, the bank is still selecting concrete sibling worker modes.

## Second asymmetric tier: `(0x18, fmt, 0)`

A later thunk run around `0x0045CE40` shows another non-symmetric family:

- `0x0045CE40`: pushes `0, 0x12, 0x18` then patches `-> 0x00A26CCC`
- `0x0045CE70`: pushes `0, 0x13, 0x18` then patches `-> 0x00A26CD0`
- `0x0045CEA0`: pushes `0, 0x14, 0x18` then patches `-> 0x00A26CD4`
- `0x0045CED0`: pushes `0, 0x15, 0x18` then patches `-> 0x00A26CD8`
- `0x0045CF00`: pushes `0, 0x16, 0x18` then patches `-> 0x00A26CDC`

These are best read as another family keyed on first field `0x18`, with the varying second field selecting format family:

- `(0x18, 0x12, 0x0) -> 0x00A26CCC -> FUN_00681860`
- `(0x18, 0x13, 0x0) -> 0x00A26CD0 -> FUN_00681ED0`
- `(0x18, 0x14, 0x0) -> 0x00A26CD4 -> FUN_006825A0`
- `(0x18, 0x15, 0x0) -> 0x00A26CD8 -> FUN_00682980`
- `(0x18, 0x16, 0x0) -> 0x00A26CDC -> FUN_006833F0`

I have not fully decompiled that later slice yet, but the registry shape is now clear enough to name it as a second asymmetric tier.

## What changed in the model

Before this pass, the best reading was:

- symmetric `(fmt, fmt, variant)` bank:
  - `0x12`, `0x13` -> heavy workers
  - `0x14`, `0x15` -> support slices

After this pass, the better model is:

- symmetric tier:
  - mostly family-local direct workers and support slices
- asymmetric tier A:
  - one-sided `(fmt, 0, 1)` heavy worker registrations
- asymmetric tier B:
  - `(0x18, fmt, 0)` registrations into a further method slice

So `0x14` and `0x15` are no longer outliers in the strong sense. They looked incomplete only because we were reading one tier in isolation.

## Best current interpretation

The transfer registry is best understood now as a staged family toolkit with multiple registration planes:

- direct symmetric format-family registrations
- one-sided format-family registrations
- later stage-keyed registrations with `0x18` as the leading selector

That explains why the same format ids sometimes land on heavy workers and sometimes on support slices: the registry key is selecting both the format family and the stage/tool layer.

## Best next step

The next highest-value move is to decompile the first few `(0x18, fmt, 0)` targets:

- `FUN_00681860`
- `FUN_00681ED0`
- `FUN_006825A0`
- `FUN_00682980`

That should tell us what stage `0x18` actually represents and whether it is a post-filter, neighborhood expansion, flip, or another transfer phase layered above the earlier family kernels.
