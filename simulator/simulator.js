/**
 * simulator.js
 * Browser simulator coordinator: state lifecycle, WASM core initialization, and screen routing.
 */
import { Bmo3DConsole } from "./bmo_3d_console.js";
import { bmoModeMethods } from "./js/bmo_mode.js";
import { CONSOLES_LIST, MAX_BROWSER_ROM_SIZE, STATE, slugifyTitle } from "./js/constants.js";
import { DEFAULT_HARDWARE_LAYOUT, loadHardwareLayout } from "./js/hardware_layout_data.js";
import { seedBuiltInHomebrewDemos } from "./js/homebrew_demos.js";
import { internalsMethods } from "./js/internals.js";
import { decodeRawBgr565ToBmpBlob, fetchServerLocalLibrary } from "./js/local_library_client.js";
import { museumMethods } from "./js/museum.js";
import { applyUrlStateParamsImpl, handleButtonPressImpl, setupInputHandlersImpl, setupUIControlsImpl } from "./js/sim_controls.js";
import { renderBootSplashImpl, renderConsoleSelectMenuImpl, renderEmulationViewImpl, renderGameSelectMenuImpl, renderIdleMascotScreenImpl } from "./js/sim_screens.js";
import { webserialMethods } from "./js/webserial.js";
import { WasmPlaytestEngine } from "./wasm_playtest_engine.js";

export { MAX_BROWSER_ROM_SIZE, STATE };

const EXT_KEY_BIT_MAP = {
  UP: 1, DOWN: 2, LEFT: 4, RIGHT: 8, A: 16, B: 32, START: 64, SELECT: 128, X: 256, Y: 512, L: 1024, R: 2048,
};

export class BmoSimulator {
  constructor() {
    this.canvas = /** @type {HTMLCanvasElement} */ (document.getElementById("tft-display"));
    this.ctx = this.canvas ? this.canvas.getContext("2d", { alpha: false }) : null;
    if (this.canvas) {
      this.canvas.width = 320;
      this.canvas.height = 240;
    }
    this.hardwareLayout = DEFAULT_HARDWARE_LAYOUT;
    /** @type {number | null} */
    this._rafId = null;
    /** @type {FrameRequestCallback} */
    this._boundRenderLoop = (ts) => this.renderLoop(ts);
    /** @type {Array<() => void>} */
    this._cleanupFns = [];
    /** @type {string | null} */
    this._lastRenderSig = null;
    this._screenTextureDirty = true;
    /** @type {Map<string, any>} */
    this.coverImages = new Map();
    /** @type {Record<string, any>} */
    this.gameLibrary = this.createGameLibrary();
    /** @type {WasmPlaytestEngine | null} */
    this.wasmEngine = null;
    /** @type {any} */
    this.bmo = { expression: "IDLE", blinkState: 0.0, blinkTimer: 0, eyeLookX: 0, eyeLookY: 0, mouthCurve: 0.8, isBlinking: false };
    this.bmoLastDirectionChangeMs = 0;
    this.bmoDirectionToggleCount = 0;
    this.bmoQuoteIndex = 0;
    this.bmoButtonHoldStart = 0;
    this.bmoLastGazeX = 0;
    this.bmoLastInputMs = 0;
    this.lastInputActivityMs = 0;
    this.bootTimer = 0;
    this.topPicksFilter = false;
    this.librarySearchQuery = "";
    /** @type {string | number} */
    this.currentState = STATE.BOOT_SPLASH;
    this.engineReady = Promise.resolve(false);
    this.engineLoadFailed = false;
    this.engineLoadError = "";

    this.initSimulationState();
    this.initEngineAndControls();
    this.initThreeView();
    this._rafId = requestAnimationFrame(this._boundRenderLoop);
  }

  initSimulationState() {
    this.currentState = STATE.BOOT_SPLASH;
    this.selectedConsoleIndex = 0;
    this.selectedGameIndex = 0;
    this.gameListScrollOffset = 0;
    this.topPicksFilter = false;
    this.librarySearchQuery = "";
    this.coverImages = new Map();
    this._lastRenderSig = null;
    this._screenTextureDirty = true;
    this.lastHdCoverSlug = "";
    this.bootTimer = 0;
    this.lastFrameTime = performance.now();
    this.fps = 60;
    this._activeMouseButton = null;

    this.bmo = { expression: "IDLE", blinkState: 0.0, blinkTimer: 0, eyeLookX: 0, eyeLookY: 0, mouthCurve: 0.8, isBlinking: false };
    this.bmoSessionStarted = false;
    this.bmoEntryGreeting = true;
    this.bmoQuoteIndex = 0;
    this.bmoLastInputMs = this.lastFrameTime;
    this.lastInputActivityMs = this.bmoLastInputMs;
    this.bmoButtonHoldStart = 0;
    this.bmoLastGazeX = 0;
    this.bmoLastDirectionChangeMs = 0;
    this.bmoDirectionToggleCount = 0;
    this.bmoModeActive = false;
    this.bmoAutoSleep = true;
    this.bmoHudVisible = true;
    this.bmoAutoCycle = false;
    this.bmoAutoCycleIndex = 0;
    this.bmoLastAutoCycleMs = 0;
    this.bmoDanceBpm = 125;
    this.bmoGazeX = 0;
    this.bmoGazeY = 0;
    this.bmoLastUiSyncMs = 0;

    this.internalsModeActive = false;
    this.internalsPreset = "assembled";
    this.internalsHighlight = "all";
    this.internalsStepIndex = 0;
    this.pendingInternalsPreset = null;

    this.palettes = {
      classic: ["#9bbc0f", "#8bac0f", "#306230", "#0f380f"],
      bmo: ["#dcf8f2", "#53c6b5", "#164848", "#051214"],
      pocket: ["#f5f5f5", "#aaaaaa", "#555555", "#0a0a0a"],
      light: ["#b4fff0", "#50c8d2", "#146478", "#021e28"],
      amber: ["#ffd250", "#dc9614", "#784600", "#140a00"],
    };
    this.activePalette = "classic";
    this.gameLibrary = this.createGameLibrary();
    this.isPausedInGame = false;
    this.pauseMenuIndex = 0;
  }

  initEngineAndControls() {
    this.wasmEngine = new WasmPlaytestEngine();
    this.engineLoadFailed = false;
    this.engineLoadError = "";
    this.engineReady = this.wasmEngine.init().then(() => {
      this.engineLoadFailed = false;
      this.engineLoadError = "";
      this.populateBmoQuotes();
      this.syncBmoModeUI();
      return true;
    }).catch((error) => {
      this.engineLoadFailed = true;
      this.engineLoadError = (error && error.message) ? error.message : "Failed to initialize WebAssembly BMO core.";
      this.showSimulatorMessage(this.engineLoadError, true);
      this.syncBmoModeUI();
      return false;
    });

    this.setupInputHandlers();
    this.setupUIControls();
    this.applyUrlStateParams();
    loadHardwareLayout().then((layout) => {
      this.hardwareLayout = layout;
      this.syncInternalsLayoutSpec(layout);
    });
    this.initLocalLibraryAutoDetect();
  }

  initLocalLibraryAutoDetect() {
    if (typeof window === "undefined" || !window.location) return;
    if (window.location.protocol === "file:") {
      this.updateLibraryStatsBanner();
      const serialLog = document.getElementById("webserial-log");
      if (serialLog) {
        serialLog.textContent = "Direct file:// mode: browsers block auto-reading disk folders without permission. Click '+ Load Folder' to mount F:\\ or games/ in one step, or run 'python simulator/run_simulator.py' for automatic detection.";
      }
      return;
    }
    fetchServerLocalLibrary(this).catch(() => {});
  }

  initThreeView() {
    this.bmo3D = null;
    this.is3DView = true;
    const threeContainer = typeof /** @type {any} */ (globalThis).THREE !== "undefined"
      ? document.getElementById("three-viewport-container")
      : null;
    if (!threeContainer) {
      this.switchTo2DView();
      return;
    }
    try {
      this.bmo3D = new Bmo3DConsole(
        threeContainer,
        this.canvas,
        (key) => this.handleExternalButtonPress(key),
        (key) => this.handleExternalButtonRelease(key),
        this.hardwareLayout
      );
      this.bmo3D.setOnInternalSelect((meta) => this.handleInternalPartSelected(meta));
      this.bmo3D.onExplosionChange = (t) => this.syncExplosionReadout(t);
      this.bmo3D.onScreenClick = () => {
        if (this.currentState === STATE.BOOT_SPLASH) {
          this.handleButtonPress("A");
          this.markRenderDirty();
        }
      };
      this.bmo3D.shouldUpdateScreenTexture = () => {
        if (!this._screenTextureDirty) return false;
        this._screenTextureDirty = false;
        return true;
      };
      if (this.pendingInternalsPreset) this.applyInternalsPresetUI(this.pendingInternalsPreset);
    } catch (err) {
      console.warn("Failed to initialize 3D console view:", err);
      this.switchTo2DView();
    }
  }

  trackListener(target, type, handler, options) {
    if (!target || typeof target.addEventListener !== "function") return;
    target.addEventListener(type, handler, options);
    this._cleanupFns.push(() => target.removeEventListener(type, handler, options));
  }

  markRenderDirty() {
    this._lastRenderSig = null;
    this._screenTextureDirty = true;
  }

  dispose() {
    if (this._rafId !== null) {
      cancelAnimationFrame(this._rafId);
      this._rafId = null;
    }
    while (this._cleanupFns.length > 0) {
      const cleanup = this._cleanupFns.pop();
      if (cleanup) cleanup();
    }
    if (this.coverImages instanceof Map) {
      for (const entry of this.coverImages.values()) {
        if (entry) entry.disposed = true;
        if (entry && entry.img) {
          entry.img.onload = null;
          entry.img.onerror = null;
        }
        if (entry && entry.objectUrl && typeof URL !== "undefined" && typeof URL.revokeObjectURL === "function") {
          URL.revokeObjectURL(entry.objectUrl);
        }
      }
      this.coverImages.clear();
    }
    if (this.serialPort && typeof this.serialPort.close === "function") {
      Promise.resolve(this.serialPort.close()).catch(() => {});
      this.serialPort = null;
    }
    if (this.bmo3D && typeof this.bmo3D.dispose === "function") {
      this.bmo3D.dispose();
      this.bmo3D = null;
    }
  }

  showSimulatorMessage(message, isError = false) {
    const log = document.getElementById("playtest-engine-log") || document.getElementById("webserial-log");
    if (log) {
      log.textContent = message;
      log.dataset.status = isError ? "error" : "info";
    }
    const status = document.getElementById("wasm-engine-status");
    if (status && isError) status.textContent = "Error";
  }

  applyUrlStateParams() { applyUrlStateParamsImpl(this); }
  setupInputHandlers() { setupInputHandlersImpl(this); }
  setupUIControls() { setupUIControlsImpl(this); }
  handleButtonPress(key) { handleButtonPressImpl(this, key); }

  createGameLibrary() {
    const db = {};
    CONSOLES_LIST.forEach((c) => { db[c.id] = []; });
    seedBuiltInHomebrewDemos(db);
    this.rebuildFavoritesInDatabase(db);
    return db;
  }

  getVisibleGames(consoleId) {
    let list = this.gameLibrary[consoleId] || [];
    if (consoleId !== "ROM_FAVORITES" && this.topPicksFilter) {
      const popular = list.filter((g) => g && g.isPopular);
      if (popular.length > 0) list = popular;
    }
    const q = String(this.librarySearchQuery || "").trim().toLowerCase();
    if (!q) return list;
    return list.filter((g) => {
      const title = typeof g === "string" ? g : (g && g.title ? g.title : "");
      return title.toLowerCase().includes(q);
    });
  }

  updateLibraryStatsBanner() {
    const badge = document.getElementById("rom-library-count-badge");
    if (!badge) return;
    let total = 0;
    for (const c of CONSOLES_LIST) {
      if (c.id !== "ROM_FAVORITES") total += (this.gameLibrary[c.id] || []).length;
    }
    badge.textContent = `${total.toLocaleString()} ROMs`;
  }

  rebuildFavoritesInDatabase(db) {
    const favs = [];
    Object.keys(db).forEach((cid) => {
      if (cid === "ROM_FAVORITES") return;
      db[cid].forEach((g) => { if (g.isFav) favs.push(g); });
    });
    db.ROM_FAVORITES = favs;
    CONSOLES_LIST.forEach((consoleInfo) => {
      consoleInfo.count = consoleInfo.id === "ROM_FAVORITES" ? favs.length : (db[consoleInfo.id] || []).length;
    });
  }

  handleButtonRelease(key) {
    if (this.bmo3D) this.bmo3D.releaseButton(key, false);
    if (key === "B") this.bmoButtonHoldStart = 0;
  }

  handleBmoButtonPress(key) {
    const now = performance.now();
    const engine = this.requireBmoEngine();
    if (!engine) return;
    if (key === "START") { this.exitBmoCompanion(); return; }
    if (key === "LEFT" || key === "RIGHT") {
      const gazeX = key === "LEFT" ? -0.85 : 0.85;
      if (this.bmoLastGazeX !== 0 && gazeX !== this.bmoLastGazeX) {
        if (now - this.bmoLastDirectionChangeMs < 350) {
          this.bmoDirectionToggleCount++;
          if (this.bmoDirectionToggleCount >= 2) { engine.petBmoCompanion(); this.bmoDirectionToggleCount = 0; }
        } else {
          this.bmoDirectionToggleCount = 0;
        }
        this.bmoLastDirectionChangeMs = now;
      }
      this.bmoLastGazeX = gazeX;
      this.bmoLastInputMs = now;
      return;
    }
    if (key === "UP") engine.toggleBmoDance();
    else if (key === "A") { engine.triggerBmoWink(); engine.sayBmoQuote(7, 3000); }
    else if (key === "B") {
      engine.triggerBmoBlush();
      engine.sayBmoQuote(8, 3000);
      if (this.bmoButtonHoldStart === 0) this.bmoButtonHoldStart = now;
    } else if (key === "SELECT") {
      engine.triggerBmoJoy();
      engine.sayBmoQuote(this.bmoQuoteIndex % 7, 3800);
      this.bmoQuoteIndex++;
    }
    this.bmoLastInputMs = now;
  }

  /** @param {string} title @param {string} gameConsoleId @param {any} gameObj */
  async _resolveGameRomBytes(title, gameConsoleId, gameObj) {
    const targetConsole = CONSOLES_LIST.find((item) => item.id === gameConsoleId);
    if (targetConsole && !targetConsole.browserSupported) {
      const extLabel = gameObj?.ext || targetConsole.ext;
      throw new Error(
        `${targetConsole.name} (${extLabel}) runs on physical ESP32-S3 hardware / native CTest (browser WASM supports GB, GBC, NES, SMS, GG, Atari 2600, and ColecoVision).`
      );
    }
    let romBytes = gameObj?.romBytes || null;
    if (!romBytes && gameObj?.romFile) {
      this.showSimulatorMessage(`Loading ${title}...`);
      romBytes = new Uint8Array(await gameObj.romFile.arrayBuffer());
    } else if (!romBytes && gameObj?.romUrl && typeof fetch === "function") {
      this.showSimulatorMessage(`Loading ${title}...`);
      const resp = await fetch(gameObj.romUrl);
      if (!resp.ok) throw new Error(`Could not fetch local ROM (${resp.status}).`);
      romBytes = new Uint8Array(await resp.arrayBuffer());
    }
    if (!romBytes) throw new Error("This library entry has no ROM data. Load a supported local ROM file to play.");
    if (romBytes.length > MAX_BROWSER_ROM_SIZE) {
      throw new Error(`${title}: exceeds ${Math.round(MAX_BROWSER_ROM_SIZE / (1024 * 1024))} MiB browser limit.`);
    }
    return romBytes;
  }

  async launchSelectedGame(consoleId, gameObj) {
    const title = typeof gameObj === "string" ? gameObj : (gameObj.title || "Game");
    const gameConsoleId = (typeof gameObj === "object" && gameObj && gameObj.systemId) ? gameObj.systemId : consoleId;
    try {
      const romBytes = await this._resolveGameRomBytes(title, gameConsoleId, gameObj);
      if (!(await this.engineReady) || !this.wasmEngine) {
        throw new Error("The WebAssembly emulator cores could not be initialized.");
      }
      this.wasmEngine.loadRom(gameConsoleId, title, romBytes);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      this.showSimulatorMessage(message, true);
      return false;
    }
    this.isPausedInGame = false;
    this.currentState = STATE.EMULATION_VIEW;
    this.bmo.expression = "HAPPY";
    this.runtimeErrorShown = false;
    this.showSimulatorMessage(`Running ${title} in the vendored ${this.wasmEngine.coreName} core.`);
    return true;
  }

  handleExternalButtonPress(key) {
    if (this.wasmEngine && EXT_KEY_BIT_MAP[key]) this.wasmEngine.setButtonBit(EXT_KEY_BIT_MAP[key], true);
    this.handleButtonPress(key);
  }

  handleExternalButtonRelease(key) {
    if (this.wasmEngine && EXT_KEY_BIT_MAP[key]) this.wasmEngine.setButtonBit(EXT_KEY_BIT_MAP[key], false);
    this.handleButtonRelease(key);
  }

  updateViewMode() {
    const stage = document.getElementById("stage-container");
    const toggleBtn = document.getElementById("btn-toggle-viewmode");
    if (!stage) return;
    stage.classList.toggle("view-3d", this.is3DView);
    stage.classList.toggle("view-2d", !this.is3DView);
    if (toggleBtn) toggleBtn.textContent = this.is3DView ? "2D Flat View" : "3D Console View";
    if (this.is3DView && this.bmo3D) this.bmo3D.onResize();
  }

  switchTo2DView() {
    this.is3DView = false;
    this.updateViewMode();
  }

  _getFaceBlinkKey() {
    return this.bmo.isBlinking ? Math.round(this.bmo.blinkState * 12) : -1;
  }

  /** @param {number} cIdx */
  _computeGameSelectSignature(cIdx) {
    const c = CONSOLES_LIST[cIdx];
    const games = c ? this.getVisibleGames(c.id) : [];
    const rawIdx = this.selectedGameIndex ?? 0;
    const idx = games.length > 0 ? Math.min(Math.max(rawIdx, 0), games.length - 1) : -1;
    const game = idx >= 0 ? games[idx] : null;
    const title = typeof game === "string" ? game : (game?.title || "");
    const isFav = Boolean(game && typeof game === "object" && game.isFav);
    const coverSlug = (game && typeof game === "object" && game.coverSlug) ? game.coverSlug : "";
    const cover = title ? this.getCoverImage(title, coverSlug, game) : null;
    let coverFlag = "N";
    if (cover) coverFlag = cover.ready ? "R" : (cover.failed ? "F" : "L");
    return `GAME|${cIdx}|${idx}|${games.length}|${this.topPicksFilter}|${this.librarySearchQuery}|${isFav}|${coverFlag}|${this._getFaceBlinkKey()}`;
  }

  /** @param {number} cIdx */
  _computeEmuSignature(cIdx) {
    const eng = this.wasmEngine;
    if (eng?.ready && eng?.loaded && !this.isPausedInGame) return null;
    const flags = eng ? `${eng.ready}|${eng.loaded}|${eng.frameCount}|${eng.swapAB}|${eng.bezelEnabled}|${eng.runtimeError}` : "none";
    return `EMU|${cIdx}|${this.activePalette}|${this.isPausedInGame}|${this.pauseMenuIndex}|${flags}`;
  }

  /** @param {number} dt */
  _computeRenderSignature(dt) {
    if (this.currentState === STATE.IDLE_MASCOT) return null;
    if (this.currentState === STATE.BOOT_SPLASH) {
      this.bootTimer += dt;
      const blink = Math.floor(this.bootTimer * 2) % 2;
      return `BOOT|${blink}|${this._getFaceBlinkKey()}`;
    }
    const cIdx = this.selectedConsoleIndex ?? 0;
    if (this.currentState === STATE.CONSOLE_MUSEUM) return `MUSEUM|${cIdx}`;
    if (this.currentState === STATE.CONSOLE_SELECT) {
      const c = CONSOLES_LIST[cIdx];
      return `CONS|${cIdx}|${c ? c.count : 0}|${this._getFaceBlinkKey()}`;
    }
    if (this.currentState === STATE.GAME_SELECT) return this._computeGameSelectSignature(cIdx);
    if (this.currentState === STATE.EMULATION_VIEW) return this._computeEmuSignature(cIdx);
    return null;
  }

  renderLoop(timestamp) {
    const dt = (timestamp - this.lastFrameTime) / 1000;
    this.lastFrameTime = timestamp;
    this.fps = Math.round(1 / (dt || 0.016));
    if (this.currentState === STATE.CONSOLE_SELECT && timestamp - this.lastInputActivityMs > 30000) {
      this.enterBmoCompanion(false);
    }
    this.updateBmoAnimation(dt);

    const sig = this._computeRenderSignature(dt);
    const shouldDraw = (sig === null || sig !== this._lastRenderSig);
    if (shouldDraw) {
      this._lastRenderSig = sig;
      if (this.ctx) {
        this.ctx.fillStyle = "#0a0e17";
        this.ctx.fillRect(0, 0, 320, 240);
      }

      /** @type {Record<string | number, () => void>} */
      const screenPainters = {
        [STATE.BOOT_SPLASH]: () => this.renderBootSplash(this.bootTimer === dt ? dt : 0),
        [STATE.CONSOLE_SELECT]: () => this.renderConsoleSelectMenu(),
        [STATE.GAME_SELECT]: () => this.renderGameSelectMenu(),
        [STATE.CONSOLE_MUSEUM]: () => this.renderConsoleMuseumModal(),
        [STATE.EMULATION_VIEW]: () => this.renderEmulationView(),
        [STATE.IDLE_MASCOT]: () => this.renderIdleMascotScreen(timestamp),
      };
      const painter = screenPainters[this.currentState];
      if (painter) painter();
      this._screenTextureDirty = true;
    }
    this.updateTelemetryHud();
    this._rafId = requestAnimationFrame(this._boundRenderLoop);
  }

  updateTelemetryHud() {
    const fpsVal = document.getElementById("telemetry-fps");
    if (fpsVal) fpsVal.innerText = `${this.fps} FPS`;
    const eng = this.wasmEngine;
    if (!eng) return;
    const crcEl = document.getElementById("playtest-crc-val");
    const coreEl = document.getElementById("playtest-core-val");
    const statusEl = document.getElementById("wasm-engine-status");
    if (eng.runtimeError && !this.runtimeErrorShown) {
      this.runtimeErrorShown = true;
      this.showSimulatorMessage(eng.runtimeError, true);
    }
    if (crcEl) crcEl.textContent = "0x" + (eng.romChecksum >>> 0).toString(16).toUpperCase();
    if (coreEl && eng.viewport) coreEl.textContent = `${eng.activeConsoleId.replace("ROM_", "")} (${eng.viewport.w}×${eng.viewport.h})`;
    if (statusEl) {
      statusEl.textContent = this.engineLoadFailed ? "Error" : eng.runtimeError ? "Core error"
        : eng.loaded ? `Frame ${eng.frameCount}` : eng.ready ? "Core ready" : "Loading";
    }
  }

  updateBmoAnimation(dt) {
    this.bmo.blinkTimer += dt;
    if (this.bmo.blinkTimer > 3.5) {
      this.bmo.isBlinking = true;
      this.bmo.blinkState += dt * 8;
      if (this.bmo.blinkState >= 1.0) {
        this.bmo.isBlinking = false;
        this.bmo.blinkState = 0.0;
        this.bmo.blinkTimer = Math.random() * 1.5;
      }
    }
  }

  drawBmoFace(x, y, scale = 1.0) {
    const ctx = this.ctx;
    if (!ctx) return;
    ctx.save();
    ctx.translate(x, y);
    ctx.fillStyle = "#55efc4";
    ctx.beginPath();
    ctx.roundRect(0, 0, 48 * scale, 32 * scale, 6 * scale);
    ctx.fill();
    const eyeH = this.bmo.isBlinking ? Math.max(1, 4 * scale * (1 - this.bmo.blinkState)) : 5 * scale;
    ctx.fillStyle = "#2d3436";
    ctx.beginPath();
    ctx.ellipse(14 * scale, 14 * scale, 3 * scale, eyeH, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.beginPath();
    ctx.ellipse(34 * scale, 14 * scale, 3 * scale, eyeH, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.strokeStyle = "#2d3436";
    ctx.lineWidth = 2 * scale;
    ctx.beginPath();
    if (this.bmo.expression === "HAPPY") ctx.arc(24 * scale, 18 * scale, 6 * scale, 0.2 * Math.PI, 0.8 * Math.PI, false);
    else ctx.arc(24 * scale, 19 * scale, 5 * scale, 0.1 * Math.PI, 0.9 * Math.PI, false);
    ctx.stroke();
    ctx.fillStyle = "rgba(255, 118, 117, 0.6)";
    ctx.beginPath();
    ctx.arc(8 * scale, 17 * scale, 2.5 * scale, 0, Math.PI * 2);
    ctx.arc(40 * scale, 17 * scale, 2.5 * scale, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  slugifyTitle(title) { return slugifyTitle(title); }

  _evictOldestCover(cache) {
    if (cache.size < 32) return;
    const oldestKey = cache.keys().next().value;
    if (oldestKey === undefined) return;
    const victim = cache.get(oldestKey);
    if (victim) victim.disposed = true;
    if (victim && victim.img) {
      victim.img.onload = null;
      victim.img.onerror = null;
      victim.img.src = "";
    }
    if (victim && victim.objectUrl && typeof URL !== "undefined" && typeof URL.revokeObjectURL === "function") {
      URL.revokeObjectURL(victim.objectUrl);
    }
    cache.delete(oldestKey);
  }

  /** @param {any} entry @param {string} slug @param {any} [gameObj] */
  _assignCoverSource(entry, slug, gameObj = null) {
    const isFileProto = typeof window !== "undefined" && window.location?.protocol === "file:";
    const hasUrlApi = typeof URL !== "undefined" && typeof URL.createObjectURL === "function";
    const coverFile = gameObj?.coverFile || null;
    const coverRawFile = gameObj?.coverRawFile || null;
    if (coverFile && hasUrlApi) {
      entry.objectUrl = URL.createObjectURL(coverFile);
      entry.canvasSafe = true;
      entry.img.src = entry.objectUrl;
      return;
    }
    if (coverRawFile && hasUrlApi) {
      entry.canvasSafe = true;
      Promise.resolve(coverRawFile.arrayBuffer()).then((buf) => {
        if (entry.disposed) return;
        entry.objectUrl = URL.createObjectURL(decodeRawBgr565ToBmpBlob(buf));
        entry.img.src = entry.objectUrl;
      }).catch(() => { entry.failed = true; this.markRenderDirty(); });
      return;
    }
    const srcUrl = gameObj?.coverUrl || "";
    entry.canvasSafe = !isFileProto || srcUrl.startsWith("blob:") || srcUrl.startsWith("data:");
    entry.img.src = srcUrl || `covers/${slug}.png`;
  }

  /** @param {string} title @param {string} [slugOverride] @param {any} [gameObj] */
  getCoverImage(title, slugOverride = "", gameObj = null) {
    const slug = slugOverride || this.slugifyTitle(title);
    const cacheKey = (gameObj && typeof gameObj === "object" && gameObj.coverKey) ? gameObj.coverKey : slug;
    const cache = this.coverImages instanceof Map ? this.coverImages : new Map();
    if (!(this.coverImages instanceof Map)) this.coverImages = cache;
    let entry = cache.get(cacheKey);
    if (entry) {
      cache.delete(cacheKey);
      cache.set(cacheKey, entry);
      return entry;
    }
    this._evictOldestCover(cache);
    entry = { img: new Image(), ready: false, failed: false, canvasSafe: true, disposed: false, objectUrl: "", slug: cacheKey };
    entry.img.onload = () => {
      entry.ready = true;
      this.markRenderDirty();
    };
    entry.img.onerror = () => {
      entry.failed = true;
      this.markRenderDirty();
    };
    this._assignCoverSource(entry, slug, gameObj);
    cache.set(cacheKey, entry);
    return entry;
  }

  updateHdCoverPanel(entry, title) {
    const slug = entry ? entry.slug : "";
    if (slug === this.lastHdCoverSlug && (!entry || entry.ready || entry.failed)) return;
    const panelImg = /** @type {HTMLImageElement | null} */ (document.getElementById("hd-boxart-preview-img"));
    const panelMeta = document.getElementById("hd-boxart-meta");
    const panelTitle = document.getElementById("hd-boxart-title");
    if (!panelImg || !panelMeta || !panelTitle) return;
    if (entry && entry.ready) {
      this.lastHdCoverSlug = slug;
      panelImg.src = entry.img.src;
      panelImg.style.visibility = "visible";
      panelTitle.textContent = title;
      panelMeta.textContent = `${entry.img.naturalWidth} × ${entry.img.naturalHeight} box art`;
    } else if (!entry || entry.failed) {
      this.lastHdCoverSlug = slug;
      panelImg.style.visibility = "hidden";
      panelTitle.textContent = title || "Select a Game";
      panelMeta.textContent = entry ? "No cover art for this title" : "Browse any console library to preview artwork";
    }
  }

  renderBootSplash(dt) { renderBootSplashImpl(this, dt); }
  renderConsoleSelectMenu() { renderConsoleSelectMenuImpl(this); }
  renderIdleMascotScreen(now = performance.now()) { renderIdleMascotScreenImpl(this, now); }
  renderGameSelectMenu() { renderGameSelectMenuImpl(this); }
  renderEmulationView() { renderEmulationViewImpl(this); }

  enterBmoCompanion(showGreeting = true, interactiveSandbox = false) { bmoModeMethods.enterBmoCompanion.call(this, showGreeting, interactiveSandbox); }
  enterBmoMode() { bmoModeMethods.enterBmoMode.call(this); }
  ensureBmoModeActive() { bmoModeMethods.ensureBmoModeActive.call(this); }
  exitBmoCompanion() { bmoModeMethods.exitBmoCompanion.call(this); }
  requireBmoEngine() { return bmoModeMethods.requireBmoEngine.call(this); }
  runBmoAction(callback, stopCycle = false) { return bmoModeMethods.runBmoAction.call(this, callback, stopCycle); }
  setBmoModeGaze(gx, gy) { bmoModeMethods.setBmoModeGaze.call(this, gx, gy); }
  updateBmoGazePadVisual(gx, gy) { bmoModeMethods.updateBmoGazePadVisual.call(this, gx, gy); }
  resetBmoNeutral() { bmoModeMethods.resetBmoNeutral.call(this); }
  populateBmoQuotes() { bmoModeMethods.populateBmoQuotes.call(this); }
  syncBmoModeUI() { bmoModeMethods.syncBmoModeUI.call(this); }
  setupBmoModeControls() { bmoModeMethods.setupBmoModeControls.call(this); }
  wrapSpeechLines(text, maxWidth, maxLines = 2) { return bmoModeMethods.wrapSpeechLines.call(this, text, maxWidth, maxLines); }

  syncInternalsLayoutSpec(layout = DEFAULT_HARDWARE_LAYOUT) { internalsMethods.syncInternalsLayoutSpec.call(this, layout); }
  setupInternalsControls() { internalsMethods.setupInternalsControls.call(this); }
  applyInternalsPresetUI(presetName) { internalsMethods.applyInternalsPresetUI.call(this, presetName); }
  setInternalsHighlightUI(tag) { internalsMethods.setInternalsHighlightUI.call(this, tag); }
  selectBeginnerGuideStep(stepIdx, applyTo3D = true) { internalsMethods.selectBeginnerGuideStep.call(this, stepIdx, applyTo3D); }
  handleInternalPartSelected(meta) { internalsMethods.handleInternalPartSelected.call(this, meta); }
  syncExplosionReadout(t) { internalsMethods.syncExplosionReadout.call(this, t); }
  syncLayerCheckboxes() { internalsMethods.syncLayerCheckboxes.call(this); }
  syncInternalsBadge() { internalsMethods.syncInternalsBadge.call(this); }

  updateSidebarMuseumView(historyDb, consolesList) { museumMethods.updateSidebarMuseumView.call(this, historyDb, consolesList); }
  drawConsolePixelArt(c, x, y, w, h) { museumMethods.drawConsolePixelArt.call(this, c, x, y, w, h); }
  renderConsoleMuseumModal() { museumMethods.renderConsoleMuseumModal.call(this); }

  connectWebSerial() { return webserialMethods.connectWebSerial.call(this); }
  ingestUploadedRoms(fileList) { return webserialMethods.ingestUploadedRoms.call(this, fileList); }
}

if (typeof window !== "undefined") {
  const startSimulator = () => {
    if (/** @type {any} */ (window).simulator) return;
    const sim = new BmoSimulator();
    /** @type {any} */ (window).simulator = sim;
    const btnSerial = document.getElementById("btn-webserial-connect");
    if (btnSerial) sim.trackListener(btnSerial, "click", () => sim.connectWebSerial());
    const fileInput = document.getElementById("rom-file-upload");
    if (fileInput) {
      sim.trackListener(fileInput, "change", (/** @type {Event} */ e) => {
        const target = /** @type {HTMLInputElement | null} */ (e.target);
        if (target?.files) sim.ingestUploadedRoms(target.files).catch((error) => sim.showSimulatorMessage(error.message, true));
      });
    }
    const folderInput = document.getElementById("rom-folder-upload");
    if (folderInput) {
      sim.trackListener(folderInput, "change", (/** @type {Event} */ e) => {
        const target = /** @type {HTMLInputElement | null} */ (e.target);
        if (target?.files) sim.ingestUploadedRoms(target.files).catch((error) => sim.showSimulatorMessage(error.message, true));
      });
    }
    const rescanBtn = document.getElementById("btn-rescan-local-library");
    if (rescanBtn) {
      sim.trackListener(rescanBtn, "click", () => {
        if (window.location && window.location.protocol === "file:") {
          if (folderInput) folderInput.click();
          return;
        }
        fetchServerLocalLibrary(sim, true).catch((error) => sim.showSimulatorMessage(error.message, true));
      });
    }
    const searchInput = /** @type {HTMLInputElement | null} */ (document.getElementById("rom-search-input"));
    if (searchInput) {
      sim.trackListener(searchInput, "input", () => {
        sim.librarySearchQuery = searchInput.value;
        sim.selectedGameIndex = 0;
        sim.markRenderDirty();
      });
      sim.trackListener(searchInput, "keydown", (/** @type {KeyboardEvent} */ e) => {
        if (e.key === "Escape") {
          searchInput.value = "";
          sim.librarySearchQuery = "";
          sim.selectedGameIndex = 0;
          sim.markRenderDirty();
          searchInput.blur();
        } else if (e.key === "Enter") {
          e.preventDefault();
          searchInput.blur();
          if (sim.currentState === STATE.BOOT_SPLASH || sim.currentState === STATE.CONSOLE_SELECT) {
            sim.currentState = STATE.GAME_SELECT;
            sim.selectedGameIndex = 0;
            sim.markRenderDirty();
          }
        }
      });
    }
    sim.trackListener(window, "dragover", (/** @type {Event} */ e) => e.preventDefault());
    sim.trackListener(window, "drop", (/** @type {any} */ e) => {
      e.preventDefault();
      if (e.dataTransfer && e.dataTransfer.files) {
        sim.ingestUploadedRoms(e.dataTransfer.files).catch((error) => sim.showSimulatorMessage(error.message, true));
      }
    });
  };
  const ready = typeof document !== "undefined" ? document.readyState : undefined;
  if (ready === "interactive" || ready === "complete") {
    startSimulator();
  } else {
    window.addEventListener("DOMContentLoaded", startSimulator);
  }
}
