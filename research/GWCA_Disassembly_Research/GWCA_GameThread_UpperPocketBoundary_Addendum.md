# GWCA GameThread Upper Pocket Boundary Addendum

This pass closes the small unresolved pocket around:

- `FUN_00680140`
- `FUN_006803F0`
- `FUN_006809A0`
- `FUN_00680A70`
- `FUN_00680E20`

The goal was to check whether this upper `0x00680***` cluster still hides any additional full front-door workers beyond the already known outliers.

## Main Result

The upper pocket now separates cleanly into three roles:

- `FUN_006803F0` remains the only full front-door caller in this local pocket
- `FUN_00680140` is a **repacker-only source bridge**
- `FUN_006809A0`, `FUN_00680A70`, and `FUN_00680E20` are **direct materializers**

So the current best boundary is:

- full front-door caller cluster still ends at the known `16` workers
- the adjacent `0x00680***` pocket does contain related transfer-bank neighbors
- but those neighbors do not introduce another hidden `FUN_006A5F10(...)` caller family

## Caller Boundary

Fresh caller search against `FUN_006A5F10(...)` still returns exactly these `16` workers:

- `FUN_00679D70`
- `FUN_0067A210`
- `FUN_0067A6B0`
- `FUN_0067AB50`
- `FUN_0067AFF0`
- `FUN_0067B490`
- `FUN_0067B930`
- `FUN_0067BDD0`
- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067CAC0`
- `FUN_0067CF10`
- `FUN_0067D360`
- `FUN_0067D7B0`
- `FUN_006803F0`
- `FUN_0068C310`

The nearby entries:

- `FUN_00680140`
- `FUN_006809A0`
- `FUN_00680A70`
- `FUN_00680E20`

do **not** appear in that caller set.

That matters because it means the upper pocket is adjacent to the front-door cluster, but does not expand it.

## `FUN_00680140`: Repacker-Only Source Bridge

`FUN_00680140(...)` does **not** call `FUN_006A5F10(...)`.

Instead it:

- reads `ushort` source texels
- expands them through `DAT_00A2C588`
- builds a local `16`-entry working block in `local_68[16]`
- then calls:
  - `FUN_006A5CB0(local_28, &local_84, local_68, 3, 0)`

So the cleanest reading is:

- still a **source bridge worker**
- still bridging into family `(3,0)`
- but entering the shared seam from the **repacker side only**

This makes it a useful edge case for the subtype model:

- `FUN_006803F0`
  - full front-door source bridge
- `FUN_00680140`
  - repacker-only source bridge

Both normalize 16-bit source texels into family `(3,0)`, but they join the shared toolkit at different depths.

## `FUN_006809A0`: Indexed Direct Materializer

`FUN_006809A0(...)`:

- copies a `256`-entry color table into a local lookup
- byte-swaps each entry into output order
- then writes final pixels directly from indexed source bytes

It does not call:

- `FUN_006A5F10(...)`
- `FUN_006A5CB0(...)`

So this is best treated as a direct materializer/output worker, not an intermediate-family worker.

## `FUN_00680A70`: Color-Block Direct Materializer

`FUN_00680A70(...)`:

- reads two `ushort` color endpoints
- expands them through the `5:6:5` lookup tables
- builds the usual four-color local palette
- writes destination pixels directly from the selector bits

Again, it does not route through the shared family front door.

So this is another direct materializer neighbor in the upper pocket.

## `FUN_00680E20`: Scalar-Plus-Color Direct Materializer

`FUN_00680E20(...)`:

- reads a color block plus scalar payload
- reconstructs local color candidates
- combines selector and scalar state per pixel
- writes final destination pixels directly

It also does not call the shared front-door helpers.

So this one belongs with the direct output/materialization side, not the intermediate edit/repack side.

## Best Current Boundary Statement

The upper pocket now reads as:

- `FUN_00680140`
  - repacker-only source bridge into `(3,0)`
- `FUN_006803F0`
  - full front-door source bridge into `(3,0)`
- `FUN_006809A0`
  - indexed direct materializer
- `FUN_00680A70`
  - color-block direct materializer
- `FUN_00680E20`
  - scalar-plus-color direct materializer

So the strongest current interpretation is:

- the front-door cluster itself is probably already closed
- but the nearby upper pocket still contains adapters and output neighbors that sit beside that cluster

## Practical Takeaway

This gives a much better quick read for future reverse passes:

1. if a new upper-pocket worker calls `FUN_006A5F10(...)`, it is a real front-door cluster member
2. if it skips `FUN_006A5F10(...)` but still calls `FUN_006A5CB0(...)`, compare it against `FUN_00680140`
3. if it calls neither and writes final pixels directly, compare it against the `006809A0 / 00680A70 / 00680E20` output side

That should prevent future outliers from getting mixed together just because they sit near each other in the bank.

## Supporting Artifacts

- `tools/ghidra_projects/gw_findcallers_006a5f10_temp174.log`
- `tools/ghidra_projects/gw_findcallers_006a5cb0_temp175.log`
- `tools/ghidra_projects/gw_listfuncs_00680000_00681000_temp175.log`
- `tools/ghidra_projects/gw_decomp_00680140_00680e20_temp176.log`
