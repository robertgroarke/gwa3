# GWCA MemoryPatcher Live Addendum

## Scope

This pass turns the earlier "GWCA also has a memory patcher" observation into something concrete:

- recover the `MemoryPatcher` object layout from compiled code
- dump the live patcher vector from the running `L I L B I S C U I T` client
- compare it against the live detour hook table

The result is the first direct side-by-side view of GWCA's two runtime instrumentation systems.

## Static `MemoryPatcher` object layout

The key compiled functions were:

- [SetPatch](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001de70_deep.log)
- [Reset](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001dd70_deep.log)
- [FUN_1001dd10](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001dd10_deep.log)

Those make the object shape clear:

- `+0x00` target address
- `+0x04` patched-bytes buffer
- `+0x08` original-bytes buffer
- `+0x0C` patch size
- `+0x10` enabled/active flag

`SetPatch(...)` does this:

1. `Reset(this)`
2. store `target`
3. store `size`
4. allocate `patched` buffer
5. allocate `original` buffer
6. copy caller-supplied patch bytes into `patched`
7. snapshot live bytes from target into `original`
8. append `this` into the global patcher vector

So the patcher is object-based, not just a flat range list.

## Important behavioral detail

`SetPatch(...)` does **not** immediately write the patch into the live target.

It stages both buffers and registers the object.

Actual application is coordinated later by:

- `MemoryPatcher::EnableHooks()`
- `MemoryPatcher::DisableHooks()`

That is an important distinction from the detour hook engine, which installs each hook as part of `CreateHook(...)` plus later enable/disable control.

## Global patcher vector

The compiled code shows a vector of `MemoryPatcher*` objects:

- `DAT_1008a1F8` = vector start
- `DAT_1008a1FC` = vector end
- `DAT_1008a200` = vector capacity end

And one global state byte:

- `DAT_10088144` = patcher enabled flag

This is separate from the detour hook table:

- `DAT_1008b0c0` / `DAT_1008b0c8`

So the two systems are structurally independent in memory as well as behavior.

## Runtime dump: live memory patch vector

I extended:

- [live_gwca_runtime_dump.ps1](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\scripts\live_gwca_runtime_dump.ps1)

so it now reports:

- `MemPatchVecStart`
- `MemPatchVecEnd`
- `MemPatchVecCap`
- `MemPatchEnabled`
- each live patch object dereferenced into:
  - target
  - patched buffer
  - original buffer
  - size
  - enabled flag

The raw output is in:

- [live_gwca_runtime_report.txt](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\tests\live_gwca_runtime_report.txt)

## Live patcher state from the current client

From the latest runtime dump:

- `MemPatchVecStart = 0x1CF44FA8`
- `MemPatchVecEnd = 0x1CF44FB4`
- `MemPatchVecCap = 0x1CF44FB4`
- `MemPatchEnabled = 0x00000001`

That means:

- the patcher vector contained exactly `3` entries
- the patcher system was currently enabled

## Live memory patches

### Patch 0

```text
obj=0x6EDA9F18
target=0x00ECFD06
patched=0x1D13CF40
original=0x1D13CF60
size=0x2
enabled=0x00000000
```

### Patch 1

```text
obj=0x6EDAA188
target=0x01019362
patched=0x1D13D050
original=0x1D13CFB0
size=0x1
enabled=0x00000001
```

### Patch 2

```text
obj=0x6EDA9FA4
target=0x00F1898C
patched=0x1D13D0B0
original=0x1D13CFE0
size=0x2
enabled=0x00000000
```

A few things stand out immediately:

- all three are tiny direct byte patches
- sizes are only `1` or `2` bytes
- unlike detour hooks, there are no replay slots or relocation tables attached

## Important interpretation of the per-object enabled flag

The per-object flag at `+0x10` is **not** the same as the global `MemPatchEnabled` byte.

From `Reset(...)` and `SetPatch(...)`, the object flag looks like a per-record armed/present marker rather than the global runtime on/off state.

That matches the live dump:

- global patching is enabled
- but two of the three objects show `enabled=0`

So the safest current reading is:

- global byte-patch application is governed by `DAT_10088144`
- the object flag tracks whether a record is logically active/registered for reset/update purposes

I would treat the exact semantics of that object flag as partially resolved rather than fully nailed down.

## Detour vs memory patch: first live comparison

In this client state:

- detour hook table count: `39`
- memory patch record count: `3`

That’s a useful practical result.

At least in this initialized state, GWCA is leaning much more heavily on:

- detour hooks for the bulk of its runtime interception

while using:

- only a very small number of direct byte patches

So the memory patcher is real, but it is not the dominant instrumentation plane in this run.

## What this means for replacement feasibility

This sharpens the engineering picture:

- the hard part of replacing GWCA is still the detour hook cluster, not the memory patch list
- the memory patcher is comparatively small and mechanically simple
- a partial replacement strategy could reasonably prioritize:
  1. UIModule detours
  2. other major detour-backed subsystems
  3. only then the small direct byte-patch set

That is a better prioritization than treating all "hooks" as equivalent.

## Best next step

The next logical reverse pass is:

1. identify which subsystem owns the three live memory patches
2. name more of the unresolved live detour RVAs from the hook table
3. build a unified matrix:
   - subsystem
   - mechanism = detour or memory patch
   - live count
   - confidence level

At this point, though, the detour-vs-memory-patch split is no longer abstract.
