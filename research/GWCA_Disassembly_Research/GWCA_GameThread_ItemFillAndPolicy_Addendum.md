## `Gw.exe` Frame Callback Item Fill And Policy Addendum

This pass followed the next backend targets under the producer engine:

- `FUN_00613DC0`
- `FUN_00612C20`
- `FUN_00613F40`
- `FUN_00653A60`

The goal was to answer two questions:

- what a single generated item really is before final emission
- how the producer engine gets its default policy/style state

## Source artifacts

These results come from:

- [gw_decomp_itemfill_temp115.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_itemfill_temp115.log)
- [gw_findcallers_00613dc0_temp116.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00613dc0_temp116.log)

## High-level result

This pass sharpened the backend stack a lot:

- `FUN_00613DC0(...)` is not the full item constructor
  - it is the per-line / per-segment splitter and width-fit worker used only by `FUN_00614220(...)`
- `FUN_00653A60(...)` is the actual emitted-handle creator
  - it builds an object through `FUN_00652500(...)`
  - then extracts an `HGrModel` handle from it
- `FUN_00613F40(...)` returns the active/default policy record from a global policy table
- `FUN_00612C20(...)` is a style/policy cache builder and resolver, not a direct geometry emitter

So the backend is now much more legible:

- policy lookup and cache
- line/segment split
- placement and per-item geometry
- final emitted-handle creation

## `FUN_00613DC0(...)`: line break and segment-fit worker

The caller map is tiny and decisive:

- only `FUN_00614220(...)` calls it

That alone already makes it look like an internal worker rather than a shared object builder.

### What it does

Its inputs are roughly:

- `param_1`
  - current UTF-16 pointer
- `param_2`
  - remaining character count
- `param_3`
  - available width/extent
- `param_4`
  - current offset/bias
- `param_5`
  - flag byte
- `param_6..param_8`
  - style/context forwarded deeper
- `param_9`
  - out count/fit result
- `param_10`
  - fallback count sink when `param_9 == 0`

The most important call is:

- `FUN_00653D40(...)`

That helper is used first to compute how many UTF-16 code units fit in the available extent.

Then `FUN_00613DC0(...)` post-processes that fit result by scanning for breakpoints:

- explicit newline via:
  - `FUN_0046B9A0(text, 10, count)`
- break opportunities via:
  - `FUN_00613750(...)`
- whitespace/control threshold checks like:
  - `*puVar5 < 0x21`

Finally it updates:

- current text pointer
- remaining count

So the cleanest interpretation is:

- `FUN_00613DC0(...)` takes a raw candidate line extent
- finds the actual segment boundary
- then advances the producer to the next segment

This is a segment-selection worker, not an object emitter.

### Special flag behavior

If `(param_5 & 0x10) != 0`, it skips the newline/breakpoint search and directly advances by the fitted count from `FUN_00653D40(...)`.

That looks like a no-wrap / strict-fit mode.

So one useful semantic split now is:

- normal mode
  - fit width, then back up to a break boundary
- `0x10` mode
  - fit width, consume exactly that range

## `FUN_00653A60(...)`: emitted-handle creator

This function turned out to be the real creation seam for produced items.

Its flow is:

1. resolve a required owner/context object through:
   - `FUN_0046F9B0(param_1, 0x6772666F)`
2. validate:
   - source text pointer
   - positive extent
3. increment a global generation counter:
   - `DAT_00BD89D8`
4. create an object with:
   - `FUN_00652500(text_begin, text_end, extent, ...)`
5. obtain a runtime context through:
   - `FUN_0046D0B0()`
6. query an `HGrModel` handle via:
   - `FUN_0046F7B0(obj, "HGrModel", 0x67726D64, 0, 0)`
7. release the temporary object reference
8. return the handle

That is the strongest direct emission result so far.

So the per-item path inside `FUN_00614220(...)` now looks like:

- split text segment with `FUN_00613DC0(...)`
- compute placement
- create handle with `FUN_00653A60(...)`
- append handle to output vector

This finally separates:

- segment selection
- from actual emitted-object creation

## `FUN_00613F40(...)`: default policy record resolver

This helper is much cleaner than the others.

It:

- returns `&DAT_00BB53E4` when no policy table is loaded
- otherwise clamps the requested index
- walks backward until it finds an active record with flag byte:
  - `record + 0x0B != 0`
- returns:
  - `DAT_00BB53D4 + index * 0x10`

So the policy table shape is now partially visible:

- fixed-size `0x10` records
- active flag at `+0x0B`
- global base:
  - `DAT_00BB53D4`
- count:
  - `DAT_00BB53DC`

This means `FUN_00614220(...)` really is pulling default layout/style policy from a compact global record table before it starts producing items.

## `FUN_00612C20(...)`: style/policy cache builder

This function is broader, but even the visible part is enough to classify it.

### Global cache behavior

It manages a second global table:

- base:
  - `DAT_00BB53C4`
- capacity:
  - `DAT_00BB53C8`
- count:
  - `DAT_00BB53CC`

These are `0x10`-byte records, and the function grows/reallocates the table as needed.

So `FUN_00612C20(...)` is caching resolved policy/style entries by index.

### Source lookup behavior

It then attempts to resolve source records through:

- `FUN_006129A0(DAT_00BD011C, DAT_00BD0124 + DAT_00BD0120 + DAT_00BD0128, &local_28)`

If no source record is found, it creates a default cached entry using:

- `FUN_00653C40()`

and writes default fields like:

- handle/id at offset `+0`
- type/tag-ish field `0xE` at offset `+8`
- zero at offset `+0x0C`

If a source record is found, it looks backward for a usable source entry in a `0x1C`-byte record array and then copies/transforms fields from that source into the cache.

It also uses:

- `FUN_00653BA0(...)`

on one branch, which looks like conversion from source data into cached metric/style values.

So the strongest current interpretation is:

- `FUN_00612C20(...)` builds or refreshes compact cached policy records from a larger source descriptor table

That fits perfectly with how `FUN_00614220(...)` uses it before laying out items.

## What this pass changes

Before this pass, the missing detail was “what is one item?”

After this pass, the answer is much cleaner:

- an item is not created by `FUN_00613DC0(...)`
- `FUN_00613DC0(...)` only chooses the text segment boundary for one produced line/item
- the actual item/object handle is created by `FUN_00653A60(...)`
- policy defaults come from:
  - `FUN_00613F40(...)`
- cached style/layout records are built by:
  - `FUN_00612C20(...)`

So the engine underneath `FUN_00614220(...)` now has a believable internal split instead of one opaque blob.

## Updated working model

The cleanest current producer stack is now:

- `FUN_00613F40(...)`
  - resolve default active policy record
- `FUN_00612C20(...)`
  - build/refresh compact cached style record(s)
- `FUN_00613DC0(...)`
  - choose the next text segment / line break boundary
- `FUN_00614220(...)`
  - drive layout, placement, metadata output, handle output
- `FUN_00653A60(...)`
  - create one emitted handle/object from a chosen segment
- `FUN_006163E0(...)`
  - bucket and post-process emitted handles

That is the strongest internal decomposition we have recovered for this branch.

## Best next step

The next best step is to decompile:

- `FUN_00653D40`
- `FUN_00613750`
- `FUN_00652500`
- `FUN_00653C40`
- `FUN_00653BA0`

Why these are now the right targets:

- `FUN_00653D40(...)` appears to be the true width-fit / glyph-span measurer
- `FUN_00613750(...)` looks like the break-opportunity classifier
- `FUN_00652500(...)` appears to be the actual emitted object constructor beneath `FUN_00653A60(...)`
- `FUN_00653C40(...)` and `FUN_00653BA0(...)` are the remaining style/policy value producers used by the cache layer

That should let the next pass answer the last big backend question:

- exactly how a text segment is measured, classified, and turned into a concrete emitted object.*** End Patch
