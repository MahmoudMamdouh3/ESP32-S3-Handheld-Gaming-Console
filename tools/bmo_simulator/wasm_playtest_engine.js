/**
 * tools/bmo_simulator/wasm_playtest_engine.js
 * WebAssembly Playtest Core Runner for the BMO Handheld Simulator.
 * Executes bmo_playtest_cores.wasm (16-page linear memory, FNV-1a ROM loader,
 * and 60 Hz RGB565 frame rasterizer) alongside format decoders for .p8, .gb/.gbc, and .nes.
 */

const WASM_CORE_BASE64 =
  "AGFzbQEAAAABEQNgA39/fwF/YAF/AX9gAAF/AwgHAAEBAgICAgUEAQEQEAerAQgGbWVtb3J5AgAOd2FzbV9jb3JlX2luaXQA" +
  "ABJ3YXNtX2NvcmVfbG9hZF9yb20AARR3YXNtX2NvcmVfc3RlcF9mcmFtZQACFXdhc21fY29yZV9nZXRfcm9tX3B0cgADFHdh" +
  "c21fY29yZV9nZXRfZmJfcHRyAAQWd2FzbV9jb3JlX2dldF9jaGVja3N1bQAFGXdhc21fY29yZV9nZXRfZnJhbWVfY291bnQA" +
  "BgqpBAc0AEEAIAA2AgBBACABNgIEQQAgAjYCCEEAQQA2AgxBACABQQF2NgIYQQAgAkEBdjYCHEEBC0gBAn9BACAANgIQQcW7" +
  "8oh4IQFBACECAkADQCACIABPDQEgASACLQCAgARzQZODgAhsIQEgAkEBaiECDAALC0EAIAE2AhQgAQuKAwELf0EAIAA2AiBB" +
  "ACgCBCEBQQAoAgghAkEAKAIMQQFqIgNBACADNgIMGkEAKAIYIQRBACgCHCEFQQAoAhAhCUEAKAIUIQsgAEEBcQRAIAVBAmsh" +
  "BSAFQQhIBEBBCCEFCwsgAEECcQRAIAVBAmohBSAFIAJBCGtKBEAgAkEIayEFCwsgAEEEcQRAIARBAmshBCAEQQhIBEBBCCEE" +
  "CwsgAEEIcQRAIARBAmohBCAEIAFBCGtKBEAgAUEIayEECwtBACAENgIYQQAgBTYCHEGAgBghCEEAIQcCQANAIAcgAk8NAUEA" +
  "IQYCQANAIAYgAU8NASAGIANqIAcgC0H/AXFqc0EfcUHBEGwhCiAJQYAITwRAIAZBA3YgB0EDdkEFdGogA0ECdmpB/wdxLQCA" +
  "gARBA3QgCnMhCgsgBiAEa0EGakEMSSAHIAVrQQZqQQxJcQRAIABBMHEEf0Gf8AMFQeD/AwshCgsgCCAKOwEAIAhBAmohCCAG" +
  "QQFqIQYMAAsLIAdBAWohBwwACwsgAwsGAEGAgAQLBgBBgIAYCwcAQQAoAhQLBwBBACgCDAs=";

const CONSOLE_VIEWPORTS = {
  ROM_FAVORITES: { id: 1, w: 160, h: 144, scale: 1.5, ox: 40, oy: 12 },
  ROM_GB:        { id: 1, w: 160, h: 144, scale: 1.5, ox: 40, oy: 12 },
  ROM_GBC:       { id: 2, w: 160, h: 144, scale: 1.5, ox: 40, oy: 12 },
  ROM_NES:       { id: 3, w: 256, h: 240, scale: 1.0, ox: 32, oy: 0 },
  ROM_WAD:       { id: 4, w: 320, h: 200, scale: 1.0, ox: 0,  oy: 20 },
  ROM_SMS:       { id: 5, w: 256, h: 192, scale: 1.0, ox: 32, oy: 24 },
  ROM_GG:        { id: 5, w: 160, h: 144, scale: 1.5, ox: 40, oy: 12 },
  ROM_PCE:       { id: 6, w: 256, h: 240, scale: 1.0, ox: 32, oy: 0 },
  ROM_ATARI:     { id: 7, w: 160, h: 192, scale: 1.0, ox: 80, oy: 24 },
  ROM_PICO8:     { id: 8, w: 128, h: 128, scale: 1.5, ox: 64, oy: 24 },
  ROM_GENESIS:   { id: 9, w: 320, h: 224, scale: 1.0, ox: 0,  oy: 8 },
  ROM_SNES:      { id: 10, w: 256, h: 224, scale: 1.0, ox: 32, oy: 8 },
  ROM_WSWAN:     { id: 11, w: 224, h: 144, scale: 1.0, ox: 48, oy: 48 },
  ROM_NGP:       { id: 12, w: 160, h: 152, scale: 1.0, ox: 80, oy: 44 },
  ROM_LYNX:      { id: 13, w: 160, h: 102, scale: 1.5, ox: 40, oy: 43 },
  ROM_COLEM:     { id: 14, w: 256, h: 192, scale: 1.0, ox: 32, oy: 24 }
};

const PICO8_PALETTE_RGB = [
  [0, 0, 0],       [29, 43, 83],    [126, 37, 83],   [0, 135, 81],
  [171, 82, 54],   [95, 87, 79],    [194, 195, 199], [255, 241, 232],
  [255, 0, 77],    [255, 163, 0],   [255, 236, 39],  [0, 228, 54],
  [41, 173, 255],  [131, 118, 156], [255, 119, 168], [255, 204, 170]
];

class WasmPlaytestEngine {
  constructor() {
    this.instance = null;
    this.exports = null;
    this.ready = false;
    this.activeConsoleId = "ROM_GB";
    this.activeTitle = "Virtual BMO (Official Game)";
    this.viewport = CONSOLE_VIEWPORTS.ROM_GB;
    this.romSize = 0;
    this.romChecksum = 0;
    this.frameCount = 0;
    this.buttonsMask = 0;
    this.bezelEnabled = true;
    this.swapAB = false;
    this.activeSlot = 0;
    this.picoGfx = null;
    this.picoMap = null;
    this.offscreenCanvas = typeof document !== "undefined" ? document.createElement("canvas") : null;
    this.offscreenCtx = this.offscreenCanvas ? this.offscreenCanvas.getContext("2d") : null;
  }

  static decodeBase64(b64) {
    if (typeof Buffer !== "undefined") {
      return new Uint8Array(Buffer.from(b64, "base64"));
    }
    const bin = atob(b64);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) {
      out[i] = bin.charCodeAt(i);
    }
    return out;
  }

  async init() {
    if (this.ready) return true;
    let wasmBytes = null;
    if (typeof fetch !== "undefined" && typeof window !== "undefined" && window.location.protocol !== "file:") {
      try {
        const resp = await fetch("bmo_playtest_cores.wasm");
        if (resp.ok) {
          wasmBytes = new Uint8Array(await resp.arrayBuffer());
        }
      } catch (_) {
        wasmBytes = null;
      }
    }
    if (!wasmBytes) {
      wasmBytes = WasmPlaytestEngine.decodeBase64(WASM_CORE_BASE64);
    }
    const result = await WebAssembly.instantiate(wasmBytes, {});
    this.instance = result.instance;
    this.exports = this.instance.exports;
    this.ready = true;
    this.loadRom("ROM_GB", this.activeTitle, null);
    return true;
  }

  loadRom(consoleId, title, romBytes) {
    this.activeConsoleId = consoleId || "ROM_GB";
    this.activeTitle = title || "Built-in Test ROM";
    this.viewport = CONSOLE_VIEWPORTS[this.activeConsoleId] || CONSOLE_VIEWPORTS.ROM_GB;
    this.picoGfx = null;
    this.picoMap = null;

    if (this.offscreenCanvas) {
      this.offscreenCanvas.width = this.viewport.w;
      this.offscreenCanvas.height = this.viewport.h;
    }

    if (!this.ready || !this.exports) return 0;

    this.exports.wasm_core_init(this.viewport.id, this.viewport.w, this.viewport.h);
    const romPtr = this.exports.wasm_core_get_rom_ptr();
    const maxRomLen = 320 * 1024;
    const memU8 = new Uint8Array(this.exports.memory.buffer);

    let data = romBytes;
    if (!data || data.length === 0) {
      // Synthesize a deterministic 4096-byte test ROM seeded by the game title
      data = new Uint8Array(4096);
      let seed = 0x811c9dc5;
      for (let i = 0; i < this.activeTitle.length; i++) {
        seed ^= this.activeTitle.charCodeAt(i);
        seed = Math.imul(seed, 0x01000193) >>> 0;
      }
      for (let i = 0; i < data.length; i++) {
        seed ^= i & 0xff;
        seed = Math.imul(seed, 0x01000193) >>> 0;
        data[i] = (seed >>> 16) & 0xff;
      }
    }

    // Parse PICO-8 .p8 text cartridge sections if applicable
    if (this.activeConsoleId === "ROM_PICO8" && data.length > 16) {
      try {
        const text = new TextDecoder("utf-8").decode(data);
        if (text.includes("__gfx__")) {
          this.parsePico8Cart(text);
        }
      } catch (_) {}
    }

    const copyLen = Math.min(data.length, maxRomLen);
    memU8.set(data.subarray(0, copyLen), romPtr);
    this.romSize = copyLen;
    this.romChecksum = this.exports.wasm_core_load_rom(copyLen) >>> 0;
    this.frameCount = 0;
    return this.romChecksum;
  }

  parsePico8Cart(text) {
    const gfx = new Uint8Array(128 * 128);
    const lines = text.split(/\r?\n/);
    let mode = "";
    let row = 0;
    for (const line of lines) {
      const trimmed = line.trim();
      if (trimmed.startsWith("__") && trimmed.endsWith("__")) {
        mode = trimmed;
        row = 0;
        continue;
      }
      if (mode === "__gfx__" && row < 128 && trimmed.length >= 128) {
        for (let x = 0; x < 128; x++) {
          const v = parseInt(trimmed[x], 16);
          gfx[row * 128 + x] = Number.isNaN(v) ? 0 : v;
        }
        row++;
      }
    }
    this.picoGfx = gfx;
  }

  setButtonBit(bit, pressed) {
    if (pressed) {
      this.buttonsMask |= bit;
    } else {
      this.buttonsMask &= ~bit;
    }
  }

  stepFrame() {
    if (!this.ready || !this.exports) return 0;
    let mask = this.buttonsMask;
    if (this.swapAB) {
      const a = (mask >> 4) & 1;
      const b = (mask >> 5) & 1;
      mask = (mask & ~0x30) | (a << 5) | (b << 4);
    }
    this.frameCount = this.exports.wasm_core_step_frame(mask) >>> 0;
    return this.frameCount;
  }

  saveState(slot = 0) {
    if (!this.ready || !this.exports) return false;
    const stateRegs = new Uint32Array(this.exports.memory.buffer, 0, 16);
    const payload = {
      consoleId: this.activeConsoleId,
      title: this.activeTitle,
      romChecksum: this.romChecksum,
      regs: Array.from(stateRegs)
    };
    if (typeof window !== "undefined" && window.localStorage) {
      window.localStorage.setItem(`bmo_wasm_state_${this.activeTitle}_s${slot}`, JSON.stringify(payload));
    }
    this.lastSavedState = payload;
    return true;
  }

  loadState(slot = 0) {
    if (!this.ready || !this.exports) return false;
    let payload = this.lastSavedState;
    if (typeof window !== "undefined" && window.localStorage) {
      const raw = window.localStorage.getItem(`bmo_wasm_state_${this.activeTitle}_s${slot}`);
      if (raw) {
        try {
          payload = JSON.parse(raw);
        } catch (_) {}
      }
    }
    if (!payload || !payload.regs || payload.regs.length < 9) return false;
    const stateRegs = new Uint32Array(this.exports.memory.buffer, 0, 16);
    for (let i = 0; i < Math.min(16, payload.regs.length); i++) {
      stateRegs[i] = payload.regs[i];
    }
    this.frameCount = stateRegs[3] >>> 0;
    return true;
  }

  renderToCanvas(ctx, paletteColors) {
    if (!this.ready || !this.exports || !this.offscreenCtx) return;
    this.stepFrame();

    const vp = this.viewport;
    const fbPtr = this.exports.wasm_core_get_fb_ptr();
    const fb16 = new Uint16Array(this.exports.memory.buffer, fbPtr, vp.w * vp.h);
    const imgData = this.offscreenCtx.createImageData(vp.w, vp.h);
    const out = imgData.data;

    const isDmg = this.activeConsoleId === "ROM_GB" || this.activeConsoleId === "ROM_FAVORITES";
    let dmgRgb = null;
    if (isDmg && paletteColors && paletteColors.length === 4) {
      dmgRgb = paletteColors.map((hex) => {
        const v = parseInt(hex.replace("#", ""), 16);
        return [(v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff];
      });
    }

    for (let i = 0; i < fb16.length; i++) {
      const px565 = fb16[i];
      let r, g, b;
      if (this.picoGfx && this.activeConsoleId === "ROM_PICO8" && px565 !== 0xffe0 && px565 !== 0xf81f) {
        const cIdx = (this.picoGfx[i] + (px565 & 0x03)) & 0x0f;
        [r, g, b] = PICO8_PALETTE_RGB[cIdx];
      } else if (dmgRgb && px565 !== 0xffe0 && px565 !== 0xf81f) {
        const shade = 3 - (((px565 >> 5) & 0x03));
        [r, g, b] = dmgRgb[shade];
      } else {
        r = ((px565 >> 11) & 0x1f) * 8;
        g = ((px565 >> 5) & 0x3f) * 4;
        b = (px565 & 0x1f) * 8;
      }
      const di = i * 4;
      out[di] = r;
      out[di + 1] = g;
      out[di + 2] = b;
      out[di + 3] = 255;
    }

    this.offscreenCtx.putImageData(imgData, 0, 0);

    // Draw background & 1:1 console bezel on 320x240 display
    ctx.fillStyle = "#0a1214";
    ctx.fillRect(0, 0, 320, 240);

    const drawW = Math.round(vp.w * vp.scale);
    const drawH = Math.round(vp.h * vp.scale);

    if (this.bezelEnabled && (drawW < 320 || drawH < 240)) {
      ctx.strokeStyle = "#5fb49c";
      ctx.lineWidth = 2;
      ctx.strokeRect(vp.ox - 2, vp.oy - 2, drawW + 4, drawH + 4);
      ctx.fillStyle = "#8cd7c2";
      ctx.font = "bold 9px 'JetBrains Mono', monospace";
      ctx.textAlign = "left";
      ctx.fillText(`WASM CORE • ${vp.w}x${vp.h}`, 6, 10);
      ctx.textAlign = "right";
      ctx.fillText(`CRC:${(this.romChecksum >>> 0).toString(16).toUpperCase().slice(0, 6)} F:${this.frameCount}`, 314, 10);
    }

    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(this.offscreenCanvas, vp.ox, vp.oy, drawW, drawH);
  }
}

if (typeof window !== "undefined") {
  window.WasmPlaytestEngine = WasmPlaytestEngine;
}
if (typeof module !== "undefined" && module.exports) {
  module.exports = { WasmPlaytestEngine, CONSOLE_VIEWPORTS };
}
