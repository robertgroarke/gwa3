# GWCA GameThread ImgMem Storage Boundary Addendum

This pass follows the reconstructed block streams one step outward into the storage object that the decode and build callbacks target:

- `FUN_006903C0(...)`
- `FUN_006901E0(...)`
- `FUN_00690260(...)`
- `FUN_00690430(...)`
- `FUN_00690620(...)`

The goal was to answer a narrower question than the last few codec notes:

- once the primary and companion streams are reconstructed, where do they actually live?

The result is useful even though it does not yet name a final rendering consumer:

- the reconstructed block streams land in `ImgMem` per-level payload slabs
- those slabs are persistent format-sized storage regions, not just ephemeral scratch arrays

So the primary/companion split is not just an internal decoder convenience. It is written into the engine’s image-storage object model.

## 1. `FUN_006903C0(...)`: the front door into `ImgMem`

The constructor front door is small and clear:

```cpp
void FUN_006903c0(int param_1,undefined4 param_2,undefined4 param_3,undefined4 *param_4)
{
  if (param_4 != (undefined4 *)0x0) {
    uVar1 = FUN_00689e90(param_1);
    if ((uVar1 & 8) == 0) {
      *param_4 = 0;
      param_4 = (undefined4 *)0x0;
    }
  }
  uVar2 = FUN_0068b010(local_c,param_1);
  uVar3 = FUN_0068ae30(param_1);
  FUN_006902c0(uVar3,uVar2,param_2,param_3,param_4);
}
```

This confirms the object-building order:

- get format capability word
- maybe request auxiliary/palette support through the low `& 8` trait
- get format descriptor from `FUN_0068AE30(...)`
- get block/layout metadata from `FUN_0068B010(...)`
- build the storage object through `FUN_006902C0(...)`

So the decode callbacks are not writing to an ad hoc local buffer. They are writing into the image memory object built here.

## 2. `FUN_006901E0(...)`: size planning for the payload slabs

This helper computes the memory layout of one `ImgMem` storage form:

```cpp
void FUN_006901e0(..., uint param_4, uint param_5, int param_6, int *param_7, uint *param_8, int *param_9)
{
  *param_7 = param_5 * 4;
  uVar2 = -(uint)(param_6 != 0) & 0x400;
  *param_8 = uVar2;
  uVar2 = *param_7 + uVar2;
  ...
  while (param_4 != 0) {
    iVar1 = FUN_0068cc20(param_1,param_2,param_3,param_4);
    uVar3 = uVar3 + 3 + iVar1 & 0xfffffffc;
  }
  *param_9 = uVar3 - uVar2;
}
```

The important structural parts are:

- `param_5 * 4` bytes for a pointer table
- optional `0x400` aux block when the low broad trait requests it
- then aligned per-level payload slabs sized by `FUN_0068CC20(...)`

So the payload layout under `ImgMem` is:

- pointer table
- optional auxiliary block
- level slabs

That lines up cleanly with the earlier image-storage work.

## 3. `FUN_00690260(...)`: level pointer table builder

This helper actually fills the pointer table:

```cpp
void FUN_00690260(int param_1,..., uint param_5, uint param_6, uint param_7)
{
  while (uVar2 != 0) {
    *(uint *)(param_1 + uVar2 * 4) = param_1 + param_7;
    iVar1 = FUN_0068cc20(...,uVar2);
    param_7 = param_7 + 3 + iVar1 & 0xfffffffc;
  }
  ...
}
```

So each level gets:

- one pointer entry
- into a real aligned payload slab for that mip level

That means when a callback like `FUN_0069E1C0(...)` receives its destination plane pointer array, it is writing directly into persistent per-level storage blocks.

## 4. `FUN_00690430(...)`: six-bank storage object

This function is the six-bank constructor we had already associated with one `ImgMem` mode.

The important part here is not just that it allocates six banks, but how:

```cpp
do {
  *(int *)(local_14 + local_c * 4) = iVar4;
  ...
  while (uVar6 != 0) {
    *(uint *)(iVar4 + uVar6 * 4) = iVar4 + uVar5;
    iVar3 = FUN_0068cc20(...,uVar6);
    uVar5 = uVar5 + 3 + iVar3 & 0xfffffffc;
  }
  ...
  iVar4 = iVar4 + iVar2;
  local_c = local_c + 1;
} while (local_c < 6);
```

So the six-bank form is:

- top-level bank pointer table
- six repeated bank records
- each bank contains its own level-pointer table plus aligned level slabs

That is a real persistent storage topology, not a temporary decode scratch buffer.

## 5. `FUN_00690620(...)`: single-bank grow/rebuild form

The sibling constructor/grower shows the same thing for the other storage mode:

- count levels
- build a new pointer table
- lay out level slabs
- free or replace the old object as needed

So both `ImgMem` storage modes share the same core idea:

- pointer-table-driven persistent per-level slabs

The differences are:

- single-bank vs six-bank organization
- not whether the payload is “real” storage

## 6. What this means for the reconstructed block streams

This is the important bridge to the codec work.

Inside `FUN_0069E1C0(...)`, each level does:

- `local_c = *(uint **)(param_6 + local_34 * 4)`

and then the family helpers write their primary and companion block streams into data derived from that pointer.

Now that we have the `ImgMem` constructors in view, the clean interpretation is:

- `param_6` is a level-pointer table supplied by `ImgMem`
- `local_c` is the base of one persistent per-level payload slab
- the primary and companion streams are written directly into that slab layout

So the stream split is not:

- a temporary internal reconstruction that disappears immediately

It is:

- part of the concrete stored block layout of the decoded image object

That makes the `DXTA` distinction stronger again:

- `DXTA` does not merely “skip a helper”
- its stored per-level payload slab lacks the companion block stream region that `DXT5` and `DXTL` populate

## 7. What `0x210` does *not* control here

One useful negative result from this pass:

- the `ImgMem` constructors themselves do not branch on `0x210`
- the broad constructor-visible capability split is still mostly `& 8`

So:

- `0x210` is still an inner format-logic trait
- not a top-level storage-mode selector

That means the storage object is generic enough to hold either:

- primary-only layout
- or primary-plus-companion layout

and the format-specific callbacks decide which one gets populated.

## 8. Strongest current conclusion

The strongest current conclusion is:

- the primary and companion block streams are written into persistent `ImgMem` per-level payload slabs
- `DXTA` therefore differs in stored block layout, not just in transient decode behavior
- the absence of the companion stream for `DXTA` is a real content/layout distinction inside the image object

That is a meaningful upgrade from the previous note, because it confirms the stream split is part of storage, not just reconstruction.

## 9. Best next step

The strongest next move is to identify the first code that interprets the stored block slab after construction.

The best concrete targets are:

- consumers of the `ImgMem` bank/level pointers created by:
  - `FUN_00690430(...)`
  - `FUN_00690620(...)`
- or consumers immediately above `FUN_006903C0(...)` that inspect the stored block payload rather than just passing the object along

That should let us move from:

- “the companion stream is stored persistently”

to:

- “the engine uses that stored companion stream as X”

which is the last big semantic gap in this seam.
