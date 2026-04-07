# GWCA UIMessage Hook Chain Addendum

## Scope

This addendum continues the GWCA UI/frame-message reverse work after the live detour validation pass.

The specific question here was:

- what exactly is the relationship between the live patched game entrypoint, `FUN_10026860`, `SendFrameUIMessage`, and the runtime value stored at `gwca + 0x8A3A0`?

## The key result

The hook chain is now much tighter:

1. the live game `SendFrameUIMsg` entrypoint is patch-jumped into `gwca + 0x26860`
2. `gwca + 0x26860` is `FUN_10026860`
3. `FUN_10026860` is the inbound frame trampoline
4. it forwards into `GW::UI::SendFrameUIMessage(...)`
5. `SendFrameUIMessage(...)` replays the original/raw dispatcher through `DAT_1008a3a0`
6. `DAT_1008a3a0` is populated by `CreateHook(..., FUN_10026860, &DAT_1008a3a0)`

So the active frame hook path is now directly validated both statically and at runtime.

## New static decompilation: `SendFrameUIMessage`

I re-decompiled:

- `GW::UI::SendFrameUIMessage @ 0x100274d0`

The most important behavior is:

```cpp
if ((DAT_1008a3a0 == 0) || (param_1 == 0)) {
    return false;
}
...
Hook::EnterHook();
(*DAT_1008a3a0)(param_2, param_3, param_4);
Hook::LeaveHook();
```

That confirms `DAT_1008a3a0` is the callable replay target used when GWCA decides to forward the frame message onward.

This matters because the wrapper does **not** call the raw game entrypoint directly by a fixed symbol name. It calls whatever `CreateHook(...)` installed into `DAT_1008a3a0`.

## New static decompilation: UIModule bootstrap

I also re-decompiled:

- `FUN_10024690 @ 0x10024690`

The critical line is:

```cpp
if (DAT_1008a39c != 0) {
    GW::Hook::CreateHook((void **)&DAT_1008a39c, FUN_10026860, (void **)&DAT_1008a3a0);
}
```

That is the missing ownership statement for the frame-side replay pointer.

It means:

- `DAT_1008a39c` = scanned game-side target to patch
- `FUN_10026860` = detour body
- `DAT_1008a3a0` = out-trampoline/original replay pointer produced by the hook installer

So the strange live value seen at `gwca + 0x8A3A0` is not supposed to look like a normal named function in the image. It is expected to be a runtime-created replay stub / trampoline target.

## New static decompilation: companion global UI shim

The neighboring shim:

- `FUN_100268a0 @ 0x100268a0`

decompiles to:

```cpp
void __cdecl FUN_100268a0(UIMessage msgid, void *wParam, void *lParam)
{
  GW::Hook::EnterHook();
  GW::UI::SendUIMessage(msgid, wParam, lParam);
  GW::Hook::LeaveHook();
}
```

This further strengthens the pattern that GWCA keeps:

- one hook bridge for frame dispatch
- one hook bridge for global UI dispatch

in the same local trampoline cluster.

## Live + static combined chain

Putting the live dump together with the new decompiles gives this chain:

### Frame-side inbound path

1. game code enters `SendFrameUIMsg`
2. the original entrypoint is patched with a jump
3. that jump lands at `gwca + 0x26860`
4. `FUN_10026860` enters hook scope and sets the frame re-entry guard
5. it calls `GW::UI::SendFrameUIMessage((Frame*)((int)this - 0xA8), msgid, wParam, lParam)`

### Frame-side outbound replay path

1. `SendFrameUIMessage(...)` runs callbacks / block logic
2. if forwarding is allowed, it calls `DAT_1008a3a0`
3. `DAT_1008a3a0` is the replay/original trampoline installed by `CreateHook(...)`

### Bootstrap/install path

1. `FUN_10024690` scans the raw frame dispatcher into `DAT_1008a39c`
2. it calls `CreateHook((void**)&DAT_1008a39c, FUN_10026860, (void**)&DAT_1008a3a0)`
3. that produces the live patched front door plus replay pointer pair

## What this means for `+0x8A39C` vs `+0x8A3A0`

The two pointers now have a cleaner interpretation:

- `+0x8A39C`
  - cached original/raw game-side function address found by the scanner
  - this is the stable "real dispatcher" address in the game image

- `+0x8A3A0`
  - runtime-created replay/original trampoline produced by the hook installer
  - this is the callable continuation target GWCA uses after its own callback layer

That is why the two values diverge after `GW::Initialize`.

## Strongest current conclusion

The important conceptual correction is:

- `+0x8A3A0` is not just "the hooked function pointer"

More precisely, it is:

- the post-hook replay target that `SendFrameUIMessage(...)` invokes after GWCA callback mediation

That explains why:

- it becomes non-null only after hook setup
- it does not need to match the original game address
- it is the critical field checked by GWCA wrapper code

## Next logical reverse step

The best next decompile step after this is:

1. inspect the hook installer / trampoline machinery closely enough to characterize the replay stub layout
2. if a live client is running, dump bytes at the current `+0x8A3A0` value after full init
3. compare that runtime replay stub to the original game bytes at `+0x8A39C`
4. then move outward again to the game-side `FrMsg.cpp` class-routing helper

At this point the remaining ambiguity is no longer about GWCA’s high-level design.

It is about the exact runtime shape of the replay stub that `CreateHook(...)` builds for the frame dispatcher.
