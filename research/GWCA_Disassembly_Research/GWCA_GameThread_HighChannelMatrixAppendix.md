# GWCA GameThread High Channel Matrix Appendix

This appendix consolidates the current owner-local high-band model that was spread across:

- the high-channel spread note
- the high-channel tail note
- the high-channel tail closure note

The goal is not to over-name every id.
It is to preserve the strongest stable structure in one place before the next reverse passes move back into deeper seams.

## Scope

This appendix is about:

- `FUN_006100A0(owner, type_id, payload, arg)`
- its live higher ids
- and the shared liveness hinge:
  - `FUN_0060F610(...)`

It is **not** the same thing as:

- the low typed-channel family around:
  - `FUN_00628A10(...)`
  - `type_id 0..6`
- or the raw high global message plane around:
  - `FUN_00610160(...)`
  - `msg_id >= 0x56`

## Core Split

The strongest current split is:

- broad center ids:
  - `7`
  - `8`
  - `9`
  - `10`
- narrow tail ids:
  - `0x0B`
  - `0x0C`
- shared post-dispatch liveness hinge:
  - `FUN_0060F610(...)`

That means the high band is not one flat pool of equally generic ids.

It has:

- a broadly reused center
- a narrower family-specific tail
- and one broad follow-up validity probe

## High-Level Matrix

| id | current structural role | breadth | strongest safe reading |
| --- | --- | --- | --- |
| `7` | center | broad | begin / arm / changed-envelope style phase, family-specific payload meaning |
| `8` | center | broad | reset / commit / end-adjacent phase, family-specific meaning |
| `9` | center | broad | follow-up / content-update / below-limit / payload-bearing update phase |
| `10` | center | broad | directional or continuation update phase in several families |
| `0x0B` | tail | narrow | end / cancel / stop-like tail verb in relation/selection families |
| `0x0C` | tail | narrow | confirm / apply / secondary finalize-like tail verb in narrow families |
| `FUN_0060F610(...)` | hinge | broad | post-dispatch owner/control liveness gate |

## Observed Family Shapes

### Broad-center families

These families use the center ids without proving the narrow tail is part of their normal protocol:

- character-creation item/color control
  - `FUN_004D06D0(...)`
  - uses:
    - `7`
    - `8`
- bounded numeric/value control
  - `FUN_00535770(...)`
  - uses:
    - `7`
    - `8`
    - `9`
- mission-selection control
  - `FUN_00508810(...)`
  - uses:
    - `8`
    - `9`
  - probes:
    - `FUN_0060F610(...)`
- refresh/rebind helper family
  - `FUN_005068C0(...)`
  - uses:
    - `7`
    - `8`
  - probes:
    - `FUN_0060F610(...)`
  - then refreshes through:
    - `FUN_0060D890(...)`

### Narrow-tail families

These families are where `0x0B` and `0x0C` currently live most convincingly:

- relation-driven repeated region/stateful control
  - `FUN_00557A40(...)`
  - uses:
    - `9`
    - `10`
    - `0x0B`
    - `0x0C`
- repeated-slot sibling with the same tail shape
  - `FUN_00578460(...)`
  - uses:
    - `0x0B`
    - `8`
    - `9`
    - `10`
- richer selection/list-style state machine
  - `FUN_005E7DC0(...)`
  - uses:
    - `0x0B`
    - `10`
    - `0x0C`
    - and sometimes re-arms through:
      - `7`

### Mixed but still consistent families

- `UiCtlInstance`-style control shell
  - `FUN_00514140(...)`
  - uses:
    - `8`
    - `10`
    - `0x0B`
  - this fits the current model because it still looks closer to the interaction/relation tail families than to the purely scalar/value families

## `FUN_0060F610(...)` Role

The helper itself remains small:

- effectively a validity probe around:
  - `FUN_006290B0(...)`

But structurally it now has a clear role:

- dispatch high-band phase
- test whether the owner/control still survives as a meaningful target
- only then continue into:
  - another high-band phase
  - deep refresh/rebind
  - local extraction/enumeration
  - or owner dirty/reevaluation helpers

So the safest short description is:

- post-dispatch liveness hinge

not:

- semantic part of the narrow tail

## Stable Patterns

### Pattern A: center id -> liveness -> refresh

Observed in:

- `FUN_005068C0(...)`
- `FUN_00604EE0(...)`

Shape:

```cpp
FUN_006100A0(owner, 7_or_8, payload, 0);
if (FUN_0060F610(owner) != 0) {
    FUN_0060D890(owner);
}
```

### Pattern B: `8` -> liveness -> `10`

Observed in:

- `FUN_00557A40(...)`
- `FUN_00578460(...)`

Shape:

```cpp
FUN_006100A0(owner, 8, ..., 0);
if (FUN_0060F610(owner) != 0) {
    FUN_006100A0(owner, 10, ..., 0);
}
```

This pattern is one of the stronger reasons to keep:

- `8`
- and `10`

in the shared broad center rather than treating them as tail-only ids.

### Pattern C: narrow tail stop/apply verbs

Observed in:

- `FUN_00557A40(...)`
- `FUN_00578460(...)`
- `FUN_005E7DC0(...)`
- partially in:
  - `FUN_00514140(...)`

Shape:

- `0x0B` appears as a stop/end/cancel-like side branch
- `0x0C` appears as a narrower apply/finalize-like continuation

This pattern is what keeps the tail narrow in the current model.

## Working Rules

When a new `FUN_006100A0(...)` caller appears, the fastest current classifier is:

1. if the caller only uses `7 / 8 / 9 / 10`, classify it as:
   - broad-center high-band family
2. if the caller uses `0x0B` or `0x0C`, treat it as:
   - candidate narrow-tail family
3. if the caller uses `FUN_0060F610(...)` after dispatch, do **not** treat that alone as tail evidence
   - it is broad and shared
4. if the caller uses:
   - `8 -> FUN_0060F610(...) -> 10`
   classify that as:
   - center-plus-hinge pattern
   not automatically:
   - narrow tail

## Current Limits

What is strong now:

- center vs tail split
- `FUN_0060F610(...)` as a broad hinge
- `0x0B / 0x0C` as narrow-family verbs

What is still provisional:

- exact user-facing names for each id
- whether `0x0C` is always apply/finalize rather than sometimes another narrow transition
- whether every `0x0B` caller is truly cancellation/end rather than a broader stop-like signal

## Best Next Step

With this appendix in place, the best next reverse step is no longer more high-band classification.

The strongest next move is to pivot inward again to one of the deeper unresolved seams:

- emitted-object helpers under:
  - `FUN_00653A60(...)`
- or staged helper correspondence under:
  - `FUN_0069E870(...)`
  - `FUN_0069E1C0(...)`
