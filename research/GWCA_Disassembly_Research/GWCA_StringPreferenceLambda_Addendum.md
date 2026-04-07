# GWCA StringPreference Lambda Addendum

This pass takes the next logical step after the key-adapter and number-preference lambda work:

- recover the concrete deferred-task lambda used by `GW::UI::SetPreference(StringPreference, wchar_t*)`
- compare its slot family against the already-recovered key-adapter and number-preference families

The result is a cleaner lambda taxonomy for this `gwca.dll` build.

## Targets

Primary function:

- `GW::UI::SetPreference(StringPreference, wchar_t*) @ 0x10028150`

Supporting artifacts:

- [decomp_setpreference_temp21.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_setpreference_temp21.log)
- [disasm_10028190_temp25.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10028190_temp25.log)
- [decomp_stringpref_vtable_temp27.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_stringpref_vtable_temp27.log)

## Off-thread branch shape

The decompile already showed the high-level pattern:

- verify module/game-state guards
- if not in the game thread, build a stack-local lambda-backed callable
- enqueue it with `GameThread::Enqueue()`
- otherwise call the real preference setter directly through `DAT_1008A3F0`

The new disassembly pins down the local object layout exactly:

```asm
1002817d: MOV ECX,[EBP + 0xC]
10028180: PUSH 0x0
10028182: SUB ESP,0x28
10028185: MOV EAX,ESP
10028187: MOV dword ptr [EAX],0x100545A4
1002818D: MOV dword ptr [EAX + 0x4],ESI
10028190: MOV dword ptr [EAX + 0x8],ECX
10028193: MOV dword ptr [EAX + 0x24],EAX
10028196: CALL 0x10019D50
```

So the deferred lambda object is:

- vtable at `0x100545A4`
- captured `StringPreference` at `+0x04`
- captured `wchar_t*` at `+0x08`
- self/manager slot at `+0x24`

That is already a useful contrast with the number-preference family, which only captured one scalar at `+0x04`.

## Vtable family

The vtable at `0x100545A4` decodes to:

- `+0x00 -> 0x10028490`
- `+0x04 -> 0x10028490`
- `+0x08 -> 0x10028620`
- `+0x0C -> 0x10028BB0`
- `+0x10 -> 0x100284D0`
- `+0x14 -> 0x10001900`

This is the same broad slot pattern seen in the other recovered GWCA lambda families:

- copy/clone helper
- copy/clone helper alias
- invoke body
- RTTI/type descriptor getter
- destroy/free helper
- target accessor

## Slot meanings

### `0x10028490`: clone/copy helper

`FUN_10028490` writes the same vtable and copies the two captures:

```cpp
*param_1 = string_pref_lambda_vftable;
param_1[1] = *(this + 0x04);
param_1[2] = *(this + 0x08);
```

So this family is a two-capture `_Func_impl_no_alloc` specialization.

### `0x10028620`: invoke body

`FUN_10028620` is very clean:

```cpp
GW::UI::SetPreference(
    *(StringPreference *)(param_1 + 4),
    *(wchar_t **)(param_1 + 8));
```

This confirms the async lambda is just a deferred replay wrapper:

- read captured preference id
- read captured string pointer
- call the same public setter again on the game thread

That is different from the number-preference deferred lambda, whose invoke body contained additional renderer-setting logic rather than a simple replay.

### `0x10028BB0`: RTTI getter

`FUN_10028BB0` returns the type descriptor for:

- ``bool __cdecl GW::UI::SetPreference(enum GW::UI::StringPreference,wchar_t*)::__l7::<lambda_1>``

That keeps matching the MSVC `std::function` / `_Func_impl_no_alloc` model recovered in the earlier passes.

### `0x100284D0`: destroy/free helper

`FUN_100284D0` is a trivial free-if-owned destroy helper:

```cpp
if (param_1 != '\0') {
    FUN_1002B48D(this);
}
```

This matches the same general destroy contract we saw in the neighboring lambda families.

### `0x10001900`: target accessor

`FUN_10001900` returns:

- `this + 4`

Again, this matches the usual MSVC callable-holder target-access pattern already seen in the previous work.

## Comparison with the other families

### Versus keydown/keyup adapters

The key adapter families were callback-plane adapters:

- registered onto frame message `0x20` / `0x22`
- invoked from the frame callback plane
- unpacked an incoming frame-callback arg pack
- forwarded selected pieces into the captured user callback

The string-preference family is not doing any argument adaptation at invoke time.
It is a deferred task wrapper:

- capture arguments now
- replay the same public API later on the game thread

So the key path is still special in its unpacking behavior.

### Versus number-preference deferred lambda

The number-preference family was also a deferred task lambda, but its invoke body was heavier:

- read one captured numeric preference id
- compute/query the current value through GWCA state
- call `GW::Render::SetGraphicsRendererValue(...)` in specialization-specific switch cases

The string-preference family is simpler:

- capture two values
- directly replay `GW::UI::SetPreference(StringPreference, wchar_t*)`

That means even within the deferred-task category, GWCA uses more than one specialization style:

- replay wrapper lambdas
- task-body lambdas with real implementation logic

## Updated taxonomy

After this pass, the best current lambda taxonomy is:

1. callback-plane adapters
   - example: keydown/keyup wrappers
   - behavior: unpack callback args, forward into captured user callback

2. deferred replay wrappers
   - example: `SetPreference(StringPreference, wchar_t*)`
   - behavior: capture API args, replay the public setter on the game thread

3. deferred task-body lambdas
   - example: `SetPreference(NumberPreference, uint32_t)`
   - behavior: capture state, run specialization-specific game-thread work

4. cleanup/utility callables
   - example: the earlier grouped UI callback cleanup callable
   - behavior: module-owned cleanup over callback registries

## Most important takeaway

The key-adapter family still is not a universal GWCA lambda convention.

What is universal is the compiler-generated shell:

- `_Func_impl_no_alloc`-style vtable family
- stable slot pattern
- specialization-specific capture layout
- specialization-specific invoke semantics

This string-preference pass makes that much harder to misread, because it gives us a second fully recovered deferred-task family and shows that even two nearby `SetPreference(...)` overloads use different invoke-body styles.
