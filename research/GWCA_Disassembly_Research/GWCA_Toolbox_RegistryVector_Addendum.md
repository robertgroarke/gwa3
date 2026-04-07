# GWCA Toolbox Registry Vector Addendum

## Scope

This pass continues the toolbox seam work from:

- [GWCA_Toolbox_ExportSeam_DeepDive.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ExportSeam_DeepDive.md)

The goal here was to reduce two remaining unknowns:

1. what the listener vector at `DAT_1008A1E0..DAT_1008A1E4` actually stores
2. whether the surrounding helpers are generic allocator glue or subsystem-specific logic

## Strongest new result: `DAT_1008A1E0..DAT_1008A1E4` is a vector of heap records keyed by integer id

The most useful decompile in this pass is:

- [FUN_1001db40](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001db40_deep.log)

Recovered behavior:

1. iterate `puVar3` from `DAT_1008A1E0` to `DAT_1008A1E4`
2. treat each entry as a pointer to a heap object
3. compare `*entry_object` against the caller-supplied integer id
4. if it matches:
   - look at `entry_object[0x0B]` which is offset `+0x2C`
   - if non-null, invoke a virtual callback at `vtable+0x10`
   - then clear `entry_object[0x0B]`
   - free the heap object
   - compact the vector

That gives a much cleaner field sketch for the vector contents:

```text
struct ListenerRecord {
    uint32_t key;          // offset +0x00
    ...
    void* callback_ctx;    // offset +0x2C
};
```

The exact middle fields are still unresolved, but the vector is no longer abstract.

It is a heap-backed registry of records matched by integer key.

## The cleanup path is explicitly heap/free based

Two nearby helpers close the loop:

- [FUN_1001dbc0](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001dbc0_recheck.log)
- [FUN_1001dbe0](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001dbe0_recheck.log)
- [FUN_1001dc00](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001dc00_deep.log)

### `FUN_1001dbc0`

Builds a `std::_Func_impl_no_alloc<...>` wrapper around:

- `GW::MemoryMgr::MemFree(void*)`

using the pointer stored at `this + 4`.

### `FUN_1001dbe0`

Is the actual free helper:

```cpp
lpMem = *(LPVOID *)(param_1 + 4);
HeapFree(GetProcessHeap(), 0, lpMem);
```

### `FUN_1001dc00`

Returns the RTTI descriptor for that same `MemFree` lambda type.

So this part of the seam is definitely using:

- heap-allocated records
- function-object cleanup glue
- vector compaction after removal

That is stronger than just saying “it looks STL-ish.”

## The listener vector and the exported seam are directly connected

The exported/heavy routine at `0x1001CCC0` walks:

- `DAT_1008A1E0`
- `DAT_1008A1E4`

and for each record:

1. loads the object pointer
2. reads `[object + 0x2C]`
3. if non-null, calls through its vtable at `+0x08` with a pointer to the current argument slot

Then after the iteration completes it calls the indirect target in:

- `DAT_1008A1BC`

That makes the control flow much clearer:

```text
exported wrapper
  -> enumerate listener records
  -> optionally let each listener mutate/inspect the argument
  -> invoke final underlying target
  -> rebuild related metadata
```

So the listener vector is not a side structure.
It is on the critical path of this seam.

## The range-table side remains separate but related

Nothing in this pass contradicted the previous range-table model:

- `DAT_1008A1EC..DAT_1008A1F4` is still the `0x0C`-record range vector
- `0x1001CBB0` is still the lookup helper
- `0x1001CD20` is still the builder

What changed is that the seam is now split into two clearer cooperating structures:

1. a listener-record vector
   - `DAT_1008A1E0..DAT_1008A1E4`
2. a range-record vector
   - `DAT_1008A1EC..DAT_1008A1F4`

This is better than treating all those globals as one generic “registry blob.”

## Most important takeaway

The toolbox build seam is now best modeled as:

- indirect exported wrappers
- a listener/interceptor record vector keyed by integer id
- a separate runtime-built range metadata vector
- explicit heap/free glue for record lifecycle

That gives the first field-level evidence that this compiled layer is doing structured interception and dispatch, not merely hiding a few function pointers.

## Best next step

The next logical pass is to recover the constructor/inserter side for the listener records:

1. identify who allocates and populates the objects pushed into `DAT_1008A1E0..DAT_1008A1E4`
2. recover more of the record layout between:
   - `+0x00` key
   - `+0x2C` callback/context slot
3. determine whether the integer key is:
   - an item/model id
   - a frame/message id
   - an opcode/descriptor id
   - or some other subsystem tag

At this point, that constructor path is the shortest way to turn this from a structural model into a semantic one.
