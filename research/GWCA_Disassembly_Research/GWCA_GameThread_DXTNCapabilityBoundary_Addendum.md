# GWCA GameThread DXTN Capability Boundary Addendum

This pass follows the next logical question after [GWCA_GameThread_DXTNPlacement_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_DXTNPlacement_Addendum.md):

- what does `DXTN`'s extra capability bit `0x200` actually *do*?

The purpose of this pass was not to rediscover the DDS mapping or the compact-family branch. It was to find out whether the unusual `0x200` bit:

- only affects the compressed block builder
- or also changes the broader upload / import / export pipeline

## 1. Starting point: `DXTN` has capability word `0x201`

From the previous pass:

- `DXTN -> 0x16`
- its static descriptor row matches `DXTL`
- but its capability word is:
  - `0x201`

That differs from the other compressed families:

- `DXT1` -> `0x71`
- `DXT2 / DXT3 / DXT4 / DXT5` -> `0xB1`
- `DXTA` -> `0xA1`
- `DXTL` -> `0x11`

So the only obviously unique feature of `DXTN` at the capability-word level is the extra `0x200`.

## 2. Caller sweep result: most `FUN_00689E90(...)` consumers care about lower bits

The caller sweep for `FUN_00689E90(...)` turned up many users, but the already-reversed high-signal consumers continue to cluster around the lower capability bits:

- `0x08`
- `0x10`
- `0x20`
- and the grouped masks:
  - `0x210`
  - `0x280`

The strongest already-mapped examples are:

- `FUN_006A0630(...)`
  - checks `& 8` to reject one export family
- `FUN_006A1710(...)`
  - checks `& 8` to decide whether to apply a palette-table side path
- `FUN_00688A10(...)`
  - previously mapped as caring about `8`, `0x10`, and `0x20` in the upload / tiled-transfer path
- `FUN_0069E1C0(...)`
  - uses:
    - `& 0x210`
    - `& 0x280`
    to choose compressed block reconstruction branches
- `FUN_0069E870(...)`
  - uses:
    - `& 0x210`
    - `& 0x280`
    to choose compressed block emission branches

The important pattern is:

- the broad image-transfer and DDS-facing helpers are clearly structured around the lower capability bits
- the odd `0x200` behavior does **not** show up as a generic “special upload mode” in the already-reversed transfer stack

## 3. `FUN_006A1710(...)`: example of a non-`0x200` consumer

One useful spot-check in this pass was `FUN_006A1710(...)`, which is another `FUN_00689E90(...)` consumer in the DDS/palette orbit.

Its relevant behavior is:

- read capability word through `FUN_00689E90(format)`
- check:
  - `if ((uVar4 & 8) != 0) ...`
- use that bit to decide whether:
  - the source pointer is adjusted
  - and whether an extra 256-entry ARGB palette table is unpacked at the end

That is exactly the kind of branch that would have been a good candidate if `0x200` were a broad import/export mode bit.

But it is not using `0x200` at all.

So this is a good negative result:

- at least one additional DDS-adjacent consumer treats the lower capability bits as meaningful
- and ignores the extra `0x200` entirely

## 4. Where `0x200` *does* matter: the compressed block family seam

The strongest concrete effect of the `0x200` bit remains the one already visible in `FUN_0069E870(...)`.

For `DXTN = 0x16`, capability word `0x201` yields:

- `local_3c = 0x201 & 0x280 = 0`
- `local_18 = 0x201 & 0x210 = 0x200`

That pushes the builder into the branch:

- `FUN_0069CC40(...)`
- plus `FUN_0069C3F0(...)`

instead of the branches used by:

- `DXT2 / DXT3`
- or `DXT4 / DXT5 / DXTA / DXTL`

So the extra `0x200` bit is already proven to have one very real effect:

- it changes compact-family routing in the compressed block builder / rebuilder seam

## 5. Best current boundary statement

This pass supports a stronger and cleaner boundary statement than we had before:

- `DXTN`'s extra `0x200` bit is **not** currently visible as a generic upload, palette, or DDS export-mode bit in the broader image pipeline we have reversed so far
- the strongest known effect of `0x200` is still **inside the compressed block family routing seam**

In other words:

- `DXTN` is special first and foremost as a *compressed-family variant*
- not as a broadly different `ImgMem` / `ImgPal` transfer mode

That is a good reduction in uncertainty.

## 6. Updated interpretation of DXTN

Putting the last few passes together, the best current interpretation is:

- `DXTN`
  - is a real DDS-facing internal format id
  - shares static storage-layout characteristics with `DXTL`
  - has a distinct capability word because of the extra `0x200`
  - and that extra `0x200` currently appears to matter primarily by selecting a distinct compact-family branch:
    - `FUN_0069CC40(...)`
    - `FUN_0069C3F0(...)`

That makes `DXTN` look less like a general-purpose separate transfer format and more like:

- a specific compressed block-family variant inside the same broader image-format subsystem

## 7. What remains open

This pass narrows the uncertainty, but does not eliminate it completely.

Still open:

1. whether there are unreversed `FUN_00689E90(...)` consumers elsewhere that use `0x200` directly
2. whether `DXTN`'s `0x200` bit corresponds to a known external compressed-family concept, or to an engine-local subtype layered on top of the DXT-style group

So the current best claim is:

- `0x200` is not yet seen as a broad pipeline mode
- it is most strongly evidenced as a compact-family routing bit

## 8. Best next step

The next best step is to target the remaining unreversed `FUN_00689E90(...)` callers most likely to care about compressed-family distinctions, especially:

- `FUN_0069F7F0(...)`
- `FUN_006903C0(...)`
- `FUN_006A19D0(...)`
- `FUN_006A20A0(...)`
- `FUN_006A2410(...)`

Those are better next targets than more table dumping, because the main unresolved question is no longer “what is the static data?” It is:

- where else, if anywhere, the `0x200` bit changes behavior outside the already-mapped compact block seam
