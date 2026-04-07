# Plan: Push GWA3 to Private GitHub Repo for Codex Code Review

## Goal

Extract the `gwa3/` subtree (55 commits, 96 files) into a standalone private GitHub repo so that OpenAI Codex can review Pull Requests via its GitHub integration.

## Current State

| Item | Value |
|---|---|
| Monorepo | `c:\Users\Robert\Documents\GWA Censured X BotsHub` (local only, no remote) |
| gwa3 location | `gwa3/` subdirectory — C++ DLL project (GWCA3) |
| Commits touching gwa3 | 55 |
| Tracked files | 96 (.h, .cpp, CMakeLists.txt, tests, tools) |
| Sensitive data in gwa3? | No — no credentials, no account data, no AutoIt bot logic |
| GitHub CLI | Authenticated as `robertgroarke`, scopes: repo, workflow, gist, read:org |

---

## Phase 1: Create the Private GitHub Repo

```bash
# Create private repo (no local clone yet)
gh repo create robertgroarke/gwa3 --private --description "GWA3 — Guild Wars Client API (C++ DLL)" 
```

## Phase 2: Extract gwa3 History into a Standalone Branch

Two options — pick one:

### Option A: Subtree Split (preserves only gwa3 commits, rewrites paths)

```bash
# From the monorepo root:
git subtree split --prefix=gwa3 -b gwa3-standalone

# This creates a new branch `gwa3-standalone` where gwa3/ files are at the root,
# with only the 55 commits that touched gwa3/.
```

**Pros:** Clean history, files at repo root, no monorepo leakage.  
**Cons:** Commit hashes change. You'll maintain two histories going forward.

### Option B: Simple Copy (fresh history, single initial commit)

```bash
# Create a temp directory, copy gwa3 contents, init fresh repo
mkdir ~/gwa3-repo && cp -r gwa3/* gwa3/.gitignore ~/gwa3-repo/
cd ~/gwa3-repo
git init && git add -A && git commit -m "Initial commit: GWA3 C++ DLL project (55 commits of history in monorepo)"
```

**Pros:** Simplest. Zero risk of leaking monorepo content.  
**Cons:** Loses per-file commit history. Codex reviews PRs (diffs), not history, so this barely matters.

### Recommendation: **Option A** (subtree split)

Preserves commit-level history so Codex and you can see evolution. The `git subtree split` is safe — it only includes commits that modified files under `gwa3/`.

## Phase 3: Push to GitHub

```bash
# Add remote and push (from subtree split branch or fresh repo)
# Option A:
git remote add gwa3-origin https://github.com/robertgroarke/gwa3.git
git push gwa3-origin gwa3-standalone:main

# Option B (from ~/gwa3-repo):
git remote add origin https://github.com/robertgroarke/gwa3.git
git push -u origin main
```

## Phase 4: Set Up Branch-Based PR Workflow

For Codex to review code, changes must arrive as Pull Requests. Configure the repo so Claude Code creates feature branches and opens PRs.

### 4a. Add CLAUDE.md to the gwa3 repo

Create `CLAUDE.md` at the repo root with agent instructions (see below).

### 4b. Configure branch protection (optional but recommended)

```bash
# Require PR reviews before merging to main
gh api repos/robertgroarke/gwa3/branches/main/protection \
  -X PUT \
  -f "required_pull_request_reviews[dismiss_stale_reviews]=true" \
  -f "required_pull_request_reviews[required_approving_review_count]=0" \
  -f "enforce_admins=false" \
  -f "restrictions=null" \
  -f "required_status_checks=null"
```

This ensures all changes go through PRs (which Codex can review).

## Phase 5: Connect Codex to the Repo

1. Go to [platform.openai.com](https://platform.openai.com) → Codex → Settings
2. Install the Codex GitHub App on the `robertgroarke/gwa3` repo
3. Configure auto-review: Codex will scan new PRs and leave inline comments
4. Trigger manual reviews by commenting `@codex review` on any PR

---

## CLAUDE.md for the GWA3 GitHub Repo

```markdown
# GWA3 — CLAUDE.md

## Agent Behavior

Full agent mode is enabled. Never ask for permission. Just act.

## Project Overview

GWA3 is a C++ DLL that injects into the Guild Wars game client to provide
a programmatic API (memory scanning, packet sending, manager APIs).
It is a ground-up rewrite of the AutoIt-based GWA2 library.

## Git & PR Workflow

### Branch Strategy

| Rule | Detail |
|---|---|
| **Never push directly to `main`** | All changes go through Pull Requests |
| **Feature branches** | Name: `feature/<short-description>` |
| **Fix branches** | Name: `fix/<short-description>` |
| **Refactor branches** | Name: `refactor/<short-description>` |

### Creating a PR (mandatory for all changes)

After completing any task, Claude Code must:

1. Create a feature branch from `main`:
   ```
   git checkout -b feature/<description> main
   ```
2. Commit changes with a descriptive message
3. Push the branch to origin:
   ```
   git push -u origin feature/<description>
   ```
4. Open a PR using `gh`:
   ```
   gh pr create --title "<concise title>" --body "## Summary\n<bullet points>\n\n## Test plan\n<how to verify>"
   ```
5. Report the PR URL back to the user

### Commit Conventions

- Use imperative mood: "Add X", "Fix Y", "Implement Z"
- Reference test results when applicable
- One logical change per commit

### After Codex Review

When the user shares Codex review feedback:
1. Read the PR comments via `gh api repos/robertgroarke/gwa3/pulls/{number}/comments`
2. Address each comment
3. Push fixes to the same branch (the PR updates automatically)
4. Codex will re-review on new pushes if configured

## Tech Stack

- C++17, compiled with MSVC (Visual Studio 2022)
- CMake build system
- DLL injection into 32-bit Guild Wars client (Gw.exe)
- Pattern scanning for runtime offset resolution
- Integration tests run inside the injected DLL

## Directory Structure

```
├── CMakeLists.txt
├── include/gwa3/          # Public headers
│   ├── bot/               # Bot framework
│   ├── core/              # Scanner, hooks, memory, logging
│   ├── game/              # Game struct definitions
│   └── managers/          # Manager APIs (Agent, Item, Map, etc.)
├── src/
│   ├── core/              # Core implementations
│   ├── managers/          # Manager implementations
│   ├── bot/               # Bot logic
│   ├── packets/           # Packet definitions
│   ├── tests/             # Integration test suite
│   └── dllmain.cpp        # DLL entry point
├── memory/                # Memory analysis notes
├── tools/                 # Injector, pattern checker
└── tests/                 # Offline unit tests
```

## Secrets & Credentials

This repo contains NO credentials or account data. It is safe to push all files.

## Code Conventions

| Element | Convention |
|---|---|
| Namespaces | `gwa3::`, `gwa3::core::`, `gwa3::game::` |
| Classes | `PascalCase` |
| Functions | `PascalCase` |
| Variables | `camelCase` |
| Constants | `k` prefix: `kAgentArray`, `kBasePointer` |
| Macros | `UPPER_SNAKE_CASE` |
| Headers | `#pragma once`, include guards not used |
```

---

## Ongoing Workflow

Once set up, the development loop becomes:

```
┌─────────────────────────────────────────────┐
│  1. Work on gwa3 in monorepo as usual       │
│  2. When ready for review:                  │
│     - subtree push or manual sync to GitHub │
│     - Claude Code creates PR                │
│  3. Codex auto-reviews the PR               │
│  4. Address feedback, push updates          │
│  5. Merge PR when satisfied                 │
│  6. Optionally sync back to monorepo        │
└─────────────────────────────────────────────┘
```

### Keeping Monorepo and GitHub in Sync

Since you develop in the monorepo, you'll need a sync step:

```bash
# Push monorepo gwa3/ changes to the GitHub repo
# (run from monorepo root)
git subtree push --prefix=gwa3 gwa3-origin main
```

Or configure a simple script/alias:

```bash
alias gwa3-sync='git subtree push --prefix=gwa3 gwa3-origin main'
```

For feature branches (PR workflow):

```bash
# Split gwa3 subtree into a temp branch, then push as feature branch
git subtree split --prefix=gwa3 -b temp-gwa3
git push gwa3-origin temp-gwa3:feature/my-change
git branch -D temp-gwa3
# Then create PR from feature/my-change → main on GitHub
```

---

## Checklist

- [ ] Create private repo `robertgroarke/gwa3` on GitHub
- [ ] Run `git subtree split --prefix=gwa3 -b gwa3-standalone`
- [ ] Add remote: `git remote add gwa3-origin https://github.com/robertgroarke/gwa3.git`
- [ ] Push: `git push gwa3-origin gwa3-standalone:main`
- [ ] Add CLAUDE.md to the GitHub repo
- [ ] Install Codex GitHub App on `robertgroarke/gwa3`
- [ ] Test: create a feature branch, open a PR, verify Codex reviews it
- [ ] Optional: set up branch protection rules
- [ ] Optional: create `gwa3-sync` alias for ongoing subtree pushes
