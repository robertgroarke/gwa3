# GWCA GameThread Callback Registry Installation Appendix

This appendix closes the callback-registry branch from the installation side.

The earlier notes already recovered:

- the lookup side for the broad transfer registry
  - `FUN_00688890(...)`
- the lookup side for the `ImgFlip` registry
  - `FUN_0068F610(...)`
- and one concrete callback-family cluster behind the `ImgFlip` path

The remaining question was:

- are these just isolated lookup helpers, or part of a broader shared registration framework?

The answer is now strong enough to state directly:

- both callback registries sit on top of a shared registry-node base
- both have recovered constructor/inserter halves
- and at least the `ImgFlip` side appears to be installed by static initializer machinery rather than by ordinary runtime call sites

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_FirstConcreteCallbackBodiesAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_FirstConcreteCallbackBodiesAppendix.md)
- [GWCA_GameThread_ImgFlipRegistryBuilder_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ImgFlipRegistryBuilder_Addendum.md)
- [GWCA_GameThread_StaticRegistryInstaller_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_StaticRegistryInstaller_Addendum.md)
- [GWCA_GameThread_CallbackLookupRegistry_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackLookupRegistry_Addendum.md)
- [GWCA_GameThread_CallbackRecordShape_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackRecordShape_Addendum.md)

## Two Recovered Registry Pairs

The strongest current callback-registry symmetry is:

### Broad transfer-manager registry

- constructor / inserter:
  - `FUN_00688720(...)`
- lookup:
  - `FUN_00688890(...)`

### `ImgFlip` family registry

- constructor / inserter:
  - `FUN_0068F540(...)`
- lookup:
  - `FUN_0068F610(...)`

That means the callback side is no longer recovered only from lookup and invoke bodies.

We now have both sides for two sibling registries.

## Shared Registry-Node Base

The constructor notes make the shared base pattern much clearer.

Across multiple constructor-style helpers, the common structure is:

- write one to three explicit key fields
- set:
  - `+0x00 = PTR_DAT_00A2595C`
- initialize intrusive-link subobjects
- insert through:
  - `FUN_00473D80(...)`

This pattern appears in:

- `FUN_00688720(...)`
- `FUN_0068F540(...)`
- `FUN_0068C500(...)`
- `FUN_00662670(...)`
- `FUN_0065AB00(...)`
- `FUN_0066A810(...)`
- `FUN_0069B550(...)`

So `PTR_DAT_00A2595C` is best read as:

- a shared registry-node / callback-record base table

not:

- one single-purpose `ImgFlip` vtable

## Constructor / Lookup Correspondence

The two recovered registry pairs are numerically and structurally tight.

### `FUN_00688720(...)` <-> `FUN_00688890(...)`

The constructor packs:

- `(key1 << 3 | key0) << 3 | key2`

The lookup uses the same packed-key formula.

So the broad transfer registry is recovered from both:

- constructor side
- lookup side

### `FUN_0068F540(...)` <-> `FUN_0068F610(...)`

The constructor packs:

- `key2 + key1 ^ key0 << 2 ^ extra`

with:

- `extra = 0` when `key0 < 8`
- otherwise:
  - `FUN_0046DCF0()`

The lookup uses the same formula.

So the `ImgFlip` registry is also recovered from both:

- constructor side
- lookup side

That gives us two independent examples of the same architecture:

- shared record base
- subsystem-local key packing
- shared intrusive/hash insertion
- subsystem-local lookup

## Static Installation Story

The `ImgFlip` side also has a strong installation result.

`FindCallers` for `FUN_0068F540(...)` does not show ordinary function callers.
Instead it shows a tight run of evenly spaced references in:

- `0x0045D6EB .. 0x0045DB3B`

and those references sit outside recognized function bodies.

That is much more consistent with:

- static initializer entries
- constructor thunk tables
- or another data-driven registration sequence

than with ordinary runtime helper calls.

So the best current model is:

- `ImgFlip` callback records are statically installed into the registry
- then later looked up dynamically through `FUN_0068F610(...)`

This is a stronger and cleaner explanation than any lazy-runtime-builder model.

## What This Means for the `ImgFlip` Family

The `ImgFlip` path is now the best-recovered callback subsystem in the whole branch.

We have:

1. concrete family dispatcher
   - `FUN_0068FE40(...)`
2. concrete registry lookup
   - `FUN_0068F610(...)`
3. concrete constructor/inserter
   - `FUN_0068F540(...)`
4. concrete slot-`0` family bodies
   - `FUN_0068F6C0(...)`
   - `FUN_0068F840(...)`
   - `FUN_0068FA70(...)`
   - `FUN_0068FCB0(...)`
5. likely static installation machinery
   - the `0x0045D6EB .. 0x0045DB3B` region

That means `ImgFlip` is no longer just a lucky one-off family note.
It is the clearest end-to-end callback subsystem we have recovered so far.

## What This Means for the Broad Transfer Registry

The broad transfer side is not as complete as `ImgFlip`, but it is now on firmer footing than before.

We already had:

- lookup through `FUN_00688890(...)`
- use through `FUN_00688930(...)` and `FUN_00689A00(...)`
- provisional callback-record header shape

Now we can add:

- constructor/inserter through `FUN_00688720(...)`
- same shared base record family
- same intrusive/hash infrastructure

So the broad transfer registry is no longer a lookup-only abstraction.

It is a sibling registry built on the same underlying framework as the `ImgFlip` family.

## Best Current Architecture

The callback-record architecture now reads as:

1. shared registry-node / callback-record base
   - `PTR_DAT_00A2595C`
2. subsystem-specific constructor/inserter
   - e.g. `FUN_00688720(...)`
   - e.g. `FUN_0068F540(...)`
3. intrusive/hash insertion
   - `FUN_00473D80(...)`
4. subsystem-specific hashed lookup
   - `FUN_00688890(...)`
   - `FUN_0068F610(...)`
5. callback-record slot `0` dispatch
   - concrete family logic

That is a substantial upgrade over the earlier “callback record shape” model because it explains:

- how records are created
- how they are keyed
- how they are installed
- and how they are later found again

## What Is Still Open

This does not completely finish the callback-registry problem.

What is now strong:

- shared registry-node base
- two constructor/lookup registry pairs
- static installation evidence for the `ImgFlip` side

What remains open:

- the exact walker or helper that processes the static installer region
- the full installed key set for the `ImgFlip` family
- whether the broad transfer registry also uses a comparable static installer region
- the first non-`ImgFlip` concrete slot-`0` callback bodies reached through `FUN_00688890(...)`

## Best Next Step

The strongest next reverse step is now very specific:

- inspect the static initializer / registration region around `0x0045D6EB .. 0x0045DB3B`

That should expose:

- the installed `ImgFlip` callback records
- their exact key triples
- and, ideally, the direct binding between registry entries and the concrete family callbacks we already recovered

If that stalls, the next-best sibling target is:

- find the comparable installation path for the broad transfer registry behind `FUN_00688720(...)`

because that would tell us whether the wider callback system is installed the same way.
