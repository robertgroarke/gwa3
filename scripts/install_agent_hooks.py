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
DEFAULT_HOOK_SOURCES = {
    "pre-commit": REPO_ROOT / "scripts" / "git-hooks" / "pre-commit-claim-check",
    "post-commit": REPO_ROOT / "scripts" / "git-hooks" / "post-commit-landing-log",
}


@dataclass(frozen=True)
class HookInstallResult:
    repo: Path
    hook_name: str
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


def install_hook(repo: Path, parent_repo: Path, hook_name: str, hook_source: Path, timestamp: str) -> HookInstallResult:
    target_dir = hooks_dir(repo)
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / hook_name
    backup = None
    if target.exists():
        backup = target_dir / f"{hook_name}.bak.{timestamp}"
        shutil.move(str(target), str(backup))
    target.write_text(render_hook(hook_source, parent_repo), encoding="utf-8")
    mark_executable(target)
    return HookInstallResult(repo=repo, hook_name=hook_name, target=target, backup=backup)


def install_hooks(
    parent_repo: Path = REPO_ROOT,
    private_repo: Path = DEFAULT_PRIVATE_REPO,
    hook_sources: dict[str, Path] | None = None,
    timestamp_fn=timestamp_now,
) -> list[HookInstallResult]:
    timestamp = timestamp_fn()
    hook_sources = hook_sources or DEFAULT_HOOK_SOURCES
    results: list[HookInstallResult] = []
    for repo in [parent_repo, private_repo]:
        for hook_name, hook_source in hook_sources.items():
            results.append(install_hook(repo, parent_repo, hook_name, hook_source, timestamp))
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description="Install agent coordination git hooks")
    parser.add_argument("--parent-repo", type=Path, default=REPO_ROOT)
    parser.add_argument("--private-repo", type=Path, default=DEFAULT_PRIVATE_REPO)
    args = parser.parse_args()

    for result in install_hooks(args.parent_repo, args.private_repo):
        print(f"installed {result.hook_name} {result.target}")
        if result.backup:
            print(f"backup {result.backup}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
