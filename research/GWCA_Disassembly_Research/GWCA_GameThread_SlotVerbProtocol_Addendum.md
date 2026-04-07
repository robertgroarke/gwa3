## `Gw.exe` Frame Callback Slot-Verb Protocol Addendum

This pass continues from the secondary-slot identity correction by decompiling the next helper cluster:

- `FUN_00605690`
- `FUN_00610EA0`
- `FUN_0060DD30`
- `FUN_005F1170`
- `FUN_005F1190`
- `FUN_005F1150`
- `FUN_005F1AB0`
- `FUN_0060D080`

The goal was to move from:

- "there is a high-id protocol on slot children"

to:

- "which verbs are getters, which are setters, and what kind of state transition they trigger"

## Source artifacts

These results come from:

- [gw_decomp_slot_verbs_temp82.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_verbs_temp82.log)
- [gw_decomp_slot_commit_temp83.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_commit_temp83.log)
- [gw_decomp_secondary_callers_temp81.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_secondary_callers_temp81.log)

## High-level result

This pass makes the slot protocol much less abstract.

The strongest current model is:

- slot children are owner-like relation objects
- `0x59` = getter/query
- `0x58` = getter/query
- `0x57` = setter/update
- `0x5C` = setter/update
- after slot mutation, the parent owner is marked dirty through `FUN_0060D080(owner)`

So the core slot family now looks like a real:

- query / set / dirty-notify

protocol rather than a vague "extended high-id message plane."

## `FUN_005F1190(slot_obj)`: `0x59` getter

This helper is very small:

```cpp
FUN_00610160(slot_obj, 0x59, 0, &local_8);
return local_8;
```

So `0x59` is now directly confirmed as a getter-style verb that writes its result through `lParam`.

That matches how earlier callers used `0x59` as a read before selection/mapping logic.

## `FUN_005F1170(slot_obj)`: `0x58` getter

This helper has the same pattern:

```cpp
FUN_00610160(slot_obj, 0x58, 0, &local_8);
return local_8;
```

So `0x58` is also a getter-style verb.

At this point:

- `0x58`
- `0x59`

are no longer inferentially "query-like."
They are concrete query verbs.

## `FUN_005F1150(slot_obj, value)`: `0x57` setter

This helper is the setter counterpart:

```cpp
FUN_00610160(slot_obj, 0x57, value, 0);
```

So `0x57` is now directly confirmed as an update/set verb on a slot child.

That lines up with the earlier caller patterns where `0x57` was used on slot `3` after compatibility or mode checks.

## `FUN_005F1AB0(slot_obj, value)`: `0x5C` setter

This helper is equally direct:

```cpp
FUN_00610160(slot_obj, 0x5c, value, 0);
```

So `0x5C` is another setter/update verb.

Combined with the switch body from the earlier caller pass, this gives a clearer grouping:

- `0x57` = direct set/update on one slot child family
- `0x5C` = direct set/update on another slot-directed property family
- `0x58` / `0x59` = read/query verbs

## `FUN_00605690(owner, slot, value)`: generic slot-set helper

This helper finally explains the `0x5B..0x5E` family from the earlier caller switch.

Its body is:

- resolve slot child through `FUN_0060E2B0(owner, slot)`
- if found:
  - call `FUN_005F1AB0(slot_child, value)`
  - call `FUN_0060D080(owner)`

So `FUN_00605690(...)` is a generic:

- resolve slot child by slot number
- apply `0x5C` setter to that slot child
- mark parent owner dirty

helper.

This means the switch cases that used:

- `FUN_00605690(owner, 2, param)`
- `FUN_00605690(owner, 3, param)`
- `FUN_00605690(owner, 4, param)`
- `FUN_00605690(owner, 5, param)`
- `FUN_00605690(owner, 6, param)`

are best interpreted as:

- slot-family setters routed through a common `0x5C` slot-child update verb

not:

- five unrelated high-id messages

## `FUN_0060D080(owner)`: owner dirty / refresh kick

This helper is tiny but important:

- validate owner
- call `FUN_006176B0(4, 0xFFFFFFFF)`

So after slot mutation, the parent owner is not directly sending another `0x56..0x5E` verb.
It is triggering a broader owner-level refresh/dirty path.

That makes the slot setters look transactional:

- update slot child property
- kick owner refresh

## `FUN_00610EA0(obj_id, mask)`: test owner flag word at `+0x190`

This helper returns:

```cpp
*(obj + 400) & mask
```

which is:

- `0x190` in hex

That matters because the earlier callers gated several `0x57` / `0x5A` paths on `FUN_00610EA0(obj, 0x2000)`.

So `0x2000` is now firmly a bit in the owner-local flag word at `+0x190`, not just a free-floating protocol state.

This strengthens the owner/frame interpretation again.

## `FUN_0060DD30(obj_id, enabled)`: toggle `0x10` and emit message `0x0C`

This helper mirrors the earlier `FUN_00610D30(obj_id, enabled)` helper.

Its behavior is:

- validate object
- check current `0x10` flag
- if state differs:
  - mutate flags through `FUN_0062E5D0(0, 0, 0x10)`
  - emit global message `0x0C`

So the slot/owner layer now has at least two visible flag-driven notification channels:

- `0x200` -> global message `0x36`
- `0x10` -> global message `0x0C`

That is useful because it shows the slot protocol is tied into a larger owner-state machine, not only local getters and setters.

## What this says about `0x57..0x5E`

With the new helper bodies in hand, the safest refined interpretation is:

- `0x57` = direct setter on a slot child
- `0x58` = getter on a slot child
- `0x59` = getter on a slot child
- `0x5C` = direct setter on a slot child
- `0x5B`, `0x5D`, `0x5E` remain setter-family neighbors because the sampled callers route them through the same slot-oriented controller layer

So the protocol is no longer best described as:

- "high-id messages"

It is better described as:

- a slot-child property protocol layered over owner-attached relation children

with:

- read verbs
- write verbs
- parent dirty/refresh follow-up

## Revised slot model

Putting the last two passes together, the best current model is:

1. parent owner/control object exists
2. `FUN_0060E2B0(owner, slot)` resolves owner-attached slot child by slot number
3. the slot child has its own owner-like runtime id and flag word
4. verbs like `0x57`, `0x58`, `0x59`, and `0x5C` operate on that slot child
5. setters commonly trigger parent-side refresh through `FUN_0060D080(owner)`

That is much sharper than the earlier "secondary frame/control object" wording.

## Best next step

The strongest next pass is to identify what the concrete slot numbers mean.

The highest-yield targets are:

- more callers of `FUN_0060E2B0(owner, slot)`
- especially the families using:
  - slot `0`
  - slot `1`
  - slot `2`
  - slot `3`
  - slot `4`
- plus the downstream helper:
  - `FUN_006176B0(4, 0xFFFFFFFF)`

That should let us move from:

- "slot 1 child"
- "slot 3 child"

to:

- actual semantic labels such as list/selection/status/preview/commit child families, if the caller patterns are distinct enough.
