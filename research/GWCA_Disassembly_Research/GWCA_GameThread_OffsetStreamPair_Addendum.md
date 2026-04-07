# GWCA GameThread Offset Stream Pair Addendum

This pass goes back to the exact inner seam that still mattered after the container-side detours:

- the offsetted path in `FUN_0069E1C0(...)`
- especially the helpers reached through:
  - `local_c`
  - `local_c + local_24 + local_28`

The goal was to answer the strongest remaining question in the `DXTA` thread:

- what does the `0x210` capability trait actually buy the format inside the compressed-family core?

The result is the clearest structural answer so far:

- `0x210` looks like the presence of a second per-block **two-word companion stream**

That still leaves the semantic label of that stream open, but it is much stronger than the earlier “some extra trait” wording.

## 1. The two offset terms matter differently

From `FUN_0069E1C0(...)`:

```cpp
local_30 = FUN_00689e90(param_3) & 0x210;
local_38 = FUN_00689e90(param_3) & 0x280;
local_24 = -(uint)(local_38 != 0) & 2;
local_28 = (param_3 != 0x15) - 1 & 2;
local_14 = (-(uint)(local_30 != 0) & 2) + local_28 + local_24;
```

and the key calls are:

```cpp
FUN_0069da70(local_c + local_24 + local_28,...)
FUN_0069cfe0(local_c,...)
FUN_0069d320(local_c,...)
FUN_0069d660(local_c + local_24 + local_28,...)
```

Then later, when `local_30 != 0`, there are two copyback loops:

```cpp
puVar6 = local_c + local_24 + local_28;
...
*puVar6 = uVar7;

puVar6 = local_c + local_24 + local_28 + 1;
...
*puVar6 = uVar7;
```

That is the key structural clue.

The offset path is not a scalar side value. It is two adjacent words per block:

- `base + offset + 0`
- `base + offset + 1`

and those two words are copied back only when:

- `local_30 != 0`
- i.e. when `FUN_00689E90(format) & 0x210 != 0`

So the trait behind `0x210` is most naturally read as:

- this format uses an extra recovered two-word block stream

## 2. Primary vs companion stream helpers

The helper split reinforces that reading.

### Primary-stream writers

- `FUN_0069CFE0(...)`
- `FUN_0069D320(...)`

Both are called with:

- `param_1 = local_c`

and both write:

```cpp
*param_1 = ...
param_1[1] = ...
```

for claimed blocks.

So these are clearly writing a two-word **primary** per-block record stream.

### Companion-stream writers

- `FUN_0069DA70(...)`
- `FUN_0069D660(...)`

Both are called with:

- `param_1 = local_c + local_24 + local_28`

and they also write:

```cpp
*param_1 = ...
param_1[1] = ...
```

for claimed blocks.

So these are clearly writing a separate two-word **companion** per-block record stream.

That is the strongest structural conclusion of the pass.

## 3. What each family writes

The companion-stream conclusion gets even stronger when you look at the four helper bodies side by side.

### `FUN_0069DA70(...)`

This is the flag-`1` family.

When the decoded bit says “claimed,” it writes:

```cpp
*param_1 = 0xfffffffe;
param_1[1] = 0xffffffff;
```

and marks both masks.

So this helper writes a very specific two-word sentinel pair into the companion stream.

### `FUN_0069D660(...)`

This is the flag-`8` family.

At the top it synthesizes a representative pair:

```cpp
FUN_006a66c0(&local_28, uVar9 >> 8 | 0xff000000, param_7);
...
local_20 = local_24;
local_1c = local_28;
```

Then, for claimed blocks:

```cpp
*param_1 = local_28;
param_1[1] = local_24;
```

So the heavier flag-`8` family also writes a two-word pair into the same companion stream, but now it is a synthesized representative block pair rather than a fixed sentinel.

### `FUN_0069CFE0(...)`

This is the flag-`2` family.

It writes its two-word outputs to the primary stream via:

```cpp
*param_1 = *local_18;
param_1[1] = local_38[local_1c * 2 + 1];
```

### `FUN_0069D320(...)`

This is the flag-`4` family.

It also writes its two-word outputs to the primary stream via the same basic form:

```cpp
*param_1 = *local_18;
param_1[1] = local_38[local_1c * 2 + 1];
```

So the compressed-family split is now much easier to describe:

- flag `2` / `4` families populate the primary two-word stream
- flag `1` / `8` families populate the companion two-word stream

## 4. Why `DXTA` falls out where it does

This is the strongest payoff for the `DXTA` thread.

We already knew:

- `DXTA / 0x14` participates in the richer stage-1 family
- but not in the heavier stage-2 family

Now the structural reason is more visible:

- the heavier stage-2 path is one of the families that populates the companion two-word stream
- and that companion stream only exists when `capability & 0x210 != 0`

But for `DXTA / 0x14`:

- capability = `0xA1`
- `0xA1 & 0x210 = 0`

So `DXTA` is not just “missing the second-stage family” abstractly.
It is missing the format trait that gives the core a place to carry that second recovered two-word block stream.

That is much more concrete than anything we had before.

## 5. Best current structural model

The cleanest current model is:

### Per-block primary stream

- always addressed from `local_c`
- written by:
  - `FUN_0069CFE0(...)`
  - `FUN_0069D320(...)`

### Per-block companion stream

- addressed from `local_c + local_24 + local_28`
- copied back only when `local_30 != 0`
- written by:
  - `FUN_0069DA70(...)`
  - `FUN_0069D660(...)`

So `0x210` is best understood as:

- the trait that enables the companion per-block stream

That is stronger and cleaner than:

- “maybe a second plane”

It is now fair to say:

- a second two-word block stream is present

What remains open is the semantic identity of that stream.

## 6. What `local_24` and `local_28` most likely are

We still should not overclaim here, but the offsets now look much less mysterious:

- `local_24 = 2` when `capability & 0x280 != 0`
- `local_28 = 2` only for `DXTL / 0x15`

Since the companion stream starts at:

- `base + local_24 + local_28`

the most likely role of these terms is:

- format-family-specific placement of the companion two-word stream inside the per-block record layout

So the offsets do not appear to create or remove the extra stream.
That role belongs to `0x210`.
The offsets appear to tell the core where, within the per-block record, the companion stream lives for that particular format family.

## 7. Strongest current conclusion

The strongest current conclusion is now:

- `DXTA` is excluded from the heavier upper-family path because it lacks the `0x210` trait
- that trait corresponds to the existence/use of a companion two-word per-block stream
- the inner helpers already separate cleanly into:
  - primary-stream writers
  - companion-stream writers

So the `DXTA` story is no longer just about “family membership.”
It is about record layout and stream presence inside the compressed-family core.

## 8. Best next step

The strongest next move is to identify what the companion two-word stream *means* semantically.

The best targets are:

- the consumers that read the recovered block records after `FUN_0069E1C0(...)`
- or the corresponding builder-side interpretation in `FUN_0069E870(...)`

In particular, the highest-value next pass is:

- compare the final reconstructed record layout for a `DXTA` case versus a `DXT5` or `DXTL` case
- and track which downstream consumer reads the companion pair at `base + local_24 + local_28`

That should let us move from:

- “a second two-word stream exists”

to:

- “that second stream is the companion alpha/intensity block family”

or whatever more exact name the consumer logic supports.
