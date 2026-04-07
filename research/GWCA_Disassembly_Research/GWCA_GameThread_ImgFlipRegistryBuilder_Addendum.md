# GWCA GameThread ImgFlip Registry Builder Addendum

This pass finally exposes a concrete registry-builder seam for one of the callback families.

The key result is that `FUN_0068F540(...)` is a real `ImgFlip` callback-record constructor/inserter, and its hash logic matches the lookup logic in `FUN_0068F610(...)` exactly. That means we now have both sides of one callback registry:

- lookup
- and record creation/insertion

Supporting logs:

- [gw_listfuncs_0068e000_006902ff_temp146.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_listfuncs_0068e000_006902ff_temp146.log)
- [gw_decomp_imgflip_prelude_temp147.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgflip_prelude_temp147.log)
- [gw_decomp_imgflip_callback_families_temp144.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgflip_callback_families_temp144.log)

## 1. Range structure around `FUN_0068F610(...)`

The nearby function cluster is tighter and more meaningful than it first looked:

- `FUN_0068F000(...)`
- `FUN_0068F050(...)`
- `FUN_0068F0E0(...)`
- `FUN_0068F130(...)`
- `FUN_0068F1B0(...)`
- `FUN_0068F230(...)`
- `FUN_0068F2B0(...)`
- `FUN_0068F330(...)`
- `FUN_0068F3A0(...)`
- `FUN_0068F420(...)`
- `FUN_0068F4D0(...)`
- `FUN_0068F540(...)`
- `FUN_0068F610(...)`

That layout now makes sense as:

- a bank of tiny raw flip kernels
- followed by a callback-record constructor
- followed by the hashed callback lookup

So this is no longer just “some functions near the registry.” It looks like the callback-family implementation cluster itself.

## 2. `FUN_0068F540(...)`: concrete callback-record constructor

This helper is the strongest new result.

The body:

```c
void FUN_0068f540(undefined4 key0, undefined4 key1, undefined4 key2)
{
  this[1] = key0;
  this[2] = key1;
  this[3] = key2;
  *this = &PTR_DAT_00a2595c;

  // initialize two intrusive-link subobjects
  this[5] = &this[5];
  this[6] = (int)this + 0x15;
  this[7] = &this[7];
  this[8] = (int)this + 0x1d;

  if (this[1] < 8) hash_extra = 0;
  else hash_extra = FUN_0046dcf0();

  FUN_00473d80(this, this[3] + this[2] ^ this[1] << 2 ^ hash_extra);
}
```

The important points:

- `this + 0x00`
  - initialized to `PTR_DAT_00A2595C`
  - this is the callback record’s table/vtable pointer
- `this + 0x04`
  - key field 0
- `this + 0x08`
  - key field 1
- `this + 0x0C`
  - key field 2
- two intrusive-link/list subobjects are initialized immediately after
- insertion happens through `FUN_00473D80(...)`
- and the inserted hash key is:
  - `key2 + key1 ^ key0 << 2 ^ extra`

That hash expression is exactly the same one `FUN_0068F610(...)` uses for lookup.

So `FUN_0068F540(...)` is not just a nearby helper. It is the creation/insertion half of the same registry that `FUN_0068F610(...)` later queries.

## 3. Why this matters

This closes an important architectural gap.

Before this pass we had:

- `FUN_0068F610(...)`
  - lookup side of the `ImgFlip` callback registry

Now we also have:

- `FUN_0068F540(...)`
  - record construction + intrusive-list insertion side

That means the `ImgFlip` callback family is no longer “just behavior selected somewhere.” It is a real registry of concrete polymorphic records with:

- explicit header keys
- a dispatch-table pointer
- insertion into a hashed/indexed structure

## 4. Provisional `ImgFlip` callback-record layout

This pass lets us tighten the record layout quite a bit.

For the `ImgFlip` registry specifically, the record now looks like:

- `+0x00`
  - table pointer `PTR_DAT_00A2595C`
- `+0x04`
  - family key field 0
- `+0x08`
  - family key field 1
- `+0x0C`
  - family key field 2
- `+0x10..`
  - intrusive-link / registry-node fields

And we know from `FUN_0068F610(...)` that this record later gets matched by:

- field `+0x04`
- field `+0x08`
- field `+0x0C`
- plus a packed/derived key stored at an owner-selected offset

So the `ImgFlip` callback record is now a real object, not just an inferred abstraction.

## 5. The prelude functions are likely the raw flip kernels

The functions from `0068F000` through `0068F4D0` are not registry helpers at all. They are tiny data-transform kernels.

Examples:

- `FUN_0068F000(...)`
  - row-wise reversal over 2-byte elements
- `FUN_0068F050(...)`
  - row-wise reversal over 3-byte elements
- `FUN_0068F0E0(...)`
  - row-wise reversal over 4-byte elements
- `FUN_0068F130(...)`
  - vertical swap/reversal over row records
- `FUN_0068F1B0(...)`
  - similar vertical block swap with `*param_2 * 2`
- `FUN_0068F230(...)`
  - same pattern with `*param_2 * 3`
- `FUN_0068F330(...)`
  - byte-granularity row swap
- `FUN_0068F3A0(...)`
  - 2-byte row swap
- `FUN_0068F420(...)`
  - 3-byte row swap
- `FUN_0068F4D0(...)`
  - 4-byte row swap

These look exactly like the low-level primitives an `ImgFlip` callback family would wrap or dispatch to.

So the cluster now reads very naturally:

- low-level raw flip kernels
- callback-record constructor
- callback-record lookup
- family dispatcher

## 6. What this says about the family callbacks

This pass does not yet prove that the table `PTR_DAT_00A2595C` points directly at `FUN_0068F6C0(...)`, `FUN_0068F840(...)`, `FUN_0068FA70(...)`, or `FUN_0068FCB0(...)`.

But it makes the model much stronger:

- `FUN_0068F540(...)` builds concrete callback records in the same family cluster
- the raw kernels it likely wraps sit immediately before it
- the lookup and dispatch code sit immediately after it

So even without the exact table entries yet, this looks like a real self-contained `ImgFlip` callback subsystem.

## 7. Strongest architectural result

The `ImgFlip` side is now the best-recovered callback registry in this investigation.

We have:

1. concrete family dispatcher
   - `FUN_0068FE40(...)`
2. concrete registry lookup
   - `FUN_0068F610(...)`
3. concrete record constructor/inserter
   - `FUN_0068F540(...)`
4. likely raw worker kernels
   - `FUN_0068F000(...)` through `FUN_0068F4D0(...)`

That is a much better footing than the broader `FUN_00688890(...)` transfer-manager registry, which we have mostly from the lookup/invoke side only.

## 8. Best current model

The `ImgFlip` callback subsystem now looks like:

1. choose compressed-family bucket in `FUN_0068FE40(...)`
2. look up a callback record through `FUN_0068F610(...)`
3. callback record was originally built by `FUN_0068F540(...)`
4. callback record dispatches into family logic built on the nearby flip kernels

That means the `ImgFlip` registry is now the cleanest route to a fully concrete callback object family.

## 9. Next best step

The strongest next reverse step is to tie the record constructor directly to the concrete family callbacks:

- identify where `FUN_0068F540(...)` is called
- recover what values are passed for its three key fields
- and, if possible, resolve the table behind `PTR_DAT_00A2595C`

That should let us answer the remaining high-value question:

- which concrete callback record instance corresponds to each of the `DXT1`, `DXT2/3`, `DXT4/5/DXTL`, and `DXTA` family methods
