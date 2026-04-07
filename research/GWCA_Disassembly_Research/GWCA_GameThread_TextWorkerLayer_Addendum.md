## `Gw.exe` Frame Callback Text Worker Layer Addendum

This pass followed the next concrete targets under the multiline-text control:

- `FUN_005EB4E0`
- `FUN_005EC600`
- `FUN_005ECEB0`
- `FUN_005ECC80`
- `FUN_005ECDC0`

The goal was to find out whether this cluster was:

- the missing raw region-record builder
- or a higher-level text/list materialization layer that sits on top of it

## Source artifacts

These results come from:

- [gw_decomp_text_workers_temp108.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_text_workers_temp108.log)
- [gw_findcallers_005eb4e0_temp109.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_005eb4e0_temp109.log)

## High-level result

This cluster is not the low-level region-child record writer.

It is a higher-level text/markup traversal and materialization layer:

- `FUN_005EB4E0(...)` is the main multiline text / markup walker and layout driver
- `FUN_005EC600(...)` materializes a concrete image-like child control from normalized geometry
- `FUN_005ECEB0(...)` materializes a repeated anchored child-control array from row-style output records
- `FUN_005ECC80(...)` and `FUN_005ECDC0(...)` are lighter dispatch wrappers into the same owner-local generation path

So this pass pushes the builder boundary down again:

- the worker cluster consumes row/layout output
- it does not look like the place where the raw region-table record array is originally authored

## `FUN_005EB4E0(...)`: multiline markup traversal and callback-driven materialization

This function is much larger than a simple iterator.

The strongest signals are:

- it copies a large style/context blob into `DAT_00BCFE88`
- it walks UTF-16 style text/tag input from `param_5`
- it recognizes markup tags beginning with `0x3C` (`'<'`)
- it parses tag names and attributes through helpers like:
  - `FUN_005C96A0(...)`
  - `FUN_005EA310(...)`
- it maintains running line/layout state in a large local frame
- it invokes two caller-supplied callbacks:
  - `param_8`
  - `param_9`

That makes it look like a generic:

- markup/text layout walker
- with callback-based emission of concrete results

### Callback roles inside `FUN_005EB4E0(...)`

The callback split is useful.

`param_8` is used on one branch where the code computes an explicit positioned rectangle from current text/layout state and then calls:

```cpp
(*local_1e8)(&DAT_00bcfe88, DAT_00bcfe20, &local_230, &local_1e0,
             &DAT_00bcfe24, &DAT_00bcfe34, &DAT_00bcfe3c, local_1cc);
```

That looks like a concrete positioned-entity emission path.

`param_9` is used on the ordinary text-run path after width/height and count-like values are computed:

```cpp
(*local_200)(&DAT_00bcfe88, &local_228, &local_178, &local_19c,
             &local_1c8, local_14c, local_1cc);
```

That looks like text-run or row/run materialization.

So the cleanest current model is:

- `FUN_005EB4E0(...)` = parser/layout driver
- worker callbacks = concrete result builders for different emitted content families

## Caller boundary for `FUN_005EB4E0(...)`

The caller search is small and informative.

Current refs are:

- `FUN_005ED590`
- `FUN_005EA9A0` at multiple sites

That is a strong sign this helper is a shared text/list rendering/materialization engine within the `CtlTextMl` family, not a generic region-table constructor used everywhere.

## `FUN_005EC600(...)`: image-like child materializer with normalized geometry

This helper is much more concrete than expected.

It:

- requires a live owner/control context at `param_8 + 0x18`
- creates a child via:
  - `FUN_0060D300(owner, 0, param_1[8] != 0, FUN_005F3550, 0, 0)`
- configures message surface with:
  - `FUN_00610000(...)`
  - optional `FUN_00610C70(...)`
- computes normalized ratios:
  - `param_7 / param_5`
  - `param_6 / param_5`
- packages those values into a small local block
- sends:
  - `FUN_00610160(child, 0x58, &local_28, 0)`
  - `FUN_00610860(child, param_3, param_4)`

So `FUN_005EC600(...)` is not building raw records.
It is taking already-derived geometry/content input and instantiating a concrete image/coordinate child control under `FUN_005F3550(...)`.

## `FUN_005ECEB0(...)`: anchored repeated-child materializer

This is the strongest worker in the set.

Its shape is:

- validate owner/context/input pointers
- prepare a generation descriptor block
- call `FUN_00610F60(...)`
- optionally receive a generated output array in `local_2c`
- if rows/records were produced, iterate them as `7 dword` records

For each emitted record it:

- creates a child:
  - `FUN_0060D300(owner, optional_0x2000, 2, FUN_005F8810, 0, 0)`
- allocates a `0x104` anchor object:
  - `FUN_0046CA10(0x104, s___AUAnchorParam_CtlTextMl___00bc19c4)`
- stores that anchor object into an owner-managed pointer array at:
  - `param_7 + 0x150`
  - `param_7 + 0x154`
  - `param_7 + 0x158`
  - `param_7 + 0x15C`
- copies style/context from `param_1`
- configures the new child with:
  - `FUN_00610C70(...)`
  - `FUN_005F9400(...)`
  - `FUN_0059A110(...)`
  - `FUN_005EB280(...)`
  - `FUN_005F93A0(...)`
  - `FUN_006108E0(...)`

This is a real repeated-child materialization path.

But the important nuance is:

- it consumes a generated record array from `FUN_00610F60(...)`
- it does not itself originate that array format

So this is still one layer above the missing raw builder seam.

## `FUN_005ECC80(...)` and `FUN_005ECDC0(...)`: light wrappers into owner-local generation

These two helpers are thinner.

Shared traits:

- both validate owner/context/input
- both build a small descriptor with message-like field `0x0B`
- both inspect owner flags like `0x2000` and `0x4000` through `FUN_00610EA0(...)`
- both forward into:
  - `FUN_00610F60(...)`

Differences:

- `FUN_005ECC80(...)`
  - has an alternate fast path through `FUN_0060CE70(...)`
  - checks `*(int *)(param_1 + 0x20)`
  - enforces a float bound on `*(float *)(param_3 + 4)`
- `FUN_005ECDC0(...)`
  - is the simpler always-`FUN_00610F60(...)` variant

So these look like specialized request wrappers into the same owner-local generation/materialization subsystem, not standalone builders.

## What this pass changes

Before this pass, the worker cluster was the best candidate for the missing builder layer.

After this pass, the cleaner picture is:

- `FUN_005EB4E0(...)` is a parser/layout walker for multiline text / markup
- its worker callbacks create concrete child controls and anchor objects
- `FUN_005ECEB0(...)` consumes a pre-generated row/record array from `FUN_00610F60(...)`
- the concrete row/record producer is therefore more likely:
  - `FUN_00610F60(...)`
  - or a helper directly beneath it

So the likely builder seam moved again:

- away from `CtlTextMl` worker callbacks
- toward the owner-local generation helpers they call

## Updated working model

The cleanest current stack is now:

- `FUN_005EA9A0(...)`
  - concrete multiline text/list control
- `FUN_005EB4E0(...)`
  - markup/text traversal and layout driver
- worker callbacks
  - instantiate concrete child controls from emitted runs/rows
- `FUN_00610F60(...)`
  - likely produces the row/record array consumed by those workers

That means the low-level data-production seam is probably not inside the text control itself, but in the owner/generation helper layer below it.

## Best next step

The next best step is to decompile:

- `FUN_00610F60`
- `FUN_0060CE70`
- `FUN_005F8810`
- `FUN_005EB280`
- `FUN_005F9400`

Why this is now the right pivot:

- `FUN_005ECEB0(...)` clearly consumes `FUN_00610F60(...)` output
- `FUN_0060CE70(...)` looks like the alternate generation path
- `FUN_005F8810(...)` is the concrete child callback used for each emitted repeated record

That is the shortest remaining route to the actual row/record production layer beneath the `CtlTextMl` worker family.
