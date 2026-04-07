## `Gw.exe` Frame Callback Measure And Relation Index Addendum

This pass continues from the metric-metadata note by decompiling the helper layer directly under measurement fallback and relation/hash insertion:

- `FUN_00629E80`
- `FUN_0062AEC0`
- `FUN_0062DCB0`
- `FUN_0062D450`
- `FUN_00473D80`

The goal was to close two remaining gaps:

- where the child/control size used by the layout engine actually comes from
- how constructed relation nodes are indexed and reordered after insertion

## Source artifacts

These results come from:

- [gw_decomp_measure_hash_temp87.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_measure_hash_temp87.log)
- [gw_decomp_metric_metadata_temp86.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_metric_metadata_temp86.log)
- [gw_decomp_metric_builder_temp85.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_metric_builder_temp85.log)

## High-level result

This pass makes both the measurement path and the relation index path much clearer.

First, `FUN_00629E80(...)` shows that layout size is not coming from one single child metric cache.
It is a staged measurement pipeline:

- clamp requested bounds
- ask global message `0x15` for container margins/insets
- ask global message `0x38` for preferred content size
- fall back to local default size helpers if the preferred size is incomplete

So the layout engine underneath GWCA’s hook seam is a real multi-stage measurement system.

Second, the relation insertion helpers now split cleanly:

- `FUN_00473D80(...)` inserts a node into both a global list and a hash-bucket list keyed by a per-node hash
- `FUN_0062D450(...)` is a keyed lookup over that relation hash table
- `FUN_0062DCB0(...)` reorders a node inside an intrusive list according to `node[10]`
- `FUN_0062AEC0(...)` computes parent-relative anchoring offsets and then enqueues the owner in an intrusive "dirty/active" list

That means the constructor path is no longer just "hash insert plus parent link."
It is:

- parent/type/hash attachment
- hash-bucket insertion
- stable ordered-list placement
- anchored placement caching

## `FUN_00629E80(out_size, requested_bounds)`: staged content measurement pipeline

This helper is the strongest measurement result so far.

### Step 1: clamp requested bounds against local minimums

It starts with:

- `local_1c = requested_width`
- `local_18 = requested_height`

then calls:

- `FUN_00629E10(&local_c)`

and clamps each requested dimension upward against local defaults.
If the requested dimension is negative/invalid, it substitutes `100000.0`.

So the helper treats the incoming bounds as soft constraints, not hard dimensions.

### Step 2: ask global message `0x15` for surrounding insets

It zeroes a four-float block and sends:

- `FUN_006286D0(0x15, 0, &local_40)`

The resulting four values are then used as inset/margin terms:

- `local_40`
- `local_38`
- `local_3C`
- `local_34`

That is very strong evidence that global message `0x15` is a margin/inset contribution query.

### Step 3: derive available content area

It subtracts those insets from the clamped requested size:

- `available_w = requested_w - left_inset - right_inset`
- `available_h = requested_h - top_inset - bottom_inset`

and floors each at zero.

### Step 4: ask global message `0x38` for preferred content size

It seeds:

- `local_1C = available_w`
- `local_18 = available_h`
- `local_28 = &local_14`

then calls:

- `FUN_006286D0(0x38, &local_30, 0)`

So global message `0x38` is now strongly associated with:

- preferred content-size query / measurement negotiation

### Step 5: fallback size chain

It then checks:

- `FUN_006290B0(uVar1)`

and if active, fills missing width/height through fallbacks:

- `FUN_00619480(&local_c)`
- `FUN_00622760(&local_24, &local_1c)`

So size negotiation is layered:

1. request insets
2. request preferred content size
3. if incomplete, use built-in defaults

### Step 6: return final measured size

Finally:

- `out_w = left_inset + right_inset + measured_w`
- `out_h = top_inset + bottom_inset + measured_h`

This is a real frame-layout measure pass, not an isolated metric read.

## What this means for the layout engine

The earlier notes showed:

- child metric ids like `0x89`, `0x94`, `0x105`, `0x114`

feeding into placement.

This helper shows the layer above those metric ids:

- container margins via `0x15`
- preferred size via `0x38`
- fallback/default size helpers

So the best current model is:

- child metrics handle local child placement
- global messages `0x15` and `0x38` handle parent/container measure negotiation

## `FUN_0062AEC0(pos, size, mask)`: parent-relative anchor-offset computation

This helper finally makes the `shrink_mode` path in `FUN_0060FB00(...)` more concrete.

It begins by recovering the current owner via:

- `FUN_0062DAE0()`

Then it reads the owner bounds:

- `+0xEC`
- `+0xF0`
- `+0xF4`
- `+0xF8`

and computes the owner width/height.

It stores:

- left/top-style offsets at `+0x0C` / `+0x10`
- right/bottom-style offsets at `+0x14` / `+0x18`

depending on anchor mask bits:

- horizontal masks `2`, `8`, centered otherwise
- vertical masks `4`, `0x10`, centered otherwise

Then it relinks the current owner into the intrusive list rooted at:

- `DAT_00BD0CCC`
- `DAT_00BD0CD0`

So this helper is best read as:

- compute cached anchor offsets relative to the parent bounds
- then mark/requeue the owner in an active/dirty anchored-layout list

That makes the earlier `FUN_0060FB00(..., shrink_mode=1)` path much clearer:

- it is not just "consume rect"
- it is also caching anchored placement against the current parent owner

## `FUN_00473D80(node, hash)`: dual insertion into global and bucket lists

This helper is the core of relation hash insertion.

It:

- writes the node hash into a node-relative field:
  - `*(node + in_ECX[3]) = hash`
- picks a hash bucket via:
  - `bucket = in_ECX[7] & hash`
- may grow the table if a bucket chain gets too long

Then it performs two intrusive-list insertions:

1. insert node into a global list anchored at `in_ECX[1]`
2. insert node into the chosen bucket list anchored by the selected bucket record

So every inserted node participates in:

- one global iteration order
- one hash-bucket chain

That is the cleanest structural explanation yet for how the relation nodes are indexed.

## `FUN_0062D450(key_ptr)`: relation hash-table lookup by `(owner, type)`

This helper is the natural inverse of `FUN_00473D80(...)`.

It:

- computes the same hash through `FUN_0062D880()`
- selects the bucket via:
  - `mask & hash`
- walks the bucket chain

and matches:

- stored hash
- `*(entry + 8) == *param_1`
- `*(entry + 0xC) == param_1[1]`

So the keyed lookup is effectively over:

- object/root identity
- plus type/category

This aligns nicely with the constructor metadata we saw in `FUN_0062C650(...)`, where:

- parent linkage
- type id
- hashed payload id

were all installed together.

## `FUN_0062DCB0(node)`: stable ordered relink by `node[10]`

This helper walks an intrusive list looking for the insertion point for `param_1`.

The key comparison is:

- `param_1[10] < piVar2[10]`

and then it unlinks/relinks the node accordingly.

So this is not hash insertion.
It is a secondary ordered-list placement based on a node-local sort key at index `10`.

That means the relation system maintains at least two different organization schemes:

- hash buckets for lookup
- ordered intrusive lists for stable traversal / rendering / update order

## Updated constructor model

With this pass included, the strongest constructor story is now:

1. `FUN_00627740(...)`
   - allocate/register global runtime id
2. `FUN_0062C650(parent, type, arg)`
   - attach parent/type/hash metadata
   - link into parent relation list
   - hash-insert through `FUN_00473D80(...)`
   - maybe verify through `FUN_0062D450(...)`
3. `FUN_00627D70(callback, payload)`
   - attach callback/handler registry state
4. later placement helpers
   - measure through `FUN_00629E80(...)`
   - cache anchor offsets through `FUN_0062AEC0(...)`
   - reorder through `FUN_0062DCB0(...)`

That is a significantly more concrete frame/relation pipeline than we had before.

## Strongest current architectural picture

At this point the cleanest model for the reverse path is:

- constructed control nodes are relation owners with:
  - runtime ids
  - parent linkage
  - type metadata
  - hash-table membership
  - ordered-list membership
  - callback registry state
- measurement is negotiated through:
  - global message `0x15` for insets
  - global message `0x38` for preferred content size
  - local fallback/default size helpers
- local child metrics are then placed with the metric reader `FUN_0060FB00(...)`

That puts the GWCA-hooked frame seam firmly inside a real multi-stage UI layout and relation engine.

## Best next step

The highest-yield next pass is to name the still-anonymous measure/default helpers and the constructor-adjacent frame helpers:

- `FUN_00629E10`
- `FUN_00619480`
- `FUN_00622760`
- `FUN_006240D0`
- `FUN_00613400`

Those should tell us:

- what the local minimum/default size sources actually are
- and what kind of frame/control-class state gets initialized immediately around construction
