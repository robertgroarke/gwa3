"""Install agent coordination Git hooks into the parent and private repos."""

from __future__ import annotations

import argparse
import shutil
import stat
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PRIVATE_REPO = Path(r"C:\Users\Robert\Documents\gwa3-private")
DEFAULT_HOOK_SOURCE = REPO_ROOT / "scripts" / "git-hooks" / "pre-commit-claim-check"


@dataclass(frozen=True)
class HookInstallResult:
    repo: Path
    target: Path
    backup: Path | None


def timestamp_now() -> str:
    return datetime.now().strftime("%Y%m%d%H%M%S")


def hooks_dir(repo: Path) -> Path:
    git_path = repo / ".git"
    if git_path.is_dir():
        return git_path / "hooks"
    if git_path.is_file():
        content = git_path.read_text(encoding="utf-8").strip()
        prefix = "gitdir:"
        if content.lower().startswith(prefix):
            git_dir = content[len(prefix) :].strip()
            path = Path(git_dir)
            if not path.is_absolute():
                path = (repo / path).resolve()
            return path / "hooks"
    return git_path / "hooks"


def render_hook(hook_source: Path, parent_repo: Path) -> str:
    text = hook_source.read_text(encoding="utf-8")
    return text.replace('PARENT_REPO_TEXT = "__PARENT_REPO__"', f"PARENT_REPO_TEXT = {str(parent_repo)!r}")


def mark_executable(path: Path) -> None:
    current_mode = path.stat().st_mode
    path.chmod(current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def install_hook(repo: Path, parent_repo: Path, hook_source: Path, timestamp: str) -> HookInstallResult:
    target_dir = hooks_dir(repo)
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / "pre-commit"
    backup = None
    if target.exists():
        backup = target_dir / f"pre-commit.bak.{timestamp}"
        shutil.move(str(target), str(backup))
    target.write_text(render_hook(hook_source, parent_repo), encoding="utf-8")
    mark_executable(target)
    return HookInstallResult(repo=repo, target=target, backup=backup)


def install_hooks(
    parent_repo: Path = REPO_ROOT,
    private_repo: Path = DEFAULT_PRIVATE_REPO,
    hook_source: Path = DEFAULT_HOOK_SOURCE,
    timestamp_fn=timestamp_now,
) -> list[HookInstallResult]:
    timestamp = timestamp_fn()
    return [
        install_hook(parent_repo, parent_repo, hook_source, timestamp),
        install_hook(private_repo, parent_repo, hook_source, timestamp),
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Install agent claim-check pre-commit hooks")
    parser.add_argument("--parent-repo", type=Path, default=REPO_ROOT)
    parser.add_argument("--private-repo", type=Path, default=DEFAULT_PRIVATE_REPO)
    parser.add_argument("--hook-source", type=Path, default=DEFAULT_HOOK_SOURCE)
    args = parser.parse_args()

    for result in install_hooks(args.parent_repo, args.private_repo, args.hook_source):
        print(f"installed {result.target}")
        if result.backup:
            print(f"backup {result.backup}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
