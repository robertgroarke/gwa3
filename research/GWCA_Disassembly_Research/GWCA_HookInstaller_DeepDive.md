# GWCA Hook Installer Deep Dive

## Scope

This note continues the hook-engine reverse path below `GW::Hook::CreateHook(...)`.

The target in this pass was:

- `FUN_10029730`

This is the internal installer that actually creates the detour/trampoline state used by GWCA's frame and UI hooks.

## Refined decompile result

The earlier high-level summary was accurate, but the fuller decompile makes several details much clearer.

Simplified from the latest decompile:

```cpp
int FUN_10029730(target_ptr, detour, out_original)
{
    initialize_hook_subsystem();
    if (heap_handle == 0) return 2;

    if (!validate_address(target_ptr) || !validate_address(detour)) return 7;
    if (find_existing_hook(target_ptr) != 0xffffffff) return 3;

    hook_record = allocate_hook_record();
    if (!hook_record) return 9;

    plan.target = target_ptr;
    plan.detour = detour;
    plan.record = hook_record;

    if (!analyze_target_and_build_patch_plan(&plan)) {
        free_hook_record(hook_record);
        return 8;
    }

    ensure_or_grow_global_hook_table();

    table_entry.target = analyzed_target;
    table_entry.detour = detour;
    table_entry.record = hook_record;
    table_entry.flags = patch_mode / relocation bits;
    table_entry.saved_words = copied original-byte / trampoline metadata;

    if (patch_mode == 0) {
        table_entry.saved_dword_0 = *(target_ptr + 0)
        table_entry.saved_byte_4  = *((byte*)target_ptr + 4)
    } else {
        table_entry.saved_dword_0 = *(target_ptr - 5)
        table_entry.saved_word_4  = *(target_ptr - 1)
        table_entry.saved_byte_6  = *((byte*)target_ptr + 1)
    }

    if (out_original) *out_original = hook_record;

    clear_hook_creation_lockflag();
    return status;
}
```

## New important details from the deeper decompile

### 1. The installer clearly has multiple patch modes

The branch on `local_20` is the most useful new detail.

When `local_20 == 0`, the installer copies:

- one dword from `target`
- one byte from `target + 4`

When `local_20 != 0`, it instead copies:

- one dword from `target - 5`
- one word from `target - 1`
- one byte from `target + 1`

That strongly suggests the installer supports at least two detour layouts, probably depending on where the canonical entry lands and how the jump/near-call normalization worked.

This is stronger than the earlier generic “saved bytes or metadata” conclusion.

We now know:

- the saved-byte layout is mode-dependent
- the hook table preserves enough original code bytes to support different patch geometries

### 2. The returned `out_original` pointer is the allocated hook record itself

At the end:

```cpp
if (param_3 != 0) {
    *param_3 = local_24;
}
```

And `local_24` is the allocated record returned by `FUN_10029d60()`.

So the installer is not returning:

- the raw original function address
- or a direct pointer to the untouched game symbol

It is returning:

- the allocated hook record pointer

This is a very strong confirmation of the prior interpretation of `+0x8A3A0`.

In the frame hook case, `DAT_1008a3a0` is literally the installer-returned record/replay handle.

## 3. The global hook table entry is compact but structured

The installer stores entries in a table whose slots are `0x2c` bytes apart:

```cpp
iVar1 = DAT_1008b0c8 * 0x2c;
...
puVar5 = (undefined4 *)(iVar1 + (int)DAT_1008b0c0);
```

From the recovered writes, each entry contains at least:

- `+0x00` target pointer
- `+0x04` detour pointer
- `+0x08` hook record pointer
- `+0x14` low flag bits / mode bits
- `+0x18..0x28` saved metadata words

That means the global hook table is not just a registry of addresses. It is a compact per-hook state block.

## 4. Heap behavior is now concrete

The installer’s global hook table starts with capacity `0x20` entries:

```cpp
DAT_1008b0c4 = 0x20;
DAT_1008b0c0 = HeapAlloc(..., 0x580);
```

Since:

- `0x20 * 0x2c = 0x580`

the sizes line up exactly.

When full, the table doubles:

```cpp
if (DAT_1008b0c4 <= DAT_1008b0c8) {
    pvVar4 = HeapReAlloc(..., DAT_1008b0c4 * 0x58);
    DAT_1008b0c4 = DAT_1008b0c4 * 2;
}
```

So the hook engine uses:

- one heap-backed global table of fixed-size `0x2c` entries
- separate allocated hook records returned through `out_original`

That separation is important.

It means the per-hook “original” handle and the table entry are related, but not the same object.

## What this means for the frame hook

For the frame dispatcher path:

```cpp
CreateHook((void **)&DAT_1008a39c, FUN_10026860, (void **)&DAT_1008a3a0);
```

we can now say more precisely:

- `DAT_1008a39c` is the canonicalized patch target
- `FUN_10026860` is the detour body
- `DAT_1008a3a0` receives the allocated hook-record / replay handle created by `FUN_10029730`

And then:

- `SendFrameUIMessage(...)` uses that handle as its continuation target

So the frame-side replay chain is:

```text
patched game entry -> FUN_10026860 -> SendFrameUIMessage -> DAT_1008a3a0 handle -> original/replay path
```

## Strongest new conclusion

The most important refinement from this pass is:

- `+0x8A3A0` should be thought of as an installer-returned hook record / replay handle

not merely:

- “the hooked function pointer”

That wording matters because the decompile shows the out pointer is the allocated record object, while the installer separately stores the target/detour/metadata in the global hook table.

## Best next step

The next strongest reverse step is now very focused:

1. identify the layout of the allocated record returned by `FUN_10029d60()`
2. characterize how `SendFrameUIMessage(...)` is able to call that record as if it were a function
3. dump the live bytes at the `+0x8A3A0` value again, with a validated live-process probe

That is the remaining gap between:

- “we know what role the handle plays”

and

- “we know the exact stub/record layout and call mechanics.”
