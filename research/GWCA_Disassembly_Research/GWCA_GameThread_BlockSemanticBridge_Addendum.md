# GWCA GameThread Block Semantic Bridge Addendum

This pass takes the final step that was still missing from the stream work:

- connect the primary and companion streams to their block-semantic anchor helpers directly

The goal was to get past “looks like” language and tie each stream to the block decoder/builder pair that best explains it.

The result is the clearest semantic split so far:

- the primary stream is anchored to the repeated-selector / scalar decoder pair
- the companion stream is anchored to the BC1-style color-block decoder/builder pair

That still stops short of claiming the engine’s own final type name, but it is now well beyond a loose analogy.

## 1. Companion stream anchor: `FUN_0069DD70(...)`

The strongest decoder-side anchor is `FUN_0069DD70(...)`.

Its body:

- takes a two-word block pair
- interprets the first word as two RGB565-like endpoints
- expands those endpoints through the 5/6/5 lookup tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`
- derives the intermediate colors according to endpoint ordering
- then returns one of four colors based on:
  - `param_1[1] & 3`

That is exactly the shape of a BC1 / DXT1-style color-block decoder:

- two color endpoints
- derived 4-color palette
- 2-bit selector

So whenever a helper’s logic revolves around:

- `FUN_0069DD70(...)`

we are on very strong ground calling that stream the BC1-like color-block side.

## 2. Companion stream anchor: `FUN_006A66C0(...)`

The strongest builder-side anchor is `FUN_006A66C0(...)`.

Its body:

- quantizes an input ARGB-like color into 5/6/5 endpoint candidates
- constructs two endpoint words
- computes a repeated selector pattern
- writes:
  - `param_1[0] = endpoint pair`
  - `param_1[1] = repeated selector dword`

That is the natural inverse partner to `FUN_0069DD70(...)`.

So:

- `FUN_0069DD70(...)` decodes a BC1-like block pair
- `FUN_006A66C0(...)` synthesizes a representative BC1-like block pair

This is exactly the pair used around the companion stream families:

- `FUN_0069D660(...)`
- `FUN_0069C3F0(...)`

That is the strongest direct bridge in the whole seam.

## 3. Why this locks the companion stream down

We already established in the previous note that:

- `FUN_0069DA70(...)`
- `FUN_0069D660(...)`

write to the companion stream at:

- `local_c + local_24 + local_28`

Now the semantic bridge is:

### `FUN_0069DA70(...)`

- writes a fixed sentinel pair
- that sentinel pair has the same two-word shape as the BC1-like block pair

### `FUN_0069D660(...)`

- uses `FUN_006A66C0(...)`
- then writes the resulting two-word block pair into the companion stream

### `FUN_0069C3F0(...)`

- filters candidate source blocks
- decodes them through `FUN_0069DD70(...)`
- chooses a dominant decoded color
- stabilizes a representative block through `FUN_006A66C0(...)`
- and encodes that family into the stage-2 compact payload

So the companion stream is no longer just “BC1-like” by resemblance.
It is directly tied to the engine’s own BC1-style decoder/builder pair.

That makes the best current description:

- companion stream = BC1-style color-block stream

## 4. Primary stream anchor: `FUN_0069DCE0(...)`

The primary-stream side has a similarly strong anchor.

`FUN_0069DCE0(...)`:

- reads two endpoint bytes plus a 3-bit selector
- applies 7-step interpolation or 5-step-plus-special-case interpolation
- returns a single 8-bit scalar

That is the classic DXT5-style alpha interpolation shape.

Then `FUN_0069EE40(...)` verifies that the associated selector word is one of the legal repeated 3-bit families:

- `0x00000000`
- `0x24924924`
- `0x49249249`
- `0x6DB6DB6D`
- `0x92492492`
- `0xB6DB6DB6`
- `0xDB6DB6DB`
- `0xFFFFFFFF`

So the primary stream is not just “some scalar pattern stream.”
It is anchored to:

- repeated 3-bit selector families
- decoded through a DXT5-style scalar/alpha interpolation helper

That is a much stronger bridge than simple shape-based speculation.

## 5. Why this locks the primary stream down

The primary-stream writers:

- `FUN_0069CFE0(...)`
- `FUN_0069D320(...)`

already looked scalar/selector-like because they:

- start from 4-bit or 8-bit seeds
- expand them into repeated patterns
- and write those repeated two-word records into the primary stream

Now the semantic bridge is:

- the stage-1 prepass feeding that family is explicitly repeated-selector scalar/alpha logic
- the decoder behind that prepass is explicitly `FUN_0069DCE0(...)`

So the best current description of the primary stream is:

- repeated-selector scalar/alpha-style stream

That is more precise than the previous wording “scalar/selector-style.”

## 6. Cleanest current stream identities

At this point the stream identities are best stated as:

### Primary stream

- repeated-selector scalar/alpha-style block stream
- anchored by:
  - `FUN_0069DCE0(...)`
  - `FUN_0069EE40(...)`
  - `FUN_0069CFE0(...)`
  - `FUN_0069D320(...)`

### Companion stream

- BC1-style color-block stream
- anchored by:
  - `FUN_0069DD70(...)`
  - `FUN_006A66C0(...)`
  - `FUN_0069DA70(...)`
  - `FUN_0069D660(...)`
  - `FUN_0069C3F0(...)`

That is the strongest internal taxonomy we have recovered so far.

## 7. What this means for `DXTA`

This now gives `DXTA` a genuinely meaningful interpretation:

- `DXTA` keeps the repeated-selector scalar/alpha-style stream
- `DXTA` does not carry the companion BC1-style color-block stream gated by `0x210`

That is much stronger than the earlier “partial member” description.

It means `DXTA` is best read as:

- the upper-family format variant with the scalar/alpha block side only

while:

- `DXT4`, `DXT5`, and `DXTL`

carry both:

- the scalar/alpha block side
- and the BC1-style color-block side

## 8. Strongest current conclusion

The strongest current conclusion is:

- the `0x210` trait gates the presence of a BC1-style companion color-block stream
- the always-present side is the repeated-selector scalar/alpha stream
- `DXTA` is the upper-family variant that lacks the companion BC1-style color-block stream

That is now supported by:

1. stage/helper selection
2. decode-side reconstruction layout
3. builder-side serialized layout
4. direct block decoder/builder semantics

## 9. Best next step

The strongest next move is to find the first downstream consumer that treats the reconstructed pair as:

- scalar/alpha block stream
- plus BC1-style color-block stream

The best concrete targets now are:

- consumers above `FUN_0069E1C0(...)` that operate on the reconstructed per-block layout
- or any helper that reads both:
  - `local_c`
  - and `local_c + local_24 + local_28`

That should let us move from:

- “BC1-style companion stream”

to:

- the exact engine role the pair plays in the final image/block pipeline.
