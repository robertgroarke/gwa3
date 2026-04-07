# GWCA GameThread ImgFlip Callback Families Addendum

This pass pushes one step deeper from the abstract callback-record seam into a concrete callback-family cluster.

The key result is that `FUN_0068FE40(...)` is an `ImgFlip.cpp`-anchored family dispatcher, and the callbacks it selects are no longer generic mysteries. They split cleanly into four concrete compressed-format families:

- `DXT1`
- `DXT2 / DXT3`
- `DXT4 / DXT5 / DXTL`
- `DXTA`

Supporting log:

- [gw_decomp_registry_manager_candidates_temp143.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_registry_manager_candidates_temp143.log)
- [gw_decomp_imgflip_callback_families_temp144.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgflip_callback_families_temp144.log)

## 1. `FUN_0068FE40(...)`: `ImgFlip.cpp` family dispatcher

This helper is much more revealing than it first looked.

Important behavior:

- anchored by `P:\\Code\\Engine\\Gr\\Img\\ImgFlip.cpp`
- probes:
  - block geometry via `FUN_0068B010(...)`
  - block bit width via `FUN_0068AE30(...)`
  - format capability via `FUN_00689E90(...)`
- then classifies formats into four family ids when `capability & 1 != 0`

The family mapping is explicit:

```c
switch(format) {
case 0x0F: local_34 = 0; break;              // DXT1
case 0x10:
case 0x11: local_34 = 1; break;              // DXT2 / DXT3
case 0x12:
case 0x13:
case 0x15: local_34 = 2; break;              // DXT4 / DXT5 / DXTL
case 0x14: local_34 = 3; break;              // DXTA
}
```

Then it resolves a callback record through:

```c
local_20 = (undefined4 *)FUN_0068f610(&local_34);
```

So this is the first concrete family dispatcher we’ve recovered above the more generic callback-plane layer.

## 2. `FUN_0068F610(...)`: second hashed callback registry

`FUN_0068F610(...)` is another hashed lookup over callback records, very similar in shape to `FUN_00688890(...)`, but with a different key packer:

```c
uVar3 = param_1[2] + param_1[1] ^ *param_1 << 2 ^ extra;
bucket = *(uint *)(this + 0x1c) & uVar3;
node = *(int *)(this + 0x10) + bucket * 0xc;
```

It matches:

- `record + 0x04 == field0`
- `record + 0x08 == field1`
- `record + 0x0C == field2`
- `record + owner.key_offset == packed_key`

The only twist is:

- if `field0 >= 8`, the key packer mixes in `FUN_0046DCF0()`

So `FUN_0068F610(...)` appears to be a sibling hashed callback registry specialized for the `ImgFlip` family-dispatch path.

## 3. What the `ImgFlip` callback record does

After `FUN_0068F610(...)` returns a callback record, `FUN_0068FE40(...)` immediately uses:

```c
(**(code **)*local_20)(level_ptr, &param_3, byte_span);
```

So once again:

- callback record at `+0x00` points to a dispatch table
- slot `0` is the active method body

But now we have a concrete family classification wrapped around it, so the slot-0 callback is no longer “some arbitrary format-pair handler.” It is one of four `ImgFlip`-family handlers.

## 4. `FUN_0068F6C0(...)`: `DXT1` family callback

This helper matches a BC1/DXT1-style selector transform:

- expects dimensions `1..4`
- works over `param_1 + 4`, i.e. the second dword of the block pair
- repeatedly extracts two-bit selector fields
- mirrors/repositions them into a new packed dword
- uses `3` as the fill selector for padded area

That is a strong match for:

- `DXT1` / BC1-style selector remapping under horizontal/vertical flip-style transforms

So family `0` is no longer just “the `DXT1` family bucket.” It has a concrete per-block callback body with BC1-style selector behavior.

## 5. `FUN_0068F840(...)`: `DXT2 / DXT3` family callback

This helper is wider:

- updates:
  - `param_1[0]`
  - `param_1[1]`
  - `param_1[3]`
- uses:
  - four-bit nibble extraction for one plane
  - two-bit selector extraction for another plane

That lines up well with:

- explicit 4-bit alpha plane
- plus BC1-like 2-bit selector plane

So family `1` is a good fit for:

- `DXT2 / DXT3`
- i.e. the explicit-alpha compressed family

## 6. `FUN_0068FA70(...)`: `DXT4 / DXT5 / DXTL` family callback

This helper is the richest of the four:

- updates:
  - `param_1[0]`
  - `param_1[1]`
  - `param_1[3]`
- uses:
  - three-bit packed fields for one plane
  - two-bit selector extraction for another plane
- preserves the high 16 bits of `param_1[1]`

That matches the family we already know has:

- a repeated-selector scalar/alpha-style stream
- plus a companion BC1-style color stream

So family `2` is a strong fit for:

- `DXT4 / DXT5 / DXTL`
- i.e. the interpolated-alpha upper family

This is especially important because it fits the earlier two-stream model cleanly.

## 7. `FUN_0068FCB0(...)`: `DXTA` family callback

This helper is narrower again:

- only updates:
  - `param_1[0]`
  - `param_1[1]`
- uses three-bit field extraction
- does not touch the BC1-style selector word at `param_1[3]`

That matches the `DXTA` partial-membership result almost perfectly:

- `DXTA` belongs to the upper scalar/selector family
- but lacks the companion BC1-style stream

So family `3` is no longer speculative at all. It is exactly the kind of callback body we would expect for a format that carries the scalar/selector side but not the color-block companion side.

## 8. Why this is a meaningful bridge

This is the strongest callback-family bridge so far because the four helpers line up directly with the compressed-family results we already had:

- `DXT1`
  - BC1-style selector-only family
- `DXT2 / DXT3`
  - explicit 4-bit-alpha + selector family
- `DXT4 / DXT5 / DXTL`
  - 3-bit interpolated scalar/alpha + BC1-style selector/color family
- `DXTA`
  - 3-bit scalar/alpha family without the companion BC1-style plane

That means the old `DXTA` interpretation is now supported from a new angle:

- not just from build/decode branch gating
- but from concrete family callback bodies in `ImgFlip.cpp`

## 9. `FUN_0068FE40(...)` fallback path also respects the family split

The fallback path is useful too.

When the direct family callback cannot handle the remaining levels cleanly, `FUN_0068FE40(...)`:

1. creates a temporary object through `FUN_0047EF00(...)`
2. gets another callback via `FUN_0068F610(...)` with family id `0x20`
3. uses `FUN_00689A00(...)` to convert into the temp object
4. runs the temporary callback
5. uses `FUN_00689A00(...)` again to convert back

So even the fallback path is built around the same callback-family model rather than abandoning it.

That suggests the callback-family layer is a real engine abstraction, not a set of special cases.

## 10. What this says about the companion stream

This pass gives the strongest practical confirmation yet of the companion-stream model.

The concrete callback families now line up like this:

- `FUN_0068F6C0(...)`
  - BC1-style selector remap family
- `FUN_0068F840(...)`
  - explicit-alpha + selector family
- `FUN_0068FA70(...)`
  - interpolated-alpha/scalar + selector/color family
- `FUN_0068FCB0(...)`
  - interpolated-alpha/scalar family without the extra BC1-style companion side

So `DXTA`’s special status is no longer just “it fails the `0x210` gate.”

It now has a matching concrete callback body:

- it manipulates the scalar/selector plane
- and does not manipulate the BC1-style companion side that the upper-family callback does

That is a very strong semantic bridge.

## 11. Best current model

The callback side now looks like this:

1. broad transfer plane selection:
   - `FUN_00688930(...)`
2. broad callback-record lookup:
   - `FUN_00688890(...)`
3. `ImgFlip`-family dispatch:
   - `FUN_0068FE40(...)`
4. `ImgFlip` callback lookup:
   - `FUN_0068F610(...)`
5. concrete family callbacks:
   - `FUN_0068F6C0(...)` for `DXT1`
   - `FUN_0068F840(...)` for `DXT2 / DXT3`
   - `FUN_0068FA70(...)` for `DXT4 / DXT5 / DXTL`
   - `FUN_0068FCB0(...)` for `DXTA`

That is the strongest callback-family recovery we have so far.

## 12. Next best step

The strongest next reverse step is to inspect the callback registries that back:

- `FUN_0068F610(...)`
- and `FUN_00688890(...)`

for their population/build path, because we now know those registries lead to concrete callback families. The other high-value option is to keep pushing inside the family helpers’ neighboring siblings so we can identify exactly which block fields correspond to:

- the BC1-style companion side
- the explicit-alpha side
- the interpolated-alpha side
