## `Gw.exe` Frame Callback Secondary Slot Identity Addendum

This pass follows the extended-protocol note by decompiling the helpers that were still ambiguous:

- `FUN_006289F0`
- `FUN_00628E00`
- `FUN_00610D30`
- `FUN_00610540`
- `FUN_0060E2B0`
- `FUN_0062D4E0`
- `FUN_0062E640`
- `FUN_0062E5D0`
- `FUN_0062ABB0`
- representative callers of `FUN_00610370`

The goal was to answer a narrower question than the last pass:

- what are the "secondary objects" in this protocol really?

## Source artifacts

These results come from:

- [gw_decomp_secondary_objects_temp78.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_secondary_objects_temp78.log)
- [gw_decomp_secondary_followup_temp79.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_secondary_followup_temp79.log)
- [gw_findcallers_0060e2b0_temp78.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0060e2b0_temp78.log)
- [gw_findcallers_00610370_temp80.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00610370_temp80.log)
- [gw_decomp_secondary_callers_temp81.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_secondary_callers_temp81.log)

## High-level result

This pass corrects an important over-merge from the previous note.

The old wording treated:

- `FUN_00610370(...)`
- and `FUN_0060E2B0(owner, slot)`

as if they were both ways of creating or resolving the same kind of "secondary object."

The compiled helper bodies do **not** support that.

The stronger current model is:

1. `FUN_0060E2B0(owner, slot)` resolves an **existing owner-attached relation child / slot object** and returns that object's runtime id from `+0xBC`.
2. `FUN_00610370(owner_id, cb, arg)` is a **separate indexed registration path** that allocates a slot in a callback-like manager table and installs a handler/payload pair there.

So the extended protocol is mixing two related but distinct things:

- owner-attached child/relation objects addressed by slot number
- owner-scoped registration/notification state installed through `FUN_00610370(...)`

That is a meaningful correction.

## `FUN_0060E2B0(owner, slot)`: slot child resolver returning runtime id

This helper is now straightforward:

- validate `owner`
- call `FUN_0062D4E0(slot)`
- if found, return `*(resolved + 0xBC)`
- otherwise return `0`

The important part is the return shape.

It does **not** return an arbitrary payload pointer.
It returns the familiar owner/frame-style runtime id at `+0xBC`.

That strongly indicates the resolved object is itself another owner-like relation object.

## `FUN_0062D4E0(slot)`: hashed lookup of owner-local slot objects

This is the function underneath `FUN_0060E2B0(...)`.

Its structure is:

- hash through `FUN_0062D880()`
- index a global bucket table
- walk chained records
- match on:
  - current `ECX`
  - slot number `param_1`
  - hash value
- return `piVar2 - 0x4A`

That `-0x4A` adjustment is the strongest structural clue in the pass.

It means the bucket node is embedded inside a larger object, and the returned base is well before the chained node fields. That is consistent with the larger frame/relation-owner layouts we already recovered elsewhere, where meaningful object identity sits at offsets like:

- `+0xBC`
- `+0xC0`
- `+0xEC..+0xF8`
- `+0x128`

So `FUN_0060E2B0(owner, slot)` is best understood as:

- resolve an owner-local relation child by numeric slot
- return that child's runtime id

not:

- allocate a new detached control object

## `FUN_00610370(owner_id, cb, arg)`: indexed registration path, not a child-object factory

This helper now decompiles cleanly:

- require `owner_id != 0`
- validate owner through `FUN_00628800(owner_id)`
- get a new index from `FUN_006289F0()`
- install `(cb, arg)` through `FUN_00628E00(index, cb, arg)`
- return the allocated index

That return value is not used as an owner id in the representative callers below.
It behaves like a registration token or manager index.

So the strongest correction from this pass is:

- `FUN_00610370(...)` is **not** the same kind of object-production path as `FUN_0060E2B0(...)`

## `FUN_006289F0()`: next free index from a manager table

This function is tiny:

- `FUN_006159D0()`
- `FUN_0046D7D0()`
- return `*(ECX + 8) - 1`

That reads much more like:

- get current count / end
- return latest/next index

than like allocating a heap object.

So the indexed-registration interpretation fits the body better than the older "secondary object allocation" wording.

## `FUN_00628E00(index, cb, arg)`: install / replace manager-table entry with lifecycle notifications

This helper is the clearest evidence that `FUN_00610370(...)` is table installation.

The table shape is:

- entries are `0x0C` bytes each
- base pointer is `*ECX`
- count is `ECX[2]`

At a high level it does:

- bounds-check the index
- compute `entry = base + index * 0x0C`
- if an old callback/object exists:
  - build a small local event packet
  - invoke the old entry with event code `0x0B`
- store:
  - new callback/object pointer
  - new arg/state field with top bit forced on
- if a non-null callback/object was installed:
  - call `FUN_00628880(...)`
  - invoke installed entry with event code `5`
- then invoke installed entry again with event code `9`

So this path is emphatically:

- install or replace an indexed callback/object entry
- send lifecycle notifications to old/new entry

not:

- construct a free-standing UI relation child

## `FUN_00610D30(obj_id, enabled)`: toggle `0x200` flag and emit message `0x36`

This helper also helps classify the slot-resolved objects.

Its behavior is:

- require valid object id
- validate with `FUN_00628800(obj_id)`
- read current `0x200` flag via `FUN_0062E640(0x200)`
- if desired state differs:
  - update flags through `FUN_0062E5D0(0, 0, 0x200)`
  - emit global message `0x36` via `FUN_006286D0(0x36, enabled, 0)`

This makes the slot objects look even more like owner/frame-style relation objects with a real flag word and message participation, not passive data records.

## `FUN_00610540(obj_id)`: owner-local cleanup / flush path

This helper is smaller than expected:

- validate object id
- call `FUN_00628800(obj_id)`
- call `FUN_0062ABB0(1)`

The important point is that it does **not** destroy anything directly.
Its real behavior depends on `FUN_0062ABB0(...)`.

## `FUN_0062ABB0(1)`: set `0x2000` and relink in intrusive list

This helper:

- if `param_1 != 0`, calls `FUN_0062E5D0(0x2000, 0, 0)`
- then rewires an intrusive list anchored at globals like:
  - `DAT_00BD0CCC`
  - `DAT_00BD0CD0`

So `FUN_00610540(obj_id)` reads more like:

- mark object dirty / pending / active in a secondary queue
- relink it in an owner-managed intrusive list

than like:

- free object
- close object
- detach child permanently

## Concrete caller evidence: `FUN_004E1F70`

One representative caller is very useful:

```cpp
uVar1 = FUN_0060D300(param_1, 0, 0x77, FUN_005204B0, &local_18, 0);
FUN_00610370(uVar1, FUN_00851180, 0);
FUN_00610160(uVar1, 0x57, &local_c, 0);
```

This is strong evidence that:

- `uVar1` is the owner/control object being worked on
- `FUN_00610370(uVar1, ...)` registers auxiliary behavior on that object
- high-id message `0x57` is then sent to the same owner/control object

So `FUN_00610370(...)` is acting as registration scaffolding around an existing object, not creating the object that `0x57` targets.

## Concrete caller evidence: `FUN_005711E0`

This caller is even stronger because it uses both mechanisms side by side.

It does all of the following:

- resolve slot children:
  - `FUN_0060E2B0(owner, 3)`
  - `FUN_0060E2B0(owner, 0)`
  - `FUN_0060E2B0(owner, 2)`
  - `FUN_0060E2B0(owner, 1)`
- query and configure those slot children through helpers like:
  - `FUN_00610160(..., 0x57, ...)`
  - `FUN_005F1190(...)`
  - `FUN_005F1170(...)`
  - `FUN_005F1150(...)`
- sometimes create a new owner/control object through `FUN_0060D300(...)`
- immediately register auxiliary behavior on that new owner with:
  - `FUN_00610370(uVar1, FUN_00851180, 0)`
- then send higher-id messages like:
  - `0x5E`
  - `0x5C`

This is the clearest proof in the pass that the two mechanisms are distinct:

- slot-resolution for existing owner-attached child objects
- registration installation for owner-local callback/control state

## Revised interpretation of the high-id protocol

The strongest safe model now is:

- slots `0..4` resolve existing relation/owner child objects
- high-id messages like `0x56..0x5E` operate on those owner-like child objects
- `FUN_00610370(...)` is companion registration machinery that installs handler state onto an owner/control object

So the earlier "secondary object allocation/init" phrasing should be tightened to:

- "owner-scoped registration/handler installation"

for `FUN_00610370(...)`, while:

- "owner-attached slot child resolution"

remains the best label for `FUN_0060E2B0(...)`.

## What changed from the previous pass

The previous addendum correctly noticed that the extended cluster was not a flat callback namespace.
But this pass shows the helper cluster was still too coarsely grouped.

The corrected split is:

- `FUN_0060E2B0(...)`
  - existing child/relation object lookup by slot
- `FUN_00610370(...)`
  - owner-scoped indexed registration install
- `FUN_00610D30(...)`
  - object flag toggle + global notify
- `FUN_00610540(...)`
  - object dirty/queue relink path

That is a better fit for the compiled bodies.

## Best next step

Now that the object/registration split is clearer, the highest-value next pass is to name the slot families and the high-id message verbs more precisely by tracing:

- `FUN_00605690(...)`
- `FUN_00610EA0(...)`
- `FUN_0060DD30(...)`
- `FUN_005F1170(...)`
- `FUN_005F1190(...)`
- `FUN_005F1150(...)`

Those are the helpers most likely to tell us:

- what slots `0`, `1`, `2`, `3`, and `4` actually represent
- and whether messages like `0x57`, `0x58`, `0x59`, `0x5C`, `0x5D`, and `0x5E` are query, set, select, or commit verbs on those slot children
