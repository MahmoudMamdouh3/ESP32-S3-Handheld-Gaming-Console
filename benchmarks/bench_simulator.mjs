#!/usr/bin/env node
// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
/**
 * Headless Node.js benchmark for simulator WASM engine, JS GC allocation churn,
 * palette parsing overhead, and upfront synchronous startup script payload bytes.
 * Run with `node --expose-gc benchmarks/bench_simulator.mjs` for GC heap precision.
 */

import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";
import { performance } from "node:perf_hooks";
import { fileURLToPath, pathToFileURL } from "node:url";

const require = createRequire(import.meta.url);
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const REPO_ROOT = path.resolve(__dirname, "..");
const SIM_DIR = path.join(REPO_ROOT, "simulator");

function createMockCanvasContext() {
  const stats = {
    createImageDataCalls: 0,
    imageDataBytesAllocated: 0,
    putImageDataCalls: 0,
    drawImageCalls: 0,
    fillRectCalls: 0,
  };
  return {
    imageSmoothingEnabled: true,
    fillStyle: "#000000",
    strokeStyle: "#000000",
    lineWidth: 1,
    font: "10px monospace",
    textAlign: "left",
    createImageData(w, h) {
      stats.createImageDataCalls += 1;
      const byteLen = w * h * 4;
      stats.imageDataBytesAllocated += byteLen;
      return { width: w, height: h, data: new Uint8ClampedArray(byteLen) };
    },
    putImageData() {
      stats.putImageDataCalls += 1;
    },
    drawImage() {
      stats.drawImageCalls += 1;
    },
    fillRect() {
      stats.fillRectCalls += 1;
    },
    strokeRect() {},
    fillText() {},
    measureText(s) {
      return { width: String(s).length * 6 };
    },
    resetStats() {
      stats.createImageDataCalls = 0;
      stats.imageDataBytesAllocated = 0;
      stats.putImageDataCalls = 0;
      stats.drawImageCalls = 0;
      stats.fillRectCalls = 0;
    },
    stats,
  };
}

function makeSyntheticGbRom() {
  const rom = new Uint8Array(32768);
  // 0x100: NOP; JP 0x0150
  rom[0x100] = 0x00;
  rom[0x101] = 0xc3;
  rom[0x102] = 0x50;
  rom[0x103] = 0x01;
  // 0x150: JR -2
  rom[0x150] = 0x18;
  rom[0x151] = 0xfe;
  const title = "BMO_BENCH";
  for (let i = 0; i < title.length; i++) {
    rom[0x134 + i] = title.charCodeAt(i);
  }
  let sum = 0;
  for (let a = 0x134; a <= 0x14c; a++) {
    sum = (sum - rom[a] - 1) & 0xff;
  }
  rom[0x14d] = sum;
  return rom;
}

function measureSyncIndexBundleBytes() {
  const indexPath = path.join(SIM_DIR, "index.html");
  const html = fs.readFileSync(indexPath, "utf8");
  const scriptRe = /<script\b[^>]*\bsrc=["']([^"']+)["'][^>]*>/gi;
  let totalBytes = 0;
  const syncScripts = [];
  let match;
  while ((match = scriptRe.exec(html)) !== null) {
    const tag = match[0];
    const src = match[1];
    if (/\b(?:defer|async)\b/i.test(tag)) continue;
    const full = path.join(SIM_DIR, src);
    const sz = fs.existsSync(full) ? fs.statSync(full).size : 0;
    totalBytes += sz;
    syncScripts.push({ src, bytes: sz });
  }
  return { totalBytes, syncScripts };
}

async function main() {
  const wasmEnginePath = path.join(SIM_DIR, "wasm_playtest_engine.js");
  const modSource = fs.readFileSync(wasmEnginePath, "utf8");
  let engineMod;
  if (/^\s*(?:export|import)\b/m.test(modSource)) {
    engineMod = await import(pathToFileURL(wasmEnginePath).href);
  } else {
    engineMod = require(wasmEnginePath);
  }
  const { WasmPlaytestEngine } = engineMod;

  const engine = new WasmPlaytestEngine();
  const mainCtx = createMockCanvasContext();
  const offCtx = createMockCanvasContext();
  engine.offscreenCanvas = { width: 320, height: 240 };
  engine.offscreenCtx = offCtx;

  const tInit0 = performance.now();
  await engine.init();
  const wasmInitMs = performance.now() - tInit0;

  // 1. BMO Face 600 frames
  engine.setBmoExpression("IDLE");
  for (let i = 0; i < 10; i++) {
    engine.renderBmoFrame(mainCtx, 1000 + i * 16.667);
  }
  mainCtx.resetStats();
  if (typeof globalThis.gc === "function") globalThis.gc();
  const heapFaceBefore = process.memoryUsage().heapUsed;
  const tFace0 = performance.now();
  for (let i = 0; i < 600; i++) {
    engine.renderBmoFrame(mainCtx, 2000 + i * 16.667);
  }
  const bmoFace600Ms = performance.now() - tFace0;
  const heapFaceDelta = Math.max(0, process.memoryUsage().heapUsed - heapFaceBefore);
  const bmoFaceAllocBytes = mainCtx.stats.imageDataBytesAllocated;
  const bmoFaceImageDataCalls = mainCtx.stats.createImageDataCalls;

  // 2. Game Boy emulator 600 frames via renderToCanvas
  const gbRom = makeSyntheticGbRom();
  engine.loadRom("ROM_GB", "bench.gb", gbRom);
  const palette = ["#CEF5E4", "#5FB49C", "#1A4B42", "#0F2620"];
  for (let i = 0; i < 10; i++) {
    engine.renderToCanvas(mainCtx, palette, false, 1000 + i * 16.75);
  }
  mainCtx.resetStats();
  offCtx.resetStats();
  if (typeof globalThis.gc === "function") globalThis.gc();
  const heapGbBefore = process.memoryUsage().heapUsed;
  const tGb0 = performance.now();
  for (let i = 0; i < 600; i++) {
    engine.renderToCanvas(mainCtx, palette, false, 2000 + i * 16.75);
  }
  const gbRender600Ms = performance.now() - tGb0;
  const heapGbDelta = Math.max(0, process.memoryUsage().heapUsed - heapGbBefore);
  const gbAllocBytes = offCtx.stats.imageDataBytesAllocated + mainCtx.stats.imageDataBytesAllocated;
  const gbImageDataCalls = offCtx.stats.createImageDataCalls + mainCtx.stats.createImageDataCalls;

  const bundleInfo = measureSyncIndexBundleBytes();

  const out = {
    wasm_init_ms: Number(wasmInitMs.toFixed(2)),
    bmo_face_600_frames_ms: Number(bmoFace600Ms.toFixed(2)),
    bmo_face_600_frames_imagedata_bytes: bmoFaceAllocBytes,
    bmo_face_600_frames_imagedata_calls: bmoFaceImageDataCalls,
    bmo_face_heap_delta_bytes: heapFaceDelta,
    gb_render_600_frames_ms: Number(gbRender600Ms.toFixed(2)),
    gb_render_600_frames_imagedata_bytes: gbAllocBytes,
    gb_render_600_frames_imagedata_calls: gbImageDataCalls,
    gb_heap_delta_bytes: heapGbDelta,
    sync_startup_script_bytes: bundleInfo.totalBytes,
    sync_startup_script_count: bundleInfo.syncScripts.length,
  };
  process.stdout.write(JSON.stringify(out, null, 2) + "\n");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
