# GWCA Hook Toggle Addendum

## Scope

This pass continues the GWCA hook-engine reverse work at the next logical layer after the replay-stub analysis:

- the code that suspends threads
- the code that writes and removes detour bytes
- the state bits that track whether hooks are enabled or disabled

The main target functions were:

- `FUN_100292f0`
- `FUN_100293b0`
- `FUN_10029470`
- `FUN_10029550`
- `FUN_100295e0`

## Big result

`FUN_10029470` is the patch-application helper.

It is the function that:

- chooses the patch address
- `VirtualProtect`s the target bytes
- writes either the original bytes back or a detour jump
- flushes the instruction cache
- updates the hook-entry state bits

So the earlier replay-stub work now has its missing counterpart:

- `FUN_10029ed0` builds the replay slot
- `FUN_10029470` writes/removes the live detour at the original target

## `FUN_10029470`: patch writer / patch remover

The decompile is very direct:

```cpp
puVar4 = (undefined4 *)(param_1 * 0x2c + DAT_1008b0c0);
lpAddress = (undefined4 *)((int)*puVar4 + -5);
if ((*(byte *)(puVar4 + 5) & 1) == 0) {
    lpAddress = (undefined4 *)*puVar4;
}
dwSize = (uint)(*(byte *)(puVar4 + 5) & 1) * 2 + 5;
VirtualProtect(lpAddress,dwSize,PAGE_EXECUTE_READWRITE,&oldProtect);
```

That means:

- `entry+0x00` = canonical target pointer
- if flag bit `0` is clear, patch at `target`
- if flag bit `0` is set, patch at `target - 5`
- patch length is:
  - `5` bytes in normal mode
  - `7` bytes in the special mode

### Disable path

When `param_2 == 0`, GWCA restores the original bytes:

```cpp
*lpAddress = puVar4[3];
if ((flags & 1) == 0) {
    *(byte *)(lpAddress + 1) = *(byte *)(puVar4 + 4);
}
else {
    *(ushort *)(lpAddress + 1) = *(ushort *)(puVar4 + 4);
    *(byte *)((int)lpAddress + 6) = *(byte *)((int)puVar4 + 0x12);
}
```

So the installer-saved bytes at:

- `entry+0x0C`
- `entry+0x10`
- and, in special mode, `entry+0x12`

are exactly the original bytes needed to restore the patched target.

### Enable path

When `param_2 != 0`, GWCA writes the detour:

```cpp
*(byte *)lpAddress = 0xE9;
*(int *)((int)lpAddress + 1) = (entry.detour - lpAddress) - 5;
if ((flags & 1) != 0) {
    *(ushort *)target = 0xF9EB;
}
```

So the normal hook form is:

```asm
E9 <rel32>
```

The special form is:

- a detour jump written at `target - 5`
- plus `EB F9` written at `target`

That `EB F9` is a short jump back `-7`, which sends execution from `target` back to `target - 5`, i.e. into the 5-byte `jmp rel32`.

That finally explains the meaning of the special patch mode:

- the function is hooked by placing the main detour just before the target
- the actual target entry is turned into a 2-byte short jump back to that pre-entry detour

This matches the earlier installer logic that saved bytes from `target - 5 .. target + 1` in special mode.

## Stronger hook-entry layout

This pass sharpens the compact `0x2c` entry layout:

- `+0x00` target pointer
- `+0x04` detour pointer
- `+0x08` replay-slot pointer
- `+0x0C` first 4 original bytes from patch site
- `+0x10` 5th original byte in normal mode, or bytes `+4..+5` in special mode
- `+0x12` 7th original byte in special mode
- `+0x14` flag byte
- `+0x18` low nibble = relocation-entry count
- `+0x1C..0x23` original-side offset table
- `+0x24..0x2B` replay-side offset table

## Flag-byte semantics: bit `2` is no longer vague

The state update at the end of `FUN_10029470` is:

```cpp
bVar3 = ((char)iVar1 * 2 ^ flags) & 2 ^ flags;
flags = ((char)iVar1 << 2 ^ bVar3) & 4 ^ bVar3;
```

Where `iVar1` is the requested enable state (`0` or `1`).

That means:

- bit `1` is set to the requested enable state
- bit `2` is also set to the requested enable state

So the best current interpretation is:

- bit `1` = current applied hook state
- bit `2` = synchronized desired/latched hook state used during thread-context repair

This matches `FUN_10029b80`, where:

- `param_3 == 0` forces comparison against disabled state
- `param_3 == 1` forces comparison against enabled state
- otherwise it uses `flag bit 2` as the desired-state reference

So bit `2` is not an unrelated mystery flag. It is a second state bit intentionally kept in sync with the requested hook state and used by the thread-repair helper.

## `FUN_100295e0`: suspend all peer threads and repair EIP

This helper does two jobs:

1. enumerate all threads in the current process except the current thread
2. suspend them, then call `FUN_10029b80(...)` for each suspended thread

Simplified:

```cpp
snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
collect_peer_thread_ids();
for each thread_id:
    hThread = OpenThread(...);
    SuspendThread(hThread);
    FUN_10029b80(hThread, hook_index_or_all, desired_state_selector);
```

Then `FUN_10029d00` later resumes them all.

So GWCA’s hook toggling is not a naive in-place write. It is a coordinated operation:

- acquire hook-system lock
- suspend peer threads
- translate any affected `EIP`s between original and replay domains
- patch/unpatch targets
- resume peer threads

## `FUN_100292f0`: bulk enable / bulk disable

This function toggles all hooks toward one target state:

```cpp
if (((entry.flags >> 1) & 1) != desired_state) {
    FUN_100295e0(thread_list, 0xffffffff, desired_state != 0);
    for each mismatched hook:
        FUN_10029470(index, desired_state);
    FUN_10029d00(thread_list);
}
```

Two important details:

- it only touches hooks whose current state differs from the requested one
- for the thread repair pass, it uses `param_3 = desired_state != 0`, so `FUN_10029b80` compares against a concrete state rather than consulting bit `2`

## `FUN_100293b0`: single-hook enable / disable

This is the per-hook wrapper:

- if `param_1 == 0`, it forwards to bulk toggle via `FUN_100292f0`
- otherwise it finds the specific hook entry by target pointer
- if already in requested state, it returns a state-specific status code
- else it suspends peer threads, toggles that one hook with `FUN_10029470`, then resumes threads

So this is the exported single-hook control path behind the tiny wrappers we already saw:

- `FUN_10029940(param_1)` -> `FUN_100293b0(param_1, 0)`
- `FUN_10029960(param_1)` -> `FUN_100293b0(param_1, 1)`

## `FUN_10029550`: hook-system lock

This helper is just a cooperative lock on `DAT_1008a474`:

- spin until lock becomes `0`
- set it to `1`
- back off with `Sleep(0)` / `Sleep(1)` behavior as contention grows

Every major hook-management function calls this first, then later clears the lock before returning.

So GWCA’s hook engine has explicit serialization around:

- create hook
- remove hook
- enable/disable hook
- shutdown

## What this means for the frame UI hook specifically

For the `SendFrameUIMsg` path:

1. installer allocates replay slot via `FUN_10029d60`
2. installer emits relocated replay code via `FUN_10029ed0`
3. compact entry stores:
   - original target
   - detour target `FUN_10026860`
   - replay slot
   - saved original bytes
4. `FUN_10029470(..., 1)` writes the live detour jump
5. `SendFrameUIMessage(...)` later replays through the slot at `DAT_1008a3a0`

So the frame hook path is now modeled end-to-end:

`scan -> install -> build replay slot -> suspend threads -> patch target -> callback mediation -> replay original through slot`

## Best next step

The next logical reverse pass is now:

1. identify which exported helpers map to:
   - initialize hook subsystem
   - shutdown hook subsystem
   - enable all hooks
   - disable all hooks
2. map the remaining neighboring live replay slots to their owning hook entries
3. if needed, inspect the special patch-mode hooks specifically, since `target-5` plus `EB F9` is now clearly a distinct hook style worth cataloging

At this point, though, the detour-write question is resolved.

We now know exactly which function writes GWCA’s live hook jumps and how it restores them.
