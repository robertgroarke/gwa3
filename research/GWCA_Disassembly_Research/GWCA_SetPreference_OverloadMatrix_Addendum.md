# GWCA SetPreference Overload Matrix Addendum

This pass closes the remaining logical gap in the `SetPreference(...)` lambda thread by finishing the overload family comparison.

Previously recovered:

- `StringPreference` off-thread lambda
- `NumberPreference` off-thread lambda

This pass adds:

- `EnumPreference` off-thread lambda
- `FlagPreference` off-thread lambda

The result is that the `SetPreference(...)` overload family now splits cleanly into two different deferred-lambda styles.

## Targets

Primary overloads:

- `GW::UI::SetPreference(EnumPreference, uint32_t) @ 0x10027A60`
- `GW::UI::SetPreference(FlagPreference, bool) @ 0x10027B90`

Supporting artifacts:

- [disasm_10027a60_temp28.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10027a60_temp28.log)
- [disasm_10027be0_temp31.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10027be0_temp31.log)
- [decomp_setpref_exactslots_temp34.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_setpref_exactslots_temp34.log)

## EnumPreference deferred lambda

The off-thread branch in `SetPreference(EnumPreference, uint32_t)` builds this object:

```asm
10027a76: PUSH 0x0
10027a78: SUB ESP,0x28
10027a7b: MOV EAX,ESP
10027a7d: MOV dword ptr [EAX],0x10054550
10027a83: MOV dword ptr [EAX + 0x4],ECX
10027a86: MOV dword ptr [EAX + 0x8],EDX
10027a89: MOV dword ptr [EAX + 0x24],EAX
10027a8c: CALL 0x10019D50
```

So the captures are:

- `+0x04 = EnumPreference`
- `+0x08 = uint32_t value`

Its vtable at `0x10054550` resolves to:

- `+0x00 -> 0x10028430`
- `+0x04 -> 0x10028430`
- `+0x08 -> 0x100285E0`
- `+0x0C -> 0x10028B80`
- `+0x10 -> 0x100284D0`
- `+0x14 -> 0x10001900`

The key slots are:

- `0x10028430`: clone/copy helper, copies the two captures
- `0x100285E0`: invoke body
- `0x10028B80`: RTTI getter for
  - ``bool __cdecl GW::UI::SetPreference(enum GW::UI::EnumPreference,unsigned int)::__l5::<lambda_1>``

The invoke body is simple:

```cpp
GW::UI::SetPreference(*(EnumPreference *)(param_1 + 4), *(uint *)(param_1 + 8));
```

So `EnumPreference` is a deferred replay wrapper.

## FlagPreference deferred lambda

The off-thread branch in `SetPreference(FlagPreference, bool)` builds this object:

```asm
10027bd3: PUSH 0x0
10027bd5: SUB ESP,0x28
10027bde: MOV ECX,ESP
10027be0: MOV dword ptr [ECX],0x100545C0
10027be6: MOV dword ptr [ECX + 0x4],EDI
10027be9: MOV dword ptr [ECX + 0x8],EAX
10027bec: MOV dword ptr [ECX + 0x24],ECX
10027bef: CALL 0x10019D50
```

So the captures are:

- `+0x04 = FlagPreference`
- `+0x08 = bool value` stored in a widened slot

Its vtable at `0x100545C0` resolves to:

- `+0x00 -> 0x10028450`
- `+0x04 -> 0x10028450`
- `+0x08 -> 0x100285F0`
- `+0x0C -> 0x10028B90`
- `+0x10 -> 0x100284D0`
- `+0x14 -> 0x10001900`

The key slots are:

- `0x10028450`: clone/copy helper, copies the two captures
- `0x100285F0`: invoke body
- `0x10028B90`: RTTI getter for
  - ``bool __cdecl GW::UI::SetPreference(enum GW::UI::FlagPreference,bool)::__l7::<lambda_1>``

The invoke body is also simple:

```cpp
GW::UI::SetPreference(*(FlagPreference *)(param_1 + 4), *(bool *)(param_1 + 8));
```

So `FlagPreference` is also a deferred replay wrapper.

## Comparison across all four overload families

At this point the `SetPreference(...)` overload behavior is:

### `EnumPreference`

- two-capture lambda
- deferred replay wrapper

### `FlagPreference`

- two-capture lambda
- deferred replay wrapper

### `StringPreference`

- two-capture lambda
- deferred replay wrapper

### `NumberPreference`

- one-capture lambda
- deferred task-body specialization
- invoke body contains real renderer-setting logic rather than replaying the public setter

So the overload family divides naturally into:

1. replay wrappers
   - enum
   - flag
   - string

2. task-body specialization
   - number

## Why NumberPreference is the outlier

This is the main architectural point from the overload matrix.

Three overloads use the same high-level deferred pattern:

- capture arguments
- queue lambda
- invoke body just calls the same public API on the game thread

But `NumberPreference` breaks that pattern:

- it does not merely replay `SetPreference(NumberPreference, value)`
- it runs a specialization-specific implementation that drives renderer values directly

That strongly suggests the overload split is not arbitrary compiler noise. It reflects a real API design difference in GWCA:

- enum/flag/string settings can safely replay through the public setter
- numeric preferences need a more specialized game-thread implementation path

## Relationship to the input-side lambdas

This also sharpens the broader lambda taxonomy.

We now have:

1. callback-plane adapters
   - keydown/keyup registration wrappers

2. deferred replay wrappers
   - `SetPreference(EnumPreference, uint32_t)`
   - `SetPreference(FlagPreference, bool)`
   - `SetPreference(StringPreference, wchar_t*)`

3. deferred choreography lambdas
   - `Keypress(ControlAction, Frame*)`
   - immediate `0x20`, deferred `0x22`

4. deferred task-body lambdas
   - `SetPreference(NumberPreference, uint32_t)`

5. cleanup/utility callables
   - grouped module-owned callback cleanup

## Most important takeaway

The `SetPreference(...)` overload family is now structurally complete in compiled form.

The cleanest conclusion is:

- enum, flag, and string overloads all use deferred replay-wrapper lambdas
- number is the deliberate outlier with a real task-body lambda

So the difference is not “string versus non-string” or “scalar versus pointer.”
The real split is:

- generic replayable preference setters
- one special numeric family with a hand-written game-thread implementation path
