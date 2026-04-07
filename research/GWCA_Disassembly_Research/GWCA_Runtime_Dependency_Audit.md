# GWCA Runtime Dependency Audit

Updated: 2026-03-30

## Scope

Live runtime code under `GWA Censured/lib/` was checked for remaining `gwca.dll` dependencies and direct reads/writes against GWCA's data section.

## Current state

The active FrameUI click path is already native:

- [`GWA2_FrameUI.au3`](/c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib/custom/GWA2_FrameUI.au3#L762) `ClickFrameButton()` calls the game's `SendFrameUIMsg` directly.
- [`GWA2_FrameUI.au3`](/c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib/custom/GWA2_FrameUI.au3#L976) `ClickFrameByPtr()` also uses the native game function directly.
- `GetFrameByHash()` reads the scanned `FrameArray` label rather than GWCA state.

## Remaining GWCA-coupled code

Only one live helper in `lib/` still depends on `gwca.dll`:

- [`GWA2_FrameUI.au3`](/c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib/custom/GWA2_FrameUI.au3#L201) `_InitGWCAForButtonClick()`

That helper still:

- injects `gwca.dll`
- writes `+0x8A39C`
- writes `+0x8A3A0`
- writes `+0x8A37C`
- writes `+0x8A410`
- writes `+0x8A3B0`

The related constants are declared at:

- [`GWA2_FrameUI.au3`](/c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib/custom/GWA2_FrameUI.au3#L180)

## Interpretation

This means the runtime codebase is much closer to GWCA-free than the older research notes suggest:

- Native scan/bootstrap is already the default path.
- The remaining GWCA dependency is isolated to a legacy compatibility helper.
- The next safe cleanup step after validating `test_no_gwca.au3` is removing `_InitGWCAForButtonClick()` and its unused constants, or replacing it with a native no-op/init shim if any callers still reference it.
