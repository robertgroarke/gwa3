# GWCA GameThread Sibling Decode Families Addendum

This pass decompiles the four sibling rebuild stages called by [GWCA_GameThread_CompressedBlockOrchestration_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CompressedBlockOrchestration_Addendum.md):

- `FUN_0069DA70(...)`
- `FUN_0069CFE0(...)`
- `FUN_0069D320(...)`
- `FUN_0069D8F0(...)`

This is the first point where the internal compact-family flags stop being abstract and start looking like concrete decode classes.

## 1. `FUN_0069DA70(...)`: flag-`1` sentinel-block filler

`FUN_0069DA70(...)` is the narrowest of the sibling decoders.

It:

- consumes the same variable-length run/state stream format as the other compact-family decoders
- walks only blocks not already claimed in the primary mask
- and, when the current compact-state says “emit,” writes:
  - `0xFFFFFFFE`
  - `0xFFFFFFFF`

into the destination block, then marks the block as claimed in both:

- the primary mask
- the secondary mask

That makes flag `1` very different from the richer color-family decoders. It is not reconstructing a full endpoint-derived block template. It is scattering a fixed sentinel block into selected positions.

Best current interpretation:

- flag `1` is a special “constant block” compact family
- likely representing an extreme or degenerate compressed-block case rather than a normal color interpolation family

## 2. `FUN_0069CFE0(...)`: flag-`2` nibble-seeded compact block family

`FUN_0069CFE0(...)` is structurally close to `FUN_0069D320(...)`, but its seed extraction is smaller:

- it pulls a 4-bit seed from the stream front
- expands that nibble into a repeated byte / word pattern
- then uses the same variable-length run/state reader to decide where emitted blocks are applied

The reconstructed block values come from a tiny local palette table:

- `local_38 + local_1c * 2`

where only one entry is nonzero and the others are zeroed. That means this path is still a compact-family scatter stage, but it is driven by a very small seed representation.

The parent orchestrator already told us this family is format-gated to:

- `0x10`
- `0x11`

So the picture is now cleaner:

- flag `2` is the compact-family path specific to the `0x10 / 0x11` format pair
- and its compact seed is only 4 bits wide before expansion

## 3. `FUN_0069D320(...)`: flag-`4` byte-seeded compact block family

`FUN_0069D320(...)` is the sibling for the upper format group.

It has almost the same control structure as `FUN_0069CFE0(...)`, but the seed extraction is wider:

- it pulls an 8-bit seed from the stream front
- expands it by byte replication
- then uses the same run/state decode pattern to scatter emitted blocks across unclaimed positions

Again, the emitted blocks come from a tiny local palette table with one nonzero seeded entry and zeroed siblings, which means this is still a small template-family decoder rather than a full endpoint-interpolation decode.

The parent orchestrator already gated this family to:

- `0x12`
- `0x13`
- `0x14`
- `0x15`

So the most useful difference between flag `2` and flag `4` is now concrete:

- flag `2` = 4-bit seeded compact family for `0x10 / 0x11`
- flag `4` = 8-bit seeded compact family for `0x12..0x15`

That is stronger and more useful than the earlier “format family class” wording.

## 4. `FUN_0069D660(...)`: flag-`8` representative-block family

For comparison with the new sibling bodies:

- `FUN_0069D660(...)` is much richer than `FUN_0069CFE0(...)` or `FUN_0069D320(...)`
- it calls `FUN_006A66C0(...)`
- and scatters a synthesized representative compressed block, not just a tiny seeded local palette entry

So the compact-family hierarchy now looks like:

- flag `1`: fixed sentinel block
- flag `2`: 4-bit seeded compact block family
- flag `4`: 8-bit seeded compact block family
- flag `8`: representative-block family synthesized through BC1-like helper logic

That is a meaningful structural split.

## 5. `FUN_0069D8F0(...)`: block swizzle / quadrant permutation post-pass

`FUN_0069D8F0(...)` is not another compact-family decoder. It is a format-specific block-layout post-pass.

The parent orchestrator only calls it when all of these are true:

- chunk flag `0x10` is present
- width is `0x100`
- height is `0x100`
- format is `0x10` or `0x11`

The body walks 16-byte blocks and conditionally:

- flips low-order spatial index bits
- relocates a source block from one `(major, minor)` position to another
- and permutes the packed block words differently depending on two bit tests

The transformations include:

- nibble/byte reordering within the first block word
- byte-lane swapping in the fourth word
- 16-bit half swapping in the middle words

That is best described as:

- a tiled or quadrant-aware compressed-block swizzle
- applied only to a very specific `256 x 256` format pair

So `FUN_0069D8F0(...)` is not a codec family in the same sense as the others. It is a special post-decode layout correction for one format pair.

## 6. Updated family map

With this pass, the sibling decode family map is much stronger:

- flag `1`
  - decoded by `FUN_0069DA70(...)`
  - emits a fixed sentinel block
  - marks both masks

- flag `2`
  - decoded by `FUN_0069CFE0(...)`
  - uses a 4-bit seed expanded into a tiny local block template
  - only used by formats `0x10 / 0x11`

- flag `4`
  - decoded by `FUN_0069D320(...)`
  - uses an 8-bit seed expanded into a tiny local block template
  - only used by formats `0x12..0x15`

- flag `8`
  - decoded by `FUN_0069D660(...)`
  - uses `FUN_006A66C0(...)` to synthesize a representative compressed block
  - spans a broader format capability family

- chunk flag `0x10`
  - not a compact-family decoder bit in the same sense
  - instead triggers the special `FUN_0069D8F0(...)` block-swizzle pass for `256 x 256` format `0x10 / 0x11`

This makes the internal payload design look much more intentional:

- several compact block templates for different seed widths / complexity levels
- one fixed sentinel shortcut
- one richer representative-block family
- and one format-specific post-layout correction path

## 7. What remains unresolved

Even with the sibling bodies in hand, a few important things are still open:

- what exact external names correspond to format ids `0x0F`, `0x10`, `0x11`, `0x12`, `0x13`, `0x14`, and `0x15`
- whether flag `1`’s sentinel block corresponds to a known degenerate BC-family block meaning
- whether the `0x10 / 0x11` versus `0x12..0x15` split maps onto BC1/BC2/BC3 style families, or onto an engine-specific grouping layered on top of them

But the gap is much smaller now. We are no longer trying to infer behavior from context alone. The decode roles are visible in the binary.

## 8. Best next step

The strongest next step is now outside these helpers and back toward naming the format ids themselves.

Best targets:

- more callers and neighbors around `FUN_0069E1C0(...)`
- higher-level DDS import / export mapping in:
  - `FUN_006A0330(...)`
  - `FUN_006A0630(...)`
- and any static format-id tables feeding `FUN_00689E90(...)`

That should let the research answer the last important question in this seam:

- which exact external compressed formats the internal `0x0F..0x15` families correspond to
