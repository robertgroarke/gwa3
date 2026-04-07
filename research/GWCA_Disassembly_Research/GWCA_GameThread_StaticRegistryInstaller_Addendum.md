# GWCA GameThread Static Registry Installer Addendum

This pass closes an important gap in the callback-family story.

The main result is that `FUN_0068F540(...)` does not appear to be called like an ordinary runtime helper. Instead, it is referenced from a tight run of non-function addresses, which strongly suggests a static installer / constructor-table style initialization path. Combined with the shared `PTR_DAT_00A2595C` sites, that gives us the cleanest model yet for how these callback families get installed.

Supporting logs:

- [gw_findrefs_ptra2595c_temp148.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findrefs_ptra2595c_temp148.log)
- [gw_decomp_ptra2595c_sites_temp149.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_ptra2595c_sites_temp149.log)
- [gw_findcallers_0068f540_temp149.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0068f540_temp149.log)

## 1. `FUN_0068F540(...)` has no ordinary code callers

This was the strongest structural surprise in the pass.

`FindCallers` for `FUN_0068F540(...)` returned 24 references, but all of them are:

- `0045D6EB`
- `0045D71B`
- `0045D74B`
- ...
- through `0045DB3B`

And every one of those refs is:

- outside any recognized function body
- evenly spaced
- in a very tight contiguous range

That is not the shape of ordinary call sites.

It looks much more like:

- a static initialization table
- a constructor thunk array
- or another data-driven registration sequence that Ghidra has not lifted into normal functions yet

So the `ImgFlip` callback records are very likely installed by static initializer logic, not lazily built by ad hoc runtime callers.

## 2. `PTR_DAT_00A2595C` is shared by a whole family of registry-record constructors

The vtable/table pointer `PTR_DAT_00A2595C` is not unique to `FUN_0068F540(...)`.

Current referring constructor-style sites:

- `FUN_00688720(...)`
- `FUN_0068C500(...)`
- `FUN_00662670(...)`
- `FUN_0065AB00(...)`
- `FUN_0066A810(...)`
- `FUN_0069B550(...)`
- `FUN_0068F540(...)`

That means `PTR_DAT_00A2595C` is not “the one `ImgFlip` callback vtable” in a narrow sense.

It is the table pointer for a broader registry-record family used in several subsystems.

The common pattern across these constructors is:

- write one to three key fields
- set `+0x00 = PTR_DAT_00A2595C`
- initialize one or two intrusive-link subobjects
- insert through `FUN_00473D80(...)`

So what is shared is most likely:

- a registry-node base class / table
- with subsystem-specific key packing above it

## 3. `FUN_00688720(...)`: the transfer-manager-style record builder

This helper is especially important because it matches the broad transfer lookup side we reversed earlier:

```c
void FUN_00688720(undefined4 key0, undefined4 key1, undefined4 key2)
{
    this[1] = key0;
    this[2] = key1;
    this[3] = key2;
    *this = &PTR_DAT_00a2595c;
    ...
    FUN_00473d80(this, (this[2] << 3 | this[1]) << 3 | this[3]);
}
```

That packed-key formula matches the lookup side of `FUN_00688890(...)`:

```c
packed = (field1 << 3 | field0) << 3 | field2;
```

So this is the constructor/inserter half of the broad transfer-manager callback registry, just as `FUN_0068F540(...)` was the constructor/inserter half of the `ImgFlip` registry.

This is a strong symmetry result:

- `FUN_00688720(...)` <-> `FUN_00688890(...)`
- `FUN_0068F540(...)` <-> `FUN_0068F610(...)`

So there are at least two sibling callback registries built on the same shared base record family.

## 4. `FUN_0068C500(...)`: one-key registry record

`FUN_0068C500(...)` is the simplest member of the family:

- one explicit key at `+0x04`
- two intrusive-link subobjects
- hash = `key0`

This suggests the shared base is general-purpose enough to support:

- one-key registries
- two-key registries
- three-key registries

That fits the broader engine-local “registry base class” reading better than a single-purpose callback-table hypothesis.

## 5. `FUN_00662670(...)`: another three-key registry with a different packer

This helper is a very good comparison point because it uses the same base record/table pointer but a different hash packer:

```c
hash = key1 * 2 ^ (uint)key0 >> 4 ^ key2 ^ key0;
```

It also includes a small constraint check:

```c
if (((param_3 == 0) && ((param_1 & 2) == 0)) && ((param_2 & 2) != 0)) abort();
```

So the shared base does not imply shared semantics. The record base is generic, while each subsystem chooses its own:

- key layout
- validation rules
- hash packing

## 6. `FUN_0065AB00(...)`, `FUN_0066A810(...)`, and `FUN_0069B550(...)`

These three are all simpler two-key variants using the same base record pattern.

Interesting detail:

- `FUN_0065AB00(...)` and `FUN_0069B550(...)` share:
  - `hash = key0 << (key1 != 0)`
- `FUN_0066A810(...)` uses:
  - `hash = key1 << 2 ^ key0`

That again reinforces the same picture:

- shared registry-node base
- subsystem-local hash/key policy

## 7. Strongest architectural interpretation now

The best current model is no longer “we found one callback-record constructor for `ImgFlip`.”

It is:

- `PTR_DAT_00A2595C` is the table pointer for a shared registry-node / callback-record base family
- multiple subsystems build specialized records on top of that base
- each subsystem chooses:
  - key arity
  - key packing
  - validation
  - hash formula
- the records are then inserted through the same intrusive/hash infrastructure `FUN_00473D80(...)`

So the engine’s callback-family system now looks like a general registration framework rather than a one-off image-transfer trick.

## 8. Why this matters for the transfer and `ImgFlip` registries

This pass gives both major registries a cleaner installation story:

- broad transfer-manager registry:
  - constructor side: `FUN_00688720(...)`
  - lookup side: `FUN_00688890(...)`
- `ImgFlip` family registry:
  - constructor side: `FUN_0068F540(...)`
  - lookup side: `FUN_0068F610(...)`

And in both cases, the records appear to be installed through static registration rather than through ordinary direct code calls.

That means the next reverse step should probably not be “find runtime callers of the constructor.”

It should be:

- inspect the static initializer region around `0x0045D6EB..0x0045DB3B`
- or resolve the data/initializer machinery that walks those entries

That is much more likely to reveal:

- the full set of installed `ImgFlip` callback records
- and how each record maps onto `DXT1`, `DXT2/3`, `DXT4/5/DXTL`, and `DXTA`

## 9. Best current model

The callback-family architecture now looks like:

1. shared registry-node base
   - `PTR_DAT_00A2595C`
   - intrusive/hash insertion via `FUN_00473D80(...)`
2. subsystem-specific record builders
   - `FUN_00688720(...)`
   - `FUN_0068F540(...)`
   - and siblings
3. subsystem-specific hashed lookups
   - `FUN_00688890(...)`
   - `FUN_0068F610(...)`
4. subsystem-specific callback dispatch
   - broad transfer callbacks
   - `ImgFlip` family callbacks

That is a substantial upgrade over the earlier “callback record shape” model.

## 10. Next best step

The strongest next reverse step is to decompile the static initializer / registration region that references `FUN_0068F540(...)`, or the helper that walks that region.

That should finally expose:

- the actual installed records
- their exact key triples
- and, with luck, the direct binding between registry entries and the concrete family callbacks we already recovered
