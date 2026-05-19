"""Extract skill and item ID constants from AutoIt source files into Python dicts.

Regenerate:
    python bridge/generate_gamedata.py --source-root path/to/BotsHub

Check committed output:
    python bridge/generate_gamedata.py --check --source-root path/to/BotsHub
"""

from __future__ import annotations

import argparse
import difflib
import os
from pathlib import Path
import re
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_ROOT = Path(
    os.environ.get("GWA3_GAMEDATA_SOURCE_ROOT", REPO_ROOT.parent / "BotsHub-latest")
)
DEFAULT_OUTPUT = Path(__file__).resolve().with_name("gamedata.py")

CONST_RE = re.compile(r"Global Const \$ID_(\w+)\s*=\s*(\d+)")


def source_files(source_root: Path) -> tuple[Path, Path, Path]:
    lib_root = source_root / "lib"
    return (
        lib_root / "GWA2_ID_Skills.au3",
        lib_root / "GWA2_ID_Items.au3",
        lib_root / "GWA2_ID.au3",
    )


def validate_source_root(source_root: Path) -> tuple[Path, Path, Path]:
    files = source_files(source_root)
    missing = [str(path) for path in files if not path.exists()]
    if missing:
        joined = "\n  ".join(missing)
        raise FileNotFoundError(
            "Missing gamedata source files. Pass --source-root or set "
            f"GWA3_GAMEDATA_SOURCE_ROOT.\n  {joined}"
        )
    return files


def extract_consts(path: Path):
    result = {}
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = CONST_RE.match(line.strip())
            if m:
                result[int(m.group(2))] = m.group(1)
    return result


def format_name(raw):
    """Convert UPPER_SNAKE to Title Case: HEALING_SIGNET -> Healing Signet"""
    return raw.replace("_", " ").title()


def generate(source_root: Path, output: Path) -> None:
    skills_file, items_file, ids_file = validate_source_root(source_root)

    # Skills
    raw_skills = extract_consts(skills_file)
    # Filter out weapon type constants (low IDs that start with SKILL_)
    skills = {}
    for k, v in raw_skills.items():
        if v.startswith("SKILL_") and k < 200:
            continue
        skills[k] = format_name(v)
    print(f"Extracted {len(skills)} skill IDs")

    # Items
    raw_items = extract_consts(items_file)
    items = {k: format_name(v) for k, v in raw_items.items()}
    print(f"Extracted {len(items)} item model IDs")

    # Professions and attributes from GWA2_ID.au3
    extract_consts(ids_file)

    # Write output
    with output.open("w", encoding="utf-8") as f:
        f.write('"""Auto-generated Guild Wars game data lookups."""\n\n')
        f.write("# fmt: off\n\n")

        # Professions
        f.write("PROFESSIONS = {\n")
        for pid, name in [
            (0, "Unknown"), (1, "Warrior"), (2, "Ranger"), (3, "Monk"),
            (4, "Necromancer"), (5, "Mesmer"), (6, "Elementalist"),
            (7, "Assassin"), (8, "Ritualist"), (9, "Paragon"), (10, "Dervish"),
        ]:
            f.write(f"    {pid}: {name!r},\n")
        f.write("}\n\n")

        # Skill types (from GWCA constant data)
        f.write("SKILL_TYPES = {\n")
        for tid, name in [
            (0, "Stance"), (1, "Hex"), (2, "Spell"), (3, "Enchantment"),
            (4, "Signet"), (5, "Well"), (6, "Skill"), (7, "Ward"),
            (8, "Glyph"), (9, "Attack"), (10, "Shout"), (11, "Preparation"),
            (12, "Trap"), (13, "Ritual"), (14, "Item Spell"), (15, "Weapon Spell"),
            (16, "Flash Enchantment"), (17, "Double Attack"), (18, "Pet Attack"),
            (19, "Form"), (20, "Chant"), (21, "Echo"), (22, "Bounty"),
            (23, "Disguise"), (24, "Title"),
        ]:
            f.write(f"    {tid}: {name!r},\n")
        f.write("}\n\n")

        # Item rarity (from interaction flags)
        f.write("ITEM_RARITY = {\n")
        for rid, name in [
            (2621, "White"), (2622, "Gray"), (2623, "Blue"),
            (2624, "Gold"), (2626, "Purple"), (2627, "Green"),
        ]:
            f.write(f"    {rid}: {name!r},\n")
        f.write("}\n\n")

        # Item types
        f.write("ITEM_TYPES = {\n")
        for tid, name in [
            (0, "Salvage Armor"), (2, "Axe"), (3, "Bag"), (4, "Boots"),
            (5, "Bow"), (6, "Bundle"), (7, "Chest Armor"), (8, "Upgrade"),
            (9, "Usable"), (10, "Dye"), (11, "Material"), (12, "Offhand"),
            (13, "Gloves"), (14, "Celestial Sigil"), (15, "Hammer"),
            (16, "Headgear"), (17, "Trophy"), (18, "Key"), (19, "Leggings"),
            (20, "Gold Coins"), (21, "Quest Item"), (22, "Wand"),
            (24, "Shield"), (26, "Staff"), (27, "Sword"), (29, "Kit"),
            (30, "Trophy"), (31, "Scroll"), (32, "Dagger"), (33, "Present"),
            (34, "Minipet"), (35, "Scythe"), (36, "Spear"), (43, "Book"),
        ]:
            f.write(f"    {tid}: {name!r},\n")
        f.write("}\n\n")

        # Skill IDs -> names
        f.write(f"# {len(skills)} skill name mappings\n")
        f.write("SKILL_NAMES = {\n")
        for k in sorted(skills.keys()):
            f.write(f"    {k}: {skills[k]!r},\n")
        f.write("}\n\n")

        # Item model IDs -> names
        f.write(f"# {len(items)} item model ID mappings\n")
        f.write("ITEM_NAMES = {\n")
        for k in sorted(items.keys()):
            f.write(f"    {k}: {items[k]!r},\n")
        f.write("}\n\n")

        f.write("# fmt: on\n\n\n")
        f.write("def skill_name(skill_id: int) -> str:\n")
        f.write("    return SKILL_NAMES.get(skill_id, f\"Skill#{skill_id}\")\n\n\n")
        f.write("def item_name(model_id: int) -> str:\n")
        f.write("    return ITEM_NAMES.get(model_id, f\"Item#{model_id}\")\n\n\n")
        f.write("def profession_name(prof_id: int) -> str:\n")
        f.write("    return PROFESSIONS.get(prof_id, f\"Prof#{prof_id}\")\n\n\n")
        f.write("def skill_type_name(type_id: int) -> str:\n")
        f.write("    return SKILL_TYPES.get(type_id, f\"Type#{type_id}\")\n\n\n")
        f.write("def item_type_name(type_id: int) -> str:\n")
        f.write("    return ITEM_TYPES.get(type_id, f\"Type#{type_id}\")\n")

    print(f"Written to {output}")


def check(source_root: Path, output: Path) -> int:
    with tempfile.TemporaryDirectory() as tmp:
        generated = Path(tmp) / "gamedata.py"
        generate(source_root, generated)
        expected = output.read_text(encoding="utf-8").splitlines(keepends=True)
        actual = generated.read_text(encoding="utf-8").splitlines(keepends=True)
        if expected == actual:
            print("gamedata.py is up to date")
            return 0

        diff = difflib.unified_diff(
            expected,
            actual,
            fromfile=str(output),
            tofile=str(generated),
        )
        sys.stdout.writelines(diff)
        print("gamedata.py is stale; rerun bridge/generate_gamedata.py and commit the result")
        return 1


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-root",
        type=Path,
        default=DEFAULT_SOURCE_ROOT,
        help="Path to a BotsHub checkout containing lib/GWA2_ID*.au3 files.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Output gamedata.py path.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Regenerate to a temp file and fail if output differs.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    source_root = args.source_root.resolve()
    output = args.output.resolve()
    try:
        if args.check:
            return check(source_root, output)
        generate(source_root, output)
        return 0
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
