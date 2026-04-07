# GWCA GameThread Repacker Ownership Boundary Addendum

This pass follows the four repacker helpers outward:

- `FUN_006A5050(...)`
- `FUN_006A5160(...)`
- `FUN_006A53D0(...)`
- `FUN_006A5560(...)`

The main result is not a wide caller map. It is a much cleaner ownership boundary.

## Main Result

The strongest result from this pass is that these helpers appear to be **private implementation details of `FUN_006A5CB0(...)`** rather than shared format utilities used all over the image.

The headless caller search only surfaced the direct internal reference from:

- `FUN_006A5CB0`

and the earlier decomp work already showed the full local call structure:

- `block_kind == 2`
  - `FUN_006A53D0(...)`
  - `FUN_006A5050(...)`
- `block_kind == 3`
  - `FUN_006A5560(...)`
  - `FUN_006A5160(...)`

So the companion-layout split is not a diffuse engine-wide abstraction. It is a tight, localized repacker-family decision inside the shared mode worker.

## Why That Matters

This is a useful narrowing result.

Before this pass, the next question was whether the nibble family and richer scalar family might be broad reusable block formats that reappear independently in other transfer seams.

The current evidence points the other way:

- `FUN_006A5F10(...)` and `FUN_006A5CB0(...)` are the real public seam for this intermediate block family
- the four helper functions are the internal mechanics behind that seam
- the outer transfer tiers select into `FUN_006A5F10/FUN_006A5CB0`, not directly into the helper quartet

So the cleanest staged model now is:

1. outer transfer worker picks a mode/family
2. `FUN_006A5F10(...)` expands to a 16-entry working block
3. outer worker performs neighborhood-conditioned transforms
4. `FUN_006A5CB0(...)` chooses the compact companion layout
5. helper quartet performs the exact overlay/repack details

That is a much better architecture story than “many unrelated callers happen to share some helpers.”

## Ownership Split

The ownership tree is now best described as:

### Public-ish intermediate seam

- `FUN_006A5F10(...)`
- `FUN_006A5CB0(...)`

These are what the one-sided tier and related transfer workers visibly call.

### Private `block_kind == 2` mechanics

- `FUN_006A5050(...)`
- `FUN_006A53D0(...)`

These implement the nibble-seeded scalar/intensity companion family.

### Private `block_kind == 3` mechanics

- `FUN_006A5160(...)`
- `FUN_006A5560(...)`

These implement the richer endpoint-and-selector scalar/alpha companion family.

So the internal family split is now localized and coherent:

- one repacker front door
- two compact companion layouts
- two helper pairs hidden underneath

## Best Current Interpretation

The best current reading is that the engine wants callers to think in terms of:

- working 16-pixel block state
- mode selection
- repack family

not in terms of the raw nibble or selector helpers themselves.

That supports the earlier conclusion that the one-sided tier is a genuine **intermediate block editing layer** rather than a loose collection of compressed-format functions.

## Next Step

The best next reverse step is to go back outward, not farther inward:

- trace more callers of `FUN_006A5F10(...)`
- trace more callers of `FUN_006A5CB0(...)`
- and line those caller groups up against the previously recovered transfer-bank installs

That should let us finish a cleaner crosswalk from:

- transfer-bank selector triple
- to intermediate block family
- to chosen companion layout
- to final output tier

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_repacker_helpers_temp167.log`
- `tools/ghidra_projects/gw_decomp_mode_workers_temp166.log`
