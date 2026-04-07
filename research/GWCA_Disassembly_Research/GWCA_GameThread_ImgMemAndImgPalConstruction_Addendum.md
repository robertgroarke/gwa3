## `Gw.exe` Frame Callback ImgMem And ImgPal Construction Addendum

This pass followed the next construction helpers beneath the cached `ImgMem` processor and temporary `ImgPal` resource paths:

- `FUN_0068CBB0`
- `FUN_006901E0`
- `FUN_00690260`
- `FUN_006A0DB0`
- `FUN_006A0E70`
- `FUN_006A0FA0`
- `FUN_006A1400`
- `FUN_006A1150`

The goal was to identify:

- how the cached `ImgMem` processor chooses its internal sizing/layout
- and what the temporary `ImgPal` resource is actually preparing for the two-plane transfer path

## Source artifacts

These results come from:

- [gw_decomp_imgmem_imgpal_temp128.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgmem_imgpal_temp128.log)

## High-level result

This pass turns both object families into something much more concrete.

The best current decomposition is:

- `FUN_0068CBB0(...)`
  - level-count / depth helper derived from the larger of two power-of-two dimensions
- `FUN_006901E0(...)`
  - `ImgMem` layout planner that sizes the pointer table, optional scratch block, and packed payload region
- `FUN_00690260(...)`
  - `ImgMem` pointer-table initializer over the packed payload region
- `FUN_006A0DB0(...)`
  - allocation/init for a large pair-list storage arena
- `FUN_006A0E70(...)`
  - allocation/init for a full `0x10000`-entry score table
- `FUN_006A0FA0(...)`
  - palette-cell to candidate-list builder over the 256-entry RGBA palette
- `FUN_006A1400(...)`
  - fallback/best-match builder for empty palette-cell buckets
- `FUN_006A1150(...)`
  - compact palette-lookup table generator

So the temporary `ImgPal` object now looks much less like a vague staging buffer and much more like a palette-distance / nearest-match precomputation engine.

## `FUN_0068CBB0(...)`: level-count helper

This helper is small but it clarifies the `ImgMem` sizing path.

### What it does

Its behavior is:

- require nonzero width and height in `param_1[0..1]`
- take the larger of width and height
- compute a log-like value through:
  - `FUN_0046DCF0()`
- if the larger dimension is not a pure power of two and still has the top bit set:
  - return `log + 2`
- otherwise:
  - return `log + 1`

### Best interpretation

The cleanest reading is:

- `FUN_0068CBB0(...)` computes the number of downscaled levels or hierarchy depth slots needed for the larger source dimension

That fits `FUN_006902C0(...)`, which used it to clamp the count of internal blocks/levels it was about to allocate.

## `FUN_006901E0(...)`: `ImgMem` layout planner

This helper is the internal sizing policy under `FUN_006902C0(...)`.

### What it does

Its behavior is:

- require:
  - `param_5 > param_4`
- compute a pointer-table size:
  - `*param_7 = param_5 * 4`
- if the caller requested an extra block:
  - set:
    - `*param_8 = 0x400`
- otherwise:
  - set:
    - `*param_8 = 0`
- set a running base size:
  - pointer table
  - plus optional `0x400` scratch/aux block
- for each active level:
  - call:
    - `FUN_0068CC20(param_1, param_2, param_3, level)`
  - add that level payload size rounded to 4-byte alignment
- finally write:
  - `*param_9 = payload_bytes_only`

### Best interpretation

The strongest reading is:

- `FUN_006901E0(...)` computes the internal memory layout of the cached `ImgMem` processor object as:
  - pointer table
  - optional `0x400` auxiliary block
  - followed by aligned per-level payload regions

That makes `FUN_006902C0(...)` much less opaque.

## `FUN_00690260(...)`: `ImgMem` pointer-table initializer

This helper builds the actual interior pointer table that `FUN_006902C0(...)` relies on.

### What it does

Its behavior is:

- walk active levels from highest to lowest
- for each level:
  - store a pointer at:
    - `param_1 + level * 4`
  - that pointer targets the current payload cursor:
    - `param_1 + param_7`
  - advance the payload cursor by:
    - aligned `FUN_0068CC20(...)` size
- if reserved table entries exceed active levels:
  - zero the extra pointer-table slots

### Best interpretation

The cleanest reading is:

- `FUN_00690260(...)` initializes the `ImgMem` processor as a pointer-table header followed by compact per-level payload regions

So the `ImgMem` object is not a monolithic opaque blob.
It is a structured layout with direct per-level payload pointers.

## `FUN_006A0DB0(...)`: large pair-list arena initializer

This helper is the first half of the temporary `ImgPal` resource setup.

### What it does

Its behavior is:

- compute a capacity:
  - `param_1 * 8 + 0xD2F`
- allocate backing storage through:
  - `FUN_004735B0(0, capacity * 8)`
- reset current count/state
- install the new capacity and pointer
- record an additional zeroed field at:
  - `this + 0x0C`

### Best interpretation

The strongest reading is:

- `FUN_006A0DB0(...)` allocates a large pair-record arena, very likely for linked candidate entries of the form:
  - pointer
  - score or next-link

That matches the later palette-building helpers, which keep writing 8-byte records.

## `FUN_006A0E70(...)`: full 16-bit score-table initializer

This helper is the second major `ImgPal` constructor piece.

### What it does

Its behavior is:

- allocate storage for:
  - `0x10000` 32-bit entries
- initialize a symmetric table over two 8-bit coordinates
- each entry stores:
  - square of the difference between the two byte values

### Best interpretation

The cleanest reading is:

- `FUN_006A0E70(...)` builds a full 256x256 squared-difference lookup table

That is a strong signal that the temporary `ImgPal` object is preparing for color-distance computation.

## `FUN_006A0FA0(...)`: palette candidate-list builder

This helper starts consuming the actual 256-entry palette passed in `param_1`.

### What it does

Its behavior is:

- iterate 256 RGBA entries (`param_1` to `param_1 + 0x400`)
- keep only entries with alpha/high byte > `0x7F`
- quantize RGB high nibbles
- expand each quantized component into a small local range
- for every cell in that local 3D neighborhood:
  - append an 8-byte record into the arena
  - store:
    - pointer to the source palette entry
    - previous bucket head / link
  - update the bucket head to the new record

### Best interpretation

The strongest reading is:

- `FUN_006A0FA0(...)` builds bucketed candidate lists from palette entries into a quantized RGB cube

That means the temporary `ImgPal` object is not simply copying palette bytes.
It is building a fast approximate nearest-color search structure.

## `FUN_006A1400(...)`: empty-bucket fallback builder

This helper fills in the buckets that did not get direct candidate entries.

### What it does

Its behavior is:

- walk every quantized RGB cell in the 3D bucket grid
- if a bucket is empty:
  - derive three weighted channel tables from the precomputed squared-difference table
  - scan all palette entries with alpha > `0x7F`
  - compute a total distance score
  - choose the best-matching palette entry
  - append a new 8-byte record for that bucket
  - set the bucket head to the new record

### Best interpretation

The cleanest reading is:

- `FUN_006A1400(...)` ensures every quantized RGB bucket has at least one nearest-color candidate, even when the palette did not naturally populate that bucket

That makes the palette structure complete and lookup-safe.

## `FUN_006A1150(...)`: compact lookup-table generator

This helper appears to build the final fast lookup structure using the candidate buckets.

### What it does

Its behavior is:

- iterate a coarse 3D grid using two static byte tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`
- for each coarse cell:
  - derive weighted channel tables from the squared-difference table
  - resolve the candidate-list head for the corresponding quantized RGB bucket
  - scan the linked candidate list
  - choose the best palette entry by total weighted distance
  - write a one-byte palette index into the output table

### Best interpretation

The strongest reading is:

- `FUN_006A1150(...)` builds a compact RGB-to-palette lookup table from the candidate buckets and weighted distance tables

So the temporary `ImgPal` object is now pretty clearly:

- a palette acceleration structure
- plus a compact direct lookup table

rather than a generic temporary image object.

## Updated object model

With this pass included, the current object picture is:

### `ImgMem`

- `FUN_0068CBB0(...)`
  - decide active level count/depth
- `FUN_006901E0(...)`
  - size pointer table, optional aux block, and payload bytes
- `FUN_00690260(...)`
  - lay out the per-level payload pointers
- `FUN_006902C0(...)`
  - allocate the final `ImgMem` object and initialize it

### `ImgPal`

- `FUN_006A0DB0(...)`
  - allocate candidate-record arena
- `FUN_006A0E70(...)`
  - build squared-difference lookup table
- `FUN_006A0FA0(...)`
  - build bucketed palette candidate lists
- `FUN_006A1400(...)`
  - fill empty buckets with nearest matches
- `FUN_006A1150(...)`
  - generate the compact final lookup table

So the optional two-plane path is now much less mysterious:

- `ImgMem` provides structured per-level transfer/storage state
- `ImgPal` provides accelerated palette conversion support

## Best next step

The strongest next targets are:

- `FUN_0068CC20`
- `FUN_00688890` callers or adjacent data objects
- `FUN_006902C0` caller-adjacent object fields
- `FUN_006A1610` adjacent helper data / object layout
- and, if useful, the static tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`

That should let us:

- name what each per-level `ImgMem` payload actually stores
- and tighten the palette-conversion interpretation from “accelerated nearest-color lookup” to the exact conversion role used by the two-plane transfer
