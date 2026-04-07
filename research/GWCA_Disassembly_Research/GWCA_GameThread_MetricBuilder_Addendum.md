## `Gw.exe` Frame Callback Metric And Builder Addendum

This pass continues from the provisional slot-role note by decompiling the layout and construction helpers directly underneath it:

- `FUN_0060FAC0`
- `FUN_0060F920`
- `FUN_0060FE70`
- `FUN_0060FA20`
- `FUN_0060D300`

The goal was to answer two narrower questions:

- what kind of metric/layout operations are the slot helpers really performing?
- what kind of object does the owner/control builder actually create?

## Source artifacts

These results come from:

- [gw_decomp_metric_builder_temp85.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_metric_builder_temp85.log)
- [gw_decomp_slot_roles_temp84.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_roles_temp84.log)

## High-level result

This pass strengthens both halves of the current model.

First, the metric helpers are clearly rectangle/extent combiners, not generic callback traffic:

- `FUN_0060FE70` mutates a four-float rect by edge-mask bits
- `FUN_0060FA20` and `FUN_0060F920` accumulate consumed width/height
- `FUN_0060FAC0` is a thin metric-read wrapper into the shared reader `FUN_0060FB00(...)`

Second, `FUN_0060D300(...)` is now clearly an owner/control construction path in the frame/relation system:

- it references `P:\\Code\\Engine\\Frame\\FrApi.cpp`
- seeds the new object with style bits / flags
- runs a stack of frame-side init helpers
- installs callbacks/user payload
- sets owner flags
- emits owner-local type `2` and global message `10`
- and returns the new object's runtime id from `+0xBC`

So the strongest new conclusion is:

- the slot families really are measuring and configuring concrete frame/control objects
- and `FUN_0060D300(...)` is the primary constructor for those owner/control nodes

## `FUN_0060FE70(delta, edge_mask, rect)`: edge-based rect shrink/advance helper

This helper is foundational for the layout story.

Its input is:

- a scalar `delta`
- an edge mask
- a `rect` buffer of four floats

It updates the rect according to edge bits:

- bit `1`: move the top/first edge inward
- bit `4`: move the left/second edge inward
- bit `8`: move the opposite horizontal edge inward
- bit `0x10`: move the opposite vertical edge inward

The shape is unmistakably rect/box arithmetic:

- add a margin/inset
- clamp against the opposite edge
- preserve valid bounds

This confirms that the slot-role helpers were not only "querying children."
They were composing child extents into a mutable box.

## `FUN_0060FA20(delta, edge_mask, rect, accum)`: apply rect delta and accumulate usage

This helper wraps `FUN_0060FE70(...)`.

It:

- snapshots the old rect edges
- calls `FUN_0060FE70(...)`
- computes how much each side changed
- accumulates that consumed extent into `accum[0]` and `accum[1]`

So this is not just:

- mutate rect

It is:

- mutate rect
- and track how much width/height budget that mutation consumed

That fits the repeated row/entry family interpretation from the previous note.

## `FUN_0060F920(child, metric_id, rect, accum, scratch)`: metric-read plus accumulate

This helper calls the shared metric reader `FUN_0060FB00(...)`, then merges the returned metric values into an accumulation buffer.

Two behaviors matter:

- if the metric id's sign bit is set, one dimension is added directly
- otherwise it behaves like a max/extent combiner
- if metric id has `0x100`, the secondary dimension is additive
- otherwise it is max-clamped

So `FUN_0060F920(...)` is a:

- query-child-metric
- then merge that metric into current layout usage

helper.

That is exactly the kind of thing we expected if the repeated slot family were rows or peer entries in a composed UI layout.

## `FUN_0060FAC0(child, metric_id, rect, scratch)`: thin metric-read wrapper

This helper is simple:

```cpp
FUN_0060FB00(child, metric_id, rect, scratch, 1);
```

So `FUN_0060FAC0(...)` is the read-only metric fetch form, while:

- `FUN_0060F920(...)`

is the accumulate-and-merge form.

That lines up nicely with the previous slot-role note:

- some slot paths just fetch a child's contribution
- others compose multiple contributions into a final width/height pair

## `FUN_0060D300(...)`: owner/control constructor in the frame/relation system

This is the strongest result in the batch.

At a high level, it does all of the following:

1. resolve the parent owner/context through:
   - `FUN_00628800(parent_id)` or `FUN_0062DC20()`
2. allocate/get a new object through:
   - `FUN_0047EF00("P:\\Code\\Engine\\Frame\\FrApi.cpp", 0x10F)`
3. run a long sequence of frame-side init helpers:
   - `FUN_00614CB0()`
   - `FUN_006161A0()`
   - `FUN_00618020()`
   - `FUN_00619400()`
   - `FUN_0061A4F0()`
   - `FUN_0061EEF0()`
   - `FUN_006240D0()`
   - `FUN_00627740(type_id)`
   - `FUN_00629190()`
   - `FUN_0062C650(parent, type_id, arg6)`
   - `FUN_0062E520()`
   - `FUN_00613400(parent)`
   - `FUN_0060BBD0()`
   - `FUN_0062EFE0()`
   - `FUN_0062F580()`
4. write `param_2` into object offset `+0x190`
5. write callback/user payload via:
   - `FUN_00627D70(param_4, param_5)`
6. set owner flags via:
   - `FUN_0062E5D0(4, 0, 0)`
7. emit owner-local and global notifications:
   - `FUN_00628A10(2, 0, 0)`
   - `FUN_006286D0(10, 0, 0)`
8. return the new object's runtime id from `+0xBC`

That is much stronger than the earlier "maybe builder" wording.

This is the constructor path for a concrete frame/relation owner object.

## What `param_2` and `param_3` now look like

The layout and builder passes together sharpen two constructor arguments.

### `param_2` -> object flag/style word at `+0x190`

`FUN_0060D300(...)` writes:

- `*(obj + 0x190) = param_2`

And the previous pass showed:

- `FUN_00610EA0(obj, mask)` reads `*(obj + 0x190) & mask`

So `param_2` is now cleanly identified as:

- initial style / feature / mode flags for the new control object

### `param_3` -> owner-local type id

`FUN_0060D300(...)` passes `param_3` through:

- `FUN_00627740(param_3)`
- `FUN_0062C650(parent, param_3, arg6)`

and then emits:

- owner-local channel `2`
- global message `10`

So `param_3` still reads best as the constructed object's local type/category id inside the frame/relation system.

## What this does to the provisional slot-role note

The previous note proposed that:

- slots `2/4/5/6` are peer entry-like children
- slot `0` is an optional auxiliary child
- slot `1` is a final sink child
- slot `3` is a mode/detail child

This pass makes that model more credible because the helpers underneath are now clearly:

- box/rect layout arithmetic
- child metric querying
- child metric accumulation

not:

- arbitrary message routing

So the slot-role note is now standing on real layout code, not only behavioral inference.

## Strongest current architectural picture

After this pass, the cleanest overall model is:

1. `FUN_0060D300(...)` constructs owner/control nodes in the frame/relation system
2. those owner/control nodes can have owner-attached slot children
3. slot children expose query/set verbs like:
   - `0x57`
   - `0x58`
   - `0x59`
   - `0x5A`
   - `0x5C`
4. owner/layout helpers fetch metrics from those slot children using ids like:
   - `0x89`
   - `0x94`
   - `0x105`
   - `0x114`
5. those metrics are merged through rect/extent helpers into final layout decisions

That is a much more concrete control/layout stack than we had a few notes ago.

## Best next step

The highest-yield next pass is to decompile the shared metric reader and a few of the frame-side constructor helpers:

- `FUN_0060FB00`
- `FUN_00627740`
- `FUN_0062C650`
- `FUN_00627D70`

That should answer two important remaining questions:

- what the metric ids like `0x89`, `0x94`, `0x105`, and `0x114` actually mean
- and what exact child/control type metadata `FUN_0060D300(...)` is attaching during construction
