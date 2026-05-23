"""Utility for listing and updating AGENT_WORK_REGISTRY.md."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REGISTRY = REPO_ROOT / "AGENT_WORK_REGISTRY.md"
TABLE_HEADER = "| Work Area | Status | Owner | Lane | Heartbeat | Scope | Primary Files | Notes |"
LEGAL_LANES = {"none", "beastrit", "disco", "blumpkins", "marvin", "biscuit", "any"}
STALE_STATUSES = {"active", "blocked"}


@dataclass
class WorkRow:
    work_area: str
    status: str
    owner: str
    lane: str
    heartbeat: str
    scope: str
    primary_files: str
    notes: str

    @classmethod
    def from_cells(cls, cells: list[str]) -> "WorkRow":
        return cls(
            work_area=cells[0].strip("`"),
            status=cells[1].strip("`"),
            owner=cells[2].strip("`"),
            lane=cells[3].strip("`"),
            heartbeat=cells[4].strip("`"),
            scope=cells[5],
            primary_files=cells[6].strip("`"),
            notes=cells[7],
        )

    def to_markdown_row(self) -> str:
        return (
            f"| `{self.work_area}` | `{self.status}` | `{self.owner}` | `{self.lane}` | `{self.heartbeat}` | {self.scope} | "
            f"`{self.primary_files}` | {self.notes} |"
        )


def _parse_table_row(line: str) -> WorkRow | None:
    stripped = line.strip()
    if not stripped.startswith("|") or stripped.startswith("|---"):
        return None
    cells = [cell.strip() for cell in stripped.strip("|").split("|")]
    if len(cells) != 8 or cells[0] == "Work Area":
        return None
    return WorkRow.from_cells(cells)


def load_registry(path: Path = DEFAULT_REGISTRY) -> tuple[list[str], list[WorkRow], list[str]]:
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


def save_registry(rows: list[WorkRow], prefix: list[str], suffix: list[str], path: Path = DEFAULT_REGISTRY) -> None:
    body = prefix + [row.to_markdown_row() for row in rows] + suffix
    path.write_text("\n".join(body) + "\n", encoding="utf-8")


def find_row(rows: list[WorkRow], work_area: str) -> WorkRow:
    target = work_area.strip().lower()
    for row in rows:
        if row.work_area.lower() == target:
            return row
    raise ValueError(f"work area not found: {work_area}")


def list_rows(rows: list[WorkRow], status: str | None = None) -> str:
    filtered = [row for row in rows if status is None or row.status == status]
    return "\n".join(
        f"{row.work_area}: [{row.status}] owner={row.owner} lane={row.lane} heartbeat={row.heartbeat} scope={row.scope}"
        for row in filtered
    )


def normalize_lane(lane: str) -> str:
    normalized = lane.strip().strip("`").lower()
    if normalized not in LEGAL_LANES:
        raise ValueError(f"invalid lane: {lane} (expected one of {', '.join(sorted(LEGAL_LANES))})")
    return normalized


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_heartbeat(value: str) -> datetime | None:
    cleaned = value.strip().strip("`")
    if not cleaned:
        return None
    if cleaned.endswith("Z"):
        cleaned = cleaned[:-1] + "+00:00"
    try:
        parsed = datetime.fromisoformat(cleaned)
    except ValueError:
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def heartbeat_is_stale(row: WorkRow, now: datetime, max_age_minutes: int) -> bool:
    parsed = parse_heartbeat(row.heartbeat)
    if parsed is None:
        return True
    return now - parsed > timedelta(minutes=max_age_minutes)


def prune_stale_rows(
    rows: list[WorkRow],
    max_age_minutes: int = 15,
    now: datetime | None = None,
    replacement_heartbeat: str | None = None,
) -> list[tuple[WorkRow, str, str, str]]:
    now = now or datetime.now(timezone.utc)
    replacement_heartbeat = replacement_heartbeat or now.replace(microsecond=0).isoformat().replace("+00:00", "Z")
    pruned: list[tuple[WorkRow, str, str, str]] = []
    for row in rows:
        if row.status not in STALE_STATUSES:
            continue
        if not heartbeat_is_stale(row, now, max_age_minutes):
            continue
        old_status = row.status
        old_owner = row.owner
        old_heartbeat = row.heartbeat
        row.status = "available"
        row.owner = "-"
        row.heartbeat = replacement_heartbeat
        pruned.append((row, old_status, old_owner, old_heartbeat))
    return pruned


def claim_row(
    rows: list[WorkRow],
    work_area: str,
    owner: str,
    lane: str,
    scope: str,
    primary_files: str,
    notes: str,
    force: bool = False,
) -> WorkRow:
    lane = normalize_lane(lane)
    heartbeat = utc_now()
    try:
        row = find_row(rows, work_area)
    except ValueError:
        row = WorkRow(work_area, "active", owner, lane, heartbeat, scope, primary_files, notes)
        rows.append(row)
        rows.sort(key=lambda item: item.work_area.lower())
        return row
    if row.status == "active" and row.owner != owner and not force:
        raise ValueError(f"work area already active: {work_area} (owner={row.owner})")
    row.status = "active"
    row.owner = owner
    row.lane = lane
    row.heartbeat = heartbeat
    row.scope = scope
    row.primary_files = primary_files
    row.notes = notes
    return row


def release_row(row: WorkRow, notes: str | None = None) -> None:
    row.status = "available"
    row.owner = "-"
    row.heartbeat = utc_now()
    if notes is not None:
        row.notes = notes


def main() -> int:
    parser = argparse.ArgumentParser(description="Manage AGENT_WORK_REGISTRY.md")
    parser.add_argument("command", choices=["list", "claim", "release", "prune-stale"])
    parser.add_argument("--area")
    parser.add_argument("--owner", default="-")
    parser.add_argument("--lane", default="none")
    parser.add_argument("--scope", default="-")
    parser.add_argument("--files", default="-")
    parser.add_argument("--notes", default="-")
    parser.add_argument("--status")
    parser.add_argument("--max-age-minutes", type=int, default=15)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    args = parser.parse_args()

    prefix, rows, suffix = load_registry(args.registry)

    if args.command == "list":
        print(list_rows(rows, status=args.status))
        return 0

    if args.command == "prune-stale":
        pruned = prune_stale_rows(rows, max_age_minutes=args.max_age_minutes)
        if pruned:
            save_registry(rows, prefix, suffix, args.registry)
            for row, old_status, old_owner, old_heartbeat in pruned:
                print(
                    f"pruned {row.work_area}: {old_status}/{old_owner} lane={row.lane} "
                    f"heartbeat={old_heartbeat} -> available"
                )
        else:
            print("no stale rows found")
        return 0

    if not args.area:
        raise SystemExit("--area is required for claim/release")

    if args.command == "claim":
        claim_row(rows, args.area, args.owner, args.lane, args.scope, args.files, args.notes, force=args.force)
    else:
        row = find_row(rows, args.area)
        release_row(row, notes=args.notes)
    save_registry(rows, prefix, suffix, args.registry)
    print(f"{args.command}d {args.area}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
