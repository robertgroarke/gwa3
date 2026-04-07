# GWCA GameThread High Channel Tail Closure Addendum

This pass closes the follow-up question from the high-channel tail note by sampling `FUN_0060F610(...)` callers outside the current region/selection pocket.

The goal was simple:

- if outside callers also emit `0x0B` or `0x0C`, then the narrow-tail model stays open
- if they do not, then the narrow-tail model is probably good enough to treat as closed

Fresh outlier targets for this pass were:

- `FUN_005068C0`
- `FUN_00508810`

These were chosen because they call `FUN_0060F610(...)` but were not part of the earlier repeated-slot / relation-selection sample set.

## Source Artifacts

These results come from:

- [gw_findcallers_0060f610_temp188.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_findcallers_0060f610_temp188.log)
- [gw_decomp_0060f610_outliers_temp190.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_0060f610_outliers_temp190.log)

## Main Result

This pass is clean confirmation rather than a correction.

The new outlier callers use:

- `FUN_0060F610(...)`
- and the broad center ids:
  - `7`
  - `8`
  - `9`

but they do **not** introduce:

- `0x0B`
- or `0x0C`

So the best current model now looks stable:

- `FUN_0060F610(...)` is broad
- `7 / 8 / 9 / 10` form the reusable center of the high owner-local band
- `0x0B / 0x0C` stay narrow and clustered in the relation/selection tail families

That is exactly the closure signal we wanted.

## `FUN_005068C0(...)`: broad-center caller with liveness-gated refresh only

This caller uses `FUN_0060F610(...)` twice, but only with the broad center ids.

It has three main mode branches:

- one local slot/child-management branch around:
  - `FUN_0060E2B0(owner, 2)`
  - and `FUN_0060DD30(...)`
- one branch that does:
  - `FUN_006100A0(owner, 7, payload, 0)`
  - `FUN_0060F610(owner)`
  - if still valid:
    - `FUN_0060D890(owner)`
- and a sibling branch that does:
  - `FUN_006100A0(owner, 8, 0, 0)`
  - `FUN_0060F610(owner)`
  - if still valid:
    - `FUN_0060D890(owner)`

What matters here is what it does **not** do:

- no `0x0B`
- no `0x0C`

So this is a clear outlier-family example of:

- broad high-band center id
- then liveness probe
- then deep refresh/rebind

without entering the narrow tail at all.

## `FUN_00508810(...)`: mission-selection control also avoids the tail ids

This caller names:

- `P:\\Code\\Gw\\Ui\\Game\\GmSelectMission.cpp`

which is useful because it is plainly outside the repeated-slot / relation-selection family we were testing against.

The important path is:

- in case `0x24`
  - resolve an owner through `FUN_005099D0(...)`
  - `FUN_006100A0(owner, 8, 0, 0)`
  - `FUN_0060F610(owner)`
  - if still valid and a local flag bit is set:
    - `FUN_006100A0(owner, 9, 0, 0)`

Again, the key fact is absence:

- no `0x0B`
- no `0x0C`

So even in a clearly different mission-selection control shell, the outlier still stays inside the broad reusable center and never reaches the narrow tail.

## What This Changes

Before this pass, the best safe reading was:

- `0x0B / 0x0C` looked narrow
- but we still had not checked whether outside `FUN_0060F610(...)` callers reused them

After this pass, the stronger reading is:

- outside `FUN_0060F610(...)` callers do not seem to pull `0x0B / 0x0C` outward
- they keep using:
  - `7`
  - `8`
  - `9`
  - liveness gating
  - and refresh/rebind follow-up

So the current layered model is now strong enough to promote:

- broad center:
  - `7 / 8 / 9 / 10`
- narrow tail:
  - `0x0B / 0x0C`
- broad hinge:
  - `FUN_0060F610(...)`

## Updated Interpretation

The high-band picture is now much cleaner:

- `FUN_0060F610(...)` is not evidence for the narrow tail by itself
- it is the general liveness hinge after owner-local high-band dispatch
- `0x0B / 0x0C` remain specialized tail verbs rather than general-purpose high-band phases

So the narrow-tail model can now be treated as provisionally closed.

## Best Next Step

With the high-band tail question basically closed, the next best step is no longer more caller sampling here.

The strongest next move is to pivot back inward and document the settled model in one compact appendix or matrix:

1. center high-band ids:
   - `7 / 8 / 9 / 10`
2. narrow tail ids:
   - `0x0B / 0x0C`
3. shared hinge/helper:
   - `FUN_0060F610(...)`

After that, the next fresh reverse seam should probably be:

- emitted-object helpers under `FUN_00653A60(...)`
- or the exact staged helper mapping under:
  - `FUN_0069E870(...)`
  - `FUN_0069E1C0(...)`
