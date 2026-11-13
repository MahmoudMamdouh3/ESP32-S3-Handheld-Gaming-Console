#!/usr/bin/env python3
"""Compile firmware emulator sources used by browser gameplay to WebAssembly."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
WASM_OUT_PATH = REPO_ROOT / "tools" / "bmo_simulator" / "bmo_playtest_cores.wasm"
CORE_SOURCE = REPO_ROOT / "tools" / "bmo_simulator" / "gameboy_wasm.cpp"
NES_SOURCE = REPO_ROOT / "tools" / "bmo_simulator" / "nes_wasm.cpp"
AGNES_SOURCE = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "vendor" / "agnes" / "agnes.c"
ATARI_SOURCE = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "vendor" / "stella" / "atari.c"
ATARI_WASM_SOURCE = REPO_ROOT / "tools" / "bmo_simulator" / "atari_wasm.cpp"
COLEM_SOURCE = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "vendor" / "colem" / "colem.c"
COLEM_WASM_SOURCE = REPO_ROOT / "tools" / "bmo_simulator" / "colem_wasm.cpp"
BMO_FACE_FIRMWARE_SOURCE = REPO_ROOT / "tools" / "bmo_simulator" / "bmo_face_firmware.cpp"
BMO_FACE_WASM_SOURCE = REPO_ROOT / "tools" / "bmo_simulator" / "bmo_face_wasm.cpp"
BMO_COMPAT = REPO_ROOT / "tools" / "bmo_simulator" / "bmo_face_compat"
WASM_HAL = REPO_ROOT / "tools" / "bmo_simulator" / "hal"
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


def build_wasm_module(output_path: Path | None = WASM_OUT_PATH) -> bytes:
    """Compile the firmware-derived WebAssembly cores and optionally write the output."""
    compiler = _find_compiler()
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_root = REPO_ROOT / "build" / "wasm-tmp"
    temp_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="bmo-wasm-", dir=str(temp_root)) as temp_dir:
        temp_output = Path(temp_dir) / "bmo_playtest_cores.wasm"
        agnes_object = Path(temp_dir) / "agnes.o"
        c_compiler = _find_c_compiler()
        c_objects = []
        c_commands = []
        for source in (AGNES_SOURCE, ATARI_SOURCE, COLEM_SOURCE):
            output = Path(temp_dir) / f"{source.stem}.o"
            c_objects.append(output)
            c_commands.append([
                str(c_compiler),
                "--target=wasm32-wasip1",
                "-O3",
                "-flto",
                "-c",
                str(source),
                "-o",
                str(output),
            ])
        command = [
            str(compiler),
            "--target=wasm32-wasip1",
            "-O3",
            "-flto",
            "-fno-exceptions",
            "-fno-rtti",
            "-I",
            str(BMO_COMPAT),
            "-I",
            str(WASM_HAL),
            "-DLOG_LEVEL=0",
            "-nostartfiles",
            "-DPEANUT_GB_IS_LITTLE_ENDIAN=1",
            "-DPEANUT_GB_HIGH_LCD_ACCURACY=1",
            "-std=c++17",
            str(CORE_SOURCE),
            str(NES_SOURCE),
            str(ATARI_WASM_SOURCE),
            str(COLEM_WASM_SOURCE),
            str(BMO_FACE_FIRMWARE_SOURCE),
            str(BMO_FACE_WASM_SOURCE),
            *(str(obj) for obj in c_objects),
            "-Wl,--no-entry",
            "-Wl,--export-memory",
            "-Wl,--initial-memory=16777216",
            "-Wl,--max-memory=33554432",
            "-Wl,-z,stack-size=1048576",
            *(f"-Wl,--export={symbol}" for symbol in EXPORTS),
            "-o",
            str(temp_output),
        ]
        try:
            for c_command in c_commands:
                subprocess.run(c_command, cwd=REPO_ROOT, check=True, capture_output=True, text=True)
            subprocess.run(command, cwd=REPO_ROOT, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as error:
            raise RuntimeError(
                f"WASI core compilation failed for {error.cmd[0]}:\n{error.stdout}{error.stderr}"
            ) from error
        wasm_bytes = temp_output.read_bytes()
        if not wasm_bytes.startswith(b"\0asm\x01\0\0\0"):
            raise RuntimeError("WASI compiler produced an invalid WebAssembly module")
        if output_path is not None:
            output_path.write_bytes(wasm_bytes)
    return wasm_bytes


def main() -> int:
    wasm_bytes = build_wasm_module()
    print(f"[WASM] Compiled browser gameplay cores: {WASM_OUT_PATH} ({len(wasm_bytes)} bytes)")
    return 0


if __name__ == "__main__":
    main()
