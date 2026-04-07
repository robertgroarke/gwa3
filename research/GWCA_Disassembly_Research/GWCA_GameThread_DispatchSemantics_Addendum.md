## `Gw.exe` Frame Callback Dispatch Semantics Addendum

This pass continues directly from the message-path addendum by tracing the next four high-value helpers underneath that branch:

- `FUN_006286D0`
- `FUN_00628A10`
- `FUN_00629A00`
- `FUN_006298F0`

The goal here was to answer the next obvious questions:

1. what kind of dispatcher is actually consuming the packet-like state from `FUN_00624900`?
2. what coordinate space conversion is happening before those packets are sent?

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_message_dispatch_temp64.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_message_dispatch_temp64.log)

## High-level result

This pass sharpens the model in three useful ways.

First:

- `FUN_006286D0` is not a raw “send packet to engine” leaf.
- it is an **ordered listener/handler dispatcher** over a `(callback, altitude)` style table.

Second:

- `FUN_00628A10` is not just “second dispatch happens if valid.”
- it is a **gated secondary fan-out** that first runs message `0x32` against one listener table and, if not blocked, runs message `0x31` against another one.

Third:

- the coordinate transforms are now much clearer.
- `FUN_00629A00` subtracts an origin/bounds minimum.
- `FUN_006298F0` then converts the result into `[-1, 1]` normalized coordinates.

So the branch under the frame callback now looks less like a vague commit path and more like:

- local coordinate pair
- subtract frame/viewport origin
- optionally convert into normalized device-like coordinates
- build a message record
- fan it through ordered handlers

## `FUN_00629A00(out, in)`: subtract local origin

This helper is tiny but very revealing.

It does:

- `out[0] = in[0] - *(this + 0x1C)`
- `out[1] = in[1] - *(this + 0x20)`

That is not an arbitrary transform. It is a straight subtraction of a stored origin or lower bound.

So the first stage in `FUN_00624900(...)` is:

- convert absolute/scaled coordinates into a local coordinate space

The strongest interpretation is:

- subtract current frame or viewport minimums

not:

- rotate
- project
- or do any physics-style movement math

## `FUN_006298F0(out, local_pair)`: convert local coordinates into `[-1, 1]`

This helper is the second half of the transform chain.

It first validates the current bounds:

- `this + 0x1C <= this + 0x24`
- `this + 0x20 <= this + 0x28`

Then it computes:

- `x_norm = local_x / (max_x - min_x)`
- `y_norm = local_y / (max_y - min_y)`

and maps each one into:

- `(norm * 2.0) - 1.0`

That is a textbook conversion into a centered `[-1, 1]` space.

So the transform stack recovered so far is:

1. scale incoming stored values
2. subtract an origin/minimum with `FUN_00629A00`
3. optionally convert to centered normalized coordinates with `FUN_006298F0`

That is much more specific than the earlier “normalized coordinate path” shorthand. It strongly suggests a UI/frame/viewport coordinate convention rather than world movement.

## `FUN_006286D0(msg_id, payload, arg)`: ordered handler dispatch

This helper explains the first dispatch out of `FUN_00624900(...)`.

Its shape is:

- reject message `9` and `0x0B` with hard assertions
- otherwise iterate a table held in `in_ECX`
- each entry appears to be a 3-word record
- iterate backward from the end
- skip empty entries and skip entries whose priority/altitude is non-negative
- call:
  - `FUN_00628740(record, msg_id, payload, arg)`

So the important semantic point is:

- this is not a single destination
- it is a **dispatch over registered handlers**

The record layout is not fully named yet, but the usage is very suggestive:

- one field is a presence pointer or callback target
- one field behaves like an ordering/altitude discriminator
- negative values run in this pre-pass

That makes `FUN_006286D0` look much more like:

- a prioritized event bus
- or a callback plane

than a plain packet sink.

## `FUN_00628A10(type_id, payload, arg)`: gated secondary fan-out

This helper is the second dispatch stage that `FUN_00624900(...)` only performs for message `0x24`.

Its structure is:

1. call `FUN_0062DAE0()`
2. if that succeeds:
   - build a local control block carrying:
     - `type_id`
     - `payload`
     - `arg`
     - `in_ECX[4]`
     - `in_ECX[5]`
   - initialize `local_8 = 0`
3. if the first listener table is non-empty:
   - iterate it backward in the same style as `FUN_006286D0`
   - call:
     - `FUN_00628740(record, 0x32, &local_1c, &local_8)`
4. if the id in `in_ECX[5]` validates through the registry layer and `local_8 == 0`:
   - iterate a second listener table from the object returned by `FUN_0062DAE0()`
   - call:
     - `FUN_00628740(record, 0x31, &local_1c, 0)`

This gives us a much stronger model for the “secondary dispatch” stage:

- message `0x32` is a first, blockable/gating pass
- message `0x31` is the follow-up pass if validation succeeds and the first pass did not veto it

The exact names of `0x31` and `0x32` are still unresolved, but the control pattern is now clear:

- **preflight/gate**
- then **final fan-out**

That resembles the GWCA-side callback patterns we already recovered in `gwca.dll`, which is a nice structural echo even though this is the game-side image.

## What this does to the message-id interpretation

### `0x24`, `0x2D`, `0x2E`

From the previous addendum:

- `0x24` inserts/activates and then dispatches
- `0x2E` clears/deactivates and then dispatches
- `0x2D` skips the float-transform path

This pass does not fully rename them, but it improves the framing:

- they are not just “engine message ids”
- they are ids processed by an ordered listener system

So the best current reading is:

- `0x24` = activate/register + emit/update
- `0x2E` = deactivate/unregister + emit/update
- `0x2D` = emit/update using already-local/raw coordinates

That is still somewhat inferential, but it is now grounded in the actual handler-flow beneath them.

### `0x31` and `0x32`

These now appear as a second message family in the same branch.

The safest current description is:

- `0x32` = preflight/gating notification over one listener set
- `0x31` = post-validation/final notification over another listener set

The important part is not the provisional names. It is the fact that the branch is using **two separate handler planes** with a blockable intermediate state.

## Updated end-to-end model

With this pass added, the strongest current model for the `FUN_0061AD80` descendant branch is:

1. compute/update a bounded coordinate pair
2. scale it into the current UI/frame space
3. subtract origin/minimum with `FUN_00629A00`
4. optionally convert to centered normalized coordinates with `FUN_006298F0`
5. assemble a packet-like state block in `FUN_00624900`
6. send it through an ordered handler bus with `FUN_006286D0`
7. for the active/register path, run a gated secondary notification chain with `FUN_00628A10`
8. separately build/link a rect/state object through `FUN_0062B0B0`

That is now clearly a control/message system inside the frame/update layer.

## Best current interpretation

The strongest safe interpretation after this pass is:

- the callback GWCA hooks for `GameThread` sits above a UI/frame-space coordinate submission system
- that system uses origin-relative and optionally `[-1, 1]` normalized coordinates
- and it routes updates through ordered handler tables rather than one monolithic consumer

So the branch is now better described as:

- **frame-space coordinate/update dispatch with preflight and final notification phases**

not:

- movement
- camera control
- or a single raw packet emitter

## Best next step

The next best reverse step is to identify the handler-record executor and the gating source:

- `FUN_00628740`
- `FUN_0062DAE0`

Those two targets should answer:

- what each 3-word listener record actually contains
- whether the negative-value field is really altitude/priority
- what subsystem owns the `0x31` / `0x32` second-stage tables
- and whether `local_8` is a true veto/block flag or a richer status code
