# GWCA GameThread Format Capability Consumers Addendum

This pass follows the next best `FUN_00689E90(...)` callers after [GWCA_GameThread_DXTNCapabilityBoundary_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTNCapabilityBoundary_Addendum.md):

- `FUN_0069F7F0(...)`
- `FUN_006903C0(...)`
- `FUN_006A19D0(...)`
- plus the already-reversed nearby `FUN_006A1710(...)`

The point of the pass was simple:

- if `DXTN`'s extra capability bit `0x200` matters outside the compact block builder
- these format-facing consumers are some of the best places to catch it

The result is useful even though it is not flashy:

- these callers keep reinforcing the same lower-bit split
- especially capability bit `0x8`
- and they still do **not** surface a broader `0x200`-driven mode outside the compressed DXT-family seam

## 1. `FUN_0069F7F0(...)`: BMP parser / internal-format mapper uses bit `0x8`, not `0x200`

`FUN_0069F7F0(...)` is a parser/validator for a BMP-like image buffer:

- requires magic `0x4D42` (`"BM"`)
- validates header size / offsets
- inspects bit depth and compression mode
- maps that image data into the engine's internal format ids

Its relevant capability use is:

```c
uVar3 = FUN_00689e90(*param_3);
if ((((uVar3 & 8) == 0) || ... ) && ... ) {
    ...
}
```

So once the BMP data has been mapped to an internal format id, the parser cares about:

- capability bit `0x8`

and not about:

- `0x200`

This is a strong negative data point, because `FUN_0069F7F0(...)` is format-facing and validation-heavy. If `0x200` were a broad import/export or generic image-layout mode, this would have been a plausible place to see it. It is not.

## 2. `FUN_006903C0(...)`: cached processor construction also keys off bit `0x8`

`FUN_006903C0(...)` is the `ImgMem`-side constructor/front door we already had near the cached transfer processor.

Its capability use is:

```c
uVar1 = FUN_00689e90(param_1);
if ((uVar1 & 8) == 0) {
  *param_4 = 0;
  param_4 = (undefined4 *)0x0;
}
...
FUN_006902c0(...)
```

So this constructor:

- gates whether an optional auxiliary object/plane is passed forward
- using capability bit `0x8`

Again, it does not key off:

- `0x200`

This matters because `FUN_006903C0(...)` is much closer to the reusable storage/transfer layer than the compressed block builder. If `DXTN`'s extra bit were a broad `ImgMem` or upload mode, we would expect to see it here before or alongside the compact-family code. We do not.

## 3. `FUN_006A1710(...)`: palette-table decode path is also bit-`8` driven

`FUN_006A1710(...)` continues the same theme.

Its relevant behavior is:

- read capability flags through `FUN_00689E90(format)`
- if `(flags & 8) != 0`
  - adjust one pointer path
  - and decode an extra 256-entry ARGB table from the tail of the buffer
- otherwise
  - synthesize a default identity-like palette

So this helper is another high-value example showing:

- capability bit `0x8` has visible effects in the image import/decode layer
- `0x200` still does not show up here

This fits the earlier understanding that lower capability bits are driving generic image-pipeline features, while `0x200` is still confined to the compressed-family branch we mapped around `DXTN`.

## 4. `FUN_006A19D0(...)`: palettized/export helper still keys off bit `0x8`

`FUN_006A19D0(...)` is another format-facing builder in the same neighborhood. It:

- allocates a headered output blob
- RLE-packs or row-packs source bytes
- and, when the format uses 8-bit indexed data, appends a 256-entry palette table

The important part is the palette decision:

```c
uVar3 = FUN_00689e90(param_3);
if ((uVar3 & 8) == 0) {
    // synthesize default 0..255 grayscale-like palette
} else {
    // copy supplied ARGB palette entries
}
```

Again:

- bit `0x8` matters
- `0x200` does not

So this is a fourth independent consumer reinforcing the same distinction.

## 5. What this means for `DXTN`

After this pass, the cleanest current boundary is:

### Lower capability bits are generic pipeline traits

The lower bits, especially:

- `0x8`
- `0x10`
- `0x20`

are showing up repeatedly in:

- BMP parsing
- DDS import/export
- `ImgMem` processor construction
- palette decode/export helpers
- transfer path selection

So those bits clearly belong to the general image-format system.

### The extra `0x200` bit still looks compressed-family-specific

By contrast, `DXTN`'s unusual:

- `0x200`

still has its strongest and clearest effect only in the compressed-block family seam:

- `FUN_0069E870(...)`
- and the corresponding compact-family routing already documented earlier

So the best current interpretation is:

- `0x200` is not a broad “generic image transfer” capability bit
- it still looks like a compressed-family discriminator

That is a stronger result than we had before this pass, because it is now supported by several independent negative examples from the general image pipeline.

## 6. Updated capability model

The current best model for `FUN_00689E90(...)` capability flags is now:

- low bits such as `0x8`, `0x10`, `0x20`
  - generic image-format / transfer / palette semantics
- grouped masks such as `0x210`, `0x280`
  - compressed-family routing / payload-structure decisions
- extra `0x200` on `DXTN`
  - currently best explained as a compressed-family variant selector, not a general pipeline mode

That keeps the model compact and fits all the reversed evidence so far.

## 7. Best next step

The strongest next step is no longer “more random `FUN_00689E90(...)` callers.” We have enough to make the broad boundary credible.

The highest-yield move now is to go back into the compressed-family side with this boundary in hand and trace:

- `FUN_0069CC40(...)` callers and siblings more deeply
- `FUN_0069C3F0(...)` neighbors
- and the remaining `FUN_0069E870(...)` / `FUN_0069E1C0(...)` adjacent helpers

That should answer the remaining sharper question:

- what exact compressed-family concept `DXTN`'s `0x200` branch corresponds to inside the DXT-style block system
