## `Gw.exe` Frame Callback Constructor Skeleton Addendum

This pass continues from the aggregate-grid note by decompiling the last still-anonymous helpers that appear in the core control-node constructor chain:

- `FUN_00629190`
- `FUN_0062E520`
- `FUN_0060BBD0`
- `FUN_0062EFE0`
- `FUN_0062F580`

The goal was simple:

- finish the base control-node construction story before branching back out into more specialized control families

## Source artifacts

These results come from:

- [gw_decomp_constructor_layers_temp91.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_constructor_layers_temp91.log)
- [gw_decomp_inherited_state_callers_temp90.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_inherited_state_callers_temp90.log)

## High-level result

This pass is less about surprise semantics and more about closing structural gaps.

The five remaining helpers are mostly:

- embedded subobject/list initializers
- plus one default flag/value seed

So the core constructor path is now much more readable:

1. global id registration
2. relation/hash metadata
3. inherited parent state
4. callback/handler registration
5. a fixed set of embedded subobject/list skeletons
6. initial owner flags/state defaults

That means the base control object is not a single flat record.
It is a composed object with several always-present internal substructures.

## `FUN_00629190()`: initialize embedded list-backed state block and enqueue it

This helper is the richest of the batch.

It:

- initializes a self-linked intrusive node at the current `this`
- zeroes a block of fields through roughly:
  - `+0x1C .. +0x54`
- sets:
  - `*(this + 8) = 1`
- then inserts the object into the intrusive list rooted at:
  - `DAT_00BD0CCC`
  - `DAT_00BD0CD0`

So this helper is best described as:

- initialize an embedded list-bearing state block
- then immediately register it in the global active/dirty list

That means this subobject is not passive scaffolding.
It is part of the runtime scheduling/update machinery for the control node.

## `FUN_0062E520()`: seed default owner/state flags to `0x4100`

This helper is tiny:

```cpp
*this = 0x4100;
```

But it is useful because we now know one default state field is always initialized to:

- `0x4100`

immediately after:

- global id registration
- relation metadata attachment

and before the later explicit flag mutation:

- `FUN_0062E5D0(4, 0, 0)`

So the constructor always starts from a fixed baseline owner/state flag word.

## `FUN_0060BBD0()`: initialize a small embedded intrusive-list subobject

This helper:

- initializes one self-linked node at `this`
- initializes a second self-linked node at `this + 0x0C`
- sets:
  - `*(this + 8) = 0x0C`

So this is another embedded list container / node block with a fixed local type/size marker.

The strongest safe reading is:

- one always-present internal list head / collection subobject

not:

- a one-off runtime action

## `FUN_0062EFE0()`: zero one embedded field

This helper is minimal:

```cpp
*this = 0;
```

By itself it does not tell us much semantically, but in context it matters because it shows the constructor is explicitly seeding even the trivial embedded fields rather than relying on allocator-zeroed memory.

So the control-node constructor is deliberate and field-by-field, not a mostly-zero default with a few later writes.

## `FUN_0062F580()`: initialize another small embedded intrusive-list subobject

This helper is very similar in flavor to `FUN_0060BBD0()`.

It:

- zeroes/initializes a root pointer field
- initializes a self-linked node at `this + 0x0C`
- sets:
  - `*(this + 8) = 0x0C`

So this is a second always-present list/container style embedded subobject.

Taken together with `FUN_0060BBD0()`, it strongly suggests the base control node includes multiple internal collection/list heads, each with its own lifecycle and usage.

## What this does to the constructor picture

Earlier passes showed that `FUN_0060D300(...)` does all of the following:

- `FUN_006240D0()`
- `FUN_00627740(type)`
- `FUN_00629190()`
- `FUN_0062C650(parent, type, arg)`
- `FUN_0062E520()`
- write style flags at `+0x190`
- `FUN_00613400(parent)`
- `FUN_0060BBD0()`
- `FUN_0062EFE0()`
- `FUN_0062F580()`
- `FUN_00627D70(callback, payload)`
- `FUN_0062E5D0(4,0,0)`
- `FUN_00628A10(2,0,0)`
- `FUN_006286D0(10,0,0)`

This pass lets us tighten the anonymous middle of that sequence:

- `FUN_00629190()` = active/dirty-list-bearing embedded state block
- `FUN_0062E520()` = baseline owner/state flags `0x4100`
- `FUN_0060BBD0()` = embedded list/container subobject A
- `FUN_0062EFE0()` = zeroed embedded field
- `FUN_0062F580()` = embedded list/container subobject B

So the constructor is now best described as building:

- one relation identity layer
- one owner-state layer
- several embedded list/container subobjects
- one callback/handler layer

## What this changes about the root/bootstrap path

The root/bootstrap path in `FUN_0060EF50(...)` runs the same embedded-subobject sequence:

- `FUN_006240D0()`
- `FUN_00627740(0)`
- `FUN_00629190()`
- `FUN_0062C650(0,0,0)`
- `FUN_0062E520()`
- `FUN_00613400(0)`
- `FUN_0060BBD0()`
- `FUN_0062EFE0()`
- `FUN_0062F580()`
- `FUN_0062E5D0(4,0,0)`

That is important because it shows these helpers are not special to one derived control family.
They are part of the base/root control skeleton itself.

So this constructor skeleton is genuinely foundational.

## Strongest current model after this pass

At this point the cleanest constructor model is:

- allocate control node
- register runtime id
- attach relation/hash metadata
- seed baseline owner/state flags
- inherit parent state if there is a parent
- initialize several embedded list/container subobjects
- install callback/handler layer
- trigger owner-local type-2 and global message-10 startup

That is a fairly complete base-control construction story.

## Best next step

The highest-yield next pass is no longer the base constructor itself.
The best move now is to pivot back outward and name one of the specialized control families that sits on top of this skeleton.

The strongest candidates are:

- the slot-child repeated family around slots `2/4/5/6`
- or the created-child family driven through `FUN_0060D300(...)` plus `0x57..0x62`

If we keep going on the next pass, I’d target the repeated slot-family children first, because the constructor and layout substrate under them is now strong enough to support more concrete control-type naming.  
