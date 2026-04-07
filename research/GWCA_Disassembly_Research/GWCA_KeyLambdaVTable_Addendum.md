# GWCA Key Lambda VTable Addendum

## Scope

This pass continues from:

- [GWCA_KeyAdapter_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_KeyAdapter_Addendum.md)

The goal here was to move one level below the key wrapper functions and recover the actual lambda-backed `_Func_impl_no_alloc` vtable family used by:

- `RegisterKeydownCallback`
- `RegisterKeyupCallback`

Fresh artifacts:

- [disasm_10026c20_temp16.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10026c20_temp16.log)
- [disasm_10026d20_temp19.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10026d20_temp19.log)
- [decomp_keylambda_vtable_temp18.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_keylambda_vtable_temp18.log)
- [decomp_keyupdiff_temp20.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_keyupdiff_temp20.log)

## Strongest new result: the keydown and keyup adapters are sibling lambda-specialization vtables with one shared invoke body

The raw wrapper disassembly exposed the concrete vtable writes:

- keydown adapter vtable:
  - `0x100545F8`
- keyup adapter vtable:
  - `0x100545DC`

Dumping those tables shows they are almost identical:

### Keydown vtable `0x100545F8`

- `+0x00 -> 0x10028330`
- `+0x04 -> 0x10023300`
- `+0x08 -> 0x100285A0`
- `+0x0C -> 0x10028B60`
- `+0x10 -> 0x100284F0`
- `+0x14 -> 0x10028A80`

### Keyup vtable `0x100545DC`

- `+0x00 -> 0x100283B0`
- `+0x04 -> 0x10023300`
- `+0x08 -> 0x100285A0`
- `+0x0C -> 0x10028B70`
- `+0x10 -> 0x100284F0`
- `+0x14 -> 0x10028A80`

So the only differing slots are:

- clone/copy constructor:
  - `0x10028330` vs `0x100283B0`
- RTTI getter:
  - `0x10028B60` vs `0x10028B70`

The important consequence is:

- keydown and keyup use the same invoke path
- keydown and keyup use the same destroy path
- the semantic difference between them is mostly their concrete lambda type identity plus the fact that the wrapper binds them to different frame message ids (`0x20` vs `0x22`)

## Slot map for the key adapter lambda family

This is the tightest current slot interpretation:

### `+0x00` clone/copy-to-heap

- keydown:
  - `FUN_10028330`
- keyup:
  - `FUN_100283B0`

Both functions:

1. allocate `0x30`
2. write the specialization-specific vtable pointer
3. zero `+0x2C`
4. clone the captured callback payload from the source lambda object into `this + 0x08`

So this is the specialization-specific heap clone entry.

### `+0x04` auxiliary query

- `FUN_10023300`

This simply returns `0`.

The exact semantic label is still uncertain, but it is clearly shared and trivial.

### `+0x08` invoke

- `FUN_100285A0`

This is the most important slot in the family.

### `+0x0C` RTTI / target-type getter

- keydown:
  - `FUN_10028B60`
- keyup:
  - `FUN_10028B70`

These return the lambda specialization’s RTTI type descriptor.

### `+0x10` destroy

- `FUN_100284F0`

This destroys the captured callback payload at `+0x2C`, then optionally frees the heap object itself.

### `+0x14` target accessor

- `FUN_10028A80`

This returns:

- `this + 8`

which matches the inline callable/capture object location.

## `FUN_100285A0` is the shared invoke adapter body

This is the key new function in the pass.

Recovered body:

```text
void __thiscall
FUN_100285a0(this, param_1, param_2, param_3, param_4)
{
    param_4 = *(undefined4 **)*param_4;
    param_1 = (undefined4 *)*param_1;
    if (*(int **)(this + 0x2c) != 0) {
        captured_callback->vtable[+0x08](&param_1, &param_4);
        return;
    }
    fail_bad_function_call();
}
```

The exact ABI shape is a little compiler-noisy, but several things are clear:

1. this function is not the user callback itself
2. it is an adapter/invoke shim over the captured callback stored at:
   - `this + 0x2C`
3. it dereferences pieces of the incoming frame-callback argument pack before forwarding
4. it forwards through the captured callback’s invoke slot:
   - virtual slot `+0x08`
5. if no callback is present, it falls into the `bad_function_call` path

## Most likely payload mapping

This part is partly direct and partly inference.

Binary-confirmed:

- the adapter extracts two values from the incoming frame-callback argument pack
- one becomes the forwarded first argument
- one becomes the forwarded second argument
- it then invokes the captured user callback through its own function-object invoke slot

Best inference from the surrounding compiled context:

- the first forwarded argument is the incoming `HookStatus*`
- the second forwarded argument is the first dword of the frame payload, which corresponds to the key/control code

Why this is the strongest inference:

- the public callback type is:
  - `function<void(HookStatus*, uint32_t)>`
- the wrapper registers on:
  - `0x20` for keydown
  - `0x22` for keyup
- `Keypress` builds a local control-action payload whose first dword is the requested action id

So the shared invoke body is best understood as:

- unpack frame callback arguments
- extract `HookStatus*`
- extract key/control code from the message payload
- call the captured user callback

That is the first concrete end-to-end adapter body we have for the input callback layer.

## What the wrapper disassembly added

The raw wrapper disassembly also gave two useful direct anchors:

### Keydown wrapper

- writes vtable:
  - `MOV [ESI], 0x100545F8`

### Keyup wrapper

- writes vtable:
  - `MOV [ESI], 0x100545DC`

That eliminates any ambiguity about which lambda specialization object each wrapper constructs.

## Updated end-to-end key callback model

The strongest current compiled model is now:

1. user supplies:
   - `function<void(HookStatus*, uint32_t)>`
2. wrapper clones that callback
3. wrapper allocates a lambda-backed `_Func_impl_no_alloc`
4. wrapper installs a specialization-specific vtable
5. wrapper registers that adapter on the frame callback plane:
   - `0x20` or `0x22`
   - altitude `-0x8000`
6. when invoked:
   - shared adapter body `FUN_100285A0`
   - unpacks the frame callback argument pack
   - forwards `HookStatus*` plus key/control code into the captured user callback

That is substantially deeper than the earlier wrapper-only model.

## Most important takeaway

The most important new fact is not just that the wrappers allocate `_Func_impl_no_alloc`.

It is that:

- keydown and keyup are sibling specialization vtables
- both share one invoke adapter body
- that invoke body forwards the captured callback after unpacking the frame-message argument pack

So the key callback path is now effectively recovered from:

- public API
- wrapper construction
- vtable identity
- invoke slot
- destroy slot

## Best next step

The next logical reverse step is to compare this key-adapter pattern against the other lambda-backed adapters already visible in RTTI, especially:

- `Keypress`
- `SetPreference`
- `MemFree`

and, within the UI/input thread specifically:

- try to identify whether the frame payload unpacked by `FUN_100285A0` is always the first dword or whether some control families use a different extraction convention
