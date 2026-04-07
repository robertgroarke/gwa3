## `Gw.exe` Frame Callback Interaction Lifecycle Addendum

This pass continues from the directional-control state-machine note by decompiling the remaining helper bodies around the `CtlInstance` interaction path:

- `FUN_005EDF50`
- `FUN_005EDB80`
- `FUN_005EDE20`
- `FUN_005EDEA0`
- `FUN_005EFFC0`
- `FUN_0052D190`

The goal was to replace the provisional labels:

- capture
- update
- completion

with a more exact lifecycle model.

## Source artifacts

These results come from:

- [gw_decomp_interaction_helpers_temp97.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_interaction_helpers_temp97.log)
- [gw_decomp_axis_followups_temp96.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_axis_followups_temp96.log)

## High-level result

This pass makes the control-instance lifecycle much clearer.

The strongest current model is now:

- `FUN_005EDF50(...)` seeds a tokened planar-motion state block
- `FUN_005EDB80(...)` seeds a secondary directional-motion state block
- `FUN_005EDE20(...)` promotes the planar-motion state from armed to active when squared distance exceeds a threshold
- `FUN_005EDEA0(...)` promotes the other motion path from armed to active when accumulated scalar travel exceeds a threshold
- `FUN_0052D190(...)` is the release/end handler, and it emits different owner-local channels depending on which interaction mode was active

So the control family now looks like a real multi-mode interaction state machine, not just a generic drag helper.

## `FUN_005EDF50(state, token, pos)`: initialize tokened planar-motion state

This helper seeds a state block very explicitly:

```cpp
state[0] = token;
state[6] = 1;
state[1] = pos.x;
state[2] = pos.y;
state[3] = pos.x;
state[4] = pos.y;
state[5] = 0;
```

That is a strong pattern for:

- bind active token/id
- store start position
- store current position
- arm the state as mode `1`

So this is best described as:

- tokened planar-motion state initializer

This is the helper used in `FUN_0052CC30(...)` case `0x3D` before the owner-local channel `7` emission.

## `FUN_005EDB80(state, pos)`: initialize directional-motion state

This helper is very similar, but not identical:

```cpp
state[0] = 1;
state[1] = pos.x;
state[2] = pos.y;
state[3] = pos.x;
state[4] = pos.y;
state[5] = 0;
state[6] = 0;
```

So this looks like a second state record for the same interaction family, but without the external token field that `FUN_005EDF50(...)` stores.

The cleanest description is:

- initialize directional/auxiliary motion state

paired with the tokened planar state above.

This lines up neatly with `FUN_0052CC30(...)`, where both helpers are called during interaction acquisition.

## `FUN_005EDE20(state, pos)`: planar distance threshold promotion

This helper:

- updates current position at `state + 0x0C / +0x10`
- if `state + 0x18 == 1`, computes squared distance from the stored origin
- compares that squared distance to `_DAT_00bb52cc`
- if exceeded, writes state `2` and returns `1`

So this is not a generic updater.
It is a very specific:

- planar distance threshold promotion helper

That gives a stronger interpretation of the `CtlInstance` branch where mode `1` emits channel `9`:

- channel `9` belongs to the planar-motion family once movement exceeds the activation threshold

## `FUN_005EDEA0(state, value)`: scalar accumulation threshold promotion

This helper is the scalar counterpart.

It:

- accumulates `value` into `state + 0x14`
- compares the accumulated scalar to `_DAT_00bb52c8`
- if the threshold is exceeded while `state + 0x18 == 1`, it promotes the state to `2` and returns `1`

So this is best described as:

- scalar accumulation threshold promotion helper

This matters because it suggests the interaction family really has two different thresholding modes:

- planar distance activation
- scalar travel activation

That is stronger than a one-size-fits-all drag model.

## `FUN_0052D190(event)`: end/reset dispatcher

This helper is the clearest lifecycle close-out body we have so far.

If the event token matches the active token:

- when `instance + 0x3C == 1`
  - it emits:
    - `FUN_006100A0(owner, 8, &local, 0)`
- when `instance + 0x3C == 2`
  - it emits:
    - `FUN_006100A0(owner, 0x0B, &local, 0)`

Then it always:

- resets the planar state via `FUN_005EDEE0(instance + 0x24)`
- resets the directional state via `FUN_005EDB70(instance + 0x60)`
- clears the active token at `instance + 0x20`

So this is best described as:

- interaction release / completion / reset dispatcher

with mode-specific end channels.

This gives the clearest phase pairings yet:

- begin for the family at channel `7`
- one active/update family around channel `9`
- another active/update family around channel `10`
- corresponding end/reset signals at channels `8` and `0x0B`

## What channels `8` and `0x0B` now look like

This pass gives the best current interpretation:

- channel `8`
  - end/release signal for the planar-motion mode
- channel `0x0B`
  - end/release signal for the directional-motion mode

That fits the earlier model well:

- `7` = interaction acquisition
- `9` = planar motion update family
- `10` = normalized directional motion update family
- `8` = planar interaction end
- `0x0B` = directional interaction end

I would still mark the exact user-facing names as provisional, but the lifecycle pairing is now much stronger than before.

## `FUN_005EFFC0(rect)`: validated region/update commit helper

This helper is less obviously about the state machine itself, but it is still useful context.

It:

- validates the incoming rectangle-like bounds
- computes width and height
- calls:
  - `FUN_005F00F0(&index, rect, &size, 0x100)`
- then asserts that the returned index matches the currently selected/active slot in an owner-managed table

So the safest description is:

- validated region/update commit helper

This keeps the interaction state machine tied to a managed region/slot collection, which fits the broader host/region interpretation.

## Updated lifecycle model

After this pass, the control-instance lifecycle reads much more cleanly:

1. acquire / arm
   - `FUN_005EDF50(...)`
   - `FUN_005EDB80(...)`
   - owner-local channel `7`
2. threshold into active mode
   - planar via `FUN_005EDE20(...)`
   - directional via `FUN_005EDBD0(...)`
   - alternate scalar via `FUN_005EDEA0(...)`
3. emit active updates
   - planar family via channel `9`
   - normalized directional family via channel `10`
4. release / complete / reset
   - planar end via channel `8`
   - directional end via channel `0x0B`
   - local state reset helpers

That is a much stronger lifecycle than the earlier generic "capture/update/completion" phrasing.

## What this does to the slot `2/3` interpretation

This pass does not directly rename slots `2` and `3`, but it strengthens their role indirectly:

- their `0x5B` updates are now sitting inside a real interaction lifecycle
- that lifecycle distinguishes begin, threshold, active update, and end phases
- and it contains separate planar and directional paths

So the best current interpretation remains:

- slot `2` and slot `3` are paired directional targets under the host

but now with a much stronger claim:

- they are part of a multi-mode interaction-control protocol, not just passive child controls

## Best next step

The next best step is to resolve the remaining reset/selection and table helpers around this lifecycle:

- `FUN_005EDEE0`
- `FUN_005EDB70`
- `FUN_005F00F0`
- `FUN_005EF740`

That should let us answer two remaining questions:

- what exact state is cleared at interaction end?
- what owner-managed region/slot table is this control instance committing into?

