# GWCA GameThread Callback Plane And Handler Registry Addendum

This pass follows the first true consumer seam one layer deeper.

The last note established that `ImgMem` per-level slabs are first interpreted by:

- callback planes selected by `FUN_00688930(...)`
- and a format-keyed handler registry used by `FUN_0069AB50(...)`

This pass confirms both parts directly and makes the separation much cleaner.

Supporting logs:

- [gw_findcallers_00688930_temp138.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00688930_temp138.log)
- [gw_findcallers_0069ab50_temp137.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0069ab50_temp137.log)
- [gw_decomp_callback_handler_seam_temp138.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_handler_seam_temp138.log)

## 1. `FUN_00688930(...)`: one-plane vs two-plane callback selection

This helper is much smaller and cleaner than it first looked:

```c
void FUN_00688930(int src_fmt, int dst_fmt, uint flags, int same_aux,
                  int *out_plane0, int *out_plane1)
{
    if ((src_fmt != dst_fmt) || (((flags & 1) == 0) ? same_aux : 1) != 0) {
        key = { src_fmt, dst_fmt, flags };
        plane = FUN_00688890(&key);
        *out_plane0 = plane;
        if (plane != 0) {
            *out_plane1 = 0;
            return;
        }
    }

    key0 = { 0, dst_fmt, 0 };
    key1 = { src_fmt, 0, flags };
    *out_plane0 = FUN_00688890(&key0);
    *out_plane1 = FUN_00688890(&key1);
    if (*out_plane0 == 0) abort();
    if (*out_plane1 == 0) abort();
}
```

That gives the cleanest callback-plane model yet:

- first try a single combined transfer callback keyed by:
  - source format
  - destination format
  - transfer flags
- if no combined callback exists, fall back to a two-plane composition:
  - plane 0 keyed only by destination side
  - plane 1 keyed only by source side plus flags

So the stored block payload is not interpreted by one universal decoder.

It is interpreted by either:

- one direct format-pair callback
- or two staged callbacks composed together

That makes the earlier “primary stream plus companion stream” result fit even better. The engine already expects multi-plane interpretation at the callback-selection layer.

## 2. Caller map for the callback selector

`FUN_00688930(...)` has only two callers in the explored build:

- `FUN_00689A00(...)`
- `FUN_00688A10(...)`

That is important because it keeps the seam narrow.

- `FUN_00689A00(...)` is the broad initialization / import path we already had
- `FUN_00688A10(...)` is the rectangle/subregion blit path used by higher-level texture operations

So the callback selector really is the central plane-routing seam for these stored format payloads.

## 3. `FUN_0069AB50(...)`: format-keyed handler registry is real

The previous note already showed that `FUN_0069AB50(...)` dispatches through a global handler table. This pass makes the shape of that registry more concrete.

Important globals used by `FUN_0069AB50(...)`:

- `DAT_00BDB6A8`
- `DAT_00BDB6A4`
- `DAT_00BDB6B4`
- `DAT_00BDB6B0`

The function:

- hashes or indexes by format id
- walks a compact node/bucket structure
- validates it found the exact format record
- then dispatches through handler methods at:
  - slot `+0x00`
  - slot `+0x04`
  - slot `+0x08`

So this is not a generic “recompute mips” helper.

It is a real format-keyed virtual handler registry.

## 4. What the registry methods do

Inside the level loop, `FUN_0069AB50(...)`:

- computes current and next level dimensions
- derives the current level pointer and previous/adjacent level pointer
- chooses one of three handler methods based on how width and height shrink

The split is:

- method `+0x00`
  - used when both dimensions shrink between levels
- method `+0x04`
  - used when width is stable but height shrinks
- method `+0x08`
  - used when height is stable but width shrinks

That is a meaningful refinement, because it shows the registry is not just keyed by format family. It also owns the per-format downscale/update logic for different shrink cases.

So the first consumer layer above `ImgMem` is now clearly two-part:

1. callback-plane selection for inter-format transfer
2. format-handler registry for intra-format level update / propagation

## 5. New callers of `FUN_0069AB50(...)`

The xref map is broader than the last pass suggested.

Current callers:

- `FUN_00689D40(...)`
- `FUN_0064C340(...)`
- `FUN_0064B640(...)`
- `FUN_0064BB40(...)`
- `FUN_00665520(...)`
- `FUN_00664EC0(...)`

That means the handler registry is not just for one setup path. It sits behind:

- processor initialization/fill paths
- image-copy / blit paths
- clone/assign style paths
- and the default/fallback setup path

So this is a shared interpretation seam, not a narrow utility.

## 6. `FUN_00689D40(...)`: sub-rectangle update bridge

This helper is especially useful because it bridges a rectangular subregion operation into `FUN_0069AB50(...)`.

It:

- validates power-of-two dimensions
- derives a level-relative subrectangle
- computes local dimensions
- then calls:

```c
FUN_0069ab50(&local_14, format, &local_c, piVar1 - 1, 1, 2);
```

So `FUN_0069AB50(...)` is not just “build missing mip levels for a whole image.”

It is also reused as a compact format-aware update engine for subregion-derived level propagation.

That makes the handler methods more likely to understand concrete stored block layout, not just abstract image metadata.

## 7. `FUN_0064C340(...)`: non-six-bank object uses the same seam

`FUN_0064C340(...)` builds an object through `FUN_006903C0(...)` and then immediately routes into:

- `FUN_00689A00(...)`
- optional `FUN_0069AB50(...)`

This is the `0x8`-capable side of the family, since `FUN_006903C0(...)` preserves the auxiliary side only when capability bit `0x8` is set.

That matters because it shows:

- the callback-plane seam is used on the auxiliary-capable side too
- the handler-registry seam is reused there too

So neither seam is exclusive to the non-`0x8` DXT-family branch.

## 8. `FUN_0064B640(...)`: rectangle blit falls into `FUN_00688A10(...)`

This is the strongest supporting caller for the callback-plane split.

When the blit covers a whole level, it directly calls:

```c
FUN_00689a00(...)
```

When the blit is a subrectangle, it instead calls:

```c
FUN_00688a10(...)
```

And we already know `FUN_00688A10(...)` is the other caller of `FUN_00688930(...)`.

So the callback selector is used in both:

- full-level processing
- and subrectangle processing

That makes it a true plane-routing core, not just an initialization detail.

## 9. What this means for the companion stream

This pass does not yet give the exact final role name for the companion stream, but it sharpens the boundary substantially.

We now know:

- the stored slab layout can be interpreted through:
  - one combined format-pair callback
  - or two staged callback planes
- per-format level-update logic is handled by a separate virtual registry
- both seams are reused broadly across:
  - full-level processing
  - subregion blits
  - clone/assign paths
  - default/fallback paths

So the companion stream is very likely not a mere optional trailer.

It is part of the payload that one of these two layers interprets:

- callback-plane objects selected by `FUN_00688930(...)`
- or format-registry handlers reached by `FUN_0069AB50(...)`

The strongest current guess is still:

- primary stream = scalar/selector-side block plane
- companion stream = BC1-style color-side block plane

But now we can say more carefully where that interpretation must live:

- probably in the format-pair callback objects chosen by `FUN_00688890(...)`
- and secondarily in the level-update handler objects in the `DAT_00BDB6A*` registry

## 10. Best current model

The current storage-to-consumer chain is now:

1. compressed-family rebuild:
   - `FUN_0069E1C0(...)`
2. persistent storage:
   - `ImgMem` per-level slabs
3. first interpretation seam:
   - callback-plane selection via `FUN_00688930(...)`
4. first update/propagation seam:
   - format-keyed virtual handler registry via `FUN_0069AB50(...)`

That is a much better structural stopping point than “some function probably reads both streams somewhere.”

We now know the two exact dispatch seams where that code should live.

## 11. Next best step

The strongest next reverse step is to go after the concrete objects behind:

- `FUN_00688890(...)`
  - the callback-plane selector
- the registry records reached by `FUN_0069AB50(...)`

That should finally expose:

- which callback object reads the companion stream
- whether the companion plane is treated as color, endpoint, atlas, or another engine-local block role
