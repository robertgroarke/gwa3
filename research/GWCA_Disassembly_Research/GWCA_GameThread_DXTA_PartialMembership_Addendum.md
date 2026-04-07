# GWCA GameThread DXTA Partial-Membership Addendum

This pass focuses on the remaining odd member in the upper DXT-family cluster:

- `DXTA / 0x14`

The specific question was:

- why does `DXTA` share the richer stage-1 prepass with `DXT4 / DXT5 / DXTL`, but then fall out of the shared second stage?

The answer is now much cleaner because the same split appears on both sides of the seam:

- export/build in `FUN_0069E870(...)`
- decode/rebuild in `FUN_0069E1C0(...)`

So `DXTA` is not an oddity caused by only one one-sided helper. It is a real partial member of the richer family in both directions.

## 1. Decode-side mirror in `FUN_0069E1C0(...)`

The decode/rebuild function starts with the same capability-derived selectors as the builder:

```cpp
local_30 = FUN_00689e90(param_3);
local_38 = local_30 & 0x280;
local_24 = -(uint)(local_38 != 0) & 2;
local_30 = local_30 & 0x210;
local_28 = (param_3 != 0x15) - 1 & 2;
local_14 = (-(uint)(local_30 != 0) & 2) + local_28 + local_24;
```

So the decode side uses the same core format traits:

- `capability & 0x280`
- `capability & 0x210`
- the special `DXTL / 0x15` offset tweak

That already tells us the oddness is intentional and format-driven, not an accident of one branch body.

## 2. Stage-1 decode family: `DXTA` is included

Inside `FUN_0069E1C0(...)`, the richer stage-1 family is decoded by:

```cpp
if (((puVar2[1] & 4) != 0) &&
   (((param_3 == 0x12 || (param_3 == 0x13)) || ((param_3 == 0x14 || (param_3 == 0x15)))))) {
  FUN_0069d320(local_c,iVar8,iVar1,&local_50,uVar9,local_14);
}
```

This is the decode-side counterpart to the stage-1 prepass family.

It is explicitly chosen for:

- `DXT4 / 0x12`
- `DXT5 / 0x13`
- `DXTA / 0x14`
- `DXTL / 0x15`

So `DXTA` definitely belongs to the richer upper-family stage-1 path on both sides:

- build side: `FUN_0069BCD0(...)`
- decode side: `FUN_0069D320(...)`

That is the strongest confirmation so far that `DXTA` is not just accidentally adjacent to this family.

## 3. Stage-2 decode family: `DXTA` is excluded

The decode-side heavier second stage is gated separately:

```cpp
if (((puVar2[1] & 8) != 0) && (local_30 != 0)) {
  FUN_0069d660(local_c + local_24 + local_28,iVar8,iVar1,&local_50,uVar9,local_14,
               param_3 == 0xf);
}
```

and here `local_30` is:

- `FUN_00689E90(format) & 0x210`

For `DXTA / 0x14`, we already recovered:

- capability = `0xA1`
- `0xA1 & 0x210 = 0`

So `DXTA` is excluded from the heavier stage-2 family on decode for the same reason it was excluded on build:

- its capability word clears `0x210`

That means the partial-membership story is fully mirrored:

### `DXTA`

- yes to richer stage 1
- no to shared heavier stage 2

### `DXT4 / DXT5 / DXTL`

- yes to richer stage 1
- yes to shared heavier stage 2

This is no longer just a parent-branch observation. It is a full encode/decode structural fact.

## 4. Exact partial-family split

Putting `FUN_0069E870(...)` and `FUN_0069E1C0(...)` together:

### Stage 1 upper-family membership

- `DXT4`
- `DXT5`
- `DXTA`
- `DXTL`

### Stage 2 upper-family membership

- `DXT4`
- `DXT5`
- `DXTL`

### Odd member

- `DXTA`

So `DXTA` is best described as:

- a stage-1 member of the richer repeated-selector family
- but not a stage-2 member of the shared heavier compact family

That is a much better description than lumping it entirely with either side.

## 5. What that implies about `DXTA`

This narrows the likely interpretation of `DXTA`.

Because it participates in:

- the richer repeated-selector stage-1 family

but not in:

- the heavier second-stage family gated by `0x210`

the cleanest current interpretation is that `DXTA` preserves the upper family’s structured selector/scalar channel but lacks the extra trait that the heavier second stage expects.

So the current best guess is not:

- "DXTA is unrelated to the upper family"

but more like:

- "DXTA shares the upper family’s repeated-selector scalar channel, but not the additional channel/trait required for the heavier second-stage family"

That keeps the interpretation source-anchored without overcommitting to an external codec label too early.

## 6. Useful caller-side boundary

I also checked callers for `FUN_0069E1C0(...)`.

Current direct refs are:

- `FUN_00654E10 @ 00654e10`
- `FUN_00655100 @ 00655100`

with four total references.

So the next level outward is now well scoped:

- the decode/rebuild path is feeding into a small caller cluster
- if we need to tighten the semantic role of `DXTA`, those are the next best outer consumers to inspect

## 7. Best current conclusion

The strongest current conclusion is:

- `DXTA` is a real partial member of the upper compressed-family cluster
- that partial membership is symmetric across build and decode
- the dividing line is exactly the capability trait behind `& 0x210`

So the family should now be described as:

- upper stage-1 selector/scalar family:
  - `DXT4`, `DXT5`, `DXTA`, `DXTL`
- upper stage-2 heavier family:
  - `DXT4`, `DXT5`, `DXTL`
- partial member:
  - `DXTA`

## 8. Best next step

The strongest next move is to explain what the `0x210` trait practically means.

The most direct targets are:

- more `FUN_00689E90(...)` consumers
- especially the decode-side outer callers:
  - `FUN_00654E10(...)`
  - `FUN_00655100(...)`

That should tell us whether the missing `0x210` trait is best understood as:

- a second data plane
- a color companion plane
- or another engine-local compressed-family feature that `DXTA` intentionally lacks
