## `Gw.exe` Frame Callback Metric Metadata Addendum

This pass continues from the metric/builder note by decompiling the shared metric reader and the constructor-side metadata installers:

- `FUN_0060FB00`
- `FUN_00627740`
- `FUN_0062C650`
- `FUN_00627D70`

The goal was to tighten two questions left open by the previous pass:

- what do the metric ids really drive inside the shared metric reader?
- what exact metadata does the constructor attach when it creates a new owner/control node?

## Source artifacts

These results come from:

- [gw_decomp_metric_metadata_temp86.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_metric_metadata_temp86.log)
- [gw_decomp_metric_builder_temp85.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_metric_builder_temp85.log)

## High-level result

This pass upgrades both the metric and constructor stories in a useful way.

First, `FUN_0060FB00(...)` is not a metric-id switch table with named cases.
It is a generic metric placement helper:

- read or derive width/height
- resolve alignment/fit behavior from metric flags
- place that metric inside the current rect
- optionally collapse/shrink the rect afterward
- return `{x, y, w, h}` through the scratch/out block

So the metric ids like:

- `0x89`
- `0x94`
- `0x105`
- `0x114`

still are not semantically named, but we now know the machinery around them is:

- rectangle placement and alignment

not:

- arbitrary widget-specific dispatch

Second, the constructor metadata is much clearer now:

- `FUN_00627740(...)` initializes global object-id registration state
- `FUN_0062C650(parent, type, arg)` installs parent linkage, type id, hashed payload id, intrusive list nodes, and hash-table insertion
- `FUN_00627D70(callback, payload)` installs a callback/handler registry plus indexed per-entry lifecycle notifications

That means `FUN_0060D300(...)` is now standing on three concrete metadata planes:

- global object registration
- relation/parent/type metadata
- callback/handler registration

## `FUN_0060FB00(child, metric_id, rect, scratch, shrink_mode)`: generic metric placement helper

This helper sits underneath:

- `FUN_0060FAC0(...)`
- `FUN_0060F920(...)`

and its behavior is now fairly readable.

### Input validation and baseline extents

It first validates:

- `child != 0`
- `rect != 0`
- rect bounds are sane

Then it derives a baseline width/height either from:

- the current rect
- or from the incoming scratch block

depending on scratch flags.

### Width/height resolution

It resolves candidate width and height from several sources:

- full available rect extent if `metric_id` carries force-fill bits
- explicit scratch-provided values if scratch flags say so
- or measured child dimensions via:
  - `FUN_00629E80(&local_2c, &local_24)`

So the actual metric id is not directly yielding one scalar by itself.
It is steering how the child metric should be fit into the current rect.

### Placement and alignment

It then computes:

- `local_14 = placed_x`
- `local_10 = placed_y`
- `local_c  = width`
- `local_8  = height`

using metric-id flag bits for:

- left/top anchoring
- right/bottom anchoring
- center placement
- full-fill behavior

This is the strongest confirmation in the pass:

- the metric path is a generic frame/layout placement engine

### Optional rect collapse

If `shrink_mode != 0`, it additionally calls:

- `FUN_0062AEC0(&local_14, &local_c, 6)`

Then, based on metric-id flags, it may collapse or shrink the current rect through:

- `FUN_0060FE70(...)`

So the metric helper is not only "measure child."
It is:

- place child
- optionally consume layout space
- return the placed rectangle

### Returned scratch layout

It writes the final placed rectangle to:

- `scratch[3] = x`
- `scratch[4] = y`
- `scratch[5] = w`
- `scratch[6] = h`

So the higher-level callers that used ids like `0x89`, `0x94`, `0x105`, and `0x114` were getting a full placed rect, not a single magic scalar.

## What this means for the metric ids

The metric ids still are not named, but the helper body rules out a few weaker interpretations.

They are **not** merely:

- widget-specific property selectors with no shared geometry semantics

They are being used inside a common placement engine that understands:

- fill vs measured content
- horizontal/vertical anchoring
- centering
- rect consumption

So a safer description now is:

- "metric/layout mode ids consumed by a shared child-placement engine"

That is stronger and more precise than just calling them "metric ids."

## `FUN_00627740(...)`: global object-id registration bootstrap

This helper initializes a newly constructed object's registration state.

The most important parts are:

- zero/init several object fields
- set a fixed marker:
  - `in_ECX[3] = 0x15`
- store an incoming pointer/type token at:
  - `in_ECX[4] = param_1`
- allocate or reuse a global object id from:
  - `DAT_00BB563C`
  - `DAT_00BB5634`
  - `DAT_00BB5624`
- store that runtime id in:
  - `in_ECX[5]`
- increment global object count:
  - `DAT_00BD0CC8`

This lines up perfectly with the earlier observation that owner/control ids live at `+0xBC`.
`FUN_00627740(...)` is clearly one of the bootstrap layers underneath that global id system.

So this helper is best read as:

- register new control object in the global runtime-id pool

## `FUN_0062C650(parent, type, arg)`: relation/parent/type metadata installer

This helper is the clearest constructor-side metadata layer in the pass.

It stores:

- parent relation pointer:
  - `*in_ECX = parent ? parent + 0x128 : 0`
- type id:
  - `in_ECX[1] = param_2`
- hashed/derived payload id:
  - `in_ECX[3] = FUN_0046BF00(param_3, 0xFFFFFFFF)` when `param_3 != 0`

It also initializes multiple intrusive-list/list-head structures at:

- `+0x14`-style regions in the local object

Then, if there is a parent, it links the object into the parent's relation list and calls:

- `FUN_0062DCB0(in_ECX)`

Finally it hashes and inserts the object with:

- `FUN_0062D880()`
- `FUN_00473D80(in_ECX, hash)`

and may repeat the hash insertion if `param_3 != 0` and a lookup fails.

The strongest current interpretation is:

- `FUN_0062C650(...)` attaches the new object into the relation graph and the hashed relation lookup tables

That is exactly the kind of metadata we expected under `FUN_0060D300(...)`.

## `FUN_00627D70(callback, payload)`: callback/handler installer with sorted registries

This helper is much bigger than the name suggested, but its broad purpose is clear.

It is maintaining two linked registries:

- a global sorted callback/function registry rooted at:
  - `DAT_00BB5644`
  - `DAT_00BB5648`
  - `DAT_00BB564C`
- and an object-local `0x0C` entry table attached to the current object

The important behaviors are:

- if `callback == 0`, it tears down/replays existing installed entries using event code `9`
- otherwise it inserts the callback into a sorted global table
- it ensures object-local storage for a new `0x0C` entry
- stores:
  - callback pointer
  - zero/aux slot
  - payload/state value
- invokes the callback with event code `5`
- records callback-associated bookkeeping in a parallel array
- invokes the callback again with a second control packet

So `FUN_00627D70(...)` is not a thin "set callback" helper.
It is a real callback installation and lifecycle-management subsystem.

That matches the earlier correction that `FUN_00610370(...)` was a registration path rather than a child-object factory.

## What this does to `FUN_0060D300(...)`

The previous note already showed `FUN_0060D300(...)` to be a constructor.
This pass lets us say more precisely what it is composing:

1. `FUN_00627740(...)`
   - register object in global runtime-id pool
2. `FUN_0062C650(...)`
   - attach parent/type/hash relation metadata
3. `FUN_00627D70(...)`
   - attach callback/handler registration state

So `FUN_0060D300(...)` is now best described as:

- a full owner/control object constructor that wires up identity, relation linkage, and handler state

not merely:

- "make a frame"

## Strongest current model after this pass

At this point the cleanest architecture model is:

- `FUN_0060D300(...)` constructs a relation/frame control node
- `FUN_00627740(...)` gives it a global runtime id
- `FUN_0062C650(...)` links it into the parent/type/hash relation system
- `FUN_00627D70(...)` wires its callback/handler plane
- slot children are queried and placed by:
  - `FUN_0060FB00(...)`
  - `FUN_0060FAC0(...)`
  - `FUN_0060F920(...)`
  - `FUN_0060FE70(...)`
  - `FUN_0060FA20(...)`

So the remaining unknowns are now less about raw structure and more about naming:

- what exact control classes the constructed nodes correspond to
- what exact semantic families the metric/layout mode ids represent

## Best next step

The highest-yield next pass is to target the helpers immediately around the shared metric reader and the parent-link system:

- `FUN_00629E80`
- `FUN_0062AEC0`
- `FUN_0062DCB0`
- `FUN_0062D450`
- `FUN_00473D80`

That should answer:

- where the measured child width/height actually comes from
- how relation/hash insertion is structured
- and whether the metric/layout modes can be grouped into named classes instead of just numeric ids
