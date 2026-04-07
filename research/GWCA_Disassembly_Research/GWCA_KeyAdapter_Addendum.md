# GWCA Key Adapter Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_StdFunction_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_StdFunction_Addendum.md)

The target here was the next logical step after proving the callback payloads are MSVC `std::function` machinery:

- follow the concrete key-adapter wrappers
- see how GWCA turns:
  - `function<void(HookStatus*, uint32_t)>`

into:

- `function<void(HookStatus*, Frame const*, UIMessage, void*, void*)>`

for the frame-message callback plane

The main bodies for this pass are:

- [decomp_10026bc0.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10026bc0.log)
- [decomp_10026cc0.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10026cc0.log)
- [decomp_10026370.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10026370.log)
- [range_10026900_10026d80_temp13.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\range_10026900_10026d80_temp13.log)

## Strongest new result: the key callback APIs are explicit adapter builders over frame-message callbacks

The compiled `gwca.dll` is now quite explicit about this.

### `RegisterKeydownCallback`

It does not register the user callback directly into some separate key system.

Instead it:

1. clones the incoming user callback:
   - `function<void(HookStatus*, uint32_t)>`
2. allocates a new `0x30` `_Func_impl_no_alloc<lambda_1>` object
3. stores the cloned user callback inside that lambda object
4. registers the lambda object through:
   - `RegisterFrameUIMessageCallback(...)`
5. targets message:
   - `0x20`
6. uses altitude:
   - `-0x8000`

### `RegisterKeyupCallback`

It is the same pattern, but targets:

- frame message `0x22`

So the binary model is:

- `RegisterKeydownCallback`
  - adapter to frame message `0x20`
- `RegisterKeyupCallback`
  - adapter to frame message `0x22`

This is the cleanest confirmation so far that the compiled build’s key interception surface is frame-message based.

## The adapter object type is visible in the wrapper body

The decompile shows the concrete implementation class being instantiated:

```text
std::_Func_impl_no_alloc<
    ... RegisterKeydownCallback ... ::<lambda_1>,
    void,
    HookStatus*,
    Frame const*,
    UIMessage,
    void*,
    void*
>
```

and equivalently for:

- `RegisterKeyupCallback`

That matters because it tells us the adapter is not hypothetical.
GWCA literally constructs a lambda-backed `std::function` object specialized to the frame-message callback signature.

So the wrapper is doing two nested callable steps:

1. clone the user-supplied key callback
2. wrap that clone into a frame-message lambda adapter

## The wrappers also expose the exact registration altitude

Both key wrappers pass:

- `-0x8000`

into `RegisterFrameUIMessageCallback(...)`.

That is important because it tells us the key-adapter lambdas are not just registered in the default callback phase.
They are installed at a very early altitude.

So the practical execution model is:

- raw frame message arrives
- the keyed adapter can run very early in the ordered callback bucket
- the adapter then translates the frame payload back into the user-facing key callback shape

## `Keypress` stays on the actuation side and matches the same frame plane

The current `Keypress` decompile also lines up nicely with this model.

`Keypress(ControlAction, Frame*)`:

1. builds a local control-action payload
2. resolves the default `"Game"` frame if the caller passed `nullptr`
3. sends frame UI message:
   - `0x20`
4. if successful:
   - queues follow-up work through `GameThread::Enqueue()`

So the same message family appears on both sides:

- actuation:
  - `Keypress -> SendFrameUIMessage(..., 0x20, ...)`
- interception:
  - `RegisterKeydownCallback -> RegisterFrameUIMessageCallback(..., 0x20, ...)`

That does not prove the lambda body semantics by itself, but it gives a very coherent compiled model.

## The wrapper bodies are enough to tighten the input-plane architecture

After this pass, the best compiled picture is:

### Public user-facing key callback API

- `function<void(HookStatus*, uint32_t)>`

### Internal GWCA adaptation step

- clone user callback into temporary `std::function` storage
- allocate `_Func_impl_no_alloc<lambda_1>` adapter object
- capture the cloned user callback inside the adapter

### Registration target

- `RegisterFrameUIMessageCallback`
- message:
  - `0x20` for keydown
  - `0x22` for keyup
- altitude:
  - `-0x8000`

### Dispatch plane

- frame UI message callback bucket

So the key callback layer is now substantially recovered even before separately decompiling the lambda operator body.

## Why this is still useful even without the separate lambda operator body

The wrapper decompiles answer several important questions already:

- where key callbacks actually register
- what message ids they bind to
- what altitude they use
- what concrete function-object class wraps them
- that the adapter is lambda-backed and capture-bearing

The remaining missing detail is only:

- the exact field-by-field unpacking performed inside the lambda operator body

That is narrower than the earlier uncertainty.

## Best current interpretation

The strongest conservative reading now is:

- the compiled GWCA build treats key callbacks as adapted frame-message callbacks
- the adapter is implemented as a captured lambda stored in `std::_Func_impl_no_alloc`
- the user callback is cloned and then invoked through that lambda
- keydown and keyup are separated only by:
  - bound frame message id (`0x20` vs `0x22`)

That is a meaningful step beyond just saying “key callbacks exist.”

## Best next step

The next logical decompile step is now very focused:

1. identify the concrete call operator body for:
   - `RegisterKeydownCallback::<lambda_1>`
   - `RegisterKeyupCallback::<lambda_1>`
2. recover:
   - which input payload field becomes the `uint32_t key`
   - whether the frame pointer or message id is inspected
   - how `HookStatus*` is forwarded

That should let us finish the key-adapter path end to end instead of stopping at the wrapper construction layer.
