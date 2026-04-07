## `Gw.exe` Downstream Identity Addendum

This pass continues the frame-callback reversing by tracing the concrete helpers named at the end of the previous addendum:

- `FUN_00624F40`
- `FUN_00624F80`
- `FUN_006108E0`
- `FUN_006446B0`
- `FUN_00645410`
- `FUN_006290B0`

The goal was to replace the last vague subsystem labels with something tighter.

## High-level result

This pass makes three useful corrections:

1. the `FUN_0061AD80` branch is not just “movement-like”
   - it is clearly a **normalized-coordinate commit path** that pushes state through message/commit helpers

2. the hierarchy cluster is not just “highlight-like”
   - it operates on a concrete object type tagged `0x67726d64`
   - and writes directly into per-element fields of that object

3. `FUN_006290B0` is a registry/validity gate on ids
   - not a large subsystem body by itself

That sharpens the frame callback model considerably.

## Decompiled helper set

These bodies come from:

- [gw_decomp_subsystem_identity_temp62.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_subsystem_identity_temp62.log)

## `FUN_00624F40()`: minimal commit trigger

This one is tiny:

```cpp
FUN_00626180(1);
FUN_00625650();
```

It is a pure trigger/commit wrapper.

Since `FUN_0061AD80` calls it after building normalized values like:

- `local_28 / local_30`
- `local_24 / local_2c`

the best reading is:

- “commit current normalized coordinate state”

not:

- a standalone movement routine

So `FUN_00624F40()` is a useful confirmation that `FUN_0061AD80()` is ending in a commit stage, not merely computing values internally.

## `FUN_00624F80(vec2_or_pair)`: structured state submission

This one is much richer and confirms the same direction.

It:

- checks `DAT_00BD0C40 == 1`
- uses both elements of the input pair
- pulls auxiliary globals:
  - `DAT_00BD0C28`
  - `DAT_00BD0C2C`
  - `DAT_00BD0C44`
- gets an additional value from `FUN_0061FCA0()`
- assembles a structured block
- validates it through `FUN_00624DC0(..., 0x40, ...)`
- and on success, if `FUN_0062E550()` says the channel is active:
  - calls `FUN_00624900(0x30, &local_24, 1, 0)`

This is strong evidence that `FUN_0061AD80()` is feeding a real:

- packed state packet
- or engine message/command object

based on normalized coordinate input.

That pushes the interpretation away from generic “camera math” and closer to:

- controlled coordinate submission
- cursor/focus/viewport-anchor style engine state

I’m still keeping the exact domain slightly open, but the “state submission” part is now clear.

## `FUN_006108E0(handle, pair_ptr)`: object + coordinate commit

This one is also compact:

```cpp
if (param_1 == 0) abort;
FUN_00628800(param_1);
FUN_0062b0b0(param_2);
```

So the role is:

- validate/activate an object/handle
- submit the coordinate pair

This supports the same conclusion as `FUN_00624F40` / `FUN_00624F80`:

- `FUN_0061AD80()` is not just updating local engine variables
- it is driving an object-bound coordinate commit path

## `FUN_006446B0(object_id, byte_value)`: byte-field propagation over concrete objects

This one is the clearest identity improvement for the hierarchy cluster.

It:

- resolves `object_id` through:
  - `FUN_0046F9B0(param_1, 0x67726d64)`
- asserts the resulting object type is `5`
- reads object count at `+0x9C`
- base pointer at `+0x94`
- then writes the incoming byte into each element:
  - `*(base + i * 0x7C - 0x4D) = param_2`

That means the hierarchy cluster is not operating on loose nodes in the abstract. It is operating on a specific object family with:

- a concrete runtime type
- a count of sub-elements
- fixed-size `0x7C` records
- a per-element byte field at offset `0x2F`

This is exactly the kind of structure you would expect for:

- per-part alpha/intensity
- per-part emphasis
- some other per-element visual-control byte

So the earlier “fade/highlight” reading holds up, but now it is grounded in a concrete object layout.

## `FUN_00645410(object_id, value32)`: parallel 32-bit field propagation

This is the sibling to `FUN_006446B0`.

It resolves the **same** object family:

- tag `0x67726d64`
- type `5`
- count at `+0x9C`
- record array at `+0x94`

but writes a 32-bit value instead:

- `*(base + i * 0x7C - 0x04) = param_2`

So the two helpers are clearly paired:

- one writes a byte field across all parts
- one writes a dword field across all parts

That makes `FUN_00617AC0` and `FUN_00617EC0` much less mysterious:

- they are applying global visual/state changes across every element in a concrete runtime object

The exact object name is still not recovered from strings, but this is no longer just “hierarchy fade” in the abstract. It is a real engine object with repeated per-element state.

## `FUN_006290B0(id)`: registry / validity gate

This one is simpler than the surrounding callers might have suggested:

```cpp
if ((id < DAT_00BB562C) && (*(int *)(DAT_00BB5624 + id * 4) != 0)) {
    if (FUN_00627A50(id) != -1) {
        return 1;
    }
}
return 0;
```

So its role is:

- validate that an id is in range
- ensure the registry slot is populated
- then confirm an additional lookup/index succeeds

This is useful because it tells us that when the parent helpers call `FUN_006290B0(...)`, they are not entering a big subsystem. They are just checking whether a referenced object/context is currently valid/registered.

## What this does to the earlier subsystem guesses

### The `FUN_0061AD80` branch

Before this pass, the best label was something like:

- smooth bounded movement/focus update

Now the stronger label is:

- **object-bound normalized coordinate update with commit/packet emission**

Why:

- it computes bounded normalized pairs
- commits them through `FUN_006108E0`
- flushes them through `FUN_00624F40`
- and can emit a structured state object through `FUN_00624F80`

That is more specific and more defensible.

### The hierarchy cluster

Before this pass, the best label was:

- hierarchy fade/highlight/selection

Now the stronger label is:

- **per-element visual/state propagation over a concrete type-5 object family**

Why:

- both downstream writers resolve the same typed object
- both iterate its per-element array
- both write to fixed offsets inside `0x7C`-byte records

The exact semantic names of those fields are still unresolved, but the mechanism is now concrete.

## Strongest current model

The per-frame callback GWCA hooks now looks like this:

1. validate/clamp elapsed frame time
2. emit frame/update event markers
3. run a context-lifetime pass
4. run a timed scheduler/interpolator
5. advance frame-cache / viewport maintenance
6. advance object-bound normalized coordinate state
7. propagate per-element visual/state changes across concrete objects
8. flush auxiliary dirty/selection/focus state

That is a very rich engine maintenance phase, which explains why GWCA’s deferred execution model works well there.

## What remains unresolved

Two exact names are still missing:

- what object family `0x67726d64` actually represents
- what user-visible concept the normalized coordinate path manipulates

But the uncertainty is now much narrower than before.

## Best next reverse step

The next best step is to attack the two unresolved domains directly:

- the object tag path:
  - `FUN_0046F9B0(..., 0x67726d64)`
  - nearby object-type / tag naming helpers
- the normalized coordinate path:
  - `FUN_00624900`
  - `FUN_00624DC0`
  - `FUN_0062B0B0`
  - `FUN_00628800`

That should tell us:

- what the type-5 object family actually is
- and whether the normalized pair is camera, cursor, viewport anchor, or some other focus/control vector

## Supporting artifacts

- [GWCA_GameThread_SubsystemIdentity_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_GameThread_SubsystemIdentity_Addendum.md)
- [gw_decomp_subsystem_identity_temp62.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_subsystem_identity_temp62.log)
