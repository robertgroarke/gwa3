# GWCA Hook Installer Addendum

## Scope

This addendum captures the next logical reverse step after the live frame-detour validation:

- move from the live detour target itself into the hook installer path that creates the replay/original trampoline used by GWCA

The main question here was:

- how does GWCA turn a scanned target plus detour into the runtime replay pointer seen at `gwca + 0x8A3A0`?

## `GW::Hook::CreateHook`

I re-decompiled `GW::Hook::CreateHook`, which resolves to:

```cpp
int __cdecl GW::Hook::CreateHook(void **target_ptr, void *detour, void **out_original)
{
  void *normalized;
  int result;

  if ((target_ptr != 0) && (*target_ptr != 0)) {
    normalized = (void *)Scanner::FunctionFromNearCall((uint)*target_ptr, false);
    if (normalized != 0) {
      *target_ptr = normalized;
    }
    result = FUN_10029730(*target_ptr, detour, out_original);
    return result;
  }
  return -1;
}
```

That is important because it shows `CreateHook` itself is not the real installer.

It does two things:

1. normalize/canonicalize the target through `Scanner::FunctionFromNearCall(...)`
2. delegate the actual hook construction to `FUN_10029730(...)`

So any real understanding of the replay pointer has to live in `FUN_10029730`, not in `CreateHook` itself.

## What that means for the frame dispatcher path

From the already recovered UI bootstrap:

```cpp
GW::Hook::CreateHook((void **)&DAT_1008a39c, FUN_10026860, (void **)&DAT_1008a3a0);
```

Combining that with the `CreateHook` decompile gives the real chain:

1. `DAT_1008a39c` is the scanned raw frame dispatcher target
2. `CreateHook(...)` normalizes it if needed
3. `FUN_10029730(target, FUN_10026860, &DAT_1008a3a0)` installs the detour
4. the installer returns a replay/original callable through `DAT_1008a3a0`

That fits the live behavior we already validated:

- the front door is patched into `FUN_10026860`
- `SendFrameUIMessage(...)` forwards onward through `DAT_1008a3a0`

## Why this matters for `+0x8A3A0`

This decompile sharpens the meaning of `+0x8A3A0`.

It is not:

- just a copy of the original game function
- just another named GWCA export

It is:

- the out-parameter produced by the internal hook installer
- the replay/original continuation target used after GWCA callback mediation

So the most likely possibilities are:

- an allocated trampoline stub
- a rewritten continuation entry
- or a managed hook-record entry point that eventually replays saved original bytes and returns to the remainder of the target

## Relationship to the existing live findings

The live validation already showed:

- game `SendFrameUIMsg` is patched to jump into `gwca + 0x26860`
- `gwca + 0x26860` is `FUN_10026860`
- `gwca + 0x8A39C` still holds the original/raw game-side dispatcher address
- `gwca + 0x8A3A0` becomes a different runtime value after init

`CreateHook` now explains why those last two diverge:

- `+0x8A39C` is the scanner-discovered raw target
- `+0x8A3A0` is the installer-produced replay handle

That is exactly the distinction we needed.

## Most important remaining unknown

The one unresolved piece is still the exact runtime shape of the object/stub pointed to by `+0x8A3A0`.

The logic is now clear, but the concrete representation is not yet.

That means the next best reverse step is:

1. characterize `FUN_10029730` in more detail
2. dump the live bytes at the replay pointer after full init
3. compare that replay stub to the original bytes at the raw target

At that point we should be able to say not just that `+0x8A3A0` is a replay handle, but what kind of replay handle GWCA actually builds.
