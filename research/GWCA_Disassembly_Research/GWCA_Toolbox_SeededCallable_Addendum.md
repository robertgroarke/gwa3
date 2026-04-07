# GWCA Toolbox Seeded Callable Addendum

## Scope

This pass continues directly from:

- [GWCA_Toolbox_ListenerPath_Correction_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ListenerPath_Correction_Addendum.md)

The target here was the bootstrap-installed callable body:

- `0x100265E0`

That was the next logical reverse step because `FUN_10024690` seeds one listener record with a stack-built callable object whose target function is:

- `0x100265E0`

## Strongest new result: `0x100265E0` is a grouped UI-callback cleanup routine

Fresh decompilation from a clean project:

- [decomp_100265e0_temp2.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_100265e0_temp2.log)

shows that `FUN_100265E0(param_1)` does **not** mutate the intercepted argument on the live call path.

Instead, it:

1. does a lower-bound lookup into `DAT_1008A418`
2. finds a matching grouped entry for `param_1`
3. clones the associated vector of `HookEntry*`
4. for each `HookEntry*` in that group:
   - call `GW::UI::RemoveCreateUIComponentCallback(hook_entry)`
   - iterate `DAT_1008A438` and call `GW::UI::RemoveUIMessageCallback(hook_entry, message_id)` for each stored message id
   - call `GW::UI::RemoveFrameUIMessageCallback(hook_entry)`
5. erase the keyed group from `DAT_1008A418`
6. free the temporary copy buffer

So the seeded callable is best understood as:

- a bulk cleanup handler for UI callback ownership groups

not:

- a front-door live parameter mutator

## The grouped registry at `DAT_1008A418` is now the key structure

The most revealing lines in the new decompile are:

```cpp
_Find_lower_bound<>(&DAT_1008a418, ..., &param_1);
...
puVar6 = (uint *)(*(int *)((int)puVar5 + 0x18) - *(int *)((int)puVar5 + 0x14) >> 2);
...
GW::UI::RemoveCreateUIComponentCallback(pHVar1);
...
GW::UI::RemoveUIMessageCallback(pHVar1, puVar2[2]);
...
GW::UI::RemoveFrameUIMessageCallback(pHVar1);
...
FUN_10028d40(&DAT_1008a418, &param_1);
```

That gives a much cleaner interpretation of `DAT_1008A418` than we had before:

- it is a keyed lookup structure
- each matched entry owns a vector/range of `HookEntry*`
- that vector is used as a cleanup ownership group

So the seeded callable is a registry-driven “remove all UI callbacks associated with this key” routine.

## The callback families are now explicit

This pass is especially useful because it confirms the cleanup routine spans all three UI callback planes together:

1. create-component callbacks
2. global UI-message callbacks
3. frame UI-message callbacks

That means the ownership model underneath GWCA’s UI layer is broader than any single registry.

The seeded cleanup callable is a bridge across those registries.

## The `DAT_1008A438` list is a message-id catalog for cleanup

One subtle but important detail in the decompile is this loop:

```cpp
puVar3 = DAT_1008a438;
for (puVar2 = (undefined4 *)*DAT_1008a438; puVar2 != puVar3; puVar2 = (undefined4 *)*puVar2) {
    GW::UI::RemoveUIMessageCallback(pHVar1, puVar2[2]);
}
```

So `DAT_1008A438` is not just another random vector nearby.
It behaves like a list of known UI message ids that should be walked when bulk-removing a grouped callback owner.

That gives the cleanup path a two-level structure:

- grouped owner key -> list of `HookEntry*`
- global message-id catalog -> per-message removal for each `HookEntry*`

## Important interpretation change: the seeded listener is about lifecycle, not interception logic

Before this pass, the open question was whether the bootstrap-installed listener was changing intercepted arguments on the live path.

The new answer is:

- the seeded callable body is lifecycle/cleanup oriented
- it bulk-removes callback registrations associated with a key

So the bootstrap-installed listener record is best read as:

- ownership cleanup support for the UI callback system

not:

- primary behavior injection into a specific UI message

That makes the corrected listener seam much more coherent.

## Relationship to the earlier listener record model

This pass does **not** break the corrected listener-record model.

It sharpens it.

The record at `DAT_1008A1E0..DAT_1008A1E8` still holds:

- key at `+0x00`
- callable-holder at `+0x08 .. +0x2B`

What changed is our understanding of what the seeded callable actually does:

- it is a cleanup callable over grouped UI callback ownership

not:

- a direct handler for one concrete intercepted argument transformation

## Caller confirmation

The only recovered caller of `0x100265E0` is still:

- `FUN_10024690`

Artifact:

- [findcallers_100265e0_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_100265e0_temp.log)

That keeps the ownership story tight:

- UI bootstrap installs the detours
- UI bootstrap seeds the cleanup listener
- seeded cleanup listener knows how to tear down grouped UI callback ownership

## Best current interpretation

The strongest conservative model after this pass is:

```text
FUN_10024690
  -> install UI detour cluster
  -> seed one listener record via FUN_1001da50
      -> listener record owns callable-holder
          -> target callable = FUN_100265e0
              -> bulk-remove UI callback groups for a key
```

So this seam now looks like bootstrap-installed lifecycle glue for the UI interception system.

## Best next step

The next logical reverse step is to trace how `DAT_1008A418` is populated.

That should answer:

1. what the key `param_1` actually represents
2. which registration paths append `HookEntry*` into each grouped owner bucket
3. whether the grouping unit is:
   - module
   - callback owner token
   - plugin-style identity
   - or another lifecycle handle

At this point, `DAT_1008A418` is the highest-value unresolved structure in this toolbox/UI seam.
