# GWCA GameThread DXTN Placement Addendum

This pass closes the one big holdout left in the compressed-format seam:

- internal format `0x16`
- external FourCC `DXTN`

The goal was to stop treating `DXTN` as “present in the DDS seam, but not yet placed” and determine how it actually sits inside the parent compressed-block builder.

## 1. DDS seam confirms `DXTN -> 0x16`

This part was already visible before, but it is worth restating because the new result builds on it directly.

From `FUN_006A0330(...)`:

- FourCC `0x4E545844`
- string `"DXTN"`
- maps to internal format id:
  - `0x16`

From `FUN_006A0630(...)`:

- internal format `0x16`
- exports back to FourCC:
  - `"DXTN"`

So `0x16` is definitely not an analysis artifact or an unnamed internal-only code. It is a real DDS-facing format family in this build.

## 2. Static format-descriptor rows for `0x0F..0x16`

Using the static descriptor table rooted at `DAT_00A271F8` and the capability table region around `DAT_00A27340`, the compressed families now line up like this.

### Per-format 12-byte descriptor rows

The dword triples at `DAT_00A271F8 + format_id * 0x0C` for `0x0F..0x16` are:

- `0x0F` (`DXT1`) -> `4, 1, 0`
- `0x10` (`DXT2`) -> `8, 4, 0`
- `0x11` (`DXT3`) -> `8, 4, 0`
- `0x12` (`DXT4`) -> `8, 4, 0`
- `0x13` (`DXT5`) -> `8, 4, 0`
- `0x14` (`DXTA`) -> `4, 4, 0`
- `0x15` (`DXTL`) -> `8, 0, 0`
- `0x16` (`DXTN`) -> `8, 0, 0`

Whatever the full semantic meaning of the three dwords is, one important structural result is already clear:

- `DXTL`
- `DXTN`

share the same static descriptor row.

That immediately suggests `DXTN` belongs closer to `DXTL` than to the earlier DXT2/3 or DXT4/5 pairs.

### Capability words for the same range

Using the `format_id * 4` view over the capability region, the relevant entries are:

- `0x0F` (`DXT1`) -> `0x71`
- `0x10` (`DXT2`) -> `0xB1`
- `0x11` (`DXT3`) -> `0xB1`
- `0x12` (`DXT4`) -> `0xB1`
- `0x13` (`DXT5`) -> `0xB1`
- `0x14` (`DXTA`) -> `0xA1`
- `0x15` (`DXTL`) -> `0x11`
- `0x16` (`DXTN`) -> `0x201`

This is the part that finally separates `DXTN` from the others in a meaningful way.

## 3. What `0x201` does inside `FUN_0069E870(...)`

The orchestration body `FUN_0069E870(...)` derives:

- `local_3c = local_18 & 0x280`
- `local_30 = -(uint)(local_3c != 0) & 2`
- `local_18 = local_18 & 0x210`
- `local_34 = (param_2 != 0x15) - 1 & 2`

For `DXTN = 0x16`, using capability word `0x201`:

- `local_3c = 0x201 & 0x280 = 0`
- `local_30 = 0`
- `local_18 = 0x201 & 0x210 = 0x200`
- `local_34 = (0x16 != 0x15) - 1 & 2 = 0`

That means the key branch in `FUN_0069E870(...)` resolves like this:

```c
if ((local_18 == 0) || (local_3c != 0)) {
    ...
}
else if (param_2 != 0x15) {
    FUN_0069cc40(...);
}
...
if (local_18 != 0) {
    FUN_0069c3f0(...);
}
```

For `DXTN` specifically:

- the first `if` is false
- the `else if (param_2 != 0x15)` is true
- so `FUN_0069CC40(...)` runs
- and because `local_18 != 0`, `FUN_0069C3F0(...)` also runs

That is the missing placement result.

## 4. DXTN is not fallback-only

This is the most important correction from the pass.

Before this pass, the strongest safe wording was:

- `DXTN` is in the DDS map, but its exact compact-family placement is still unclear

After this pass, the stronger and better wording is:

- `DXTN` is a distinct compressed-family branch that uses:
  - `FUN_0069CC40(...)`
  - `FUN_0069C3F0(...)`
- and does not use the same specialized branches as:
  - `DXT2 / DXT3`
  - `DXT4 / DXT5 / DXTA / DXTL`

So `DXTN` is not “the uninteresting leftover.” It has a real and specific branch profile.

## 5. Updated family grouping

With `DXTN` placed, the compressed-family map is much cleaner:

### `DXT1` (`0x0F`)

- descriptor row: `4, 1, 0`
- capability word: `0x71`
- strongest anchor for the BC1-like color-block / representative-block family

### `DXT2 / DXT3` (`0x10 / 0x11`)

- descriptor rows: `8, 4, 0`
- capability words: `0xB1`
- use:
  - `FUN_0069B720(...)`
  - plus the `256 x 256` `FUN_0069D8F0(...)` post-swizzle under the special chunk flag

### `DXT4 / DXT5` (`0x12 / 0x13`)

- descriptor rows: `8, 4, 0`
- capability words: `0xB1`
- use:
  - `FUN_0069BCD0(...)`
  - `FUN_0069C3F0(...)`

### `DXTA` (`0x14`)

- descriptor row: `4, 4, 0`
- capability word: `0xA1`
- still grouped with the upper family in control flow:
  - `FUN_0069BCD0(...)`
  - `FUN_0069C3F0(...)`
- but distinguished by its static descriptor row

### `DXTL` (`0x15`)

- descriptor row: `8, 0, 0`
- capability word: `0x11`
- grouped with the upper family for:
  - `FUN_0069BCD0(...)`
  - `FUN_0069C3F0(...)`
- but still has its own special fallback-layout behavior in the parent builder

### `DXTN` (`0x16`)

- descriptor row: `8, 0, 0`
- capability word: `0x201`
- uses:
  - `FUN_0069CC40(...)`
  - `FUN_0069C3F0(...)`
- does **not** share the same explicit specialized branch as:
  - `DXT2 / DXT3`
  - or `DXT4 / DXT5 / DXTA / DXTL`

This makes `DXTN` look like a sibling of `DXTL` at the static-layout level, but a distinct branch at the compact-family level.

## 6. Best current interpretation of DXTN

The cleanest current interpretation is:

- `DXTN` is a named DDS-facing compressed format family
- it shares storage-layout characteristics with `DXTL`
- but its compact-family reconstruction path is different
- specifically, it uses the:
  - `FUN_0069CC40(...)`
  - `FUN_0069C3F0(...)`
  path

That is a much stronger result than “it exists in the mapping table.”

## 7. What still remains unresolved

Even with `DXTN` now placed, there are still two narrower open questions:

1. what exact external compressed semantics `DXTL` and `DXTN` correspond to beyond the engine’s own FourCC names
2. whether the `0x201` capability word for `DXTN` implies one more special-case branch elsewhere in the image-transfer / import-export layer

So the remaining uncertainty is not about branch placement anymore. It is about exact external interpretation.

## 8. Best next step

The strongest next move is now to chase the format-capability consumers that care about the unusual `0x201` shape, especially:

- more callers of `FUN_00689E90(...)`
- neighbors around:
  - `FUN_0069E870(...)`
  - `FUN_006A0330(...)`
  - `FUN_006A0630(...)`
- and any logic that branches on:
  - `& 0x200`
  - `& 0x210`
  - `& 0x280`

That should reveal whether `DXTN`’s extra `0x200` capability bit is only a compact-family routing bit or whether it also changes import/export or upload behavior elsewhere.
