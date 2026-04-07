# GWCA GameThread Callback Record Shape Addendum

This pass tightens the callback-object seam one step further.

The main result is that we can now describe the callback registry and callback record shape much more concretely from two sides at once:

- lookup side: `FUN_00688890(...)`
- invoke side: `FUN_00689A00(...)` and `FUN_00688A10(...)`

Supporting logs:

- [gw_findcallers_00688890_temp139.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00688890_temp139.log)
- [gw_decomp_00688890_temp140.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_00688890_temp140.log)
- [gw_findcallers_00689a00_temp142.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00689a00_temp142.log)
- [gw_decomp_processor_consumers_temp136.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_processor_consumers_temp136.log)
- [gw_decomp_callback_handler_seam_temp138.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_callback_handler_seam_temp138.log)

## 1. `FUN_00688890(...)` gives the registry-owner layout

The registry owner object behind `FUN_00688890(...)` now has a reasonably clear field model:

- `this + 0x0C`
  - offset inside each callback record where the packed combined signature lives
- `this + 0x10`
  - pointer to the bucket table base
- `this + 0x18`
  - bucket count / table bound
- `this + 0x1C`
  - bucket mask used to derive the bucket index

That comes directly from the lookup body:

```c
packed = (field1 << 3 | field0) << 3 | field2;
bucket = *(uint *)(this + 0x1c) & packed;
node = *(int *)(this + 0x10) + bucket * 0xc;
...
if (*(uint *)(rec + *(int *)(this + 0xc)) == packed &&
    *(uint *)(rec + 4) == field0 &&
    *(uint *)(rec + 8) == field1 &&
    *(uint *)(rec + 0xc) == field2) {
    return rec;
}
```

So the callback lookup is a real hashed registry object, not a flat array and not a switch statement.

## 2. Provisional callback-record layout

Combining the lookup and invoke sites gives the strongest callback-record layout we have so far.

Lookup side proves the record contains:

- `record + 0x04`
  - key field 0
- `record + 0x08`
  - key field 1
- `record + 0x0C`
  - key field 2
- `record + [owner.key_offset]`
  - packed combined key

Invoke side from `FUN_00689A00(...)` proves the record also contains:

- `record + 0x00`
  - pointer to a method table / vtable-like dispatch object

because the code does:

```c
puVar4 = (undefined4 *)*local_7c;
(*(code *)*puVar4)(...);
```

That is:

1. dereference the record at `+0x00`
2. treat that as a table pointer
3. call slot `0` of that table

So the best current provisional record layout is:

- `+0x00`
  - table pointer / vtable-like pointer
- `+0x04`
  - source/field-0 key
- `+0x08`
  - destination/field-1 key
- `+0x0C`
  - flags/field-2 key
- `+0x10` or another owner-selected offset
  - packed combined key

I am calling the packed-key slot provisional because `FUN_00688890(...)` reads the exact offset from the owner object rather than hardcoding it.

## 3. What `FUN_00688890(...)` is really returning

This pass makes the callback return value much less abstract.

`FUN_00688890(...)` returns:

- not a function id
- not a tiny policy token
- not a format descriptor row

It returns:

- a callback record pointer
- whose first field points to a dispatch table
- whose remaining header fields describe the selector key that matched

So the first real consumer object above the stored block slabs is now very likely a polymorphic callback record family.

## 4. `FUN_00689A00(...)`: first invoked callback slot is slot `0`

We still do not have the concrete class/type name for the returned callback objects, but the invoke contract is now explicit.

In the one-plane case:

```c
puVar4 = (undefined4 *)*local_7c;
(*(code *)*puVar4)(...);
```

In the split two-plane case:

- plane 0 callback is invoked first
- plane 1 callback is invoked second

So the callback record’s first useful dispatch entry is table slot `0`.

That gives us the right next reverse target: not “find anything near `00688890`,” but “find the concrete callback objects whose table slot `0` bodies match the signatures used here.”

## 5. `FUN_00689A00(...)` is a very broad seam

The caller map for `FUN_00689A00(...)` is wider than the earlier narrow storage-only picture.

Current callers in this explored build:

- `FUN_00654AF0(...)`
- `FUN_00654E10(...)`
- `FUN_00655830(...)` at two sites
- `FUN_00654C70(...)` at two sites
- `FUN_0064C340(...)`
- `FUN_0064B640(...)`
- `FUN_0064BB40(...)`
- `FUN_0068FE40(...)` at two sites
- `FUN_00665520(...)`
- `FUN_00648E80(...)`
- `FUN_00664CA0(...)`
- `FUN_00655100(...)`
- `FUN_00664EC0(...)`
- `FUN_006350A0(...)`

That matters because it shows `FUN_00689A00(...)` is not a narrow helper just for one import path.

It is a broad transfer/initialization seam used by:

- DDS/ATEX-side front doors
- object setup paths
- clone/assign paths
- blit paths
- fallback/default paths
- and at least one older lower-addressed cluster

So the callback record family selected by `FUN_00688890(...)` is very likely foundational to the engine’s image transfer path, not a corner-case adapter.

## 6. What this says about the companion stream

This pass does not yet name the companion stream’s final engine role, but it makes one thing much more solid:

- the first code that can understand the companion stream is almost certainly a polymorphic callback record
- not a generic `ImgMem` reader
- and not the hash registry itself

That is exactly where we would expect format-pair-specific behavior like:

- `DXTA` seeing only the primary stream
- `DXT5` / `DXTL` seeing both primary and companion streams

So the “companion BC1-style block plane” interpretation is now sitting on a stronger architectural base:

- one callback family can consume only the scalar/selector plane
- another can consume scalar/selector plus the extra companion plane

## 7. Best current model

At this point the callback side looks like:

1. `FUN_00688930(...)`
   - decide one-plane or two-plane routing
2. `FUN_00688890(...)`
   - hashed lookup of callback records
3. callback record
   - header with three explicit selector fields
   - packed combined key
   - table pointer at `+0x00`
4. slot `0` of that table
   - first real callback body that can interpret the stored slab payload

That is the cleanest “what object do we need next?” answer we’ve had yet.

## 8. Next best step

The strongest next reverse step is now:

- find the builder/populator for the callback registry object behind `FUN_00688890(...)`
- or directly identify one callback record instance whose slot-`0` target we can resolve

That should finally expose:

- the concrete callback class family
- and the first real method body that can read both the primary and companion streams together
