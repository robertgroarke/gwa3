"""Provision and inspect isolated GWA3 git worktrees."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DOCUMENTS_DIR = Path(os.environ.get("USERPROFILE", r"C:\Users\Robert")) / "Documents"
DEFAULT_REPO = Path(os.environ.get("GWA3_WORKSPACE_REPO", r"C:\Users\Robert\Documents\gwa3-private"))
LANE_WORKTREE_DEFAULTS = {
    "none": DOCUMENTS_DIR / "gwa3-none",
    "beastrit": DOCUMENTS_DIR / "gwa3-beastrit",
    "disco": DOCUMENTS_DIR / "gwa3-disco",
    "blumpkins": DOCUMENTS_DIR / "gwa3-blumpkins",
    "marvin": DOCUMENTS_DIR / "gwa3-marvin",
    "biscuit": DOCUMENTS_DIR / "gwa3-biscuit",
    "any": DOCUMENTS_DIR / "gwa3-any",
}


@dataclass(frozen=True)
class WorktreeEntry:
    path: Path
    head: str = ""
    branch: str = ""
    detached: bool = False
    bare: bool = False
    prunable: str = ""

    @property
    def branch_name(self) -> str:
        if self.branch.startswith("refs/heads/"):
            return self.branch.removeprefix("refs/heads/")
        if self.branch:
            return self.branch
        return "(detached)" if self.detached else "-"


@dataclass(frozen=True)
class PruneCandidate:
    entry: WorktreeEntry
    reasons: tuple[str, ...]


def run_git(repo: Path, args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        capture_output=True,
        check=True,
        text=True,
    )


def parse_worktree_porcelain(output: str) -> list[WorktreeEntry]:
    entries: list[WorktreeEntry] = []
    current: dict[str, object] | None = None

    def flush() -> None:
        nonlocal current
        if current is None:
            return
        entries.append(
            WorktreeEntry(
                path=Path(str(current["path"])),
                head=str(current.get("head", "")),
                branch=str(current.get("branch", "")),
                detached=bool(current.get("detached", False)),
                bare=bool(current.get("bare", False)),
                prunable=str(current.get("prunable", "")),
            )
        )
        current = None

    for raw_line in output.splitlines():
        line = raw_line.rstrip()
        if not line:
            flush()
            continue
        key, _, value = line.partition(" ")
        if key == "worktree":
            flush()
            current = {"path": value}
            continue
        if current is None:
            continue
        if key == "HEAD":
            current["head"] = value
        elif key == "branch":
            current["branch"] = value
        elif key == "detached":
            current["detached"] = True
        elif key == "bare":
            current["bare"] = True
        elif key == "prunable":
            current["prunable"] = value or "prunable"

    flush()
    return entries


def list_worktrees(repo: Path) -> list[WorktreeEntry]:
    result = run_git(repo, ["worktree", "list", "--porcelain"])
    return parse_worktree_porcelain(result.stdout)


def default_worktree_path(lane: str) -> Path:
    normalized = lane.strip().lower()
    return LANE_WORKTREE_DEFAULTS.get(normalized, DOCUMENTS_DIR / f"gwa3-{normalized}")


def lane_for_path(path: Path) -> str:
    path_text = str(path).rstrip("\\/").lower()
    for lane, default_path in LANE_WORKTREE_DEFAULTS.items():
        if str(default_path).rstrip("\\/").lower() == path_text:
            return lane
    name = path.name.lower()
    if name.startswith("gwa3-") and len(name) > len("gwa3-"):
        return name.removeprefix("gwa3-")
    return "-"


def provision_worktree(repo: Path, lane: str, branch: str, path: Path | None = None) -> Path:
    worktree_path = path or default_worktree_path(lane)
    worktree_path.parent.mkdir(parents=True, exist_ok=True)
    run_git(repo, ["worktree", "add", str(worktree_path), branch])
    return worktree_path


def merged_branches(repo: Path, base_branch: str = "master") -> set[str]:
    result = run_git(repo, ["branch", "--merged", base_branch, "--format", "%(refname:short)"])
    return {line.strip() for line in result.stdout.splitlines() if line.strip()}


def find_prune_candidates(
    entries: list[WorktreeEntry],
    merged: set[str],
    repo: Path,
    base_branch: str = "master",
) -> list[PruneCandidate]:
    candidates: list[PruneCandidate] = []
    try:
        repo_root = repo.resolve()
    except OSError:
        repo_root = repo

    for entry in entries:
        try:
            entry_path = entry.path.resolve()
        except OSError:
            entry_path = entry.path
        if entry_path == repo_root:
            continue

        branch_name = entry.branch_name
        reasons: list[str] = []
        if not entry.path.exists():
            reasons.append("missing-path")
        if branch_name not in {"-", "(detached)", base_branch} and branch_name in merged:
            reasons.append(f"merged-into-{base_branch}")
        if reasons:
            candidates.append(PruneCandidate(entry=entry, reasons=tuple(reasons)))
    return candidates


def format_worktree_table(entries: list[WorktreeEntry]) -> str:
    if not entries:
        return "no worktrees"
    lines = ["worktree | branch | lane", "---|---|---"]
    for entry in entries:
        lines.append(f"{entry.path} | {entry.branch_name} | {lane_for_path(entry.path)}")
    return "\n".join(lines)


def prune_worktrees(repo: Path, apply: bool, base_branch: str = "master") -> list[str]:
    entries = list_worktrees(repo)
    candidates = find_prune_candidates(entries, merged_branches(repo, base_branch), repo, base_branch)
    if not candidates:
        return ["no stale worktrees found"]

    messages: list[str] = []
    for candidate in candidates:
        reason_text = ",".join(candidate.reasons)
        action = "removed" if apply else "would remove"
        if apply:
            run_git(repo, ["worktree", "remove", "--force", str(candidate.entry.path)])
        messages.append(
            f"{action} {candidate.entry.path} branch={candidate.entry.branch_name} "
            f"lane={lane_for_path(candidate.entry.path)} reason={reason_text}"
        )
    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description="Provision and inspect GWA3 git worktrees")
    parser.add_argument("--repo", type=Path, default=DEFAULT_REPO, help="source git repo to manage")
    subparsers = parser.add_subparsers(dest="command", required=True)

    provision_parser = subparsers.add_parser("provision", help="create a lane worktree")
    provision_parser.add_argument("--lane", required=True)
    provision_parser.add_argument("--branch", required=True)
    provision_parser.add_argument("--path", type=Path)

    subparsers.add_parser("list", help="show worktree to branch to lane mapping")

    prune_parser = subparsers.add_parser("prune", help="remove stale or missing worktrees")
    prune_mode = prune_parser.add_mutually_exclusive_group(required=True)
    prune_mode.add_argument("--dry-run", action="store_true")
    prune_mode.add_argument("--apply", action="store_true")
    prune_parser.add_argument("--base", default="master")

    args = parser.parse_args()

    try:
        if args.command == "provision":
            path = provision_worktree(args.repo, args.lane, args.branch, path=args.path)
            print(path)
            return 0
        if args.command == "list":
            print(format_worktree_table(list_worktrees(args.repo)))
            return 0
        if args.command == "prune":
            for message in prune_worktrees(args.repo, apply=args.apply, base_branch=args.base):
                print(message)
            return 0
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.strip() if exc.stderr else str(exc)
        print(f"error: {stderr}", file=sys.stderr)
        return exc.returncode or 1
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
