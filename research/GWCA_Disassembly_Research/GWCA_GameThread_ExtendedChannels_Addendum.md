## `Gw.exe` Frame Callback Extended Channels Addendum

This pass continues from the channel-classification note by decompiling the remaining high-yield `FUN_00628A10(...)` callers:

- `FUN_0062F7E0`
- `FUN_0060DAA0`
- `FUN_006100A0`
- `FUN_0060C3A3`

The goal here was to answer the obvious follow-up:

- does the owner-local channel family really stop at `1..4`, or are there live channels beyond that range?

## Source artifacts

These results come from:

- [gw_decomp_remaining_channels_temp73.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_remaining_channels_temp73.log)

## High-level result

This pass pushes the channel family further than the previous map.

The strongest new points are:

- `type_id 6` is definitely live
- it is paired with a new global message family:
  - `0x3d`
  - `0x3c`
  - `0x3f`
- `FUN_006100A0(...)` looks like a generic typed-channel entrypoint for `type_id >= 7`
- `FUN_0060C3A3(...)` is effectively another front door into the same preflight/activation path already mapped by `FUN_0060BCB0(...)`
- `FUN_0060DAA0(...)` is another traversal driver over channel `3`, reinforcing that `type_id 3` is not a one-off but a real owner-family drain/walk channel

So the owner-local channel family is now best described as:

- a core visible set `1..4`
- plus a live message-coupled `6`
- plus evidence of an externally invokable extended range `>= 7`

That is a more open-ended system than the previous lifecycle-only model suggested.

## `FUN_0062F7E0(...)`: message-coupled channel `6`

This is the biggest extension in the batch.

Its behavior mirrors the earlier coordinate/message builders in several ways:

- for `msg == 0x3d`, it allocates and list-links a record, then sets a bit in a bitfield
- for `msg == 0x3c` and `msg == 0x3f`, it clears the bit instead
- it builds a local packet-like structure containing:
  - `*param_2`
  - transformed coordinates from `param_2[1]` / `param_2[2]`
  - `param_3`
  - `param_4`
  - current flags/bitfield
- it uses:
  - `FUN_00629A00(...)`
  - and, when a flag bit is set, `FUN_006298F0(...)`
- then dispatches through:
  - `FUN_006286D0(msg, local_38, param_5)`

Most importantly:

- if the registry/id check succeeds and `msg == 0x3d`
- it calls:
  - `FUN_00628A10(6, local_38, param_5)`

That gives us a new live channel:

- **`type_id 6`**

### What `type_id 6` currently looks like

The safest current reading is:

- a message-coupled owner-local channel associated with the `0x3d` add/activate path

The precise user-facing name is still unresolved, but the structure is very strong:

- `0x3d` = add/activate/register for this message family
- `0x3c` / `0x3f` = clear/remove/deactivate companions
- `type_id 6` = owner-local follow-up channel for the `0x3d` activation case

That makes `type_id 6` structurally similar to the earlier `0x24 -> type_id 5?` style pattern, but now with direct evidence in hand.

## `FUN_0060DAA0(owner_id)`: additional channel `3` traversal driver

This helper starts by validating an owner id, then:

- resolves the owner
- repeatedly walks:
  - `FUN_0062D220(3, 0)`
- raises owner flag `8`
- calls:
  - `FUN_00628A10(3, 0, 0)` when needed
- and runs the same broad refresh cascade we already saw in the channel-`3` path

This is not introducing a brand-new channel, but it matters because it proves `type_id 3` is used from multiple entrypoints:

- direct owner traversal
- owner-id driven activation/rebind
- recursive drain/walk logic

So `type_id 3` is not just “whatever one helper happened to use.” It is a central owner activation/drain channel in this subsystem.

## `FUN_0060C3A3()`: another front door into channel `1 -> 3`

This helper is effectively a sibling/front-end to `FUN_0060BCB0(...)`.

Its behavior is:

- resolve the current owner through `FUN_00628800()`
- initialize a local veto/status slot
- call:
  - `FUN_006286D0(7, 0, &status)`
- if still valid and not vetoed:
  - call:
    - `FUN_00628A10(1, 0, &status)`
- if still valid and not vetoed:
  - call:
    - `FUN_0060BE80(owner)`

So this does not introduce a new channel, but it is valuable confirmation that:

- global message `7`
- owner-local `type_id 1`
- then owner-local `type_id 3`

is a stable transition pattern, not a single special-case helper.

That makes the current preflight/activation classification much harder to doubt.

## `FUN_006100A0(owner_id, type_id, arg3, arg4)`: generic extended-channel entrypoint

This helper is one of the more interesting architectural finds in the batch.

Its behavior is:

- require `owner_id != 0`
- require `type_id >= 7`
- validate the owner id
- then call:
  - `FUN_00628A10(type_id, arg3, arg4)`

So this looks like a deliberately generic front door for **extended typed channels**, but only for ids:

- `>= 7`

That matters a lot for the channel model.

Until now, the visible map looked like:

- `1` through `4`, then `6`

Now we know the compiled code also expects a broader extended range above that.

The most cautious interpretation is:

- `1..6` are the common built-in owner-local channels we’ve recovered in active use
- `>=7` are an additional typed-channel range exposed through a generic API/helper

We do not yet know which of those extended ids are actually used in this build, but the entrypoint is real.

## Updated channel map

With this pass added, the strongest current channel family is:

### `type_id 1`

- owner-local preflight / transition veto

### `type_id 2`

- owner-local setup / enter / initialization

### `type_id 3`

- owner-local activation / rebind / drain

### `type_id 4`

- owner-local relation/layout reevaluation

### `type_id 6`

- owner-local follow-up channel for the `0x3d` activation/add path

### `type_id >= 7`

- generic extended-channel range accepted by `FUN_006100A0(...)`

That leaves:

- `type_id 0`
- `type_id 5`

still unresolved from the “is this live and how is it used?” standpoint, even though the resolver supports them.

## Updated global-message map around the extended family

This pass also expands the visible global-message cluster:

- `0x07` = global preflight/veto
- `0x10` = global setup/enter
- `0x25` = active-owner changed
- `0x26` = global relation cleanup/reset
- `0x29` = global relation deactivation/clear
- `0x3d` = add/activate/register in the extended message family
- `0x3c` / `0x3f` = clear/remove/deactivate companions in that same family

And the important structural point is:

- `0x3d` is now explicitly paired with owner-local `type_id 6`

That means the broader frame/relation system contains more than one global-message subfamily.

## Best current interpretation

The strongest safe reading after this pass is:

- the GWCA-hooked frame/relation seam contains a full owner lifecycle for at least one core family (`1..4`)
- plus at least one additional message-coupled owner channel (`6`)
- plus an exposed extended range (`>=7`)

So the system is not just a single fixed lifecycle. It is a richer relation-channel framework with multiple message families and owner-local follow-up channels.

## Best next step

The next best reverse step is to answer the two remaining structural gaps:

1. are `type_id 0` and `type_id 5` actually used in this build?
2. what, if anything, calls the generic `type_id >= 7` entrypoint?

The highest-yield targets now are:

- callers of `FUN_006100A0`
- callers of `FUN_0062D220` filtered for `type_id 0` / `5`
- callers or xrefs around the `0x3d` / `0x3c` / `0x3f` family

That should let us decide whether the unresolved channels are dead scaffolding, niche runtime paths, or still-important parts of the frame/relation engine.
