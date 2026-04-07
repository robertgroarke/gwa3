# GWCA Toolbox Record Layout Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_ContainerHelpers_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ContainerHelpers_Addendum.md)

The target here was the callback-record constructor/mover layer:

- `FUN_10006480`
- `FUN_10006600`
- `FUN_10005E90`

The goal was to move from:

- “`0x30` record with scalar header and callable tail”

to:

- a more explicit field-level model

## Strongest new result: the callable-holder subobject starts at `record + 0x08`, and its active ownership slot is at `record + 0x2C`

Fresh decompilation:

- [decomp_recordhelpers_temp8.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_recordhelpers_temp8.log)

gives the clearest field-level picture so far.

### `FUN_10006480`

This helper:

1. copies:
   - `record[+0x00]`
   - `record[+0x04]`
2. zeroes:
   - destination `+0x2C`
3. inspects source:
   - `source[+0x2C]`
4. if non-null:
   - if it equals `source + 0x08`, clone via virtual slot `+0x04` into `dest + 0x08`
   - otherwise transfer pointer ownership directly into `dest + 0x2C`

That means the internal callable-holder layout is now much more explicit:

- embedded SBO buffer / self-object begins at:
  - `record + 0x08`
- the active pointer/reference to the callable implementation lives at:
  - `record + 0x2C`

This is stronger than the earlier “callable tail somewhere after the header” phrasing.

## `FUN_10006600` confirms the same layout, but as an overwrite/assignment helper

`FUN_10006600` behaves like an assignment form of the same logic:

1. copy scalar header:
   - `+0x00`
   - `+0x04`
2. if destination already owns a callable at `dest + 0x2C`:
   - destroy it through virtual slot `+0x10`
3. clone or transfer the source callable from:
   - `source + 0x08`
   - `source + 0x2C`
4. install the result in:
   - `dest + 0x08`
   - `dest + 0x2C`

So the same field interpretation survives in both:

- copy-construct path
- overwrite/assignment path

That is a strong signal that the record layout is real, not an artifact of one helper.

## Updated callback-record layout

The tightest model we can now state is:

```text
struct CallbackRecord30 {
    int32_t    altitude;        // +0x00
    HookEntry* hook_entry;      // +0x04
    uint8_t    callable_sbo[0x24]; // +0x08 .. +0x2B
    void*      callable_ptr;    // +0x2C
}; // size 0x30
```

Where:

- `callable_ptr == record + 0x08` means “using inline/SBO callable storage”
- `callable_ptr != record + 0x08` means “using external callable object”
- `callable_ptr == 0` means “no active callable payload”

This is the clearest record model we have recovered so far.

## `FUN_10005E90` is the specialized grow-and-insert path for `0x30` records

This helper confirms the record size and growth mechanics:

1. capacity/count are measured in units of `0x30`
2. new capacity grows geometrically
3. raw bytes allocated are:
   - `count * 0x30`
4. insertion slot is:
   - `new_base + index * 0x30`
5. the new record is written with:
   - `FUN_10006480(...)`
6. existing records are migrated with repeated:
   - `FUN_10006480(...)`
7. the old vector is cleaned with:
   - `FUN_10005D30(begin, end)`

So the specialized vector path is not copying raw 48-byte blobs.
It is using record-aware move/copy helpers for every element.

That tells us the record is non-trivially movable because of the callable-holder tail.

## The scalar header is now clearer too

The constructor/mover helpers only copy the first two dwords directly:

- `+0x00`
- `+0x04`

Those line up perfectly with the registration/removal behavior we already saw:

- `+0x00` is the altitude sort key
- `+0x04` is the `HookEntry*` identity used by removers and ownership grouping

So the callback-record header is now effectively confirmed.

## Relationship to the public APIs

This pass tightens the earlier registration/removal model:

### Registration

- build `CallbackRecord30`
- `FUN_10007360` inserts it
- `FUN_10006480` / `FUN_10005E90` manage record-aware moves

### Removal

- locate matching `HookEntry*` at `record + 0x04`
- shift trailing `0x30` records left
- use the same callable-holder destruction rules via `+0x2C`

So the public APIs are sitting on a very regular record-management layer now.

## Most important takeaway

The most useful new fact is not just that the record is `0x30` bytes.

It is that the record’s callable payload is a distinct two-part subobject:

- inline callable storage at `+0x08`
- active pointer/owner slot at `+0x2C`

That is the key to understanding nearly every move, insert, and remove helper in this subsystem.

## Best current interpretation

The strongest conservative reading after this pass is:

- `CallbackRecord30` has a 2-dword scalar header and a callable-holder subobject
- the callable-holder subobject begins at `record + 0x08`
- `record + 0x2C` is the active callable pointer/ownership discriminator
- the vector helpers are element-aware because that subobject is non-trivial

That gives us a practical record layout we can now use throughout the rest of the UI callback analysis.

## Best next step

The next logical reverse step is to inspect the remaining callable-holder support helpers, especially:

- `FUN_10005D30`
- `FUN_10007080`
- and, if useful, the virtual-family helpers those functions ultimately rely on

That should answer the last field-level questions:

1. whether `FUN_10005D30` is purely a destructor sweep over `CallbackRecord30`
2. what exact object family lives behind the callable-holder vtable
3. whether there are multiple callable-holder implementations or just SBO-vs-external variants
