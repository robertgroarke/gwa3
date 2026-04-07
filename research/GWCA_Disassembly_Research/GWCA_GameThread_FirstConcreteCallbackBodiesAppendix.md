# GWCA GameThread First Concrete Callback Bodies Appendix

This appendix closes the next callback-side gap in the compressed consumer chain.

The previous consumer notes narrowed the first true semantic target to:

- callback-record slot `0`

The `ImgFlip` callback-family work is now strong enough to say more than that.

The first concrete callback bodies are no longer abstract:

- `FUN_0068F6C0(...)`
- `FUN_0068F840(...)`
- `FUN_0068FA70(...)`
- `FUN_0068FCB0(...)`

These are concrete slot-`0` callback-family bodies selected through the `ImgFlip.cpp` callback registry path.

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_CompressedConsumerChainAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CompressedConsumerChainAppendix.md)
- [GWCA_GameThread_CallbackRecordShape_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackRecordShape_Addendum.md)
- [GWCA_GameThread_ImgFlipCallbackFamilies_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ImgFlipCallbackFamilies_Addendum.md)
- [GWCA_GameThread_DXTUpperFamilySplitAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTUpperFamilySplitAppendix.md)

## Callback Path

The current concrete callback path is:

1. broad transfer-plane routing
   - `FUN_00688930(...)`
2. broad callback-record lookup
   - `FUN_00688890(...)`
3. `ImgFlip` family dispatch
   - `FUN_0068FE40(...)`
4. `ImgFlip` callback-record lookup
   - `FUN_0068F610(...)`
5. callback-record slot `0`
   - concrete family body

That means the callback-object seam is no longer open-ended.

At least one important callback family is now resolved down to real method bodies.

## The `ImgFlip` Family Dispatcher

`FUN_0068FE40(...)` is now the strongest bridge from:

- abstract callback record

to:

- concrete callback family

It classifies compressed formats into four explicit family ids:

- family `0`
  - `DXT1`
- family `1`
  - `DXT2 / DXT3`
- family `2`
  - `DXT4 / DXT5 / DXTL`
- family `3`
  - `DXTA`

Then it resolves the callback record through:

- `FUN_0068F610(&local_34)`

and immediately invokes slot `0`.

So these are not just neighboring helpers that happen to look relevant.
They are concrete callback bodies reached through the recovered callback-record mechanism.

## Concrete Slot-`0` Bodies

### `FUN_0068F6C0(...)` -> `DXT1`

This family body:

- operates on the BC1/DXT1 selector word
- extracts two-bit selector fields
- mirrors or repositions those selectors
- uses selector `3` as padding fill

Best current reading:

- BC1-style selector remap callback

This is the clearest concrete callback body for the `DXT1` family.

### `FUN_0068F840(...)` -> `DXT2 / DXT3`

This family body:

- updates multiple block words
- manipulates a 4-bit nibble plane
- also manipulates a 2-bit selector plane

Best current reading:

- explicit-alpha plus BC1-style selector callback

So the `DXT2 / DXT3` pair now has a concrete callback-family body rather than only a stage-bit identity.

### `FUN_0068FA70(...)` -> `DXT4 / DXT5 / DXTL`

This family body:

- updates the scalar/alpha-side words
- also updates the BC1-style selector/color-side word
- uses three-bit packed fields on one side and two-bit selectors on the other

Best current reading:

- interpolated-alpha/scalar plus BC1-style companion callback

This is especially important because it is the first concrete callback body that visibly matches the recovered dual-stream upper-family model.

### `FUN_0068FCB0(...)` -> `DXTA`

This family body:

- manipulates the scalar/alpha-side words
- uses three-bit packed fields
- does not touch the BC1-style selector/color-side word

Best current reading:

- scalar/alpha-only upper-family callback

This is the strongest callback-side confirmation of the `DXTA` split:

- `DXTA` keeps the scalar/alpha side
- `DXTA` lacks the BC1-style companion side

## Why This Matters

This upgrades the callback seam in an important way.

Before this point, the best current statement was:

- the first real consumer should live in callback-record slot `0`

Now the stronger statement is:

- at least one callback-record family has already been recovered down to concrete slot-`0` bodies

And those bodies line up directly with the compressed-family taxonomy:

- `DXT1`
  - BC1-style selector family
- `DXT2 / DXT3`
  - explicit-alpha plus selector family
- `DXT4 / DXT5 / DXTL`
  - scalar/alpha plus BC1-style companion family
- `DXTA`
  - scalar/alpha-only upper-family family

So the callback side is no longer only an architectural guess.
It now materially reinforces the compressed-family stream model.

## Best Current Combined Model

Putting the codec and callback sides together:

- `FUN_0069E1C0(...)`
  - reconstructs primary scalar/alpha-side blocks
  - and, when `capability & 0x210 != 0`, companion BC1-style color-side blocks
- callback records
  - are selected through hashed registries
- `ImgFlip` callback slot `0` bodies
  - already show the concrete family-specific interpretation of those block layouts

That makes `ImgFlip` the first concrete callback-family bridge from:

- stored slab layout

to:

- named per-format block manipulation logic

## What Is Still Open

This does not yet mean the whole callback problem is finished.

What is resolved:

- one concrete callback-family cluster
- one concrete set of slot-`0` bodies
- the callback-side confirmation of the `DXTA` split

What remains open:

- the population/build path for the callback registries
- whether non-`ImgFlip` callback families reuse the same record/header structure exactly
- the first non-`ImgFlip` concrete callback bodies reached through `FUN_00688890(...)`

## Best Next Step

The best next callback-side target is now narrower than before.

It is no longer:

- find any slot-`0` body

It is:

- identify the population/build path behind `FUN_0068F610(...)` and `FUN_00688890(...)`
- or recover one non-`ImgFlip` callback family reached through the broader `FUN_00688890(...)` registry

That would tell us whether the `ImgFlip` family is just one well-recovered island inside the callback system, or the template for the broader callback-record architecture.
