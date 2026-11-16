#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Compile firmware emulator sources used by browser gameplay to WebAssembly."""

from __future__ import annotations

import base64
import hashlib
import os
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
WASM_OUT_PATH = REPO_ROOT / "simulator" / "bmo_playtest_cores.wasm"
CORE_SOURCE = REPO_ROOT / "simulator" / "gameboy_wasm.cpp"
NES_SOURCE = REPO_ROOT / "simulator" / "nes_wasm.cpp"
AGNES_SOURCE = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "third_party" / "agnes" / "agnes.c"
STELLA_SRC_DIR = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "third_party" / "stella2014" / "src"
ATARI_WASM_SOURCE = REPO_ROOT / "simulator" / "atari_wasm.cpp"
SMSPLUS_DIR = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "third_party" / "smsplus"
COLEM_WASM_SOURCE = REPO_ROOT / "simulator" / "colem_wasm.cpp"
BMO_FACE_FIRMWARE_SOURCE = REPO_ROOT / "simulator" / "bmo_face_firmware.cpp"
BMO_FACE_WASM_SOURCE = REPO_ROOT / "simulator" / "bmo_face_wasm.cpp"
BMO_COMPAT = REPO_ROOT / "simulator" / "bmo_face_compat"
WASM_HAL = REPO_ROOT / "simulator" / "hal"
LOCAL_WASI_SDK = REPO_ROOT / ".tools" / "wasi-sdk-34.0-x86_64-windows"
EXPORTS = (
    "bmo_emu_rom_buffer",
    "bmo_emu_rom_capacity",
    "bmo_emu_load_rom",
    "bmo_emu_set_buttons",
    "bmo_emu_run_frame",
    "bmo_emu_framebuffer",
    "bmo_emu_frame_count",
    "bmo_emu_rom_checksum",
    "bmo_emu_button_state",
    "bmo_emu_joypad_state",
    "bmo_emu_last_error",
    "bmo_emu_is_running",
    "bmo_emu_set_palette",
    "bmo_emu_save_state",
    "bmo_emu_load_state",
    "bmo_emu_destroy",
    "bmo_nes_load_rom",
    "bmo_nes_set_buttons",
    "bmo_nes_run_frame",
    "bmo_nes_framebuffer",
    "bmo_nes_frame_count",
    "bmo_nes_rom_checksum",
    "bmo_nes_button_state",
    "bmo_nes_last_error",
    "bmo_nes_is_running",
    "bmo_nes_save_state",
    "bmo_nes_load_state",
    "bmo_nes_destroy",
    "bmo_atari_load_rom",
    "bmo_atari_set_buttons",
    "bmo_atari_run_frame",
    "bmo_atari_framebuffer",
    "bmo_atari_frame_count",
    "bmo_atari_rom_checksum",
    "bmo_atari_button_state",
    "bmo_atari_joypad_state",
    "bmo_atari_last_error",
    "bmo_atari_is_running",
    "bmo_atari_save_state",
    "bmo_atari_load_state",
    "bmo_atari_destroy",
    "bmo_colem_load_rom",
    "bmo_colem_set_buttons",
    "bmo_colem_run_frame",
    "bmo_colem_framebuffer",
    "bmo_colem_ram",
    "bmo_colem_frame_count",
    "bmo_colem_rom_checksum",
    "bmo_colem_button_state",
    "bmo_colem_joystick_state",
    "bmo_colem_last_error",
    "bmo_colem_is_running",
    "bmo_colem_save_state",
    "bmo_colem_load_state",
    "bmo_colem_destroy",
    "bmo_face_init",
    "bmo_face_set_clock",
    "bmo_face_step",
    "bmo_face_framebuffer",
    "bmo_face_set_expression",
    "bmo_face_get_expression",
    "bmo_face_set_gaze",
    "bmo_face_trigger_wink",
    "bmo_face_trigger_blush",
    "bmo_face_trigger_joy",
    "bmo_face_tickle",
    "bmo_face_pet_companion",
    "bmo_face_say_quote",
    "bmo_face_clear_speech",
    "bmo_face_toggle_dance",
    "bmo_face_is_dancing",
    "bmo_face_has_speech",
    "bmo_face_visible_chars",
    "bmo_face_current_quote",
    "bmo_face_buddy_level",
    "bmo_face_happiness",
    "bmo_face_speech_buffer",
    "bmo_face_speech_capacity",
    "bmo_face_say_buffer",
    "bmo_face_start_dance",
    "bmo_face_stop_dance",
    "bmo_face_quote_count",
    "bmo_face_quote_text",
)


def _find_compiler() -> Path:
    sdk_root = Path(os.environ.get("WASI_SDK_PATH", LOCAL_WASI_SDK))
    compiler = sdk_root / "bin" / "clang++.exe"
    if not compiler.is_file():
        compiler = sdk_root / "bin" / "clang++"
    if compiler.is_file():
        return compiler
    available = shutil.which("clang++")
    if available:
        return Path(available)
    raise FileNotFoundError(
        "WASI SDK clang++ was not found. Set WASI_SDK_PATH or install wasi-sdk."
    )


def _find_c_compiler() -> Path:
    c_compiler = _find_compiler().with_name("clang.exe" if os.name == "nt" else "clang")
    if c_compiler.is_file():
        return c_compiler
    available = shutil.which("clang")
    if available:
        return Path(available)
    raise FileNotFoundError(
        "WASI SDK clang was not found. Set WASI_SDK_PATH or install wasi-sdk."
    )


def _build_compile_commands(
    cache_dir: Path, c_compiler: Path, compiler: Path
) -> tuple[list[tuple[Path, Path, Path, list[str]]], list[Path]]:
    c_sources = [AGNES_SOURCE, *sorted(SMSPLUS_DIR.glob("*.c")), *sorted(STELLA_SRC_DIR.glob("*.c"))]
    stella_cpp_sources = sorted(STELLA_SRC_DIR.glob("*.cpp"))
    compile_units: list[tuple[Path, Path, Path, list[str]]] = []
    object_paths: list[Path] = []
    common_flags = ["--target=wasm32-wasip1", "-Os", "-flto", "-ffunction-sections", "-fdata-sections"]

    for idx, source in enumerate(c_sources):
        output = cache_dir / f"c_{idx}_{source.stem}.o"
        dep_file = output.with_suffix(".d")
        object_paths.append(output)
        cmd = [
            str(c_compiler), *common_flags, "-MMD", "-MF", str(dep_file),
            "-I", str(SMSPLUS_DIR), "-I", str(STELLA_SRC_DIR),
            "-c", str(source), "-o", str(output),
        ]
        compile_units.append((source, output, dep_file, cmd))

    for idx, source in enumerate(stella_cpp_sources):
        output = cache_dir / f"stella_{idx}_{source.stem}.o"
        dep_file = output.with_suffix(".d")
        object_paths.append(output)
        cmd = [
            str(compiler), *common_flags, "-MMD", "-MF", str(dep_file),
            "-fno-exceptions", "-fno-rtti", "-std=c++17",
            "-I", str(STELLA_SRC_DIR), "-c", str(source), "-o", str(output),
        ]
        compile_units.append((source, output, dep_file, cmd))
    return compile_units, object_paths


def _parse_dep_paths(dep_file: Path) -> list[Path]:
    if not dep_file.is_file():
        return []
    raw = dep_file.read_text(encoding="utf-8", errors="ignore").replace("\\\r\n", " ").replace("\\\n", " ")
    if ":" not in raw:
        return []
    deps_text = raw.split(":", 1)[1].strip()
    tokens: list[str] = []
    current: list[str] = []
    escaped = False
    for ch in deps_text:
        if escaped:
            current.append(ch)
            escaped = False
        elif ch == "\\":
            escaped = True
        elif ch.isspace():
            if current:
                tokens.append("".join(current))
                current = []
        else:
            current.append(ch)
    if current:
        tokens.append("".join(current))
    return [Path(tok) for tok in tokens]


def _object_is_fresh(source: Path, obj_path: Path, dep_file: Path) -> bool:
    if not obj_path.is_file() or not dep_file.is_file():
        return False
    try:
        obj_mtime = obj_path.stat().st_mtime_ns
        if source.stat().st_mtime_ns > obj_mtime:
            return False
        deps = _parse_dep_paths(dep_file)
        if not deps:
            return False
        return all(dep.is_file() and dep.stat().st_mtime_ns <= obj_mtime for dep in deps)
    except OSError:
        return False


def _cache_dir_for(compiler: Path, c_compiler: Path) -> Path:
    sig_src = f"{compiler}:{compiler.stat().st_mtime_ns}:{c_compiler}:{c_compiler.stat().st_mtime_ns}".encode()
    tag = hashlib.sha256(sig_src).hexdigest()[:12]
    cache_dir = REPO_ROOT / "build" / "wasm-cache" / tag
    cache_dir.mkdir(parents=True, exist_ok=True)
    return cache_dir


def _build_link_command(compiler: Path, object_paths: list[Path], out_file: Path) -> list[str]:
    return [
        str(compiler), "--target=wasm32-wasip1", "-Os", "-flto", "-fno-exceptions", "-fno-rtti",
        "-I", str(BMO_COMPAT), "-I", str(WASM_HAL), "-I", str(STELLA_SRC_DIR), "-I", str(SMSPLUS_DIR),
        "-DLOG_LEVEL=0", "-nostartfiles", "-DPEANUT_GB_IS_LITTLE_ENDIAN=1", "-DPEANUT_GB_HIGH_LCD_ACCURACY=1",
        "-std=c++17",
        str(CORE_SOURCE), str(NES_SOURCE), str(ATARI_WASM_SOURCE), str(COLEM_WASM_SOURCE),
        str(BMO_FACE_FIRMWARE_SOURCE), str(BMO_FACE_WASM_SOURCE),
        *(str(obj) for obj in object_paths),
        "-Wl,--no-entry", "-Wl,--gc-sections", "-Wl,--strip-all", "-Wl,--export-memory",
        "-Wl,--initial-memory=16777216", "-Wl,--max-memory=67108864", "-Wl,-z,stack-size=1048576",
        *(f"-Wl,--export={symbol}" for symbol in EXPORTS),
        "-o", str(out_file),
    ]


def build_wasm_module(output_path: Path | None = WASM_OUT_PATH) -> bytes:
    """Compile the firmware-derived WebAssembly cores using an incremental .o/.d cache."""
    compiler = _find_compiler()
    c_compiler = _find_c_compiler()
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
    cache_dir = _cache_dir_for(compiler, c_compiler)
    compile_units, object_paths = _build_compile_commands(cache_dir, c_compiler, compiler)
    dirty_commands = [cmd for src, obj, dep, cmd in compile_units if not _object_is_fresh(src, obj, dep)]
    temp_output = cache_dir / "bmo_playtest_cores.wasm.tmp"
    link_cmd = _build_link_command(compiler, object_paths, temp_output)

    def _compile_unit(cmd: list[str]) -> None:
        subprocess.run(cmd, cwd=REPO_ROOT, check=True, capture_output=True, text=True)

    try:
        if dirty_commands:
            with ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 4)) as pool:
                list(pool.map(_compile_unit, dirty_commands))
        subprocess.run(link_cmd, cwd=REPO_ROOT, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as error:
        raise RuntimeError(
            f"WASI core compilation failed for {error.cmd[0]}:\n{error.stdout}{error.stderr}"
        ) from error
    wasm_bytes = temp_output.read_bytes()
    temp_output.unlink(missing_ok=True)
    if not wasm_bytes.startswith(b"\0asm\x01\0\0\0"):
        raise RuntimeError("WASI compiler produced an invalid WebAssembly module")
    if output_path is not None:
        output_path.write_bytes(wasm_bytes)
        b64_payload = base64.b64encode(wasm_bytes).decode("ascii")
        output_path.with_suffix(".b64.js").write_text(
            f'window.BMO_PLAYTEST_WASM_B64 = "{b64_payload}";\n',
            encoding="utf-8",
        )
    return wasm_bytes


def main() -> int:
    wasm_bytes = build_wasm_module()
    print(f"[WASM] Compiled browser gameplay cores: {WASM_OUT_PATH} ({len(wasm_bytes)} bytes)")
    return 0


if __name__ == "__main__":
    main()
