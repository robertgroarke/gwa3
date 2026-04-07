# GWCA UIMessage Live Detour Addendum

## Scope

This addendum records the live-runtime validation work done after the static GWCA UI-message decompilation pass.

The key goal was to connect:

- the live Guild Wars client
- the injected `gwca.dll`
- the already decompiled GWCA hook/trampoline functions

into one verified chain.

## Target client

The runtime work used the local Guild Wars install associated with:

- `L I L B I S C U I T`

The runtime dump helper for this pass is:

- [live_gwca_runtime_dump.ps1](c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/scripts/live_gwca_runtime_dump.ps1)

The captured dump report is:

- [live_gwca_runtime_report.txt](c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWA%20Censured/tests/live_gwca_runtime_report.txt)

## Important execution note

The injection helper had to be run under 32-bit PowerShell:

- `C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe`

Using 64-bit PowerShell caused `LoadLibraryW` injection to fail, because the local `kernel32.dll` export address was the wrong bitness for a 32-bit `Gw.exe` target.

## Live runtime values

From the successful live dump:

- `PID = 20116`
- `GwBase = 0x00D30000`
- `GwcaBase = 0x6F3F0000`
- `Game SendFrameUIMsg = 0x00F586D0`
- `Game GetChildFrame = 0x00F3E2B0`
- `Game RootFrame = 0x00F5DC20`

After `Scanner::Initialize` and `GW::Initialize`:

- `gwca + 0x8A39C = 0x00F586D0`
- `gwca + 0x8A3A0 = 0x0A1B0EE0`
- `gwca + 0x8A37C = 0x00F3E2B0`
- `gwca + 0x8A410 = 0x00F5DC20`
- `gwca + 0x8A3B0 = 0x014E5624`

This confirms live that:

- `+0x8A39C` is still the cached original/raw game dispatcher pointer
- `+0x8A3A0` is the active hooked/mediated pointer populated by GWCA init

## New live finding: the game front door is patched

The live bytes at the game `SendFrameUIMsg` entrypoint were:

```text
00F586D0: E9 8B E1 4B 6E ...
```

That is a near jump. Resolving the relative target:

- source = `0x00F586D0`
- target = `0x00F586D0 + 5 + 0x6E4BE18B`
- target = `0x6F416860`

Since:

- `GwcaBase = 0x6F3F0000`

the detour target is:

- `0x6F416860 - 0x6F3F0000 = 0x26860`

So the live-patched game entrypoint jumps directly to:

- `gwca + 0x26860`
- static function name: `FUN_10026860`

## Why that matters

This is the strongest runtime validation so far because it connects the live client to an already decompiled GWCA function, instead of just matching offsets by guesswork.

Earlier static decompilation had already identified:

- `FUN_10026860` as the inbound frame-message trampoline
- `FUN_100268a0` as the matching global `SendUIMessage` shim

The live detour now proves that `FUN_10026860` is not just adjacent infrastructure. It is the actual live hook destination owning the patched game `SendFrameUIMsg` front door after `GW::Initialize`.

## Decompiled meaning of the live detour target

The previously recovered decompile for `FUN_10026860` is:

```cpp
void __thiscall FUN_10026860(void *this, UIMessage msgid, void *wParam, void *lParam)
{
  GW::Hook::EnterHook();
  DAT_1008a350 = 1;
  GW::UI::SendFrameUIMessage((Frame *)((int)this + -0xa8), msgid, wParam, lParam);
  DAT_1008a350 = 0;
  GW::Hook::LeaveHook();
}
```

That means the live runtime chain is now concrete:

1. game code calls the original `SendFrameUIMsg` entrypoint
2. the entrypoint is patched with a jump
3. the jump lands in `gwca + 0x26860`
4. `FUN_10026860` enters hook scope
5. it flips the frame re-entry guard flag
6. it converts `this` back to a `Frame*` by subtracting `0xA8`
7. it routes the call through GWCA's `SendFrameUIMessage(...)`
8. it clears the guard and leaves hook scope

That is a much tighter statement than the earlier purely static model.

## Companion shim next to the live detour target

The neighboring function `FUN_100268a0` decompiles as:

```cpp
void __cdecl FUN_100268a0(UIMessage msgid, void *wParam, void *lParam)
{
  GW::Hook::EnterHook();
  GW::UI::SendUIMessage(msgid, wParam, lParam);
  GW::Hook::LeaveHook();
}
```

This strongly suggests the pair is intentional:

- `0x26860` = frame-dispatch detour path
- `0x268a0` = global UI-message detour/shim path

So GWCA’s live hook layer appears to centralize both halves of the UI system in one small trampoline cluster.

## Runtime implication for the hook model

Before this live pass, the hook model was:

- `DAT_1008a39c` = raw frame dispatcher target
- `DAT_1008a3a0` = original/trampoline replay pointer
- `FUN_10026860` = detour

After this live pass, we can state it more concretely:

- the game entrypoint itself is visibly patched
- the patched front door lands at the detour body we decompiled
- the cached original pointer at `+0x8A39C` remains intact alongside the live patch

So the next logical reverse step is no longer broad.

It is:

1. keep following the live detour cluster around `gwca + 0x26860`
2. compare its actual bytes and callers to the `CreateHook(...)` setup path
3. map the exact ownership of `+0x8A3A0 = 0x0A1B0EE0`
4. then move back outward to the game-side `FrMsg.cpp` helper / class-routing question

## Related artifacts

- [GWCA_UIMessage_Research.md](c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWCA_UIMessage_Research.md)
- [test_gwca_inject_research.au3](c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWA%20Censured/test_gwca_inject_research.au3)
- [gwca_research_state.png](c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWA%20Censured/tests/gwca_research_state.png)
