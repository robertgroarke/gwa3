## `Gw.exe` Frame Callback Provisional Slot Roles Addendum

This pass continues from the slot-verb protocol note by decompiling the helper layer that sits right on top of the slot children:

- `FUN_0060F5C0`
- `FUN_005FC4D0`
- `FUN_005EB3D0`
- `FUN_00604F20`
- `FUN_00605160`
- `FUN_00605350`

The goal was not to force final names too early.
It was to answer a narrower question:

- what kinds of child roles do the slot numbers appear to represent?

## Source artifacts

These results come from:

- [gw_decomp_slot_roles_temp84.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_roles_temp84.log)
- [gw_decomp_slot_verbs_temp82.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_verbs_temp82.log)
- [gw_decomp_slot_commit_temp83.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_slot_commit_temp83.log)
- [gw_decomp_secondary_callers_temp81.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_secondary_callers_temp81.log)
- [gw_decomp_extended_cluster_temp77.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_extended_cluster_temp77.log)

## High-level result

This pass does not give final UI names like:

- "tab strip"
- "preview pane"
- "list box"

But it does tighten the slot families substantially.

The strongest current model is:

- slot `0` is an optional auxiliary child that expands layout width and participates in selection updates
- slot `1` is the final layout/commit sink child
- slot `2` is a gated availability/compatibility child
- slot `3` is a mode/status child with query+set behavior and optional layout contribution
- slots `4`, `5`, and `6` behave like peer item/row children in a repeated option family

That is already much better than "slot 0/1/2/3 are unknown children."

## `FUN_0060F5C0(slot_obj)`: tests whether `0x200` is clear

This helper is tiny:

```cpp
FUN_00628800(param_1);
iVar1 = FUN_0062e640(0x200);
return iVar1 == 0;
```

So it is effectively:

- `is_not_flagged_0x200(slot_obj)`

That matters because `FUN_005711E0(...)` uses slot `2` through this gate:

- resolve slot `2`
- call `FUN_0060F5C0(slot2)`
- if acceptable, continue into value/selection logic

This makes slot `2` look like an availability/compatibility gate rather than a pure display-only child.

## `FUN_005FC4D0(slot_obj, value)`: `0x5A` query/transform verb

This helper does:

```cpp
FUN_00610160(slot_obj, 0x5A, value, &local_8);
return local_8;
```

So `0x5A` is neither a simple setter nor a pure getter.
It is a query/transform verb:

- input in `wParam`
- result in `lParam`

That is important because `FUN_005711E0(...)` uses it on slot `3` immediately after reading slot `3`'s current `0x59` value.

This makes slot `3` look more like a mode/status translator or validator than a plain "display field."

## `FUN_005EB3D0(slot_obj, value)`: `0x62` direct push verb

This helper is:

```cpp
FUN_00610160(slot_obj, 0x62, value, 0);
```

It is used by `FUN_00604F20(...)` on the object created in `local_14`.

That is useful because it shows the slot/owner protocol is broader than the `0x57..0x5E` family.
There is at least one neighboring direct push verb `0x62`.

For this pass, the key point is simpler:

- the owner/control creation helpers are pushing concrete configuration payloads into newly created child objects immediately after creation

## `FUN_00605160(owner, point)`: layout probe using slot families

This helper is one of the strongest slot-role clues so far.

It computes geometry using several slot children in a repeated pattern.

### Repeated row family: slots `2`, `5`, `4`, `6`

This function loops over four slots encoded as integer bit-patterns in float locals:

- `2`
- `5`
- `4`
- `6`

For each resolved slot child it calls:

- `FUN_0060FAC0(slot_child, 0x89, ...)`

and tracks a maximum extent-like value.

That is strong evidence these slots are peer children in a repeated visual family:

- row-like
- entry-like
- option-like

not four unrelated child types.

### Slot `0`: optional auxiliary width contributor

After the repeated family, the helper resolves slot `0` and, if present, applies:

- `FUN_0060FAC0(slot0, 0x94, ...)`
- plus an extra fixed spacing contribution

So slot `0` behaves like an optional auxiliary child that changes overall width/extent.

### Slot `3`: optional mode/detail contributor

Then the helper resolves slot `3`.

If slot `3` exists:

- it adds another small fixed contribution
- calls `FUN_0060FAC0(slot3, 0x105, ...)`
- and uses a slightly larger constant than the no-slot-3 case

This makes slot `3` look like an optional detail/header/mode child whose presence changes layout and whose internal content contributes its own metric.

### Slot `1`: final sink child

Finally, the helper resolves slot `1` and drives:

- `FUN_0060FAC0(slot1, 0x114, ...)`

as the last step.

That is a strong sign slot `1` is not just another peer row.
It behaves like the final target/sink child in this layout pass.

## `FUN_00605350(owner, out_rect)`: fuller size computation confirms the same role split

This function is a larger sibling of `FUN_00605160(...)`, and it reinforces the same picture.

It does all of the following:

- checks slot `0` first and increases base width if it exists
- loops the same repeated slot family:
  - `2`
  - `5`
  - `4`
  - `6`
- uses the same slot `0` metric id:
  - `0x94`
- uses the same slot `3` metric id:
  - `0x105`
- ends with slot `1` through:
  - `FUN_0060F920(slot1, 0x114, ...)`

So the split seen in `FUN_00605160(...)` is not accidental.
It is a real structural family:

- one optional auxiliary child: slot `0`
- one final sink child: slot `1`
- one gating/detail child: slot `3`
- one repeated peer-entry family: `2/4/5/6`

## `FUN_00604F20(owner, param)`: slot-based control creation and state branching

This helper is noisier, but it adds two valuable slot-role clues.

### It branches heavily on owner flags

It tests flags like:

- `0x8000`
- `0x1000`
- `0x10000`
- `0x4000`
- `0x2000`
- `0xE0000`

and creates child objects through repeated `FUN_0060D300(...)` calls.

So the owner is clearly building a structured control surface, not just relaying one event.

### It pushes a final value into the created child through `0x62`

If `*param_2 != 0`, it calls:

- `FUN_005EB3D0(local_14, *param_2)`

which is:

- `FUN_00610160(local_14, 0x62, value, 0)`

So newly created child objects can be directly configured with a value push.

### It uses a separate created child with `0x58`

When `DAT_00BCFFEC != 0` and another created child exists, it computes a payload and sends:

- `FUN_00610160(iVar2, 0x58, &local_40, 0)`

That is a little different from the earlier slot-child `0x58` getter wrappers and is worth keeping separate for now:

- the slot-child wrappers made `0x58` look getter-like
- this created-child path sends `0x58` with a concrete input payload

So `0x58` may be context-sensitive across object families, or the wrapper naming only captured one usage pattern.
This is one place where the safer wording is still:

- source evidence shows multiple usage forms

## Provisional slot-role matrix

Based on the current evidence, the best working matrix is:

- slot `0`
  - optional auxiliary child
  - contributes extra width/extent
  - participates in value selection/update paths
- slot `1`
  - final sink/commit/layout child
  - last stage in both measurement helpers
  - likely receives the owner's composed output
- slot `2`
  - gated peer child
  - checked through `FUN_0060F5C0`
  - likely availability/compatibility sensitive
- slot `3`
  - mode/detail/status child
  - queried through `0x59`
  - transformed/validated through `0x5A`
  - updated through `0x57`
  - contributes optional layout metric `0x105`
- slots `4`, `5`, `6`
  - peer repeated row/entry family
  - same metric pipeline as slot `2`
  - likely option-entry siblings rather than unique control kinds

## What changed from the previous pass

The last pass established the protocol shape:

- getters
- setters
- parent dirty/refresh

This pass adds a stronger structural interpretation:

- the slots are not arbitrary ids
- they group into distinct role families

The most important new improvement is the repeated-family result:

- `2/4/5/6` really do look like peers

That gives us a good anchor for future naming.

## Remaining ambiguity

There are still two places where the naming should stay cautious.

First:

- slot `0` might be a preview, sidecar, or auxiliary control child
- but the current evidence only proves "optional extent contributor plus value-path participant"

Second:

- slot `1` looks like a final sink child
- but we still do not know whether it is a viewport child, text sink, button cluster, or another specific widget type

So the matrix above should still be treated as provisional.

## Best next step

The next highest-yield pass is to target the metric and builder helpers that sit directly behind the role split:

- `FUN_0060FAC0`
- `FUN_0060F920`
- `FUN_0060FE70`
- `FUN_0060FA20`
- `FUN_0060D300`

That should tell us whether the repeated slot family is measuring:

- rows
- buttons
- labels
- or some other standardized child control type

and it should also help decide whether slot `1` is a true content sink or just the last child in a composed layout chain.
