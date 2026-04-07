## `Gw.exe` Frame Callback Message Path Addendum

This pass continues the `GameThread` callback reversal by tracing the next layer under the coordinate/state helpers from the previous addendum.

The main goal here was to answer two open questions:

1. what exactly is the `0x67726d64`-based lookup doing?
2. is the normalized-coordinate branch just updating local state, or is it building and sending a real internal message/packet?

The new decompilation batch makes both answers sharper.

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_domain_resolution_temp63.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_domain_resolution_temp63.log)

The key functions were:

- `FUN_0046F9B0`
- `FUN_00624900`
- `FUN_00624DC0`
- `FUN_0062B0B0`
- `FUN_00628800`
- `FUN_00627A50`

## High-level result

This pass makes two meaningful corrections to the previous model.

First:

- `0x67726d64` is not, by itself, the recovered name of the object family.
- it is an **access-key check** on a wrapper/record object.

Second:

- the normalized-coordinate branch is not merely “movement-like”
- it is a real **internal message/commit path**
- with structured local packet assembly, coordinate normalization, id validation, and follow-up dispatch

So the stronger current label for that branch is:

- **rect/coordinate message submission inside the frame/update system**

not:

- generic movement
- or a vague focus updater

## `FUN_0046F9B0(rec_wrapper, access_key)`: access-key resolver, not a subsystem name

This function is small but it corrects an important earlier assumption.

Its behavior is:

- if the wrapper pointer is null, return `0`
- if `*wrapper == 0`, assert/fail
- if `wrapper[2] != 0` and the requested access key does not match, assert/fail
- otherwise return `*wrapper`

The decompiled assertion text is the important clue:

- `"accessKey (0x%x) != rec->accessKey (0x%x),  recObject (0x%x)"`

That means the earlier `FUN_006446B0(..., 0x67726d64)` / `FUN_00645410(..., 0x67726d64)` path should now be read as:

- resolve a record/object through **access key** `0x67726d64`
- then validate that the resolved runtime object type is `5`

So the safer wording going forward is:

- **type-5 objects resolved through access key `0x67726d64`**

not:

- object family “named” `0x67726d64`

That is a meaningful tightening of the model.

## `FUN_00624900(msg_id, entry, arg3, arg4)`: central message/commit builder

This is the most important function in the new batch.

It is not a small setter. It is a real structured dispatch helper.

### Message ids recovered here

The body explicitly handles:

- `0x24`
- `0x2d`
- `0x2e`

The meanings are still inferred, but the control flow is not:

- `0x24` performs list insertion / active-registration work and sets a bit in a bitfield
- `0x2e` clears that bit and enforces that it was previously set
- `0x2d` skips the float normalization step and uses raw coordinates from the entry

### Packet-like local state

After the per-message prologue, the function assembles a local state block:

- `local_34 = *entry`
- `local_20 = *in_ECX`
- `local_24 = arg3`
- `local_1c = entry[5]`
- `local_2c / local_28` = either raw or transformed coordinates
- `local_30` = packed flags derived from `entry[1]`

That is already more than “commit current state.” It is packaging a specific record for downstream dispatch.

### Coordinate handling

If the message is not `0x2d`, the function:

- scales `entry[2]` and `entry[3]` by globals
  - `_DAT_00bd0d7c`
  - `_DAT_00bd0d80`
- runs them through `FUN_00629A00(...)`
- optionally runs them through `FUN_006298F0(...)` if a flag bit is set in `in_ECX[0x3f]`

So the coordinate pair is not being passed through untouched. It is normalized/transformed into message-ready form.

### Final dispatch

The assembled block is then sent through:

- `FUN_006286D0(msg_id, &local_34, arg4)`

and, if a registry/id validation succeeds for `in_ECX[10]` and the message is `0x24`, it also calls:

- `FUN_00628A10(5, &local_34, arg4)`

That makes the normalized-coordinate branch much stronger semantically:

- it builds a packet-like local structure
- emits it through a central message dispatcher
- optionally follows with a second typed dispatch when the id/context is valid

This is now clearly a message-submission path.

## `FUN_00624DC0(state, kind, out)`: coordinate normalization and validation gate

This helper sits right before the message builder and explains more of the data-prep stage.

Its behavior is:

- `out` must be non-null
- initialize `*out = 0`
- reject the input if either coordinate is `INFINITY`
- scale the stored coordinates at `state + 8` and `state + 0xC`
- then choose one of two conversion paths

Path 1:

- if `DAT_00bd0c40 != 1`
- call `FUN_0060C1E0(&local_14, kind, out)`

Path 2:

- if `DAT_00bd0c40 == 1`
- get a float from `FUN_0061BC70()`
- call `FUN_0060C050(&local_14, that_float)`

So `FUN_00624DC0` is not just a helper for “getting floats.” It is the validation/normalization gate that turns stored coordinate state into a downstream-ready converted output object.

That fits tightly with the previous addendum’s `FUN_00624F80(...)` path, which called:

- `FUN_00624DC0(..., 0x40, ...)`

before triggering `FUN_00624900(...)`.

## `FUN_0062B0B0(pair)`: rect/object construction from the coordinate pair

This helper sharpens the “what kind of state is being submitted?” question.

Its behavior is:

- call `FUN_0062A220(...)` to obtain current bounds/viewport-like values
- require the bounds to be valid
- write:
  - `x = pair[0]`
  - `y = pair[1]`
  - `x2 = pair[0] + width`
  - `y2 = pair[1] + height`
- write type/state value `6` at `+8`
- link the object into a registry/list rooted at `DAT_00bd0cd0`, using offset `DAT_00bd0ccc`

That is much more specific than the earlier “movement/focus update” label.

The helper is building a real rect-like runtime object from:

- the normalized coordinate pair
- the current bounds/viewport dimensions

and then submitting/linking it into an engine-owned registry.

The best current description is:

- **rect/state object submission**

The exact user-facing name is still unresolved, but this looks much closer to:

- viewport anchor state
- selection/focus rect state
- or another UI-space rectangle submission

than to anything like free camera motion.

## `FUN_00628800(id)` and `FUN_00627A50(id)`: validated registry access

These two functions explain the validity checks in the surrounding code.

### `FUN_00627A50`

This is the raw table lookup:

- use `in_ECX` as the owning table object
- ensure `id < count`
- ensure the slot is non-null when `id != 0`
- return `table[id]`

### `FUN_00628800`

This is a checked wrapper around the same table:

- validate the id against globals
  - `DAT_00bb5624`
  - `DAT_00bb562c`
- call `FUN_00627A50(id)`
- assert if invalid

That means the earlier `FUN_006290B0(id)` path from the previous addendum is now easier to interpret:

- it is part of a registry/handle validation layer
- not the subsystem itself

And it clarifies `FUN_006108E0(handle, pair)` from the previous pass:

- first validate the handle/id
- then submit the rect/state object derived from the coordinate pair

## What this changes about the callback model

### Previous model

The previous best label for the `FUN_0061AD80` branch was:

- smooth bounded normalized-coordinate update with commit behavior

That was reasonable, but still slightly too loose.

### Stronger current model

This pass supports a stronger description:

- time-driven coordinate update
- normalized and validated through `FUN_00624DC0`
- converted into a packet-like local record by `FUN_00624900`
- emitted through internal dispatcher `FUN_006286D0`
- optionally followed by a second typed dispatch via `FUN_00628A10`
- and paired with rect/object submission through `FUN_0062B0B0`

So the path now looks like:

`frame callback -> timed coordinate update -> validation/normalization -> packet/message assembly -> internal dispatch -> rect/object submission`

That is much closer to a real engine messaging/control plane than to a mere local updater.

## Updated interpretation

The strongest safe reading after this pass is:

- GWCA’s `GameThread` hook sits above a real per-frame UI/visual maintenance callback
- one important child branch in that callback performs **internal rect/coordinate message submission**
- and the hierarchy-side object helpers act on **type-5 objects resolved through access key `0x67726d64`**

So both halves of the earlier downstream interpretation are now tighter:

- the coordinate path is a message/packet submission path
- the object path is access-key mediated, not named directly by the key constant

## Best next step

The next best reverse step is to keep following the message layer rather than the high-level callback shell.

The highest-value nearby targets are:

- `FUN_006286D0`
- `FUN_00628A10`
- `FUN_00629A00`
- `FUN_006298F0`

That should tell us:

- what the `0x24` / `0x2d` / `0x2e` message ids actually mean
- whether the second dispatch is a fan-out/event bus or a type-specialized submit path
- and whether the coordinate transforms are viewport-space, client-space, or another frame-space convention
