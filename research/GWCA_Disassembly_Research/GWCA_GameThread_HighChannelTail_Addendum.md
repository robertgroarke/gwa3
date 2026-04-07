# GWCA GameThread High Channel Tail Addendum

This pass continues directly from the high-channel spread note by tightening the narrow end of the owner-local high-band protocol:

- channel `0x0B`
- channel `0x0C`
- and the repeated validity gate:
  - `FUN_0060F610(...)`

The goal was to answer the next structural question:

- are `0x0B` and `0x0C` broad members of the same shared high-band plane as `7 / 8 / 9 / 10`?
- or do they belong to a narrower relation-driven tail family?

Fresh targets for this pass were:

- `FUN_00578460`
- `FUN_005E7DC0`
- `FUN_005ED590`
- `FUN_00604EE0`
- `FUN_0061CE50`

## Source Artifacts

These results come from:

- [gw_findcallers_0060f610_temp188.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_findcallers_0060f610_temp188.log)
- [gw_decomp_highband_tail_temp189.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_highband_tail_temp189.log)
- and the earlier helper baseline:
  - [gw_decomp_extended_cluster_temp77.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_extended_cluster_temp77.log)

## Main Result

This pass gives a clean split.

The strongest current reading is:

- `7 / 8 / 9 / 10` are the broad reusable center of the owner-local high-band plane
- `0x0B` and `0x0C` are noticeably narrower tail ids
- `FUN_0060F610(...)` is the post-dispatch liveness gate that decides whether a control can continue into:
  - a follow-up high-band phase
  - deeper refresh/requery work
  - or both

So the best current model is not:

- one flat high-band protocol where all ids are equally general

It is:

- broad center ids
- plus a narrower tail used by relation-driven or selection-driven control families

## `FUN_0060F610(...)`: post-dispatch liveness gate still holds

The older helper note remains correct.

`FUN_0060F610(...)` is still just:

- a tiny validity probe over:
  - `FUN_006290B0(...)`

But this pass gives that probe a much stronger structural role.

It consistently appears:

- after an owner-local high-band dispatch
- before a follow-up phase
- before refresh / requery work
- or before deeper slot/object extraction

So it is best described now as:

- post-dispatch owner/control liveness gate

rather than merely:

- generic validity helper

## Tail Patterns

### `FUN_00604EE0(...)`: generic high-band dispatch followed by liveness-gated refresh

This helper is still the clearest compact expression of the pattern:

```cpp
FUN_006100A0(owner, type_id, payload, 0);
if (FUN_0060F610(owner) != 0) {
    if (FUN_00610EA0(owner, 0x100000) == 0) {
        FUN_0060D890(owner);
    }
}
```

So one stable architecture rule is now:

- high-band dispatch may be followed by a liveness-guarded deep refresh/rebind pass

That matters because it explains why `FUN_0060F610(...)` shows up in so many control-adjacent callers without carrying semantic meaning by itself.

### `FUN_00578460(...)`: repeated-slot sibling confirms the `8 -> valid -> 10` pattern

This caller is very close in shape to the earlier `FUN_00557A40(...)` sample, which is useful because it confirms the pattern rather than introducing a new one.

It shows:

- when relation/event `8` arrives with zero payload:
  - `FUN_006100A0(owner, 0x0B, 0, 0)`
- when relation/event `8` carries a region payload:
  - fetch geometry/state through `0x5A`
  - `FUN_006100A0(owner, 9, local_c, 0)`
- when relation/event `9` arrives:
  - `FUN_006100A0(owner, 8, 0, 0)`
  - if `FUN_0060F610(owner) != 0`
    - fetch geometry/state again
    - `FUN_006100A0(owner, 10, local_c, 0)`

That is an important confirmation.

It says the repeated/region family already has a stable tail shape:

- immediate stop/end-style signal:
  - `0x0B`
- intermediate reset/ack phase:
  - `8`
- liveness probe
- then continuation/update:
  - `10`

So the earlier `FUN_00557A40(...)` result was not a one-off.

### `FUN_005E7DC0(...)`: selection/list family uses `0x0B` and `0x0C` as narrow tail verbs

This is the strongest new tail result in the pass.

The function is clearly a richer selection/list-style state machine, with many cases that mutate current indices, normalize current-vs-anchor values, and trigger refresh work.

Its high-band tail usage is distinctive:

- case `9`
  - `FUN_006100A0(owner, 0x0B, 0, 0)`
- case `0x14` and `0x69`
  - `FUN_006100A0(owner, 10, 0, 0)`
- case `0x16`
  - `FUN_006100A0(owner, 0x0C, 0, 0)`

And after the case work, it does:

- `FUN_0060F610(owner)`
- if still valid:
  - maybe emit:
    - `FUN_006100A0(owner, 7, 0, 0)`
  - maybe dirty and reevaluate through:
    - `FUN_0060D080(owner)`
    - `FUN_00610330(owner)`
  - maybe run a local finalize helper

That is the clearest evidence so far that:

- `0x0B` and `0x0C` live in a narrower tail branch tied to selection/relation control families
- not in the broader general-purpose center where `7 / 8 / 9 / 10` are widely reused

The cleanest current reading is:

- `0x0B` = end / cancel / stop-like tail signal
- `0x0C` = confirm / apply / secondary finalize-like tail signal

Those names are still provisional, but the narrow-family behavior is real.

### `FUN_005ED590(...)`: `10` is followed by liveness-gated deeper extraction, not by `0x0C`

This caller is useful mainly because it shows a different post-`10` pattern.

It:

- optionally emits:
  - `FUN_006100A0(owner, 9, 0, local_block)`
- dirties and reevaluates the owner
- emits:
  - `FUN_006100A0(owner, 10, payload, 0)`
- then probes:
  - `FUN_0060F610(owner)`

If the owner remains valid, it does not emit `0x0C`.
Instead it gates deeper extraction / enumeration work and may then emit:

- `FUN_006100A0(owner, 7, local_block + 4, 0)`

That is a useful limit on the model.

It means:

- `10` does not inherently imply `0x0C`
- `0x0C` is not the generic next phase after every `10`
- the `0x0B / 0x0C` tail is more family-specific than that

### `FUN_0061CE50(...)`: `FUN_0060F610(...)` can also guard non-high-band follow-up control work

This one does not emit `0x0B` or `0x0C`, but it still matters for the helper role.

It:

- checks whether a global owner/control handle exists
- probes it through:
  - `FUN_0060F610(...)`
- if valid:
  - updates attached state through:
    - `FUN_005F3A60(...)`
  - and toggles activity through:
    - `FUN_00610D30(...)`

So the validity probe is even more general than the tail ids:

- the probe is broad
- the `0x0B / 0x0C` usage is narrow

That keeps the architecture clean.

## What This Changes

Before this pass, the best safe reading was:

- `0x0B` and `0x0C` might just be later members of the same general high-band protocol

After this pass, the stronger reading is:

- the shared center of the high-band is:
  - `7`
  - `8`
  - `9`
  - `10`
- the narrower tail is:
  - `0x0B`
  - `0x0C`
- and the tail clusters around:
  - relation-driven controls
  - repeated region/selection families
  - selection/list-style state machines

So the best current layered high-band map is:

- broad control/content phases:
  - `7 / 8 / 9 / 10`
- narrow tail phases:
  - `0x0B / 0x0C`
- post-dispatch survival gate:
  - `FUN_0060F610(...)`

## Updated Interpretation

The strongest current interpretation is now:

- `FUN_006100A0(...)` remains the broad owner-local control/content plane
- but not every id in that plane is equally generic
- `0x0B` and `0x0C` are best treated as tail verbs used mainly by relation/selection-oriented families
- `FUN_0060F610(...)` is the liveness hinge between:
  - dispatch
  - follow-up tail phase
  - and deeper refresh/requery work

So the high-band picture is now cleaner:

- center ids are shared broadly
- tail ids are specialized

## Best Next Step

The next best reverse step is to close the tail with one compact sibling pass:

1. follow one or two more `FUN_0060F610(...)` callers that are outside the current region/selection pocket
2. prioritize callers that might emit:
   - `0x0B`
   - or `0x0C`
3. if they do not, promote the current model:
   - `0x0B / 0x0C` are narrow tail verbs
   - while `FUN_0060F610(...)` stays a broader post-dispatch gate
