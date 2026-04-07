# GWCA GameThread Emitted Segment Pipeline Appendix

This appendix consolidates the emitted-segment branch that sits under:

- `FUN_00614220(...)`
- and the final per-segment wrapper:
  - `FUN_00653A60(...)`

The goal is to make the recovered backend readable as one pipeline rather than as a chain of scattered addenda.

## Scope

This appendix covers the branch that turns a chosen UTF-16 segment into:

- a composed rendered object
- an `HGrModel` handle
- and finally an atlas-backed registered piece path beneath that handle

It is not primarily about:

- the high-band owner-local channel plane
- the transfer-bank workers
- or the staged codec branch

## High-Level Pipeline

The strongest current end-to-end model is:

1. resolve policy/style defaults
2. measure how much UTF-16 text fits
3. choose a legal segment boundary
4. plan piece range and atlas sizing
5. iterate the chosen span into effect-aware subpieces
6. build or convert those subpieces
7. compose lane coverage into destination piece data
8. place the result into a `GrTex2d` atlas rectangle
9. wrap the result as a composed rendered object
10. expose it as an `HGrModel`

That means the engine is not producing abstract layout tokens.
It is producing real rendered text/image pieces backed by graphics objects.

## Compact Matrix

| stage | main function(s) | current role |
| --- | --- | --- |
| policy default | `FUN_00613F40(...)` | resolve active default policy record |
| policy cache | `FUN_00612C20(...)` | build/refresh compact cached style record |
| default font | `FUN_00653C40(...)`, `FUN_00653BA0(...)` | construct default or alternate `HGrFont` handles |
| width fit | `FUN_00653D40(...)` | compute how many code units fit in a width budget |
| legal break | `FUN_00613750(...)` | classify valid break boundaries |
| segment split | `FUN_00613DC0(...)` | choose the next line/item span |
| producer loop | `FUN_00614220(...)` | drive layout, placement, metadata, handle output |
| span measure | `FUN_006530A0(...)` | measure the fitted span against glyph/effect metrics |
| piece iterator | `FUN_006541A0(...)` | iterate the fitted span into subpieces |
| clipping | `FUN_00669800(...)` | map source rect into clipped piece space |
| atlas planner | `FUN_00669DF0(...)` | choose slice/page and texture dimensions |
| lane extents | `FUN_0066B130(...)`, `FUN_0066B200(...)` | compute left/right extents across effect lanes |
| lane compositor | `FUN_0066B4D0(...)` | composite one lane into destination coverage |
| lane bounds | `FUN_0066B4A0(...)` | cheap bounds/metric companion |
| descriptor classify | `FUN_00653890(...)` | map code unit into cached descriptor range |
| range load | `FUN_00654500(...)` | lazily load and populate font/code-unit range data |
| glyph descriptor | `FUN_0066B810(...)` | hashed glyph-descriptor lookup by code unit |
| converter lookup | `FUN_0066A8C0(...)` | hashed converter lookup by effect signature |
| lane stream ints | `FUN_0066BBA0(...)`, `FUN_0066BB10(...)` | decode packed control integers and refresh lane state |
| piece constructor | `FUN_00652500(...)` | build the composed rendered object for one segment |
| handle wrapper | `FUN_00653A60(...)` | query and return the segment’s `HGrModel` |
| atlas placement | `FUN_006486B0(...)` | split/search rectangle sheet for atlas placement |
| upload | `FUN_0064BA50(...)` | process blit/upload into `GrTex2d` |
| piece registration | `FUN_006488A0(...)`, `FUN_0064AF80(...)`, `FUN_0064AFB0(...)` | allocate/reuse/register atlas-backed piece objects |
| piece finalizer | `FUN_006490F0(...)` | unlink and release registered piece wrapper |

## Layered Walkthrough

### 1. Policy and style seed

The branch begins with a compact policy layer:

- `FUN_00613F40(...)`
  - returns the active `0x10`-byte policy record
- `FUN_00612C20(...)`
  - builds or refreshes compact cached style entries
- `FUN_00653C40(...)`
  - constructs a default `HGrFont`
- `FUN_00653BA0(...)`
  - constructs an alternate/default font variant

So before any segment is measured, the producer already has:

- a policy record
- a cached style entry
- and a resolved font handle

### 2. Measure and choose the segment

The next layer is the real segmentation seam:

- `FUN_00653D40(...)`
  - translates width budget plus font/style context into a maximum fit count
- `FUN_00613750(...)`
  - says whether a boundary is a legal break point
- `FUN_00613DC0(...)`
  - combines fit count and break classification to choose the actual segment span

This is the key split that earlier notes established:

- `FUN_00613DC0(...)` does **not** create the object
- it only chooses the next emitted segment boundary

### 3. Producer engine

`FUN_00614220(...)` is still the owner of the whole production loop.

Its current strongest reading is:

- normalize layout policy
- derive working rect and line/item count
- repeatedly call:
  - `FUN_00613DC0(...)`
- place each resulting segment
- optionally emit metadata
- optionally emit a handle through:
  - `FUN_00653A60(...)`

So this is the true segmented layout-and-emission engine.

### 4. Span to subpiece expansion

Once one segment is chosen, the inner construction layer takes over:

- `FUN_006530A0(...)`
  - measures the chosen span against glyph/effect metrics
- `FUN_006541A0(...)`
  - iterates that span into renderable subpieces
- `FUN_00669800(...)`
  - computes clipped source/destination mapping
- `FUN_00669DF0(...)`
  - chooses atlas/storage dimensions

This is where the “one segment becomes one composed object” story becomes concrete.

The segment is not monolithic.
It expands into one or more subpieces whose size and clipping are planned first.

### 5. Descriptor and converter path

The piece system is descriptor-driven rather than ad hoc:

- `FUN_00653890(...)`
  - selects a cached descriptor range for one code unit
- `FUN_00654500(...)`
  - lazily loads a missing font/code-unit range when needed
- `FUN_0066B810(...)`
  - resolves the actual glyph-descriptor record
- `FUN_0066A8C0(...)`
  - resolves the converter object for a two-dword effect signature

So the piece constructor is fed by:

- code-unit descriptors
- effect signatures
- and lazily materialized font-range data

### 6. Effect lanes and packed stream

The deeper piece system is effect-lane aware:

- `FUN_0066B130(...)`
  - computes left/base advance across up to four lanes
- `FUN_0066B200(...)`
  - computes right/final width across the same lanes
- `FUN_0066B4D0(...)`
  - composites lane coverage into destination alpha data
- `FUN_0066B4A0(...)`
  - returns cheap lane-local bounds

And the lane data is driven by a packed control stream:

- `FUN_0066BBA0(...)`
  - decodes variable-length packed integers
- `FUN_0066BB10(...)`
  - refreshes lane state and run/refill tokens

So one strong current inference is:

- each segment piece can carry multiple effect lanes
- and those lanes are described by a compact packed-control format

### 7. Composed object construction

The actual segment object is built in:

- `FUN_00652500(...)`

Its current strongest reading is:

- allocate per-slice/per-piece arrays
- iterate the subpiece stream
- build converter-generated piece objects
- compute final piece widths
- assemble a composed rendered-text object

That is the concrete object immediately under:

- `FUN_00653A60(...)`

### 8. Handle extraction

`FUN_00653A60(...)` now has a very tight role:

- call:
  - `FUN_00652500(...)`
- obtain runtime context
- query:
  - `"HGrModel"`
  - tag `0x67726D64`
- release the temporary object reference
- return the resulting handle

So the product of this branch is best described as:

- an `HGrModel` handle for one composed rendered text segment

### 9. Atlas-backed piece path

Below the composition helpers, the backing graphics path is also fairly clear now:

- `FUN_006486B0(...)`
  - recursive rectangle-sheet splitter and placement finder
- `FUN_0064BA50(...)`
  - `GrTex2d` blit/upload processor
- `FUN_006488A0(...)`
  - atlas-backed piece allocation and registration path
- `FUN_0064AF80(...)`
  - piece lookup/reuse wrapper
- `FUN_0064AFB0(...)`
  - `GrTex2d`-backed fallback piece creator
- `FUN_006490F0(...)`
  - wrapper unlink / finalizer

So the segment pipeline is not only logical/layout-heavy.
It terminates in:

- actual atlas placement
- actual texture upload
- and actual wrapper lifecycle management

## Strongest Current Inferences

The strongest stable takeaways from this branch are:

- `FUN_00614220(...)` is the real segment-emission engine
- `FUN_00613DC0(...)` is the legal-break segment selector, not the constructor
- `FUN_00652500(...)` is the composed rendered-object constructor
- `FUN_00653A60(...)` is the `HGrModel` exposure wrapper
- the inner construction layer is:
  - descriptor-driven
  - effect-aware
  - packed-stream decoded
  - and atlas-backed

That is enough to treat the emitted-segment pipeline as substantially recovered.

## Fast Reference

When a future reverse pass touches this branch, the quickest current classifier is:

1. if the helper chooses text boundaries, it probably belongs to:
   - `FUN_00653D40(...)`
   - `FUN_00613750(...)`
   - `FUN_00613DC0(...)`
2. if it builds one segment object, it probably belongs to:
   - `FUN_00652500(...)`
   - or a direct inner subpiece helper beneath it
3. if it resolves glyph/effect state, it probably belongs to:
   - `FUN_00653890(...)`
   - `FUN_00654500(...)`
   - `FUN_0066B810(...)`
   - `FUN_0066A8C0(...)`
4. if it touches packed lane bytes or coverage, it probably belongs to:
   - `FUN_0066BBA0(...)`
   - `FUN_0066BB10(...)`
   - `FUN_0066B4D0(...)`
5. if it touches `GrTex2d` or rectangle sheets, it probably belongs to:
   - `FUN_006486B0(...)`
   - `FUN_0064BA50(...)`
   - `FUN_006488A0(...)`
   - `FUN_006490F0(...)`

## Best Next Step

With this appendix in place, the emitted-segment branch is organized enough to pause.

The strongest next move is to pivot back to a still-open seam rather than keep widening this one:

- exact stage/helper correspondence under:
  - `FUN_0069E870(...)`
  - `FUN_0069E1C0(...)`

That seam now looks like the biggest remaining unresolved synthesis target nearby.
