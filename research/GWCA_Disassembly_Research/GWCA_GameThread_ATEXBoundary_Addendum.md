# GWCA GameThread ATEX Boundary Addendum

This pass followed the next decode-side target suggested by the outer front-door work:

- `FUN_0069F580(...)`

The hope was that this callback might expose the missing `0x210` trait as an extra compressed payload plane or companion block stream.

Instead, the main result is another boundary correction:

- `FUN_0069F580(...)` is not part of the inner DXT-family compressed-block seam
- it belongs to a separate `ATEX/ATTX`-style container family

That is still useful, because it prevents us from mixing two neighboring but different image paths together while chasing the `DXTA` question.

## 1. Caller boundary: `FUN_0069F580(...)` is its own front-door-selected family

I checked callers for `FUN_0069F580(...)`.

Current direct refs are:

- `FUN_00654E10 @ 00654e10`
- `FUN_00655100 @ 00655100`

Those are the same two outer decode front doors we just inspected.

So `FUN_0069F580(...)` is definitely one of the format-family callbacks selected by the generic image loader, but that alone does not make it a DXT-family inner worker.

The function body settles that.

## 2. `FUN_0069E6A0(...)`: the recognizer that selects this family

The recognizer is very explicit:

```cpp
undefined4 FUN_0069e6a0(int *param_1,uint param_2,undefined4 *param_3,uint *param_4,int *param_5)
{
  if ((param_2 < 0x14) || ((*param_1 != 0x58455441 && (*param_1 != 0x58545441)))) {
    return 0;
  }
  ...
  if (uVar3 == 0x31545844) *param_3 = 0xf;
  else if (uVar3 == 0x32545844) *param_3 = 0x10;
  else if (uVar3 == 0x33545844) *param_3 = 0x11;
  else if (uVar3 == 0x34545844) *param_3 = 0x12;
  else if (uVar3 == 0x35545844) *param_3 = 0x13;
  else if (uVar3 == 0x41545844) *param_3 = 0x14;
  else if (uVar3 == 0x4c545844) *param_3 = 0x15;
  else if (uVar3 == 0x4e545844) *param_3 = 0x16;
  ...
}
```

The outer magic check is:

- `0x58455441`
- `0x58545441`

which corresponds to:

- `ATEX`
- `ATTX`

Then the inner type tag maps to:

- `DXT1`
- `DXT2`
- `DXT3`
- `DXT4`
- `DXT5`
- `DXTA`
- `DXTL`
- `DXTN`

So this family is not "raw DDS." It is an engine-local `ATEX/ATTX` container that *carries* one of the internal DXT-family ids.

That distinction matters a lot.

## 3. `FUN_0069F580(...)`: row-oriented container decode, not inner block-family semantics

The callback body has two main jobs:

1. optional palette/table extraction into `param_7`
2. row-oriented payload copy / expansion into the destination image buffer

The first half:

- checks header fields like `*(int *)(param_1 + 0xe)` and `*(int *)(param_1 + 0x1e)`
- if `param_7 != 0`, it extracts up to 256 packed 32-bit palette entries
- fills any remainder with `0xff000000`

Then the image copy side:

- gets the destination bytes-per-pixel from `FUN_0068AE30(format) >> 3`
- gets a source-row byte count from `*(ushort *)(param_1 + 0x1c) >> 3`
- checks whether the container is in a special narrow mode
- and either:
  - copies/pads rows directly
  - or, in one special `1-byte` mode, calls `FUN_0069F200(...)`

This is all row/storage logic.

There is no sign here of:

- the compact-family masks
- `FUN_0069CC40(...)`
- `FUN_0069BCD0(...)`
- `FUN_0069C3F0(...)`
- `FUN_0069D660(...)`
- or any of the inner DXT-family block-state logic

So `FUN_0069F580(...)` is not the place where the missing `0x210` trait turns into a second compressed stream.

It is a container-level decode callback for the `ATEX/ATTX` image family.

## 4. Why this matters for the `DXTA` question

This is the useful narrowing:

- `FUN_0069F580(...)` does carry the same internal format ids, including `DXTA / 0x14`
- but it does so inside a different top-level container path than the DDS/compressed seam we were following

So if we keep using it as evidence for the DXT-family inner branch structure, we risk mixing:

- `ATEX/ATTX` container behavior

with:

- the actual DXT-family compressed block-family behavior

That would make the `DXTA` story less trustworthy, not more.

The cleanest interpretation now is:

- `ATEX/ATTX` is a separate container seam that can wrap the same internal format ids
- `FUN_0069F580(...)` decodes that container’s payload layout
- the `DXTA` partial-membership question still belongs to the DDS/compressed-family seam we mapped through:
  - `FUN_0069E870(...)`
  - `FUN_0069E1C0(...)`
  - and their block-family helpers

## 5. `FUN_0069FEC0(...)`: another separate family

The neighboring recognizer `FUN_0069FEC0(...)` also reinforces the same lesson.

It is not a DXT-family inner helper either.

Instead, it parses:

- a magic `0x3ADE68B1`
- a table of offsets
- and repeatedly probes each segment through `FUN_006A1880(...)`

while validating:

- consistent dimensions
- consistent format across slices
- and a bounded segment count

So this is another container/meta-family boundary, not the inner compressed-family trait split.

That makes the container seam map cleaner:

- `ATEX/ATTX` family via `FUN_0069E6A0(...)` -> `FUN_0069F580(...)`
- another offset-table container via `FUN_0069FEC0(...)`
- DDS family via `FUN_006A0330(...)` -> `FUN_006A01D0(...)`

and only some of those families eventually route into the compressed-block core we care about.

## 6. Best current boundary map

At this point the image decode surface is cleaner than it was before:

### Generic front doors

- `FUN_00654E10(...)`
- `FUN_00655100(...)`

### DDS-family recognizer/callback

- `FUN_006A0330(...)`
- `FUN_006A01D0(...)`

### `ATEX/ATTX` container family

- `FUN_0069E6A0(...)`
- `FUN_0069F580(...)`

### other container/meta family

- `FUN_0069FEC0(...)`

### inner compressed-family core

- `FUN_0069E870(...)`
- `FUN_0069E1C0(...)`
- and the stage-1/stage-2 helpers beneath them

So the `DXTA` partial-membership story should stay anchored in the last group, not generalized across all neighboring image callbacks.

## 7. Best next step

The strongest next move is to return directly to the compressed-family core and stay there.

The best candidates now are:

- more inner helpers beneath `FUN_0069E1C0(...)`
- or the exact worker pair around the `flag 8` family and the second stream copybacks

In practice, the highest-yield next step is probably:

- decompile the remaining inner helper(s) that consume the `local_24 + local_28` offset path in `FUN_0069E1C0(...)`
- and compare that against formats with and without `capability & 0x210`

That should be the shortest route to a concrete answer like:

- "`0x210` means the format carries the extra scalar stream at `base + local_24 + local_28`"

or

- "`0x210` gates a second recovered block plane that `DXTA` intentionally does not use"
