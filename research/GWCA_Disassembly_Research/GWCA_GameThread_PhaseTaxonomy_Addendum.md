## `Gw.exe` Frame Callback Phase Taxonomy Addendum

This pass continues from the active-owner note by reversing the next helpers in the phase/notification layer:

- `FUN_0062D220`
- `FUN_00624700`
- `FUN_006287D0`
- `FUN_00618F10`

The goal here was to untangle three things that had still been partially conflated:

1. global message ids such as `0x25`, `0x26`, and `0x29`
2. owner-local typed phases like `type_id 3` and `type_id 4`
3. the higher-level forwarding wrapper that sits above the raw record executor

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_phase_helpers_temp70.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_phase_helpers_temp70.log)

## High-level result

This pass gives us a much cleaner taxonomy.

The strongest new points are:

- `FUN_00624700(msg_id, state)` is a global message packer/forwarder rooted on `DAT_00bd0c68`
- `FUN_006287D0(...)` is a gated wrapper into the higher-level owner dispatch path
- `FUN_0062D220(type_id, owner)` is an owner lookup helper for typed phase work
- `FUN_00618F10(owner_subptr)` updates a separate global/current-subobject selection target

So the current system is best understood as three layers:

1. global relation messages
   - `0x25`
   - `0x26`
   - `0x29`
2. owner-local typed phases
   - `type_id 3`
   - `type_id 4`
3. gated callback/forwarding wrappers
   - `FUN_006287D0`
   - `FUN_00628570`
   - `FUN_00628740`

That is a cleaner and more defensible split than the earlier “everything is just messages” shorthand.

## `FUN_00624700(msg_id, state)`: global relation-message forwarder

This helper is the clearest separation win in the batch.

It requires:

- `DAT_00bd0c68 != 0`

Then it builds a local block with:

- transformed/scaled coordinates from `state + 8` / `state + 0xC`
- relation globals:
  - `DAT_00bd0c54`
  - `DAT_00bd0c60`
  - `DAT_00bd0c58`
- `state + 0x14`
- a one-bit flag from `state + 4`

If bit `0x40` is set at:

- `DAT_00bd0c68 + 400`

it also runs the pair through:

- `FUN_006298F0(...)`

Finally it emits through:

- `FUN_006286D0(msg_id, &local_38, 0)`

That means `FUN_00624700(...)` is not an owner-local typed phase helper at all. It is a **global relation-message forwarder** that packages state and sends it into the global dispatch plane.

That helps separate the layers:

- `0x25`, `0x26`, `0x29` are global relation messages
- they are not the same thing as owner-local `type_id 3` / `type_id 4`

### What this says about `0x26`

Because `FUN_00624CE0(...)` called:

- `FUN_00624700(0x26, payload)`

and this helper packages relation-state payload then dispatches it globally, `0x26` now looks like:

- a **global relation-state reset/cleanup broadcast**

rather than a local owner-only event.

## `FUN_006287D0(arg0, arg1, arg2)`: gated higher-level forwarder

This helper is tiny but important:

- call `FUN_0048F830(&arg0)`
- if that succeeds:
  - call `FUN_00628570(arg1, arg2)`

So this is a gating wrapper above the higher-level owner dispatch machinery.

That means calls like:

- `FUN_006287D0(0x4e, &local_c, 0)`

from the active-owner setter are not direct message sends.

They are:

- conditional/gated entry into the higher owner-level callback path

This makes the control layers clearer:

- `FUN_006286D0` = raw global dispatch over record tables
- `FUN_00628570` = higher-level owner dispatch wrapper
- `FUN_006287D0` = guard/gate before entering that wrapper

So `0x4e` should currently be described as:

- a gated higher-level owner event id

not as one of the raw global relation message ids.

## `FUN_0062D220(type_id, owner)`: owner lookup for typed phase work

This helper is also small but structurally important.

It does:

- `FUN_0062D5C0(type_id, owner ? owner + 0x128 : 0x128)`
- if that returns non-zero, return `result - 0x128`

That means `FUN_0062D220(...)` is effectively:

- look up an owner/frame relation object for a given `type_id`
- optionally scoped to a provided owner
- and normalize the result back to the containing owner base

This is the missing piece that makes `type_id 3` and `type_id 4` feel like a proper owner-local phase system rather than anonymous integers.

The strongest interpretation now is:

- typed phases operate by retrieving owner-local relation objects of a particular kind
- not by simply sending a number through one flat message bus

That reinforces the distinction:

- global message ids = `0x25`, `0x26`, `0x29`
- owner-local typed phase ids = `3`, `4`

## `FUN_00618F10(owner_subptr)`: current subobject / selection-target update

This helper manages another global/current pointer pair:

- `DAT_00bd028c`
- `DAT_00bd0290`

Its behavior is:

- if the input is null, choose a default entry from a global table
- else follow `*param_1`
- if that subobject exists and `subobject[5] == 0`, capture `subobject[3]`
- compare the chosen subobject against `DAT_00bd028c`
- if it changed:
  - possibly `FUN_00618630(DAT_00bd02a4)`
  - `FUN_0061CE50(captured_value)`
  - store `DAT_00bd0290 = param_1`
- otherwise just update `DAT_00bd0290`

This is important because it shows the active-owner path has another layer beneath it:

- owner/frame relation selection
- plus a current chosen/default subobject inside that owner family

So the relation layer is not only choosing an active owner. It is also updating a current subordinate target/view/state object underneath that owner.

That is one more reason the current best label is:

- focus/selection-aware relation maintenance

not merely layout.

## What this changes about the current model

Before this pass, the strongest model was:

- `0x25` = active-owner changed
- `0x26` / `0x29` = cleanup/deactivation family
- `type_id 3` = activation/rebind side
- `type_id 4` = layout/relation reevaluation side

That was directionally right, but the planes were still a little blurred.

After this pass, the stronger model is:

### Global relation-message plane

- `0x25` = active-owner changed
- `0x26` = global relation-state cleanup/reset broadcast
- `0x29` = global relation deactivation/clear broadcast

These go through:

- `FUN_006286D0(...)`
- and at least `0x26` is packed by `FUN_00624700(...)`

### Owner-local typed phase plane

- `type_id 3` = owner activation/rebind-side phase
- `type_id 4` = owner relation/layout reevaluation phase

These go through:

- `FUN_00628A10(type_id, ...)`
- `FUN_0062D220(type_id, owner)` style lookups

### Higher-level gated wrapper plane

- `FUN_006287D0(...)`
- `FUN_00628570(...)`
- `FUN_00628740(...)`

These are the guarded callback/forwarding path above the lower dispatch machinery.

That is a much more precise taxonomy.

## Best current interpretation

The strongest safe reading after this pass is:

- the GWCA-hooked callback sits above a real frame/relation engine layer
- that layer includes:
  - global relation broadcasts
  - owner-local typed relation phases
  - gated forwarding/callback wrappers
  - active-owner selection
  - subordinate subobject/selection-target updates

So the reverse-engineered seam is now best described as:

- **frame/relation/focus maintenance with both global message and owner-local phase planes**

not just “UI messages” and not just “layout callbacks.”

## Best next step

The next best reverse step is to resolve the typed phase backing objects and the `FUN_0062D5C0(...)` lookup path itself.

The highest-value nearby targets are:

- `FUN_0062D5C0`
- more callers of `FUN_00628A10(...)`
- more callers of `FUN_00624700(...)`
- and, if useful, callers of `FUN_006287D0(...)`

That should let us:

- map which typed phases actually exist beyond `3` and `4`
- identify what owner-local object family each phase id retrieves
- and tighten the distinction between relation phases, selection phases, and layout phases even further
