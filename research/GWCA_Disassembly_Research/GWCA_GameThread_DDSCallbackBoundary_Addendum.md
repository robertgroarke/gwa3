# GWCA GameThread DDS Callback Boundary Addendum

This pass follows the next inner target from the outer decode front doors:

- `FUN_006A01D0(...)`

The working hypothesis was that this DDS-side callback might be where the missing `0x210` trait finally turns into a concrete payload-plane difference.

The important result is a correction:

- `FUN_006A01D0(...)` is **not** the deep compressed-family decoder
- it is a lightweight DDS slab/level walker

That is useful because it moves the boundary cleanly:

- the generic DDS callback seam is mostly bookkeeping
- the real `DXTA` vs upper-family distinction still lives deeper in the compressed-family core

## 1. `FUN_006A01D0(...)`: level-size walker, not deep decode logic

The decompile is very small:

```cpp
void FUN_006a01d0(int param_1,uint param_2,undefined4 param_3,undefined4 param_4,uint param_5,
                 undefined4 param_6,int param_7)
{
  undefined4 uVar1;
  int iVar2;
  uint uVar3;
  uint uVar4;
  undefined1 local_c [8];
  
  if (param_7 != 0) {
    FUN_00487260(0x8e);
  }
  uVar1 = FUN_0068ae30(param_3);
  FUN_0068b010(local_c,param_3);
  uVar4 = 0;
  uVar3 = *(int *)(param_1 + 4) + 4;
  if (param_5 != 0) {
    do {
      iVar2 = FUN_0068cc20(uVar1,local_c,param_4,uVar4);
      uVar3 = uVar3 + iVar2;
      if (param_2 < uVar3) {
        FUN_00487260(0x98);
      }
      FUN_0046d790(iVar2);
      uVar4 = uVar4 + 1;
    } while (uVar4 < param_5);
  }
  return;
}
```

What it actually does:

- gets the format descriptor through `FUN_0068AE30(...)`
- gets the block-dimension/aux layout through `FUN_0068B010(...)`
- walks mip/level count `param_5`
- asks `FUN_0068CC20(...)` for each level’s byte size
- advances through the payload by those sizes
- validates that the source buffer is large enough

There is no per-format branch inside it, and no sign of the `DXTA`-specific trait split.

So this function is best described as:

- a DDS level-size validator / payload-advance helper

not:

- a compressed-format semantic decoder

## 2. `FUN_006A0280(...)`: six-bank sibling, same boundary

Its sibling callback makes the same point from the multi-bank side:

```cpp
void FUN_006a0280(..., int param_6)
{
  ...
  do {
    ...
    local_14 = *(undefined4 *)(param_6 + local_10 * 4);
    if (param_5 != 0) {
      ...
      iVar2 = FUN_0068cc20(uVar1,local_1c,param_4,uVar4);
      uVar3 = uVar3 + iVar2;
      ...
    }
    local_10 = local_10 + 1;
  } while (local_10 < 6);
}
```

This is the six-bank version of the same general job:

- iterate payload banks
- iterate levels inside each bank
- advance and validate by shared level-size math

So both DDS-side callbacks near the front door are about:

- payload layout accounting

not about:

- compressed-family semantics

That is an important boundary correction.

## 3. Where the broad `& 8` trait does become concrete: `FUN_006A1710(...)`

To avoid losing the useful part of this pass, I also decompiled `FUN_006A1710(...)`, because the outer front doors were clearly using `capability & 8` to decide whether the temporary auxiliary resource existed.

This function finally shows what that broad low-bit trait looks like in practice.

Highlights:

- it expects `param_5 == 1`
- it reads `FUN_00689E90(format)`
- if `(capability & 8) != 0`, it rewinds the payload pointer by `0x301`
- then it expands the main payload into image bytes
- and if `(capability & 8) != 0`, it parses a trailing 256-entry color table

The palette section is explicit:

```cpp
if ((uVar4 & 8) != 0) {
  if (*pbVar11 != 0xc) {
    FUN_00487260(0xab);
  }
  ...
  do {
    ...
    *(int **)(param_7 + uVar4 * 4) = param_4;
    ...
  } while (uVar4 < 0x100);
}
```

So the low capability bit:

- `& 8`

really is a broad outer-pipeline trait for:

- appended palette-table presence
- and the later depalettize flow we had already inferred from the outer callers

That strengthens the earlier conclusion that `0x8` and `0x210` belong to different layers of meaning.

## 4. `FUN_006A16B0(...)`: palette-table quick test

The helper used by the outer caller is also simple:

```cpp
undefined4 FUN_006a16b0(int param_1)
{
  char *pcVar1;
  uint uVar2;
  
  uVar2 = 0;
  pcVar1 = (char *)(param_1 + 3);
  do {
    if (*pcVar1 != -1) {
      return 1;
    }
    uVar2 = uVar2 + 1;
    pcVar1 = pcVar1 + 4;
  } while (uVar2 < 0x100);
  return 0;
}
```

This is essentially:

- "does the 256-entry auxiliary table contain any non-default alpha bytes?"

That matches the outer depalettize decision very well.

Again, this is a broad palette-side feature check, not the inner `DXTA`-specific compressed-family distinction.

## 5. Best current boundary

This pass lets us draw the seam much more cleanly:

### Outer DDS callback layer

- `FUN_006A01D0(...)`
- `FUN_006A0280(...)`

Role:

- payload sizing
- level/bank walking
- bounds validation

### Broad auxiliary/palette trait layer

- `FUN_006A1710(...)`
- `FUN_006A16B0(...)`

Role:

- optional appended palette table
- depalettize support
- driven by `capability & 8`

### Inner compressed-family distinction

- still **not** explained here
- still points inward toward:
  - `FUN_0069E1C0(...)`
  - `FUN_0069E870(...)`
  - and the family decoders they invoke

So the search for the missing `0x210` trait should not stay at the DDS callback boundary.

## 6. What this says about `DXTA`

This pass makes one thing much less likely:

- `DXTA` is probably **not** special because DDS parsing or generic level walking treats it differently

Instead, the evidence still says:

- `DXTA` shares the same broad DDS callback/bookkeeping path as its neighboring compressed formats
- and its oddness only emerges once the compressed-family inner workers are selected

So the `DXTA` question remains where the previous notes left it:

- a compressed-family inner distinction

not:

- a generic DDS callback difference

## 7. Best next step

The strongest next move is to go back inward rather than outward:

- `FUN_0069F580(...)`
- and/or the inner helpers immediately beneath the compressed-family decoders

That should be the shortest route to a concrete statement like:

- "`0x210` means a second compact stream is present"
- or
- "`0x210` means the format carries a companion block plane that `DXTA` lacks"

At this point, that is much more likely than finding the answer in any higher DDS wrapper.
