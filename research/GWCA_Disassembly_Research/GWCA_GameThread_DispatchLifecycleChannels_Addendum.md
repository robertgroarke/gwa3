# GWCA GameThread Dispatch Lifecycle Channels Addendum

This pass starts the concrete 30-function queue by decompiling the first batch targets:

- `FUN_006286D0`
- `FUN_00628A10`
- `FUN_00628740`
- `FUN_0062DAE0`
- `FUN_0060D300`
- `FUN_0060BE80`
- `FUN_0060BCB0`
- `FUN_0060BD30`
- `FUN_006A5740`
- `FUN_006848D0`

The goal was to convert the first queue batch into a usable structural result instead of a loose target list.

## Main Result

The first batch now reads as one coherent owner/frame lifecycle.

The strongest current split is:

- `FUN_006286D0(...)`
  - global ordered message dispatch
- `FUN_00628A10(...)`
  - owner-local typed-channel dispatch
- `FUN_00628740(...)`
  - one-record callback executor
- `FUN_0062DAE0(...)`
  - recover containing frame-like base object
- `FUN_0060BCB0(...)`
  - veto-gated preflight entry
- `FUN_0060D300(...)` / `FUN_0060BD30(...)`
  - setup/enter wrappers
- `FUN_0060BE80(...)`
  - activation/rebind/drain path

So the seam GWCA sits above is now best described as:

- a frame-local owner lifecycle split across
  - global broadcasts
  - owner-local typed channels
  - ordered callback-record dispatch

not:

- a flat callback family
- or a generic UI-update helper layer

## Fresh Structural Results

### `FUN_006286D0(...)`: first-stage ordered dispatch

Fresh decompilation confirms:

- message ids `9` and `0x0B` are hard-fail/assert cases
- other messages walk a backward `0x0C` record array
- records with null callback or non-negative gate/order fields are skipped
- eligible records are executed through:
  - `FUN_00628740(...)`

So `FUN_006286D0(...)` is a real first-stage ordered callback bus.

### `FUN_00628A10(...)`: owner-local second-stage dispatch

Fresh decompilation confirms:

- it first executes a local `0x32` pass over its own listener table
- that pass can veto later work through `local_8`
- if not vetoed and the owner/handle still validates, it recovers a containing base through:
  - `FUN_0062DAE0(...)`
- then walks the recovered base object's second table at:
  - `+0xA8`
  - count at `+0xB0`
- and executes a final `0x31` pass

So `FUN_00628A10(...)` is not a generic event fan-out.

It is a:

- gated owner-local typed-channel dispatcher

with:

- early phase `0x32`
- final phase `0x31`

### `FUN_00628740(...)`: exact callback ABI

Fresh decompilation reconfirms and sharpens the executor model:

- `record + 0x00`
  - callback function pointer
- `record + 0x04`
  - callback-associated pointer copied into context
- `record + 0x08`
  - scalar metadata copied into context

The callback receives:

1. a synthesized local context block
2. payload
3. auxiliary argument

The context includes:

- message id
- owner-derived fields from the dispatcher object
- record-local metadata
- record index

So the dispatch layer has a real callback ABI, not a minimal function-pointer call.

### `FUN_0062DAE0(...)`: containing-base recovery

Fresh decompilation remains tiny and decisive:

- if `*this == 0`, return `0`
- else return:
  - `*this - 0x128`

So the second-stage listener tables are not owned by a registry singleton.

They belong to:

- a containing frame-like base object recovered from an embedded interior pointer

## Lifecycle Channel Recovery

The most useful new classification result in this batch comes from:

- `FUN_0060BCB0`
- `FUN_0060D300`
- `FUN_0060BD30`
- `FUN_0060BE80`

### `FUN_0060BCB0(owner)`: preflight/veto gate

Fresh decompilation shows this sequence:

1. emit global message:
   - `FUN_006286D0(7, 0, &status)`
2. if owner still valid and no veto:
   - `FUN_00628A10(1, 0, &status)`
3. if owner still valid and still no veto:
   - continue into:
     - `FUN_0060BE80(owner)`

That gives the cleanest current reading:

- global message `7`
  - global preflight/veto broadcast
- typed channel `1`
  - owner-local preflight/veto channel

### `FUN_0060D300(...)` and `FUN_0060BD30(...)`: setup/enter wrappers

These two functions now read as near-siblings.

Shared tail:

- broad setup/reset/init work
- `FUN_0062E5D0(4, 0, 0)`
- `FUN_00628A10(2, 0, 0)`
- `FUN_006286D0(10, 0, 0)`

The main difference is source selection:

- `FUN_0060D300(...)`
  - resolves the owner/handle through:
    - `FUN_0062DC20()` when `param_1 == 0`
    - `FUN_00628800(param_1)` otherwise
- `FUN_0060BD30(...)`
  - takes the already-resolved owner directly

So the current best reading is:

- typed channel `2`
  - owner-local setup/enter channel
- global message `10`
  - global setup/enter broadcast

### `FUN_0060BE80(owner)`: activation/rebind/drain

Fresh decompilation confirms:

- validate category/flag state through:
  - `FUN_0062E640(4)`
  - `FUN_0062E640(8)`
- raise owner flag/category `8` via:
  - `FUN_0062E5D0(8, 0, 0)`
- if category `8` was not already active:
  - `FUN_00628A10(3, 0, 0)`
- then iterate channel members through:
  - `FUN_0062D220(3, 0)`
- recursively process them
- then run the large refresh cascade

That makes typed channel `3` now read very strongly as:

- activation / rebind / drain

not just:

- “some later phase”

## Best Current Channel Map

After this batch, the strongest current map is:

### Global message plane

- `7`
  - preflight/veto broadcast
- `10`
  - setup/enter broadcast
- `0x25`
  - active-owner changed
- `0x26`
  - relation cleanup/reset broadcast
- `0x29`
  - relation deactivation/clear broadcast

### Owner-local typed channel plane

- `1`
  - preflight/veto
- `2`
  - setup/enter
- `3`
  - activation/rebind/drain
- `4`
  - relation/layout reevaluation

This is now a much cleaner lifecycle than the older “typed phases 1/2/3/4” phrasing.

## Transfer-Bank Pair In This Batch

The two transfer-bank targets from batch 1 were still worth refreshing:

- `FUN_006A5740(...)`
- `FUN_006848D0(...)`

But the fresh decompilation mainly reconfirms the existing model rather than changing it.

### `FUN_006A5740(...)`

Still reads as:

- the shared lower-family color-table emitter
- threshold-driven active-pixel selection
- endpoint/selector search over a BC1-like lower-family lane

### `FUN_006848D0(...)`

Still reads as:

- byte-packed scalar-plus-color feeder
- one-input local rebuild
- repack through:
  - `FUN_006A5CB0(..., 0, 1)`

So this batch does not overturn the earlier family-`0` / family-`1` emitter split.

It strengthens confidence in it.

## What This Batch Closed

The first queue batch closes three practical gaps:

1. the first-stage and second-stage dispatchers now form one concrete model
2. the owner lifecycle has named preflight, setup, and activation channels
3. the lower-family transfer seam remains stable under fresh decompilation

That is enough to move batch 2 from “speculative follow-up” to “direct continuation.”

## Best Next Step

The strongest next move is now batch 2 of the concrete queue:

- `FUN_0062D5C0`
- `FUN_00624700`
- `FUN_006287D0`
- `FUN_0062F7E0`
- `FUN_0060DAA0`
- `FUN_006100A0`
- `FUN_0060C3A3`

Those should answer:

- whether any typed channels beyond `1..4` are real and live
- how global relation-message packing maps onto the lifecycle just recovered
- where the higher-level guard/veto wrapper sits relative to the newly sharpened dispatch model

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_batch1_queue_temp182.log`
- earlier supporting context:
  - `tools/ghidra_projects/gw_decomp_006a5cb0_temp180.log`
  - `tools/ghidra_projects/gw_decomp_006a5740_temp181.log`
