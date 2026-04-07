# GWCA GameThread Family/Mode Appendix

This appendix reorganizes the current transfer-bank results by:

- `block_kind`
- mode
- helper pair
- known caller set
- observed worker subtype

The goal is to make the branch easier to continue from the family/mode axis instead of from individual function names.

## Shared Seams

All currently recovered intermediate-family workers route through the same front door:

- expansion:
  - `FUN_006A5F10(...)`
- repack:
  - `FUN_006A5CB0(...)`

Family-specific helper pairs under that seam are:

- `block_kind 2`
  - `FUN_006A5050(...)`
  - `FUN_006A53D0(...)`
- `block_kind 3`
  - `FUN_006A5160(...)`
  - `FUN_006A5560(...)`

`block_kind 1` currently has no extra companion-helper pair exposed below the front door in the same way.

## `block_kind 1`

Best current name:

- plain color-table intermediate family

Observed modes:

- `0`

Observed worker set:

- `FUN_00679D70`
- `FUN_0067BDD0`

Observed worker subtype:

- neighborhood edit worker

Current interpretation:

- BC1/DXT1-like color-table working representation
- simplest recovered family
- no broader mode spread seen yet

## `block_kind 2`

Best current name:

- nibble-seeded scalar/intensity companion family

Helper pair:

- unpack/overlay:
  - `FUN_006A5050(...)`
- repack:
  - `FUN_006A53D0(...)`

### Mode `0`

Observed worker set:

- `FUN_0067A6B0`
- `FUN_0067C670`

Observed worker subtype:

- neighborhood edit worker

Current interpretation:

- family-`2` baseline mode
- supports multiple neighborhood-edit siblings
- no bridge-style user observed yet

### Mode `1`

Observed worker set:

- `FUN_0067A210`
- `FUN_0067C220`

Observed worker subtype:

- neighborhood edit worker

Current interpretation:

- second live variant of the nibble companion family
- same family and helper pair, different worker bodies layered on top

## `block_kind 3`

Best current name:

- endpoint-plus-selector scalar/color family

Helper pair:

- unpack/overlay:
  - `FUN_006A5160(...)`
- repack:
  - `FUN_006A5560(...)`

### Mode `0`

Observed worker set:

- `FUN_0067AFF0`
- `FUN_0067CF10`
- `FUN_006803F0`

Observed worker subtype:

- neighborhood edit worker:
  - `FUN_0067AFF0`
  - `FUN_0067CF10`
- source bridge worker:
  - `FUN_006803F0`

Current interpretation:

- shared mode across both ordinary compact-record workers and a 16-bit source-side bridge

### Mode `1`

Observed worker set:

- `FUN_0067AB50`
- `FUN_0067CAC0`

Observed worker subtype:

- neighborhood edit worker

Current interpretation:

- stable second variant in the richer family

### Mode `2`

Observed worker set:

- `FUN_0067B930`
- `FUN_0067D7B0`
- `FUN_0068C310`

Observed worker subtype:

- neighborhood edit worker:
  - `FUN_0067B930`
  - `FUN_0067D7B0`
- transform bridge worker:
  - `FUN_0068C310`

Current interpretation:

- richest bridge-bearing mode so far
- supports both ordinary neighborhood workers and a coefficient-driven transform bridge

### Mode `3`

Observed worker set:

- `FUN_0067B490`
- `FUN_0067D360`

Observed worker subtype:

- neighborhood edit worker

Current interpretation:

- upper family variant with a narrower currently observed subtype spread than mode `2`

## Output Plane

These do not use the family front door and belong to the final materialization stage:

- `FUN_00681860`
- `FUN_00681ED0`
- `FUN_006825A0`
- `FUN_00682980`
- `FUN_006833F0`

Current best reading:

- `DXT4 / 0x12` -> direct 32-bit scalar-plus-color output
- `DXT5 / 0x13` -> direct 32-bit scalar-plus-color output
- `DXTA / 0x14` -> alpha-only white-RGB output
- `DXTL / 0x15` -> scalar-weighted color output
- `DXTN / 0x16` -> color-only 24-bit output

## Practical Takeaways

The current family/mode map suggests a reusable workflow for future reversing:

1. identify whether a worker calls `FUN_006A5F10(...)` / `FUN_006A5CB0(...)`
2. if yes, classify it first by:
   - `block_kind`
   - mode
3. then classify worker subtype:
   - neighborhood edit
   - source bridge
   - transform bridge
4. only after that worry about exact format-family naming

That ordering should keep future passes from mixing stage identity with format identity.

## Best Next Step

The strongest next synthesis step is to add one more tiny appendix mapping:

- worker subtype
- representative functions
- source-side input shape
- whether the worker preserves family or bridges into one

If you want the next decompilation step instead, the highest-yield target is any remaining upper-region outlier caller of `FUN_006A5F10(...)` / `FUN_006A5CB0(...)` beyond `FUN_0068C310(...)`.
