# GWCA GameThread Callback Lookup Registry Addendum

This pass takes the next step down from the callback-plane selector and confirms what `FUN_00688890(...)` actually is.

The key result is that `FUN_00688890(...)` is not a constructor or policy helper. It is a hashed lookup over a callback-object registry keyed by a compact three-field signature. That puts us directly on the first concrete object seam that can interpret the stored block streams.

Supporting logs:

- [gw_findcallers_00688890_temp139.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_00688890_temp139.log)
- [gw_decomp_00688890_temp140.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_00688890_temp140.log)

## 1. `FUN_00688890(...)`: hashed callback-record lookup

The body is short and very telling:

```c
int FUN_00688890(uint *param_1)
{
    uint packed = (param_1[1] << 3 | *param_1) << 3 | param_1[2];
    uint bucket = *(uint *)(this + 0x1c) & packed;
    if (*(uint *)(this + 0x18) <= bucket) abort();

    int *node = *(int *)(this + 0x10) + bucket * 0xc;
    if ((node[2] & 1U) == 0) {
        for (rec = node[2]; rec != 0; rec = *(int *)(*node + rec + 4)) {
            if (*(uint *)(rec + *(int *)(this + 0xc)) == packed &&
                *(uint *)(rec + 4) == param_1[0] &&
                *(uint *)(rec + 8) == param_1[1] &&
                *(uint *)(rec + 0xc) == param_1[2]) {
                return rec;
            }
            if ((*(uint *)(*node + rec + 4) & 1) != 0) {
                return 0;
            }
        }
    }
    return 0;
}
```

The important points are:

- the lookup key is exactly three fields:
  - field 0
  - field 1
  - field 2
- those are packed into one combined selector:
  - `(field1 << 3 | field0) << 3 | field2`
- the registry is bucketed through a mask at `this + 0x1c`
- bucket count lives at `this + 0x18`
- bucket table lives at `this + 0x10`
- there is also a base/offset field at `this + 0x0c` used to reach the packed signature inside the record

So this is a real hashed callback-object registry, not just a linear table.

## 2. What the returned value is

The return value is an integer record pointer, not a small enum or function index.

That matters because it matches how `FUN_00688930(...)` uses the result:

- it stores the returned pointer directly as `plane0`
- or as `plane0` and `plane1`
- later `FUN_00689A00(...)` dereferences those pointers and calls through their first function pointer

So the registry is returning full callback records / objects, not bare function ids.

## 3. Caller map: only `FUN_00688930(...)`

`FUN_00688890(...)` has only three xrefs, all from `FUN_00688930(...)`:

- one for the single-plane lookup
- two for the split two-plane fallback

That is a very nice boundary because it means:

- `FUN_00688890(...)` is the concrete lookup primitive
- `FUN_00688930(...)` is the policy layer deciding whether to use:
  - one combined callback
  - or two staged callbacks

So there is no hidden third user muddying the model.

## 4. `FUN_00688A10(...)`: rectangle path confirms the split-plane model

The large rectangle/subregion path in `FUN_00688A10(...)` reinforces the same story.

It:

- validates source and destination geometry
- checks capability bits like `0x8`, `0x10`, and `0x20`
- computes local block geometry and per-level offsets
- calls `FUN_00688930(...)`
- may create an auxiliary resource with `FUN_006A1610(...)`
- then either:
  - calls the single callback directly
  - or runs the staged split path using both callbacks
- and fills leftover regions through `FUN_00689D40(...)`

So the first concrete consumer of the stored payload is not “the registry” abstractly. It is the callback record returned by `FUN_00688890(...)`, invoked in one-plane or two-plane form by:

- `FUN_00689A00(...)`
- `FUN_00688A10(...)`

## 5. Why this matters for the companion stream

This pass sharpens the target considerably.

We already knew:

- `FUN_0069E1C0(...)` reconstructs per-level slabs
- those slabs carry:
  - a primary two-word-per-block stream
  - and sometimes a `0x210`-gated companion two-word-per-block stream

Now we know the first concrete object seam above that storage:

- `FUN_00688890(...)` returns callback records keyed by:
  - source format
  - destination format
  - transfer flags

That means the companion stream’s meaning is very likely not universal across the whole engine.

It is probably interpreted by specific callback records selected for specific:

- format-pair conversions
- or split source-side / destination-side transfer planes

That fits the `DXTA` result very well:

- `DXTA` lacks the companion stream
- so callback records selected for `DXTA` will only ever see the primary stream
- callback records for `DXT5` / `DXTL`-style paths can receive both

## 6. Best current model

The storage-to-callback path now looks like:

1. `FUN_0069E1C0(...)`
   - rebuild persistent per-level block slabs
2. `ImgMem`
   - store those slabs by level
3. `FUN_00688930(...)`
   - choose one-plane or two-plane interpretation
4. `FUN_00688890(...)`
   - return concrete callback records from a hashed registry
5. `FUN_00689A00(...)` / `FUN_00688A10(...)`
   - invoke those callback records on full-level or subrectangle work

So we now have the exact lookup seam where the engine chooses the object that can understand the companion block plane.

## 7. Next best step

The strongest next step is to recover the callback-record layout and the first method body behind those records.

The best targets are:

- the callback registry object fields around `this + 0x0c .. 0x1c`
- the record layout returned by `FUN_00688890(...)`
- and the first invoked callback method used by `FUN_00689A00(...)` / `FUN_00688A10(...)`

That should finally expose the first concrete code path that can read both:

- the primary scalar/selector stream
- and the companion BC1-style stream
