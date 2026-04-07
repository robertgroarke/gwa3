## `Gw.exe` Frame Callback Default Sources And Init Addendum

This pass continues from the measure-and-relation-index note by decompiling the remaining default-size and immediate-constructor helpers:

- `FUN_00629E10`
- `FUN_00619480`
- `FUN_00622760`
- `FUN_006240D0`
- `FUN_00613400`

The goal was to close two practical gaps:

- where the measurement pipeline gets its fallback/default sizes
- what a few of the immediate constructor-side init helpers are actually seeding

## Source artifacts

These results come from:

- [gw_decomp_default_init_temp88.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_default_init_temp88.log)
- [gw_decomp_measure_hash_temp87.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_measure_hash_temp87.log)
- [gw_decomp_metric_builder_temp85.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_metric_builder_temp85.log)

## High-level result

This pass makes the default-size path much less vague.

The strongest current measurement model is now:

1. local object-level default size from short fields at `+0x50/+0x52`
2. fallback to an object/resource descriptor through `FUN_00619480(...)`
3. if size is still incomplete, aggregate child/subcomponent contributions through `FUN_00622760(...)`
4. finally combine with global insets from message `0x15` and preferred-size negotiation from message `0x38`

On the constructor side, the most useful new point is:

- `FUN_00613400(parent)` copies `parent + 0x194` into the new object's current field

which gives one more real parent-to-child inheritance seam in the control constructor.

## `FUN_00629E10(out_size)`: object-local default size from `+0x50/+0x52`

This helper is the most direct default-size source in the batch.

It does:

- if both `*(short *)(this + 0x50)` and `*(short *)(this + 0x52)` are non-zero:
  - return them directly
- otherwise call:
  - `FUN_006194F0(&local_c)`
- and substitute any missing dimension from that fallback result

So the current object has built-in short-sized default width/height fields at:

- `+0x50`
- `+0x52`

That is a strong upgrade from the earlier generic “local default” wording.

This means at least some control nodes carry explicit intrinsic default dimensions directly in the object.

## `FUN_00619480(out_size)`: descriptor/resource size fallback

This helper is a cleaner second-stage fallback:

- if `*(this + 4) == -1`, return `(0, 0)`
- otherwise resolve a descriptor through:
  - `FUN_00619410(*(this + 4), local_8)`
- then return:
  - `*(ushort *)(desc + 4)`
  - `*(ushort *)(desc + 6)`

So this fallback is not computing size from live children.
It is reading width/height from an object/resource descriptor record.

That gives the size pipeline a nice layered shape:

- intrinsic object defaults
- descriptor defaults
- aggregated child contribution

## `FUN_00622760(out_size, param_2)`: aggregate child/subcomponent measurement

This helper is the most complex fallback source in the batch and probably the most informative.

At a high level it:

- checks `this[2]` child count
- if zero, returns `(0, 0)`
- prepares a local temporary block through:
  - `FUN_00620E90(param_2)`
- iterates children
- for each child, calls a virtual slot:
  - `(**(child_vtable + 8))(local_d0, this - 0x21, 0)`
- then performs several aggregate passes through:
  - `FUN_00621480(...)`
    - with `FUN_006225F0`
    - and `FUN_00622550`
- then iterates children again and calls another virtual slot:
  - `(**(child_vtable + 4))(local_d0, &local_d8)`
- finally returns:
  - `out_w = local_d8`
  - `out_h = local_d4`

The important practical result is:

- this is a child/subcomponent aggregate measurement pass

not:

- a simple one-child lookup

and not:

- another resource-descriptor fallback

So the measurement pipeline now clearly has a real composition phase when earlier defaults are insufficient.

## `FUN_006240D0()`: small constructor-side subobject init

This helper is a compact initializer for one embedded subobject.

It:

- zeroes a field
- sets:
  - `in_ECX[1] = 0x11`
- initializes a self-linked intrusive node
- sets:
  - `in_ECX[2] = 8`
- flips a global bit:
  - `DAT_00BD0C3C |= 1`

So this helper is not a high-level feature by itself.
It is a small embedded constructor for one internal control/relation subobject with:

- type-like marker `0x11`
- list membership scaffolding
- a global "initialized/used" bit

That fits the constructor stack we’ve been recovering: `FUN_0060D300(...)` is assembling multiple embedded pieces, not only one flat struct.

## `FUN_00613400(parent)`: inherit parent field at `+0x194`

This helper is tiny but useful:

- if `parent != 0`
  - `*this = *(parent + 0x194)`
- else
  - `*this = 0`

So one of the immediate post-construction init steps is explicit parent-to-child inheritance from:

- parent offset `+0x194`

This is stronger than just saying “construction links to parent.”
It means a specific parent control state/value at `+0x194` is copied into the new object during construction.

We still do not know the semantic name of that field, but it is now clearly inherited state.

## Updated default-size pipeline

After this pass, the size fallback chain is much cleaner:

1. `FUN_00629E10(...)`
   - use object short defaults at `+0x50/+0x52`
   - else fill missing values from `FUN_006194F0(...)`
2. `FUN_00619480(...)`
   - use descriptor/resource width/height from `desc + 4/+6`
3. `FUN_00622760(...)`
   - aggregate child/subcomponent contributions through virtual calls and combine passes
4. `FUN_00629E80(...)`
   - combine that measured content size with global insets and preferred-size negotiation

That is now a fairly concrete multi-layer UI measurement system.

## What this changes about the constructor story

The constructor stack under `FUN_0060D300(...)` now has a little more texture:

- `FUN_006240D0()` initializes one embedded subobject/list head
- `FUN_00613400(parent)` copies inherited parent state from `+0x194`

So the constructor is not only assigning:

- runtime id
- relation metadata
- callback/handler state

It is also seeding:

- embedded subobject scaffolding
- inherited parent control state

That keeps reinforcing the same conclusion: this is a real control/layout framework, not just a detour-friendly message bus.

## Strongest current model after this pass

The cleanest architecture picture now is:

- owner/control nodes carry intrinsic default dimensions
- can also consult descriptor defaults
- can also aggregate child contributions
- container-level messages then add margins and preferred-size policy
- constructor-time initialization pulls in both embedded subobjects and parent-inherited state

That is enough to describe the measurement and init sides as real subsystems instead of scattered helper calls.

## Best next step

The highest-yield next pass is to go after the remaining combine/aggregation helpers and the parent-inherited field:

- `FUN_006194F0`
- `FUN_00620E90`
- `FUN_00621480`
- `FUN_006225F0`
- `FUN_00622550`
- plus nearby users of parent field `+0x194`

That should let us say:

- how the aggregate child measurement is combined mathematically
- and what kind of inherited control state `+0x194` actually represents
