# GWCA GameThread Builder Serialization Split Addendum

This pass takes the last stream-semantics result and closes the loop from the builder side:

- `FUN_0069E870(...)`

The goal was to verify the stream interpretation from final serialization rather than only from the inner helper bodies.

The result is clean:

- the builder emits the primary two-word block stream through one path
- and emits the companion two-word block stream only through a second path gated by `local_18 != 0`

That means the serialized layout itself now supports the interpretation we reached from the decode-side writer helpers.

## 1. The builder maintains one per-block working array with optional companion region

From `FUN_0069E870(...)`:

```cpp
local_3c = capability & 0x280;
local_30 = -(uint)(local_3c != 0) & 2;
local_18 = capability & 0x210;
local_34 = (param_2 != 0x15) - 1 & 2;
local_14 = (-(uint)(local_18 != 0) & 2) + local_34 + local_30;
```

Then all stage families work over `local_10` with stride `local_14`.

That already tells us the builder is preparing a format-dependent per-block record layout, not just a flat stream.

The key split is:

- primary families consume/write from `local_10`
- companion families consume/write from `local_10 + local_30 + local_34`

and the final serialization mirrors that exact distinction.

## 2. Primary-stream serialization path

After all stage helpers run, the builder serializes the primary stream in this block:

```cpp
if (((local_3c != 0) || (param_2 == 0x15)) && (uVar8 = 0, uVar6 != 0)) {
  uVar6 = 1;
  puVar9 = local_10;
  do {
    if ((*(uint *)(iVar7 + (uVar8 >> 5) * 4) & uVar6) == 0) {
      *puVar4 = *puVar9;
      puVar4[1] = puVar9[1];
      puVar4 = puVar4 + 2;
    }
    ...
    puVar9 = puVar9 + local_14;
  } while (uVar8 < local_8);
}
```

This is the final primary-stream append:

- source = `local_10`
- two words per surviving block
- output appended directly to the serialized mip payload

So the primary stream is not hypothetical anymore. It has an explicit dedicated serialization loop.

## 3. Companion-stream serialization path

Then, only when `local_18 != 0`, the builder runs two more loops:

```cpp
if ((local_18 != 0) && (uVar6 = 0, local_8 != 0)) {
  uVar8 = 1;
  puVar9 = local_10 + local_30 + local_34;
  do {
    if ((*(uint *)(local_c + (uVar6 >> 5) * 4) & uVar8) == 0) {
      *puVar4 = *puVar9;
      puVar4 = puVar4 + 1;
    }
    ...
    puVar9 = puVar9 + local_14;
  } while (uVar6 < local_8);

  uVar8 = 0;
  uVar6 = 1;
  puVar9 = local_10 + local_30 + local_34 + 1;
  do {
    if ((*(uint *)(local_c + (uVar8 >> 5) * 4) & uVar6) == 0) {
      *puVar4 = *puVar9;
      puVar4 = puVar4 + 1;
    }
    ...
    puVar9 = puVar9 + local_14;
  } while (uVar8 < local_8);
}
```

This is exactly the shape we wanted to confirm:

- first serialize companion word `0`
- then serialize companion word `1`
- and do both only when `capability & 0x210 != 0`

So the builder is not just keeping an internal companion stream for convenience.
It serializes that stream as a real distinct payload region in the final block data.

That is the strongest confirmation yet that `0x210` means a genuinely present second block stream, not just an internal optimization flag.

## 4. What this means for `DXTA`

This makes the `DXTA` split very concrete.

For `DXTA / 0x14`:

- capability = `0xA1`
- `capability & 0x210 = 0`

So in `FUN_0069E870(...)`:

- the companion serialization loops do **not** run

That means the final serialized `DXTA` mip payload contains:

- the stage-1/primary-side data
- but not the appended companion two-word-per-block stream

This is stronger than saying:

- "`DXTA` skips stage 2"

We can now say:

- "`DXTA` does not serialize the companion block-stream payload that `DXT4`, `DXT5`, and `DXTL` do"

## 5. What this means for `DXT5` and `DXTL`

For `DXT5 / 0x13`:

- capability = `0xB1`
- `capability & 0x210 != 0`
- `capability & 0x280 != 0`
- so:
  - primary stream path can run
  - companion stream serialization definitely runs
  - companion start offset includes `local_30 = 2`

For `DXTL / 0x15`:

- capability = `0x11`
- `capability & 0x210 != 0`
- `capability & 0x280 == 0`
- and the special-case term gives:
  - `local_34 = 2`

So `DXTL` also serializes the companion stream, but its placement within the per-block working layout is shifted by the `DXTL`-specific offset term rather than the broader `0x280` family term.

That is useful because it separates two ideas:

- `0x210` decides whether the companion stream exists at all
- `0x280` and the special `DXTL` case decide where that stream lives in the working record layout before serialization

## 6. Best current comparison: `DXTA` vs `DXT5` / `DXTL`

This is the cleanest current builder-side comparison:

### `DXTA`

- richer stage-1 family can run
- primary stream data can exist
- no companion serialization block
- final payload lacks the appended companion per-block stream

### `DXT5`

- richer stage-1 family can run
- companion family can run
- final payload includes appended companion stream
- companion placement uses the `0x280`-family offset

### `DXTL`

- richer stage-1 family can run
- companion family can run
- final payload includes appended companion stream
- companion placement uses the `DXTL`-specific offset instead

That means the difference between `DXTA` and the other upper-family members is now visible in final serialized layout, not just in helper selection.

## 7. Why this strengthens the stream-semantics interpretation

The previous note argued:

- primary stream looks scalar/selector-like
- companion stream looks BC1-like color-block-like

This pass does not by itself prove the semantic labels, but it strongly supports the structural part:

- these are two independently serialized payload regions
- not just one record interpreted two ways

So the interpretation:

- primary scalar/selector stream
- companion color-block stream

is now resting on both:

1. the inner writer/helper bodies
2. the final builder-side serialized layout

That is a good place to be before trying to name the companion stream more precisely.

## 8. Strongest current conclusion

The strongest current conclusion is:

- `0x210` gates a real appended companion two-word-per-block payload stream in the final serialized data
- `DXTA` lacks that serialized companion stream
- `DXT5` and `DXTL` both include it

So the `DXTA` partial-membership result is now visible:

- in stage/helper selection
- in decode/rebuild layout
- and in final serialized builder output

## 9. Best next step

The strongest next move is the consumer side:

- find what reads the serialized companion stream after build or after decode/reconstruct
- and identify whether that stream is semantically:
  - color endpoints/selectors
  - companion alpha/color block data
  - or some other engine-local block family

The most valuable concrete target now is:

- a downstream consumer of the reconstructed per-block records after `FUN_0069E1C0(...)`

because at this point the existence and serialization of the stream are no longer in doubt. What remains is its exact meaning.
