"""Safely synchronize supported local games and firmware box art to an SD card."""

import argparse
import ctypes
import hashlib
import os
import shutil
import tempfile
import warnings
from pathlib import Path

from scripts.repo_tools import get_data_dir
from scripts.rom_key import make_rom_key

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_GAMES = get_data_dir(REPO_ROOT) / "games"
SRC_BOXART = get_data_dir(REPO_ROOT) / "roms" / "boxart"
ROM_EXTENSIONS = frozenset({
    ".gb", ".gbc", ".nes", ".wad", ".sms", ".gg", ".pce", ".a26",
    ".p8", ".gen", ".md", ".smd", ".sfc", ".smc", ".ws",
    ".wsc", ".ngp", ".ngc", ".lnx", ".col", ".sg",
})
BOXART_EXTENSIONS = frozenset({".raw", ".bmp"})
BUFFER_SIZE = 1024 * 1024
EXPECTED_VOLUME_LABEL = "GAMELIB"


def sha256_file(path: Path) -> bytes:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(BUFFER_SIZE):
            digest.update(chunk)
    return digest.digest()


def deployment_tasks(
    src_games: Path, src_boxart: Path, dest_root: Path
) -> list[tuple[Path, Path]]:
    if not src_games.is_dir():
        raise FileNotFoundError(f"Game source directory not found: {src_games}")
    if not src_boxart.is_dir():
        raise FileNotFoundError(f"Box-art source directory not found: {src_boxart}")
    if not dest_root.is_dir():
        raise FileNotFoundError(f"Destination is not an existing directory: {dest_root}")

    games = sorted(
        path for path in src_games.rglob("*")
        if path.is_file() and path.suffix.lower() in ROM_EXTENSIONS
    )
    boxart = sorted(
        path for path in src_boxart.iterdir()
        if path.is_file() and path.suffix.lower() in BOXART_EXTENSIONS
    )
    relative_paths = {path: path.relative_to(src_games).as_posix() for path in games}
    for relative_path in relative_paths.values():
        if len(relative_path.encode("utf-8")) >= 64:
            raise ValueError(f"ROM path is too long for firmware index: {relative_path}")

    games_by_stem: dict[str, list[Path]] = {}
    for game in games:
        games_by_stem.setdefault(game.stem.casefold(), []).append(game)

    tasks = [
        (path, dest_root / Path(relative_paths[path]))
        for path in games
    ]
    keyed_art_targets: set[str] = set()
    ambiguous_art: list[str] = []
    for art_path in boxart:
        matching_games = games_by_stem.get(art_path.stem.casefold(), [])
        if len(matching_games) == 1:
            relative_path = relative_paths[matching_games[0]]
            target_name = f"{make_rom_key(relative_path)}{art_path.suffix.lower()}"
            destination = dest_root / "boxart" / target_name
        elif len(matching_games) > 1:
            # Ambiguous basename art is omitted; firmware cannot associate it safely.
            ambiguous_art.append(art_path.name)
            continue
        else:
            destination = dest_root / "boxart" / art_path.name
        target_key = str(destination.relative_to(dest_root)).casefold()
        if target_key in keyed_art_targets:
            # A path-keyed cover takes precedence over a legacy basename cover.
            continue
        keyed_art_targets.add(target_key)
        tasks.append((art_path, destination))
    if ambiguous_art:
        warnings.warn(
            f"Skipped {len(ambiguous_art)} box-art files with ambiguous ROM basenames.",
            RuntimeWarning,
            stacklevel=2,
        )
    validate_target_collisions(tasks, dest_root)
    return tasks


def validate_target_collisions(
    tasks: list[tuple[Path, Path]], dest_root: Path
) -> None:
    targets = set()
    for _, destination in tasks:
        key = str(destination.relative_to(dest_root)).casefold()
        if key in targets:
            raise ValueError(f"Case-insensitive destination filename collision: {destination.name}")
        targets.add(key)


def deployment_plan(
    tasks: list[tuple[Path, Path]],
    report_progress: bool = False,
) -> tuple[list[tuple[Path, Path]], int, int]:
    updates = []
    unchanged = 0
    update_bytes = 0
    for index, (src, dst) in enumerate(tasks, start=1):
        if dst.is_symlink() or (dst.exists() and not dst.is_file()):
            raise OSError(f"Destination exists and is not a regular file: {dst}")
        if dst.is_file() and dst.stat().st_size == src.stat().st_size:
            if sha256_file(dst) == sha256_file(src):
                unchanged += 1
            else:
                updates.append((src, dst))
                update_bytes += src.stat().st_size
        else:
            updates.append((src, dst))
            update_bytes += src.stat().st_size
        if report_progress and (index % 500 == 0 or index == len(tasks)):
            print(f"Compared {index:,}/{len(tasks):,} files.", flush=True)
    return updates, unchanged, update_bytes


def verify_volume_label(root: Path, expected_label: str) -> None:
    if os.name != "nt":
        raise OSError("SD deployment is supported only on Windows.")

    resolved_root = root.resolve()
    drive_root = resolved_root.anchor
    if resolved_root != Path(drive_root):
        raise ValueError(f"Destination must be the SD-card root, not a subdirectory: {root}")

    volume_name = ctypes.create_unicode_buffer(261)
    if not ctypes.windll.kernel32.GetVolumeInformationW(
        drive_root, volume_name, len(volume_name), None, None, None, None, 0
    ):
        raise ctypes.WinError()
    if volume_name.value.casefold() != expected_label.casefold():
        raise ValueError(
            f"Refusing to write to {drive_root}: expected volume label "
            f"{expected_label!r}, found {volume_name.value!r}."
        )


def verify_destination_root(root: Path) -> None:
    if os.name == "nt" and root.resolve() != Path(root.resolve().anchor):
        raise ValueError(f"Destination must be the SD-card root, not a subdirectory: {root}")


def copy_single_file(src: Path, dst: Path) -> tuple[bool, int]:
    size = src.stat().st_size
    if dst.is_symlink() or (dst.exists() and not dst.is_file()):
        raise OSError(f"Destination exists and is not a regular file: {dst}")
    source_digest = sha256_file(src)
    if dst.is_file() and dst.stat().st_size == size and sha256_file(dst) == source_digest:
        return False, size

    dst.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=".bmo-copy-", suffix=".tmp",
            dir=dst.parent, delete=False
        ) as staged:
            temporary_path = Path(staged.name)
            with src.open("rb") as source:
                shutil.copyfileobj(source, staged, BUFFER_SIZE)
            staged.flush()
            os.fsync(staged.fileno())

        if temporary_path.stat().st_size != size or sha256_file(temporary_path) != source_digest:
            raise OSError(f"Staged file verification failed: {src}")
        os.replace(temporary_path, dst)
        temporary_path = None
        return True, size
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def deploy(
    dest_root: Path,
    apply: bool = False,
    expected_label: str = EXPECTED_VOLUME_LABEL,
    src_games: Path | None = None,
    src_boxart: Path | None = None,
) -> int:
    data_dir = get_data_dir(REPO_ROOT)
    games_dir = src_games if src_games is not None else (data_dir / "games")
    boxart_dir = src_boxart if src_boxart is not None else (data_dir / "roms" / "boxart")
    verify_destination_root(dest_root)
    tasks = deployment_tasks(games_dir, boxart_dir, dest_root)
    game_count = sum(src.suffix.lower() in ROM_EXTENSIONS for src, _ in tasks)
    art_count = len(tasks) - game_count
    updates, unchanged, update_bytes = deployment_plan(tasks, report_progress=True)
    missing_count = sum(not dst.exists() for _, dst in updates)
    replace_count = len(updates) - missing_count
    print(f"Games: {game_count:,}; firmware box-art files: {art_count:,}")
    print(
        f"Would copy {missing_count:,} missing files and replace "
        f"{replace_count:,} different files; {unchanged:,} files already match."
    )
    print(f"Additional free space required: {update_bytes:,} bytes; destination: {dest_root}")
    for src, dst in updates[:20]:
        action = "replace" if dst.exists() else "copy"
        print(f"  {action}: {src.name}")
    if len(updates) > 20:
        print(f"  ... and {len(updates) - 20:,} more")

    if not apply:
        print("Dry run only. Re-run with --apply to write to the specified SD card.")
        return 0

    verify_volume_label(dest_root, expected_label)
    free_bytes = shutil.disk_usage(dest_root).free
    if free_bytes < update_bytes:
        raise OSError(
            f"Insufficient free space: need {update_bytes:,} bytes, "
            f"have {free_bytes:,}."
        )

    if not updates:
        print("No files need updating; the SD card was not changed.")
        return 0

    for directory in (dest_root / "boxart", dest_root / "saves", dest_root / "screenshots"):
        directory.mkdir(parents=True, exist_ok=True)

    index_path = dest_root / ".bmo_index"
    if index_path.exists():
        index_path.unlink()
        print("Removed the derived ROM index so firmware will rescan changed files.")

    copied = skipped = errors = copied_bytes = 0
    for index, (src, dst) in enumerate(updates, start=1):
        try:
            was_copied, size = copy_single_file(src, dst)
            if was_copied:
                copied += 1
                copied_bytes += size
            else:
                skipped += 1
        except OSError as error:
            errors += 1
            print(f"[ERROR] {src.name}: {error}")
        if index % 500 == 0 or index == len(updates):
            print(
                f"Processed {index:,}/{len(updates):,}: "
                f"{copied:,} copied, {skipped:,} verified unchanged, {errors:,} errors.",
                flush=True,
            )

    print(
        f"Finished: copied {copied:,} files ({copied_bytes:,} bytes), "
        f"verified unchanged {skipped:,}, errors {errors:,}."
    )
    return 1 if errors else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--destination",
        type=Path,
        required=True,
        help="Existing SD-card root directory (for example, F:\\).",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Write verified files. Without this flag, only show a dry-run summary.",
    )
    parser.add_argument(
        "--expected-label",
        default=EXPECTED_VOLUME_LABEL,
        help=f"Required SD volume label for writes (default: {EXPECTED_VOLUME_LABEL}).",
    )
    parser.add_argument(
        "--games-dir",
        type=Path,
        default=None,
        help="Source directory containing user-supplied ROMs (default: BMO_DATA_DIR/games).",
    )
    parser.add_argument(
        "--boxart-dir",
        type=Path,
        default=None,
        help="Source directory containing converted .raw/.bmp covers (default: BMO_DATA_DIR/roms/boxart).",
    )
    args = parser.parse_args(argv)
    return deploy(
        args.destination,
        apply=args.apply,
        expected_label=args.expected_label,
        src_games=args.games_dir,
        src_boxart=args.boxart_dir,
    )


if __name__ == "__main__":
    raise SystemExit(main())
