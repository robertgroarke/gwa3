# GWCA Toolbox Subobject Correction Addendum

## Scope

This pass follows the “constructor target” thread from:

- [GWCA_Toolbox_ListenerConstructor_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ListenerConstructor_Addendum.md)

The target was:

- `0x100172B0`

The important result is not just more detail.
It is a **correction** to the prior mental model.

## Important correction: `0x100172B0` is not safely interpretable as `ProgressBar::GetValue` here

One Ghidra project labels `0x100172B0` as:

- `GW::ProgressBar::GetValue(void)`

Artifact:

- [decomp_100172b0.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_100172b0.log)

But the raw bytes and full body shape around `0x100172B0` contradict that interpretation for this call site.

Raw disassembly shows:

- a real stack argument at `[EBP+8]`
- object state at `this + 0x24`
- virtual calls through offsets `+0x00`, `+0x04`, and `+0x10`
- inline/local temporary storage at `[EBP-0x38]`
- ownership transfer / destruction logic

That is **not** the shape of a trivial frame `GetValue()` wrapper.

So for this seam, the safer conclusion is:

- the symbol/signature at `0x100172B0` is misleading in at least one analyzed project
- the raw body behavior is the stronger authority

## What the body at `0x100172B0` actually looks like

The most relevant raw behavior:

1. read a source object from `[EBP+8]`
2. load `source[0x24]`
3. if non-null, call its vtable slot `+0x00` into local stack storage
4. compare the returned object against inline local storage
5. swap/update `this[0x24]`
6. if old callable state exists:
   - call vtable slot `+0x04`
   - later call vtable slot `+0x10` with a boolean flag

That is much closer to:

- assignment/copy/move of a small-buffer-optimized callable/object holder

than to:

- “query progress bar value”

## Strongest semantic interpretation: the subobject at `ListenerRecord + 0x08` is function-object style storage

This now fits the listener-record story much better than the earlier “maybe a UI control subobject” reading.

From the hidden constructor:

- `ECX = ListenerRecord + 0x08`
- `PUSH &local`
- `CALL 0x100172B0`

That matches a pattern like:

- construct/assign callable-holder state into the embedded subobject

And it lines up with the rest of the listener lifecycle:

- `+0x2C` is another optional callback/context pointer consulted later
- nearby helpers use explicit `MemFree` cleanup glue
- removal/teardown calls through virtual slots that look like holder destruction hooks

So the best current model is:

```text
struct ListenerRecord {
    uint32_t key;              // +0x00
    uint32_t zero_04;          // +0x04
    CallableHolder payload;    // +0x08 .. +0x2B
    void* callback_ctx;        // +0x2C
};
```

Where `CallableHolder` is some compiled function-object / erased-callback storage, not a plain frame wrapper.

## Why this is a good fit for the surrounding code

This corrected model explains three things at once:

1. why the constructor zeroes the whole `+0x08 .. +0x2C` region before calling into `0x100172B0`
2. why removal consults `+0x2C` and then frees the whole record
3. why the exported-heavy seam can iterate records and invoke behavior indirectly before delegating to the final target

In other words:

- the listener vector is probably storing interception handlers
- not just passive metadata records

## Nearby UI-control methods are still real, but probably unrelated to this exact call site

The surrounding range around `0x10017290 .. 0x10017340` does contain genuine frame-control methods in existing Ghidra output, for example:

- [EditableTextFrame::GetValue](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10017290.log)
- [SliderFrame::GetValue](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_100172e0.log)
- [DropdownFrame::HasValueMapping](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10017340.log)

So the region itself is definitely UI/frame-heavy.

But for `0x100172B0` specifically, the raw call/ownership pattern is a better clue than the inherited symbol name.

## Best current interpretation

The strongest updated interpretation after this pass is:

- the hidden listener registry is storing callback-handler records
- the embedded subobject at `+0x08` is likely an erased callable / function-object holder
- the integer key at `+0x00` selects which handler record to find
- `+0x2C` is an auxiliary callback/context slot used during listener iteration/removal

That is a much stronger semantic model than “map-window-adjacent registry with unknown payload.”

## Best next step

The next logical pass is to chase the inserter/caller that prepares the source object passed into `0x100172B0`.

That should answer:

1. what callable is being wrapped
2. what the integer key represents
3. which exported-heavy operation these listener records are intercepting

At this point, the shortest path forward is no longer the subobject internals alone.
It is the call site that packages the source object before assignment into `ListenerRecord + 0x08`.
