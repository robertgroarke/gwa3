## `Gw.exe` Frame Callback Callback Selection And Temp Resource Addendum

This pass followed the next helpers under the format/callback-selection layer:

- `FUN_00688890`
- `FUN_0069AB50`
- `FUN_006A1610`
- `FUN_006A16E0`
- `FUN_006902C0`

The goal was to identify:

- what concrete object `FUN_00688930(...)` is selecting
- what the remainder filler really does
- what temporary resource the two-plane path acquires
- and what object `FUN_006903C0(...)` ultimately constructs

## Source artifacts

These results come from:

- [gw_decomp_callback_resource_temp127.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_resource_temp127.log)

## High-level result

This pass makes the callback/resource side much more concrete.

The best current decomposition is:

- `FUN_00688890(...)`
  - hashed selector for one transfer-callback object keyed by a 3-field format/mode signature
- `FUN_0069AB50(...)`
  - multi-level remainder-fill dispatcher using a small vtable-driven handler family
- `FUN_006A1610(...)`
  - temporary `ImgPal`-anchored resource constructor
- `FUN_006A16E0(...)`
  - releaser for that temporary resource
- `FUN_006902C0(...)`
  - `ImgMem`-anchored processor/memory object constructor used by `FUN_006903C0(...)`

So the two-plane transfer story is now much clearer:

- callback selection is a hashed object lookup, not a switch statement
- remainder fill is also handler-object driven
- and the optional secondary resource path really does allocate a temporary image/palette-side object

## `FUN_00688890(...)`: hashed callback-object selector

This helper is the real selector behind `FUN_00688930(...)`.

### What it does

Its behavior is:

- take a 3-field signature from `param_1`:
  - field 0
  - field 1
  - field 2
- hash it as:
  - `((field1 << 3) | field0) << 3 | field2`
- mask the hash with:
  - `this + 0x1C`
- index a bucket table at:
  - `this + 0x10`
- validate against bucket count at:
  - `this + 0x18`
- walk the bucket chain
- compare candidate records on:
  - stored hash
  - field 0
  - field 1
  - field 2
- return the matching record pointer or `0`

### Best interpretation

The strongest reading is:

- `FUN_00688890(...)` selects a transfer-callback object from a hashed registry keyed by a three-part source/destination/mode signature

That means `FUN_00688930(...)` is not merely choosing function pointers.
It is choosing handler objects from a real keyed registry.

## `FUN_0069AB50(...)`: remainder-fill handler dispatcher

This helper is the real action path behind `FUN_00689D40(...)`.

### What it does

Its behavior is:

- require that the target format supports a certain capability bit:
  - checked through `FUN_00689E90(format)`
- require a valid level range:
  - `param_5 != 0`
  - `param_5 <= param_6`
- resolve a handler object from a global hashed table rooted at:
  - `DAT_00BDB6A8`
  - `DAT_00BDB6A4`
  - mask/count globals alongside it
- require that the resolved handler matches the requested format
- derive per-level width/height from:
  - `param_3`
  - shifting by `param_5 - 1`
- then, for each remaining level:
  - compute next width/height
  - choose among three vtable slots on the handler object
  - pass source/destination offsets and the previous-level pointer/state

### Best interpretation

The cleanest reading is:

- `FUN_0069AB50(...)` is a multi-level remainder-fill dispatcher over a format-specific handler object

So the "remainder fill" is not a generic memset-like cleanup.
It is a proper format-aware handler path with multiple per-level methods.

## `FUN_006A1610(...)`: temporary `ImgPal` resource constructor

This helper finally explains the temporary resource path in the two-plane worker.

### What it does

Its behavior is:

- allocate an object anchored at:
  - `P:\\Code\\Engine\\Gr\\Img\\ImgPal.cpp`
- then run a setup sequence:
  - `FUN_006A0DB0(0x100)`
  - `FUN_006A0E70()`
  - `FUN_006A0FA0(param_1)`
  - `FUN_006A1400(param_1)`
  - `FUN_006A1150(param_1, allocated_obj)`
- release temporary intermediates if they were created
- return the allocated object

### Best interpretation

The strongest safe reading is:

- `FUN_006A1610(...)` constructs a temporary image/palette-side resource object

That matches the caller perfectly:

- `FUN_00688A10(...)` only acquires it when a secondary callback plane exists and a specific source side is active

So this really does look like a staging or conversion-side image/palette resource, not a generic heap block.

## `FUN_006A16E0(...)`: temporary resource releaser

This helper is straightforward:

- require nonzero object
- free it through:
  - `FUN_0047EF60()`

### Best interpretation

- `FUN_006A16E0(...)` is the release-side companion for the temporary `ImgPal`-side object created by `FUN_006A1610(...)`

So the temp resource lifecycle is now closed:

- allocate during the two-plane path
- release at the end of `FUN_00688A10(...)`

## `FUN_006902C0(...)`: `ImgMem` processor object constructor

This helper closes the `FUN_006903C0(...)` path in a useful way.

### What it does

Its behavior is:

- require power-of-two block dimensions
- query an upper bound/count through:
  - `FUN_0068CBB0(param_3)`
- clamp requested level count
- choose a minimum control count of at least `0x0C`
- compute internal layout through:
  - `FUN_006901E0(...)`
- allocate an object anchored at:
  - `P:\\Code\\Engine\\Gr\\Img\\ImgMem.cpp`
- initialize it through:
  - `FUN_00690260(...)`
- optionally return an interior pointer via `param_5`
- return the allocated object

### Best interpretation

The cleanest reading is:

- `FUN_006902C0(...)` constructs the cached `ImgMem`-side processor/memory object that `FUN_006903C0(...)` hands back to the upload path

So `FUN_006903C0(...)` is not just a policy chooser.
It is the small front door to an actual image-memory processor object.

## Updated callback/resource model

With this pass included, the current selection and resource model is:

1. `FUN_00688930(...)`
   - decide whether one callback plane is enough or two are required
2. `FUN_00688890(...)`
   - resolve the actual callback handler object(s) by hashed 3-field signature
3. `FUN_006903C0(...)`
   - build the cached `ImgMem` processor object through `FUN_006902C0(...)`
4. `FUN_00688A10(...)`
   - run the tiled transfer through one or two selected callback planes
5. optional two-plane resource:
   - acquire through `FUN_006A1610(...)`
   - release through `FUN_006A16E0(...)`
6. `FUN_00689D40(...)` -> `FUN_0069AB50(...)`
   - fill remaining regions through format-specific handler objects

That means the `GrTex2d` upload path is now well past “mysterious helper soup.”
It is a real:

- format-table
- handler-registry
- processor-object
- optional temp-resource
- staged transfer

pipeline.

## Best next step

The strongest next targets are:

- `FUN_0068CBB0`
- `FUN_006901E0`
- `FUN_00690260`
- `FUN_006A0DB0`
- `FUN_006A0E70`
- `FUN_006A0FA0`
- `FUN_006A1400`
- `FUN_006A1150`

That should let us name:

- the internal sizing/layout policy for the cached `ImgMem` processor
- and the exact role of the temporary `ImgPal` resource in the two-plane path
