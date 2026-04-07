# GWCA GameThread Outer Decode Front-Door Boundary Addendum

This pass follows the `DXTA` question one layer outward from the compressed-family core into the two decode-side front doors:

- `FUN_00654E10(...)`
- `FUN_00655100(...)`

The goal was to test whether the missing `0x210` trait shows up as a broad image-pipeline distinction at the outer decode seam, or whether it stays local to the compressed-family machinery we have already mapped.

The result is pretty clean:

- the outer decode front doors do **not** appear to treat `0x210` as a first-class feature bit
- they mainly care about a lower capability bit, `0x8`
- so the `DXTA` oddness still looks like an inner compressed-family distinction, not a broad decode-pipeline mode

## 1. What the two outer callers actually are

Both functions are decode/load front doors:

- probe the input against a sequence of format recognizers
- choose a format-specific decode callback
- allocate an `ImgMem` processor through `FUN_006903C0(...)`
- optionally allocate an auxiliary temporary resource when capability bit `0x8` is set
- run the chosen decode callback
- optionally depalettize afterward

The main probe sequence is the same in both functions:

- `FUN_0069E6A0(...)`
- `FUN_006A0330(...)`
- `FUN_0069F7F0(...)`
- `FUN_0069FEC0(...)`
- `FUN_006A1880(...)`
- `FUN_006A1FD0(...)`
- `FUN_006A40A0(...)`

and when the DDS probe succeeds:

- `FUN_006A0330(...)` identifies the format id
- the chosen decode callback is `FUN_006A01D0(...)`

That means these are broad image front doors, not compressed-format-special wrappers.

## 2. `FUN_00654E10(...)`: capability use stays low-bit

After selecting the format and callback, `FUN_00654E10(...)` does:

```cpp
local_1c = FUN_00689e90(local_8);
...
local_18 = FUN_006903c0(local_8,&local_24,0,-(uint)((local_1c & 8) != 0) & (uint)&local_14);
(*local_10)(...);
if (((local_1c & 8) == 0) || (param_4 != (undefined4 *)0x0)) {
  ...
}
else {
  ...
  iVar4 = FUN_006a16b0(local_14);
  ...
  FUN_00689a00(..., local_14, 0xd, ...);
  ...
}
```

So at this outer layer, the format capability word is used primarily for:

- `& 8`

which controls:

- whether a temporary auxiliary resource is created
- and whether a later depalettize-style conversion path is run

There is no comparable outer branch here on:

- `& 0x210`
- `& 0x200`
- or `& 0x280`

So the broad decode front door does not treat `DXTA` as a special pipeline family the way the inner compressed-family code does.

## 3. `FUN_00655100(...)`: same conclusion on the stream/file path

`FUN_00655100(...)` is the sibling front door that starts from an owned/streamed input object rather than a raw pointer/length pair.

Its post-probe logic is the same in the part that matters:

```cpp
local_20 = FUN_00689e90(local_8);
if ((param_2 != (undefined4 *)0x0) || ((param_4 != (int *)0x0 && ((local_20 & 8) != 0)))) {
  local_10 = FUN_006903c0(local_8,&local_30,0,-(uint)((local_20 & 8) != 0) & (uint)&local_14);
  (*pcVar6)(...);
  if (((local_20 & 8) == 0) || (param_3 != (undefined4 *)0x0)) {
    ...
  }
  else {
    ...
    iVar4 = FUN_006a16b0(local_14);
    ...
    FUN_00689a00(..., local_14, 0xd, ...);
    ...
  }
}
```

So the sibling front door confirms the same boundary:

- the outer decode seam keys off capability bit `0x8`
- not off the `0x210` trait that made `DXTA` a partial member deeper inside the compressed-family branch logic

## 4. What that means for the `DXTA` question

This is the most important outcome of the pass.

If `DXTA`'s missing `0x210` trait were a broad top-level decode mode, we would expect to see it influencing:

- front-door callback selection
- temporary-resource creation
- or post-decode conversion paths

But these outer callers mainly care about:

- probe success
- chosen decode callback
- capability bit `0x8`

So the evidence keeps pointing inward:

- `DXTA`'s specialness is not at the outer decode front door
- it is at the inner compressed-family seam

That fits the earlier result that `DXTA` is a partial member specifically inside:

- `FUN_0069E870(...)`
- `FUN_0069E1C0(...)`

rather than across the whole image subsystem.

## 5. What `FUN_00689E90(...)` looks like at this boundary

The decompile of `FUN_00689E90(...)` itself is still minimal:

```cpp
undefined4 FUN_00689e90(int param_1)
{
  if (0x1a < param_1) {
    FUN_00487260(0x98);
  }
  return (&DAT_00a27340)[param_1];
}
```

As before, the decompiler is not expressing the table type very helpfully, but the important practical result remains:

- outer decode callers are sampling low capability bits from this table
- especially `0x8`
- and not showing any new broad behavior tied to `0x210`

So the current capability-boundary story still holds.

## 6. Best current interpretation

The cleanest current interpretation is:

- `0x8` is a broad outer-pipeline trait
  - temporary auxiliary resource
  - depalettize/post-conversion support
- `0x210` is not a broad outer-pipeline trait
  - it remains an inner compressed-family discriminator

That is useful because it narrows the remaining search.

If we want to know what `DXTA` lacks relative to `DXT4 / DXT5 / DXTL`, the answer is probably **not** in the generic image entrypoints. It is more likely in:

- the compressed-family inner workers
- or the format-specific callback bodies selected before they return to the outer front door

## 7. Best next step

The strongest next move is to stay near the compressed-family core and inspect the format-specific decode callback side that these front doors select for DDS/compressed formats.

The best candidates are:

- `FUN_006A01D0(...)`
- `FUN_0069F580(...)`
- or the inner helpers they immediately delegate to

That should tell us whether the missing `0x210` trait corresponds to:

- a second compressed plane
- a companion channel
- or another compressed-family payload stream that `DXTA` deliberately does not carry
