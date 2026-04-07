# GWCA GameThread Stage-18 Per-Format Crosswalk Addendum

This pass fills in the remaining `0x18`-tier workers and turns the new output plane into a usable per-format crosswalk.

The static installs for this tier are:

- `(0x18, 0x12, 0) -> FUN_00681860`
- `(0x18, 0x13, 0) -> FUN_00681ED0`
- `(0x18, 0x14, 0) -> FUN_006825A0`
- `(0x18, 0x15, 0) -> FUN_00682980`
- `(0x18, 0x16, 0) -> FUN_006833F0`

Using the earlier DDS/format crosswalk, those map to:

- `0x12 = DXT4`
- `0x13 = DXT5`
- `0x14 = DXTA`
- `0x15 = DXTL`
- `0x16 = DXTN`

## Main Result

The strongest upgrade from this pass is that selector `0x18` is not just a generic late-stage worker bank. It is a **format-specific direct output tier**, and the family split now lines up with the higher-level format semantics we already recovered elsewhere:

- alpha-plus-color families
- alpha-only family
- premultiplied/intensity-tinted family
- color-only family

So the `0x18` tier looks like the point where the engine stops talking in terms of intermediate block streams and starts emitting final destination pixel payloads in format-shaped ways.

## `FUN_00681860` and `FUN_00681ED0`

These two remain the clearest full emitters in the family.

Both functions:

- build 8-entry scalar/alpha ladders from the first two bytes,
- expand RGB565 endpoints through `DAT_00A2C6C8` and `DAT_00A2C948`,
- derive intermediate color entries,
- consume packed selector fields,
- combine scalar and color contributions,
- and write packed 32-bit destination pixels directly.

Their common final-store pattern is the strongest anchor:

- `uVar >> 0x10 & 0xff | (uVar & 0xff) << 0x10 | uVar & 0xff00ff00`

So `(0x18, 0x12, 0)` and `(0x18, 0x13, 0)` are clearly direct block-to-32-bit-output workers for the upper two-stream DXT families.

## `FUN_006825A0` = Alpha-Only Output Family

`FUN_006825A0(...)` is meaningfully different.

It:

- builds the same 8-entry scalar ladder from the first two bytes,
- consumes only the packed scalar-selector fields,
- does **not** decode or combine any RGB565 color endpoints,
- and writes values of the form:
  - `local_44[...] | 0xFFFFFF`

That is a very strong sign that this worker is materializing an **alpha-only plane over constant white RGB**.

So `(0x18, 0x14, 0)` fits the earlier `DXTA` reading very well:

- scalar/alpha-family member
- without the companion BC1-style color side
- and with a direct output worker that reflects exactly that split

This is probably the cleanest callback-side confirmation yet that `DXTA` really is the alpha-only upper-family variant.

## `FUN_00682980` = Premultiplied/Intensity-Tinted Color Output

`FUN_00682980(...)` sits between the full two-stream workers and the alpha-only worker.

It still:

- builds the scalar ladder,
- expands RGB565 endpoints,
- combines color and scalar selectors,

but the final output path multiplies RGB components by the scalar value before writing them and forces the top byte to `0xFF`.

The final stores look like:

- scaled blue/green/red by `alpha-like scalar`
- then `| 0xFF000000`

So this worker is not just a copy of `FUN_00681860(...)`. It is a **color materializer with scalar-weighted RGB output**, which fits the earlier interpretation that `DXTL` is an upper-family variant with different companion/output semantics from ordinary `DXT5`.

The safest wording is:

- `(0x18, 0x15, 0)` is a scalar-weighted color output worker
- likely a luminance/intensity-tinted member of the upper family

## `FUN_006833F0` = Color-Only 24-Bit Output Family

`FUN_006833F0(...)` is the cleanest outlier in the whole tier.

It:

- takes `ushort*` source blocks,
- expands RGB565 endpoints,
- builds only 4 color entries,
- uses 2-bit selector fields,
- and writes **three bytes per pixel**:
  - two bytes through `*puVar3 = (short)uVar5`
  - one extra byte through `*(char *)(puVar3 + 1) = (char)(uVar5 >> 0x10)`

So this is plainly a **color-only output family**, not an alpha-bearing one.

That makes `(0x18, 0x16, 0)` fit the earlier `DXTN` placement well:

- distinct from the upper scalar/alpha family
- direct color materialization path
- no companion alpha-style ladder combination beyond ordinary color selection

## Best Current Crosswalk

The strongest current per-format reading of the `0x18` tier is:

- `DXT4 / 0x12` -> direct 32-bit scalar-plus-color output worker
- `DXT5 / 0x13` -> sibling direct 32-bit scalar-plus-color output worker
- `DXTA / 0x14` -> alpha-only white-RGB output worker
- `DXTL / 0x15` -> scalar-weighted color output worker
- `DXTN / 0x16` -> color-only 24-bit output worker

That is a much better result than just “five more handlers,” because it makes the `0x18` registry plane look like a final-format materialization layer rather than a generic transfer callback bank.

## Architectural Meaning

The transfer registry now looks like a true multi-plane system:

1. earlier planes reconstruct, compact, or support intermediate block state
2. asymmetric planes expose heavier worker variants
3. the `0x18` plane emits final format-shaped pixel output

So the third selector dimension is starting to read like:

- not just “variant”
- but “which stage/tool family should interpret this stored block layout”

## Next Step

The next best step is to compare the `0x18` family directly against the earlier one-sided heavy workers:

- `FUN_0067AB50`
- `FUN_0067AFF0`
- `FUN_0067B490`
- `FUN_0067B930`

That should tell us whether those earlier workers feed this output plane, duplicate it for another destination layout, or represent a different stage entirely.

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_stage18_tier_temp165.log`
- `tools/ghidra_projects/gw_dump_a26c74_temp163.log`
