"""Utility for listing and updating AGENT_ACCOUNT_REGISTRY.md."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REGISTRY = REPO_ROOT / "AGENT_ACCOUNT_REGISTRY.md"
TABLE_HEADER = "| Account Index | Character | Status | Lane Tag | Build Dir | DLL | Pipe | Launcher Script |"


@dataclass
class RegistryRow:
    account_index: str
    character: str
    status: str
    lane_tag: str
    build_dir: str
    dll: str
    pipe: str
    launcher_script: str

    @classmethod
    def from_cells(cls, cells: list[str]) -> "RegistryRow":
        return cls(
            account_index=cells[0],
            character=cells[1].strip("`"),
            status=cells[2].strip("`"),
            lane_tag=cells[3].strip("`"),
            build_dir=cells[4].strip("`"),
            dll=cells[5].strip("`"),
            pipe=cells[6].strip("`"),
            launcher_script=cells[7].strip("`"),
        )

    def to_markdown_row(self) -> str:
        return (
            f"| {self.account_index} | `{self.character}` | `{self.status}` | `{self.lane_tag}` | "
            f"`{self.build_dir}` | `{self.dll}` | `{self.pipe}` | `{self.launcher_script}` |"
        )


def _parse_table_row(line: str) -> RegistryRow | None:
    stripped = line.strip()
    if not stripped.startswith("|") or stripped.startswith("|---"):
        return None
    cells = [cell.strip() for cell in stripped.strip("|").split("|")]
    if len(cells) != 8 or cells[0] == "Account Index":
        return None
    return RegistryRow.from_cells(cells)


def load_registry(path: Path = DEFAULT_REGISTRY) -> tuple[list[str], list[RegistryRow], list[str]]:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    header_index = next(i for i, line in enumerate(lines) if line.strip() == TABLE_HEADER)
    prefix = lines[:header_index]
    table_lines = lines[header_index:]
    divider_index = next(i for i, line in enumerate(table_lines) if line.strip().startswith("|---"))
    suffix_start = divider_index + 1
    rows = []
    while suffix_start < len(table_lines):
        row = _parse_table_row(table_lines[suffix_start])
        if row is None:
            break
        rows.append(row)
        suffix_start += 1
    suffix = table_lines[suffix_start:]
    table_prefix = table_lines[: divider_index + 1]
    return prefix + table_prefix, rows, suffix


def save_registry(rows: list[RegistryRow], prefix: list[str], suffix: list[str], path: Path = DEFAULT_REGISTRY) -> None:
    body = prefix + [row.to_markdown_row() for row in rows] + suffix
    path.write_text("\n".join(body) + "\n", encoding="utf-8")


def find_row(rows: list[RegistryRow], account_index: str | None, character: str | None) -> RegistryRow:
    if account_index is not None:
        for row in rows:
            if row.account_index == str(account_index):
                return row
        raise ValueError(f"account index not found: {account_index}")
    if character is not None:
        normalized = character.strip().lower()
        for row in rows:
            if row.character.lower() == normalized:
                return row
        raise ValueError(f"character not found: {character}")
    raise ValueError("either --index or --character is required")


def find_first_available(rows: list[RegistryRow]) -> RegistryRow:
    for row in rows:
        if row.status == "available":
            return row
    raise ValueError("no available account rows found")


def list_rows(rows: list[RegistryRow], status: str | None = None) -> str:
    filtered = [row for row in rows if status is None or row.status == status]
    return "\n".join(
        f"{row.account_index}: {row.character} [{row.status}] lane={row.lane_tag} pipe={row.pipe}"
        for row in filtered
    )


def claim_row(row: RegistryRow, force: bool = False) -> None:
    if row.status == "helper-only" and not force:
        raise ValueError(f"cannot claim helper-only account without --force: {row.character}")
    if row.status == "active" and not force:
        raise ValueError(f"account already active: {row.character}")
    row.status = "active"


def release_row(row: RegistryRow, force: bool = False) -> None:
    if row.status == "helper-only" and not force:
        raise ValueError(f"cannot release helper-only account without --force: {row.character}")
    row.status = "available"


def main() -> int:
    parser = argparse.ArgumentParser(description="Manage AGENT_ACCOUNT_REGISTRY.md")
    parser.add_argument("command", choices=["list", "claim", "release"])
    parser.add_argument("--index", dest="account_index")
    parser.add_argument("--character")
    parser.add_argument("--first-available", action="store_true")
    parser.add_argument("--status")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    args = parser.parse_args()

    prefix, rows, suffix = load_registry(args.registry)

    if args.command == "list":
        print(list_rows(rows, status=args.status))
        return 0

    if args.first_available:
        row = find_first_available(rows)
    else:
        row = find_row(rows, args.account_index, args.character)
    if args.command == "claim":
        claim_row(row, force=args.force)
    elif args.command == "release":
        release_row(row, force=args.force)
    save_registry(rows, prefix, suffix, args.registry)
    print(f"{args.command}d {row.character} -> {row.status}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
