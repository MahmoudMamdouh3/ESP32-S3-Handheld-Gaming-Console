"""
tests/test_native_cores_execution.py — Native Host Execution & 50-Frame Regression Suite
Compiles and executes tests/host/host_cores_runner.c + per-core C/C++ bridges against the genuine
upstream emulator engines under firmware/BmoGameboy/src/third_party/
(stella2014, smsplus, handy, race, snes9x, oswan, gwenesis, huexpress, fake08, agnes)
and also tests Phase D QoL features (Recursive Folder Navigation, Console Bezels,
Cover Art Converter, and Serial Telemetry Plotter).
"""

from concurrent.futures import ThreadPoolExecutor
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts import fetch_covers
from tools import telemetry_plotter
from host_test_support import find_wasi_cxx, require_compiler

REPO_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"


class TestNativeCoresExecutionAndPhaseD(unittest.TestCase):
    """Executes all 11 genuine upstream emulator engines on host CPU and verifies Phase D tools."""

    def test_01_compile_and_execute_all_11_c_emulator_cores_50_frames(self):
        wasi_cxx = find_wasi_cxx(REPO_ROOT)
        node = shutil.which("node")
        gcc = shutil.which("gcc")
        gxx = shutil.which("g++")
        use_wasi = bool(wasi_cxx and node)
        if not use_wasi and not (gcc and gxx):
            require_compiler(self, None, "WASI SDK clang++/node or GNU gcc/g++")

        third_party = FIRMWARE_DIR / "src" / "third_party"
        sim_hal = REPO_ROOT / "simulator" / "hal"
        fw_hal = FIRMWARE_DIR / "src" / "hal"
        host_dir = REPO_ROOT / "tests" / "host"

        groups = [
            ("runner", [host_dir / "host_cores_runner.c"], [third_party]),
            ("agnes", [third_party / "agnes" / "agnes.c"], [third_party / "agnes"]),
            ("smsplus", sorted((third_party / "smsplus").glob("*.c")), [third_party / "smsplus"]),
            ("pce", [host_dir / "bridge_pce.c", *sorted((third_party / "huexpress").glob("*.c"))], [third_party / "huexpress"]),
            ("ngp", [host_dir / "bridge_ngp.c", *sorted((third_party / "race").glob("*.c"))], [third_party / "race"]),
            ("snes", [host_dir / "bridge_snes.c", *sorted((third_party / "snes9x" / "src").glob("*.c"))], [third_party / "snes9x", third_party / "snes9x" / "src"]),
            ("wswan", [host_dir / "bridge_wswan.c", *sorted((third_party / "oswan").rglob("*.c"))], [third_party / "oswan", third_party / "oswan" / "cpu"]),
            ("genesis", [host_dir / "bridge_genesis.c", *sorted((third_party / "gwenesis").rglob("*.c"))], [
                third_party / "gwenesis",
                third_party / "gwenesis" / "bus",
                third_party / "gwenesis" / "cpus" / "M68K",
                third_party / "gwenesis" / "cpus" / "Z80",
                third_party / "gwenesis" / "io",
                third_party / "gwenesis" / "savestate",
                third_party / "gwenesis" / "sound",
                third_party / "gwenesis" / "vdp",
            ]),
            ("stella", [*sorted((third_party / "stella2014" / "src").glob("*.c")), *sorted((third_party / "stella2014" / "src").glob("*.cpp"))], [third_party / "stella2014" / "src"]),
            ("handy", sorted((third_party / "handy").glob("*.cpp")), [third_party / "handy"]),
            ("fake08", sorted((third_party / "fake08" / "src").glob("*.cpp")), [third_party / "fake08" / "src"]),
            ("cpp_bridge", [host_dir / "host_cores_cpp_bridge.cpp"], [
                third_party,
                third_party / "stella2014" / "src",
                third_party / "handy",
                third_party / "fake08" / "src",
            ]),
        ]

        c_compiler = (
            str(wasi_cxx.with_name("clang.exe" if os.name == "nt" else "clang"))
            if use_wasi else gcc
        )
        cxx_compiler = str(wasi_cxx) if use_wasi else gxx

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            compile_tasks: list[tuple[list[str], Path]] = []
            obj_paths: list[Path] = []
            for group_name, sources, includes in groups:
                group_obj_dir = tmp_path / "objs" / group_name
                group_obj_dir.mkdir(parents=True, exist_ok=True)
                inc_flags = [f"-I{sim_hal}", f"-I{fw_hal}", *[f"-I{inc}" for inc in includes]]
                for idx, src in enumerate(sources):
                    obj = group_obj_dir / f"{idx:03d}_{src.stem}.o"
                    obj_paths.append(obj)
                    is_cpp = src.suffix.lower() == ".cpp"
                    cmd = [cxx_compiler if is_cpp else c_compiler]
                    if use_wasi:
                        cmd.extend([
                            "--target=wasm32-wasip1",
                            "-D_WASI_EMULATED_SIGNAL",
                            "-D_WASI_EMULATED_PROCESS_CLOCKS",
                            "-fwasm-exceptions",
                            "-mllvm",
                            "-wasm-enable-eh",
                            "-mllvm",
                            "-wasm-enable-sjlj",
                            "-mllvm",
                            "-wasm-use-legacy-eh=false",
                        ])
                    cmd.extend(["-O2", "-c", *inc_flags])
                    if is_cpp:
                        cmd.extend(["-std=c++17", "-fno-rtti"])
                        if not use_wasi:
                            cmd.append("-fno-exceptions")
                    else:
                        cmd.append("-std=gnu11")
                        if group_name not in ("runner",):
                            cmd.extend([
                                "-Wno-incompatible-function-pointer-types",
                                "-Wno-int-conversion",
                                "-include",
                                "stdint.h",
                                "-include",
                                "stddef.h",
                            ])
                    cmd.extend([str(src), "-o", str(obj)])
                    compile_tasks.append((cmd, src))

            def _compile_one(task: tuple[list[str], Path]):
                cmd, src = task
                res = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO_ROOT, timeout=60)
                if res.returncode != 0:
                    raise AssertionError(
                        f"Compile failed for {src}:\nCMD: {' '.join(cmd)}\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}"
                    )

            with ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 4)) as pool:
                list(pool.map(_compile_one, compile_tasks))

            if use_wasi:
                out_bin = tmp_path / "host_cores_runner.wasm"
                link_cmd = [
                    cxx_compiler,
                    "--target=wasm32-wasip1",
                    "-fno-rtti",
                    "-fwasm-exceptions",
                    "-Wl,-mllvm,-wasm-enable-eh",
                    "-Wl,-mllvm,-wasm-enable-sjlj",
                    "-Wl,-mllvm,-wasm-use-legacy-eh=false",
                    "-Wl,-z,stack-size=1048576",
                    "-Wl,--max-memory=134217728",
                    *[str(o) for o in obj_paths],
                    "-lwasi-emulated-signal",
                    "-lwasi-emulated-process-clocks",
                    "-lsetjmp",
                    "-lunwind",
                    "-o",
                    str(out_bin),
                ]
                link_res = subprocess.run(link_cmd, capture_output=True, text=True, cwd=REPO_ROOT, timeout=60)
                self.assertEqual(
                    link_res.returncode,
                    0,
                    msg=f"WASI link failed:\nSTDOUT:\n{link_res.stdout}\nSTDERR:\n{link_res.stderr}",
                )
                node_runner = (
                    "const fs=require('node:fs');"
                    "const {WASI}=require('node:wasi');"
                    "const wasi=new WASI({version:'preview1',args:[process.argv[1]],env:process.env,preopens:{}});"
                    "WebAssembly.compile(fs.readFileSync(process.argv[1])).then(m=>"
                    "wasi.start(new WebAssembly.Instance(m,{wasi_snapshot_preview1:wasi.wasiImport})))"
                    ".catch(e=>{console.error(e);process.exit(1);});"
                )
                proc = subprocess.run(
                    [node, "-e", node_runner, str(out_bin)],
                    capture_output=True,
                    text=True,
                    cwd=REPO_ROOT,
                    timeout=120,
                )
            else:
                out_bin = tmp_path / "host_cores_runner"
                link_cmd = [cxx_compiler, *[str(o) for o in obj_paths], "-lm", "-o", str(out_bin)]
                subprocess.run(link_cmd, check=True, capture_output=True, text=True, cwd=REPO_ROOT, timeout=60)
                proc = subprocess.run([str(out_bin)], capture_output=True, text=True, cwd=REPO_ROOT, timeout=120)

            self.assertEqual(proc.returncode, 0, msg=f"Host runner failed:\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")
            self.assertIn("ALL 11 HOST EMULATOR CORES (INCLUDING NES AGNES + STATE DETERMINISM) PASSED!", proc.stdout)
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
                "NES_AGNES",
            ]:
                self.assertIn(f"[HOST_CORE] {core_tag}", proc.stdout)
                self.assertNotIn("STATUS=FAIL", proc.stdout)

    def test_02_recursive_folder_hierarchy_navigation_in_sd_card(self):
        sd_h = (FIRMWARE_DIR / "src" / "storage" / "sd_card.h").read_text(encoding="utf-8")
        sd_cpp = (FIRMWARE_DIR / "src" / "storage" / "sd_catalog.cpp").read_text(encoding="utf-8")
        self.assertIn("scanDirectory", sd_h)
        self.assertIn("SDCard::scanDirectory", sd_cpp)
        self.assertIn("isIgnoredDirName", sd_cpp)

    def test_03_custom_console_bezels_and_pause_menu_qol_wiring(self):
        disp_h = (FIRMWARE_DIR / "src" / "ui" / "display_emu.h").read_text(encoding="utf-8")
        disp_cpp = (FIRMWARE_DIR / "src" / "ui" / "display_worker.cpp").read_text(encoding="utf-8")
        app_cpp = (FIRMWARE_DIR / "src" / "app" / "bmo_app.cpp").read_text(encoding="utf-8")
        for sym in ["setBezelEnabled", "isBezelEnabled", "toggleBezel", "drawConsoleBezel", "getLastFramebuffer"]:
            self.assertIn(sym, disp_h)
            self.assertIn(f"DisplayEmu::{sym}", disp_cpp)
        self.assertIn("DisplayEmu::drawConsoleBezel(s_selectedEmulatorIndex)", app_cpp)
        self.assertIn("SaveManager::saveScreenshotBmp", app_cpp)

    def test_04_fetch_covers_bgr565_and_bmp_generator(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            out_dir = Path(tmpdir)
            raw_path, bmp_path = fetch_covers.write_cover_files(
                "Homebrew_Test", out_dir, rom_relative_path="gb/Homebrew_Test.gb"
            )
            self.assertTrue(raw_path.exists())
            self.assertTrue(bmp_path.exists())
            self.assertEqual(raw_path.stat().st_size, 64 * 64 * 2)
            bmp_bytes = bmp_path.read_bytes()
            self.assertEqual(bmp_bytes[:2], b"BM")
            self.assertEqual(len(bmp_bytes), 54 + 64 * 64 * 3)
            self.assertFalse(hasattr(fetch_covers, "build_libretro_thumbnail_url"))
            self.assertEqual(fetch_covers.slugify_title("Demo Cart (PD).gb"), "demo_cart")

    def test_05_telemetry_plotter_parser_and_summary(self):
        text = telemetry_plotter.generate_synthetic_telemetry(10)
        samples = telemetry_plotter.parse_telemetry_stream(text)
        self.assertEqual(len(samples), 10)
        self.assertEqual(samples[0].emulator_index, 1)
        self.assertEqual(samples[0].target_frame_us, 16742)
        windows = telemetry_plotter.parse_frame_percentile_windows(text)
        self.assertEqual(len(windows), 1)
        self.assertEqual(windows[0].frame_count, 600)
        self.assertEqual((windows[0].p50_us, windows[0].p95_us, windows[0].p99_us),
                         (5100, 6200, 7400))
        summary = telemetry_plotter.summarize_telemetry(samples)
        self.assertGreaterEqual(summary.mean_fps, 59.0)
        self.assertGreater(summary.mean_headroom_percent, 50.0)
        plot = telemetry_plotter.render_ascii_plot(
            samples, frame_windows=windows, source_label="SYNTHETIC"
        )
        self.assertIn("BMO HANDHELD TELEMETRY & FRAME PACING REPORT [SYNTHETIC]", plot)
        self.assertIn("p95=6200us", plot)

    def test_06_web_simulator_webserial_sd_manager_and_rom_dropzone(self):
        html = (REPO_ROOT / "simulator" / "index.html").read_text(encoding="utf-8")
        js = (REPO_ROOT / "simulator" / "simulator.js").read_text(encoding="utf-8")
        self.assertIn("webserial-sd-manager", html)
        self.assertIn("connectWebSerial", js)
        self.assertIn("ingestUploadedRoms", js)

    def test_08_wasm_playtest_cores_binary_and_engine_execution(self):
        from scripts import build_wasm_cores

        wasm_bytes = build_wasm_cores.build_wasm_module(output_path=None)
        self.assertEqual(wasm_bytes[:8], b"\x00asm\x01\x00\x00\x00")

        node_bin = shutil.which("node")
        if node_bin:
            rom = bytearray(32 * 1024)
            rom[0x100:0x103] = bytes([0xC3, 0x50, 0x01])
            rom[0x134:0x13C] = b"BMO TEST"
            rom[0x147:0x14A] = bytes([0, 0, 0])
            code = [0x3E, 0xE4, 0xE0, 0x47, 0x21, 0x00, 0x80]
            for _ in range(8):
                code.extend([0x3E, 0xAA, 0x22, 0x3E, 0x55, 0x22])
            code.extend([0x3E, 0x91, 0xE0, 0x40, 0x18, 0xFE])
            rom[0x150:0x150 + len(code)] = bytes(code)
            checksum = 0
            for byte in rom[0x134:0x14D]:
                checksum = (checksum - byte - 1) & 0xFF
            rom[0x14D] = checksum

            nes_rom = bytearray(16 + 16384 + 8192)
            nes_rom[:7] = b"NES\x1a\x01\x01\x02"
            prg = memoryview(nes_rom)[16:16 + 16384]
            prg[:] = b"\xEA" * len(prg)
            prg[0x3FFC:0x3FFE] = b"\x00\x80"
            nes_program = bytes([
                0xA9, 0x5C, 0x8D, 0x00, 0x60,
                0xA9, 0x3F, 0x8D, 0x06, 0x20,
                0xA9, 0x00, 0x8D, 0x06, 0x20,
                0xA9, 0x21, 0x8D, 0x07, 0x20,
                0xA9, 0x08, 0x8D, 0x01, 0x20,
                0xE8, 0x8E, 0x01, 0x60, 0x4C, 0x18, 0x80
            ])
            prg[:len(nes_program)] = nes_program
            atari_rom = bytearray(b"\xEA" * 4096)
            atari_rom[0xFFC:0xFFE] = b"\x00\xF0"
            atari_program = bytes([
                0xA9, 0x88, 0x85, 0x80,
                0xA9, 0x2E, 0x85, 0x08,
                0xA9, 0x44, 0x85, 0x09,
                0xA9, 0xAA, 0x85, 0x0E,
                0xA9, 0x02, 0x85, 0x02,
                0x85, 0x00, 0x85, 0x02,
                0x85, 0x02, 0x85, 0x02,
                0xA9, 0x00, 0x85, 0x00,
                0x85, 0x01, 0xA2, 0xFA,
                0x85, 0x02, 0xCA, 0xD0,
                0xFB, 0x4C, 0x10, 0xF0
            ])
            atari_rom[:len(atari_program)] = atari_program
            colem_rom = bytearray(8192)
            colem_rom[0] = 0x55
            colem_rom[1] = 0xAA
            colem_rom[0x0A:0x0C] = b"\x20\x80"
            colem_program = bytes([
                0x3E, 0x5A, 0x32, 0x00, 0x60, 0xD3, 0xBF,
                0x3E, 0x87, 0xD3, 0xBF, 0xC3, 0x29, 0x80
            ])
            colem_rom[0x20:0x20 + len(colem_program)] = colem_program

            with tempfile.TemporaryDirectory() as tmp:
                rom_path = Path(tmp) / "homebrew_test.gb"
                rom_path.write_bytes(rom)
                nes_rom_path = Path(tmp) / "homebrew_test.nes"
                nes_rom_path.write_bytes(nes_rom)
                atari_rom_path = Path(tmp) / "homebrew_test.a26"
                atari_rom_path.write_bytes(atari_rom)
                colem_rom_path = Path(tmp) / "homebrew_test.col"
                colem_rom_path.write_bytes(colem_rom)
                wasm_path = Path(tmp) / "bmo_playtest_cores.wasm"
                wasm_path.write_bytes(wasm_bytes)
                js_test = (
                    "const fs=require('fs');"
                    "const originalReadFileSync=fs.readFileSync.bind(fs);"
                    "fs.readFileSync=(file,...args)=>{"
                    "  const normalized=String(file).replace(/\\\\/g,'/');"
                    "  return normalized.endsWith('/simulator/bmo_playtest_cores.wasm')"
                    "    ? originalReadFileSync(process.argv[5],...args)"
                    "    : originalReadFileSync(file,...args);"
                    "};"
                    "const {WasmPlaytestEngine}=require('./simulator/wasm_playtest_engine.js');"
                    "(async()=>{"
                    "  const eng=new WasmPlaytestEngine();"
                    "  await eng.init();"
                    "  const facePtr=eng.exports.bmo_face_framebuffer();"
                    "  const facePixels=new Uint16Array(eng.exports.memory.buffer,facePtr,320*240);"
                    "  const faceColors=new Set(facePixels).size;"
                    "  eng.exports.bmo_face_say_quote(0,3800);"
                    "  if(eng.exports.bmo_face_has_speech()!==1 || eng.bmoCurrentQuote()!=='Who wants to play video games?!' || eng.bmoVisibleChars()<1)"
                    "    throw new Error('firmware BMO dialogue did not initialize');"
                    "  eng.exports.bmo_face_trigger_wink();"
                    "  if(eng.exports.bmo_face_get_expression()!==11) throw new Error('firmware BMO wink expression did not activate');"
                    "  eng.exports.bmo_face_pet_companion();"
                    "  if(eng.exports.bmo_face_buddy_level()!==1) throw new Error('firmware BMO pet counter did not advance');"
                    "  eng.exports.bmo_face_toggle_dance();"
                    "  const bmoDanceStarted=eng.exports.bmo_face_is_dancing()===1;"
                    "  eng.exports.bmo_face_toggle_dance();"
                    "  if(faceColors<10 || !bmoDanceStarted || eng.exports.bmo_face_is_dancing()!==0)"
                    "    throw new Error(JSON.stringify({faceColors,bmoDanceStarted,danceStopped:eng.exports.bmo_face_is_dancing()}));"
                    "  eng.exports.bmo_face_set_gaze(0.85,-0.85);"
                    "  if(!eng.exports.bmo_face_step((Math.floor(performance.now()*1000)+16667)>>>0))"
                    "    throw new Error('firmware BMO face did not render a frame');"
                    "  const rom=new Uint8Array(fs.readFileSync(process.argv[1]));"
                    "  const csum=eng.loadRom('ROM_GB','Homebrew test',rom);"
                    "  eng.stepFrame(); eng.stepFrame();"
                    "  const fbPtr=eng.exports.bmo_emu_framebuffer();"
                    "  const fb=new Uint16Array(eng.exports.memory.buffer,fbPtr,240*216);"
                    "  const colors=new Set(fb);"
                    "  eng.setButtonBit(1,true); eng.setButtonBit(16,true); eng.stepFrame();"
                    "  const joypad=eng.exports.bmo_emu_joypad_state();"
                    "  eng.saveState(); eng.stepFrame(); const restored=eng.loadState();"
                    "  if(eng.frameCount!==3 || colors.size<2 || !csum || joypad!==(0xff^0x41) || !restored) "
                    "    throw new Error(JSON.stringify({frames:eng.frameCount,colors:colors.size,csum,joypad,restored}));"
                    "  if(eng.exports.bmo_emu_is_running()!==1) throw new Error('core stopped unexpectedly');"
                    "  let unsupported=false; try{eng.loadRom('ROM_SNES','unsupported',rom)}catch(e){unsupported=/supports Game Boy/.test(e.message)}"
                    "  const badRom=rom.slice();badRom[0x14d]^=1;let badChecksum=false;"
                    "  try{eng.loadRom('ROM_GB','bad header',badRom)}catch(e){badChecksum=/checksum is invalid/.test(e.message)}"
                    "  if(!unsupported || !badChecksum || eng.loaded || eng.exports.bmo_emu_is_running()!==0)"
                    "    throw new Error('unsupported or invalid ROM was not rejected explicitly');"
                    "  const nesRom=new Uint8Array(fs.readFileSync(process.argv[2]));"
                    "  const nesChecksum=eng.loadRom('ROM_NES','NES homebrew test',nesRom);"
                    "  eng.stepFrame(); eng.stepFrame();"
                    "  const nesFbPtr=eng.exports.bmo_nes_framebuffer();"
                    "  const nesFb=new Uint16Array(eng.exports.memory.buffer,nesFbPtr,256*240);"
                    "  const nesColorCount=new Set(nesFb).size;"
                    "  eng.setButtonBit(1,true); eng.setButtonBit(16,true); eng.stepFrame();"
                    "  const nesButtons=eng.exports.bmo_nes_button_state();"
                    "  eng.saveState(); eng.stepFrame(); const nesRestored=eng.loadState();"
                    "  if(eng.frameCount!==3 || !nesChecksum || nesColorCount<1 || nesButtons!==0x11 || !nesRestored)"
                    "    throw new Error(JSON.stringify({nesFrames:eng.frameCount,nesColorCount,nesChecksum,nesButtons,nesRestored}));"
                    "  if(eng.exports.bmo_nes_is_running()!==1 || eng.viewport.w!==256 || eng.viewport.h!==240 || eng.viewport.ox!==32)"
                    "    throw new Error('NES core state or firmware viewport mismatch');"
                    "  eng.loadRom('ROM_GB','switch back to Game Boy',rom);"
                    "  if(eng.exports.bmo_nes_is_running()!==0 || eng.exports.bmo_emu_is_running()!==1)"
                    "    throw new Error('switching cores did not release the NES session');"
                    "  const badMapper=nesRom.slice();badMapper[6]=0x30;let mapperRejected=false;"
                    "  try{eng.loadRom('ROM_NES','unsupported mapper',badMapper)}catch(e){mapperRejected=/mappers 0, 1, 2, and 4/.test(e.message)}"
                    "  if(!mapperRejected || eng.loaded || eng.exports.bmo_nes_is_running()!==0 || eng.exports.bmo_emu_is_running()!==0)"
                    "    throw new Error('unsupported NES mapper was not rejected explicitly');"
                    "  const atariRom=new Uint8Array(fs.readFileSync(process.argv[3]));"
                    "  eng.loadRom('ROM_ATARI','Atari homebrew test',atariRom);"
                    "  eng.stepFrame(); eng.stepFrame();"
                    "  const atariFbPtr=eng.exports.bmo_atari_framebuffer();"
                    "  const atariFb=new Uint16Array(eng.exports.memory.buffer,atariFbPtr,160*192);"
                    "  const atariColorCount=new Set(atariFb).size;"
                    "  eng.setButtonBit(1,true); eng.setButtonBit(16,true); eng.stepFrame();"
                    "  const atariButtons=eng.exports.bmo_atari_joypad_state();"
                    "  eng.saveState(); eng.stepFrame(); const atariRestored=eng.loadState();"
                    "  if(eng.frameCount!==3 || atariColorCount<2 || atariButtons!==(0xff^0x11) || !atariRestored)"
                    "    throw new Error(JSON.stringify({atariFrames:eng.frameCount,atariColorCount,atariButtons,atariRestored}));"
                    "  if(eng.exports.bmo_atari_is_running()!==1 || eng.viewport.w!==160 || eng.viewport.h!==192 || eng.viewport.ox!==80 || eng.viewport.oy!==24)"
                    "    throw new Error('Atari core state or firmware viewport mismatch');"
                    "  const badAtari=new Uint8Array(2049);let badSize=false;"
                    "  try{eng.loadRom('ROM_ATARI','bad size',badAtari)}catch(e){badSize=/must be 2, 4, 8, 16, or 32 KiB/.test(e.message)}"
                    "  if(!badSize || eng.loaded || eng.exports.bmo_atari_is_running()!==0)"
                    "    throw new Error('unsupported Atari ROM size was not rejected explicitly');"
                    "  const colemRom=new Uint8Array(fs.readFileSync(process.argv[4]));"
                    "  eng.loadRom('ROM_COLEM','Coleco homebrew test',colemRom);"
                    "  eng.stepFrame(); eng.stepFrame();"
                    "  const colemFbPtr=eng.exports.bmo_colem_framebuffer();"
                    "  const colemFb=new Uint16Array(eng.exports.memory.buffer,colemFbPtr,256*192);"
                    "  const colemColorCount=new Set(colemFb).size;"
                    "  const colemRamPtr=eng.exports.bmo_colem_ram();"
                    "  const colemRam=new Uint8Array(eng.exports.memory.buffer,colemRamPtr,8192);"
                    "  eng.setButtonBit(1,true); eng.setButtonBit(8,true); eng.setButtonBit(16,true); eng.stepFrame();"
                    "  const colemJoystick=eng.exports.bmo_colem_joystick_state();"
                    "  eng.saveState(); eng.stepFrame(); const colemRestored=eng.loadState();"
                    "  if(eng.frameCount!==3 || colemColorCount<1 || colemFb[0]===0 || colemRam[0]!==0x5A || colemJoystick!==0x13 || !colemRestored)"
                    "    throw new Error(JSON.stringify({colemFrames:eng.frameCount,colemColorCount,colemFirstPixel:colemFb[0],colemRam:colemRam[0],colemJoystick,colemRestored}));"
                    "  if(eng.exports.bmo_colem_is_running()!==1 || eng.viewport.w!==256 || eng.viewport.h!==192 || eng.viewport.ox!==32 || eng.viewport.oy!==24)"
                    "    throw new Error('ColecoVision core state or firmware viewport mismatch');"
                    "  let badColecoSize=false; try{eng.loadRom('ROM_COLEM','bad size',new Uint8Array(511))}catch(e){badColecoSize=/at least 512 bytes/.test(e.message)}"
                    "  if(!badColecoSize || eng.loaded || eng.exports.bmo_colem_is_running()!==0)"
                    "    throw new Error('unsupported ColecoVision ROM size was not rejected explicitly');"
                    "  console.log('REAL_GB_NES_ATARI_COLECO_WASM_PASS gbFrames=3 gbColors='+colors.size+' nesFrames=3 nesColors='+nesColorCount+' atariFrames=3 atariColors='+atariColorCount+' colemFrames=3 colemColors='+colemColorCount);"
                    "})().catch(err=>{console.error(err);process.exit(1);});"
                )
                proc = subprocess.run(
                    [node_bin, "-e", js_test, str(rom_path), str(nes_rom_path), str(atari_rom_path),
                     str(colem_rom_path), str(wasm_path)],
                    cwd=str(REPO_ROOT),
                    capture_output=True,
                    text=True,
                    timeout=30,
                )
                self.assertEqual(proc.returncode, 0, msg=f"Node Wasm test failed: {proc.stderr}\n{proc.stdout}")
                self.assertIn("REAL_GB_NES_ATARI_COLECO_WASM_PASS", proc.stdout)

    def test_09_run_simulator_server_handler_endpoints(self):
        from simulator import run_simulator
        self.assertTrue(hasattr(run_simulator, "Handler"))
        self.assertEqual(run_simulator.Handler.extensions_map.get(".wasm"), "application/wasm")
        self.assertTrue(hasattr(run_simulator.Handler, "do_POST"))
        self.assertIn("default-src 'self'", run_simulator.CSP_POLICY)
        self.assertEqual(run_simulator.HOST, "127.0.0.1")
        self.assertTrue(run_simulator.valid_host_header("127.0.0.1:8080"))
        self.assertFalse(run_simulator.valid_host_header("localhost:8080"))
        self.assertTrue(run_simulator.valid_origin_header("http://127.0.0.1:8080"))
        self.assertFalse(run_simulator.valid_origin_header("http://localhost:8080"))
        html = (REPO_ROOT / "simulator" / "index.html").read_text(encoding="utf-8")
        self.assertIn("wasm-playtest-panel", html)
        self.assertIn("btn-wasm-save-state", html)
        self.assertNotIn("btn-verify-host-cores", html)
        self.assertIn('http-equiv="Content-Security-Policy"', html)

    def test_10_top_picks_curation_and_mbc7_rom_playability(self):
        sd_h = (FIRMWARE_DIR / "src" / "storage" / "sd_card.h").read_text(encoding="utf-8")
        sd_cpp = (FIRMWARE_DIR / "src" / "storage" / "sd_catalog.cpp").read_text(encoding="utf-8")
        disp_h = (FIRMWARE_DIR / "src" / "ui" / "display_emu.h").read_text(encoding="utf-8")
        disp_cpp = (FIRMWARE_DIR / "src" / "ui" / "ui_menus.cpp").read_text(encoding="utf-8")
        app_cpp = (FIRMWARE_DIR / "src" / "app" / "bmo_app.cpp").read_text(encoding="utf-8")
        walnut_h = (FIRMWARE_DIR / "src" / "third_party" / "walnut_cgb" / "walnut_cgb.h").read_text(encoding="utf-8")
        peanut_h = (FIRMWARE_DIR / "src" / "third_party" / "peanut_gb" / "peanut_gb.h").read_text(encoding="utf-8")

        self.assertIn("bool isPopularGame(const char* filename, RomType type);", sd_h)
        self.assertIn("int getPopularCountForType(RomType type);", sd_h)
        self.assertIn("SDCard::isPopularGame", sd_cpp)
        self.assertIn("void setTopPicksFilter(bool enabled);", disp_h)
        self.assertIn("TOP PICK", disp_cpp)
        self.assertIn("s_topPicksOnly", app_cpp)
        self.assertIn("0x22", walnut_h)
        self.assertIn("0x22", peanut_h)

    def test_11_bmo_mode_simulator_controls_and_wasm_bridge(self):
        html = (REPO_ROOT / "simulator" / "index.html").read_text(encoding="utf-8")
        js = (REPO_ROOT / "simulator" / "simulator.js").read_text(encoding="utf-8")
        css = (REPO_ROOT / "simulator" / "simulator.css").read_text(encoding="utf-8")

        for required_id in (
            "btn-bmo-mode",
            "bmo-mode-panel",
            "btn-bmo-enter",
            "btn-bmo-exit",
            "btn-bmo-reset",
            "btn-bmo-pet",
            "btn-bmo-tickle",
            "btn-bmo-wink",
            "btn-bmo-blush",
            "btn-bmo-joy",
            "btn-bmo-dance",
            "slider-bmo-bpm",
            "chk-bmo-autocycle",
            "chk-bmo-autosleep",
            "chk-bmo-hud",
            "bmo-gaze-pad",
            "select-bmo-quote",
            "input-bmo-speech",
            "btn-bmo-say-custom",
            "btn-bmo-clear-speech",
        ):
            self.assertIn(f'id="{required_id}"', html)

        expected_expressions = (
            "IDLE", "SURPRISED", "HAPPY", "SLEEPY", "LOW_BATTERY", "CHARGING",
            "ERROR", "SHUTDOWN", "HIDDEN", "JOY", "SLEEPING", "WINK",
            "BLUSH", "CONFUSED", "ANNOYED", "LOVE", "TICKLED", "CELEBRATING",
        )
        for expr_name in expected_expressions:
            self.assertIn(f'data-bmo-expr="{expr_name}"', html)

        self.assertIn('stateParam === "bmo"', js)
        self.assertIn("enterBmoMode()", js)
        self.assertIn("setupBmoModeControls()", js)
        self.assertIn("wrapSpeechLines(", js)
        self.assertIn("isEditableTarget", js)
        self.assertIn("WASM CORE FAILED TO LOAD", js)
        self.assertIn("bmo_playtest_cores.b64.js", html)
        self.assertIn(".bmo-gaze-pad:focus-visible", css)

        import base64
        from scripts import build_wasm_cores
        with tempfile.TemporaryDirectory() as tmp:
            wasm_bin_path = Path(tmp) / "bmo_playtest_cores.wasm"
            b64_js_path = wasm_bin_path.with_suffix(".b64.js")
            wasm_bytes = build_wasm_cores.build_wasm_module(output_path=wasm_bin_path)
            self.assertTrue(b64_js_path.exists())
            b64_text = b64_js_path.read_text(encoding="utf-8")
            prefix = 'window.BMO_PLAYTEST_WASM_B64 = "'
            self.assertTrue(b64_text.startswith(prefix))
            encoded_payload = b64_text[len(prefix):].rstrip(";\r\n\"")
            self.assertEqual(base64.b64decode(encoded_payload), wasm_bytes)

            node_bin = shutil.which("node")
            if node_bin:
                js_test = (
                    "const fs=require('fs');"
                    "const originalReadFileSync=fs.readFileSync.bind(fs);"
                    "fs.readFileSync=(file,...args)=>{"
                    "  const normalized=String(file).replace(/\\\\/g,'/');"
                    "  return normalized.endsWith('/simulator/bmo_playtest_cores.wasm')"
                    "    ? originalReadFileSync(process.argv[1],...args)"
                    "    : originalReadFileSync(file,...args);"
                    "};"
                    "const {WasmPlaytestEngine,BMO_EXPRESSION_NAMES,BMO_EXPRESSIONS}=require('./simulator/wasm_playtest_engine.js');"
                    "(async()=>{"
                    "  if(BMO_EXPRESSION_NAMES.length!==18) throw new Error('expected 18 expressions');"
                    "  const eng=new WasmPlaytestEngine();"
                    "  await eng.init();"
                    "  let putImg=null;"
                    "  const mockCtx={createImageData:(w,h)=>({width:w,height:h,data:new Uint8ClampedArray(w*h*4)}),putImageData:(img)=>{putImg=img;}};"
                    "  eng.renderBmoFrame(mockCtx, 1000);"
                    "  const r=putImg.data[0], g=putImg.data[1], b=putImg.data[2], a=putImg.data[3];"
                    "  if(a!==255 || Math.abs(r-138)>6 || Math.abs(g-213)>6 || Math.abs(b-195)>6 || !(g>r && b>r))"
                    "    throw new Error('BGR565 unpack color mismatch rgb='+JSON.stringify([r,g,b,a]));"
                    "  for(let i=0;i<BMO_EXPRESSION_NAMES.length;i++){"
                    "    const name=BMO_EXPRESSION_NAMES[i];"
                    "    if(BMO_EXPRESSIONS[name]!==i) throw new Error('enum mismatch '+name);"
                    "    eng.setBmoExpression(name);"
                    "    if(eng.bmoExpression()!==i) throw new Error('failed to set expression '+name);"
                    "  }"
                    "  eng.tickleBmo(1.0);"
                    "  if(eng.bmoExpression()!==BMO_EXPRESSIONS.TICKLED) throw new Error('tickle did not set TICKLED');"
                    "  eng.startBmoDance(180);"
                    "  if(!eng.bmoIsDancing()) throw new Error('startBmoDance failed');"
                    "  eng.stopBmoDance();"
                    "  if(eng.bmoIsDancing()) throw new Error('stopBmoDance failed');"
                    "  const quotes=eng.bmoQuotes();"
                    "  if(quotes.length!==10 || quotes[0]!=='Who wants to play video games?!')"
                    "    throw new Error('quotes mismatch: '+JSON.stringify(quotes));"
                    "  eng.setBmoExpression('JOY');"
                    "  eng.sayBmoText('Hello BMO Mode!', 3500);"
                    "  if(!eng.bmoHasSpeech() || eng.bmoCurrentQuote()!=='Hello BMO Mode!')"
                    "    throw new Error('custom speech mismatch: '+eng.bmoCurrentQuote());"
                    "  for(let t=1000;t<=1800;t+=40) eng.renderBmoFrame(mockCtx, t);"
                    "  const px=(x,y)=>{const i=(y*320+x)*4; return [putImg.data[i],putImg.data[i+1],putImg.data[i+2]];};"
                    "  const isMint=(rgb)=>Math.abs(rgb[0]-140)<=8 && Math.abs(rgb[1]-215)<=8 && Math.abs(rgb[2]-197)<=8;"
                    "  const eyeCorner=px(147,99), mouthBL=px(116,178), mouthBR=px(203,178), mouthIn=px(160,145), bubbleIn=px(160,34);"
                    "  if(!isMint(eyeCorner) || !isMint(mouthBL) || !isMint(mouthBR))"
                    "    throw new Error('SDF corner inverted (expected mint): '+JSON.stringify({eyeCorner,mouthBL,mouthBR}));"
                    "  if(mouthIn[0]>=45 || mouthIn[1]>=45 || bubbleIn[0]<250 || bubbleIn[1]<250 || bubbleIn[2]<250)"
                    "    throw new Error('Expected dark JOY mouth & white speech bubble: '+JSON.stringify({mouthIn,bubbleIn}));"
                    "  eng.clearBmoSpeech();"
                    "  if(eng.bmoHasSpeech()) throw new Error('clearBmoSpeech failed');"
                    "  console.log('BMO_MODE_WASM_BRIDGE_PASS rgb='+r+','+g+','+b);"
                    "})().catch(err=>{console.error(err);process.exit(1);});"
                )
                proc = subprocess.run(
                    [node_bin, "-e", js_test, str(wasm_bin_path)],
                    cwd=str(REPO_ROOT),
                    capture_output=True,
                    text=True,
                    timeout=30,
                )
                self.assertEqual(proc.returncode, 0, msg=f"Node BMO Mode test failed: {proc.stderr}\n{proc.stdout}")
                self.assertIn("BMO_MODE_WASM_BRIDGE_PASS", proc.stdout)

    def test_12_3d_enclosure_meshes_and_internals_teardown_controls(self):
        """Ensure CAD WebGL meshes, 3D teardown steps, and pin wiring honor hardware rules."""
        node_bin = shutil.which("node")
        if not node_bin:
            self.skipTest("node is not installed on PATH")
        index_html = (REPO_ROOT / "simulator" / "index.html").read_text(encoding="utf-8")
        for token in (
            "bmo_enclosure_meshes.js",
            "bmo_internals_3d.js",
            "id=\"btn-internals-mode\"",
            "id=\"internals-panel\"",
            "id=\"slider-explode\"",
            "data-cam=\"bottom\"",
            "data-cam=\"core\"",
        ):
            self.assertIn(token, index_html)

        js_test = (
            "const fs=require('fs'), vm=require('vm');\n"
            "global.window = globalThis;\n"
            "vm.runInThisContext(fs.readFileSync('simulator/bmo_enclosure_meshes.js','utf8'));\n"
            "vm.runInThisContext(fs.readFileSync('simulator/bmo_internals_3d.js','utf8'));\n"
            "const em = global.BMO_ENCLOSURE_MESHES;\n"
            "if(!em || em.units!=='cm') throw new Error('missing BMO_ENCLOSURE_MESHES cm payload');\n"
            "for(const k of ['bmo_front_shell','bmo_back_shell','bmo_battery_door']){\n"
            "  const p = em.parts && em.parts[k];\n"
            "  if(!p || !p.posB64 || !p.normI16B64 || !p.idxB64) throw new Error('incomplete mesh '+k);\n"
            "  if(k!=='bmo_battery_door' && (!p.groups || p.groups.length<2)) throw new Error('missing dark face material group '+k);\n"
            "}\n"
            "const steps = global.BmoInternals3D.BUILD_GUIDE_STEPS;\n"
            "const wires = global.BmoInternals3D.WIRE_DEFINITIONS;\n"
            "if(!Array.isArray(steps) || steps.length!==7) throw new Error('expected 7 guide steps, got '+(steps&&steps.length));\n"
            "if(!Array.isArray(wires) || wires.length!==22) throw new Error('expected 22 active wires, got '+(wires&&wires.length));\n"
            "const forbidden = new Set(['GPIO 0','GPIO 3','GPIO 33','GPIO 34','GPIO 35','GPIO 36','GPIO 37','GPIO 45','GPIO 46']);\n"
            "const allowedGpio = new Set(['GPIO 4','GPIO 5','GPIO 6','GPIO 7','GPIO 8','GPIO 9','GPIO 10','GPIO 11','GPIO 12','GPIO 13','GPIO 15','GPIO 16','GPIO 17','GPIO 18','GPIO 21']);\n"
            "for(const w of wires){\n"
            "  const pFrom = w.from[1];\n"
            "  if(forbidden.has(pFrom)) throw new Error('forbidden GPIO wired: '+w.id+' -> '+pFrom);\n"
            "  if(pFrom.startsWith('GPIO ') && !allowedGpio.has(pFrom)) throw new Error('unexpected GPIO wired: '+w.id+' -> '+pFrom);\n"
            "}\n"
            "const stats = global.BmoInternals3D.prototype.getStaticStats.call({});\n"
            "if(stats.shellOuterMm!=='82 x 124 x 26' || !stats.perfboardMm.startsWith('50 x 70 x 1.6')){\n"
            "  throw new Error('stats mismatch: '+JSON.stringify(stats));\n"
            "}\n"
            "console.log('INTERNALS_3D_CONTRACT_PASS wires='+wires.length+' steps='+steps.length);\n"
        )
        proc = subprocess.run(
            [node_bin, "-e", js_test],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(proc.returncode, 0, msg=f"Node 3D internals contract failed: {proc.stderr}\n{proc.stdout}")
        self.assertIn("INTERNALS_3D_CONTRACT_PASS wires=22 steps=7", proc.stdout)


class TestCoverArtPipeline(unittest.TestCase):
    """Covers the full-resolution to 64x64 box art conversion path."""

    def test_image_to_pixels_rgb_resizes_to_target(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "master.png"
            Image.new("RGB", (512, 731), (200, 40, 90)).save(src)
            px = fetch_covers.image_to_pixels_rgb(src, 64, 64)
            self.assertEqual(len(px), 64 * 64)
            self.assertEqual(px[0], (200, 40, 90))
            big = fetch_covers.image_to_pixels_rgb(src, 128, 96)
            self.assertEqual(len(big), 128 * 96)

    def test_write_cover_files_sizes_match_boxart_loader(self):
        with tempfile.TemporaryDirectory() as tmp:
            px = [(10, 20, 30)] * (64 * 64)
            raw, bmp = fetch_covers.write_cover_files("t", Path(tmp), px)
            self.assertEqual(raw.stat().st_size, 64 * 64 * 2)
            self.assertEqual(bmp.read_bytes()[:2], b"BM")

    def test_slug_rule_matches_simulator(self):
        self.assertEqual(fetch_covers.slugify_title("Tetris (World) (Rev A).gb"), "tetris")
        self.assertEqual(fetch_covers.slugify_title("Kirby - Tilt 'n' Tumble"), "kirby_tilt_n_tumble")
        js = (REPO_ROOT / "simulator" / "simulator.js").read_text(encoding="utf-8")
        self.assertIn("slugifyTitle(title)", js)
        self.assertIn("covers/${slug}.png", js)

    def test_downloaded_covers_are_valid_images_when_present(self):
        from PIL import Image
        cover_dir = REPO_ROOT / "simulator" / "covers"
        if not cover_dir.is_dir():
            self.skipTest("covers not downloaded; run scripts/fetch_covers.py")
        for png in cover_dir.glob("*.png"):
            with Image.open(png) as im:
                im.verify()


if __name__ == "__main__":
    unittest.main()
