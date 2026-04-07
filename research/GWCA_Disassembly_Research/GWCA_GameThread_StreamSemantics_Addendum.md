# GWCA GameThread Stream Semantics Addendum

This pass takes the structural result from the last note:

- primary two-word block stream
- companion two-word block stream

and pushes it one step further into semantics.

The key question was:

- what do those two streams most likely *mean*?

The strongest current answer is:

- the primary stream is the scalar/selector-style stream
- the companion stream is the BC1-like color-block stream

That is still an interpretation rather than a formally named engine type, but it is now strongly grounded in the helper bodies.

## 1. Primary-stream writers look like scalar/selector families

The primary-stream writers are:

- `FUN_0069CFE0(...)`
- `FUN_0069D320(...)`

Both are called with:

- `param_1 = local_c`

and both emit two-word records built from tiny replicated scalar seeds:

### `FUN_0069CFE0(...)`

- starts by extracting a 4-bit seed from the bitstream:
  - `local_38[4] = param_4[3] >> 0x1c`
- then replicates it:
  - `nibble -> byte -> word -> dword`
- so the core payload becomes a repeated scalar-expanded pattern

### `FUN_0069D320(...)`

- starts by extracting an 8-bit seed:
  - `local_38[4] = param_4[3] >> 0x18`
- then expands it into a repeated 16-bit pattern
- the rest of the helper again emits from a small scalar-derived lookup table

That is not how a color endpoint block constructor usually looks.
It *is* how a selector/intensity/alpha-family stream looks:

- compact scalar seed
- replicated/expanded pattern
- tiny class table
- repeated output pair

That lines up very well with the earlier result that:

- `FUN_0069BCD0(...)` is a repeated-selector alpha/intensity prepass
- and `FUN_0069DCE0(...)` is a DXT5-style alpha interpolation decoder

So the primary stream now reads most naturally as:

- the scalar/selector-style stream

## 2. Companion-stream writers look like BC1-style color-block families

The companion-stream writers are:

- `FUN_0069DA70(...)`
- `FUN_0069D660(...)`

Both are called with:

- `param_1 = local_c + local_24 + local_28`

and they behave very differently from the primary writers.

### `FUN_0069DA70(...)`

This is the flag-`1` family.

For a claimed block it writes a fixed two-word sentinel pair:

```cpp
*param_1 = 0xFFFFFFFE;
param_1[1] = 0xFFFFFFFF;
```

That already feels like a special “color-block family absent / degenerate / fill” code, not a scalar selector pattern.

### `FUN_0069D660(...)`

This is the stronger flag-`8` family.

At the top it immediately goes through:

```cpp
FUN_006A66C0(&local_28, uVar9 >> 8 | 0xFF000000, param_7);
```

and we already know from the earlier block-work that:

- `FUN_006A66C0(...)` synthesizes a representative BC1-like color block pair

Then, for claimed blocks, `FUN_0069D660(...)` writes:

```cpp
*param_1 = local_28;
param_1[1] = local_24;
```

So the companion stream is being populated by:

- either a fixed sentinel block pair
- or a synthesized representative BC1-like color block pair

That is about as strong a clue as we can ask for without a final consumer name.

So the companion stream now reads most naturally as:

- the color-block stream

## 3. Where `FUN_0069C3F0(...)` fits

The last missing bridge is `FUN_0069C3F0(...)`.

This helper:

- filters blocks by the simple selector families:
  - `0x00000000`
  - `0x55555555`
  - `0xAAAAAAAA`
  - `0xFFFFFFFF`
- decodes candidate blocks through:
  - `FUN_0069DD70(...)`
- chooses a dominant decoded value
- then stabilizes a representative block through:
  - `FUN_006A66C0(...)`
  - and a verify/replay through `FUN_0069DD70(...)`

That means `FUN_0069C3F0(...)` is the stage-2 bridge from:

- simple selector-patterned source blocks

into:

- a representative BC1-like color block family

That is exactly the kind of logic you would expect feeding the companion stream, not the primary scalar stream.

So the stream map now fits the family map very well:

- primary stream <- scalar/selector-family helpers
- companion stream <- BC1-like color-family helpers

## 4. Why this sharpens the `DXTA` result

This gives the `DXTA` story a much more meaningful interpretation.

We already established:

- `DXTA` keeps the richer stage-1 prepass
- `DXTA` loses the `0x210`-gated companion stream

Now we can say more specifically what that means.

If:

- the primary stream is the scalar/selector-style stream
- the companion stream is the BC1-like color-block stream

then `DXTA` is best interpreted as:

- a format that keeps the scalar/selector-style block family
- but does not carry the companion BC1-like color-block family

That is much closer to a real semantic distinction than “partial family member.”

## 5. Best current per-format reading

With this pass, the cleanest high-level interpretation is:

### `DXT4 / DXT5 / DXTL`

- primary scalar/selector stream
- companion BC1-like color-block stream

### `DXTA`

- primary scalar/selector stream
- no companion BC1-like color-block stream

### `DXT1 / DXTN`

- companion-style BC1-like path remains central
- different stage-1 family choice upstream

### `DXT2 / DXT3`

- primary stream family plus their own special lower-family stage

I still would not overclaim exact external codec names for every one of these without the final downstream consumer, but the internal relationship is much sharper now.

## 6. Strongest current conclusion

The strongest current conclusion is:

- `0x210` is not just “an extra trait”
- it most likely gates the presence of a companion BC1-like color-block stream

while the always-present primary stream is the scalar/selector-style stream.

So `DXTA` now looks much less mysterious:

- it is the upper-family variant that carries the scalar/selector side without the companion color-block side

## 7. Best next step

The strongest next move is to verify this interpretation from the consumer side instead of only from the producer/writer side.

The best targets are:

- whichever downstream consumer reads the reconstructed block records after `FUN_0069E1C0(...)`
- or a tight comparison of the final builder-side serialized layout in `FUN_0069E870(...)` for:
  - `DXTA`
  - `DXT5`
  - `DXTL`

That should let us replace:

- “BC1-like color-block stream”

with:

- the exact semantic role the engine assigns to that companion pair.
