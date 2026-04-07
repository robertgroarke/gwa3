## `Gw.exe` Frame Callback Record-Builder Boundary Addendum

This pass continued the producer search for the region-table child records, especially the fields:

- child-record `+0x24` = mode-`2` mask/flag field
- child-record `+0x28` = layout strategy field

The main targets were:

- `FUN_005F00F0` callers
- `FUN_005EFF80`
- nearby `CtlInstance` setup code

## Source artifacts

These results come from:

- [gw_findcallers_005f00f0_temp105.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_005f00f0_temp105.log)
- [gw_decomp_005eff80_temp105.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_005eff80_temp105.log)
- [gw_decomp_ctlinstance_cluster_temp104.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_ctlinstance_cluster_temp104.log)

## High-level result

This pass did not recover the final child-record builder, but it did close one more false trail and sharpen the search boundary.

The most useful results are:

- `FUN_005F00F0(...)` is only called by:
  - `FUN_005EFFC0(...)`
  - itself recursively
- `FUN_005EFF80(...)` only writes the owner/control id into the `CtlInstance` subobject

So the record table is almost certainly:

- fully built earlier in control setup
- not lazily created during interaction validation
- and not produced by the tiny setup helpers we just checked

## `FUN_005F00F0(...)` caller boundary

The caller search for `FUN_005F00F0(...)` came back very small:

- one external caller:
  - `FUN_005EFFC0(...)`
- one internal recursive caller:
  - `FUN_005F00F0(...)`

That is a strong structural result.

It means:

- the recursive region-table walker is a pure consumer of an already-built table
- it is not the place where the child records themselves are assembled

So the search for the mode-`2` record producer has to move earlier in the lifecycle than the interaction commit path.

## `FUN_005EFF80(owner_id)`: owner-id setter, nothing more

This helper decompiles to:

```cpp
*(this + 4) = owner_id;
```

That is useful because `FUN_005EFF80(...)` showed up in multiple `CtlInstance` constructor cases and looked like it might be doing some richer control setup.

It is not.

It only:

- stores the owner/control id into the subobject

So it tells us one concrete thing about the region-table manager object:

- the owner/control id lives at subobject `+0x04`

But it also confirms this helper is not involved in building the child-record array or child-record metadata.

## What this says about the setup sequence

Putting the recent passes together, the current setup split now looks like:

### `CtlInstance` setup helpers

- `FUN_005EF740(...)`
  - initialize common subobject / region-table manager shell
- `FUN_005EFF80(owner_id)`
  - store owner/control id

### interaction-side consumers

- `FUN_005EFFC0(...)`
  - validate region/update against existing table
- `FUN_005F00F0(...)`
  - recursively walk the already-built table
- `FUN_005F0970(...)`
  - recursively resolve desired sizes from the already-built table

What is still missing is the middle step:

- who actually fills `in_ECX[2]`, `in_ECX[4]`, and the child-record contents?

That missing step is now clearly separate from both:

- the tiny setup helpers
- and the runtime interaction walkers

## Why this is still a useful pass

This pass did not uncover the final builder, but it meaningfully narrows the search space.

We can now stop treating these helpers as ambiguous:

- `FUN_005EFF80(...)` is not a builder
- `FUN_005F00F0(...)` is not a builder
- the region table must exist before interaction-time validation begins

That is enough to justify shifting the reverse plan again.

## Updated working model

The cleanest current model is now:

- common `CtlInstance` substrate
  - stores owner id
  - stores interaction state
  - stores a region-table manager shell
- separate earlier build path
  - populates the child-record pointer array behind `in_ECX[2]`
  - writes child-record `+0x24` and `+0x28`
- later interaction path
  - only consumes that table

That separation is now strong enough to rely on for the next reverse pass.

## Best next step

The next best step is to pivot from the consumers to the actual table producers by targeting:

- control-specific layout/setup handlers that run before interaction
- especially builder-style cases like the `0x37` setup flows
- and helpers those flows call, such as:
  - `FUN_0052D970`
  - `FUN_0052DD20`
  - `FUN_00526A50`
  - any append/allocation helpers reached from those setup paths

That is now the likeliest route to the code that actually writes child-record mode and mask metadata.

