"""Utility for listing and updating AGENT_WORK_REGISTRY.md."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REGISTRY = REPO_ROOT / "AGENT_WORK_REGISTRY.md"
DEFAULT_SESSION_REGISTRY_DIR = Path(os.environ.get("PROGRAMDATA", r"C:\ProgramData")) / "gwa3" / "sessions"
TABLE_HEADER = "| Work Area | Status | Owner | Lane | Heartbeat | Scope | Primary Files | Notes |"
LEGAL_LANES = {"none", "beastrit", "disco", "blumpkins", "marvin", "biscuit", "any"}
STALE_STATUSES = {"active", "blocked"}
HELD_STATUSES = {"active", "blocked"}
WINDOWS_LOCK_OFFSET = 2**31 - 1
SESSIONLESS_LANES = {"none", "any"}


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


def split_primary_files(primary_files: str) -> set[str]:
    files = {part.strip().lower() for part in primary_files.split(",") if part.strip()}
    files.discard("-")
    return files


def find_file_overlaps(
    rows: list[WorkRow],
    claimed_row: WorkRow,
    primary_files: str,
) -> list[tuple[WorkRow, list[str]]]:
    requested_files = split_primary_files(primary_files)
    if not requested_files:
        return []
    overlaps: list[tuple[WorkRow, list[str]]] = []
    for other in rows:
        if other.work_area == claimed_row.work_area or other.status not in HELD_STATUSES:
            continue
        colliding_files = sorted(requested_files & split_primary_files(other.primary_files))
        if colliding_files:
            overlaps.append((other, colliding_files))
    return overlaps


def format_file_overlap_message(work_area: str, overlaps: list[tuple[WorkRow, list[str]]]) -> str:
    details = "; ".join(
        f"{row.work_area}(owner={row.owner}, status={row.status}, files={','.join(files)})"
        for row, files in overlaps
    )
    return f"file overlap for {work_area}: {details}"


@contextmanager
def lock_registry(path: Path, timeout_seconds: float = 10.0, poll_seconds: float = 0.05):
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("a+b")
    deadline = time.monotonic() + timeout_seconds
    try:
        if sys.platform == "win32":
            import msvcrt

            while True:
                handle.seek(WINDOWS_LOCK_OFFSET)
                try:
                    msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise TimeoutError(f"timed out waiting for registry lock: {path}")
                    time.sleep(poll_seconds)
            try:
                yield
            finally:
                handle.seek(WINDOWS_LOCK_OFFSET)
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            while True:
                try:
                    fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except BlockingIOError:
                    if time.monotonic() >= deadline:
                        raise TimeoutError(f"timed out waiting for registry lock: {path}")
                    time.sleep(poll_seconds)
            try:
                yield
            finally:
                fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
    finally:
        handle.close()


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


def heartbeat_age_minutes(value: str, now: datetime | None = None) -> str:
    parsed = parse_heartbeat(value)
    if parsed is None:
        return "stale"
    now = now or datetime.now(timezone.utc)
    now = now.astimezone(timezone.utc)
    age_seconds = max(0, int((now - parsed).total_seconds()))
    return str(age_seconds // 60)


def format_status_table(rows: list[WorkRow], lane_filter: str | None = None, now: datetime | None = None) -> str:
    if lane_filter:
        lane_filter = normalize_lane(lane_filter)
    filtered = [row for row in rows if lane_filter is None or row.lane == lane_filter]
    if not filtered:
        return "no rows"
    lines = ["work_area | status | owner | lane | heartbeat-age-mins", "---|---|---|---|---"]
    for row in filtered:
        lines.append(
            f"{row.work_area} | {row.status} | {row.owner} | {row.lane} | "
            f"{heartbeat_age_minutes(row.heartbeat, now=now)}"
        )
    return "\n".join(lines)


def watch_status(
    registry: Path,
    lane_filter: str | None = None,
    sleep_fn=time.sleep,
    now_fn=lambda: datetime.now(timezone.utc),
    output=sys.stdout,
) -> None:
    try:
        while True:
            _, rows, _ = load_registry(registry)
            print("\033[H\033[J", end="", file=output)
            print(format_status_table(rows, lane_filter=lane_filter, now=now_fn()), file=output)
            output.flush()
            sleep_fn(5)
    except KeyboardInterrupt:
        return


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


def session_registry_dir_from_env() -> Path:
    return Path(os.environ.get("GWA3_SESSION_REGISTRY_DIR") or DEFAULT_SESSION_REGISTRY_DIR)


def load_session_registry(session_dir: Path | None = None) -> list[dict[str, object]]:
    session_dir = session_dir or session_registry_dir_from_env()
    sessions: list[dict[str, object]] = []
    if not session_dir.exists():
        return sessions
    for path in sorted(session_dir.glob("*.json")):
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            payload = {"_path": str(path), "_error": str(exc)}
        else:
            payload["_path"] = str(path)
        sessions.append(payload)
    return sessions


def _session_lane(session: dict[str, object]) -> str:
    lane = session.get("lane")
    return str(lane).strip().lower() if lane is not None else ""


def _session_is_live(session: dict[str, object]) -> bool:
    if session.get("_error"):
        return False
    status = str(session.get("status", "running")).strip().lower()
    return status not in {"stopped", "exited", "terminated", "dead"}


def _session_pid(session: dict[str, object]) -> int | None:
    raw_pid = session.get("gw_pid") or session.get("pid")
    try:
        pid = int(raw_pid)
    except (TypeError, ValueError):
        return None
    return pid if pid > 0 else None


def pid_is_running(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        result = subprocess.run(
            ["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode != 0:
            return False
        for line in result.stdout.splitlines():
            fields = [field.strip().strip('"') for field in line.split(",")]
            if len(fields) > 1 and fields[1] == str(pid):
                return True
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def session_heartbeat_is_stale(session: dict[str, object], now: datetime, max_age_minutes: int) -> bool:
    parsed = parse_heartbeat(str(session.get("heartbeat_at") or ""))
    if parsed is None:
        return True
    return now - parsed > timedelta(minutes=max_age_minutes)


def classify_orphan_session(
    session: dict[str, object],
    now: datetime,
    max_age_minutes: int,
    pid_checker=pid_is_running,
) -> str | None:
    if session.get("_error"):
        return f"unreadable or malformed JSON: {session['_error']}"
    pid = _session_pid(session)
    if pid is None:
        return "unknown pid"
    if pid_checker(pid):
        return None
    if not session_heartbeat_is_stale(session, now, max_age_minutes):
        return None
    heartbeat = session.get("heartbeat_at") or "<missing>"
    return f"pid {pid} is not running and heartbeat_at {heartbeat} is older than {max_age_minutes} minutes"


def remove_session_file(path: Path) -> None:
    path.unlink()


def clean_orphan_sessions(
    session_dir: Path | None = None,
    dry_run: bool = False,
    max_age_minutes: int = 5,
    now: datetime | None = None,
    pid_checker=pid_is_running,
    remover=remove_session_file,
    loader=load_session_registry,
) -> tuple[list[str], dict[str, int]]:
    now = now or datetime.now(timezone.utc)
    lines: list[str] = []
    counts = {"orphans": 0, "removed": 0, "skipped": 0}
    for session in loader(session_dir):
        path = Path(str(session.get("_path", "")))
        reason = classify_orphan_session(session, now, max_age_minutes, pid_checker=pid_checker)
        if reason is None:
            continue
        counts["orphans"] += 1
        if dry_run:
            lines.append(f"would remove {path}: {reason}")
            continue
        try:
            remover(path)
        except OSError as exc:
            counts["skipped"] += 1
            lines.append(f"skipped {path}: {exc}")
        else:
            counts["removed"] += 1
            lines.append(f"removed {path}: {reason}")
    lines.append(f"orphans={counts['orphans']} removed={counts['removed']} skipped={counts['skipped']}")
    return lines, counts


def _format_session(session: dict[str, object]) -> str:
    pid = session.get("gw_pid") or session.get("pid") or "?"
    character = session.get("character") or "?"
    path = session.get("_path") or "?"
    return f"pid={pid} character={character} file={path}"


def find_session_mismatches(
    rows: list[WorkRow],
    sessions: list[dict[str, object]],
    lane_filter: str | None = None,
) -> list[str]:
    lane_filter = normalize_lane(lane_filter) if lane_filter else None
    active_rows = [
        row
        for row in rows
        if row.status == "active"
        and row.lane not in SESSIONLESS_LANES
        and (lane_filter is None or row.lane == lane_filter)
    ]
    live_sessions = [
        session
        for session in sessions
        if _session_is_live(session)
        and _session_lane(session)
        and (lane_filter is None or _session_lane(session) == lane_filter)
    ]

    live_by_lane: dict[str, list[dict[str, object]]] = {}
    for session in live_sessions:
        live_by_lane.setdefault(_session_lane(session), []).append(session)

    active_by_lane: dict[str, list[WorkRow]] = {}
    for row in active_rows:
        active_by_lane.setdefault(row.lane, []).append(row)

    mismatches: list[str] = []
    for row in active_rows:
        if row.lane not in live_by_lane:
            mismatches.append(
                f"MISMATCH active-without-live-session lane={row.lane} "
                f"row={row.work_area} owner={row.owner}"
            )

    for lane, lane_sessions in sorted(live_by_lane.items()):
        if lane not in active_by_lane:
            session_text = "; ".join(_format_session(session) for session in lane_sessions)
            mismatches.append(
                f"MISMATCH live-session-without-active-claim lane={lane} "
                f"sessions=[{session_text}]"
            )

    for lane, lane_rows in sorted(active_by_lane.items()):
        if len(lane_rows) > 1:
            row_text = ", ".join(f"{row.work_area}(owner={row.owner})" for row in lane_rows)
            mismatches.append(f"MISMATCH multiple-active-rows lane={lane} rows=[{row_text}]")

    return mismatches


def format_check_report(rows: list[WorkRow], sessions: list[dict[str, object]], lane_filter: str | None = None) -> str:
    mismatches = find_session_mismatches(rows, sessions, lane_filter=lane_filter)
    live_lanes = sorted({_session_lane(session) for session in sessions if _session_is_live(session) and _session_lane(session)})
    active_lanes = sorted({row.lane for row in rows if row.status == "active" and row.lane not in SESSIONLESS_LANES})
    header = [
        "Agent work registry reality check",
        f"active_lanes={','.join(active_lanes) if active_lanes else '-'}",
        f"live_session_lanes={','.join(live_lanes) if live_lanes else '-'}",
    ]
    if lane_filter:
        header.append(f"lane_filter={normalize_lane(lane_filter)}")
    if mismatches:
        return "\n".join(header + mismatches)
    return "\n".join(header + ["OK no mismatches"])


def claim_row(
    rows: list[WorkRow],
    work_area: str,
    owner: str,
    lane: str,
    scope: str,
    primary_files: str,
    notes: str,
    force: bool = False,
    strict_files: bool = False,
    warning_stream=sys.stderr,
) -> WorkRow:
    lane = normalize_lane(lane)
    heartbeat = utc_now()
    row = find_row(rows, work_area)
    if row.status != "available":
        raise ValueError(f"work area is not available: {work_area} (status={row.status}, owner={row.owner})")
    if lane != "none":
        for other in rows:
            if other.work_area == row.work_area:
                continue
            if other.lane == lane and other.status in HELD_STATUSES:
                raise ValueError(
                    f"lane collision: lane={lane} already held by {other.work_area} "
                    f"(status={other.status}, owner={other.owner})"
                )
    file_overlaps = find_file_overlaps(rows, row, primary_files)
    if file_overlaps:
        message = format_file_overlap_message(work_area, file_overlaps)
        if strict_files:
            raise ValueError(message)
        print(f"WARNING: {message}", file=warning_stream)
    row.status = "active"
    row.owner = owner
    row.lane = lane
    row.heartbeat = heartbeat
    row.scope = scope
    row.primary_files = primary_files
    row.notes = notes
    return row


def release_row(row: WorkRow, owner: str, notes: str | None = None) -> None:
    if row.owner != owner:
        raise ValueError(f"owner mismatch for {row.work_area}: current owner={row.owner}, requested owner={owner}")
    row.status = "available"
    row.owner = "-"
    row.heartbeat = utc_now()
    if notes is not None:
        row.notes = notes


def touch_row(row: WorkRow, owner: str, replacement_heartbeat: str | None = None) -> None:
    if row.status not in HELD_STATUSES:
        raise ValueError(f"work area is not held: {row.work_area} (status={row.status})")
    if row.owner != owner:
        raise ValueError(f"owner mismatch for {row.work_area}: current owner={row.owner}, requested owner={owner}")
    row.heartbeat = replacement_heartbeat or utc_now()


def main() -> int:
    parser = argparse.ArgumentParser(description="Manage AGENT_WORK_REGISTRY.md")
    parser.add_argument(
        "command",
        choices=["list", "claim", "release", "touch", "status", "prune-stale", "check", "clean-orphan-sessions"],
    )
    parser.add_argument("work_area", nargs="?")
    parser.add_argument("--area")
    parser.add_argument("--owner", default="-")
    parser.add_argument("--lane")
    parser.add_argument("--scope", default="-")
    parser.add_argument("--files", default="-")
    parser.add_argument("--notes", default="-")
    parser.add_argument("--status")
    parser.add_argument("--watch", action="store_true")
    parser.add_argument("--max-age-minutes", type=int)
    parser.add_argument("--session-dir", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--lock-timeout-seconds", type=float, default=10.0)
    parser.add_argument("--warn-only", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--strict-files", action="store_true")
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    args = parser.parse_args()

    try:
        if args.command == "list":
            _, rows, _ = load_registry(args.registry)
            print(list_rows(rows, status=args.status))
            return 0

        if args.command == "check":
            _, rows, _ = load_registry(args.registry)
            sessions = load_session_registry()
            report = format_check_report(rows, sessions, lane_filter=args.lane)
            print(report)
            has_mismatch = "MISMATCH " in report
            return 0 if args.warn_only or not has_mismatch else 1

        if args.command == "status":
            if args.watch:
                watch_status(args.registry, lane_filter=args.lane)
            else:
                _, rows, _ = load_registry(args.registry)
                print(format_status_table(rows, lane_filter=args.lane))
            return 0

        if args.command == "clean-orphan-sessions":
            lines, _ = clean_orphan_sessions(
                session_dir=args.session_dir,
                dry_run=args.dry_run,
                max_age_minutes=args.max_age_minutes if args.max_age_minutes is not None else 5,
            )
            print("\n".join(lines))
            return 0

        work_area = args.work_area or args.area
        with lock_registry(args.registry, timeout_seconds=args.lock_timeout_seconds):
            prefix, rows, suffix = load_registry(args.registry)

            if args.command == "prune-stale":
                pruned = prune_stale_rows(rows, max_age_minutes=args.max_age_minutes if args.max_age_minutes is not None else 15)
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

            if not work_area:
                raise ValueError("work area is required for claim/release/touch")

            if args.command == "claim":
                claim_row(
                    rows,
                    work_area,
                    args.owner,
                    args.lane or "none",
                    args.scope,
                    args.files,
                    args.notes,
                    force=args.force,
                    strict_files=args.strict_files,
                )
            elif args.command == "release":
                row = find_row(rows, work_area)
                release_row(row, owner=args.owner, notes=args.notes)
            else:
                row = find_row(rows, work_area)
                touch_row(row, owner=args.owner)
            save_registry(rows, prefix, suffix, args.registry)
            verb = "touched" if args.command == "touch" else f"{args.command}d"
            print(f"{verb} {work_area}")
            return 0
    except (TimeoutError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
