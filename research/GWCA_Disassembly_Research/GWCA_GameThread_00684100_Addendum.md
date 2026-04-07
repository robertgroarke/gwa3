# GWCA GameThread 00684100 Addendum

This pass tests the first non-local repacker-only caller outside the mapped `0x0067DC00 .. 0x00680140` band:

- `FUN_00684100`

The goal was simple:

- does the repacker-only architecture continue outside the local adapter pocket,
- or was that earlier band only a one-region special case?

## Main Result

`FUN_00684100(...)` confirms the architecture continues.

It:

- does **not** call `FUN_006A5F10(...)`
- builds local working entries directly
- calls:
  - `FUN_006A5CB0(&local_18, &local_90, local_58, 1, 0)`

So this is another **repacker-only adapter**, and specifically another family-`1`, mode-`0` member.

That means the repacker-only branch is not confined to the earlier `0067DC00 .. 00680140` band.

## Structural Shape

`FUN_00684100(...)` differs from the earlier family-`1` repacker-only workers in an important way.

It is not a two-input merge worker like:

- `FUN_0067E9C0`
- `FUN_0067F090`

And it is not a byte-packed alpha/color bridge like:

- `FUN_0067F7D0`

Instead it reads a single compact feeder record and reconstructs `16` local working entries directly.

The main visible pattern is:

- decode one `5:6:5` endpoint pair into `local_78[4]`
- combine selector/color and scalar nibble lanes into:
  - `local_58[16]`
- repack through:
  - `FUN_006A5CB0(..., 1, 0)`
- require exactly `2` output dwords

So the cleanest current reading is:

- family-`1`, mode-`0`
- repacker-only
- single-input canonicalizer / normalizer

not:

- source-vs-resident merge worker

## Why This Matters

This is the first strong proof that the repacker-only taxonomy is broader than the earlier dense pocket.

The current branch now looks like:

- local repacker-only adapter band near `0067DC00 .. 00680140`
- at least one non-local repacker-only family-`1` canonicalizer at `00684100`

That makes the repacker-only branch feel more like a reusable stage family and less like one isolated caller cluster.

## Best Current Placement

The strongest current placement for `FUN_00684100(...)` is:

- subtype:
  - source bridge worker
- sub-branch:
  - repacker-only adapter
- target family:
  - `(1,0)`
- local role:
  - single-input canonicalizer / normalizer

That adds a useful new local policy class beyond the earlier merge-style family-`1` workers.

## Practical Takeaway

The repacker-only branch now appears to contain at least two broad local shapes:

- merge/adaptation workers
- canonicalizer/normalizer workers

So for future callers, "skips `FUN_006A5F10(...)` but calls `FUN_006A5CB0(...)`" is only the first step.

After that, it is now worth asking:

1. is this a merge worker?
2. or a one-input canonicalizer?

## Best Next Step

The strongest immediate follow-up is:

- decompile `FUN_006848D0`

That should tell us whether `FUN_00684100` has a nearby sibling canonicalizer, or whether `006848D0` opens yet another repacker-only branch shape.

## Supporting Artifacts

- `tools/ghidra_projects/gw_findcallers_006a5cb0_temp175.log`
- `tools/ghidra_projects/gw_decomp_00684100_temp178.log`
