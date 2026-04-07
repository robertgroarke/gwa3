# GWCA Keypress Lambda Addendum

This pass takes the next logical step after the string-preference lambda work:

- recover the queued follow-up that `GW::UI::Keypress(...)` sends to `GameThread::Enqueue()`
- determine whether it is another `_Func_impl_no_alloc` family
- compare it against the key callback adapters and the deferred `SetPreference(...)` families

The answer is yes: `Keypress` has its own deferred lambda family, and it is very input-specific.

## Targets

Primary wrapper:

- `GW::UI::Keypress(ControlAction, Frame*) @ 0x10026370`

Supporting artifacts:

- [decomp_10026370.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10026370.log)
- [disasm_100263d8_temp24.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_100263d8_temp24.log)
- [decomp_keypresslambda_temp26.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_keypresslambda_temp26.log)

## High-level `Keypress` model

The already-known compiled body for `Keypress` is:

1. build a `ControlAction` packet locally
2. resolve the default `"Game"` frame if needed
3. send frame message `0x20`
4. if that succeeds, queue a follow-up task with `GameThread::Enqueue()`

This pass resolves step 4.

## Queued object materialization

The raw disassembly after the successful `SendFrameUIMessage(..., 0x20, ...)` shows:

```asm
100263dc: PUSH 0x0
100263de: SUB ESP,0x28
100263e1: MOV EAX,ESP
100263e3: MOV dword ptr [EAX],0x10054534
100263e9: MOV dword ptr [EAX + 0x4],EDI
100263ec: MOV dword ptr [EAX + 0x8],ESI
100263ef: MOV dword ptr [EAX + 0x24],EAX
100263f2: CALL 0x10019D50
```

So the queued follow-up is a stack-local lambda-backed callable with:

- vtable `0x10054534`
- capture `+0x04 = ControlAction`
- capture `+0x08 = Frame*`
- the usual self/manager slot at `+0x24`

That is the same broad `_Func_impl_no_alloc` object pattern we saw in the other families.

## Vtable family

The vtable at `0x10054534` resolves to:

- `+0x00 -> 0x10028310`
- `+0x04 -> 0x10028310`
- `+0x08 -> 0x10028530`
- `+0x0C -> 0x10028B50`
- `+0x10 -> 0x100284D0`
- `+0x14 -> 0x10001900`

That is the same slot pattern now seen repeatedly across GWCA lambda families:

- clone/copy
- clone/copy alias
- invoke
- RTTI getter
- destroy
- target accessor

## Slot meanings

### `0x10028310`: clone/copy helper

`FUN_10028310` is the expected capture copier:

```cpp
*param_1 = keypress_lambda_vftable;
param_1[1] = *(this + 0x04);
param_1[2] = *(this + 0x08);
```

So this family captures two values:

- `ControlAction`
- `Frame*`

### `0x10028530`: invoke body

This is the important payload:

```cpp
void __fastcall FUN_10028530(int param_1)
{
    Frame* frame = *(Frame**)(param_1 + 8);
    ControlAction action = *(ControlAction*)(param_1 + 4);
    local_packet = { action, 0, 0 };

    if (frame == 0) {
        resolve default "Game" frame again;
    }

    GW::UI::SendFrameUIMessage(frame, 0x22, &local_packet, 0);
}
```

So the queued follow-up is the input-release half of the `Keypress` actuation:

- immediate path sends `0x20`
- queued lambda later sends `0x22`

This is much more specific than the earlier high-level description of “queued follow-up work.”

### `0x10028B50`: RTTI getter

`FUN_10028B50` returns the type descriptor for:

- ``bool __cdecl GW::UI::Keypress(enum GW::UI::ControlAction, struct GW::UI::Frame*)::__l2::<lambda_1>``

So the family is explicitly tied to `Keypress`, not a generic shared input helper.

### `0x100284D0` and `0x10001900`

These are the same familiar shared helpers:

- `0x100284D0`: trivial free-if-owned destroy helper
- `0x10001900`: returns `this + 4`

## What this changes

This pass upgrades the compiled `Keypress` model from:

- send `0x20`
- queue something

to:

- send `0x20` immediately
- queue a lambda that sends `0x22` later

That means compiled `Keypress` is not merely a wrapper over a single input message. It is a two-stage input choreography with explicit deferred release.

## Relationship to the other lambda families

### Versus keydown/keyup callback adapters

The keydown/keyup registration lambdas are callback-plane adapters:

- they unpack incoming frame-callback args
- they forward `HookStatus*` and key data into user callbacks

The `Keypress` lambda is not adapting callback input.
It is an actuation lambda:

- capture action + frame
- later emit release message `0x22`

### Versus `SetPreference(StringPreference, wchar_t*)`

The string-preference lambda is a deferred replay wrapper:

- capture API args
- replay the same public setter on the game thread

The `Keypress` lambda is similar in structure but not in semantics:

- it does not replay `Keypress(...)`
- it performs the second half of the input sequence directly by sending `0x22`

### Versus `SetPreference(NumberPreference, uint32_t)`

The number-preference lambda is a deferred task-body specialization with real branch logic.

The `Keypress` lambda sits between the two `SetPreference` styles:

- more concrete than a pure replay wrapper
- simpler than the renderer-setting task-body lambda

## Updated taxonomy

After this pass, the lambda taxonomy is stronger:

1. callback-plane adapters
   - example: keydown/keyup callback registration wrappers
   - behavior: unpack callback arg pack and forward into user callback

2. deferred replay wrappers
   - example: `SetPreference(StringPreference, wchar_t*)`
   - behavior: capture API args and replay public setter later

3. deferred choreography lambdas
   - example: `Keypress(ControlAction, Frame*)`
   - behavior: capture actuation state and perform the second stage later

4. deferred task-body lambdas
   - example: `SetPreference(NumberPreference, uint32_t)`
   - behavior: run specialization-specific game-thread work

5. cleanup/utility callables
   - example: grouped UI callback cleanup callable
   - behavior: module-owned registry cleanup

## Most important takeaway

The `Keypress` path is now fully interpretable in compiled form:

- `0x20` is the immediate press/down actuation
- `0x22` is the deferred release/up actuation
- the release half is driven by a real `_Func_impl_no_alloc` lambda family tied specifically to `Keypress`

So the input layer is using the same compiler-generated callable shell as the rest of GWCA, but in a distinctly input-oriented way: not callback adaptation, not settings replay, but staged input choreography.
