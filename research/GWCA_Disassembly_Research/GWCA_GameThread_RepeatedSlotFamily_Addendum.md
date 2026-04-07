## `Gw.exe` Frame Callback Repeated Slot Family Addendum

This pass pivots back out from the constructor skeleton into the specialized child family behind the repeated slot children:

- helper:
  - `FUN_00604EE0`
- repeated-family callback:
  - `FUN_005F11B0`
- sink/host callback:
  - `FUN_005EE080`
- related created-child callbacks:
  - `FUN_005F3550`
  - `FUN_00850F60`

The goal was to move from:

- "slots `2/4/5/6` are peer repeated entry-like children"

to:

- "what sort of control family do those peers actually behave like?"

## Source artifacts

These results come from:

- [gw_decomp_repeated_slot_family_temp92.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_repeated_slot_family_temp92.log)
- [gw_decomp_slot_roles_temp84.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_roles_temp84.log)
- [gw_decomp_secondary_callers_temp81.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_secondary_callers_temp81.log)

## High-level result

This pass gives the repeated family a much sharper identity.

The strongest current model is:

- slot `1` is a view/host container child
- slots `2` and `3` are subordinate interactive region/coordinate children managed through that host
- slots `4`, `5`, and `6` still look like peer created children, but the repeated-family callback `FUN_005F11B0` now looks much more like an interactive selectable-cell / region-control family than a generic passive entry row

So the family is best described now as:

- a view-host plus interactive region/selection children

rather than just:

- repeated peer entries

## `FUN_00604EE0(owner, type_id, arg)`: extended-channel launcher with refresh follow-up

This helper is very small but useful:

```cpp
FUN_006100A0(owner, type_id, arg, 0);
if (FUN_0060F610(owner) != 0) {
    if (FUN_00610EA0(owner, 0x100000) == 0) {
        FUN_0060D890(owner);
    }
}
```

So it is not just a convenience wrapper around owner-local channel dispatch.
It also conditionally kicks a follow-up refresh/rebind path through:

- `FUN_0060D890(owner)`

unless flag `0x100000` is set.

That matters because the repeated-family channels:

- `7`
- `8`
- `10`
- `0x0B`
- `0x0C`

from the earlier notes are now clearly part of a live interactive control workflow rather than detached event ids.

## `FUN_005F11B0(...)`: interactive child/region callback family

This is the most important body in the pass.

It strongly suggests the repeated family is interactive and region-based, not merely textual rows.

### Case `1`: draw/highlight behavior with state-dependent color

When `param_1[1] == 1`, the function:

- queries owner state through:
  - `0x58`
  - `0x59`
  - `FUN_0060F540(owner)`
- chooses a color among values like:
  - `0xFF404040`
  - `0xFF808080`
  - `0xFF8080FF`
- builds an inset rectangle from `param_2[2..5]`
- and calls:
  - `FUN_0060C610(...)`

That is strong evidence this family has visible selected/available/unavailable rendering states.

### Cases `0x24`, `0x2C`, `0x2E`: toggle/select behavior through bit `4`

These cases mutate bit `4` in the child state word and then call:

- `FUN_0060D080(owner)`
- `FUN_00610540(owner)`

So these children are not passive metric providers.
They have toggled/selected state that triggers owner refresh and dirty-list behavior.

That is one of the strongest reasons to move from "entry-like" to "interactive selectable cell" language.

### Cases `0x56` and `0x57`: normalized-coordinate mapping

These two cases are especially revealing.

`0x56` maps normalized input into object-local coordinates using fields:

- `+0x08`
- `+0x0C`
- `+0x10`
- `+0x14`

`0x57` does the inverse:

- map object-local coordinates back to normalized space

That strongly suggests these children represent interactive regions with a real normalized coordinate system.

This is much more like:

- region control
- hit-test/control cell
- mapped interactive panel area

than:

- plain row text

### Case `0x59`: allocate or resize a `0x34`-byte repeated record array

This case:

- grows a heap buffer at `+0x2C`
- stores count at `+0x34`
- size/capacity at `+0x30`
- then dirties the owner

That looks like a dynamic repeated-item backing store inside the repeated family.

So the old “repeated family” wording is still valid structurally, but the repeated records now appear to support an interactive region/control behavior rather than a flat list row.

### Case `0x15`: fixed insets/margins

This case returns all four margins as:

- `4.0`

So these controls have explicit local padding/inset behavior too.

## Strongest interpretation of `FUN_005F11B0`

The safest upgraded label is:

- shared callback for interactive region/cell children with selectable state, coordinate mapping, and repeated-item backing storage

That is a materially stronger and more specific description than “peer entry-like children.”

## `FUN_005EE080(...)`: slot `1` host/view child

This callback has much stronger naming clues than the repeated-family callback.

### Case `9`: constructs a `CtlView.cpp` object

This branch does:

- `FUN_0047EF00("P:\\Code\\Engine\\Controls\\CtlView.cpp", 0x73)`

and seeds a structure whose `*this = 0x20`, then creates a child through:

- `FUN_0060D300(parent, 0x300, 0, FUN_005EF650, 0, 0)`

That is the clearest source path name we have yet in this cluster.
It makes slot `1` look strongly like a view host/container control.

### Slot `0` as optional nested child under the view host

Still in case `9`, if external payload data exists:

- resolve slot `0`
- call `FUN_0060DAA0(slot0)`
- create another child under that slot through `FUN_0060D300(...)`

So slot `0` behaves like a subordinate host or nested created-child anchor inside the view path.

### Cases `0x7FFFFFFB` and `0x7FFFFFFE`: query slot `3` and slot `2`

These cases:

- resolve slot `3` or slot `2`
- query:
  - `FUN_005F1170(slot)` => `0x58`
  - `FUN_005F1190(slot)` => `0x59`
- read extra geometry via:
  - `FUN_005FD550(...)`

Then return those values through `param_3`.

That is very strong evidence slots `2` and `3` are paired interactive subregions or subcontrols beneath the slot `1` view host.

### Case `0x7FFFFFF9`: update slot `2` and slot `3` with converted coordinates

This case:

- resolves slot `2`
- converts input edges through:
  - `FUN_0046DB80`
  - `FUN_0046DBB0`
- sends the result into slot `2` through:
  - `FUN_0055DEB0(...)`
- then resolves slot `3`
- converts another pair of values
- and sends those into slot `3`

This is a very strong sign slots `2` and `3` are coordinate-bearing children under the view host, not just arbitrary peer records.

### Case `0x7FFFFFF8`: recreate slot `1`

This branch:

- resolves slot `1`
- if present, calls `FUN_0060D890(slot1)`
- then creates a new slot `1` child through:
  - `FUN_0060D300(parent, style | 0x300, 1, param_2[1], param_2[2], 0)`

That makes slot `1` look like a replaceable primary host child.

## Strongest interpretation of slots `1`, `2`, and `3`

With `FUN_005EE080(...)` in hand, the best current role split is:

- slot `1`
  - view host/container child
- slot `2`
  - one coordinate-bearing subordinate region child
- slot `3`
  - another coordinate-bearing subordinate region child, also used in query/update paths

This is a much sharper picture than the earlier “slot `3` is mode/detail child” wording.
Slot `3` may still encode a mode/status aspect, but it is clearly also part of the host’s region/geometry protocol.

## `FUN_005F3550(...)`: created-child family includes image-style controls

This callback gives a useful contrast to the repeated slot family.

### Case `9`: constructs a `CtlImg.cpp` object

This branch does:

- `FUN_0047EF00("P:\\Code\\Engine\\Controls\\CtlImg.cpp", 0x175)`

and seeds a block with image-related fields, then dirties the owner.

So at least one created-child family in this cluster is explicitly image-based.

### Cases `0x56`, `0x57`, `0x58`, `0x59`

These cases implement:

- normalized-to-local coordinate mapping
- local-to-normalized mapping
- extra helper routing
- dynamic repeated-buffer growth / state updates

That overlap with `FUN_005F11B0(...)` is important:

- the repeated slot family and the created-child family share region/control style behavior
- but they are not identical classes

So the safest current interpretation is:

- GW’s frame/control layer reuses a common region/control interaction pattern across multiple concrete control classes

## `FUN_00850F60(...)`: created-child callback bridge

This helper is small but clarifying.

It shows that for one created-child family:

- case `4` installs callback `FUN_006049B0`
- case `9` triggers owner operations:
  - `FUN_0060DFC0(owner, 0x48, 0)`
  - `FUN_0060E020(owner)`
- case `0x56` returns a paired function pointer block

So this callback behaves like a bridge/adapter for one higher-level created-child control family rather than the repeated slot family directly.

## Updated family model

After this pass, the strongest current control-family split is:

- slot `1`
  - primary view host/control container
- slots `2` and `3`
  - paired coordinate-bearing subordinate region controls under that host
- slots `4`, `5`, and `6`
  - still part of a repeated peer family, but the family now looks like interactive selectable region/cell controls rather than generic list rows
- slot `0`
  - optional auxiliary/nested host child used in some view creation paths

That is the cleanest picture yet for the specialized family above the base constructor skeleton.

## What changed from the previous slot-role note

The previous note framed:

- slots `2/4/5/6`

as repeated peer entry-like children.

This pass keeps the “repeated peer family” part, but upgrades the rest:

- they look interactive
- they expose coordinate transforms
- they hold selectable/toggle state
- they participate in dirty/refresh and region queries

So “interactive region/cell family” is a better current label than “entry-like family.”

## Best next step

The highest-yield next pass is to tighten the paired subordinate region controls and the host-specific routing helpers:

- `FUN_005FD550`
- `FUN_0055DEB0`
- `FUN_005EF650`
- `FUN_0060DAA0`
- `FUN_0060D890`

Those should let us say more precisely:

- what geometric/state payload slot `2` and slot `3` are actually storing
- and what kind of view-host lifecycle slot `1` is running above them
