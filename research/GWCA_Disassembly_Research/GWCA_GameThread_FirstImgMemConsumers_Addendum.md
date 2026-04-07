# GWCA GameThread First ImgMem Consumers Addendum

This pass follows the `ImgMem` storage objects one layer farther than the earlier storage-boundary note. The goal was to find the first code that treats the per-level slabs as meaningful payload rather than just opaque storage.

The key result is that the first real interpretation boundary is not the generic `ImgMem` constructor layer. It is a format-specific processor/handler layer built on top of those slabs.

Supporting logs:

- [gw_findcallers_00690430_temp133.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00690430_temp133.log)
- [gw_findcallers_00690620_temp133.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00690620_temp133.log)
- [gw_findcallers_006902c0_temp133.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_006902c0_temp133.log)
- [gw_decomp_imgmem_consumers_temp134.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgmem_consumers_temp134.log)
- [gw_findcallers_0064b2f0_temp134.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0064b2f0_temp134.log)
- [gw_findcallers_00690890_temp135.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00690890_temp135.log)
- [gw_findcallers_006905a0_temp135.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_006905a0_temp135.log)
- [gw_decomp_imgmem_parents_temp135.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgmem_parents_temp135.log)
- [gw_decomp_processor_consumers_temp136.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_processor_consumers_temp136.log)

## 1. Caller map: the storage constructors are shallow wrappers

The xref map is much narrower than it first looked:

- `FUN_00690430(...)` has one caller:
  - `FUN_006905A0(...)`
- `FUN_00690620(...)` has two callers:
  - `FUN_0064B2F0(...)`
  - `FUN_00690890(...)`
- `FUN_006902C0(...)` has two callers:
  - `FUN_006903C0(...)`
  - `FUN_00690620(...)`

That means the storage constructors themselves are still mostly infrastructure. The first likely payload consumers are the wrappers and the object methods above them.

## 2. `FUN_006905A0(...)`: six-bank storage front door

`FUN_006905A0(...)` is just a guarded front door for the six-bank constructor:

```c
void FUN_006905a0(int format, undefined4 dims, undefined4 level_count) {
    capability = FUN_00689e90(format);
    if ((~(capability >> 3) & 1) == 0) abort();
    geom = FUN_0068b010(..., format);
    desc = FUN_0068ae30(format);
    FUN_00690430(desc, geom, dims, level_count);
}
```

The important point is the guard:

- this path rejects formats whose capability has bit `0x8`
- so the six-bank storage object belongs to the non-auxiliary side of the image family

## 3. `FUN_00690890(...)`: single-bank rebuild front door

`FUN_00690890(...)` is the matching thin wrapper around `FUN_00690620(...)`:

```c
void FUN_00690890(undefined4 imgmem, int format, undefined4 dims, undefined4 levels) {
    capability = FUN_00689e90(format);
    if ((~(capability >> 3) & 1) == 0) abort();
    geom = FUN_0068b010(..., format);
    desc = FUN_0068ae30(format);
    FUN_00690620(imgmem, desc, geom, dims, levels);
}
```

So the single-bank rebuild helper and the six-bank constructor live on the same non-`0x8` side of the format tree.

## 4. `FUN_0064B2F0(...)`: add-level path for an owning object

`FUN_0064B2F0(...)` is the first higher-level method that treats the `ImgMem` object as persistent state:

- it validates:
  - object flags
  - format match
  - halved-dimension mip expectations
- it increments the stored level count
- it updates stored width and height
- it rebuilds the `ImgMem` object through `FUN_00690620(...)`
- it immediately computes the byte size of level `0` via `FUN_0068CC20(...)`
- then it calls `FUN_0046D790(...)` and `FUN_0064C2C0()`

This is still not the place where the two stored block streams are decoded semantically. It is an owner method that resizes and refreshes the persistent storage object.

## 5. `FUN_0064C010(...)`: remove-level path

`FUN_0064C010(...)` is the matching remove-level path:

- it validates the object state
- halves stored dimensions downward
- decrements the level count
- rebuilds the `ImgMem` object through `FUN_00690890(...)`
- optionally calls `FUN_0064C2C0()`

Again, this confirms that the `ImgMem` slab layout is treated as persistent multi-level storage, but not yet interpreted generically.

## 6. `FUN_00665520(...)`: first real consumer seam

This is the most useful new result.

`FUN_00665520(...)` is the first function in this path that clearly turns the `ImgMem` storage object into an active processing cluster.

Important behavior:

- if no existing processor cluster exists, it computes or validates a level count
- it stores:
  - format
  - width/height
  - level count
- it creates a six-bank object through `FUN_006905A0(...)`
- then it iterates six times:

```c
FUN_00689a00(
    *(undefined4 *)(uVar4 + *(int *)(in_ECX + 0x10)),
    0,
    *(undefined4 *)(in_ECX + 0x24),
    0,
    *(undefined4 *)(uVar4 + param_1),
    0,
    *(undefined4 *)(in_ECX + 0x24),
    0,
    in_ECX + 0x14,
    uVar3,
    0,
    0
);
```

Then, if not all levels are active, it iterates six times again and calls:

```c
FUN_0069ab50(
    *(undefined4 *)(uVar4 + *(int *)(in_ECX + 0x10)),
    format,
    dims,
    0,
    requested_levels,
    stored_levels
);
```

This is the first strong consumer boundary:

- the `ImgMem` slabs are handed into six per-bank processing objects
- those processors are then initialized and incrementally updated through two dedicated helpers

## 7. `FUN_00664EC0(...)`: fallback/default-format path uses the same seam

`FUN_00664EC0(...)` is a default/error-recovery path that seeds a tiny `4 x 4` `DXT1 / 0x0F` object.

Even here, the engine does the same thing:

- create the six-bank object with `FUN_006905A0(0x0F, ...)`
- iterate six times calling `FUN_00689A00(...)`
- if there is more than one level, iterate six times calling `FUN_0069AB50(...)`

That makes the seam general rather than caller-specific.

## 8. `FUN_00689A00(...)`: processor initialization is callback-driven

`FUN_00689A00(...)` does not decode the stored format itself inline. It chooses callback planes and then feeds per-level data through them.

Important behavior:

- it probes capability on both source and destination formats with `FUN_00689E90(...)`
- if bit `0x8` is present, it preserves the auxiliary/palette-side resource handle
- otherwise it clears that side to zero
- it computes default per-level offset tables through `FUN_006887E0(...)` if the caller did not supply them
- it chooses one or two callback planes through `FUN_00688930(...)`
- if needed, it creates the temporary auxiliary resource with `FUN_006A1610(...)`
- then for each level it invokes the selected callback(s)

The call shape is the important part:

- generic setup code computes offsets, dimensions, and optional temp resources
- actual interpretation is delegated through callback objects returned by `FUN_00688930(...)`

So the first semantic consumer of the stored slabs is already callback-polymorphic.

## 9. `FUN_0069AB50(...)`: incremental update uses a format-handler table

`FUN_0069AB50(...)` is even more explicit about the first real interpretation layer.

It:

- validates the format with `FUN_00689E90(...)`
- looks up a handler from the global table:
  - `DAT_00BDB6A8`
  - `DAT_00BDB6A4`
  - `DAT_00BDB6B4`
  - `DAT_00BDB6B0`
- halves dimensions level by level
- and then dispatches through handler vtable slots:
  - slot `+0x00`
  - slot `+0x04`
  - slot `+0x08`

The level-update loop is the key:

- it walks the per-level pointer table
- it reads current and previous level pointers
- it chooses a handler method based on the shrinking-dimension case
- and it lets the format handler interpret the actual stored payload

So the exact meaning of the primary and companion per-block streams is not decided by `ImgMem` itself.

It is decided here:

- in a format-keyed handler registry
- through per-format virtual methods

## 10. What this means for the companion stream

This pass gives a cleaner answer to the question we were chasing.

What we now know:

- `FUN_0069E1C0(...)` reconstructs the per-level slabs
- those slabs definitely contain:
  - a primary two-word-per-block stream
  - and, when `capability & 0x210 != 0`, a companion two-word-per-block stream
- the slabs are stored persistently in `ImgMem`
- but the first code that interprets them is not a generic `ImgMem` reader
- instead, the first interpretation boundary is:
  - callback planes selected by `FUN_00688930(...)`
  - and a format-keyed handler table consumed by `FUN_0069AB50(...)`

So the companion stream’s exact engine role is now best described as:

- a format-specific stored companion block plane
- not a generic `ImgMem` field with one universal interpretation

That matches the earlier `DXTA` result very well:

- `DXTA` lacks the `0x210`-gated companion plane
- therefore `DXTA`’s format handler family never receives that extra stored block plane to interpret

## 11. Best current model

At this point the storage-to-consumer chain looks like:

1. `FUN_0069E1C0(...)`
   - rebuild per-level slabs
   - primary stream always present
   - companion stream only when `capability & 0x210 != 0`
2. `ImgMem`
   - persists those slabs by level
3. `FUN_00665520(...)` / `FUN_00664EC0(...)`
   - create and seed six per-bank processor objects
4. `FUN_00689A00(...)`
   - initialize processors through selected callback planes
5. `FUN_0069AB50(...)`
   - incrementally update levels through a format-keyed handler vtable

So the next true semantic step is no longer inside `ImgMem`. It is inside the format handlers and callback planes.

## 12. Next best step

The strongest next reverse target is now:

- the format-specific handler objects behind the global registry used by `FUN_0069AB50(...)`
- and the callback objects selected by `FUN_00688930(...)`

Those are the most likely places to find the first code that explicitly reads both:

- the primary scalar/selector stream
- and the companion BC1-style block stream

That should finally let us replace “companion BC1-style block plane” with the engine’s exact role for it.
