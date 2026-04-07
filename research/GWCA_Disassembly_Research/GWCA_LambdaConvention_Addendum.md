# GWCA Lambda Convention Addendum

## Scope

This pass continues from:

- [GWCA_KeyLambdaVTable_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_KeyLambdaVTable_Addendum.md)

The question here was the next best one after recovering the keydown/keyup lambda vtables:

- is the key-adapter lambda layout a one-off

or

- is it part of a broader GWCA convention for lambda-backed `_Func_impl_no_alloc` objects

The main comparison target was the off-thread path in:

- `GW::UI::SetPreference(NumberPreference, uint32_t)`

because that path visibly builds a stack-local lambda object and passes it to:

- `GameThread::Enqueue()`

Fresh artifacts:

- [decomp_setpreference_temp21.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_setpreference_temp21.log)
- [disasm_10028150_temp22.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10028150_temp22.log)
- [decomp_prefvtable_temp23.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_prefvtable_temp23.log)

## Strongest new result: the key adapters are part of a broader GWCA lambda/vtable convention, but not every lambda family uses the same payload shape

The number-preference async lambda is a strong comparison point because it is clearly not a frame-message callback adapter.

The off-thread branch in `SetPreference(NumberPreference, uint32_t)` eventually builds a stack-local lambda object with:

- vtable:
  - `0x10054588`

The raw disassembly shows:

```text
MOV [EAX], 0x10054588
MOV [EAX + 0x04], ESI
MOV [EAX + 0x24], EAX
CALL 0x10019D50   ; GameThread enqueue path
```

So unlike the key adapters, this lambda carries:

- a compact inline captured payload
- no frame-message argument unpacking
- a direct task body that runs later on the game thread

That already tells us two important things:

1. the key adapters are not unique in using `_Func_impl_no_alloc` lambda families
2. the exact payload convention depends on the lambda’s job

So the right model is:

- shared compiler/runtime callable convention
- specialization-specific payload layout and invoke behavior

not:

- one universal field convention for all lambdas beyond the generic `_Func_impl_no_alloc` shell

## The number-preference async lambda vtable family

Dumping vtable `0x10054588` gives:

- `+0x00 -> 0x100284B0`
- `+0x04 -> 0x100284B0`
- `+0x08 -> 0x10028630`
- `+0x0C -> 0x10028BC0`
- `+0x10 -> 0x10001910`
- `+0x14 -> 0x10001900`

That is a real sibling family to the key adapter vtables, but it is not identical.

### Comparison to keydown / keyup

Key adapters:

- `+0x00` clone allocates `0x30`
- `+0x08` invoke unpacks a frame-callback argument pack
- `+0x10` destroy tears down captured callback payload at `+0x2C`
- `+0x14` returns `this + 8`

Number-preference async lambda:

- `+0x00` and `+0x04` are the same helper
- `+0x08` is a direct task body
- `+0x10` is trivial free-if-owned
- `+0x14` returns `this + 4`

So the key family and the number-preference family share the same broad slot roles, but their object size and capture layout differ.

## Slot interpretation for the number-preference async lambda

### `+0x00` / `+0x04` copy helper

- `FUN_100284B0`

Recovered body:

```text
void __thiscall FUN_100284b0(this, out)
{
    out[0] = number_pref_lambda_vftable;
    out[1] = *(this + 0x04);
}
```

So this lambda’s meaningful capture is just:

- one dword at `this + 0x04`

That matches the raw stack construction in the wrapper.

### `+0x08` invoke/task body

- `FUN_10028630`

This is the real semantic body.

It reads the captured number-preference id from:

- `param_1 + 0x04`

then:

1. queries the current graphics/preference value through:
   - `DAT_1008A3FC`
2. switches on the captured preference id
3. calls `GW::Render::SetGraphicsRendererValue(...)` in several cases

So this lambda is not adapting another callback plane.
It is a deferred game-thread task object whose invoke body directly performs the actual preference-side render updates.

### `+0x0C` RTTI getter

- `FUN_10028BC0`

Returns the RTTI type descriptor for:

- `SetPreference(NumberPreference, uint32_t)::__l2::<lambda_2>`

### `+0x10` destroy

- `FUN_10001910`

This just frees the object if the “delete this” flag is set.

Unlike the key adapter lambda family, there is no nested captured callback at `+0x2C` to tear down.

### `+0x14` target accessor

- `FUN_10001900`

Returns:

- `this + 4`

which matches the simple one-dword capture layout.

## What this proves about GWCA’s lambda convention

This comparison is useful because it separates:

### Shared convention

- lambda-backed `_Func_impl_no_alloc` vtable families
- recognizable slot structure:
  - copy/clone
  - invoke
  - RTTI/type
  - destroy
  - target accessor

### Specialization-specific behavior

- object size
- capture layout
- invoke semantics

The key adapters are one specialization:

- capture another callback object
- unpack frame-callback args
- forward into user callback

The number-preference async lambda is another specialization:

- capture one preference id
- run real task logic on the game thread

So the broader rule is:

- same compiler-generated callable shell
- different bodies and capture layouts depending on use

## Relationship to `SetPreference(StringPreference, wchar_t*)`

The decompile of `SetPreference(StringPreference, wchar_t*)` also fits this story.

Its off-thread branch builds a stack-local `_Func_impl_no_alloc<lambda_1>` and then calls:

- `GameThread::Enqueue()`

So although I did not fully recover that vtable family in this pass, it clearly belongs to the same general pattern:

- lambda object materialized locally
- queued as a deferred task
- later executed on the game thread

That means GWCA is using the same underlying `std::function` machinery for both:

- callback-plane adaptation
- deferred task execution

## Updated interpretation

The best current model after this pass is:

1. GWCA uses MSVC `_Func_impl_no_alloc` lambda families per specialization
2. the slot structure is broadly stable across families
3. the capture payload layout is specialization-specific
4. the key adapters are one specialization among several, not an isolated custom mechanism

So the key-adapter invoke pattern:

- unpack incoming args
- forward selected pieces into captured callback

is a property of that lambda family, not a universal rule for all GWCA lambdas

## Most important takeaway

The biggest correction from this pass is:

- “forward first dword of payload” is not a general GWCA lambda rule

What is general is:

- GWCA relies heavily on lambda-backed `_Func_impl_no_alloc` objects
- those objects follow a shared slot/vtable convention
- each specialization then defines its own capture layout and invoke semantics

That makes the key adapter path both:

- representative of the tooling style
- special in its frame-message unpacking behavior

## Best next step

The next logical reverse step is to stay on the UI/input thread but compare one more adapter family that is closer to the key path than deferred settings work, ideally:

- the `Keypress`-adjacent lambda family if present
- or one of the `SetPreference(StringPreference, wchar_t*)` deferred lambdas

The goal would be to map one more concrete specialization end to end so we can build a small taxonomy:

- callback-plane adapters
- deferred game-thread task lambdas
- utility/cleanup lambdas
