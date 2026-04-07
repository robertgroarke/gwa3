## `Gw.exe` Frame Callback Backend Producer Engine Addendum

This pass followed the next isolated backend seam:

- `FUN_00614220`
- `FUN_006163E0`
- `FUN_00613D10`
- `FUN_005F8780`

The goal was to determine whether `FUN_00614220(...)` is the actual producer engine and, if so, what it produces.

## Source artifacts

These results come from:

- [gw_decomp_backend_producer_temp113.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_backend_producer_temp113.log)
- [gw_findcallers_00614220_temp114.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00614220_temp114.log)

## High-level result

Yes: `FUN_00614220(...)` is the real deeper backend producer layer we were looking for.

But it is broader than just “text rows.”

It is best described as a generic layout-and-emission engine that:

- takes UTF-16 style input text plus style/flag inputs
- computes line/item layout
- optionally writes per-item geometry metadata into one output structure
- optionally writes emitted object handles into a second output vector
- supports multiple wrapper/front-door callers

The caller map confirms it is shared:

- `FUN_00610EE0`
- `FUN_0060CE70`
- `FUN_00610F60`
- `FUN_0060CF60`

So this is a backend engine seam, not a one-off helper for a single control.

## `FUN_00614220(...)`: generic layout-and-emission engine

### Signature shape

The current decompile shape is:

```cpp
void FUN_00614220(
    short *text,
    uint owner_or_context,
    undefined4 *rect_in,
    uint layout_flags,
    float item_extent,
    uint style_flags,
    float scale_or_spacing,
    byte *out_policy,
    int *out_handles)
```

That naming is still provisional, but the behavior is already strong enough to describe the roles.

### Early setup

The function:

- requires non-null `text`
- requires that at least one output channel exists:
  - `param_8`
  - or `param_9`
- falls back to default output policy block:
  - `DAT_00BD0154`
- seeds defaults from:
  - `FUN_00613F40()`
  - `FUN_00612C20()`

That means this routine owns real layout policy state, not just a pass-through into another formatter.

### Flag normalization

One especially useful block is the flag cleanup:

- it merges bits from:
  - caller `param_6`
  - default policy object fields
- it counts active bits across a restricted mask
- if too many mutually exclusive bits are present, it clears/normalizes them

So this engine is enforcing a constrained layout/alignment policy internally.

### Core emission loop

The middle of the function is the strongest evidence.

It:

- computes available width/height from `rect_in`
- computes line/item counts based on:
  - input extent
  - spacing
  - available rect
- allocates a temporary stack-local array of per-item records
- repeatedly calls:
  - `FUN_00613DC0()`

That `FUN_00613DC0()` call is the local worker that appears to populate each temporary item record.

Each emitted temporary record then feeds:

- anchor/placement math
- rect clamping
- optional metadata output
- optional handle output

So `FUN_00614220(...)` is very clearly the layout producer, while `FUN_00613DC0()` looks like the per-item fill helper beneath it.

## Output channel 1: geometry/metadata table through `param_8`

`param_8` is not just a raw flag byte.
It is a small policy/output structure with flag bits and optional output pointers.

Observed behavior:

- bit `8`
  - enables an extra offset/adjustment source from `param_8 + 0x10`
- bit `4`
  - enables a metadata/record table at `*(param_8 + 0x0C)`
- bit `2`
  - enables a scalar center/offset output at `*(param_8 + 0x08)`
- bit `1`
  - enables a bounding-rect output at `*(param_8 + 0x04)`

When bit `4` is enabled, the function appends a `0x1C`-byte per-item record containing seven floats:

- source width-ish field
- source height-ish field
- source offset-ish field
- placed left
- placed top
- placed right
- placed bottom

That is the strongest concrete producer result in this pass.

So `FUN_00614220(...)` really is generating a structured geometry table when asked.

## Output channel 2: emitted handle vector through `param_9`

The second output path is separate and equally important.

When:

- `param_9 != 0`
- and the computed line/item extent is non-zero

the function:

- calls `FUN_00653A60()`
- grows a vector stored in `param_9`
- appends the returned handle/id into that vector

So the engine has two distinct products:

- geometry metadata records via `param_8`
- emitted object handles/ids via `param_9`

That finally explains the split we saw above:

- `FUN_00610F60(...)` only cared about final extents
- `FUN_0060CE70(...)` cared about the newly emitted handle list and passed it to `FUN_006163E0(...)`

## `FUN_006163E0(...)`: post-generation consumer for emitted handles

This function is the immediate downstream consumer after `FUN_0060CE70(...)`.

Its shape is:

- validate count and base pointer
- validate a small type/index value through bitmask `DAT_00BD01C4`
- grow a per-type bucketed vector rooted at fields:
  - `this + 0x1C`
  - `this + 0x20`
  - `this + 0x24`
  - `this + 0x28`
- copy the new emitted handles into that bucket
  - clone path when `param_4 == 0`
  - raw copy path when `param_4 != 0`

Then it performs global post-processing:

- if owner flag `0x400` is clear:
  - sample alpha/intensity through `FUN_00617150()`
  - push that visual state to each new handle through `FUN_006446B0(...)`
- call `FUN_00615990()`
- for bucket `7`, also walk a secondary list at:
  - `+0x70`
  - `+0x78`
  - and call `FUN_00645410(handle, 0)`

So `FUN_006163E0(...)` is not the producer.
It is the per-type registration and post-processing consumer for newly emitted handles.

## `FUN_00613D10(...)`: text-to-frame-object loader

This helper turned out to be cleaner than expected.

It:

- requires a text buffer pointer
- references:
  - `P:\\Code\\Engine\\Frame\\FrText.cpp`
- creates or resolves a frame-text object through:
  - `FUN_00613430(...)`
- updates/reset state through:
  - `FUN_006139C0(...)`
- inserts the object into the relation/index layer through:
  - `FUN_00473D80(...)`
- iterates the source UTF-16 text through:
  - `FUN_007A1C20(param_2, FUN_00614B20, puVar1)`

So this is the concrete text-buffer loader used by the `CtlTextBtn` family, not part of the generic handle-vector producer itself.

## `FUN_005F8780(...)`: post-generation property fanout

This helper is small but now makes more sense in context.

It:

- walks the attached child pointer array at:
  - `param_1 + 0x24`
  - count at `param_1 + 0x2C`
- calls:
  - `FUN_00644780(child, param_1 + 0x34)`

So this is a property/visual fanout over already attached child objects.

That matches exactly how `FUN_005F8810(...)` used it after `FUN_0060CE70(...)`:

- generate or refresh children
- then propagate shared state to each one

## What this pass changes

Before this pass, `FUN_00614220(...)` was only the best guess for the real producer.

After this pass, the picture is much firmer:

- `FUN_00614220(...)` is the real backend layout-and-emission engine
- `FUN_00613DC0(...)` is likely its per-item temporary-record worker
- `FUN_006163E0(...)` is the immediate per-type consumer of emitted handles
- `FUN_00610F60(...)` and `FUN_0060CE70(...)` are just two different front doors onto that backend

So the producer seam is finally real, not just inferred.

## Updated working model

The cleanest current stack is now:

- concrete controls
  - `CtlTextMl`
  - `CtlTextBtn`
- control-local wrappers
  - `FUN_005ECEB0(...)`
  - `FUN_005F8810(...)`
- shared front doors
  - `FUN_00610F60(...)`
  - `FUN_0060CE70(...)`
  - `FUN_00610EE0(...)`
  - `FUN_0060CF60(...)`
- real backend producer
  - `FUN_00614220(...)`
- per-item worker below it
  - `FUN_00613DC0(...)`
- post-generation handle consumer
  - `FUN_006163E0(...)`

That is the strongest backend decomposition we have had so far in this branch.

## Best next step

The next best step is to decompile:

- `FUN_00613DC0`
- `FUN_00612C20`
- `FUN_00613F40`
- `FUN_00653A60`

Why these are now the right targets:

- `FUN_00613DC0(...)` likely fills the per-item temporary records before they are placed and emitted
- `FUN_00612C20(...)` and `FUN_00613F40(...)` seed the default layout policy that `FUN_00614220(...)` depends on
- `FUN_00653A60(...)` returns the emitted handle/id that gets appended into the post-generation vector

That should let the next pass answer the remaining high-value question:

- exactly what is each generated item before it becomes a final emitted handle and geometry record?
