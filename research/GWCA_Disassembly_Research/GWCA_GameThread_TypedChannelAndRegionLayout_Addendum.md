# GWCA GameThread Typed Channel And Region Layout Addendum

This pass continues the concrete 30-function queue with the second batch:

- `FUN_0062D5C0`
- `FUN_00624700`
- `FUN_006287D0`
- `FUN_0062F7E0`
- `FUN_0060DAA0`
- `FUN_006100A0`
- `FUN_0060C3A3`
- `FUN_005F00F0`
- `FUN_005F0970`
- `FUN_005EFC20`

The goal was to tighten the typed-channel plane and the recursive region/layout plane at the same time.

## Main Result

Batch 2 does two important things.

First, it proves the typed-channel system is broader than the earlier `1..4` lifecycle map:

- `FUN_0062D5C0(...)` has real resolver cases through at least:
  - `type_id 7`
- `FUN_0062F7E0(...)` uses:
  - `FUN_00628A10(6, ...)`
- `FUN_006100A0(...)` is an explicit wrapper for:
  - `FUN_00628A10(type_id >= 7, ...)`

So the owner-local typed-channel plane does not stop at:

- preflight
- setup
- activation
- relation/layout

It extends into a second higher-numbered band.

Second, it closes the main remaining structural gap under the interactive region table:

- `FUN_005EFC20(...)`
  - child measurability / availability gate
- `FUN_005F0970(...)`
  - recursive size resolver
- `FUN_005F00F0(...)`
  - matching rect-placement / region-resolution walker

That makes the region subsystem now read as:

- recursive size negotiation
- plus rect placement
- on top of a child-region descriptor table

not just:

- recursive slot lookup

## Typed-Channel Side

### `FUN_0062D5C0(...)`: resolver cases now visible through `0..7`

Fresh decompilation shows that `FUN_0062D5C0(type_id, seed_ptr)` is not just a small `3/4` helper.

It has concrete resolver cases for:

- `0`
- `1`
- `2`
- `3`
- `4`
- `5`
- `6`
- `7`

These cases walk several embedded heads/tables off the owning object:

- `+0x18`
- `+0x1C`
- `+0x20`
- `+0x24`

and repeatedly follow linked subobjects while checking a disabled/dead bit at:

- `node + 4`

So the best current interpretation is:

- low `type_id`s are not arbitrary phase numbers
- they select different owner-local relation/subobject families
- and at least `5`, `6`, and `7` are structurally real channels, not dead enum space

### `FUN_00624700(...)`: global relation-message forwarder reconfirmed

Fresh decompilation stays aligned with the earlier model:

- package scaled/transformed coordinates
- include relation globals and one state bit
- optional centered normalization through:
  - `FUN_006298F0(...)`
- emit through:
  - `FUN_006286D0(msg_id, &local_38, 0)`

This remains the cleanest evidence that:

- `0x25`, `0x26`, `0x29`

live on the:

- global relation-message plane

not the owner-local typed-channel plane.

### `FUN_006287D0(...)`: guard before higher-level owner forwarding

Fresh decompilation remains tiny and useful:

- gate with:
  - `FUN_0048F830(&arg0)`
- if allowed:
  - forward through `FUN_00628570(...)`

So this remains:

- a higher-level gated wrapper

above:

- raw global dispatch
- and above owner-local typed-channel dispatch

### `FUN_0062F7E0(...)`: typed channel `6` is live

This is the biggest new channel discovery in batch 2.

Fresh decompilation shows:

- messages `0x3D`, `0x3F`, and `0x3C`
  - maintain a membership/registration bitfield
- all three also package a coordinate-bearing local payload
- send it through:
  - `FUN_006286D0(param_1, local_38, param_5)`
- and, when the owner/handle still validates and `param_1 == 0x3D`,
  - call:
    - `FUN_00628A10(6, local_38, param_5)`

That means:

- `type_id 6`
  - is definitely live
  - and it belongs to a registration/append style event family tied to message `0x3D`

The current safe reading is:

- owner-local registration / membership channel

or:

- owner-local append/attach notification channel

### `FUN_0060DAA0(...)`: bulk activation/drain sweep

This function does not introduce a new typed channel.

Instead it:

- validates a supplied owner
- repeatedly walks:
  - `FUN_0062D220(3, 0)`
- and replays the same activation/rebind/drain machinery already seen in:
  - `FUN_0060BE80(...)`

So this is best described as:

- bulk activation/drain sweep over typed channel `3`

not a new channel identity.

### `FUN_006100A0(...)`: explicit high-channel wrapper

This is the other major channel discovery in batch 2.

Its behavior is simple but decisive:

- assert `owner != 0`
- assert `type_id >= 7`
- validate the owner through:
  - `FUN_00628800(...)`
- then call:
  - `FUN_00628A10(type_id, payload, arg)`

That proves:

- typed channels `7+`
  - are intentional
  - and have a dedicated wrapper path

So the channel map now clearly extends above the earlier lifecycle band.

### `FUN_0060C3A3(...)`: default/current-owner preflight gate

This function is essentially the current-owner sibling of:

- `FUN_0060BCB0(owner)`

It:

- resolves the current owner through `FUN_00628800()`
- performs global preflight:
  - `FUN_006286D0(7, 0, &status)`
- performs owner-local preflight:
  - `FUN_00628A10(1, 0, &status)`
- if not vetoed:
  - enters:
    - `FUN_0060BE80(owner)`

So this does not create a new channel.

It strengthens the preflight reading by proving there is both:

- an explicit-owner path
- and a current-owner path

## Updated Channel Map After Batch 2

The best current typed-channel picture is now:

### Lifecycle band

- `1`
  - preflight / veto
- `2`
  - setup / enter
- `3`
  - activation / rebind / drain
- `4`
  - relation / layout reevaluation

### Extended band

- `6`
  - registration / membership / append-style owner-local event
- `7+`
  - definitely real and intentionally wrapped, exact semantics still pending

That is the main architectural gain from the typed-channel half of batch 2.

## Region / Layout Side

### `FUN_005EFC20(...)`: child measurability gate

Fresh decompilation confirms:

- only layout mode `1` uses the live-child measurability path
- resolve child by slot through:
  - `FUN_0060E2B0(owner_id, child_slot_id)`
- ask whether the child is measurable/live through:
  - `FUN_0060F5C0(child)`
- if not measurable:
  - fall back to a record flag nibble

So this remains best described as:

- child measurability / availability gate

### `FUN_005F0970(...)`: recursive size resolver

Fresh decompilation confirms and sharpens the earlier mode map.

#### Mode `0`

- recursively visits following children
- enforces flag family:
  - `0x53`
- accumulates one dimension as max
- conditionally consumes the other

Best reading:

- stacked aggregate on one axis

#### Mode `1`

- resolve live child
- pass availability gate
- measure through:
  - `FUN_0060E4C0(...)`

Best reading:

- measured live-child layout

#### Mode `2`

- call a virtual/policy callback through the owner
- let returned mask bits choose which dimension is meaningful

Best reading:

- policy / virtual layout

#### Mode `3`

- sibling of mode `0`
- enforces flag family:
  - `0x2E`
- flips which axis is max vs consumed

Best reading:

- stacked aggregate on the orthogonal axis

### `FUN_005F00F0(...)`: rect placement over the same tree

Fresh decompilation shows the higher-level rect walker is the placement companion to `FUN_005F0970(...)`.

It:

- resolves current child record
- builds a working rectangle
- applies clamp/percent/fill-style flags
- uses:
  - `FUN_005F0970(...)`
  - `FUN_0060FE70(...)`
  - `FUN_005319B0(...)`
- recursively descends through children
- writes the final placed rect back to the caller

So the current region-table pair now reads as:

- `FUN_005F0970(...)`
  - subtree size negotiation
- `FUN_005F00F0(...)`
  - subtree rect placement / resolution

That is a stronger and cleaner model than the earlier “recursive region walker” wording alone.

## Best Current Interpretation

After batch 2, the broader seam now looks like:

- global relation messages
- owner-local typed channels
- a higher-numbered extended typed-channel band
- and a recursive child-region layout engine with:
  - size negotiation
  - measurability gating
  - rect placement

So the frame/control subsystem is now best described as:

- owner lifecycle plus recursive layout negotiation

not:

- a flat callback network
- and not:
  - a passive slot table

## Best Next Step

The strongest follow-on is now exactly what batch 3 was meant to test:

- whether the text/generation path exposes the high typed channels
- and whether the compressed-family seam produces another real branch distinction

The most useful fresh cross-checks after this pass are:

- `FUN_005F8810`
- `FUN_00610F60`
- `FUN_0060CE70`
- `FUN_0069CC40`
- `FUN_0069C3F0`

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_batch2_queue_temp183.log`
