# GWCA GameThread DXTN Branch Mechanics Addendum

This pass goes back into the `DXTN` branch itself after the broader capability-boundary work.

The goal was to answer a more concrete question:

- what kind of compact-family branch is `DXTN` actually taking inside the DXT-style compressed block system?

The key functions for this pass are the two already identified as the `DXTN`-side branch pair:

- `FUN_0069CC40(...)`
- `FUN_0069C3F0(...)`

and the newly sampled neighboring consumer:

- `FUN_0069F7F0(...)`

The result is that `DXTN` now looks less like “the leftover format” and more like a deliberately mixed branch:

- a narrow trivial-block prepass
- plus a dominant-color compact family built over BC1-style color decoding

## 1. `FUN_0069F7F0(...)`: another lower-bit consumer, not a DXTN-special branch

One of the reasons for sampling `FUN_0069F7F0(...)` was to see whether it did anything special for the `0x200` bit.

It does not.

This helper is a BMP parser / internal-format mapper:

- validates `"BM"`
- validates header and payload lengths
- maps BMP bit depth and masks to an internal format id
- then checks:
  - `FUN_00689E90(format) & 8`

That reinforces the boundary from the previous pass:

- lower capability bits are broad image-pipeline traits
- `DXTN`'s `0x200` distinction still belongs to the compressed-family seam, not general import logic

That is useful context, but the real new findings come from the branch pair below.

## 2. `FUN_0069CC40(...)`: DXTN-side trivial-block prepass

We already knew from the earlier block-family work that `FUN_0069CC40(...)` was a narrow trivial-block encoder.

What matters here is how it fits the `DXTN` branch.

The decompile shows:

- it scans blocks not already claimed in the active mask
- it only accepts blocks where:
  - `*(int *)(block + 2) == -1`
  - `block[0] <= block[1]`
- it estimates whether compact encoding beats the raw threshold
- and if so:
  - sets `*param_1 |= 1`
  - marks accepted blocks in both masks
  - emits a run/state-coded stream through the bit writer

So `FUN_0069CC40(...)` is not the main body of the `DXTN` branch. It is the branch’s early, narrow, cheap prepass:

- detect a very restricted trivial block family
- claim those blocks first
- and leave the rest for a richer family afterward

That makes the `DXTN` branch structurally similar to a layered codec:

1. peel off the easiest special-case blocks
2. then run a stronger dominant-family encoder over what remains

## 3. `FUN_0069C3F0(...)`: dominant-color compact family over BC1-style decoded colors

This is the pass’s strongest new result.

From the larger decompile body, `FUN_0069C3F0(...)` does all of the following:

1. filters blocks by a narrow selector-pattern family:
   - `0x00000000`
   - `0x55555555`
   - `0xAAAAAAAA`
   - `0xFFFFFFFF`
2. decodes each qualifying block through:
   - `FUN_0069DD70(...)`
3. collects those decoded colors into a temporary array
4. radix-sorts / bins them
5. finds the dominant repeated decoded color
6. if the compact form wins:
   - sets `*param_1 |= 8`
   - stabilizes a representative compact block through:
     - `FUN_006A66C0(...)`
     - followed by `FUN_0069DD70(...)`
   - writes the resulting 24-bit representative color seed
   - and emits the run/state stream for the matching blocks

That is much stronger than the earlier “dominant-byte family” wording.

This function is really doing:

- BC1-style color decoding on candidate blocks
- dominant-color selection in decoded color space
- representative compact-block synthesis from that dominant color
- then run/state emission for matching blocks

So inside the `DXTN` branch, `FUN_0069C3F0(...)` is the main semantic codec family, not just a small adjunct helper.

## 4. Why this matters for DXTN specifically

From [GWCA_GameThread_DXTNPlacement_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTNPlacement_Addendum.md), `DXTN` was already placed on:

- `FUN_0069CC40(...)`
- `FUN_0069C3F0(...)`

This pass gives that branch real internal shape:

### Stage 1: trivial-block peel

- `FUN_0069CC40(...)`
- narrow predicate
- run-length-like compact coding
- flag `1`

### Stage 2: dominant-color compact family

- `FUN_0069C3F0(...)`
- selector-pattern-filtered
- decode through `FUN_0069DD70(...)`
- choose dominant decoded color
- synthesize representative compact block through `FUN_006A66C0(...)`
- run/state encode remaining matching blocks
- flag `8`

That means `DXTN` is not merely “another upper DXT family.”

It is the format whose compact path most clearly looks like:

- trivial pattern peel
- then dominant BC1-like color-family compression

## 5. Comparison to the other format groups

This pass sharpens the contrasts with the already-mapped groups.

### `DXT2 / DXT3`

These formats take the:

- `FUN_0069B720(...)`

branch and the special post-swizzle at one size. That family looks much more tied to very low-entropy nibble/block-state patterns.

### `DXT4 / DXT5 / DXTA / DXTL`

These formats take:

- `FUN_0069BCD0(...)`
- `FUN_0069C3F0(...)`

So they share the richer dominant-color family with `DXTN`, but they also have the selector-family branch `FUN_0069BCD0(...)`, not the trivial-block prepass `FUN_0069CC40(...)`.

### `DXTN`

`DXTN` takes:

- `FUN_0069CC40(...)`
- `FUN_0069C3F0(...)`

So compared with the upper family, the distinguishing feature is:

- `DXTN` replaces the richer selector-family branch `FUN_0069BCD0(...)`
- with the narrower trivial-block prepass `FUN_0069CC40(...)`

That is the cleanest description of its branch identity so far.

## 6. Best current interpretation of DXTN

The strongest current reading is:

- `DXTN` is a compressed-family variant built around the same dominant BC1-style color family as part of the upper DXT group
- but it pairs that dominant-color family with a simpler trivial-block prepass instead of the richer selector-family prepass used by `DXT4 / DXT5 / DXTA / DXTL`

That makes `DXTN` feel much less mysterious.

It is not an outlier with an entirely alien codec.
It is a close relative in the same family tree, but with a different “stage 1” compact-family choice.

## 7. What remains unresolved

Two narrower questions still remain:

1. whether the `FUN_0069CC40(...)` trivial-block predicate corresponds to a named external compressed-block subtype
2. whether the `DXTN` label itself corresponds to a standard external convention or to an engine-local name for this branch variant

But the structural question is now in good shape.

We no longer have to say:

- “DXTN uses a distinct branch”

We can now say:

- “DXTN uses the dominant BC1-style color family plus a narrow trivial-block prepass, instead of the richer selector-family prepass used by the upper DXT group”

## 8. Best next step

The strongest next move is to characterize the *difference* between the two stage-1 families directly:

- `FUN_0069CC40(...)`
- versus
- `FUN_0069BCD0(...)`

That comparison should answer the most interesting remaining question in this seam:

- what exact block-pattern property makes a format use the trivial-block prepass versus the selector-family prepass

If that difference becomes concrete enough, it may be possible to give `DXTN` a much tighter external interpretation than we have now.
