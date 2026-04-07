# GWCA GameThread Transfer Bank Crosswalk

This note consolidates the current state of the transfer-bank branch into one crosswalk.

It is meant to summarize the staged model recovered across the recent addenda:

1. compact source family
2. intermediate 16-entry working representation
3. neighborhood-conditioned worker transform
4. compact re-emission
5. final `0x18` output/materialization

The table below is intentionally conservative. It records what is currently binary-supported and keeps the naming at the strongest level the evidence supports.

## Stage Model

The best current architecture is:

- most early symmetric/asymmetric transfer-bank workers use `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`
- a nearby repacker-only adapter band joins from the `FUN_006A5CB0(...)` side without using `FUN_006A5F10(...)`
- those workers share a reusable intermediate 16-pixel block representation
- the shared seam splits into three `block_kind` families
- later `0x18` workers materialize final destination pixels directly

So the transfer bank now reads as:

- **family**
- **mode**
- **worker variant**
- **stage plane**

rather than a flat registry of unrelated callbacks.

## Crosswalk Table

| Worker | Source-Side Shape | `block_kind` | Mode | Companion/Layout Family | Worker Subtype | Stage Relation |
| --- | --- | --- | --- | --- | --- | --- |
| `FUN_00679D70` | compact color-table record | `1` | `0` | opaque-style lower color-table family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067BDD0` | compact color-table record | `1` | `0` | opaque-style lower color-table family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067A6B0` | compact record | `2` | `0` | nibble scalar/intensity companion family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067A210` | compact record | `2` | `1` | nibble scalar/intensity companion family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067C220` | compact record | `2` | `1` | nibble scalar/intensity companion family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067C670` | compact record | `2` | `0` | nibble scalar/intensity companion family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067AB50` | compact record | `3` | `1` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067CAC0` | compact record | `3` | `1` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067AFF0` | compact record | `3` | `0` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067CF10` | compact record | `3` | `0` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067B930` | compact record | `3` | `2` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067D7B0` | compact record | `3` | `2` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067B490` | compact record | `3` | `3` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067D360` | compact record | `3` | `3` | endpoint-plus-selector scalar/color family | neighborhood edit worker | intermediate edit/repack |
| `FUN_0067DC00` | paired family-`2`-like compact records merged into a local working block | `2` | `0` | nibble scalar/intensity companion family | source bridge worker | intermediate repack adapter |
| `FUN_0067E2F0` | paired family-`2`-like compact records merged into a local working block | `2` | `0` | nibble scalar/intensity companion family | source bridge worker | intermediate repack adapter |
| `FUN_0067E9C0` | richer compact feeder bridged into opaque family `1` | `1` | `0` | opaque-style lower color-table family | source bridge worker | intermediate repack adapter |
| `FUN_0067F090` | richer compact feeder bridged into opaque family `1` | `1` | `0` | opaque-style lower color-table family | source bridge worker | intermediate repack adapter |
| `FUN_0067F7D0` | byte-packed alpha/color feeder bridged into opaque family `1` | `1` | `0` | opaque-style lower color-table family | source bridge worker | intermediate repack adapter |
| `FUN_00680140` | 16-bit source texels bridged directly into repacker family `3` | `3` | `0` | endpoint-plus-selector scalar/color family | source bridge worker | intermediate repack adapter |
| `FUN_00684100` | single compact feeder normalized into opaque family `1` | `1` | `0` | opaque-style lower color-table family | source bridge worker | intermediate repack adapter |
| `FUN_006848D0` | byte-packed scalar-plus-color feeder bridged into lower family `0` | `0` | `1` | threshold-`0` lower color-table family | source bridge worker | intermediate repack adapter |
| `FUN_006803F0` | 16-bit source texels bridged into family `3` through the full front door | `3` | `0` | endpoint-plus-selector scalar/color family | source bridge worker | intermediate edit/repack |
| `FUN_0068C310` | family-`3` block grid with coefficient transform | `3` | `2` | endpoint-plus-selector scalar/color family | transform bridge worker | intermediate edit/repack |
| `FUN_00681860` | upper DXT-family compact payload | n/a | n/a | direct scalar-plus-color output | final materializer | final `0x18` materialization |
| `FUN_00681ED0` | upper DXT-family compact payload | n/a | n/a | direct scalar-plus-color output | final materializer | final `0x18` materialization |
| `FUN_006825A0` | upper scalar payload | n/a | n/a | alpha-only white-RGB output | final materializer | final `0x18` materialization |
| `FUN_00682980` | upper scalar-plus-color payload | n/a | n/a | scalar-weighted color output | final materializer | final `0x18` materialization |
| `FUN_006833F0` | color-only block payload | n/a | n/a | color-only 24-bit output | final materializer | final `0x18` materialization |

## Family Notes

### `block_kind 1`

Best current name:

- opaque-style lower color-table family

Current interpretation:

- shared lower color-table emitter with threshold `0x80`
- BC1-like color-table working representation
- only mode `0` observed so far

### `block_kind 0`

Best current name:

- threshold-`0` lower color-table family

Current interpretation:

- shared lower color-table emitter with threshold `0x00`
- likely mask-selected / transparency-capable counterpart to `block_kind 1`
- currently observed mode:
  - `1`

### `block_kind 2`

Best current name:

- nibble scalar/intensity companion family

Current interpretation:

- color-table family plus 4-bit scalar/intensity overlay
- unpack/repack helpers:
  - `FUN_006A5050(...)`
  - `FUN_006A53D0(...)`
- observed modes:
  - `0`
  - `1`

### `block_kind 3`

Best current name:

- endpoint-plus-selector scalar/color family

Current interpretation:

- richest intermediate family
- unpack/repack helpers:
  - `FUN_006A5160(...)`
  - `FUN_006A5560(...)`
- observed modes:
  - `0`
  - `1`
  - `2`
  - `3`

## Shared Front-Door Semantics

The current best reading of the shared seam is:

- `FUN_006A5F10(...)`
  - expand a compact block family into a 16-entry working block
- worker body
  - blend or transform against local neighborhood/source state
- `FUN_006A5CB0(...)`
  - normalize the working block
  - choose the correct compact companion layout
  - repack it into output words

This is the key reason the cluster no longer looks like a pile of one-off compressed-format functions. The repeated `family + mode + worker variant` pattern is real.

## Worker Subtypes

The current caller set is now strong enough to separate four practical worker subtypes:

### Neighborhood Edit Worker

These:

- expand a compact block family into a 16-entry working block
- blend it against local neighborhood pixels
- repack into the same family

Examples:

- `FUN_00679D70`
- `FUN_0067A210`
- `FUN_0067AB50`
- `FUN_0067C220`
- `FUN_0067CAC0`
- `FUN_0067D360`

### Source Bridge Worker

These:

- do not begin from the usual compact record shape
- derive a working block from another source encoding
- then repack into one of the standard intermediate families

Current examples:

- `FUN_0067DC00`
- `FUN_0067E2F0`
- `FUN_0067E9C0`
- `FUN_0067F090`
- `FUN_0067F7D0`
- `FUN_00680140`
- `FUN_006803F0`

Current useful split:

- full front-door bridge:
  - `FUN_006803F0`
- repacker-only adapters:
  - `FUN_0067DC00`
  - `FUN_0067E2F0`
  - `FUN_0067E9C0`
  - `FUN_0067F090`
  - `FUN_0067F7D0`
  - `FUN_00680140`
  - `FUN_00684100`
  - `FUN_006848D0`

### Transform Bridge Worker

These:

- expand a standard intermediate family
- apply a non-neighborhood transform over all 16 working pixels
- repack back into the same family

Current example:

- `FUN_0068C310`

### Final Materializer

These:

- do not route through the shared front-door repack seam
- directly materialize final destination pixels
- belong to the `0x18` output plane

Examples:

- `FUN_00681860`
- `FUN_00681ED0`
- `FUN_006825A0`
- `FUN_00682980`
- `FUN_006833F0`

## Output-Tier Crosswalk

The direct `0x18` output tier currently maps as:

- `0x12 / DXT4` -> `FUN_00681860`
- `0x13 / DXT5` -> `FUN_00681ED0`
- `0x14 / DXTA` -> `FUN_006825A0`
- `0x15 / DXTL` -> `FUN_00682980`
- `0x16 / DXTN` -> `FUN_006833F0`

Best current interpretation:

- `DXT4` / `DXT5`
  - direct 32-bit scalar-plus-color output workers
- `DXTA`
  - alpha-only white-RGB output worker
- `DXTL`
  - scalar-weighted color output worker
- `DXTN`
  - color-only 24-bit output worker

So the strongest stage boundary is:

- front-door cluster = intermediate edit/repack plane
- `0x18` tier = final output/materialization plane

## What Is Still Missing

The current remaining gaps are narrower now:

- exact semantic names for intermediate modes `0/1/2/3`
- how broad the lower family-`0` caller set is beyond `FUN_006848D0`
- a tighter statement of how the intermediate families map back to specific upper DXT-derived source families in every stage, not just sampled branches

## Best Next Step

The strongest next step is to keep synthesizing rather than broadening randomly:

1. add one more appendix mapping:
   - `block_kind`
   - helper pair
   - observed modes
   - known caller set
   - observed worker subtype
2. then target any remaining upper-region outliers beyond `FUN_0068C310`

That should show whether the current three-family taxonomy fully covers the transfer bank or whether one more bridge/adapter layer still sits just outside the sampled cluster.
