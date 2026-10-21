"""
tests/test_native_cores_execution.py — Native Host C Execution & 50-Frame Visual Regression Suite
Compiles and executes tools/host_cores_runner.c against the actual C emulator sources
(atari.c, colem.c, lynx.c, ngp.c, snes.c, wswan.c, genesis.c, sms.c, pce.c, pico.c)
and also tests Phase D QoL features (Recursive Folder Navigation, Console Bezels,
Cover Art Converter, and Serial Telemetry Plotter).
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts import fetch_covers
from tools import telemetry_plotter

REPO_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"


def _find_vcvars64() -> Path | None:
    candidates = [
        Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
    ]
    for c in candidates:
        if c.exists():
            return c
    return None


class TestNativeCoresExecutionAndPhaseD(unittest.TestCase):
    """Executes all 10 custom C emulator cores natively on host CPU and verifies Phase D tools."""

    def test_01_compile_and_execute_all_10_c_emulator_cores_50_frames(self):
        vcvars = _find_vcvars64()
        gcc = shutil.which("gcc") or shutil.which("clang")
        if not vcvars and not gcc:
            self.skipTest("No host C compiler (MSVC cl.exe or gcc/clang) available on this machine")

        c_sources = [
            REPO_ROOT / "tools" / "host_cores_runner.c",
            FIRMWARE_DIR / "src" / "vendor" / "stella" / "atari.c",
            FIRMWARE_DIR / "src" / "vendor" / "colem" / "colem.c",
            FIRMWARE_DIR / "src" / "vendor" / "lynx" / "lynx.c",
            FIRMWARE_DIR / "src" / "vendor" / "ngp" / "ngp.c",
            FIRMWARE_DIR / "src" / "vendor" / "snes" / "snes.c",
            FIRMWARE_DIR / "src" / "vendor" / "wswan" / "wswan.c",
            FIRMWARE_DIR / "src" / "vendor" / "genesis" / "genesis.c",
            FIRMWARE_DIR / "src" / "vendor" / "smsplus" / "sms.c",
            FIRMWARE_DIR / "src" / "vendor" / "pce" / "pce.c",
            FIRMWARE_DIR / "src" / "vendor" / "pico" / "pico.c",
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            exe_path = tmp_path / "host_cores_runner.exe"
            if vcvars:
                bat_file = tmp_path / "build_and_run.bat"
                src_args = " ".join(f'"{s}"' for s in c_sources)
                bat_file.write_text(
                    f'@echo off\r\n'
                    f'call "{vcvars}" >nul\r\n'
                    f'cl /nologo /O2 /Fe:"{exe_path}" /Fo:"{tmp_path}\\\\" {src_args}\r\n'
                    f'if errorlevel 1 exit /b 1\r\n'
                    f'"{exe_path}"\r\n',
                    encoding="utf-8",
                )
                proc = subprocess.run([str(bat_file)], capture_output=True, text=True, timeout=60)
            else:
                cmd = [gcc, "-O2", "-o", str(exe_path)] + [str(s) for s in c_sources]
                subprocess.run(cmd, check=True, capture_output=True, text=True, timeout=60)
                proc = subprocess.run([str(exe_path)], capture_output=True, text=True, timeout=60)

            self.assertEqual(proc.returncode, 0, msg=f"Host runner failed:\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")
            self.assertIn("ALL 10 HOST EMULATOR CORES PASSED 50-FRAME EXECUTION & VISUAL REGRESSION!", proc.stdout)
            for core_tag in [
                "ATARI_2600",
                "COLECO",
                "ATARI_LYNX",
                "NGP_COLOR",
                "SNES_65C816",
                "WONDERSWAN",
                "GENESIS_68K",
                "SMS_Z80",
                "PC_ENGINE",
                "PICO_8",
            ]:
                self.assertIn(f"[HOST_CORE] {core_tag}", proc.stdout)
                self.assertNotIn("STATUS=FAIL", proc.stdout)

    def test_02_recursive_folder_hierarchy_navigation_in_sd_card(self):
        sd_h = (FIRMWARE_DIR / "src" / "core" / "sd_card.h").read_text(encoding="utf-8")
        sd_cpp = (FIRMWARE_DIR / "src" / "core" / "sd_card.cpp").read_text(encoding="utf-8")
        self.assertIn("scanDirectory", sd_h)
        self.assertIn("SDCard::scanDirectory", sd_cpp)
        self.assertIn("isIgnoredDirName", sd_cpp)

    def test_03_custom_console_bezels_and_pause_menu_qol_wiring(self):
        disp_h = (FIRMWARE_DIR / "src" / "core" / "display_emu.h").read_text(encoding="utf-8")
        disp_cpp = (FIRMWARE_DIR / "src" / "core" / "display_emu.cpp").read_text(encoding="utf-8")
        ino = (FIRMWARE_DIR / "BmoGameboy.ino").read_text(encoding="utf-8")
        for sym in ["setBezelEnabled", "isBezelEnabled", "toggleBezel", "drawConsoleBezel", "getLastFramebuffer"]:
            self.assertIn(sym, disp_h)
            self.assertIn(f"DisplayEmu::{sym}", disp_cpp)
        self.assertIn("DisplayEmu::drawConsoleBezel(selectedEmulatorIndex)", ino)
        self.assertIn("SaveManager::saveScreenshotBmp", ino)

    def test_04_fetch_covers_bgr565_and_bmp_generator(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            out_dir = Path(tmpdir)
            raw_path, bmp_path = fetch_covers.write_cover_files("Virtual_BMO_Test", out_dir)
            self.assertTrue(raw_path.exists())
            self.assertTrue(bmp_path.exists())
            self.assertEqual(raw_path.stat().st_size, 64 * 64 * 2)
            bmp_bytes = bmp_path.read_bytes()
            self.assertEqual(bmp_bytes[:2], b"BM")
            self.assertEqual(len(bmp_bytes), 54 + 64 * 64 * 3)
            url = fetch_covers.build_libretro_thumbnail_url("Zelda (USA).gbc")
            self.assertIsNotNone(url)
            self.assertIn("Nintendo%20-%20Game%20Boy%20Color", url)

    def test_05_telemetry_plotter_parser_and_summary(self):
        text = telemetry_plotter.generate_synthetic_telemetry(10)
        samples = telemetry_plotter.parse_telemetry_stream(text)
        self.assertEqual(len(samples), 10)
        summary = telemetry_plotter.summarize_telemetry(samples)
        self.assertGreaterEqual(summary.mean_fps, 59.0)
        self.assertGreater(summary.mean_headroom_percent, 50.0)
        plot = telemetry_plotter.render_ascii_plot(samples)
        self.assertIn("BMO GAMEBOY REAL-TIME TELEMETRY", plot)

    def test_06_web_simulator_webserial_sd_manager_and_rom_dropzone(self):
        html = (REPO_ROOT / "tools" / "bmo_simulator" / "index.html").read_text(encoding="utf-8")
        js = (REPO_ROOT / "tools" / "bmo_simulator" / "simulator.js").read_text(encoding="utf-8")
        self.assertIn("webserial-sd-manager", html)
        self.assertIn("connectWebSerial", js)
        self.assertIn("ingestUploadedRoms", js)

    def test_07_guardian_ai_workflow_benchmark_100_percent(self):
        from tools.guardian.cli import cmd_benchmark_ai
        import argparse
        rc = cmd_benchmark_ai(argparse.Namespace())
        self.assertEqual(rc, 0)

    def test_08_wasm_playtest_cores_binary_and_engine_execution(self):
        from scripts import build_wasm_cores

        wasm_bytes = build_wasm_cores.build_wasm_module()
        self.assertEqual(wasm_bytes[:8], b"\x00asm\x01\x00\x00\x00")
        wasm_file = REPO_ROOT / "tools" / "bmo_simulator" / "bmo_playtest_cores.wasm"
        self.assertTrue(wasm_file.exists())
        self.assertEqual(wasm_file.read_bytes(), wasm_bytes)

        node_bin = shutil.which("node")
        if node_bin:
            js_test = (
                "const {WasmPlaytestEngine}=require('./tools/bmo_simulator/wasm_playtest_engine.js');"
                "(async()=>{"
                "  const eng=new WasmPlaytestEngine();"
                "  await eng.init();"
                "  const csum=eng.loadRom('ROM_GB','Tetris',new Uint8Array(2048).fill(0x5A));"
                "  eng.setButtonBit(8,true);"
                "  const f1=eng.stepFrame();"
                "  eng.saveState(0);"
                "  eng.stepFrame();"
                "  eng.loadState(0);"
                "  const fbPtr=eng.exports.wasm_core_get_fb_ptr();"
                "  const fb=new Uint16Array(eng.exports.memory.buffer,fbPtr,160*144);"
                "  let nz=0; for(let i=0;i<fb.length;i++) if(fb[i]) nz++;"
                "  if(f1!==1 || eng.frameCount!==1 || nz<20000 || csum===0) process.exit(1);"
                "  console.log('WASM_PLAYTEST_PASS csum=0x'+csum.toString(16)+' nz='+nz);"
                "})();"
            )
            proc = subprocess.run([node_bin, "-e", js_test], cwd=str(REPO_ROOT), capture_output=True, text=True, timeout=15)
            self.assertEqual(proc.returncode, 0, msg=f"Node Wasm test failed: {proc.stderr}\n{proc.stdout}")
            self.assertIn("WASM_PLAYTEST_PASS", proc.stdout)


if __name__ == "__main__":
    unittest.main()
