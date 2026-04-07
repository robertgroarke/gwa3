# Local GW Image And GWCA Injection Guide

## Purpose

This guide explains the local workflow already present in this workspace for:

- finding a local Guild Wars client image and launching it
- attaching the BotsHub/GWA2 framework to that process
- injecting `gwca.dll`
- understanding which initialization paths are safe
- executing GWCA or native UI work on the game thread instead of a random remote thread

This is a source-backed guide based on the code and research notes already in the repo, not a generic Windows injection tutorial.

## Short version

The working model in this workspace is:

1. Launch a local GW client from `Accounts.json` through `GWLauncher`.
2. Attach BotsHub with `InitializeGameClientForGWA2(False)`.
3. Let BotsHub scan the game, publish labels like `FrameArray`, and install the rendering hook / command queue.
4. Inject `gwca.dll` with `LoadLibraryW` through `CreateRemoteThread`.
5. Do not blindly call `GW::Initialize()` if you still need BotsHub's rendering hook.
6. For UI work like button clicks, execute through the rendering hook queue so the code runs on the game thread.
7. If using the manual GWCA path, populate the GWCA data section with known game pointers first.

The biggest practical lesson is this:

- `CreateRemoteThread(LoadLibraryW)` is fine for loading `gwca.dll`.
- `CreateRemoteThread` is not the right way to execute Guild Wars UI dispatch functions like `SendFrameUIMsg`.
- UI message execution needs game-thread context, and this workspace solves that with BotsHub's rendering hook queue.

## Where the local game image path comes from

The workspace does not hardcode one global `gw.exe` path. Instead, the launch path is account-driven.

### Accounts source

`GWA Censured/Accounts.json` is the local account store described in `CLAUDE.md`.

Important:

- it contains `gwpath`, character names, and credentials
- do not print or copy the credentials
- use it as a structured source of launch metadata only

`GWLauncher_LoadAccounts()` loads that JSON and returns account maps with keys including:

- `character`
- `email`
- `gwpath`
- `password`
- `extraargs`
- `elevated`
- `title`
- `Name`

`GWLauncher_LaunchAccount()` then reads the selected account's `gwpath` and launches that exact local client image.

So, in this repo, "local game images" really means "the per-account `gwpath` values stored in `Accounts.json` and launched through `GWLauncher`."

## Launch flow already implemented here

The launcher flow is already built for you in `GWA Censured/lib/custom/GWLauncher.au3`.

Relevant functions:

- `GWLauncher_LoadAccounts()`
- `GWLauncher_FindAccountByCharacter()`
- `GWLauncher_LaunchAccount()`
- `GWLauncher_AutoLaunchAndConnect()`
- `GWLauncher_ClickPlay()`
- `_GWLauncher_GetModuleBase()`
- `GWLauncher_InjectDLL()`

### What `GWLauncher_AutoLaunchAndConnect()` does

The automated path is:

1. Load accounts from `Accounts.json`
2. Find the requested character
3. Launch the configured `gwpath`
4. Wait for the client to reach login / char select
5. Scan and connect to the process
6. Call `InitializeGameClientForGWA2(False)`
7. Handle reconnect dialog or Play logic

That means the launcher side and the BotsHub attachment side are already integrated locally.

### What `_GWLauncher_GetModuleBase()` means here

`_GWLauncher_GetModuleBase($hProcess)` reads the target process PEB with `NtQueryInformationProcess`, reads `ImageBaseAddress`, and then adds `0x1000`.

That returned value is not the raw PE image base. It is:

- image base plus `0x1000`
- effectively the `.text` start used by this codebase

This matters because some of the test scripts compute the real GW image base as:

```text
$pe_sections_ranges[0][0] - 0x1000
```

So there are two related values used in the repo:

- raw image base
- `.text` base / image base plus `0x1000`

Be careful not to mix them up when translating offsets.

## BotsHub attachment: what `InitializeGameClientForGWA2(False)` actually does

The real attachment/bootstrap routine is in `GWA Censured/lib/botshub/GWA2_Assembly.au3`.

`InitializeGameClientForGWA2($changeTitle = True)` performs this sequence:

1. `RegisterScanPatterns()`
2. `ResolveAssertionsPatterns()`
3. `ExecutePatternScan()`
4. `MapScanResultsToLabels()`
5. `ModifyMemory()`
6. `InitializeCommandStructures()`

This is the core of the working local framework.

### Why this step matters before GWCA injection

After `InitializeGameClientForGWA2(False)` succeeds, the workspace has:

- a connected process handle
- scanned game pointers and function labels
- a command queue
- the rendering hook path used at char select
- useful labels such as `FrameArray`, `QueueCounter`, `QueueBase`, and many command entry points

This is what turns "I can open the process" into "I can safely execute code on the game thread."

### Important labels/patterns from the scan stage

The local scanner registers patterns for both game functions and infrastructure, including:

- `UIMessage`
- `FrameArray`
- `Render`
- `Action`
- `PreGame`
- `Dialog`
- `SetDifficulty`
- `EnterMission`

`FrameArray` is especially important because multiple GWCA manual-population flows reuse the BotsHub-resolved `FrameArray` label as GWCA's frame hash table pointer.

## The command queue and rendering hook

The rendering hook is the key reason the local approach works.

In `GWA2_Assembly.au3`, the render hook is scanned and then the injected assembly builds a queue-processing path through `RenderingModProc`.

The repo comments and tests make two important points:

- the old `GameTick` hook path was removed because it was unstable
- queue processing at char select is handled by `RenderingModProc`

In practice, the queue is used like this:

1. write shellcode or a command block into remote memory
2. enqueue its address
3. let the rendering hook execute it from inside the game thread

That is why so many test scripts verify the rendering hook before doing anything interesting.

## Safe vs unsafe operations

This repo distinguishes very clearly between what is safe to do from a foreign thread and what must run on the game thread.

### Safe from `CreateRemoteThread`

These are treated as safe or at least intentionally used in the workspace:

- `LoadLibraryW` to inject `gwca.dll`
- `Scanner::Initialize(gwImageBase)` in some research paths
- simple non-UI setup work that only reads memory or initializes scanner state

### Unsafe from `CreateRemoteThread`

These are documented as the wrong execution path:

- direct `SendFrameUIMsg`
- button click paths that eventually depend on Guild Wars UI dispatch
- more generally, in-client UI work that expects game-thread-owned state or locks

`CharSelect_ButtonClick_Research.md` explains the failure mode directly:

- calling `SendFrameUIMsg` from a foreign thread deadlocks or does nothing useful because the UI system expects game-thread context

That is why the repo keeps routing final UI actuation through the rendering hook queue.

## DLL injection already implemented locally

There are two main local implementations of DLL injection.

### 1. General helper: `GWLauncher_InjectDLL()`

`GWLauncher_InjectDLL($hProcess, $dllPath)` in `GWLauncher.au3` performs the classic pattern:

1. resolve `LoadLibraryW`
2. `VirtualAllocEx` remote memory for the DLL path
3. `WriteProcessMemory` the Unicode path
4. `CreateRemoteThread(LoadLibraryW, remotePath)`
5. wait
6. free the temporary path buffer

Use this when you want the generic "inject a DLL into the local GW client" primitive already wrapped in the launcher library.

### 2. Manual injection in the research/test scripts

The test files use the same primitive inline, especially:

- `GWA Censured/test_gwca_inject.au3`
- `GWA Censured/test_gwca_inject_research.au3`
- `GWA Censured/test_gwca_scanner_only.au3`
- `GWA Censured/test_gwca_manual_ptrs.au3`
- `GWA Censured/test_gwca_gamethread_init.au3`
- `GWA Censured/test_gwca_click_stable.au3`

Those are the best examples when you want working sequences rather than just a generic helper.

## Three GWCA initialization strategies present in this workspace

The repo has explored three distinct ways to load/use GWCA.

### Strategy A: Inject `gwca.dll` only

This is:

- `LoadLibraryW`
- no `Scanner::Initialize`
- no `GW::Initialize`

Use case:

- stable manual-population path
- char-select click experiments
- minimized interference with BotsHub hooks

This is the strategy that the later research converged on for stable button-click work.

### Strategy B: Inject + `Scanner::Initialize` only

This is the path in `test_gwca_scanner_only.au3`.

Sequence:

1. inject `gwca.dll`
2. compute the GW image base
3. call `gwcaBase + 0x21850` as `Scanner::Initialize(gwImageBase)`
4. explicitly skip `GW::Initialize`

Goal:

- populate scanner-derived pointers without installing GWCA's full hook set

This is safer than full GWCA initialization if you still need BotsHub's rendering hook, but it was not the final stable char-select click solution by itself.

### Strategy C: Inject + `Scanner::Initialize` + `GW::Initialize`

This is the full GWCA path tested in `test_gwca_inject_research.au3`.

Sequence:

1. inject `gwca.dll`
2. call `Scanner::Initialize`
3. call `GW::Initialize`

Important local finding:

- this tends to break BotsHub's rendering hook because GWCA installs overlapping hooks

That is why the research repeatedly warns:

- do not call `GW::Initialize()` if you still need the BotsHub rendering queue

## Why `GW::Initialize()` is a problem here

The repo's local research is consistent on this point:

- GWCA's full initialize path hooks functions that BotsHub also depends on
- that hook overlap breaks `RenderingModProc` queue processing

`GWCA_ButtonClick_Research.md` summarizes the effect:

- fresh client + first BotsHub init: rendering hook works
- after `GW::Initialize`: rendering hook no longer works
- after `gwca.dll` load + scanner only: rendering hook still works

So the practical rule in this codebase is:

- if your workflow depends on BotsHub queue execution, do not fully initialize GWCA unless you are deliberately testing that interaction

## The current best working pattern: manual GWCA population plus game-thread execution

The best documented working pattern in the workspace is the one distilled in:

- `GWCA_ButtonClick_Research.md`
- `test_gwca_manual_ptrs.au3`
- `lib/custom/GWA2_FrameUI.au3`

### Core idea

Instead of calling full `GW::Initialize()`, the repo:

1. injects `gwca.dll`
2. manually writes the critical game function pointers into GWCA's data section
3. calls GWCA button helpers from code executed on the game thread
4. neutralizes the risky wrapper pointer after the click
5. unloads or stops using `gwca.dll`

### Critical GWCA data offsets used locally

The local code and notes repeatedly use these offsets in `gwca.dll`:

- `+0x8A39C` = original `SendFrameUIMsg`
- `+0x8A3A0` = hooked/current `SendFrameUIMsg`
- `+0x8A37C` = `GetChildFrame`
- `+0x8A410` = `RootFrame`
- `+0x8A3B0` = frame hash table / `FrameArray`

### Game-side offsets reused by the workspace

The local manual path uses these fixed game offsets:

- `game_base + 0x2286D0` = `SendFrameUIMsg`
- `game_base + 0x20E2B0` = `GetChildFrame`
- `game_base + 0x22DC20` = `RootFrame`

And for the frame table:

- `GetLabel('FrameArray')`

### Why `+0x8A3A0` is the critical one

The local reverse-engineering found that GWCA's `SendFrameUIMessage` wrapper checks the pointer at `gwcaBase + 0x8A3A0`.

If that pointer is null:

- the wrapper returns false
- the click silently fails

That means writing only the "original" send pointer to `+0x8A39C` is not enough for this manual path.

The working manual-population path writes the game `SendFrameUIMsg` address to both:

- `+0x8A39C`
- `+0x8A3A0`

## Why execution must still go through the rendering hook

Even after manual population, the actual button click is still not launched via a raw remote thread.

The working pattern is:

1. build shellcode in remote memory
2. queue it
3. let BotsHub's rendering hook execute it on the game thread

This is the key difference between:

- "DLL injection worked"
- "the actual Guild Wars UI call worked"

Those are not the same thing.

## Stable char-select click pattern already captured locally

The stable version documented in `GWCA_ButtonClick_Research.md` is:

1. launch a fresh client
2. run `InitializeGameClientForGWA2(False)`
3. inject `gwca.dll` via `LoadLibraryW`
4. skip `Scanner::Initialize` and skip `GW::Initialize`
5. manually populate GWCA data:
   - `+0x8A39C = game_base + 0x2286D0`
   - `+0x8A3A0 = game_base + 0x2286D0`
   - `+0x8A37C = game_base + 0x20E2B0`
   - `+0x8A410 = game_base + 0x22DC20`
   - `+0x8A3B0 = GetLabel('FrameArray')`
6. call `gwca + 0x255E0` (`ButtonClick`) from shellcode queued through the rendering hook
7. zero `gwca + 0x8A3A0` after the call
8. optionally unload `gwca.dll` later with `FreeLibrary`

### Why zeroing `+0x8A3A0` matters

The local notes explain that after the click succeeds, leaving GWCA's send wrapper path armed can lead to crashes later because:

- GWCA's hook/callback state was not fully initialized
- but its wrapper code can still try to use that uninitialized state

So the stable pattern is:

- temporarily arm `+0x8A3A0`
- perform the one needed action
- zero it again immediately afterward

This is one of the most important repo-specific findings.

## The native no-GWCA alternative also present in this workspace

This workspace also has a native frame-click path in `GWA Censured/lib/custom/GWA2_FrameUI.au3`.

That file contains two useful ideas:

### 1. Manual GWCA setup helper

`_InitGWCAForButtonClick()` does exactly the manual-population pattern:

- inject `gwca.dll`
- do not call `GW::Initialize`
- populate the critical data pointers

### 2. Native click path without GWCA

`ClickFrameButton()` builds the `kMouseAction` structure itself and calls the game's `SendFrameUIMsg` through queue-executed shellcode.

That means the repo has two ways to solve the same family of problem:

- use GWCA helpers after manual population
- bypass GWCA and call the native frame dispatcher directly

If the user goal is simply "click a frame button reliably," the native path can be simpler than keeping a partially initialized GWCA module alive.

## Recommended practical workflows

### Workflow 1: Launch a local client and attach the framework

Use this when the first question is "how do I get a usable local process?"

1. Load accounts with `GWLauncher_LoadAccounts()`
2. Find the desired character with `GWLauncher_FindAccountByCharacter()`
3. Launch with `GWLauncher_LaunchAccount()`
4. Wait for the client to reach char select
5. `SelectClient(...)`
6. `InitializeGameClientForGWA2(False)`

Outcome:

- the client is live
- BotsHub is attached
- scanner labels and the queue are ready

### Workflow 2: Research-safe GWCA injection

Use this when you want to inspect GWCA loading without destabilizing the BotsHub queue.

1. Attach BotsHub first
2. Inject `gwca.dll` with `GWLauncher_InjectDLL()` or the inline test logic
3. Optionally call `Scanner::Initialize(gwImageBase)`
4. Do not call `GW::Initialize()`
5. Read back GWCA data-section pointers to confirm state

This is what `test_gwca_scanner_only.au3` demonstrates.

### Workflow 3: Stable one-shot GWCA button click

Use this when you want to trigger a GWCA button helper like the Play button path.

1. Attach BotsHub
2. Verify the rendering hook is active
3. Inject `gwca.dll`
4. Skip full GWCA initialization
5. Manually write the critical GWCA data pointers
6. Queue shellcode that calls `gwca + 0x255E0`
7. Zero `+0x8A3A0`
8. unload or stop using `gwca.dll`

This is the repo's most mature GWCA-assisted UI path.

### Workflow 4: Native frame click without GWCA

Use this when you want the end result, not specifically GWCA.

1. Attach BotsHub
2. Resolve frame pointer and context
3. Build the `kMouseAction` packet yourself
4. Queue native `SendFrameUIMsg` shellcode through the rendering hook

This is what `GWA2_FrameUI.au3` is evolving toward.

## Minimal conceptual recipe

If you want the shortest mental model, it is this:

```text
Accounts.json -> GWLauncher_LaunchAccount -> running gw.exe
running gw.exe -> InitializeGameClientForGWA2(False) -> scans + labels + rendering queue
rendering queue -> safe game-thread execution point
LoadLibraryW -> gwca.dll loaded
manual GWCA pointer population -> GWCA frame helpers become usable
queued shellcode -> GWCA/native UI action runs on game thread
```

## What files to read first

If you want to reproduce or extend the local workflow, start with these files in this order:

1. `CLAUDE.md`
2. `GWA Censured/lib/custom/GWLauncher.au3`
3. `GWA Censured/lib/botshub/GWA2_Assembly.au3`
4. `GWA Censured/test_gwca_scanner_only.au3`
5. `GWA Censured/test_gwca_manual_ptrs.au3`
6. `GWCA_ButtonClick_Research.md`
7. `CharSelect_ButtonClick_Research.md`
8. `GWA Censured/lib/custom/GWA2_FrameUI.au3`

## Practical cautions

### Credentials

Do not dump or paste account credentials from `Accounts.json`.

### Hook conflicts

If the rendering hook stops consuming the queue after GWCA setup, suspect `GW::Initialize()` first.

### Thread context

A successful remote thread is not evidence that the actual game UI action was valid. Guild Wars UI work is thread-sensitive in this repo's testing.

### Base-address confusion

Keep straight whether a script is using:

- raw image base
- image base plus `0x1000`
- a label published by BotsHub
- a module-relative offset inside `gwca.dll`

### Cleanup

If you arm GWCA's `SendFrameUIMessage` path by writing `+0x8A3A0`, clean it up afterward in the same workflow.

## Bottom line

Yes, the current workspace already contains everything needed to work with local Guild Wars images and inject GWCA into them.

The important point is that there are really two separate problems:

1. loading code into the process
2. executing Guild Wars UI logic in the right thread/context

This repo already solves both, but with different mechanisms:

- loading is done with classic `LoadLibraryW` remote-thread injection
- execution is done through BotsHub's rendering-hook queue

If you follow the local patterns, the safest practical route is:

- launch through `GWLauncher`
- attach with `InitializeGameClientForGWA2(False)`
- inject `gwca.dll`
- avoid full `GW::Initialize()` unless you are explicitly testing hook interactions
- execute UI work on the game thread through the queue

