# GWCA Toolbox Module Ownership Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_SeededCallable_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_SeededCallable_Addendum.md)

The target was the producer side of:

- `DAT_1008A418`

That was the next logical step because the previous pass showed `DAT_1008A418` is the grouped-owner registry used by the seeded cleanup callable at `0x100265E0`.

## Strongest new result: the owner key is a module handle

Fresh decompilation of the producer helper:

- [decomp_a418_producers_temp3.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_a418_producers_temp3.log)

shows that `FUN_100251D0(param_1)` is effectively:

- `AddHookEntryByModule(hook_entry)`

The critical logic is:

```cpp
local_8 = FUN_1001d7b0(param_1, '\0');
if (local_8 == 0) {
    local_8 = FUN_1001d7b0(param_1, '\x01');
}
if (local_8 == 0) {
    GW::FatalAssert(..., "`anonymous-namespace'::AddHookEntryByModule");
}
```

That means:

- `param_1` is a `HookEntry*`-like address
- GWCA resolves that address to a module handle
- then uses that module handle as the grouping key in `DAT_1008A418`

So the grouped registry is not keyed by:

- UI message id
- frame id
- callback id

It is keyed by:

- module ownership

## `FUN_1001D7B0` is the range-table resolver for module ownership

Fresh decompilation:

- [decomp_modulekey_temp4.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_modulekey_temp4.log)

shows:

```cpp
for (; puVar1 != DAT_1008a1f0; puVar1 = puVar1 + 3) {
    if (((uint)puVar1[1] <= param_1) && (param_1 < (uint)puVar1[2])) {
        return *puVar1;
    }
}
```

So `DAT_1008A1EC..DAT_1008A1F4` is a range table of `0x0C` records:

```text
struct RangeRecord {
    uint32_t module_handle; // +0x00
    uint32_t range_start;   // +0x04
    uint32_t range_end;     // +0x08
};
```

This finally explains the role of the earlier range-table work:

- it maps a `HookEntry*` address back to the owning module

That module handle then becomes the key in `DAT_1008A418`.

## `DAT_1008A418` is a map from module handle to `HookEntry*` vector

The producer helper `FUN_100251D0` does two distinct things:

1. resolve `hook_entry -> module_handle`
2. append that `hook_entry` into the grouped vector for the resolved module

Recovered behavior:

- if the module key is not present:
  - create a new bucket in `DAT_1008A418`
  - initialize its vector range to empty
- then append the incoming `HookEntry*`

So the best current model is:

```text
DAT_1008A418:
    module_handle
      -> vector<HookEntry*>
```

That lines up perfectly with the cleanup side from `FUN_100265E0`, which:

- looks up the module key
- clones the `HookEntry*` vector
- removes all three UI callback families for each hook entry in that group
- erases the bucket

## Bootstrap/init of the supporting registries is now clearer

The xref cluster around `0x10001530` and `0x100015E0` fills in the support structures:

- `FUN_10001530` initializes `DAT_1008A438` and nearby message-id/vector support state
- raw decode around `0x100015E0` initializes `DAT_1008A418`

Artifacts:

- [raw_a418_cluster_temp3.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\raw_a418_cluster_temp3.log)
- [decomp_a418_producers_temp3.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_a418_producers_temp3.log)

So the full lifecycle is now much sharper:

1. initialize grouped-owner map
2. initialize global UI message-id cleanup list
3. when UI callbacks are registered, add their `HookEntry*` into the per-module bucket
4. when cleanup fires for that module key, bulk-remove all associated UI callbacks

## The producer call sites are now identified

Caller recovery for `FUN_100251D0`:

- [findcallers_100251d0_temp4.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_100251d0_temp4.log)

Recovered callers:

1. `RegisterCreateUIComponentCallback @ 0x100268F0`
2. `RegisterFrameUIMessageCallback @ 0x100269E0`
3. `RegisterUIMessageCallback @ 0x10026DC0`

This is one of the strongest results in the whole seam because it proves the ownership map is populated by the three exact callback registration APIs that the cleanup callable later unwinds.

So the ownership story is now closed at both ends:

- registration side inserts by module
- cleanup side removes by module

## Updated UI ownership model

The corrected model is now:

```text
RegisterCreateUIComponentCallback
RegisterFrameUIMessageCallback
RegisterUIMessageCallback
  -> FUN_100251d0(AddHookEntryByModule)
      -> FUN_1001d7b0(resolve module from HookEntry address via range table)
      -> insert HookEntry* into DAT_1008A418[module_handle]

seeded cleanup callable FUN_100265e0(module_handle)
  -> find DAT_1008A418[module_handle]
  -> remove all callback registrations for each HookEntry* in that bucket
  -> erase bucket
```

That is much stronger than the previous “grouped owner key” wording.
The key is now concretely:

- module handle

## Best current interpretation

The strongest conservative reading after this pass is:

- GWCA tracks UI callback ownership by module
- range-table metadata maps hook-entry addresses back to module handles
- all three UI callback registration APIs feed the same per-module grouping map
- the bootstrap-installed cleanup listener exists to bulk-remove those callbacks later

So the toolbox/UI seam now looks like a full registration-and-cleanup ownership system rather than a loose collection of helper registries.

## Best next step

The next logical reverse step is to decompile the three registration callers side by side:

- `RegisterCreateUIComponentCallback @ 0x100268F0`
- `RegisterFrameUIMessageCallback @ 0x100269E0`
- `RegisterUIMessageCallback @ 0x10026DC0`

That should answer the remaining practical questions:

1. exactly when `FUN_100251D0` is called in each registration path
2. what a `HookEntry` record looks like at registration time
3. how the module ownership bookkeeping is attached to the public callback APIs
