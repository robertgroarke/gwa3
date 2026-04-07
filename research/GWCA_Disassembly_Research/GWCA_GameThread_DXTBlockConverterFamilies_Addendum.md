# GWCA GameThread DXT Block Converter Families Addendum

This pass continues the compressed-format branch underneath the DDS / `ImgMem` / `ImgPal` seam and focuses on the four specialized helpers called from `FUN_0069E870(...)`:

- `FUN_0069B720(...)`
- `FUN_0069CC40(...)`
- `FUN_0069BCD0(...)`
- `FUN_0069C3F0(...)`

The main result is that these are not generic image-processing helpers. They are a family of block-oriented special-case encoders that:

- walk masked block sets
- detect narrow pattern families
- choose a dominant representative value
- estimate whether a compact encoding is worthwhile
- and, if so, emit a packed bitstream while marking accepted blocks in mask words

That is strong binary support for the earlier interpretation that `FUN_0069E870(...)` is a real compressed-format payload builder, not merely a palette-side adapter.

## 1. `FUN_0069B720(...)`: trivial nibble-block RLE encoder

The decompile in [gw_decomp_block_converters_temp132.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_block_converters_temp132.log) shows a tight two-pass structure.

The function first iterates blocks that are still active in the incoming mask and accepts only a very narrow trivial family:

- `*block == block[1]`
- all bytes in the block collapse to one repeated byte
- the low nibble is nonzero

For accepted blocks it tallies a 16-entry histogram over that low nibble and selects the dominant nibble value.

It then estimates encoded size using the shared tables:

- `DAT_00A27B38`
- `DAT_00A27B39`

If the compact form wins, it:

- sets `*param_1 |= 2`
- writes the dominant nibble value into the output stream
- emits a compact RLE-style state stream over the block sequence
- marks consumed blocks in the mask

The block-state stream is ternary in practice: accepted dominant blocks, accepted non-dominant trivial blocks, and everything else. The code uses `param_2[2]` and `param_2[3]` as bit-budget / pending-word state for the packed output writer.

Best current interpretation:

- a special-case encoder for extremely low-entropy nibble blocks
- probably one of the alpha / selector side families inside a DXT-adjacent format path

That exact family name is still inferred, but the encoding strategy is binary-clear.

## 2. `FUN_0069CC40(...)`: sentinel-backed 16-bit trivial-block encoder

`FUN_0069CC40(...)` follows the same broad shape but over a different trivial block family.

The key acceptance checks are:

- `*(int *)(block + 2) == -1`
- `block[0] <= block[1]`

It again performs a first pass to estimate compactness, then a second pass to emit a packed stream only if worthwhile.

When accepted, it:

- sets `*param_1 |= 1`
- writes a run/state-coded compact stream into the same output bit writer
- marks accepted blocks in both incoming mask words

It reuses the same small size-estimation tables:

- `DAT_00A27B38`
- `DAT_00A27B39`

That makes the relation to `FUN_0069B720(...)` very strong:

- same high-level compression decision
- same mask-driven block filtering
- same packed output discipline
- but a different trivial-block predicate and a different flag bit

Best current interpretation:

- another narrow DXT-style special-case encoder
- likely a sibling family that operates on a 16-bit endpoint/sentinel representation instead of the nibble-only trivial case

## 3. `FUN_0069BCD0(...)`: dominant-byte encoder over patterned selector families

`FUN_0069BCD0(...)` is much richer and looks like the first truly expressive block-family encoder in this cluster.

The first half scans active blocks and recognizes a very specific pattern set in `block[1]`, including:

- `0x00000000`
- `0x24924924`
- `0x49249249`
- `0x6DB6DB6D`
- `0x92492492`
- `0xB6DB6DB6`
- `0xDB6DB6DB`
- `0xFFFFFFFF`

These are not arbitrary magic constants. They look like repeated selector/index patterns over packed low-bit-width lanes.

For matching blocks the function computes a representative byte value from interpolated combinations of bytes in `block[0]`, builds a 256-entry histogram, and selects a dominant byte.

It then uses helper logic and tables including:

- `FUN_0069EE40(...)`
- `FUN_0069DCE0(...)`
- `FUN_0069DD70(...)`
- `DAT_00A27BCC`
- `DAT_00A27BD8`

to classify / encode each block relative to that dominant representative value.

If compression wins, it:

- sets `*param_1 |= 4`
- writes the dominant byte to the stream
- emits a packed run/state encoding over the qualifying blocks
- marks consumed blocks in the mask

Best current interpretation:

- a patterned selector/index encoder over a richer block family than the earlier trivial cases
- probably operating on one of the compressed block payloads where a dominant alpha/intensity or selector-derived byte can be shared across many blocks

What matters most is not the exact external format name yet, but that the function is clearly encoding a pattern-classified block family with a shared representative byte and a compact state stream.

## 4. `FUN_0069C3F0(...)`: dominant-value encoder with 24-bit emitted seed

The decompile for `FUN_0069C3F0(...)` is longer and partially truncated, but the visible structure is still strong.

It:

- allocates a temporary array if the masked block count is nonzero
- scans active blocks through `FUN_0069DD70(...)`
- accepts only specific pattern families in a second block word:
  - `0x00000000`
  - `0x55555555`
  - `0xAAAAAAAA`
  - `0xFFFFFFFF`
- tallies a byte-frequency distribution over accepted blocks
- derives a dominant representative value
- calls `FUN_006A66C0(...)` iteratively until that representative stabilizes

If the compact form wins, it:

- sets `*param_1 |= 8`
- writes a 24-bit seed value into the output stream
- performs another compact pass over the block family

This is the strongest sign in the cluster that not all of these encoders are the same “kind” of special case. `FUN_0069C3F0(...)` is not just another tiny RLE tweak. It appears to encode a block family whose compact form revolves around a larger shared representative value, likely endpoint-like rather than single-nibble or single-byte triviality.

Best current interpretation:

- a more complex special-case block encoder for a different compressed payload family
- still part of the same `FUN_0069E870(...)` compressed-format builder
- but likely handling a different endpoint / selector arrangement than `FUN_0069B720(...)`, `FUN_0069CC40(...)`, or `FUN_0069BCD0(...)`

## 5. Cross-family conclusions

Taken together, these four helpers establish a consistent internal design:

1. `FUN_0069E870(...)` is orchestrating several specialized block encoders, not one monolithic generic compressor.
2. Each helper is responsible for a narrow pattern family.
3. Each helper decides independently whether its compact encoding beats the fallback representation.
4. Success is signaled by distinct flag bits in `*param_1`:
   - `|= 1`
   - `|= 2`
   - `|= 4`
   - `|= 8`
5. Accepted blocks are recorded back into mask words so later passes can skip work already claimed by a more specialized encoder.

That is exactly what a mature compressed-texture payload builder would be expected to do:

- identify trivially compressible block subsets
- encode those subsets with the cheapest possible representation
- and leave the harder blocks to broader fallback logic

## 6. Confidence and remaining uncertainty

What is binary-clear from this pass:

- these helpers are real block-family encoders
- they use histograms, dominant representatives, block masks, and packed bit writers
- they sit directly under the DDS / compressed-format seam

What is still inferred:

- the exact external mapping of each helper to a named DXT-family submode
- whether a given helper is best described as alpha-family, selector-family, endpoint-family, or a hybrid of those

So the confident claim is:

- this is a DXT-style compressed block encoder family

The still-open claim is:

- which exact external compressed block subtype each helper corresponds to

## 7. Best next step

The cleanest next pass is the shared-support cluster:

- `FUN_0069EE40(...)`
- `FUN_0069DCE0(...)`
- `FUN_0069DD70(...)`
- `FUN_006A66C0(...)`

Those helpers should reveal:

- the exact block-state taxonomy used by these encoders
- how block payloads are being normalized before histogramming
- and whether the representative values are endpoints, alpha seeds, selector classes, or something more specific

That is the most likely route to replacing the current family-level inference with exact compressed-format naming.
