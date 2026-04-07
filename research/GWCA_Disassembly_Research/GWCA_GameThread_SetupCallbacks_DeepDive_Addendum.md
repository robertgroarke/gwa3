## `Gw.exe` Frame Callback Setup Callbacks Deep-Dive Addendum

This pass follows the next concrete producer candidates from the setup-flow builders:

- `FUN_005EA9A0`
- `FUN_005F3550`

The goal was to determine whether these callbacks:

- directly allocate/fill the region child-record table
- or instead implement the concrete child controls that sit above that table

## Source artifacts

These results come from:

- [gw_decomp_setup_callbacks_temp107.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_setup_callbacks_temp107.log)
- [gw_decomp_region_builders_temp106.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_builders_temp106.log)

## High-level result

This pass answers the callback question pretty cleanly:

- `FUN_005EA9A0(...)` is a full concrete control implementation, not a raw region-record builder
- `FUN_005F3550(...)` is another concrete child-control implementation, specifically image-oriented

So these callbacks sit one layer above the missing raw record-table writer.
They clearly participate in building and driving the control tree, but they are not the low-level code that writes child-record `+0x24/+0x28` directly.

That is still useful because it places the missing builder below or beside these concrete control implementations, not above them.

## `FUN_005EA9A0(...)`: concrete text/list-style control implementation

This function is much richer than a simple adapter.

### Constructor case `9`

Case `9` allocates a real object from:

- `P:\\Code\\Engine\\Controls\\CtlTextMl.cpp`

and seeds a large instance body with:

- color-like defaults
- size/count defaults
- selection/index defaults
- owner id
- layout/visual update messages like:
  - `FUN_00610120(*param_1, 0x4C)`
  - `FUN_0060DFC0(*param_1, 0, 0x30)`

That is a concrete multiline-text / list-style control body, not a low-level record builder.

### Cases `0x57`, `0x58`, `0x59`, `0x5A`

These cases are especially informative because they expose the control’s own data surface:

- `0x57`
  - routes into `FUN_005ED1E0(..., mode=0)`
- `0x58`
  - routes into `FUN_005ED1E0(..., mode=1)`
- `0x59`
  - returns UTF-16 style text content from the current selection/range
- `0x5A`
  - returns a bounded count/selection-sized result block

So this control clearly owns:

- indexed text/list content
- selection/count state
- query/update verbs

That makes it very unlikely to be the hidden record-table allocator itself.

### Cases `0x5B` .. `0x60`

These cases update object fields at:

- `+0x20`
- `+0x24`
- `+0x28`
- `+0x2C`
- `+0x30`

and then trigger:

- `FUN_00610540(owner)`
- `FUN_00610330(owner)`
- `FUN_0060D080(owner)`

This is a strong pattern for:

- update concrete control properties
- then invalidate/refresh the owner

Again, that is consumer/control logic, not raw record-table allocation.

### Cases `0x37` and `0x38`

These cases call:

- `FUN_005EB4E0(...)`

with different worker callbacks:

- `FUN_005EC600`
- `FUN_005ECEB0`
- `FUN_005ECC80`
- `FUN_005ECDC0`

This is the most promising finding in the pass.

It suggests the actual list/record iteration and maybe the deeper record allocation/fill logic lives in:

- `FUN_005EB4E0(...)`
- or the callbacks it receives

So the search target just got sharper again.

## `FUN_005F3550(...)`: concrete image / coordinate child control

This callback is also a real control implementation.

### Constructor case `9`

It allocates from:

- `P:\\Code\\Engine\\Controls\\CtlImg.cpp`

and seeds an object with image/resource fields and geometry bounds.

So this is clearly:

- concrete image child control

not a raw generic region-record builder.

### Cases `0x56`, `0x57`, `0x58`, `0x59`

These cases expose an axis/coordinate-style control surface:

- `0x56`
  - maps normalized inputs into absolute coordinates
- `0x57`
  - maps absolute coordinates back into normalized values
- `0x58`
  - routes into `FUN_005F3E70(...)`
- `0x59`
  - resizes internal storage and updates a count/capacity-like field

That fits nicely with the broader host/region-control stack we’ve been recovering, but again it is a concrete control implementation, not the hidden region-record writer.

### Cases `0x24`, `0x25`, `0x26`

These cases emit owner-local channels:

- `8`
- `7`
- `9`

So this child control also participates directly in the interaction protocol we mapped earlier.

That is another good confirmation that the control tree and the interaction system are tightly integrated.

## What this pass changes

Before this pass, the two callbacks from the setup builders were only "good candidates."

After this pass, the cleaner picture is:

- they are real control implementations
- they expose concrete message/control surfaces
- they likely consume or iterate lower-level data structures
- but they are not themselves the raw child-record metadata writers we are trying to find

So the producer search should now move one layer lower again, into their internal worker helpers.

## Best new target from this pass

The strongest concrete target that emerged is:

- `FUN_005EB4E0(...)`

Why it matters:

- `FUN_005EA9A0(...)` uses it in both measurement/update style paths
- it takes iterator/count-like parameters plus callback arguments
- it sits exactly where a lower-level row/record traversal or materialization helper would be expected

The companion callbacks it receives are also now high-value:

- `FUN_005EC600`
- `FUN_005ECEB0`
- `FUN_005ECC80`
- `FUN_005ECDC0`

## Updated working model

The cleanest current model is now:

- setup builders like `FUN_0052D970(...)`
  - create owner children
- child callbacks like `FUN_005EA9A0(...)` and `FUN_005F3550(...)`
  - implement concrete text/list and image/coordinate controls
  - expose the message surfaces the rest of the UI uses
- lower worker helpers
  - likely where record iteration/materialization actually happens

So the record-builder search is now below the concrete control implementations, not inside them.

## Best next step

The next best step is to decompile:

- `FUN_005EB4E0`
- `FUN_005EC600`
- `FUN_005ECEB0`
- `FUN_005ECC80`
- `FUN_005ECDC0`

That is now the likeliest route to the data/row/record layer that the text/list-style control is consuming, and potentially to the code that finally explains how the child-region records are assembled.

