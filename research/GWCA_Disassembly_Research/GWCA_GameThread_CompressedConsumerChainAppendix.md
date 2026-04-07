# GWCA GameThread Compressed Consumer Chain Appendix

This appendix consolidates the first real consumer chain above the compressed rebuild shell.

The goal is to answer one practical question cleanly:

- once `FUN_0069E1C0(...)` reconstructs the primary and optional companion block planes, where does the engine first interpret them?

The current answer is now stable:

- not in generic `ImgMem` storage code
- but at two higher dispatch seams:
  - callback-plane selection through `FUN_00688930(...)`
  - format-handler registry dispatch through `FUN_0069AB50(...)`

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_FirstImgMemConsumers_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_FirstImgMemConsumers_Addendum.md)
- [GWCA_GameThread_CallbackLookupRegistry_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackLookupRegistry_Addendum.md)
- [GWCA_GameThread_CallbackPlaneAndHandlerRegistry_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackPlaneAndHandlerRegistry_Addendum.md)
- [GWCA_GameThread_CallbackRecordShape_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackRecordShape_Addendum.md)
- [GWCA_GameThread_DXTUpperFamilySplitAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTUpperFamilySplitAppendix.md)

## The Consumer Chain

The strongest current end-to-end chain is:

1. `FUN_0069E1C0(...)`
   - rebuild per-level block slabs
   - primary stream always present
   - companion BC1-style stream only when `capability & 0x210 != 0`
2. `ImgMem`
   - store those per-level slabs persistently
3. processor/object setup
   - `FUN_00665520(...)`
   - `FUN_00664EC0(...)`
4. callback-plane dispatch
   - `FUN_00689A00(...)`
   - `FUN_00688A10(...)`
   - `FUN_00688930(...)`
   - `FUN_00688890(...)`
5. format-handler update/propagation
   - `FUN_0069AB50(...)`

That is the first clean point where the engine stops treating the slabs as opaque storage and starts routing them to format-aware code.

## Why `ImgMem` Is Not the Semantic Layer

The `ImgMem` constructors and rebuild wrappers are now well bounded.

They:

- validate format capability
- allocate or rebuild level storage
- preserve dimensions and level count
- expose persistent per-level pointers

But they do not inline the real format interpretation logic.

That matters because it keeps the compressed-family semantics from getting mixed up with storage management. The first meaningful interpretation of the primary and companion planes happens later.

## First Dispatch Seam: Callback Planes

The first interpretation seam is callback-plane routing.

### `FUN_00688930(...)`

This helper now reads cleanly as:

- first try one combined callback keyed by:
  - source format
  - destination format
  - transfer flags
- if that does not exist, fall back to two staged callbacks:
  - destination-side plane
  - source-side plane

So the engine explicitly supports:

- one-plane interpretation
- or two-plane composition

That fits the recovered slab model very well.

### `FUN_00688890(...)`

This is the hashed callback-record lookup.

It returns:

- a callback record pointer

not:

- a bare function id
- or a tiny enum token

The current registry-owner layout is:

- `this + 0x0C`
  - packed-key offset inside each record
- `this + 0x10`
  - bucket table base
- `this + 0x18`
  - bucket count / bound
- `this + 0x1C`
  - bucket mask

### Callback record shape

The strongest current provisional callback-record header is:

- `record + 0x00`
  - dispatch-table / vtable-like pointer
- `record + 0x04`
  - key field `0`
- `record + 0x08`
  - key field `1`
- `record + 0x0C`
  - key field `2`
- `record + owner.key_offset`
  - packed combined key

And `FUN_00689A00(...)` shows that slot `0` of the dispatch table is the first invoked method.

So the first real code body that can understand the stored block planes is very likely:

- callback-record dispatch slot `0`

## Second Dispatch Seam: Format Handler Registry

The second interpretation seam is the format-keyed handler registry behind `FUN_0069AB50(...)`.

This layer is different from the callback-plane path.

It is not about:

- source/destination transfer pair selection

It is about:

- per-format level update and propagation

The current registry is reached through:

- `DAT_00BDB6A8`
- `DAT_00BDB6A4`
- `DAT_00BDB6B4`
- `DAT_00BDB6B0`

And the handler methods selected by `FUN_0069AB50(...)` are:

- slot `+0x00`
  - both dimensions shrink
- slot `+0x04`
  - width stable, height shrinks
- slot `+0x08`
  - height stable, width shrinks

So the engine has two different higher-level interpretation seams:

- callback records for transfer/initialization
- handler records for per-format level propagation

## What This Means for the Companion Plane

The companion plane is now much better bounded architecturally.

The current strong statement is:

- the companion BC1-style plane is a format-specific stored block plane
- not a generic `ImgMem` field with one universal meaning

Its meaning should live in:

- callback records selected by `FUN_00688930(...)`
- and possibly format-handler objects reached through `FUN_0069AB50(...)`

That also sharpens the `DXTA` result:

- `DXTA` lacks the `0x210`-gated companion plane
- so `DXTA`-side callback or handler families will never receive that extra stored color-side plane to interpret

## Best Current Practical Model

If we need to place a newly found consumer quickly, the fastest classifier is:

1. if it works through `FUN_00689A00(...)` or `FUN_00688A10(...)`
   - treat it as callback-plane interpretation
2. if it works through `FUN_0069AB50(...)`
   - treat it as format-handler propagation/update logic
3. if it only allocates or rebuilds `ImgMem`
   - do not treat it as the semantic consumer yet

That should keep future reverse passes from conflating:

- storage
- transfer-plane callbacks
- and format-local level handlers

## Best Next Step

The strongest remaining target is now very specific.

It is no longer:

- find any code near the codec shell

It is:

- identify one concrete callback record instance and recover its slot-`0` body

That should finally expose the first method that can read:

- the primary scalar/alpha-side block plane
- and, when present, the companion BC1-style color plane

If the callback path stalls, the next-best parallel seam is:

- one concrete handler object behind `FUN_0069AB50(...)`

because that is the other place where the stored pair should become semantically explicit.
