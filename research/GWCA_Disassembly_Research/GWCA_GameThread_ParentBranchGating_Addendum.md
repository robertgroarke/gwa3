# GWCA GameThread Parent Branch Gating Addendum

This pass goes back up to the parent compressed-block builder:

- `FUN_0069E870(...)`

The goal was to stop describing the richer stage-1 family only by downstream behavior and instead pin down the exact parent-side gating:

- when `FUN_0069CC40(...)` is chosen
- when `FUN_0069BCD0(...)` is chosen
- and how that lines up with the DXT-family format ids and capability bits

The result is useful for two reasons:

1. the branch logic is now explicit rather than inferred
2. it forces one important correction:
   - `DXTA` does participate in the richer stage-1 family
   - but it does **not** share the exact same second-stage gating as `DXT4 / DXT5 / DXTL`

## 1. The relevant parent-side fields

`FUN_0069E870(...)` starts with:

```cpp
local_18 = FUN_00689e90(param_2);
...
local_3c = local_18 & 0x280;
local_30 = -(uint)(local_3c != 0) & 2;
local_18 = local_18 & 0x210;
local_34 = (param_2 != 0x15) - 1 & 2;
local_14 = (-(uint)(local_18 != 0) & 2) + local_34 + local_30;
```

So the meaningful branch selectors are:

- `local_3c = capability & 0x280`
- `local_18 = capability & 0x210`
- `local_34 = 2` only for `DXTL / 0x15`, otherwise `0`

Using the already recovered capability words:

- `DXT1 / 0x0F -> 0x71`
- `DXT2 / 0x10 -> 0xB1`
- `DXT3 / 0x11 -> 0xB1`
- `DXT4 / 0x12 -> 0xB1`
- `DXT5 / 0x13 -> 0xB1`
- `DXTA / 0x14 -> 0xA1`
- `DXTL / 0x15 -> 0x11`
- `DXTN / 0x16 -> 0x201`

that gives:

### `capability & 0x210`

- `DXT1` -> `0x10`
- `DXT2` -> `0x10`
- `DXT3` -> `0x10`
- `DXT4` -> `0x10`
- `DXT5` -> `0x10`
- `DXTA` -> `0x00`
- `DXTL` -> `0x10`
- `DXTN` -> `0x200`

### `capability & 0x280`

- `DXT1` -> `0x00`
- `DXT2` -> `0x80`
- `DXT3` -> `0x80`
- `DXT4` -> `0x80`
- `DXT5` -> `0x80`
- `DXTA` -> `0x80`
- `DXTL` -> `0x00`
- `DXTN` -> `0x200`

That already hints that the parent uses two overlapping format traits rather than one flat family bit.

## 2. Exact gating for `FUN_0069CC40(...)`

The parent-side branch is:

```cpp
if ((local_18 == 0) || (local_3c != 0)) {
  if ((param_2 == 0x10) || (param_2 == 0x11)) {
    FUN_0069b720(...);
  }
}
else if (param_2 != 0x15) {
  FUN_0069cc40(...);
}
```

So `FUN_0069CC40(...)` runs only when:

- `local_18 != 0`
- `local_3c == 0`
- `param_2 != 0x15`

Applying that to the known formats:

- `DXT1 / 0x0F`:
  - `local_18 != 0`
  - `local_3c == 0`
  - not `0x15`
  - so `FUN_0069CC40(...)` runs

- `DXTN / 0x16`:
  - `local_18 != 0`
  - `local_3c == 0`
  - not `0x15`
  - so `FUN_0069CC40(...)` runs

No other DXT-family format in this group satisfies that same combination.

So the structural trivial-block peel is specifically the stage-1 family for:

- `DXT1`
- `DXTN`

That is a cleaner statement than we had before.

## 3. Exact gating for `FUN_0069B720(...)`

Still inside the same first-stage branch:

```cpp
if ((local_18 == 0) || (local_3c != 0)) {
  if ((param_2 == 0x10) || (param_2 == 0x11)) {
    FUN_0069b720(...);
  }
}
```

So `FUN_0069B720(...)` is not driven purely by capability bits. It is explicitly format-id gated to:

- `DXT2 / 0x10`
- `DXT3 / 0x11`

That keeps the older `DXT2 / DXT3` family boundary intact.

## 4. Exact gating for `FUN_0069BCD0(...)`

The richer stage-1 prepass is much more direct:

```cpp
if ((((param_2 == 0x12) || (param_2 == 0x13)) || (param_2 == 0x14)) || (param_2 == 0x15)) {
  FUN_0069bcd0(...);
}
```

This means `FUN_0069BCD0(...)` is **not** gated by `local_18` or `local_3c` at all.

It is explicitly chosen for:

- `DXT4 / 0x12`
- `DXT5 / 0x13`
- `DXTA / 0x14`
- `DXTL / 0x15`

That matters because it makes the stage-1 family boundary fully explicit:

- `DXT1`, `DXTN` -> `FUN_0069CC40(...)`
- `DXT2`, `DXT3` -> `FUN_0069B720(...)`
- `DXT4`, `DXT5`, `DXTA`, `DXTL` -> `FUN_0069BCD0(...)`

So the richer repeated-selector alpha/intensity peel is definitely a real four-format group at stage 1.

## 5. Exact gating for `FUN_0069C3F0(...)`

The shared heavier second stage is gated separately:

```cpp
if (local_18 != 0) {
  FUN_0069c3f0(..., param_2 == 0xf);
}
```

So `FUN_0069C3F0(...)` runs whenever `capability & 0x210 != 0`.

That gives:

- `DXT1 / 0x0F` -> yes
- `DXT2 / 0x10` -> yes
- `DXT3 / 0x11` -> yes
- `DXT4 / 0x12` -> yes
- `DXT5 / 0x13` -> yes
- `DXTA / 0x14` -> no
- `DXTL / 0x15` -> yes
- `DXTN / 0x16` -> yes

This is the important correction.

Earlier shorthand grouped:

- `DXT4 / DXT5 / DXTA / DXTL`

too tightly around the second-stage family.

The parent logic shows the truer statement is:

- all four share the richer stage-1 prepass `FUN_0069BCD0(...)`
- but only `DXT4`, `DXT5`, and `DXTL` also take the `FUN_0069C3F0(...)` second stage
- `DXTA` does not, because its capability word clears `0x210`

## 6. Best current family map

With the parent-side gating in hand, the cleanest map is now:

### `DXT1 / 0x0F`

- stage 1: `FUN_0069CC40(...)`
- stage 2: `FUN_0069C3F0(...)`

### `DXT2 / 0x10`

- stage 1: `FUN_0069B720(...)`
- stage 2: `FUN_0069C3F0(...)`
- plus the special `256 x 256` post-pass we already mapped elsewhere

### `DXT3 / 0x11`

- stage 1: `FUN_0069B720(...)`
- stage 2: `FUN_0069C3F0(...)`
- plus the same special post-pass family

### `DXT4 / 0x12`

- stage 1: `FUN_0069BCD0(...)`
- stage 2: `FUN_0069C3F0(...)`

### `DXT5 / 0x13`

- stage 1: `FUN_0069BCD0(...)`
- stage 2: `FUN_0069C3F0(...)`

### `DXTA / 0x14`

- stage 1: `FUN_0069BCD0(...)`
- stage 2: no `FUN_0069C3F0(...)`

### `DXTL / 0x15`

- stage 1: `FUN_0069BCD0(...)`
- stage 2: `FUN_0069C3F0(...)`

### `DXTN / 0x16`

- stage 1: `FUN_0069CC40(...)`
- stage 2: `FUN_0069C3F0(...)`

## 7. What this says about naming the richer prepass

This parent-side view supports the "repeated-selector alpha/intensity peel" reading, but it also shows we should stay a little careful with naming.

Why:

- `DXT4`, `DXT5`, and `DXTL` fit a "richer selector/alpha-family plus second-stage family" story neatly
- `DXTA` fits the richer selector-family stage 1, but drops out before the shared second stage

So the safest name right now is still:

- repeated-selector alpha/intensity prepass

rather than locking it too early to one single external codec label.

The DXT5-style alpha decoder inside `FUN_0069DCE0(...)` is real, but the parent-family membership is still a little more engine-local than a one-line external codec taxonomy would suggest.

## 8. Best next step

The strongest next move is to explain why `DXTA / 0x14` is the odd member:

- it joins `FUN_0069BCD0(...)`
- but skips `FUN_0069C3F0(...)`

So the best next target is:

- compare `DXTA / 0x14` against `DXT4 / 0x12`, `DXT5 / 0x13`, and `DXTL / 0x15`
- especially around any remaining `FUN_00689E90(...)` consumers and `FUN_0069E1C0(...)` / `FUN_0069E870(...)` inner branches

That should tell us whether `DXTA` is best understood as:

- an alpha-only variant
- an intensity-only variant
- or an engine-local hybrid format that shares only the first prepass with the upper family
