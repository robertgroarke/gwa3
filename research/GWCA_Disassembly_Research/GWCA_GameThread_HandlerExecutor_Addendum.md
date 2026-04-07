## `Gw.exe` Frame Callback Handler Executor Addendum

This pass continues one level below the dispatch-semantic layer by decompiling:

- `FUN_00628740`
- `FUN_0062DAE0`

These were the highest-value unresolved helpers from the previous addendum because they control:

- how a single listener record is actually invoked
- what object owns the `0x31` / `0x32` secondary listener tables

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_dispatch_executor_temp65.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_dispatch_executor_temp65.log)

## High-level result

This pass answers the two open questions pretty directly.

First:

- the 3-word listener records are real callable records
- field `+0x00` is the function pointer
- field `+0x04` and `+0x08` are copied into the callback context before invocation

Second:

- `FUN_0062DAE0()` is not a manager lookup or registry walk
- it is a tiny pointer-recovery helper:
  - if `*this != 0`, return `*this - 0x128`

That means the `0x31` / `0x32` secondary passes are rooted on a recovered frame-like base object, not a generic subsystem singleton.

## `FUN_00628740(record, msg_id, payload, arg)`: listener-record executor

This helper is much cleaner than its callers made it look.

Its behavior is:

- require `record != null`
- require `record[0] != 0`
- build a small local context block
- invoke the function pointer stored at `record[0]`

### Local callback context layout

The function fills the local block like this:

- `local_18 = msg_id`
- `local_1c = this[5]`
- `local_10 = this[0x3A]`
- `local_14 = record + 1`
- `local_c  = record[2]`
- `local_8  = ((record - *this) / 0x0C)`

Then it calls:

- `record[0](&local_1c, payload, arg)`

That makes the listener-record structure much more concrete.

### Recovered record shape

The strongest current reading of each `0x0C` record is:

- `+0x00` = callback function pointer
- `+0x04` = callback-associated data pointer or descriptor
- `+0x08` = scalar metadata copied into the per-call context

The exact meaning of `+0x08` is still open, but it is definitely not the same field as the ordering/altitude slot that the outer iterators were filtering on. That means the higher-level tables likely contain:

- a contiguous `0x0C` callback record array
- plus a separate outer container field that the dispatchers use for ordering

In other words, the previous “3-word record with one altitude-like field” model was too compressed. `FUN_00628740` shows the invoked record itself is a callback triple, and the outer caller’s backward-walk/`piVar[-1]` logic is reading around that storage rather than proving that `record[2]` is an altitude value.

### What the callback receives

The invoked function gets three arguments:

1. a pointer to the locally built context block
2. the payload pointer/object forwarded from the dispatch path
3. the caller-supplied auxiliary argument

That means the dispatch layer is not just doing `callback(msg, payload)`.

It is providing:

- message id
- owner/context pointers from the dispatcher object
- the callback’s own per-record metadata
- the callback’s positional index inside the record table

This is a real event-delivery ABI, not a casual helper call.

## `FUN_0062DAE0()`: frame-like base recovery

This function is tiny:

```cpp
return -(uint)(*this != 0) & *this - 0x128U;
```

In plain terms:

- if `*this == 0`, return `0`
- otherwise return `*this - 0x128`

That is a very strong shape. It means the current object stores a pointer into some interior subobject, and `FUN_0062DAE0()` recovers the full containing base by subtracting `0x128`.

### Why this matters

The previous addendum described `FUN_00628A10()` as:

- call `FUN_0062DAE0()`
- then use the returned object’s listener tables at `+0xA8` / `+0xB0`

Now we know that object is not being looked up through a registry. It is being recovered through a fixed negative offset.

That is exactly the sort of pattern you expect when:

- code holds a pointer to an embedded frame/component field
- and needs to recover the full parent frame object

So the best current interpretation is:

- the second-stage `0x31` / `0x32` dispatches are owned by a specific frame-like object
- and `FUN_0062DAE0()` is the “get containing frame/base” helper for that object

This is structurally very similar to the kind of parent/base recovery we already saw in the GWCA-side frame work, where parent/base helpers also worked by subtracting a fixed structural offset from an embedded pointer.

## What this changes about the dispatch model

### Earlier model

Before this pass, the safe description was:

- `FUN_006286D0` and `FUN_00628A10` walk ordered handler tables
- `FUN_00628740` probably executes one handler record
- `FUN_0062DAE0` probably finds the owner of the second-stage tables

That was directionally right but still vague.

### Stronger current model

This pass tightens it to:

- the listener tables contain callable `0x0C` records
- each record is invoked through a function pointer at `+0x00`
- the dispatcher synthesizes a richer per-call context object containing:
  - message id
  - owner-derived fields
  - record-local metadata
  - record index
- the second-stage listener tables are owned by a recovered frame-like base object reached through `embedded_ptr - 0x128`

That makes the secondary path under `FUN_00628A10` look much less like a generic subsystem event and much more like a frame/component-local callback plane.

## Revised end-to-end picture

With this pass added, the strongest current picture for the message branch is:

1. compute bounded coordinates in the frame callback descendant path
2. subtract origin and optionally convert to centered normalized coordinates
3. build a packet-like local state block
4. send it through an ordered first-stage listener bus
5. for active/register-style operations, recover a frame-like base object
6. run a gated `0x32` phase
7. if not vetoed, run a final `0x31` phase
8. each individual handler receives a synthesized callback context plus payload/arg

So the message branch now looks like:

- **frame-local event dispatch over callable records**

not:

- a single flat packet sink
- or a purely global subsystem bus

## Best next step

The next best reverse step is to identify the handler implementations themselves and the container shape around the callable record array.

The highest-value nearby targets are:

- callers or xrefs to `FUN_00628740`
- the allocator/builder for the `0x0C` callable records
- the object layout around the recovered base from `FUN_0062DAE0()`

Most specifically, the next useful pass is:

- trace the object at `+0xA8` / `+0xB0`
- and inspect the object owning `*this` in `FUN_0062DAE0()`

That should answer whether the `0x31` / `0x32` phases belong to a particular frame class, a viewport component, or another embedded UI object.
