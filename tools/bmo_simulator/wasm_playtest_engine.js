/**
 * Browser adapter for firmware emulator cores compiled to WebAssembly.
 */

const GAMEBOY_VIEWPORT = { w: 240, h: 216, scale: 1, ox: 40, oy: 12 };
const NES_VIEWPORT = { w: 256, h: 240, scale: 1, ox: 32, oy: 0 };
const ATARI_VIEWPORT = { w: 160, h: 192, scale: 1, ox: 80, oy: 24 };
const COLECO_VIEWPORT = { w: 256, h: 192, scale: 1, ox: 32, oy: 24 };
const BMO_EXPRESSION_NAMES = [
  "IDLE",
  "SURPRISED",
  "HAPPY",
  "SLEEPY",
  "LOW_BATTERY",
  "CHARGING",
  "ERROR",
  "SHUTDOWN",
  "HIDDEN",
  "JOY",
  "SLEEPING",
  "WINK",
  "BLUSH",
  "CONFUSED",
  "ANNOYED",
  "LOVE",
  "TICKLED",
  "CELEBRATING",
];
const BMO_EXPRESSIONS = Object.freeze(
  Object.fromEntries(BMO_EXPRESSION_NAMES.map((name, index) => [name, index]))
);
const CORE_CONFIG = {
  ROM_GB: { prefix: "bmo_emu", name: "Peanut-GB", viewport: GAMEBOY_VIEWPORT, frameRate: 59.7275 },
  ROM_NES: { prefix: "bmo_nes", name: "Agnes NES", viewport: NES_VIEWPORT, frameRate: 60.0988 },
  ROM_ATARI: { prefix: "bmo_atari", name: "Stella Atari 2600", viewport: ATARI_VIEWPORT, frameRate: 60 },
  ROM_COLEM: { prefix: "bmo_colem", name: "ColecoVision", viewport: COLECO_VIEWPORT, frameRate: 60 },
};

class WasmPlaytestEngine {
  constructor() {
    this.instance = null;
    this.exports = null;
    this.ready = false;
    this.bmoReady = false;
    this.loaded = false;
    this.activeConsoleId = "ROM_GB";
    this.activeTitle = "";
    this.viewport = GAMEBOY_VIEWPORT;
    this.corePrefix = CORE_CONFIG.ROM_GB.prefix;
    this.coreName = CORE_CONFIG.ROM_GB.name;
    this.frameIntervalMs = 1000 / CORE_CONFIG.ROM_GB.frameRate;
    this.romSize = 0;
    this.romChecksum = 0;
    this.runtimeError = "";
    this.frameCount = 0;
    this.buttonsMask = 0;
    this.bezelEnabled = true;
    this.swapAB = false;
    this.activeSlot = 0;
    this.paletteColors = null;
    this.lastRenderTime = 0;
    this.frameAccumulator = 0;
    this.offscreenCanvas = typeof document !== "undefined" ? document.createElement("canvas") : null;
    this.offscreenCtx = this.offscreenCanvas ? this.offscreenCanvas.getContext("2d") : null;
  }

  async init() {
    if (this.ready) return true;
    let wasmBytes;
    if (typeof window === "undefined" && typeof require === "function" && typeof __dirname !== "undefined") {
      const fs = require("fs");
      const path = require("path");
      wasmBytes = fs.readFileSync(path.join(__dirname, "bmo_playtest_cores.wasm"));
    } else {
      if (typeof fetch !== "function" || window.location.protocol === "file:") {
        throw new Error("Open the simulator through run_simulator.py so the WebAssembly core can load.");
      }
      const response = await fetch("bmo_playtest_cores.wasm");
      if (!response.ok) {
        throw new Error(`Could not load the WebAssembly emulator cores (HTTP ${response.status}).`);
      }
      wasmBytes = await response.arrayBuffer();
    }
    const wasi = {
      random_get: (pointer, length) => {
        const memory = this.exports && this.exports.memory;
        if (!memory || !globalThis.crypto || !globalThis.crypto.getRandomValues) {
          throw new Error("This browser cannot provide the WebAssembly runtime's secure random source.");
        }
        const target = new Uint8Array(memory.buffer, pointer, length);
        for (let offset = 0; offset < target.length; offset += 65536) {
          globalThis.crypto.getRandomValues(target.subarray(offset, Math.min(offset + 65536, target.length)));
        }
        return 0;
      }
    };
    const { instance } = await WebAssembly.instantiate(wasmBytes, { wasi_snapshot_preview1: wasi });
    this.instance = instance;
    this.exports = instance.exports;
    this.ready = true;
    const bmoInitResult = this.exports.bmo_face_init(this.bmoTimeUs(performance.now()));
    if (bmoInitResult !== 0) {
      this.ready = false;
      throw new Error(`The firmware BMO companion failed to initialize (${bmoInitResult}).`);
    }
    this.bmoReady = true;
    return true;
  }

  bmoTimeUs(nowMs) {
    return Math.floor((nowMs % 4294967.296) * 1000) >>> 0;
  }

  updateBmoClock() {
    this.exports.bmo_face_set_clock(this.bmoTimeUs(performance.now()));
  }

  renderBmoFrame(ctx, now = performance.now()) {
    if (!this.bmoReady || !this.exports) throw new Error("The firmware BMO companion is not ready.");
    const fbPtr = this.exports.bmo_face_step(this.bmoTimeUs(now));
    if (!fbPtr) throw new Error("The firmware BMO companion did not return a framebuffer.");
    const pixels = new Uint16Array(this.exports.memory.buffer, fbPtr, 320 * 240);
    const image = ctx.createImageData(320, 240);
    const rgba = image.data;
    // BmoFace::packBGR565 stores byte-swapped BGR565 for ST7789 SPI order.
    for (let i = 0; i < pixels.length; i++) {
      const pixel = pixels[i];
      const bgr = ((pixel & 0xFF) << 8) | ((pixel >> 8) & 0xFF);
      const index = i * 4;
      rgba[index] = (bgr & 0x1F) * 255 / 31;
      rgba[index + 1] = ((bgr >> 5) & 0x3F) * 255 / 63;
      rgba[index + 2] = ((bgr >> 11) & 0x1F) * 255 / 31;
      rgba[index + 3] = 255;
    }
    ctx.putImageData(image, 0, 0);
  }

  readCString(pointer, maxLen = 128) {
    if (!pointer || !this.exports) return "";
    const memory = new Uint8Array(this.exports.memory.buffer);
    let end = pointer;
    const limit = Math.min(memory.length, pointer + maxLen);
    while (end < limit && memory[end] !== 0) end++;
    return new TextDecoder().decode(memory.subarray(pointer, end));
  }

  setBmoExpression(expression) {
    let value;
    if (typeof expression === "number" && Number.isInteger(expression)) {
      value = expression >= 0 && expression < BMO_EXPRESSION_NAMES.length ? expression : undefined;
    } else if (typeof expression === "string") {
      value = BMO_EXPRESSIONS[expression.trim().toUpperCase()];
    }
    if (value === undefined) throw new Error(`Unsupported firmware BMO expression: ${expression}`);
    this.updateBmoClock();
    this.exports.bmo_face_set_expression(value);
  }

  setBmoGaze(x, y) {
    this.updateBmoClock();
    this.exports.bmo_face_set_gaze(x, y);
  }

  triggerBmoWink() {
    this.updateBmoClock();
    this.exports.bmo_face_trigger_wink();
  }

  triggerBmoBlush() {
    this.updateBmoClock();
    this.exports.bmo_face_trigger_blush();
  }

  triggerBmoJoy() {
    this.updateBmoClock();
    this.exports.bmo_face_trigger_joy();
  }

  tickleBmo(intensity = 1.0) {
    this.updateBmoClock();
    this.exports.bmo_face_tickle(intensity);
  }

  petBmoCompanion() {
    this.updateBmoClock();
    this.exports.bmo_face_pet_companion();
  }

  sayBmoQuote(index, durationMs = 3500) {
    this.updateBmoClock();
    const result = this.exports.bmo_face_say_quote(index, durationMs);
    if (result !== 0) throw new Error(`The firmware BMO companion rejected quote ${index}.`);
  }

  sayBmoText(text, durationMs = 3500) {
    const clean = String(text ?? "").trim();
    if (!clean) {
      this.clearBmoSpeech();
      return;
    }
    const bufPtr = this.exports.bmo_face_speech_buffer();
    const capacity = this.exports.bmo_face_speech_capacity();
    if (!bufPtr || capacity < 2) {
      throw new Error("The firmware BMO speech buffer is unavailable.");
    }
    const encoded = new TextEncoder().encode(clean);
    const length = Math.min(encoded.length, capacity - 1);
    const memory = new Uint8Array(this.exports.memory.buffer);
    memory.set(encoded.subarray(0, length), bufPtr);
    memory[bufPtr + length] = 0;
    this.updateBmoClock();
    const result = this.exports.bmo_face_say_buffer(length, durationMs);
    if (result !== 0) {
      throw new Error("The firmware BMO companion rejected custom speech text.");
    }
  }

  bmoQuotes() {
    if (!this.exports || !this.exports.bmo_face_quote_count) return [];
    const count = this.exports.bmo_face_quote_count();
    const list = [];
    for (let i = 0; i < count; i++) {
      list.push(this.readCString(this.exports.bmo_face_quote_text(i), 128));
    }
    return list;
  }

  clearBmoSpeech() {
    this.updateBmoClock();
    this.exports.bmo_face_clear_speech();
  }

  startBmoDance(bpm = 125) {
    this.updateBmoClock();
    this.exports.bmo_face_start_dance(bpm);
  }

  stopBmoDance() {
    this.updateBmoClock();
    this.exports.bmo_face_stop_dance();
  }

  toggleBmoDance() {
    this.updateBmoClock();
    this.exports.bmo_face_toggle_dance();
  }

  bmoIsDancing() {
    return this.exports.bmo_face_is_dancing() !== 0;
  }

  bmoHasSpeech() {
    return this.exports.bmo_face_has_speech() !== 0;
  }

  bmoVisibleChars() {
    return this.exports.bmo_face_visible_chars();
  }

  bmoCurrentQuote() {
    return this.readCString(this.exports.bmo_face_current_quote(), 64);
  }

  bmoExpression() {
    return this.exports.bmo_face_get_expression();
  }

  bmoExpressionName(expressionId = this.bmoExpression()) {
    return BMO_EXPRESSION_NAMES[expressionId] || "UNKNOWN";
  }

  bmoBuddyLevel() {
    return this.exports.bmo_face_buddy_level();
  }

  bmoHappiness() {
    return this.exports.bmo_face_happiness();
  }

  loadRom(consoleId, title, romBytes) {
    if (!this.ready || !this.exports) throw new Error("The WebAssembly emulator cores are not ready.");
    const config = CORE_CONFIG[consoleId];
    if (!config) throw new Error("Browser gameplay currently supports Game Boy (.gb), NES (.nes), Atari 2600 (.a26), and ColecoVision (.col) ROMs only.");
    if (!(romBytes instanceof Uint8Array) || romBytes.length === 0) {
      throw new Error("Choose a real .gb, .nes, .a26, or .col ROM file before starting a game.");
    }

    this.exports[`${this.corePrefix}_destroy`]();
    this.loaded = false;
    this.runtimeError = "";
    this.romSize = 0;
    this.romChecksum = 0;
    this.frameCount = 0;
    const capacity = this.exports.bmo_emu_rom_capacity();
    if (romBytes.length > capacity) {
      throw new Error(`This ROM is larger than the browser core limit (${capacity} bytes).`);
    }
    const romPointer = this.exports.bmo_emu_rom_buffer();
    new Uint8Array(this.exports.memory.buffer, romPointer, romBytes.length).set(romBytes);
    const result = this.exports[`${config.prefix}_load_rom`](romBytes.length);
    if (result !== 0) {
      const failures = consoleId === "ROM_GB"
        ? {
          "-1": "The file is too small or too large to be a Game Boy ROM.",
          "-2": "The ROM header declares an unsupported or truncated cartridge size.",
          "-3": "The Game Boy cartridge header checksum is invalid.",
          "-4": "This Game Boy cartridge mapper is not supported by the vendored firmware core."
        }
        : consoleId === "ROM_NES"
          ? {
          "-1": "The file is too small or too large to be an NES ROM.",
          "-2": "The NES ROM header is invalid or declares no PRG-ROM.",
          "-3": "NES 2.0 headers are not supported by the vendored firmware core.",
          "-4": "The NES ROM header declares a truncated cartridge image.",
          "-5": "This NES mapper is not supported by the vendored firmware core (mappers 0, 1, 2, and 4 only).",
          "-6": "The vendored NES core could not initialize this ROM or allocate its state."
          }
          : consoleId === "ROM_ATARI"
            ? {
            "-1": "Atari 2600 ROMs must be 2, 4, 8, 16, or 32 KiB.",
            "-2": "The Atari 2600 core could not initialize this ROM or allocate its state."
            }
            : {
              "-1": "ColecoVision ROM files must be at least 512 bytes.",
              "-2": "The ColecoVision core could not allocate its working or save-state memory."
            };
      throw new Error(failures[String(result)] || `${config.name} rejected this ROM (${result}).`);
    }

    this.activeConsoleId = consoleId;
    this.activeTitle = title || `${config.name} ROM`;
    this.corePrefix = config.prefix;
    this.coreName = config.name;
    this.viewport = config.viewport;
    this.frameIntervalMs = 1000 / config.frameRate;
    this.romSize = romBytes.length;
    this.romChecksum = this.exports[`${this.corePrefix}_rom_checksum`]() >>> 0;
    this.frameCount = 0;
    this.buttonsMask = 0;
    this.loaded = true;
    this.lastRenderTime = 0;
    this.frameAccumulator = 0;
    if (this.paletteColors && this.corePrefix === "bmo_emu") this.setPalette(this.paletteColors);
    if (this.offscreenCanvas) {
      this.offscreenCanvas.width = this.viewport.w;
      this.offscreenCanvas.height = this.viewport.h;
    }
    return this.romChecksum;
  }

  setPalette(colors) {
    if (!Array.isArray(colors) || colors.length !== 4) return;
    this.paletteColors = colors.slice();
    if (!this.exports) return;
    const packed = colors.map((hex) => Number.parseInt(hex.replace("#", ""), 16));
    if (this.corePrefix === "bmo_emu") this.exports.bmo_emu_set_palette(...packed);
  }

  setButtonBit(bit, pressed) {
    if (pressed) this.buttonsMask |= bit;
    else this.buttonsMask &= ~bit;
  }

  _mappedButtons() {
    let mask = this.buttonsMask;
    if (this.swapAB) {
      const a = (mask >> 4) & 1;
      const b = (mask >> 5) & 1;
      mask = (mask & ~0x30) | (a << 5) | (b << 4);
    }
    return mask >>> 0;
  }

  stepFrame() {
    if (!this.ready || !this.exports || !this.loaded) return this.frameCount;
    this.exports[`${this.corePrefix}_set_buttons`](this._mappedButtons());
    this.frameCount = this.exports[`${this.corePrefix}_run_frame`]() >>> 0;
    const error = this.exports[`${this.corePrefix}_last_error`]();
    if (error !== 0) {
      this.runtimeError = `${this.coreName} runtime error ${error}.`;
      this.loaded = false;
    }
    return this.frameCount;
  }

  saveState() {
    if (!this.loaded || !this.exports) return false;
    this.exports[`${this.corePrefix}_save_state`]();
    return true;
  }

  loadState() {
    if (!this.loaded || !this.exports) return false;
    const restored = this.exports[`${this.corePrefix}_load_state`]() !== 0;
    if (restored) this.frameCount = this.exports[`${this.corePrefix}_frame_count`]() >>> 0;
    return restored;
  }

  renderToCanvas(ctx, paletteColors, paused = false, now = performance.now()) {
    if (!this.ready || !this.exports || !this.offscreenCtx) return;
    this.setPalette(paletteColors);

    if (this.loaded && !paused) {
      if (this.lastRenderTime === 0) this.lastRenderTime = now;
      this.frameAccumulator += Math.min(now - this.lastRenderTime, 100);
      this.lastRenderTime = now;
      while (this.frameAccumulator >= this.frameIntervalMs) {
        this.stepFrame();
        this.frameAccumulator -= this.frameIntervalMs;
      }
    } else {
      this.lastRenderTime = now;
      this.frameAccumulator = 0;
    }

    const fbPtr = this.exports[`${this.corePrefix}_framebuffer`]();
    const fb16 = new Uint16Array(this.exports.memory.buffer, fbPtr, this.viewport.w * this.viewport.h);
    const imgData = this.offscreenCtx.createImageData(this.viewport.w, this.viewport.h);
    const out = imgData.data;
    for (let i = 0; i < fb16.length; i++) {
      const pixel = fb16[i];
      const index = i * 4;
      out[index] = ((pixel >> 11) & 0x1F) * 255 / 31;
      out[index + 1] = ((pixel >> 5) & 0x3F) * 255 / 63;
      out[index + 2] = (pixel & 0x1F) * 255 / 31;
      out[index + 3] = 255;
    }
    this.offscreenCtx.putImageData(imgData, 0, 0);

    ctx.fillStyle = "#0a1214";
    ctx.fillRect(0, 0, 320, 240);
    if (this.bezelEnabled) {
      ctx.strokeStyle = "#5fb49c";
      ctx.lineWidth = 2;
      ctx.strokeRect(this.viewport.ox - 2, this.viewport.oy - 2, this.viewport.w + 4, this.viewport.h + 4);
      ctx.fillStyle = "#8cd7c2";
      ctx.font = "bold 9px 'JetBrains Mono', monospace";
      ctx.textAlign = "left";
      ctx.fillText(`${this.coreName.toUpperCase()} • ${this.viewport.w}x${this.viewport.h}`, 6, 10);
      ctx.textAlign = "right";
      ctx.fillText(`F:${this.frameCount}`, 314, 10);
    }
    ctx.imageSmoothingEnabled = false;
    if (this.loaded || this.frameCount > 0) {
      ctx.drawImage(this.offscreenCanvas, this.viewport.ox, this.viewport.oy, this.viewport.w, this.viewport.h);
    }
    if (!this.loaded) {
      if (this.frameCount > 0) {
        ctx.fillStyle = "rgba(10, 18, 20, 0.78)";
        ctx.fillRect(this.viewport.ox, this.viewport.oy, this.viewport.w, this.viewport.h);
      }
      ctx.fillStyle = "#cef5e4";
      ctx.font = "bold 12px 'JetBrains Mono', monospace";
      ctx.textAlign = "center";
      ctx.fillText(this.runtimeError || "LOAD A SUPPORTED ROM TO PLAY", 160, 120);
    }
  }
}

if (typeof window !== "undefined") {
  window.WasmPlaytestEngine = WasmPlaytestEngine;
  window.BMO_EXPRESSIONS = BMO_EXPRESSIONS;
  window.BMO_EXPRESSION_NAMES = BMO_EXPRESSION_NAMES;
}
if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    WasmPlaytestEngine,
    BMO_EXPRESSIONS,
    BMO_EXPRESSION_NAMES,
    GAMEBOY_VIEWPORT,
    NES_VIEWPORT,
    ATARI_VIEWPORT,
    COLECO_VIEWPORT,
  };
}
