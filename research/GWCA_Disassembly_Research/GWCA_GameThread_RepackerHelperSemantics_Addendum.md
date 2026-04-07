# GWCA GameThread Repacker Helper Semantics Addendum

This pass decompiles the four helpers behind the shared repacker:

- `FUN_006A5050(...)`
- `FUN_006A5160(...)`
- `FUN_006A53D0(...)`
- `FUN_006A5560(...)`

These are the helpers that `FUN_006A5CB0(...)` calls after the 16-entry working block has already been expanded and optionally normalized.

## Main Result

The strongest result is that the two `block_kind` families under `FUN_006A5CB0(...)` now separate cleanly:

- `block_kind == 2`
  - uses a nibble-seeded scalar/intensity overlay plus compact nibble re-emit
- `block_kind == 3`
  - uses an 8-level scalar ladder with a richer selector encoder and two distinct packing regimes

So the shared repacker is not just “rebuilding compressed bits.” It is choosing between two genuinely different compact record layouts.

## `FUN_006A5050(...)`: Nibble Overlay Inserter

`FUN_006A5050(uint *pixels16, uint *packed_src)` does one narrow job:

- read two packed 32-bit words from `packed_src`
- extract sixteen 4-bit values
- map each nibble through `DAT_00A2C3D8`
- write that mapped value into the high byte of each `pixels16[i]`

It preserves the lower 24 bits already present in each working pixel.

So this helper is not a full encoder or decoder by itself. It is the **nibble-seeded scalar/intensity overlay pass** for the `block_kind == 2` family.

That fits exactly with what `FUN_006A5F10(...)` already suggested for `block_kind == 2`.

## `FUN_006A53D0(...)`: Nibble Repacker

`FUN_006A53D0(uint *out_words, int pixels16)` is the inverse-side companion for that same family.

It:

- reads the alpha/high-byte from every fourth byte in the 16-entry working block
- scales each byte to a 4-bit value using `((alpha * 0xF + 0x80) / 0xFF)`
- shifts those nibbles into two packed 32-bit words

So `FUN_006A53D0(...)` is the **compact nibble repacker** for the `block_kind == 2` family.

Taken together:

- `FUN_006A5050(...)` = unpack nibble overlay into the working block
- `FUN_006A53D0(...)` = repack working-block scalar values back into the nibble stream

That makes `block_kind == 2` much easier to name:

- a **4-bit scalar/intensity block family layered over color entries**

## `FUN_006A5160(...)`: 8-Level Scalar Overlay Inserter

`FUN_006A5160(uint *pixels16, byte *packed_src)` performs the richer overlay for `block_kind == 3`.

It:

- reads two endpoint bytes
- builds an 8-entry scalar ladder
  - either 7-step interpolated when `end1 < end0`
  - or 5-step-plus-special cases when not
- reads two 24-bit selector streams
- writes one scalar-ladder entry into the high byte of each working pixel

Again, it preserves the lower 24 bits already present.

So this helper is the direct companion to the `block_kind == 3` expansion logic we already saw inside `FUN_006A5F10(...)`.

That means `block_kind == 3` is now firmly anchored as:

- an **8-level scalar/alpha overlay family**
- carried by endpoint bytes plus packed 3-bit selectors

## `FUN_006A5560(...)`: Rich 8-Level Repacker

`FUN_006A5560(uint *out_words, int pixels16)` is the most informative helper in the quartet.

It scans the 16 working scalar bytes and derives:

- local minimum nonzero value
- local maximum non-`0xFF` value
- plus a second upper sample used to decide the encoding regime

Then it chooses between two packing strategies.

### First regime: 8-level / 7-step family

When the observed scalar spread is large enough, it:

- quantizes each scalar into one of 8 ladder slots
- uses selector codes from `DAT_00A28350`
- packs those 3-bit selectors across two output words

This is the richer interpolated family.

### Second regime: 6-level-plus-special family

Otherwise it:

- reserves special codes for exact `0` and exact `0xFF`
- quantizes interior values into a smaller interpolated family
- uses selector codes from `DAT_00A28330`

So `FUN_006A5560(...)` is not a generic compressor. It is explicitly implementing the two classic scalar-ladder regimes for the richer `block_kind == 3` family.

That makes the family relationship much clearer:

- `FUN_006A5160(...)` = unpack 8-level scalar family into the working block
- `FUN_006A5560(...)` = repack working scalars back into the same kind of endpoint-plus-selector representation

## Best Current Family Map

With this pass, the compact emit families now read as:

### `block_kind == 2`

- color table already present in the working block
- scalar/intensity side comes from sixteen packed 4-bit values
- helper pair:
  - `FUN_006A5050(...)`
  - `FUN_006A53D0(...)`

Best name:

- **nibble-seeded scalar/intensity companion family**

### `block_kind == 3`

- color table already present in the working block
- scalar side comes from 8-level endpoint-plus-selector encoding
- helper pair:
  - `FUN_006A5160(...)`
  - `FUN_006A5560(...)`

Best name:

- **endpoint-and-selector scalar/alpha companion family**

## Why This Matters

This closes a big interpretive gap in the staged transfer model.

We can now say the one-sided tier is not merely:

- expand pixels
- do neighborhood math
- repack somehow

It is specifically:

1. expand a compact color-plus-scalar record into 16 working pixels
2. mix/rebuild those working pixels against an existing neighborhood
3. re-emit either:
   - a nibble scalar/intensity companion family, or
   - a richer endpoint-plus-selector scalar family

That is much closer to a real engine-internal block editing pipeline than a plain decode path.

## Relationship To Earlier Stream Work

This pass also fits well with the earlier primary/companion stream recovery:

- the richer scalar-side family continues to look like the primary scalar/selector stream
- the color side remains separate and can be preserved or rebuilt independently

So the staged transfer pipeline is now converging toward a stable interpretation:

- color block data
- scalar/intensity/alpha companion data
- intermediate 16-pixel working representation
- mode-driven neighborhood transform
- compact re-emission
- later final materialization in the `0x18` tier

## Next Step

The best next reverse step is to tie these helper families back to the exact earlier format families by walking more callers of:

- `FUN_006A53D0(...)`
- `FUN_006A5560(...)`
- `FUN_006A5050(...)`
- `FUN_006A5160(...)`

That should let us say which of the upper DXT-derived families use the nibble companion layout versus the richer 8-level scalar layout in each stage.

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_repacker_helpers_temp167.log`
- `tools/ghidra_projects/gw_decomp_mode_workers_temp166.log`
