## `Gw.exe` Frame Callback Owning Frame Layout Addendum

This pass continues below the handler-executor layer by tracing the object recovered through:

- `FUN_0062DAE0() -> *this - 0x128`

and the most relevant nearby helpers using that recovered base:

- `FUN_00629A40`
- `FUN_0062A220`
- `FUN_006243E0`
- `FUN_00627D70`
- `FUN_00628570`

The goal was to stop treating the recovered owner as a generic “frame-like” blob and recover enough field structure to describe it concretely.

## Decompiled helper bundle

These bodies come from:

- [gw_decomp_owner_layout_temp67.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_owner_layout_temp67.log)
- [gw_findcallers_0062dae0_temp66.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0062dae0_temp66.log)
- [gw_findcallers_00628740_temp66.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00628740_temp66.log)

## High-level result

This pass gives the recovered owner a much more concrete shape.

The strongest points are:

- the `-0x128` base really does behave like a frame/container object
- it has a runtime id at `+0xBC`
- a flag word at `+0xC0`
- and a bounds rectangle at:
  - `+0xEC`
  - `+0xF0`
  - `+0xF4`
  - `+0xF8`

It also shows that the listener tables under discussion are not isolated one-off tables. They are part of a richer owner object that:

- exposes current bounds
- stores alignment/placement flags
- caches or tracks an active frame-like object globally
- and supports a higher-level emit/register helper above the raw dispatchers

So the previous “frame-like base” label is now much better grounded.

## `FUN_0062A220(out_rect)`: owner-relative bounds computation

This is the clearest field-layout win in the batch.

The function first calls `FUN_0062DAE0()` to recover the owner base.

If that fails, it returns a default rectangle:

- left = `0.0`
- top = `0.0`
- right = `_DAT_00bd0d7c`
- bottom = `_DAT_00bd0d80`

If the owner exists, it validates:

- `owner + 0xEC <= owner + 0xF4`
- `owner + 0xF0 <= owner + 0xF8`

and then uses those values as the owner’s active bounds rectangle.

That is already enough to assign a strong meaning to those fields:

- `+0xEC` = min-x / left
- `+0xF0` = min-y / top
- `+0xF4` = max-x / right
- `+0xF8` = max-y / bottom

The rest of the function then adjusts those bounds using a secondary object in `in_ECX`, with:

- flags at `+0x08`
- positions at `+0x0C` / `+0x10`
- extents at `+0x14` / `+0x18`

and bit tests for:

- `0x02`
- `0x04`
- `0x08`
- `0x10`

The arithmetic clearly implements anchored placement:

- left-aligned / right-aligned
- top-aligned / bottom-aligned
- or centered fallback behavior

So `FUN_0062A220()` is not just “get some bounds.” It is:

- take the recovered owner rectangle
- apply anchor/alignment rules from a child/control object
- return the effective placed rectangle

That is very strong evidence that the owning base is a real UI/frame container.

## `FUN_00629A40()`: owner-flag driven follow-up behavior

This helper is small but useful because it shows more meaningful owner fields.

After recovering the owner base with `FUN_0062DAE0()`, it checks:

- `owner + 0xC0` bit `0x200`
- `owner + 0xC0` bit `0x100`

and conditionally calls:

- `FUN_0062A980()`
- `FUN_0062ABB0(1)`

So the owner has at least one flag word at `+0xC0` controlling follow-up layout/commit behavior.

That is exactly the kind of field you would expect on a frame/container object:

- visible/dirty/layout-needed style flags
- or capability/behavior bits

Even without fully naming those bits yet, the field role is clear.

## `FUN_006243E0()`: global active-owner cache maintenance

This helper ties the owner object into a global active/cached pointer:

- `DAT_00bd0c38`

It compares:

- `in_ECX - 0x94`

against that global, and if they match, it refreshes the global via `FUN_0062DAE0()`.

Then it loops until either:

- the recovered owner is `0`
- or bit `8` at `owner + 400` is set

That gives us two more useful structural clues:

- `in_ECX - 0x94` is another path back toward the same owner family
- the owner has a meaningful status byte/flag field at `+400` (`0x190`)

The simplest safe interpretation is:

- there is a globally tracked “current” owner/frame/container
- and `FUN_006243E0()` refreshes that cache until it lands on one with an eligible status bit

That fits very naturally with a UI-frame ownership model.

## `FUN_00627D70(callback, payload)`: larger owner-scoped emit path

This helper is a bigger wrapper around the same owner family.

The most important pieces are:

- it calls `FUN_0062DAE0()`
- if the owner exists, it copies `owner + 0xBC` into `local_4C`
- it stores `in_ECX + 0x3A` as another context pointer
- it eventually walks a local `0x0C` record table
- and it calls callback records directly:
  - `(*(code *)*record)(&local_7c, &local_24, 0)`

This is important for two reasons.

First:

- it confirms `owner + 0xBC` behaves like a runtime id or frame id

Second:

- it shows the record-executor pattern is not limited to the exact `0x31` / `0x32` paths already discussed
- there is a broader owner-scoped callback/emit surface around the same table format

So the listener-record ABI we recovered in `FUN_00628740()` is part of a larger family, not a one-off local trick.

## `FUN_00628570(msg, payload)`: higher-level registration/emit wrapper

This was the one extra direct caller of `FUN_00628740()` besides the two dispatchers we already knew about.

Its structure shows a more managed path above raw dispatch:

- it manipulates linked-list style bookkeeping through `in_ECX + 6` / `in_ECX + 7`
- it rejects message ids `9` and `0x0B` just like `FUN_006286D0()`
- it extracts the current message id from `local_c[1]`
- it walks a `0x0C` callback table
- and it dispatches each record through:
  - `FUN_00628740(record, msg_id, payload, arg)`

That means the record executor is part of a small layered callback system:

- low-level per-record invoke
- basic raw-table dispatcher
- gated two-phase dispatcher
- and at least one higher-level wrapper with linked-list/registration state

So the owner-side event system is broader than the one coordinate-submission branch that first led us here.

## What the recovered owner now looks like

The strongest current field sketch for the object returned by `FUN_0062DAE0()` is:

- `+0xBC` = runtime id / frame-like id
- `+0xC0` = behavior/status flags
- `+0xEC` = left / min-x
- `+0xF0` = top / min-y
- `+0xF4` = right / max-x
- `+0xF8` = bottom / max-y
- `+0x190` = status byte/flags containing at least bit `8`
- `+0xA8` / `+0xB0` = secondary listener table pointer/count pair from the previous pass

That is now enough to describe it as a concrete owner frame/container object rather than a vague UI-adjacent base.

## What this changes about the overall model

Before this pass, the safe description was:

- `FUN_0062DAE0()` recovers some frame-like parent
- that parent owns the `0x31` / `0x32` listener tables

After this pass, the stronger description is:

- `FUN_0062DAE0()` recovers a concrete container/frame object
- that object has:
  - bounds
  - placement/alignment interaction with child/control objects
  - runtime id
  - flag words
  - cached/current-owner participation
  - multiple callback/emit entrypoints

That means the `0x31` / `0x32` second-stage dispatches are almost certainly not generic gameplay events. They belong to a real frame/container subsystem in the UI/visual maintenance layer.

## Best next step

The next best reverse step is to name the remaining owner-local emit helpers and the table manager around them.

The highest-value nearby targets are:

- `FUN_0062A980`
- `FUN_0062ABB0`
- `FUN_00625D90`
- `FUN_00624390`
- `FUN_0062CD10`
- `FUN_0062E530`

That should tell us:

- what the `+0xC0` flag bits at `0x100` and `0x200` actually trigger
- whether the owner object is best described as a viewport frame, a layout frame, or a more specialized UI container
- and how broad the owner-local callback surface really is
