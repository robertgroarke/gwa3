# GWCA Hook Record Pool Addendum

## Scope

This pass continues the hook-engine reverse work below `FUN_10029730`.

The target functions were:

- `FUN_10029d60`
- `FUN_10029de0`
- `FUN_10029ed0`

These are the functions that allocate, free, and populate the installer-returned hook record / replay handle.

## `FUN_10029d60`: hook-record allocator

The allocator decompiles to a very useful shape.

Simplified:

```cpp
record *FUN_10029d60()
{
    for (page = global_page_list; page != 0; page = page->next) {
        if (page->free_head != 0) goto use_page;
    }

    page = VirtualAlloc(0, 0x1000, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!page) return 0;

    page->active_count = 0;
    build_free_list_of_slots_inside_page();
    page->next = global_page_list;
    global_page_list = page;

use_page:
    slot = page->free_head;
    page->active_count++;
    page->free_head = slot->next;
    return slot;
}
```

### Important new details

- hook records are **not** heap allocated one at a time
- they come from a page pool allocated by:
  - `VirtualAlloc(..., 0x1000, 0x3000, 0x40)`
  - i.e. committed/reserved executable pages
- the pool uses a free-list of fixed-size slots carved out of those pages

That means the replay handle returned by the hook installer is not just metadata. It lives inside RWX memory designed to hold callable stub content.

## Slot/page layout clues

From the allocator loop:

```cpp
puVar3 = puVar2;
do {
    puVar3 = puVar3 + 8;
    *puVar3 = puVar5;
    puVar2[1] = puVar3;
    ...
} while (uVar4 < 0xfe1);
```

This means:

- the first usable slot starts `0x20` bytes into the page
- each subsequent slot is `0x20` bytes apart
- the page header itself occupies the first few dwords

So the pool is effectively:

- page header at the front
- then a chain of `0x20`-byte executable slots

This is a major refinement of the earlier “allocated record” wording.

We can now say the hook engine hands out fixed-size executable slot records.

## `FUN_10029de0`: hook-record free path

The free function confirms the page-pool interpretation.

Simplified:

```cpp
void FUN_10029de0(record *slot)
{
    page = global_page_list;
    while (page != align_down(slot, 0x1000)) {
        page = page->next;
    }

    slot->next = page->free_head;
    page->active_count--;
    page->free_head = slot;

    if (page->active_count == 0) {
        unlink_page_from_global_list();
        VirtualFree(page, 0, MEM_RELEASE);
    }
}
```

### What this tells us

- the record pointer itself lies inside the executable page
- the owning page is recovered by page-aligning the slot address
- empty record pages are fully released with `VirtualFree`

That confirms the returned hook handle is not some heap object with a separate code blob.

It **is itself a slot inside the executable record page**.

## `FUN_10029ed0`: patch-plan builder / stub writer

This function is more complex, but a few points stand out very clearly.

### 1. It writes actual machine-code bytes into the record

Early locals:

```cpp
local_54 = 0xE8;   // CALL rel32
local_40 = 0xE9;   // JMP rel32
local_4c = 0x800F; // Jcc long form seed / transformed branch template
```

And later:

```cpp
puVar9 = (undefined1 *)(param_1[2] + uVar4);
for (uVar6 = local_30; uVar6 != 0; uVar6--) {
    *puVar9 = (char)*puVar8;
    ...
}
```

So `FUN_10029ed0` is not just analyzing instructions. It is actively constructing a mini code stream in the hook record.

### 2. The hook record contains a code buffer pointer at `param_1[2]`

The decompile repeatedly writes emitted bytes to:

```cpp
param_1[2] + offset
```

That strongly suggests the slot structure includes:

- bookkeeping fields
- plus a pointer or embedded area used as the relocation/replay code buffer

### 3. It relocates control-flow instructions

The analysis logic explicitly recognizes and rewrites:

- `E8` = `CALL rel32`
- `E9` / `EB` = unconditional jumps
- short and long conditional branches
- near returns like `C2`

It also rejects cases it cannot safely relocate.

This is the clearest proof yet that GWCA’s hook engine is building a real relocated replay stub, not merely copying a few original bytes and jumping back naively.

## Strongest updated interpretation of `+0x8A3A0`

Combining these functions with the earlier installer work:

- `FUN_10029d60()` allocates a fixed-size executable slot from a `VirtualAlloc` page pool
- `FUN_10029ed0()` analyzes the target prologue and writes relocated machine code into that slot/buffer
- `FUN_10029730()` returns that slot through the `out_original` pointer

So in the frame-dispatch hook case:

- `gwca + 0x8A3A0` is best understood as an executable replay-stub handle allocated from GWCA’s hook-record page pool

This is now much more precise than saying:

- "it is the trampoline/original pointer"

The new refined statement is:

- it points to a pooled executable hook-record slot containing relocated replay code and associated bookkeeping

## Why this matters for the frame UI path

This gives the frame hook chain a much more concrete runtime shape:

1. `CreateHook(...)` chooses the frame dispatcher target
2. `FUN_10029730()` allocates an executable slot record
3. `FUN_10029ed0()` writes relocated original-byte / branch-fixup code into that slot
4. the installer returns that slot via `DAT_1008a3a0`
5. `SendFrameUIMessage(...)` calls that returned slot as the replay path

So the replay call is very likely landing in GWCA-generated executable stub code, not in a raw unmodified game symbol.

## Best next step

The next logical disassembly step is now:

1. determine the exact slot layout around the pointer returned by `FUN_10029d60()`
2. identify where `param_1[2]` points relative to the slot base
3. dump the live bytes at a valid `+0x8A3A0` handle after full init
4. match those bytes against the instruction templates recognized in `FUN_10029ed0`

That should finally let us say:

- what the replay stub looks like in memory
- and how the call from `SendFrameUIMessage(...)` lands inside it
