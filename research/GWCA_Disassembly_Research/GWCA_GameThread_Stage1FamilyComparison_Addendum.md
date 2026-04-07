# GWCA GameThread Stage-1 Family Comparison Addendum

This pass compares the two stage-1 compact-family helpers directly:

- `FUN_0069CC40(...)`
- `FUN_0069BCD0(...)`

The goal was to answer the narrow question left open by the last few DXT notes:

- what exactly distinguishes the `DXTN` stage-1 branch from the richer upper-family stage-1 branch?

The result is that the split is now much cleaner:

- `FUN_0069CC40(...)` is a pure structural trivial-block peel
- `FUN_0069BCD0(...)` is already a selector/alpha-family interpreter with a learned dominant target

So the first-stage difference is not just "narrow versus rich" in a vague sense. The two helpers are solving different kinds of block regularity.

## 1. Caller confirmation

I re-checked both helpers with `FindCallers.java`.

Both currently show the same direct caller:

- `FUN_0069E870 @ 0069e870`

Specifically:

- `FUN_0069CC40(...)` is referenced from `0069EB97`
- `FUN_0069BCD0(...)` is referenced from `0069EBFD`

That keeps the comparison clean:

- both helpers are sibling stage-1 families selected by the same parent compressed-block builder
- the difference is in helper semantics, not in separate unrelated call trees

## 2. `FUN_0069CC40(...)`: structural trivial-block peel

This helper takes:

- a claimed-block mask in `param_4`
- an output claim mask in `param_3`
- a block stream of `ushort`-based compact records
- and a bit writer in `param_2`

The acceptance predicate is very narrow.

For an unclaimed block, it accepts only when:

- `*(int *)(block + 2) == -1`
- `block[0] <= block[1]`

That is a structural predicate. It does **not** decode colors, selectors, or alpha-family values. It just recognizes a very restricted trivial record shape.

Then it performs:

1. a cost-estimation pass
   - counts candidate blocks
   - computes a run-coded bit cost through the shared tables `DAT_00A27B38/39`
2. a write pass
   - sets `*param_1 |= 1`
   - claims accepted blocks in both masks
   - writes the run/state stream through the bit writer

That makes `FUN_0069CC40(...)` a pure "cheap peel" stage:

- identify an extremely simple block form
- claim it if the run-coded representation wins
- move on

It never learns or emits a representative value beyond the binary run/state stream.

## 3. `FUN_0069BCD0(...)`: selector-family interpreter plus dominant target learning

`FUN_0069BCD0(...)` is much richer even before its second-stage sibling comes in.

For an unclaimed block, it requires:

- the selector word `puVar4[1]` to be one of:
  - `0x00000000`
  - `0x24924924`
  - `0x49249249`
  - `0x6DB6DB6D`
  - `0x92492492`
  - `0xB6DB6DB6`
  - `0xDB6DB6DB`
  - `0xFFFFFFFF`
- and `*(short *)((int)puVar4 + 2) == (short)(selector_word >> 8)`

That second check ties the record body to a selector/alpha-style consistency condition rather than the simpler `-1` sentinel used by `FUN_0069CC40(...)`.

After that, it does not stop at pattern recognition. It derives a byte-scale representative value:

- pulls three small fields from `*puVar4`
- computes a candidate byte through several interpolation branches
- ignores zero-valued results
- accumulates a histogram in `local_408[256]`
- learns the most frequent byte as `local_424`

Then the second pass:

1. re-validates each block through:
   - `FUN_0069EE40(...)`
   - `FUN_0069DCE0(...)`
2. classifies accepted blocks into:
   - class `1` when decoded value is `0`
   - class `2` when decoded value matches the learned dominant byte `local_424`
3. estimates bit cost
4. if it wins:
   - sets `*param_1 |= 4`
   - writes the learned dominant byte `local_424`
   - writes the run/state stream
   - writes an extra per-class code from `DAT_00A27BCC` / `DAT_00A27BD8`

So `FUN_0069BCD0(...)` is already doing a compact semantic analysis of the block stream:

- validate against a legal selector family
- decode the family-specific scalar value
- learn a dominant representative
- emit both run states and class codes relative to that representative

That is fundamentally richer than `FUN_0069CC40(...)`.

## 4. The real stage-1 split

Putting them side by side:

### `FUN_0069CC40(...)`

- block test is structural
- no learned representative value
- binary accepted/not-accepted run family
- flag bit `1`
- narrow sentinel/trivial-block peel

### `FUN_0069BCD0(...)`

- block test is selector-family constrained
- block is decoded into a scalar family value
- dominant representative byte is learned
- ternary-ish classification (`0`, `dominant`, other)
- extra class-code emission
- flag bit `4`
- richer selector/alpha-family peel

So the stage-1 distinction is best described as:

- `FUN_0069CC40(...)` peels structurally trivial records
- `FUN_0069BCD0(...)` peels selector-regular records that still carry meaningful family data to decode and cluster

## 5. What that means for `DXTN`

This clarifies the earlier `DXTN` placement result a lot.

`DXTN` takes:

- `FUN_0069CC40(...)`
- then `FUN_0069C3F0(...)`

The upper family takes:

- `FUN_0069BCD0(...)`
- then `FUN_0069C3F0(...)`

So the difference is now very concrete:

- both branches share the same richer second-stage dominant-family codec in `FUN_0069C3F0(...)`
- but `DXTN` gets only the structural trivial-block peel before that
- the upper family gets an additional selector-family value-learning peel before that

That makes `DXTN` look like the simpler stage-1 sibling, not a wholly separate codec family.

## 6. Cleanest current interpretation

The strongest current model is:

- `FUN_0069CC40(...)` is a stage-1 fast path for blocks that are already in a sentinel/trivial reduced form
- `FUN_0069BCD0(...)` is a stage-1 fast path for blocks that preserve a legal selector-family shape and can therefore be clustered by a decoded scalar representative
- `FUN_0069C3F0(...)` is the shared heavier second stage that both families can still use afterward

So the parent branch split is not:

- "one family is BC1-like and the other is not"

It is more like:

- "both can feed into the same stronger dominant family, but only one has a meaningful selector-family prepass worth exploiting first"

## 7. Best next step

The strongest next step is to tighten the meaning of the scalar family in `FUN_0069BCD0(...)`:

- `FUN_0069DCE0(...)`
- `FUN_0069EE40(...)`
- and the second-pass class tables `DAT_00A27BCC` / `DAT_00A27BD8`

That should let us say whether the `FUN_0069BCD0(...)` prepass is best understood as an alpha-family peel, a selector-intensity peel, or another more specific compressed-block subfamily.
