# GWCA GameThread Worker Triage Rules Appendix

This appendix is a short utility note for classifying a newly found transfer-bank worker quickly from call shape.

It is meant to complement:

- [GWCA_GameThread_WorkerSubtypeAppendix.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_WorkerSubtypeAppendix.md)
- [GWCA_GameThread_FamilyModeAppendix.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_FamilyModeAppendix.md)
- [GWCA_GameThread_TransferBank_Crosswalk.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBank_Crosswalk.md)

The goal is not to fully name the format on first contact.

The goal is to place the worker into the current taxonomy in about 30 seconds so later passes can start from the right bucket.

## Fast Classification Order

Use this order:

1. decide stage plane
2. decide worker subtype
3. if front-door worker:
   - record `block_kind`
   - record mode
4. record any unusual source-side shape

That order is usually faster and safer than guessing format identity from constants first.

## Rule 1: Check The Shared Front Door First

First question:

- does the worker call both:
  - `FUN_006A5F10(...)`
  - `FUN_006A5CB0(...)`

If yes:

- classify it as a front-door intermediate worker
- then continue with subtype triage below

If no:

- do **not** force it into the intermediate family buckets yet
- check whether it writes destination pixels directly instead

Why this works:

- current intermediate edit/repack workers all share the same expand/edit/repack seam
- current final materializers do not

## Rule 2: If It Writes Final Pixels Directly, Classify As Final Materializer

If the worker:

- builds ladders/endpoints locally
- walks destination rows directly
- writes final packed pixels or final destination words directly
- and does **not** route back through `FUN_006A5CB0(...)`

then classify it as:

- final materializer

Current examples:

- `FUN_00681860`
- `FUN_00681ED0`
- `FUN_006825A0`
- `FUN_00682980`
- `FUN_006833F0`

Practical note:

- once this classification is clear, stop trying to assign `block_kind` or mode
- those fields belong to the front-door intermediate plane, not the final `0x18` output plane

## Rule 3: For Front-Door Workers, Ask What Happens Between Expand And Repack

If the worker does call:

- `FUN_006A5F10(...)`
- worker-specific body
- `FUN_006A5CB0(...)`

then the fastest subtype question is:

- what is the body doing between expansion and repack?

There are currently three practical answers.

### 3A. Neighborhood Edit Worker

Classify as a neighborhood edit worker if the body mostly:

- expands one compact block
- samples nearby pixels or nearby rows
- blends or mixes in a local 4x4 neighborhood-shaped way
- repacks back into the same family

Common signs:

- local 16-entry working block buffer
- neighborhood-conditioned blending logic
- no obvious source-shape conversion step
- no coefficient matrix or other whole-block transform payload

Default assumption:

- if a front-door worker looks ordinary and local-neighborhood-shaped, it is probably this subtype first

Current examples:

- `FUN_00679D70`
- `FUN_0067A210`
- `FUN_0067AB50`
- `FUN_0067C670`
- `FUN_0067D360`

### 3B. Source Bridge Worker

Classify as a source bridge worker if the body mostly:

- starts from a source encoding that is not the usual compact family record
- derives or synthesizes a 16-entry working block from that alternate source
- then repacks into one of the standard intermediate families

Common signs:

- input looks like raw texels, words, or another source-side layout
- the worker feels like an adapter into the front-door family toolkit
- the unusual part is at the input side, not the per-pixel transform side

Current example:

- `FUN_006803F0`

### 3C. Transform Bridge Worker

Classify as a transform bridge worker if the body mostly:

- expands a normal intermediate family
- applies a whole-block transform across all 16 working pixels
- repacks into the same family

Common signs:

- external coefficient data, matrix terms, or policy vectors
- all sixteen working pixels rewritten in one systematic pass
- behavior looks more like remap/transform than neighborhood blending

Current example:

- `FUN_0068C310`

## Rule 4: Record `block_kind` And Mode Only After Subtype

Once the worker is confirmed as a front-door intermediate worker, record the exact seam call shape:

- `FUN_006A5F10(..., block_kind, mode)`
- `FUN_006A5CB0(..., block_kind, mode)`

Current fast map:

- `block_kind 1`
  - plain color-table intermediate family
- `block_kind 2`
  - nibble scalar/intensity companion family
- `block_kind 3`
  - endpoint-plus-selector scalar/color family

Current observed subtype spread:

- `block_kind 1`
  - neighborhood edit only so far
- `block_kind 2`
  - neighborhood edit only so far
- `block_kind 3`
  - neighborhood edit
  - source bridge
  - transform bridge

Practical note:

- family/mode is a second-pass refinement
- it should not override the earlier stage and subtype classification

## Rule 5: Treat Weird Source Shape As Metadata, Not As A Taxonomy Reset

If a worker looks unusual, record the unusual part explicitly:

- 16-bit source texels
- coefficient matrix
- upper DXT-family compact payload
- direct destination pixel stores

But do not restart the whole reasoning chain from scratch.

Use the unusual input shape as an annotation on top of:

- stage plane
- worker subtype
- `block_kind`
- mode

That keeps outliers like `FUN_006803F0` and `FUN_0068C310` inside the same taxonomy instead of making them look like unrelated subsystems.

## 30-Second Decision Tree

When a new transfer-bank worker appears, use:

1. does it call both `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`?
2. if no:
   - does it decode and write final destination pixels directly?
3. if yes:
   - final materializer
4. if it does use the front door:
   - does the body look like local neighborhood blending?
5. if yes:
   - neighborhood edit worker
6. otherwise:
   - does it bridge from an unusual source-side encoding?
7. if yes:
   - source bridge worker
8. otherwise:
   - does it apply a whole-block transform or coefficient-driven remap?
9. if yes:
   - transform bridge worker
10. then record:
   - `block_kind`
   - mode
   - unusual source-side shape

## Minimal Capture Template

For each new worker, capture only:

- function name
- front-door yes/no
- subtype
- `block_kind` if present
- mode if present
- one-line source-side shape
- one-line stage relation

Suggested template:

```text
FUN_00XXXXXX
- front-door: yes/no
- subtype: neighborhood edit | source bridge | transform bridge | final materializer
- block_kind: ?
- mode: ?
- source-side shape: ...
- stage relation: intermediate edit/repack | final 0x18 materialization | other
```

## Current Guardrails

Use these guardrails to avoid over-classifying:

- bank position alone is not enough
- helper-table reuse alone is not enough
- direct pixel stores strongly suggest final materializer
- shared `FUN_006A5F10(...) -> body -> FUN_006A5CB0(...)` strongly suggests intermediate worker
- a weird body does **not** automatically mean a new family

That last point matters most.

The current taxonomy already survived two major outliers:

- `FUN_006803F0`
- `FUN_0068C310`

So new discoveries should stay in the existing buckets unless the seam itself breaks.

## Best Next Step

The next practical use of this appendix is simple:

- take any remaining uncategorized caller of `FUN_006A5F10(...)` / `FUN_006A5CB0(...)`
- run this triage pass first
- only then spend time on exact format naming

That should make future reverse passes much faster and keep the transfer-bank notes additive instead of repetitive.
