# GWCA Toolbox `std::function` Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_CallableCluster_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_CallableCluster_Addendum.md)

The goal here was the next logical step after recovering the callback-record lifecycle:

- identify the concrete callable implementation type behind `record + 0x2C`
- determine whether GWCA uses a custom erased-callable family or ordinary MSVC `std::function` machinery
- verify that the single-record destroy helper is actually used for temporary callback objects during registration / unwinding

Fresh artifacts:

- RTTI/string scan from:
  - `rg -a "_Func|std::function|bad_function_call" ... gwca.dll`
- [findcallers_100065d0_temp11.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_100065d0_temp11.log)
- [decomp_callableunwind_temp12.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_callableunwind_temp12.log)

## Strongest new result: the callback-holder is MSVC `std::function` machinery, not a GWCA-custom callable family

The RTTI strings in the compiled `gwca.dll` are now strong enough to say this directly.

The binary contains type names for:

- `std::bad_function_call`
- `std::_Func_base<...>`
- `std::_Func_impl_no_alloc<...>`

including concrete specializations tied to the exact callback signatures we have been reversing, such as:

- UI-message callback signatures
- frame-message callback signatures
- zero-argument callback signatures
- concrete lambda specializations for:
  - `RegisterKeydownCallback`
  - `RegisterKeyupCallback`
  - `Keypress`
  - `SetPreference`
  - `MemFree`
  - other nearby helper lambdas

That means the best model is no longer:

- “GWCA has something like an erased callable”

It is:

- GWCA is using MSVC `std::function` / `_Func_base` / `_Func_impl_no_alloc` machinery, and the recovered callback-record protocol is wrapping those objects

## The recovered record layout matches `std::function`-style small-buffer storage

This lines up cleanly with the earlier field recovery:

```text
struct CallbackRecord30 {
    int32_t    altitude;           // +0x00
    HookEntry* hook_entry;         // +0x04
    uint8_t    function_sbo[0x24]; // +0x08 .. +0x2B
    void*      function_impl;      // +0x2C
}; // size 0x30
```

And the lifecycle helpers we already recovered now read very naturally:

- `function_impl == record + 0x08`
  - inline / SBO implementation
- `function_impl != record + 0x08`
  - external implementation object
- virtual slot `+0x04`
  - clone/copy into inline storage
- virtual slot `+0x10`
  - destroy/reset with inline-vs-external flag

That is exactly the shape you would expect from compiler-emitted `std::function` support.

## The unwind thunks prove `FUN_100065D0` is used for temporary callback-holder cleanup

This was the strongest remaining sanity check.

`FindCallers` for `FUN_100065D0` does not point back into the top-level registries directly.
Instead, it points to exception unwind helpers:

- `Unwind@10049b70`
- `Unwind@1004a308`
- `Unwind@1004a378`
- and two nearby siblings

The decompilation is very clean:

```text
void Unwind_10049b70(void)
{
    FUN_100065d0(EBP - 0x40);
}
```

and the same for the other two sampled unwind functions.

This matters because it shows:

- the compiler is treating `EBP - 0x40` as a stack-local object with non-trivial destruction
- the destructor for that local object is exactly the single-record callback-holder destroy helper we recovered

So `FUN_100065D0` is not just “a helper that happens to look like a record destructor.”
It is genuinely part of the exception-safe lifecycle for temporary function-holder objects in compiled GWCA registration paths.

## Practical interpretation

The cleanest high-confidence model after this pass is:

1. public GWCA registration APIs receive `std::function<signature>` arguments
2. temporary compiler-emitted `std::function`-style holder objects live on the stack
3. those objects are copied/cloned into `CallbackRecord30`
4. the callback record stores:
   - altitude
   - `HookEntry*`
   - small-buffer function object storage
   - active implementation pointer
5. cleanup is performed through the compiler-emitted `std::function` virtual-family

This means the callback subsystem is not reinventing callable erasure.
It is building registry/container logic around MSVC `std::function` internals.

## What this resolves

This pass closes several earlier uncertainties:

- it is not a custom GWCA-only callable-holder family
- it is not multiple unrelated payload object systems that merely resemble one another
- the record lifetime logic is compiler-consistent across:
  - normal cleanup
  - vector teardown
  - exception unwind cleanup

So the callback-record model is now both:

- structurally recovered
- type-family recovered

## Best current interpretation

The best conservative reading now is:

- `CallbackRecord30` embeds an MSVC `std::function` small-buffer holder beginning at `+0x08`
- `+0x2C` is the active `_Func_base` / `_Func_impl_*` implementation pointer
- the concrete implementations observed in RTTI are mostly:
  - `_Func_impl_no_alloc<...>`
- lambdas used by GWCA wrapper adapters get materialized as concrete `_Func_impl_no_alloc<lambda_...>` specializations

That is a much stronger and more specific statement than the earlier generic “erased callable” language.

## Best next step

The next logical decompile step is to follow the concrete adapter lambdas themselves, especially the ones already named by RTTI:

- `RegisterKeydownCallback` lambda
- `RegisterKeyupCallback` lambda
- `Keypress` lambda

That should let us move from:

- “this is `std::function` machinery”

to:

- “this specific lambda body adapts one public GWCA API into another callback plane”

which is the most useful next layer for the UI/input reverse thread.
