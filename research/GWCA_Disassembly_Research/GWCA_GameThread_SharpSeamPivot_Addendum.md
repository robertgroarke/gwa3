# GWCA GameThread Sharp Seam Pivot Addendum

This pass follows the three sharper seams that emerged after the 30-function queue:

1. follow high typed channels `7 / 8 / 9` from `FUN_005F8810(...)`
2. go one layer deeper under `FUN_00614220(...)`
3. keep pushing the `DXTN` branch around:
   - `FUN_0069CC40(...)`
   - `FUN_0069C3F0(...)`
   - `FUN_0069E870(...)`
   - `FUN_0069E1C0(...)`

Fresh targets for this pass were:

- `FUN_0060D080`
- `FUN_00610330`
- `FUN_00610120`
- `FUN_00610160`
- `FUN_00614220`
- `FUN_00613DC0`
- `FUN_0069E870`
- `FUN_0069E1C0`

## Main Result

This pass sharpens all three seams in one consistent way.

### 1. High channels `7 / 8 / 9` are best treated as a separate owner-local control-notification band

The new wrappers make the split much cleaner:

- `FUN_006100A0(owner, type_id, payload, arg)`
  - requires `type_id >= 7`
  - forwards into:
    - `FUN_00628A10(type_id, payload, arg)`
- `FUN_00610160(owner, msg_id, payload, arg)`
  - requires `msg_id >= 0x56`
  - forwards into:
    - `FUN_006286D0(msg_id, payload, arg)`

So the engine now exposes two distinct “high-band” front doors:

- owner-local high typed channels
- raw global high message ids

That means the newer `7 / 8 / 9` notifications seen in `FUN_005F8810(...)` should be described as:

- a higher owner-local control/content band

not simply:

- more relation-subobject channels like `1 / 2 / 3 / 4`

### 2. `FUN_00614220(...)` is now strong enough to describe as a true item-emission engine

The fresh `FUN_00613DC0(...)` body makes the per-item worker much clearer:

- ask `FUN_00653D40(...)` how many UTF-16 code units fit
- optionally back up to a legal break through:
  - `FUN_00613750(...)`
- emit:
  - start pointer
  - remaining count
  - line/item span

So `FUN_00613DC0(...)` is not just a “small fill helper.”

It is the concrete:

- item/line splitter and legal-break worker

beneath:

- `FUN_00614220(...)`

### 3. `FUN_0069E870(...)` and `FUN_0069E1C0(...)` form a real symmetric codec shell

The fresh parent decompiles show a clear mirror:

- `FUN_0069E870(...)`
  - encodes per-level payloads
  - writes a compact stage-bit header into each level blob
- `FUN_0069E1C0(...)`
  - reads that same level header
  - dispatches to matching decode helpers by those stage bits

So the `DXTN` branch is now best seen as:

- one member of a staged compressed-family codec shell

with:

- stage flags carried explicitly in serialized per-level blobs

not just:

- a loose collection of neighboring helper calls

## High-Band Owner-Local Control Plane

### `FUN_006100A0(...)`: owner-local high-band dispatcher

Fresh decompilation is simple and decisive:

- assert `owner != 0`
- assert `type_id >= 7`
- validate owner through:
  - `FUN_00628800(...)`
- forward into:
  - `FUN_00628A10(type_id, payload, arg)`

This is the clearest current front door for the high channel band.

### `FUN_00610160(...)`: raw high-message dispatcher

This is the matching global wrapper:

- assert `owner != 0`
- assert `msg_id >= 0x56`
- validate owner
- forward into:
  - `FUN_006286D0(msg_id, payload, arg)`

So the old “high-band notifications” phrase now needs one more split:

- owner-local high typed channels
  - through `FUN_006100A0(...)`
- raw high global messages
  - through `FUN_00610160(...)`

### `FUN_00610120(...)`: owner-local direct verb wrapper

This helper is also small but useful:

- validate owner
- call:
  - `FUN_00628B00(param_2)`

That makes it look like a third small surface:

- owner-local direct verb / property helper

beside:

- high typed channels
- high global messages

### `FUN_0060D080(...)` and `FUN_00610330(...)`: dirty / reevaluate pair

These are the strongest “what do concrete controls do around the high band?” helpers in this pass.

#### `FUN_0060D080(owner)`

- validate owner
- call:
  - `FUN_006176B0(4, 0xFFFFFFFF)`

Best current reading:

- owner dirty / redraw / visual invalidation trigger

#### `FUN_00610330(owner)`

- validate owner
- call:
  - `FUN_0062A980()`

Since `FUN_0062A980(...)` was already tied to:

- `FUN_00628A10(4, 0, 0)`

this fresh wrapper now reads very naturally as:

- owner-local relation/layout reevaluation trigger

### Updated owner-local surface map

The cleanest current split is now:

- low typed channel band:
  - lifecycle / relation / registration
- high typed channel band:
  - concrete control/content notifications like `7 / 8 / 9`
- high global message band:
  - message ids `>= 0x56`
- direct owner verbs:
  - wrappers like `FUN_00610120(...)`

That is a cleaner and safer model than calling everything “typed channels.”

## One Layer Deeper Under `FUN_00614220(...)`

### `FUN_00613DC0(...)`: line/item splitter with legal-break backup

This is the key new detail under the producer engine.

Its behavior is:

1. compute remaining width budget:
   - `param_3 - indent_or_offset`
2. call:
   - `FUN_00653D40(...)`
   - to get the maximum code-unit fit
3. if a force-right/consume flag is active:
   - consume exactly that amount
4. otherwise:
   - find the tentative end pointer
   - if needed, back up using:
     - `FUN_00613750(...)`
   - avoid breaking inside protected line-break ranges
5. update:
   - current text pointer
   - remaining code-unit count
   - chosen line/item span

So the best current reading is:

- `FUN_00613DC0(...)` = per-item line-fit and legal-break worker

That finally gives the `FUN_00614220(...)` temporary record loop a concrete semantic heart.

### `FUN_00614220(...)`: stronger end-to-end reading

With `FUN_00613DC0(...)` in hand, the producer engine now reads more concretely:

1. normalize layout policy
2. derive working rect and line count
3. repeatedly call `FUN_00613DC0(...)`
   - to carve one legal segment/item at a time
4. for each item:
   - place it
   - optionally append geometry metadata
   - optionally emit a handle via:
     - `FUN_00653A60(...)`

So the strongest current description is no longer just:

- generic layout-and-emission engine

It is:

- policy-normalized segmented text/item layout and emission engine

### Why this matters for the generation seam

This narrows the next backend question sharply.

The remaining mystery is no longer:

- where are items split?

That part is now visible.

It is:

- what exact emitted object family `FUN_00653A60(...)` returns per produced segment/item

and:

- how the higher control/content band consumes those emitted objects afterward

## `DXTN` Branch: Parent-Side Symmetry

### `FUN_0069E870(...)`: staged encoder shell

Fresh decompilation makes the parent structure explicit.

Per level it:

1. allocate a level-local payload area
2. compute:
   - `local_3c = capability & 0x280`
   - `local_18 = capability & 0x210`
   - stride/layout terms:
     - `local_30`
     - `local_34`
     - `local_14`
3. run stage helpers based on format/capability:
   - `FUN_0069B720(...)`
   - `FUN_0069CC40(...)`
   - `FUN_0069BCD0(...)`
   - `FUN_0069C3F0(...)`
4. copy through any residual block streams
5. write final level size

The important new fact is:

- `puVar2[1]`
  - the per-level flag word

is the explicit serialized stage map that the decoder later reads.

### `FUN_0069E1C0(...)`: matching staged decoder shell

Fresh decompilation mirrors the encoder closely.

Per level it:

1. read the level flag word from:
   - `puVar2[1]`
2. dispatch decode helpers by those bits:
   - bit `1` -> `FUN_0069DA70(...)`
   - bit `2` -> `FUN_0069CFE0(...)`
   - bit `4` -> `FUN_0069D320(...)`
   - bit `8` -> `FUN_0069D660(...)`
3. reconstruct residual base/color streams
4. reconstruct residual companion streams
5. apply the `0x10` special post-pass for the `256 x 256` `DXT2 / DXT3` case

So `FUN_0069E1C0(...)` is not just “the parent decoder.”

It is:

- the stage-flag-driven inverse of `FUN_0069E870(...)`

### What this says about `DXTN`

This symmetry sharpens the current `DXTN` reading:

- `DXTN` is not merely “the format that happens to call `FUN_0069CC40(...)` and `FUN_0069C3F0(...)`”
- it is the format whose per-level stage map selects:
  - the trivial-block peel family
  - plus the dominant-color companion family

inside a broader staged codec shell shared with the other upper compressed families.

That is a stronger architectural statement than any single helper can give us alone.

## Best Current Interpretation

After this pass, the strongest integrated model is:

### Control side

- low owner-local band:
  - lifecycle / relation
- high owner-local band:
  - concrete control/content notifications
- high global message band:
  - raw message ids `>= 0x56`

### Generation side

- `FUN_00614220(...)`
  - segmented item layout/emission engine
- `FUN_00613DC0(...)`
  - per-item fit and legal-break worker

### Compressed-family side

- `FUN_0069E870(...)`
  - staged encoder shell
- `FUN_0069E1C0(...)`
  - matching staged decoder shell
- `DXTN`
  - one specific stage-combination inside that shell

That is a cleaner and more composable model than the earlier notes had separately.

## Best Next Step

The strongest immediate next steps now are:

1. follow high-band owner-local notifications from:
   - `FUN_005F8810(...)`
   - through more callers of `FUN_006100A0(...)`
2. continue one layer below `FUN_00614220(...)` by tightening:
   - `FUN_00653A60(...)`
   - and any remaining emitted-object helpers it relies on
3. map the exact stage-bit correspondence between:
   - encoder helpers under `FUN_0069E870(...)`
   - decoder helpers under `FUN_0069E1C0(...)`

That should answer:

- whether the high channel band is per-control class specific or reused broadly
- what each emitted segment/item object really is before registration
- and how much of the compressed-family shell can now be expressed as a stable stage-bit taxonomy

## Supporting Artifacts

- `tools/ghidra_projects/gw_findcallers_006100a0_temp185.log`
- `tools/ghidra_projects/gw_decomp_sharp_seams_temp186.log`
