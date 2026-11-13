/**
 * simulator.js
 * Browser simulator for local Game Boy, NES, Atari 2600, and ColecoVision ROMs.
 * The menu shell is canvas-rendered; gameplay comes from firmware emulator cores in WebAssembly.
 */

// State Machine Constants (Synchronized with BmoGameboy.ino)
const STATE = {
  BOOT_SPLASH: 0,
  CONSOLE_SELECT: 1,
  GAME_SELECT: 2,
  CONSOLE_MUSEUM: 3,
  EMULATION_VIEW: 4,
  IDLE_MASCOT: 5
};
const MAX_BROWSER_ROM_SIZE = 8 * 1024 * 1024;

// Console Metadata
const CONSOLES_LIST = [
  { id: "ROM_FAVORITES", name: "★ Favorites", year: "Starred", ext: "★", badge: "FAV", color: "#FFE033", count: 0 },
  { id: "ROM_GB", name: "Game Boy", year: "1989", ext: ".gb", badge: "DMG", color: "#5FB49C", count: 0, browserSupported: true },
  { id: "ROM_GBC", name: "Game Boy Color", year: "1998", ext: ".gbc", badge: "GBC", color: "#2ED573", count: 0 },
  { id: "ROM_NES", name: "Nintendo NES", year: "1983", ext: ".nes", badge: "NES", color: "#FF5E57", count: 0, browserSupported: true },
  { id: "ROM_SNES", name: "Super Nintendo", year: "1990", ext: ".sfc", badge: "SNES", color: "#9C88FF", count: 0 },
  { id: "ROM_GENESIS", name: "Sega Genesis", year: "1988", ext: ".gen", badge: "MD", color: "#2E86DE", count: 0 },
  { id: "ROM_SMS", name: "Master System", year: "1985", ext: ".sms", badge: "SMS", color: "#3B82F6", count: 0 },
  { id: "ROM_GG", name: "Game Gear", year: "1990", ext: ".gg", badge: "GG", color: "#F368E0", count: 0 },
  { id: "ROM_PCE", name: "PC Engine / TG16", year: "1987", ext: ".pce", badge: "PCE", color: "#FF6B6B", count: 0 },
  { id: "ROM_ATARI", name: "Atari 2600", year: "1977", ext: ".a26", badge: "A26", color: "#E056FD", count: 0, browserSupported: true },
  { id: "ROM_COLEM", name: "ColecoVision", year: "1982", ext: ".col", badge: "COL", color: "#00D2D3", count: 0, browserSupported: true },
  { id: "ROM_NGP", name: "Neo Geo Pocket", year: "1998", ext: ".ngc", badge: "NGP", color: "#FFA502", count: 0 },
  { id: "ROM_LYNX", name: "Atari Lynx", year: "1989", ext: ".lnx", badge: "LNX", color: "#10AC84", count: 0 },
  { id: "ROM_WSWAN", name: "WonderSwan Color", year: "1999", ext: ".wsc", badge: "WS", color: "#ECCC68", count: 0 },
  { id: "ROM_PICO8", name: "PICO-8", year: "2015", ext: ".p8", badge: "P8", color: "#FF78C4", count: 0 },
  { id: "ROM_WAD", name: "DOOM", year: "1993", ext: ".wad", badge: "WAD", color: "#FF4757", count: 0 }
];

class BmoSimulator {
  constructor() {
    this.canvas = document.getElementById("tft-display");
    this.ctx = this.canvas.getContext("2d", { alpha: false });
    this.canvas.width = 320;
    this.canvas.height = 240;

    // Simulation State
    this.currentState = STATE.BOOT_SPLASH;
    this.selectedConsoleIndex = 0;
    this.selectedGameIndex = 0;
    this.gameListScrollOffset = 0;
    this.topPicksFilter = false;
    // Box art cache keyed by slug; values are { img, ready, failed }.
    this.coverImages = {};
    this.lastHdCoverSlug = "";
    this.bootTimer = 0;
    this.lastFrameTime = performance.now();
    this.fps = 60;
    this._activeMouseButton = null;
    this._boundRenderLoop = this.renderLoop.bind(this);

    // BMO Mascot Procedural 2D SDF State
    this.bmo = {
      expression: "IDLE",
      blinkState: 0.0,
      blinkTimer: 0,
      eyeLookX: 0,
      eyeLookY: 0,
      mouthCurve: 0.8,
      isBlinking: false
    };
    this.bmoSessionStarted = false;
    this.bmoEntryGreeting = true;
    this.bmoQuoteIndex = 0;
    this.bmoLastInputMs = performance.now();
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

    // Hardware Internals Dissection State
    this.internalsModeActive = false;
    this.internalsPreset = "assembled";
    this.internalsHighlight = "all";
    this.internalsStepIndex = 0;
    this.pendingInternalsPreset = null;

    // DMG Runtime Palettes
    this.palettes = {
      classic: ["#9bbc0f", "#8bac0f", "#306230", "#0f380f"],
      bmo:     ["#dcf8f2", "#53c6b5", "#164848", "#051214"],
      pocket:  ["#f5f5f5", "#aaaaaa", "#555555", "#0a0a0a"],
      light:   ["#b4fff0", "#50c8d2", "#146478", "#021e28"],
      amber:   ["#ffd250", "#dc9614", "#784600", "#140a00"]
    };
    this.activePalette = "classic";

    // Browser library contains only user-selected ROM files.
    this.gameLibrary = this.createGameLibrary();

    // Firmware-derived Game Boy, NES, Atari, and ColecoVision cores compiled to WebAssembly.
    this.isPausedInGame = false;
    this.pauseMenuIndex = 0;
    this.wasmEngine = (typeof window !== "undefined" && window.WasmPlaytestEngine)
      ? new window.WasmPlaytestEngine()
      : null;
    this.engineLoadFailed = false;
    this.engineLoadError = "";
    this.engineReady = this.wasmEngine
      ? this.wasmEngine.init().then(() => {
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
      })
      : Promise.resolve(false);

    // Setup input listeners
    this.setupInputHandlers();
    this.setupUIControls();
    this.applyUrlStateParams();

    // Initialize 3D WebGL Console Body
    this.bmo3D = null;
    this.is3DView = true;
    if (typeof THREE !== "undefined" && typeof Bmo3DConsole !== "undefined") {
      const threeContainer = document.getElementById("three-viewport-container");
      if (threeContainer) {
        try {
          this.bmo3D = new Bmo3DConsole(
            threeContainer,
            this.canvas,
            (key) => this.handleExternalButtonPress(key),
            (key) => this.handleExternalButtonRelease(key)
          );
          this.bmo3D.setOnInternalSelect((meta) => this.handleInternalPartSelected(meta));
          this.bmo3D.onExplosionChange = (t) => this.syncExplosionReadout(t);
          if (this.pendingInternalsPreset) {
            this.applyInternalsPresetUI(this.pendingInternalsPreset);
          }
        } catch (err) {
          console.warn("Failed to initialize 3D console view:", err);
          this.switchTo2DView();
        }
      }
    } else {
      this.switchTo2DView();
    }

    // Start 60 FPS Render Loop
    requestAnimationFrame(this._boundRenderLoop);
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

  applyUrlStateParams() {
    if (typeof window === "undefined" || !window.location) return;
    const params = new URLSearchParams(window.location.search);
    const consoleParam = params.get("console");
    if (consoleParam !== null) {
      const idx = parseInt(consoleParam, 10);
      if (!isNaN(idx) && idx >= 0 && idx < CONSOLES_LIST.length) {
        this.selectedConsoleIndex = idx;
        this.updateSidebarMuseumView();
      }
    }
    const gameParam = params.get("game");
    if (gameParam !== null) {
      const gi = parseInt(gameParam, 10);
      if (!isNaN(gi) && gi >= 0) this.selectedGameIndex = gi;
    }
    const stateParam = (params.get("state") || "").toLowerCase();
    if (stateParam === "splash") {
      this.currentState = STATE.BOOT_SPLASH;
    } else if (stateParam === "bmo") {
      this.enterBmoMode();
    } else if (stateParam === "internals" || stateParam === "dissect") {
      this.pendingInternalsPreset = "exploded";
    } else if (stateParam === "core" || stateParam === "unscrew" || stateParam === "wiring" || stateParam === "display") {
      this.pendingInternalsPreset = stateParam;
    } else if (stateParam === "mascot") {
      this.enterBmoCompanion(true, false);
    } else if (stateParam === "carousel") {
      this.currentState = STATE.CONSOLE_SELECT;
      if (consoleParam === null) this.selectedConsoleIndex = 2; // Game Boy Color
      this.updateSidebarMuseumView();
    } else if (stateParam === "library") {
      this.currentState = STATE.GAME_SELECT;
      if (consoleParam === null) this.selectedConsoleIndex = 2;
      this.topPicksFilter = false;
    } else if (stateParam === "toppicks") {
      this.currentState = STATE.GAME_SELECT;
      if (consoleParam === null) this.selectedConsoleIndex = 2;
      this.topPicksFilter = true;
    } else if (stateParam === "museum") {
      this.currentState = STATE.CONSOLE_MUSEUM;
      if (consoleParam === null) this.selectedConsoleIndex = 2;
      this.updateSidebarMuseumView();
    } else if (stateParam === "gameplay") {
      if (consoleParam === null) this.selectedConsoleIndex = 1;
      const cid = CONSOLES_LIST[this.selectedConsoleIndex].id;
      const games = this.gameLibrary[cid] || [];
      if (games.length > 0) this.launchSelectedGame(cid, games[0]);
      else this.currentState = STATE.GAME_SELECT;
    }
  }

  createGameLibrary() {
    const db = {};
    CONSOLES_LIST.forEach(c => {
      db[c.id] = [];
    });
    db.ROM_FAVORITES = [];
    return db;
  }

  getVisibleGames(consoleId) {
    const all = this.gameLibrary[consoleId] || [];
    if (consoleId === "ROM_FAVORITES" || !this.topPicksFilter) {
      return all;
    }
    const popular = all.filter(g => g && g.isPopular);
    return popular.length > 0 ? popular : all;
  }

  rebuildFavoritesInDatabase(db) {
    const favs = [];
    Object.keys(db).forEach(cid => {
      if (cid === "ROM_FAVORITES") return;
      db[cid].forEach(g => {
        if (g.isFav) favs.push(g);
      });
    });
    db.ROM_FAVORITES = favs;
    const favConsole = CONSOLES_LIST.find(c => c.id === "ROM_FAVORITES");
    if (favConsole) favConsole.count = favs.length;
    CONSOLES_LIST.forEach((consoleInfo) => {
      if (consoleInfo.id !== "ROM_FAVORITES") {
        consoleInfo.count = (db[consoleInfo.id] || []).length;
      }
    });
  }

  setupInputHandlers() {
    // Bits 0-7 match the 8 base buttons. Bits 8-11 are the extra X, Y, L, R
    // buttons (firmware GPIO 14, 42, 47, 48). The WASM cores mask the value to
    // 8 bits, so the extra bits are tracked for the UI without changing them.
    const keyBitMap = {
      ArrowUp: 1, ArrowDown: 2, ArrowLeft: 4, ArrowRight: 8,
      z: 16, Z: 16, x: 32, X: 32, Enter: 64, Shift: 128,
      s: 256, S: 256, a: 512, A: 512, q: 1024, Q: 1024, w: 2048, W: 2048
    };

    const isEditableTarget = (target) => {
      if (!target) return false;
      const tag = target.tagName ? target.tagName.toUpperCase() : "";
      return Boolean(target.isContentEditable || tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT");
    };

    // Keyboard Handler
    window.addEventListener("keydown", (e) => {
      if (isEditableTarget(e.target)) return;
      if (e.repeat && this.currentState === STATE.IDLE_MASCOT) return;
      if (this.wasmEngine && keyBitMap[e.key]) {
        this.wasmEngine.setButtonBit(keyBitMap[e.key], true);
      }
      switch (e.key) {
        case "ArrowUp":
          this.handleButtonPress("UP");
          e.preventDefault();
          break;
        case "ArrowDown":
          this.handleButtonPress("DOWN");
          e.preventDefault();
          break;
        case "ArrowLeft":
          this.handleButtonPress("LEFT");
          e.preventDefault();
          break;
        case "ArrowRight":
          this.handleButtonPress("RIGHT");
          e.preventDefault();
          break;
        case "z":
        case "Z":
          this.handleButtonPress("A");
          break;
        case "x":
        case "X":
          this.handleButtonPress("B");
          break;
        case "Enter":
          this.handleButtonPress("START");
          break;
        case "Shift":
          this.handleButtonPress("SELECT");
          break;
        case "s":
        case "S":
          this.handleButtonPress("X");
          break;
        case "a":
        case "A":
          this.handleButtonPress("Y");
          break;
        case "q":
        case "Q":
          this.handleButtonPress("L");
          break;
        case "w":
        case "W":
          this.handleButtonPress("R");
          break;
        case " ":
        case "m":
        case "M":
          this.handleButtonPress("MUSEUM");
          break;
      }
    });

    window.addEventListener("keyup", (e) => {
      if (isEditableTarget(e.target)) return;
      if (this.wasmEngine && keyBitMap[e.key]) {
        this.wasmEngine.setButtonBit(keyBitMap[e.key], false);
      }
      const releasedKey = {
        ArrowUp: "UP", ArrowDown: "DOWN", ArrowLeft: "LEFT", ArrowRight: "RIGHT",
        z: "A", Z: "A", x: "B", X: "B", Enter: "START", Shift: "SELECT",
        s: "X", S: "X", a: "Y", A: "Y", q: "L", Q: "L", w: "R", W: "R"
      }[e.key];
      if (releasedKey) this.handleButtonRelease(releasedKey);
    });

    // Touch & Mouse On-Screen Physical Buttons
    const buttons = [
      { id: "btn-dpad-up", key: "UP", bit: 1 },
      { id: "btn-dpad-down", key: "DOWN", bit: 2 },
      { id: "btn-dpad-left", key: "LEFT", bit: 4 },
      { id: "btn-dpad-right", key: "RIGHT", bit: 8 },
      { id: "btn-a", key: "A", bit: 16 },
      { id: "btn-b", key: "B", bit: 32 },
      { id: "btn-start", key: "START", bit: 64 },
      { id: "btn-select", key: "SELECT", bit: 128 },
      { id: "btn-x", key: "X", bit: 256 },
      { id: "btn-y", key: "Y", bit: 512 },
      { id: "btn-l", key: "L", bit: 1024 },
      { id: "btn-r", key: "R", bit: 2048 }
    ];

    buttons.forEach(b => {
      const el = document.getElementById(b.id);
      if (el) {
        el.addEventListener("mousedown", () => {
          el.classList.add("pressed");
          this._activeMouseButton = { el, b };
          if (this.wasmEngine) this.wasmEngine.setButtonBit(b.bit, true);
          this.handleButtonPress(b.key);
        });
        el.addEventListener("touchstart", (e) => {
          e.preventDefault();
          el.classList.add("pressed");
          if (this.wasmEngine) this.wasmEngine.setButtonBit(b.bit, true);
          this.handleButtonPress(b.key);
        });
        el.addEventListener("touchend", () => {
          el.classList.remove("pressed");
          if (this.wasmEngine) this.wasmEngine.setButtonBit(b.bit, false);
          this.handleButtonRelease(b.key);
        });
      }
    });
    window.addEventListener("mouseup", () => {
      if (!this._activeMouseButton) return;
      const { el, b } = this._activeMouseButton;
      this._activeMouseButton = null;
      el.classList.remove("pressed");
      if (this.wasmEngine) this.wasmEngine.setButtonBit(b.bit, false);
      this.handleButtonRelease(b.key);
    });

    if (this.canvas) {
      let draggingScreen = false;
      let screenMoved = false;
      let startX = 0;
      let startY = 0;
      const aimFromCanvasEvent = (e) => {
        const rect = this.canvas.getBoundingClientRect();
        if (!rect.width || !rect.height) return;
        const normX = ((e.clientX - rect.left) / rect.width) * 2 - 1;
        const normY = 1 - ((e.clientY - rect.top) / rect.height) * 2;
        this.setBmoModeGaze(normX, normY);
      };
      this.canvas.addEventListener("pointerdown", (e) => {
        if (this.currentState !== STATE.IDLE_MASCOT) return;
        draggingScreen = true;
        screenMoved = false;
        startX = e.clientX;
        startY = e.clientY;
        aimFromCanvasEvent(e);
      });
      window.addEventListener("pointermove", (e) => {
        if (!draggingScreen || this.currentState !== STATE.IDLE_MASCOT) return;
        if (Math.hypot(e.clientX - startX, e.clientY - startY) > 4) {
          screenMoved = true;
        }
        aimFromCanvasEvent(e);
      });
      window.addEventListener("pointerup", () => {
        if (!draggingScreen) return;
        draggingScreen = false;
        if (!screenMoved && this.currentState === STATE.IDLE_MASCOT) {
          this.runBmoAction((engine) => engine.petBmoCompanion(), false);
        }
      });
    }
  }

  enterBmoCompanion(showGreeting = true, interactiveSandbox = false) {
    this.currentState = STATE.IDLE_MASCOT;
    this.bmoSessionStarted = false;
    this.bmoEntryGreeting = showGreeting;
    this.bmoModeActive = interactiveSandbox;
    this.bmoAutoSleep = !interactiveSandbox;
    this.bmoAutoCycle = false;
    this.bmoQuoteIndex = 0;
    this.bmoLastInputMs = performance.now();
    this.lastInputActivityMs = this.bmoLastInputMs;
    this.bmoButtonHoldStart = 0;
    this.bmoLastGazeX = 0;
    this.bmoDirectionToggleCount = 0;
    const chkSleep = document.getElementById("chk-bmo-autosleep");
    if (chkSleep) chkSleep.checked = this.bmoAutoSleep;
    const chkCycle = document.getElementById("chk-bmo-autocycle");
    if (chkCycle) chkCycle.checked = false;
    this.syncBmoModeUI();
  }

  enterBmoMode() {
    this.enterBmoCompanion(true, true);
  }

  ensureBmoModeActive() {
    if (this.currentState !== STATE.IDLE_MASCOT) {
      this.enterBmoCompanion(false, true);
    } else if (!this.bmoModeActive) {
      this.bmoModeActive = true;
      this.bmoAutoSleep = false;
      const chkSleep = document.getElementById("chk-bmo-autosleep");
      if (chkSleep) chkSleep.checked = false;
    }
    this.bmoSessionStarted = true;
  }

  exitBmoCompanion() {
    this.currentState = STATE.CONSOLE_SELECT;
    this.bmoSessionStarted = false;
    this.bmoModeActive = false;
    this.bmoAutoCycle = false;
    this.bmoButtonHoldStart = 0;
    if (this.wasmEngine && this.wasmEngine.bmoReady) {
      this.wasmEngine.stopBmoDance();
      this.wasmEngine.setBmoExpression("HAPPY");
      this.wasmEngine.clearBmoSpeech();
    }
    const chkCycle = document.getElementById("chk-bmo-autocycle");
    if (chkCycle) chkCycle.checked = false;
    this.bmoLastInputMs = performance.now();
    this.lastInputActivityMs = this.bmoLastInputMs;
    this.syncBmoModeUI();
  }

  requireBmoEngine() {
    const engine = this.wasmEngine;
    if (engine && engine.bmoReady) return engine;
    const message = this.engineLoadFailed
      ? (this.engineLoadError || "The firmware BMO WebAssembly core failed to load.")
      : "The firmware BMO companion is still loading.";
    this.showSimulatorMessage(message, this.engineLoadFailed);
    return null;
  }

  runBmoAction(callback, stopCycle = false) {
    const engine = this.requireBmoEngine();
    if (!engine) return false;
    this.ensureBmoModeActive();
    if (stopCycle) {
      this.bmoAutoCycle = false;
      const chkCycle = document.getElementById("chk-bmo-autocycle");
      if (chkCycle) chkCycle.checked = false;
    }
    this.bmoLastInputMs = performance.now();
    this.lastInputActivityMs = this.bmoLastInputMs;
    callback(engine);
    this.syncBmoModeUI();
    return true;
  }

  setBmoModeGaze(gx, gy) {
    const clampedX = Math.max(-1, Math.min(1, Number(gx) || 0));
    const clampedY = Math.max(-1, Math.min(1, Number(gy) || 0));
    this.bmoGazeX = clampedX;
    this.bmoGazeY = clampedY;
    this.updateBmoGazePadVisual(clampedX, clampedY);
    this.runBmoAction((engine) => engine.setBmoGaze(clampedX, clampedY));
  }

  updateBmoGazePadVisual(gx, gy) {
    const dot = document.getElementById("bmo-gaze-dot");
    if (dot) {
      dot.style.left = `${((gx + 1) * 50).toFixed(1)}%`;
      dot.style.top = `${((1 - gy) * 50).toFixed(1)}%`;
    }
    const label = document.getElementById("val-bmo-gaze");
    if (label) label.textContent = `${gx.toFixed(2)}, ${gy.toFixed(2)}`;
  }

  resetBmoNeutral() {
    this.bmoGazeX = 0;
    this.bmoGazeY = 0;
    this.updateBmoGazePadVisual(0, 0);
    this.runBmoAction((engine) => {
      if (engine.bmoIsDancing()) engine.stopBmoDance();
      engine.clearBmoSpeech();
      engine.setBmoGaze(0, 0);
      engine.setBmoExpression("IDLE");
    }, true);
  }

  populateBmoQuotes() {
    const select = document.getElementById("select-bmo-quote");
    if (!select || !this.wasmEngine || !this.wasmEngine.bmoReady) return;
    const quotes = this.wasmEngine.bmoQuotes();
    if (!quotes.length) return;
    const prevValue = select.value;
    select.replaceChildren();
    quotes.forEach((quote, idx) => {
      const opt = document.createElement("option");
      opt.value = String(idx);
      opt.textContent = `${idx}: ${quote}`;
      select.appendChild(opt);
    });
    if (prevValue && Number(prevValue) < quotes.length) {
      select.value = prevValue;
    }
  }

  syncBmoModeUI() {
    const isInMascot = this.currentState === STATE.IDLE_MASCOT;
    const btnToolbar = document.getElementById("btn-bmo-mode");
    if (btnToolbar) {
      btnToolbar.classList.toggle("active", isInMascot);
      btnToolbar.setAttribute("aria-pressed", isInMascot ? "true" : "false");
    }
    const panel = document.getElementById("bmo-mode-panel");
    if (panel) panel.classList.toggle("is-active", isInMascot);

    const engine = this.wasmEngine;
    const ready = Boolean(engine && engine.bmoReady);
    const exprId = ready ? engine.bmoExpression() : 0;
    const exprName = ready ? engine.bmoExpressionName(exprId) : "IDLE";
    const dancing = ready ? engine.bmoIsDancing() : false;

    const statusBadge = document.getElementById("bmo-mode-status");
    if (statusBadge) {
      statusBadge.textContent = this.engineLoadFailed
        ? "WASM Error"
        : !ready ? "Loading" : isInMascot ? `Active · ${exprName}` : "Standby";
    }
    const statExpr = document.getElementById("bmo-stat-expr");
    if (statExpr) statExpr.textContent = `${exprName} (${exprId})`;
    const statBuddy = document.getElementById("bmo-stat-buddy");
    if (statBuddy && ready) statBuddy.textContent = `Lv.${engine.bmoBuddyLevel()}`;
    const statHappy = document.getElementById("bmo-stat-happy");
    if (statHappy && ready) statHappy.textContent = `${engine.bmoHappiness()}%`;
    const statDance = document.getElementById("bmo-stat-dance");
    if (statDance) statDance.textContent = dancing ? `On (${this.bmoDanceBpm} BPM)` : `Off (${this.bmoDanceBpm} BPM)`;

    const btnDance = document.getElementById("btn-bmo-dance");
    if (btnDance) {
      btnDance.classList.toggle("active", dancing);
      btnDance.setAttribute("aria-pressed", dancing ? "true" : "false");
      btnDance.textContent = dancing ? "Stop Dance" : "Dance Party";
    }
    document.querySelectorAll(".bmo-expr-btn").forEach((btn) => {
      const targetExpr = btn.getAttribute("data-bmo-expr");
      btn.classList.toggle("active", isInMascot && targetExpr === exprName);
    });
  }

  handleButtonRelease(key) {
    if (this.bmo3D) {
      this.bmo3D.releaseButton(key, false);
    }
    if (key === "B") this.bmoButtonHoldStart = 0;
  }

  handleBmoButtonPress(key) {
    const now = performance.now();
    const engine = this.requireBmoEngine();
    if (!engine) return;

    if (key === "START") {
      this.exitBmoCompanion();
      return;
    }
    if (key === "LEFT" || key === "RIGHT") {
      const gazeX = key === "LEFT" ? -0.85 : 0.85;
      if (this.bmoLastGazeX !== 0 && gazeX !== this.bmoLastGazeX) {
        if (now - this.bmoLastDirectionChangeMs < 350) {
          this.bmoDirectionToggleCount++;
          if (this.bmoDirectionToggleCount >= 2) {
            engine.petBmoCompanion();
            this.bmoDirectionToggleCount = 0;
          }
        } else {
          this.bmoDirectionToggleCount = 0;
        }
        this.bmoLastDirectionChangeMs = now;
      }
      this.bmoLastGazeX = gazeX;
      this.bmoLastInputMs = now;
      return;
    }
    if (key === "UP" || key === "DOWN") {
      this.bmoLastInputMs = now;
      if (key === "UP") engine.toggleBmoDance();
      return;
    }
    if (key === "A") {
      engine.triggerBmoWink();
      engine.sayBmoQuote(7, 3000);
    } else if (key === "B") {
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

  handleButtonPress(key) {
    this.lastInputActivityMs = performance.now();
    if (this.bmo3D) {
      this.bmo3D.pressButton(key, false);
    }
    // Visual button reflection
    const btnMap = {
      "UP": "btn-dpad-up", "DOWN": "btn-dpad-down", "LEFT": "btn-dpad-left", "RIGHT": "btn-dpad-right",
      "A": "btn-a", "B": "btn-b", "START": "btn-start", "SELECT": "btn-select",
      "X": "btn-x", "Y": "btn-y", "L": "btn-l", "R": "btn-r"
    };
    if (btnMap[key]) {
      const el = document.getElementById(btnMap[key]);
      if (el) {
        el.classList.add("pressed");
        setTimeout(() => el.classList.remove("pressed"), 120);
      }
    }

    if (this.currentState === STATE.BOOT_SPLASH) {
      this.currentState = STATE.CONSOLE_SELECT;
      this.bmo.expression = "HAPPY";
      return;
    }

    if (
      this.currentState === STATE.EMULATION_VIEW &&
      !this.isPausedInGame &&
      this.wasmEngine &&
      (this.wasmEngine.buttonsMask & 0xC0) === 0xC0
    ) {
      this.isPausedInGame = true;
      this.pauseMenuIndex = 0;
      return;
    }

    if (this.currentState === STATE.CONSOLE_SELECT) {
      if (key === "UP") {
        this.enterBmoCompanion();
      } else if (key === "RIGHT") {
        this.selectedConsoleIndex = (this.selectedConsoleIndex + 1) % CONSOLES_LIST.length;
        this.updateSidebarMuseumView();
      } else if (key === "LEFT") {
        this.selectedConsoleIndex = (this.selectedConsoleIndex - 1 + CONSOLES_LIST.length) % CONSOLES_LIST.length;
        this.updateSidebarMuseumView();
      } else if (key === "A" || key === "START") {
        this.currentState = STATE.GAME_SELECT;
        this.selectedGameIndex = 0;
        this.gameListScrollOffset = 0;
      } else if (key === "SELECT" || key === "MUSEUM") {
        this.currentState = STATE.CONSOLE_MUSEUM;
      }
    } else if (this.currentState === STATE.CONSOLE_MUSEUM) {
      if (key === "B" || key === "SELECT" || key === "A" || key === "START") {
        this.currentState = STATE.CONSOLE_SELECT;
      }
    } else if (this.currentState === STATE.GAME_SELECT) {
      const activeConsole = CONSOLES_LIST[this.selectedConsoleIndex];
      const games = this.getVisibleGames(activeConsole.id);
      
      if (key === "UP") {
        if (this.selectedGameIndex > 0) this.selectedGameIndex--;
      } else if (key === "DOWN") {
        if (this.selectedGameIndex < games.length - 1) this.selectedGameIndex++;
      } else if (key === "RIGHT") {
        this.selectedGameIndex = Math.min(games.length - 1, this.selectedGameIndex + 8);
      } else if (key === "LEFT") {
        this.selectedGameIndex = Math.max(0, this.selectedGameIndex - 8);
      } else if (key === "START" && activeConsole.id !== "ROM_FAVORITES") {
        this.topPicksFilter = !this.topPicksFilter;
        this.selectedGameIndex = 0;
      } else if (key === "SELECT") {
        if (games.length > 0) {
          const game = games[this.selectedGameIndex];
          if (game) {
            game.isFav = !game.isFav;
            this.rebuildFavoritesInDatabase(this.gameLibrary);
            if (game.isFav) {
              this.bmo.expression = "HAPPY";
            } else {
              this.bmo.expression = "SURPRISED";
            }
            if (activeConsole.id === "ROM_FAVORITES") {
              if (this.selectedGameIndex >= (this.gameLibrary.ROM_FAVORITES || []).length) {
                this.selectedGameIndex = Math.max(0, (this.gameLibrary.ROM_FAVORITES || []).length - 1);
              }
            }
          }
        }
      } else if (key === "B") {
        this.topPicksFilter = false;
        this.currentState = STATE.CONSOLE_SELECT;
        this.bmo.expression = "IDLE";
      } else if (key === "A") {
        if (games.length > 0) {
          this.launchSelectedGame(activeConsole.id, games[this.selectedGameIndex]);
        }
      }
    } else if (this.currentState === STATE.IDLE_MASCOT) {
      this.handleBmoButtonPress(key);
    } else if (this.currentState === STATE.EMULATION_VIEW) {
      if (this.isPausedInGame) {
        if (key === "UP") {
          this.pauseMenuIndex = (this.pauseMenuIndex + 3) % 4;
        } else if (key === "DOWN") {
          this.pauseMenuIndex = (this.pauseMenuIndex + 1) % 4;
        } else if (key === "LEFT" && this.wasmEngine) {
          this.wasmEngine.swapAB = !this.wasmEngine.swapAB;
        } else if (key === "RIGHT" && this.wasmEngine) {
          this.wasmEngine.bezelEnabled = !this.wasmEngine.bezelEnabled;
        } else if (key === "B") {
          this.isPausedInGame = false;
        } else if (key === "A" || key === "START") {
          if (this.pauseMenuIndex === 0) {
            this.isPausedInGame = false;
          } else if (this.pauseMenuIndex === 1 && this.wasmEngine) {
            const saved = this.wasmEngine.saveState(this.wasmEngine.activeSlot);
            this.showSimulatorMessage(saved ? "Game state saved for this browser session." : "Could not save the current game state.", !saved);
            this.isPausedInGame = false;
          } else if (this.pauseMenuIndex === 2 && this.wasmEngine) {
            const loaded = this.wasmEngine.loadState(this.wasmEngine.activeSlot);
            this.showSimulatorMessage(loaded ? "Game state restored." : "No saved state exists for this game in this browser session.", !loaded);
            this.isPausedInGame = false;
          } else if (this.pauseMenuIndex === 3) {
            this.isPausedInGame = false;
            this.currentState = STATE.GAME_SELECT;
          }
        }
      }
    }
  }

  async launchSelectedGame(consoleId, gameObj) {
    const title = typeof gameObj === "string" ? gameObj : (gameObj.title || "Game");
    const romBytes = (typeof gameObj === "object" && gameObj.romBytes) ? gameObj.romBytes : null;
    const gameConsoleId = typeof gameObj === "object" && gameObj.systemId
      ? gameObj.systemId
      : consoleId;
    try {
      if (!romBytes) throw new Error("This library entry has no ROM data. Load a supported local ROM file to play.");
      if (!(await this.engineReady) || !this.wasmEngine) {
        throw new Error("The WebAssembly emulator cores could not be initialized.");
      }
      this.wasmEngine.loadRom(gameConsoleId, title, romBytes);
    } catch (error) {
      this.showSimulatorMessage(error.message, true);
      return false;
    }
    this.isPausedInGame = false;
    this.currentState = STATE.EMULATION_VIEW;
    this.bmo.expression = "HAPPY";
    this.runtimeErrorShown = false;
    this.showSimulatorMessage(`Running ${title} in the vendored ${this.wasmEngine.coreName} core.`);
    return true;
  }

  setupUIControls() {
    // Scale controls
    document.querySelectorAll(".scale-btn").forEach(btn => {
      btn.addEventListener("click", (e) => {
        document.querySelectorAll(".scale-btn").forEach(b => b.classList.remove("active"));
        btn.classList.add("active");
        const scale = btn.getAttribute("data-scale");
        document.documentElement.style.setProperty("--scale-multiplier", scale);
      });
    });

    // Theme selector
    const themeSelect = document.getElementById("theme-select");
    if (themeSelect) {
      themeSelect.addEventListener("change", (e) => {
        document.body.setAttribute("data-theme", e.target.value);
        if (this.bmo3D) this.bmo3D.setTheme(e.target.value);
      });
    }

    // 3D Camera Preset Buttons
    document.querySelectorAll(".cam-preset-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        document.querySelectorAll(".cam-preset-btn").forEach((b) => b.classList.remove("active"));
        btn.classList.add("active");
        const cam = btn.getAttribute("data-cam");
        if (!this.is3DView) {
          this.is3DView = true;
          this.updateViewMode();
        }
        if (cam === "core") {
          this.applyInternalsPresetUI("core");
          return;
        }
        if (cam === "reset") {
          this.applyInternalsPresetUI("assembled");
          if (this.bmo3D) this.bmo3D.setCameraPreset("front");
          return;
        }
        if (this.bmo3D) {
          this.bmo3D.setCameraPreset(cam);
        }
      });
    });

    // View Mode Toggle (3D WebGL Console vs 2D Flat Layout)
    const btnToggleView = document.getElementById("btn-toggle-viewmode");
    if (btnToggleView) {
      btnToggleView.addEventListener("click", () => {
        this.is3DView = !this.is3DView;
        this.updateViewMode();
      });
    }

    // Studio Lighting, Saturation & Temperature Controls
    const sliderLighting = document.getElementById("slider-lighting");
    const sliderSaturation = document.getElementById("slider-saturation");
    const sliderTemperature = document.getElementById("slider-temperature");
    const valLighting = document.getElementById("val-lighting");
    const valSaturation = document.getElementById("val-saturation");
    const valTemperature = document.getElementById("val-temperature");
    const btnResetStudio = document.getElementById("btn-reset-studio");

    const updateStudio = () => {
      const bVal = sliderLighting ? parseInt(sliderLighting.value, 10) : 100;
      const sVal = sliderSaturation ? parseInt(sliderSaturation.value, 10) : 100;
      const tVal = sliderTemperature ? parseInt(sliderTemperature.value, 10) : 0;

      if (valLighting) valLighting.textContent = `${bVal}%`;
      if (valSaturation) valSaturation.textContent = `${sVal}%`;
      if (valTemperature) {
        if (tVal > 0) valTemperature.textContent = `+${tVal} Warm`;
        else if (tVal < 0) valTemperature.textContent = `${tVal} Cool`;
        else valTemperature.textContent = "Neutral";
      }

      if (this.bmo3D) {
        this.bmo3D.setStudioAdjustments({
          brightness: bVal / 100,
          saturation: sVal / 100,
          temperature: tVal / 100,
        });
      }

      // Also apply brightness & saturation to 2D view if active
      const chassis2D = document.getElementById("gameboy-chassis");
      if (chassis2D) {
        chassis2D.style.filter = `brightness(${bVal / 100}) saturate(${sVal / 100})`;
      }
    };

    if (sliderLighting) sliderLighting.addEventListener("input", updateStudio);
    if (sliderSaturation) sliderSaturation.addEventListener("input", updateStudio);
    if (sliderTemperature) sliderTemperature.addEventListener("input", updateStudio);

      if (btnResetStudio) {
        btnResetStudio.addEventListener("click", () => {
          if (sliderLighting) sliderLighting.value = 100;
          if (sliderSaturation) sliderSaturation.value = 100;
          if (sliderTemperature) sliderTemperature.value = 0;
          updateStudio();
        });
      }

      // Initial studio adjustment apply
      updateStudio();
    // Palette selector
    const paletteSelect = document.getElementById("palette-select");
    if (paletteSelect) {
      paletteSelect.addEventListener("change", (e) => {
        this.activePalette = e.target.value;
      });
    }

    // Filter toggles
    const filterSelect = document.getElementById("filter-select");
    if (filterSelect) {
      filterSelect.addEventListener("change", (e) => {
        document.body.classList.remove("body-crt", "body-lcd");
        if (e.target.value === "crt") document.body.classList.add("body-crt");
        if (e.target.value === "lcd") document.body.classList.add("body-lcd");
      });
    }

    // State Switcher buttons
    document.querySelectorAll(".state-nav-btn").forEach(btn => {
      btn.addEventListener("click", () => {
        const stateName = btn.getAttribute("data-state");
        if (STATE[stateName] !== undefined) {
          this.currentState = STATE[stateName];
        }
      });
    });

    // Playtest Controls (Wasm & Host C)
    const btnSave = document.getElementById("btn-wasm-save-state");
    if (btnSave) {
      btnSave.addEventListener("click", () => {
        if (this.wasmEngine) {
          this.wasmEngine.saveState(0);
          const log = document.getElementById("playtest-engine-log");
          if (log) log.textContent = `State saved to Slot 0 for ${this.wasmEngine.activeTitle}`;
        }
      });
    }

    const btnLoad = document.getElementById("btn-wasm-load-state");
    if (btnLoad) {
      btnLoad.addEventListener("click", () => {
        if (this.wasmEngine) {
          const ok = this.wasmEngine.loadState(0);
          const log = document.getElementById("playtest-engine-log");
          if (log) log.textContent = ok ? `State loaded from Slot 0 (Frame ${this.wasmEngine.frameCount})` : "No saved state in Slot 0";
        }
      });
    }

    const btnBezel = document.getElementById("btn-wasm-toggle-bezel");
    if (btnBezel) {
      btnBezel.addEventListener("click", () => {
        if (this.wasmEngine) {
          this.wasmEngine.bezelEnabled = !this.wasmEngine.bezelEnabled;
          const log = document.getElementById("playtest-engine-log");
          if (log) log.textContent = `1:1 Console Bezel: ${this.wasmEngine.bezelEnabled ? "ON" : "OFF"}`;
        }
      });
    }

    const btnSwap = document.getElementById("btn-wasm-toggle-ab");
    if (btnSwap) {
      btnSwap.addEventListener("click", () => {
        if (this.wasmEngine) {
          this.wasmEngine.swapAB = !this.wasmEngine.swapAB;
          const log = document.getElementById("playtest-engine-log");
          if (log) log.textContent = `Gamepad A/B Swap: ${this.wasmEngine.swapAB ? "ON" : "OFF"}`;
        }
      });
    }

    // Host runner button removed: local simulator operates statically in browser WASM.

    this.setupBmoModeControls();
    this.setupInternalsControls();
    this.updateSidebarMuseumView();
  }

  setupInternalsControls() {
    const bindClick = (id, handler) => {
      const el = document.getElementById(id);
      if (el) el.addEventListener("click", handler);
      return el;
    };

    bindClick("btn-internals-mode", () => {
      if (this.internalsModeActive) {
        this.applyInternalsPresetUI("assembled");
        if (this.bmo3D) this.bmo3D.setCameraPreset("front");
      } else {
        if (!this.is3DView) {
          this.is3DView = true;
          this.updateViewMode();
        }
        this.applyInternalsPresetUI("core");
        if (this.bmo3D) this.bmo3D.playDisassemblySequence(true);
        const panel = document.getElementById("internals-panel");
        if (panel && typeof panel.scrollIntoView === "function") {
          panel.scrollIntoView({ behavior: "smooth", block: "nearest" });
        }
      }
    });

    bindClick("btn-internals-play", () => {
      if (!this.is3DView) {
        this.is3DView = true;
        this.updateViewMode();
      }
      this.internalsModeActive = true;
      this.syncInternalsBadge();
      if (this.bmo3D) {
        this.bmo3D.setLayerVisible("shells", true);
        this.bmo3D.setLayerVisible("buttons", true);
        this.syncLayerCheckboxes();
        this.bmo3D.playDisassemblySequence(true);
      }
    });

    bindClick("btn-internals-assemble", () => {
      this.applyInternalsPresetUI("assembled");
      if (this.bmo3D) this.bmo3D.setCameraPreset("iso");
    });

    bindClick("btn-internals-allwires", () => {
      this.setInternalsHighlightUI("all");
    });

    const sliderExplode = document.getElementById("slider-explode");
    if (sliderExplode) {
      sliderExplode.addEventListener("input", () => {
        const pct = parseInt(sliderExplode.value, 10) || 0;
        if (!this.is3DView) {
          this.is3DView = true;
          this.updateViewMode();
        }
        this.internalsModeActive = pct > 2;
        if (this.bmo3D) {
          this.bmo3D.setExplode(pct / 100);
        }
        this.syncExplosionReadout(pct / 100);
        this.syncInternalsBadge();
      });
    }

    document.querySelectorAll(".internals-preset-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        const preset = btn.getAttribute("data-preset") || "assembled";
        if (!this.is3DView) {
          this.is3DView = true;
          this.updateViewMode();
        }
        this.applyInternalsPresetUI(preset);
      });
    });

    document.querySelectorAll(".internals-layer-chk").forEach((chk) => {
      chk.addEventListener("change", () => {
        const layer = chk.getAttribute("data-layer");
        if (this.bmo3D && layer) {
          this.bmo3D.setLayerVisible(layer, chk.checked);
          if (layer === "expansion") {
            const warnCard = document.getElementById("internals-expansion-warning");
            const warnStatus = document.getElementById("internals-expansion-status");
            if (warnCard && warnStatus) {
              warnCard.dataset.state = chk.checked ? "conflict" : "inactive";
              warnStatus.textContent = chk.checked
                ? "-9.3 mm Z clash w/ 3xAA bay (use <=6 mm LiPo)"
                : "Disabled in config.h (Stage 1 USB)";
            }
            if (chk.checked && this.bmo3D.explodeCurrent < 0.25) {
              this.applyInternalsPresetUI("core");
            }
          }
        }
      });
    });

    document.querySelectorAll(".internals-filter-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        const hl = btn.getAttribute("data-highlight") || "all";
        this.setInternalsHighlightUI(hl);
      });
    });

    document.querySelectorAll(".internals-step-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        const idx = parseInt(btn.getAttribute("data-step"), 10) || 0;
        this.selectBeginnerGuideStep(idx, true);
      });
    });

    bindClick("btn-internals-next-step", () => {
      const steps = (window.BmoInternals3D && window.BmoInternals3D.BUILD_GUIDE_STEPS) || [];
      const count = steps.length || 7;
      this.selectBeginnerGuideStep((this.internalsStepIndex + 1) % count, true);
    });

    this.selectBeginnerGuideStep(0, false);
    this.syncInternalsBadge();
  }

  applyInternalsPresetUI(presetName) {
    this.internalsPreset = presetName;
    this.internalsModeActive = presetName !== "assembled";
    document.querySelectorAll(".internals-preset-btn").forEach((b) => {
      b.classList.toggle("active", b.getAttribute("data-preset") === presetName);
    });
    if (this.bmo3D) {
      this.bmo3D.applyInternalsPreset(presetName);
      this.syncExplosionReadout(this.bmo3D.explodeTarget);
      this.syncLayerCheckboxes();
    }
    this.syncInternalsBadge();
  }

  setInternalsHighlightUI(tag) {
    this.internalsHighlight = tag || "all";
    document.querySelectorAll(".internals-filter-btn").forEach((b) => {
      b.classList.toggle("active", b.getAttribute("data-highlight") === this.internalsHighlight);
    });
    if (this.bmo3D) {
      this.bmo3D.setInternalHighlight(this.internalsHighlight);
    }
  }

  selectBeginnerGuideStep(stepIdx, applyTo3D = true) {
    const steps = (window.BmoInternals3D && window.BmoInternals3D.BUILD_GUIDE_STEPS) || [];
    if (!steps.length) return;
    const clamped = ((stepIdx % steps.length) + steps.length) % steps.length;
    this.internalsStepIndex = clamped;
    const step = steps[clamped];

    document.querySelectorAll(".internals-step-btn").forEach((b) => {
      b.classList.toggle("active", parseInt(b.getAttribute("data-step"), 10) === clamped);
    });
    const heading = document.getElementById("internals-guide-heading");
    const body = document.getElementById("internals-guide-body");
    const tip = document.getElementById("internals-guide-tip");
    if (heading) heading.textContent = step.title;
    if (body) body.textContent = step.summary;
    if (tip) tip.textContent = `Tip: ${step.tip}`;

    if (applyTo3D) {
      if (!this.is3DView) {
        this.is3DView = true;
        this.updateViewMode();
      }
      this.applyInternalsPresetUI(step.preset);
      this.setInternalsHighlightUI(step.highlight);
      if (step.id === "step_7") {
        const expChk = document.getElementById("chk-internals-expansion");
        if (expChk && !expChk.checked) {
          expChk.checked = true;
          expChk.dispatchEvent(new Event("change"));
        }
      }
    }
  }

  handleInternalPartSelected(meta) {
    const inspector = document.getElementById("internals-inspector");
    const metaEl = document.getElementById("internals-inspector-meta");
    if (!inspector || !metaEl || !meta) return;
    const titleEl = inspector.querySelector(".internals-inspector-title");
    if (titleEl) titleEl.textContent = `${meta.name} (${meta.role || "Component"})`;
    metaEl.textContent = meta.detail || "Inspecting selected assembly.";
  }

  syncExplosionReadout(t) {
    const pct = Math.round(Math.max(0, Math.min(1, Number(t) || 0)) * 100);
    const valEl = document.getElementById("val-explode");
    const sliderEl = document.getElementById("slider-explode");
    if (valEl) valEl.textContent = `${pct}%`;
    if (sliderEl && Math.abs(parseInt(sliderEl.value, 10) - pct) >= 1) {
      sliderEl.value = String(pct);
    }
  }

  syncLayerCheckboxes() {
    if (!this.bmo3D) return;
    const internalsVis = this.bmo3D.internals ? this.bmo3D.internals.layerVisibility : {};
    document.querySelectorAll(".internals-layer-chk").forEach((chk) => {
      const layer = chk.getAttribute("data-layer");
      if (!layer) return;
      if (layer in this.bmo3D.layerVis) chk.checked = Boolean(this.bmo3D.layerVis[layer]);
      else if (layer in internalsVis) chk.checked = Boolean(internalsVis[layer]);
    });
  }

  syncInternalsBadge() {
    const toolbarBtn = document.getElementById("btn-internals-mode");
    const panel = document.getElementById("internals-panel");
    const badge = document.getElementById("internals-status-badge");
    if (toolbarBtn) {
      toolbarBtn.classList.toggle("active", this.internalsModeActive);
      toolbarBtn.setAttribute("aria-pressed", String(this.internalsModeActive));
    }
    if (panel) panel.classList.toggle("is-active", this.internalsModeActive);
    if (badge) {
      const labels = {
        assembled: "Assembled",
        unscrew: "Unscrewing M2",
        display: "Display Stack",
        core: "Core PCB Open",
        wiring: "Harness View",
        exploded: "Full Stack",
      };
      badge.textContent = labels[this.internalsPreset] || (this.internalsModeActive ? "Open" : "Assembled");
    }
  }

  setupBmoModeControls() {
    const bindClick = (id, handler) => {
      const el = document.getElementById(id);
      if (el) el.addEventListener("click", handler);
      return el;
    };

    bindClick("btn-bmo-mode", () => {
      if (this.currentState === STATE.IDLE_MASCOT) this.exitBmoCompanion();
      else this.enterBmoMode();
    });
    bindClick("btn-bmo-enter", () => this.enterBmoMode());
    bindClick("btn-bmo-exit", () => this.exitBmoCompanion());
    bindClick("btn-bmo-reset", () => this.resetBmoNeutral());

    document.querySelectorAll(".bmo-expr-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        const expr = btn.getAttribute("data-bmo-expr");
        if (expr) this.runBmoAction((engine) => engine.setBmoExpression(expr), true);
      });
    });

    bindClick("btn-bmo-pet", () => this.runBmoAction((engine) => engine.petBmoCompanion()));
    bindClick("btn-bmo-tickle", () => this.runBmoAction((engine) => engine.tickleBmo(1.0)));
    bindClick("btn-bmo-wink", () => this.runBmoAction((engine) => engine.triggerBmoWink()));
    bindClick("btn-bmo-blush", () => this.runBmoAction((engine) => engine.triggerBmoBlush()));
    bindClick("btn-bmo-joy", () => this.runBmoAction((engine) => engine.triggerBmoJoy()));
    bindClick("btn-bmo-dance", () => this.runBmoAction((engine) => {
      if (engine.bmoIsDancing()) engine.stopBmoDance();
      else engine.startBmoDance(this.bmoDanceBpm);
    }));

    const bpmSlider = document.getElementById("slider-bmo-bpm");
    const bpmVal = document.getElementById("val-bmo-bpm");
    if (bpmSlider) {
      bpmSlider.addEventListener("input", () => {
        this.bmoDanceBpm = parseInt(bpmSlider.value, 10) || 125;
        if (bpmVal) bpmVal.textContent = `${this.bmoDanceBpm} BPM`;
        if (this.wasmEngine && this.wasmEngine.bmoReady && this.wasmEngine.bmoIsDancing()) {
          this.wasmEngine.startBmoDance(this.bmoDanceBpm);
        }
        this.syncBmoModeUI();
      });
    }

    const chkAutoCycle = document.getElementById("chk-bmo-autocycle");
    if (chkAutoCycle) {
      chkAutoCycle.addEventListener("change", () => {
        this.bmoAutoCycle = chkAutoCycle.checked;
        if (this.bmoAutoCycle) {
          this.ensureBmoModeActive();
          this.bmoLastAutoCycleMs = performance.now();
          const names = (window.BMO_EXPRESSION_NAMES || []).filter((n) => n !== "HIDDEN");
          if (names.length) {
            const nextExpr = names[this.bmoAutoCycleIndex % names.length];
            this.runBmoAction((engine) => engine.setBmoExpression(nextExpr), false);
            this.bmoAutoCycleIndex = (this.bmoAutoCycleIndex + 1) % names.length;
          }
        }
        this.syncBmoModeUI();
      });
    }

    const chkAutoSleep = document.getElementById("chk-bmo-autosleep");
    if (chkAutoSleep) {
      chkAutoSleep.checked = this.bmoAutoSleep;
      chkAutoSleep.addEventListener("change", () => {
        this.bmoAutoSleep = chkAutoSleep.checked;
        this.bmoLastInputMs = performance.now();
      });
    }

    const chkHud = document.getElementById("chk-bmo-hud");
    if (chkHud) {
      chkHud.checked = this.bmoHudVisible;
      chkHud.addEventListener("change", () => {
        this.bmoHudVisible = chkHud.checked;
      });
    }

    const gazePad = document.getElementById("bmo-gaze-pad");
    if (gazePad) {
      let draggingGaze = false;
      const updateFromPadPointer = (e) => {
        const rect = gazePad.getBoundingClientRect();
        if (!rect.width || !rect.height) return;
        const gx = ((e.clientX - rect.left) / rect.width) * 2 - 1;
        const gy = 1 - ((e.clientY - rect.top) / rect.height) * 2;
        this.setBmoModeGaze(gx, gy);
      };
      gazePad.addEventListener("pointerdown", (e) => {
        draggingGaze = true;
        if (typeof gazePad.setPointerCapture === "function") {
          try { gazePad.setPointerCapture(e.pointerId); } catch (_) { /* ignore synthetic events */ }
        }
        updateFromPadPointer(e);
      });
      gazePad.addEventListener("pointermove", (e) => {
        if (draggingGaze) updateFromPadPointer(e);
      });
      window.addEventListener("pointerup", () => {
        draggingGaze = false;
      });
    }

    document.querySelectorAll(".bmo-gaze-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        const pair = (btn.getAttribute("data-gaze") || "0,0").split(",");
        this.setBmoModeGaze(parseFloat(pair[0]), parseFloat(pair[1]));
      });
    });

    const quoteSelect = document.getElementById("select-bmo-quote");
    bindClick("btn-bmo-say-quote", () => {
      const idx = quoteSelect ? parseInt(quoteSelect.value, 10) || 0 : 0;
      this.runBmoAction((engine) => engine.sayBmoQuote(idx, 3800));
    });
    bindClick("btn-bmo-random-quote", () => {
      const count = quoteSelect ? quoteSelect.options.length : 10;
      const idx = Math.floor(Math.random() * Math.max(1, count));
      if (quoteSelect && idx < quoteSelect.options.length) quoteSelect.value = String(idx);
      this.runBmoAction((engine) => engine.sayBmoQuote(idx, 3800));
    });

    const customInput = document.getElementById("input-bmo-speech");
    const submitCustomSpeech = () => {
      if (!customInput) return;
      const text = customInput.value.trim();
      if (!text) return;
      this.runBmoAction((engine) => engine.sayBmoText(text, 4000));
    };
    bindClick("btn-bmo-say-custom", submitCustomSpeech);
    if (customInput) {
      customInput.addEventListener("keydown", (e) => {
        if (e.key === "Enter") {
          e.preventDefault();
          submitCustomSpeech();
        }
      });
    }
    bindClick("btn-bmo-clear-speech", () => {
      this.runBmoAction((engine) => engine.clearBmoSpeech());
    });
    this.populateBmoQuotes();
    this.syncBmoModeUI();
  }

  handleExternalButtonPress(key) {
    const keyBitMap = {
      "UP": 1, "DOWN": 2, "LEFT": 4, "RIGHT": 8,
      "A": 16, "B": 32, "START": 64, "SELECT": 128,
      "X": 256, "Y": 512, "L": 1024, "R": 2048
    };
    if (this.wasmEngine && keyBitMap[key]) {
      this.wasmEngine.setButtonBit(keyBitMap[key], true);
    }
    this.handleButtonPress(key);
  }

  handleExternalButtonRelease(key) {
    const keyBitMap = {
      "UP": 1, "DOWN": 2, "LEFT": 4, "RIGHT": 8,
      "A": 16, "B": 32, "START": 64, "SELECT": 128,
      "X": 256, "Y": 512, "L": 1024, "R": 2048
    };
    if (this.wasmEngine && keyBitMap[key]) {
      this.wasmEngine.setButtonBit(keyBitMap[key], false);
    }
    this.handleButtonRelease(key);
  }

  updateViewMode() {
    const stage = document.getElementById("stage-container");
    const toggleBtn = document.getElementById("btn-toggle-viewmode");
    if (!stage) return;
    if (this.is3DView) {
      stage.classList.remove("view-2d");
      stage.classList.add("view-3d");
      if (toggleBtn) toggleBtn.textContent = "2D Flat View";
      if (this.bmo3D) this.bmo3D.onResize();
    } else {
      stage.classList.remove("view-3d");
      stage.classList.add("view-2d");
      if (toggleBtn) toggleBtn.textContent = "3D Console View";
    }
  }

  switchTo2DView() {
    this.is3DView = false;
    this.updateViewMode();
  }

  updateSidebarMuseumView() {
    const currentConsole = CONSOLES_LIST[this.selectedConsoleIndex];
    const data = CONSOLE_HISTORY_DATABASE[currentConsole.id];
    if (!data) return;

    const titleEl = document.getElementById("museum-sidebar-title");
    const taglineEl = document.getElementById("museum-sidebar-tagline");
    const cpuEl = document.getElementById("museum-sidebar-cpu");
    const ramEl = document.getElementById("museum-sidebar-ram");
    const resEl = document.getElementById("museum-sidebar-res");
    const historyEl = document.getElementById("museum-sidebar-history");
    const gamesContainer = document.getElementById("museum-sidebar-games");

    if (titleEl) titleEl.innerText = `${data.name} (${data.year})`;
    if (taglineEl) taglineEl.innerText = data.tagline;
    if (cpuEl) cpuEl.innerText = data.specs.cpu || "N/A";
    if (ramEl) ramEl.innerText = data.specs.ram || "N/A";
    if (resEl) resEl.innerText = data.specs.resolution || "N/A";
    if (historyEl) historyEl.innerText = data.historicalSignificance;

    if (gamesContainer && data.hallmarkGames) {
      gamesContainer.replaceChildren(
        ...data.hallmarkGames.map(g => {
          const item = document.createElement("div");
          item.className = "hallmark-game-item";
          const title = document.createElement("div");
          title.className = "hallmark-title";
          title.textContent = `${g.title} (${g.year})`;
          const desc = document.createElement("div");
          desc.className = "hallmark-desc";
          desc.textContent = String(g.significance ?? "");
          item.append(title, desc);
          return item;
        })
      );
    }
  }

  // =========================================================================
  // RENDERING PIPELINE (320x240 Native ST7789 Resolution)
  // =========================================================================
  renderLoop(timestamp) {
    const dt = (timestamp - this.lastFrameTime) / 1000;
    this.lastFrameTime = timestamp;
    this.fps = Math.round(1 / (dt || 0.016));

    if (this.currentState === STATE.CONSOLE_SELECT
        && timestamp - this.lastInputActivityMs > 30000) {
      this.enterBmoCompanion(false);
    }

    // Update BMO Blink & Animation timers
    this.updateBmoAnimation(dt);

    // Clear Screen to Dark Navy/Black
    this.ctx.fillStyle = "#0a0e17";
    this.ctx.fillRect(0, 0, 320, 240);

    // State Machine Dispatch
    switch (this.currentState) {
      case STATE.BOOT_SPLASH:
        this.renderBootSplash(dt);
        break;
      case STATE.CONSOLE_SELECT:
        this.renderConsoleSelectMenu();
        break;
      case STATE.GAME_SELECT:
        this.renderGameSelectMenu();
        break;
      case STATE.CONSOLE_MUSEUM:
        this.renderConsoleMuseumModal();
        break;
      case STATE.EMULATION_VIEW:
        this.renderEmulationView();
        break;
      case STATE.IDLE_MASCOT:
        this.renderIdleMascotScreen(timestamp);
        break;
    }

    // Telemetry display update
    const fpsVal = document.getElementById("telemetry-fps");
    if (fpsVal) fpsVal.innerText = `${this.fps} FPS`;

    // Emulator telemetry update
    if (this.wasmEngine) {
      const crcEl = document.getElementById("playtest-crc-val");
      const coreEl = document.getElementById("playtest-core-val");
      const statusEl = document.getElementById("wasm-engine-status");
      if (this.wasmEngine.runtimeError && !this.runtimeErrorShown) {
        this.runtimeErrorShown = true;
        this.showSimulatorMessage(this.wasmEngine.runtimeError, true);
      }
      if (crcEl) crcEl.textContent = "0x" + (this.wasmEngine.romChecksum >>> 0).toString(16).toUpperCase();
      if (coreEl && this.wasmEngine.viewport) {
        coreEl.textContent = `${this.wasmEngine.activeConsoleId.replace("ROM_", "")} (${this.wasmEngine.viewport.w}×${this.wasmEngine.viewport.h})`;
      }
      if (statusEl) {
        statusEl.textContent = this.engineLoadFailed
          ? "Error"
          : this.wasmEngine.runtimeError
            ? "Core error"
          : this.wasmEngine.loaded
            ? `Frame ${this.wasmEngine.frameCount}`
            : this.wasmEngine.ready
              ? "Core ready"
              : "Loading";
      }
    }

    requestAnimationFrame(this._boundRenderLoop);
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

  // 2D SDF BMO Face Mascot
  drawBmoFace(x, y, scale = 1.0) {
    this.ctx.save();
    this.ctx.translate(x, y);

    // Face Background Card (Teal rounded rectangle)
    const w = 48 * scale;
    const h = 32 * scale;
    this.ctx.fillStyle = "#55efc4";
    this.ctx.beginPath();
    this.ctx.roundRect(0, 0, w, h, 6 * scale);
    this.ctx.fill();

    // Eyes
    const eyeH = this.bmo.isBlinking ? Math.max(1, 4 * scale * (1 - this.bmo.blinkState)) : 5 * scale;
    const eyeW = 3 * scale;
    this.ctx.fillStyle = "#2d3436";

    // Left eye
    this.ctx.beginPath();
    this.ctx.ellipse(14 * scale, 14 * scale, eyeW, eyeH, 0, 0, Math.PI * 2);
    this.ctx.fill();

    // Right eye
    this.ctx.beginPath();
    this.ctx.ellipse(34 * scale, 14 * scale, eyeW, eyeH, 0, 0, Math.PI * 2);
    this.ctx.fill();

    // Mouth (Procedural Arc / Line)
    this.ctx.strokeStyle = "#2d3436";
    this.ctx.lineWidth = 2 * scale;
    this.ctx.beginPath();
    if (this.bmo.expression === "HAPPY") {
      this.ctx.arc(24 * scale, 18 * scale, 6 * scale, 0.2 * Math.PI, 0.8 * Math.PI, false);
    } else {
      this.ctx.arc(24 * scale, 19 * scale, 5 * scale, 0.1 * Math.PI, 0.9 * Math.PI, false);
    }
    this.ctx.stroke();

    // Rosy Cheeks
    this.ctx.fillStyle = "rgba(255, 118, 117, 0.6)";
    this.ctx.beginPath();
    this.ctx.arc(8 * scale, 17 * scale, 2.5 * scale, 0, Math.PI * 2);
    this.ctx.arc(40 * scale, 17 * scale, 2.5 * scale, 0, Math.PI * 2);
    this.ctx.fill();

    this.ctx.restore();
  }

  renderBootSplash(dt) {
    this.bootTimer += dt;

    // Large Centered BMO Face
    this.drawBmoFace(100, 42, 2.5);

    // Title
    this.ctx.fillStyle = "#ffffff";
    this.ctx.font = "bold 16px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText("BMO HANDHELD", 160, 156);

    this.ctx.fillStyle = "#cef5e4";
    this.ctx.font = "11px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("14 Emulation Cores • Thousands of Adventures", 160, 182);

    // Press A Prompt
    if (Math.floor(this.bootTimer * 2) % 2 === 0) {
      this.ctx.fillStyle = "#ffe033";
      this.ctx.font = "bold 11px 'Press Start 2P', monospace";
      this.ctx.fillText("PRESS ANY BUTTON", 160, 218);
    }
  }

  // Pixel-Art & Vector Illustration for Active Console
  drawConsolePixelArt(c, x, y, w, h) {
    this.ctx.save();
    this.ctx.translate(x, y);

    // Subtle dark backdrop pill
    this.ctx.fillStyle = "rgba(10, 24, 30, 0.6)";
    this.ctx.beginPath();
    this.ctx.roundRect(0, 0, w, h, 8);
    this.ctx.fill();
    this.ctx.strokeStyle = "rgba(95, 180, 156, 0.3)";
    this.ctx.lineWidth = 1;
    this.ctx.stroke();

    const id = c.id;
    if (id === "ROM_FAVORITES") {
      // Cheerful smiling golden star with BMO eyes & cheeks
      this.ctx.fillStyle = "#FFE033";
      this.ctx.beginPath();
      const cx = 29, cy = 36, spikes = 5, outerRadius = 24, innerRadius = 11;
      let rot = Math.PI / 2 * 3;
      let step = Math.PI / spikes;
      this.ctx.moveTo(cx, cy - outerRadius);
      for (let i = 0; i < spikes; i++) {
        let sx = cx + Math.cos(rot) * outerRadius;
        let sy = cy + Math.sin(rot) * outerRadius;
        this.ctx.lineTo(sx, sy);
        rot += step;
        sx = cx + Math.cos(rot) * innerRadius;
        sy = cy + Math.sin(rot) * innerRadius;
        this.ctx.lineTo(sx, sy);
        rot += step;
      }
      this.ctx.lineTo(cx, cy - outerRadius);
      this.ctx.closePath();
      this.ctx.fill();

      // Star BMO Eyes
      this.ctx.fillStyle = "#0F2620";
      this.ctx.beginPath();
      this.ctx.arc(23, 34, 2, 0, Math.PI * 2);
      this.ctx.arc(35, 34, 2, 0, Math.PI * 2);
      this.ctx.fill();

      // Star Smile
      this.ctx.strokeStyle = "#0F2620";
      this.ctx.lineWidth = 1.5;
      this.ctx.beginPath();
      this.ctx.arc(29, 36, 3.5, 0.1 * Math.PI, 0.9 * Math.PI, false);
      this.ctx.stroke();

      // Rosy Cheeks
      this.ctx.fillStyle = "#F48FB1";
      this.ctx.beginPath();
      this.ctx.arc(20, 37, 1.8, 0, Math.PI * 2);
      this.ctx.arc(38, 37, 1.8, 0, Math.PI * 2);
      this.ctx.fill();

    } else if (id === "ROM_GB" || id === "ROM_GBC") {
      // Classic Vertical Handheld Game Boy
      const bodyColor = (id === "ROM_GB") ? "#5FB49C" : "#9C88FF";
      this.ctx.fillStyle = bodyColor;
      this.ctx.beginPath();
      this.ctx.roundRect(11, 6, 36, 62, [6, 6, 12, 6]);
      this.ctx.fill();

      // Bezel
      this.ctx.fillStyle = "#1A4B42";
      this.ctx.fillRect(15, 11, 28, 24);

      // Screen
      this.ctx.fillStyle = (id === "ROM_GB") ? "#CEF5E4" : "#D4F7EA";
      this.ctx.fillRect(19, 14, 20, 18);

      // Mini Face on Screen!
      this.ctx.fillStyle = "#0F2620";
      this.ctx.fillRect(23, 20, 2, 3);
      this.ctx.fillRect(33, 20, 2, 3);
      this.ctx.fillRect(26, 26, 6, 2);

      // D-Pad
      this.ctx.fillStyle = "#FFE033";
      this.ctx.fillRect(16, 42, 10, 3);
      this.ctx.fillRect(19, 39, 4, 9);

      // A / B Buttons
      this.ctx.fillStyle = "#FF91A5";
      this.ctx.beginPath();
      this.ctx.arc(38, 43, 2.5, 0, Math.PI * 2);
      this.ctx.arc(32, 47, 2.5, 0, Math.PI * 2);
      this.ctx.fill();

      // Slanted Speaker Grille
      this.ctx.fillStyle = "rgba(15, 38, 32, 0.4)";
      this.ctx.fillRect(36, 57, 6, 1);
      this.ctx.fillRect(38, 60, 5, 1);

    } else if (id === "ROM_NES") {
      // Classic NES Controller Rectangle
      this.ctx.fillStyle = "#C8D6E5";
      this.ctx.beginPath();
      this.ctx.roundRect(8, 20, 42, 28, 3);
      this.ctx.fill();

      // Black Center Stripe
      this.ctx.fillStyle = "#1E272E";
      this.ctx.fillRect(11, 23, 36, 22);

      // D-Pad
      this.ctx.fillStyle = "#C8D6E5";
      this.ctx.fillRect(14, 30, 8, 3);
      this.ctx.fillRect(17, 27, 3, 9);

      // Red A & B buttons
      this.ctx.fillStyle = "#FF4757";
      this.ctx.beginPath();
      this.ctx.arc(36, 34, 2.5, 0, Math.PI * 2);
      this.ctx.arc(42, 34, 2.5, 0, Math.PI * 2);
      this.ctx.fill();

      // Red Stripe Accent
      this.ctx.fillStyle = "#FF4757";
      this.ctx.fillRect(24, 39, 8, 2);

    } else if (id === "ROM_SNES") {
      // Super Nintendo Rounded Dogbone Controller
      this.ctx.fillStyle = "#D2D7DE";
      this.ctx.beginPath();
      this.ctx.roundRect(6, 20, 46, 26, 13);
      this.ctx.fill();

      // Inner recess
      this.ctx.fillStyle = "#A4B0BE";
      this.ctx.beginPath();
      this.ctx.roundRect(10, 23, 38, 20, 10);
      this.ctx.fill();

      // D-Pad
      this.ctx.fillStyle = "#2F3542";
      this.ctx.fillRect(13, 30, 8, 3);
      this.ctx.fillRect(16, 27, 3, 9);

      // 4 Diamond Color Buttons (Yellow, Green, Blue, Red)
      this.ctx.fillStyle = "#FFE033";
      this.ctx.beginPath(); this.ctx.arc(38, 27, 2, 0, Math.PI * 2); this.ctx.fill();
      this.ctx.fillStyle = "#2ED573";
      this.ctx.beginPath(); this.ctx.arc(34, 31, 2, 0, Math.PI * 2); this.ctx.fill();
      this.ctx.fillStyle = "#3742FA";
      this.ctx.beginPath(); this.ctx.arc(42, 31, 2, 0, Math.PI * 2); this.ctx.fill();
      this.ctx.fillStyle = "#FF4757";
      this.ctx.beginPath(); this.ctx.arc(38, 35, 2, 0, Math.PI * 2); this.ctx.fill();

    } else if (id === "ROM_GENESIS") {
      // Sega Genesis Arc Controller
      this.ctx.fillStyle = "#1E272E";
      this.ctx.beginPath();
      this.ctx.roundRect(7, 20, 44, 26, [14, 14, 8, 8]);
      this.ctx.fill();

      // Red Start Button
      this.ctx.fillStyle = "#FF4757";
      this.ctx.fillRect(25, 24, 7, 2);

      // D-Pad Disc
      this.ctx.fillStyle = "#57606F";
      this.ctx.beginPath();
      this.ctx.arc(17, 33, 6, 0, Math.PI * 2);
      this.ctx.fill();

      // 3 Action Buttons (A, B, C)
      this.ctx.fillStyle = "#747D8C";
      this.ctx.beginPath();
      this.ctx.arc(33, 36, 2.2, 0, Math.PI * 2);
      this.ctx.arc(38, 33, 2.2, 0, Math.PI * 2);
      this.ctx.arc(43, 30, 2.2, 0, Math.PI * 2);
      this.ctx.fill();

    } else if (id === "ROM_GG" || id === "ROM_LYNX") {
      // Landscape Handheld Console
      this.ctx.fillStyle = "#2F3542";
      this.ctx.beginPath();
      this.ctx.roundRect(6, 18, 46, 32, 8);
      this.ctx.fill();

      // Screen
      this.ctx.fillStyle = "#50C8D2";
      this.ctx.fillRect(17, 23, 24, 18);

      // Buttons
      this.ctx.fillStyle = "#FFE033";
      this.ctx.fillRect(10, 28, 4, 4);
      this.ctx.fillStyle = "#FF4757";
      this.ctx.beginPath();
      this.ctx.arc(45, 30, 2, 0, Math.PI * 2);
      this.ctx.arc(45, 35, 2, 0, Math.PI * 2);
      this.ctx.fill();

    } else if (id === "ROM_ATARI") {
      // Atari 2600 Joystick
      this.ctx.fillStyle = "#2F3542";
      this.ctx.beginPath();
      this.ctx.roundRect(14, 30, 30, 26, 4);
      this.ctx.fill();

      // Orange ring
      this.ctx.strokeStyle = "#FF7F50";
      this.ctx.lineWidth = 1.5;
      this.ctx.beginPath();
      this.ctx.arc(29, 43, 8, 0, Math.PI * 2);
      this.ctx.stroke();

      // Red fire button
      this.ctx.fillStyle = "#FF4757";
      this.ctx.beginPath();
      this.ctx.arc(19, 36, 3, 0, Math.PI * 2);
      this.ctx.fill();

      // Stick shaft & top
      this.ctx.fillStyle = "#1E272E";
      this.ctx.fillRect(27, 14, 4, 20);
      this.ctx.beginPath();
      this.ctx.arc(29, 14, 5, 0, Math.PI * 2);
      this.ctx.fill();

    } else if (id === "ROM_PICO8") {
      // PICO-8 Fantasy Cartridge
      this.ctx.fillStyle = "#303A52";
      this.ctx.beginPath();
      this.ctx.roundRect(12, 14, 34, 44, 4);
      this.ctx.fill();

      // Cart Label
      this.ctx.fillStyle = "#FFE033";
      this.ctx.fillRect(16, 20, 26, 26);

      // Rainbow bar
      const colors = ["#FF004D", "#FFA300", "#FFEC27", "#00E436", "#29ADFF"];
      for (let ci = 0; ci < colors.length; ci++) {
        this.ctx.fillStyle = colors[ci];
        this.ctx.fillRect(16 + ci * 5.2, 42, 5.2, 4);
      }

      // Mini PICO-8 Logo
      this.ctx.fillStyle = "#1D2B53";
      this.ctx.beginPath();
      this.ctx.arc(29, 30, 4, 0, Math.PI * 2);
      this.ctx.fill();

    } else if (id === "ROM_WAD") {
      // DOOM Skull / Helmet Icon
      this.ctx.fillStyle = "#2F3542";
      this.ctx.beginPath();
      this.ctx.roundRect(14, 18, 30, 32, 6);
      this.ctx.fill();

      // Green Visor
      this.ctx.fillStyle = "#2ED573";
      this.ctx.fillRect(18, 26, 22, 7);

      // Blood red horn accents
      this.ctx.fillStyle = "#FF4757";
      this.ctx.beginPath();
      this.ctx.moveTo(14, 20); this.ctx.lineTo(10, 12); this.ctx.lineTo(18, 16);
      this.ctx.moveTo(44, 20); this.ctx.lineTo(48, 12); this.ctx.lineTo(40, 16);
      this.ctx.fill();

    } else {
      // Elegant Generic Console Cartridge / Device
      this.ctx.fillStyle = "#1E272E";
      this.ctx.beginPath();
      this.ctx.roundRect(10, 14, 38, 44, 6);
      this.ctx.fill();

      this.ctx.fillStyle = c.color;
      this.ctx.fillRect(14, 20, 30, 20);

      this.ctx.fillStyle = "#0B0F19";
      this.ctx.font = "bold 9px 'Press Start 2P', monospace";
      this.ctx.textAlign = "center";
      this.ctx.fillText(c.badge, 29, 34);

      this.ctx.fillStyle = "#A4B0BE";
      this.ctx.fillRect(16, 44, 26, 3);
      this.ctx.fillRect(16, 49, 18, 2);
    }

    this.ctx.restore();
  }

  renderConsoleSelectMenu() {
    // Header Bar
    this.ctx.fillStyle = "#1a4b42";
    this.ctx.fillRect(0, 0, 320, 42);

    // Mini BMO Face
    this.drawBmoFace(8, 5, 1.0);

    // Console Index / Counter
    this.ctx.fillStyle = "#cef5e4";
    this.ctx.font = "bold 11px 'JetBrains Mono', monospace";
    this.ctx.textAlign = "right";
    this.ctx.fillText(`SYSTEM ${this.selectedConsoleIndex + 1}/${CONSOLES_LIST.length}`, 310, 26);

    // Active Console Card
    const c = CONSOLES_LIST[this.selectedConsoleIndex];

    // Carousel Card Center
    const cardX = 35;
    const cardY = 56;
    const cardW = 250;
    const cardH = 145;

    this.ctx.fillStyle = "#1a4b42";
    this.ctx.beginPath();
    this.ctx.roundRect(cardX, cardY, cardW, cardH, 12);
    this.ctx.fill();

    this.ctx.strokeStyle = (c.id === "ROM_FAVORITES") ? "#ffe033" : c.color;
    this.ctx.lineWidth = 2;
    this.ctx.stroke();

    // Console Badge
    this.ctx.fillStyle = (c.id === "ROM_FAVORITES") ? "#ffe033" : c.color;
    this.ctx.beginPath();
    this.ctx.roundRect(cardX + 16, cardY + 16, 54, 24, 6);
    this.ctx.fill();

    this.ctx.fillStyle = "#0b0f19";
    this.ctx.font = "bold 11px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText(c.badge, cardX + 43, cardY + 33);

    // Year
    this.ctx.fillStyle = "#ffe033";
    this.ctx.font = "bold 12px 'JetBrains Mono', monospace";
    this.ctx.textAlign = "right";
    this.ctx.fillText(c.year, cardX + cardW - 20, cardY + 33);

    // Dynamic Pixel-Art Illustration Right Underneath The Date!
    this.drawConsolePixelArt(c, cardX + 175, cardY + 50, 58, 74);

    // Full Name
    this.ctx.fillStyle = "#ffffff";
    this.ctx.font = "bold 15px 'Outfit', sans-serif";
    this.ctx.textAlign = "left";
    this.ctx.fillText(c.name, cardX + 16, cardY + 74);

    // Extension & Format Tag
    this.ctx.fillStyle = "#8cd7c2";
    this.ctx.font = "11px 'JetBrains Mono', monospace";
    this.ctx.fillText(c.browserSupported ? `Browser core: ${c.ext}` : `Firmware core: ${c.ext}`, cardX + 16, cardY + 95);

    // Game Count Tag
    this.ctx.fillStyle = "#ffe033";
    this.ctx.font = "bold 13px 'JetBrains Mono', monospace";
    this.ctx.fillText(`${c.count.toLocaleString()} LOCAL ROMS`, cardX + 16, cardY + 124);

    // Carousel Navigation Arrows
    this.ctx.fillStyle = "#5fb49c";
    this.ctx.font = "bold 18px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("◀", 18, 134);
    this.ctx.fillText("▶", 302, 134);

    // Footer Instruction Bar
    this.ctx.fillStyle = "#1a4b42";
    this.ctx.fillRect(0, 214, 320, 26);

    this.ctx.fillStyle = "#cef5e4";
    this.ctx.font = "10px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("A: Browse Games  •  SELECT: History & Specs  •  ◄/►: Console", 160, 231);
  }

  renderConsoleMuseumModal() {
    const c = CONSOLES_LIST[this.selectedConsoleIndex];
    const data = CONSOLE_HISTORY_DATABASE[c.id];

    // Background overlay
    this.ctx.fillStyle = "rgba(10, 14, 23, 0.95)";
    this.ctx.fillRect(0, 0, 320, 240);

    // Museum Card Frame
    this.ctx.fillStyle = "#151d2a";
    this.ctx.beginPath();
    this.ctx.roundRect(10, 10, 300, 220, 10);
    this.ctx.fill();
    this.ctx.strokeStyle = c.color;
    this.ctx.lineWidth = 1.5;
    this.ctx.stroke();

    // Title
    this.ctx.fillStyle = "#ffffff";
    this.ctx.font = "bold 13px 'Outfit', sans-serif";
    this.ctx.textAlign = "left";
    this.ctx.fillText(`🏛️ ${c.name} (${c.year})`, 22, 32);

    this.ctx.fillStyle = c.color;
    this.ctx.font = "bold 9px 'Press Start 2P', monospace";
    this.ctx.textAlign = "right";
    this.ctx.fillText("MUSEUM", 295, 30);

    // Separator line
    this.ctx.strokeStyle = "rgba(255,255,255,0.1)";
    this.ctx.beginPath();
    this.ctx.moveTo(22, 42);
    this.ctx.lineTo(298, 42);
    this.ctx.stroke();

    if (data) {
      // Hardware Specs Box
      this.ctx.fillStyle = "#1e272e";
      this.ctx.beginPath();
      this.ctx.roundRect(22, 50, 276, 52, 6);
      this.ctx.fill();

      this.ctx.font = "10px 'JetBrains Mono', monospace";
      this.ctx.fillStyle = "#feca57";
      this.ctx.fillText(`CPU : ${data.specs.cpu ? data.specs.cpu.slice(0, 34) : 'Custom Architecture'}`, 28, 66);
      this.ctx.fillStyle = "#00d2d3";
      this.ctx.fillText(`RAM : ${data.specs.ram || '8 KB'} | Res: ${data.specs.resolution || '256x224'}`, 28, 80);
      this.ctx.fillStyle = "#ff9ff3";
      this.ctx.fillText(`Audio: ${data.specs.sound ? data.specs.sound.slice(0, 34) : 'Stereo PSG/FM'}`, 28, 94);

      // Historical Breakthrough Text
      this.ctx.fillStyle = "#dcdde1";
      this.ctx.font = "10px 'Outfit', sans-serif";
      const words = (data.historicalSignificance || "").split(" ");
      let line = "";
      let yPos = 118;
      for (let w of words) {
        if ((line + w).length > 46) {
          this.ctx.fillText(line, 22, yPos);
          line = w + " ";
          yPos += 14;
          if (yPos > 175) break;
        } else {
          line += w + " ";
        }
      }
      if (line && yPos <= 175) this.ctx.fillText(line, 22, yPos);

      // Hallmark Game Highlight
      if (data.hallmarkGames && data.hallmarkGames.length > 0) {
        const hg = data.hallmarkGames[0];
        this.ctx.fillStyle = "#feca57";
        this.ctx.font = "bold 10px 'Outfit', sans-serif";
        this.ctx.fillText(`★ Key Landmark: ${hg.title} (${hg.year})`, 22, 195);
      }
    }

    // Dismiss Prompt
    this.ctx.fillStyle = "#a4b0be";
    this.ctx.font = "9px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText("PRESS B TO RETURN", 160, 220);
  }

  wrapSpeechLines(text, maxWidth, maxLines = 2) {
    const words = String(text || "").split(/\s+/).filter(Boolean);
    if (words.length === 0) return [];
    const lines = [];
    let current = "";
    for (const word of words) {
      const candidate = current ? `${current} ${word}` : word;
      if (this.ctx.measureText(candidate).width <= maxWidth) {
        current = candidate;
      } else if (current) {
        lines.push(current);
        if (lines.length === maxLines) {
          current = "";
          break;
        }
        current = word;
      } else {
        lines.push(word);
        if (lines.length === maxLines) {
          current = "";
          break;
        }
      }
    }
    if (current && lines.length < maxLines) lines.push(current);
    return lines.map((line) => {
      let clipped = line;
      while (clipped.length > 3 && this.ctx.measureText(clipped).width > maxWidth) {
        clipped = clipped.slice(0, -3) + "..";
      }
      return clipped;
    });
  }

  renderIdleMascotScreen(now = performance.now()) {
    const engine = this.wasmEngine;
    if (!engine || !engine.bmoReady) {
      this.ctx.fillStyle = "#8AD5C3";
      this.ctx.fillRect(0, 0, 320, 240);
      this.ctx.fillStyle = "#0F2620";
      this.ctx.textAlign = "center";
      if (this.engineLoadFailed) {
        this.ctx.font = "bold 11px monospace";
        this.ctx.fillText("BMO WASM CORE FAILED TO LOAD", 160, 102);
        this.ctx.font = "9px monospace";
        const errLines = this.wrapSpeechLines(
          this.engineLoadError || "Run: python tools/bmo_simulator/run_simulator.py",
          280,
          3
        );
        errLines.forEach((line, idx) => this.ctx.fillText(line, 160, 122 + idx * 14));
      } else {
        this.ctx.font = "bold 10px monospace";
        this.ctx.fillText("LOADING FIRMWARE BMO...", 160, 120);
      }
      return;
    }

    if (!this.bmoSessionStarted) {
      if (this.bmoEntryGreeting) {
        engine.setBmoExpression("HAPPY");
        engine.sayBmoQuote(0, 3500);
      } else {
        engine.setBmoExpression("SLEEPY");
      }
      this.bmoSessionStarted = true;
      this.bmoLastInputMs = now;
    }

    const mask = engine.buttonsMask;
    let gazeX = 0;
    let gazeY = 0;
    if (mask & 0x04) gazeX -= 0.85;
    if (mask & 0x08) gazeX += 0.85;
    if (mask & 0x01) gazeY += 0.85;
    if (mask & 0x02) gazeY -= 0.85;
    if (gazeX !== 0 || gazeY !== 0) {
      engine.setBmoGaze(gazeX, gazeY);
      this.updateBmoGazePadVisual(gazeX, gazeY);
      this.bmoLastInputMs = now;
    }

    if ((mask & 0x20) && this.bmoButtonHoldStart !== 0
        && now - this.bmoButtonHoldStart > 600) {
      this.exitBmoCompanion();
      return;
    }

    if (this.bmoAutoCycle && now - this.bmoLastAutoCycleMs >= 2500) {
      const cycleNames = (window.BMO_EXPRESSION_NAMES || []).filter((n) => n !== "HIDDEN");
      if (cycleNames.length > 0) {
        const nextExpr = cycleNames[this.bmoAutoCycleIndex % cycleNames.length];
        engine.setBmoExpression(nextExpr);
        this.bmoAutoCycleIndex = (this.bmoAutoCycleIndex + 1) % cycleNames.length;
        this.bmoLastAutoCycleMs = now;
        this.bmoLastInputMs = now;
      }
    }

    const idleSeconds = Math.floor((now - this.bmoLastInputMs) / 1000);
    if (this.bmoAutoSleep) {
      if (idleSeconds > 45 && engine.bmoExpression() !== 10) {
        engine.setBmoExpression("SLEEPING");
      } else if (idleSeconds > 20 && idleSeconds <= 45 && engine.bmoExpression() === 0) {
        engine.setBmoExpression("SLEEPY");
      }
    }

    engine.renderBmoFrame(this.ctx, now);
    if (engine.bmoHasSpeech()) {
      const visibleChars = Math.max(0, engine.bmoVisibleChars());
      const fullQuote = engine.bmoCurrentQuote();
      this.ctx.fillStyle = "#101E2B";
      this.ctx.font = "8px monospace";
      this.ctx.textAlign = "center";
      const lines = this.wrapSpeechLines(fullQuote, 116, 2);
      let remaining = visibleChars;
      const shownLines = [];
      for (const line of lines) {
        if (remaining <= 0) break;
        shownLines.push(line.slice(0, remaining));
        remaining -= line.length + 1;
      }
      if (lines.length <= 1) {
        if (shownLines[0]) this.ctx.fillText(shownLines[0], 160, 38);
      } else {
        if (shownLines[0]) this.ctx.fillText(shownLines[0], 160, 33);
        if (shownLines[1]) this.ctx.fillText(shownLines[1], 160, 42);
      }
    } else if (this.bmoHudVisible) {
      this.ctx.fillStyle = "#1A4B42";
      this.ctx.fillRect(40, 8, 240, 22);
      this.ctx.strokeStyle = "#CEF5E4";
      this.ctx.lineWidth = 1;
      this.ctx.strokeRect(40, 8, 240, 22);
      const exprName = engine.bmoExpressionName();
      const title = engine.bmoIsDancing() ? "* BMO DANCE PARTY! *"
        : this.bmoModeActive ? `BMO MODE: ${exprName}`
          : (this.bmoAutoSleep && idleSeconds > 40) ? "BMO IS SLEEPING..."
            : (this.bmoAutoSleep && idleSeconds > 20) ? "BMO IS SLEEPY..." : "LIVING BMO COMPANION";
      this.ctx.fillStyle = "#FFE033";
      this.ctx.font = "bold 12px Arial, sans-serif";
      this.ctx.textAlign = "center";
      this.ctx.fillText(title, 160, 24);
    }

    if (this.bmoHudVisible) {
      const prompt = "D-PAD:LOOK/PET  A:WINK  B:BLUSH  SEL:TALK  UP:DANCE  START:BACK";
      this.ctx.fillStyle = "#0F2620";
      this.ctx.fillRect(6, 214, 308, 22);
      this.ctx.strokeStyle = "#5FB49C";
      this.ctx.lineWidth = 1;
      this.ctx.strokeRect(6, 214, 308, 22);
      this.ctx.fillStyle = "#CEF5E4";
      this.ctx.font = "8px monospace";
      this.ctx.textAlign = "center";
      this.ctx.fillText(prompt, 160, 228);
    }

    if (now - this.bmoLastUiSyncMs >= 120) {
      this.bmoLastUiSyncMs = now;
      this.syncBmoModeUI();
    }
  }

  // Same slug rule as scripts/fetch_covers.py slugify_title().
  slugifyTitle(title) {
    let s = String(title || "").replace(/\.[^/.]+$/, "").toLowerCase();
    s = s.replace(/\([^)]*\)|\[[^\]]*\]/g, "");
    s = s.replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
    return s || "cover";
  }

  // Returns the cached cover entry for a title, starting the load on first use.
  getCoverImage(title) {
    const slug = this.slugifyTitle(title);
    let entry = this.coverImages[slug];
    if (!entry) {
      entry = { img: new Image(), ready: false, failed: false, slug };
      entry.img.onload = () => { entry.ready = true; };
      entry.img.onerror = () => { entry.failed = true; };
      entry.img.src = `covers/${slug}.png`;
      this.coverImages[slug] = entry;
    }
    return entry;
  }

  // Updates the sidebar panel with the full-resolution original box art.
  updateHdCoverPanel(entry, title) {
    const slug = entry ? entry.slug : "";
    if (slug === this.lastHdCoverSlug && (!entry || entry.ready || entry.failed)) return;
    const panelImg = document.getElementById("hd-boxart-preview-img");
    const panelMeta = document.getElementById("hd-boxart-meta");
    const panelTitle = document.getElementById("hd-boxart-title");
    if (!panelImg || !panelMeta || !panelTitle) return;
    if (entry && entry.ready) {
      this.lastHdCoverSlug = slug;
      panelImg.src = entry.img.src;
      panelImg.style.visibility = "visible";
      panelTitle.textContent = title;
      panelMeta.textContent = `${entry.img.naturalWidth} × ${entry.img.naturalHeight} original scan`;
    } else if (entry && entry.failed) {
      this.lastHdCoverSlug = slug;
      panelImg.style.visibility = "hidden";
      panelTitle.textContent = title;
      panelMeta.textContent = "No cover art for this title";
    }
  }

  // Mirrors firmware DisplayEmu::drawGameSelectMenu(): cover card, title, scroll track, button pills.
  renderGameSelectMenu() {
    const c = CONSOLES_LIST[this.selectedConsoleIndex];
    const games = this.getVisibleGames(c.id);
    const T = {
      black: "#0f2620", deepTeal: "#1a4b42", teal: "#5fb49c", mint: "#cef5e4",
      yellow: "#ffe033", coral: "#ff91a5", blue: "#52a6ba", white: "#f8fdfa"
    };
    const ctx = this.ctx;

    const pill = (x, y, glyph, label, bg, glyphCol, labelCol) => {
      const w = glyph.length * 6 + 8;
      ctx.fillStyle = bg;
      ctx.beginPath();
      ctx.roundRect(x, y, w, 14, 4);
      ctx.fill();
      ctx.font = "10px 'JetBrains Mono', monospace";
      ctx.textAlign = "left";
      ctx.fillStyle = glyphCol;
      ctx.fillText(glyph, x + 4, y + 11);
      ctx.fillStyle = labelCol;
      ctx.fillText(label, x + w + 4, y + 11);
    };

    ctx.fillStyle = T.black;
    ctx.fillRect(0, 0, 320, 240);
    ctx.fillStyle = T.deepTeal;
    ctx.fillRect(0, 0, 320, 38);
    this.drawBmoFace(8, 4, 1);

    const heading = c.id === "ROM_FAVORITES"
      ? "* FAVORITE GAMES"
      : `${c.name.toUpperCase()} ${this.topPicksFilter ? "TOP PICKS" : "LIBRARY"}`;
    ctx.fillStyle = T.yellow;
    ctx.font = "bold 12px 'Outfit', sans-serif";
    ctx.textAlign = "center";
    ctx.fillText(heading, 160, 25);

    if (games.length === 0) {
      this.updateHdCoverPanel(null, "");
      ctx.fillStyle = T.white;
      ctx.font = "bold 16px 'Outfit', sans-serif";
      if (c.id === "ROM_FAVORITES") {
        ctx.fillText("NO FAVORITES YET!", 160, 100);
        ctx.fillStyle = T.mint;
        ctx.font = "12px 'Outfit', sans-serif";
        ctx.fillText("Load a supported ROM, then press SELECT", 160, 128);
        ctx.fillText("in its library to star it *", 160, 148);
      } else {
        ctx.fillText("NO LOCAL ROMS", 160, 100);
        ctx.fillStyle = T.mint;
        ctx.font = "12px 'Outfit', sans-serif";
        const browserRom = c.browserSupported === true;
        const helpText = c.id === "ROM_FAVORITES"
          ? "Load a supported ROM file to play it here."
          : browserRom
            ? `Load a ${c.ext} file to play it here.`
            : "This system is not supported in browser gameplay yet.";
        ctx.fillText(helpText, 160, 138);
        if (browserRom || c.id === "ROM_FAVORITES") {
          ctx.fillText("Use the Load ROM button beside the simulator.", 160, 158);
        }
      }
      ctx.fillStyle = T.black;
      ctx.fillRect(0, 212, 320, 28);
      ctx.fillStyle = "#a4b0be";
      ctx.font = "10px 'JetBrains Mono', monospace";
      ctx.fillText("B: BACK", 160, 230);
      return;
    }

    const idx = Math.min(Math.max(this.selectedGameIndex, 0), games.length - 1);
    const game = games[idx];
    const title = (typeof game === "string") ? game : (game.title || `Game ${idx + 1}`);
    const isFav = (typeof game === "object") ? Boolean(game.isFav) : false;
    const isPop = (typeof game === "object") ? Boolean(game.isPopular) : false;
    const badge = (typeof game === "object" && game.console) ? game.console : c.badge;

    // Cover card (81,46,158x114)
    ctx.fillStyle = T.deepTeal;
    ctx.beginPath();
    ctx.roundRect(81, 46, 158, 114, 12);
    ctx.fill();
    ctx.strokeStyle = isFav ? T.yellow : T.teal;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(81.5, 46.5, 157, 113, 12);
    ctx.stroke();

    const cover = this.getCoverImage(title);
    this.updateHdCoverPanel(cover, title);
    if (cover.ready) {
      // 64x64 slot at (128,54), the size BoxArt::draw() uses on hardware.
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = "high";
      ctx.drawImage(cover.img, 128, 54, 64, 64);
      ctx.strokeStyle = isFav ? T.yellow : T.teal;
      ctx.strokeRect(127.5, 53.5, 65, 65);
    } else {
      ctx.fillStyle = T.black;
      ctx.fillRect(102, 62, 116, 52);
      ctx.fillStyle = T.yellow;
      ctx.font = "bold 16px 'Outfit', sans-serif";
      ctx.fillText(badge, 160, 94);
    }

    // Status pill (112,126,96x18)
    ctx.strokeStyle = isFav ? T.yellow : (isPop ? T.mint : T.teal);
    ctx.beginPath();
    ctx.roundRect(112.5, 126.5, 95, 17, 8);
    ctx.stroke();
    ctx.font = "10px 'JetBrains Mono', monospace";
    ctx.fillStyle = isFav ? T.yellow : (isPop ? T.mint : T.white);
    ctx.fillText(isFav ? "* FAVORITE" : (isPop ? "TOP PICK" : badge), 160, 138);

    // Title, truncated to 290 px like drawFittedCentered()
    ctx.font = "12px 'Outfit', sans-serif";
    ctx.fillStyle = T.white;
    let shown = title;
    while (ctx.measureText(shown).width > 290 && shown.length > 4) {
      shown = shown.slice(0, shown.length - 4) + "...";
    }
    ctx.fillText(shown, 160, 184);

    ctx.font = "10px 'JetBrains Mono', monospace";
    ctx.fillStyle = T.mint;
    ctx.fillText(`${idx + 1} / ${games.length}  [${badge}${this.topPicksFilter ? " TOP" : ""}]`, 160, 206);

    // Scroll position track
    if (games.length > 1) {
      const trackX = 60, trackW = 200, trackY = 168;
      ctx.fillStyle = T.deepTeal;
      ctx.fillRect(trackX, trackY, trackW, 2);
      const thumbW = Math.max(14, Math.floor(trackW / games.length));
      const thumbX = trackX + Math.floor((idx / (games.length - 1)) * (trackW - thumbW));
      ctx.fillStyle = isFav ? T.yellow : T.mint;
      ctx.beginPath();
      ctx.roundRect(thumbX, trackY - 1, thumbW, 4, 2);
      ctx.fill();
    }

    // Footer action bar
    ctx.fillStyle = T.deepTeal;
    ctx.fillRect(0, 214, 320, 26);
    ctx.fillStyle = T.teal;
    ctx.fillRect(0, 213, 320, 1);
    pill(6, 220, "A", "PLAY", T.coral, T.black, T.white);
    pill(56, 220, "B", "BACK", T.mint, T.black, T.white);
    pill(106, 220, "SEL", isFav ? "UNSTAR" : "STAR", T.yellow, T.black, T.white);
    pill(180, 220, "ST", this.topPicksFilter ? "ALL" : "TOP", T.blue, T.white, T.white);
    pill(236, 220, "<>", "A-Z", T.teal, T.black, T.white);
  }

  renderEmulationView() {
    const c = CONSOLES_LIST[this.selectedConsoleIndex];
    const pal = this.palettes[this.activePalette] || this.palettes.classic;

    if (this.wasmEngine && this.wasmEngine.ready) {
      this.wasmEngine.renderToCanvas(this.ctx, pal, this.isPausedInGame);
    } else {
      this.ctx.fillStyle = pal[3];
      this.ctx.fillRect(0, 0, 320, 240);
      this.ctx.fillStyle = pal[0];
      this.ctx.fillRect(40, 12, 240, 216);
      this.ctx.fillStyle = pal[3];
      this.ctx.font = "bold 13px 'Press Start 2P', monospace";
      this.ctx.textAlign = "center";
      this.ctx.fillText(c.name, 160, 115);
    }

    if (this.isPausedInGame) {
      this.ctx.fillStyle = "rgba(10, 18, 20, 0.86)";
      this.ctx.fillRect(44, 28, 232, 184);
      this.ctx.strokeStyle = "#FFE033";
      this.ctx.lineWidth = 2;
      this.ctx.strokeRect(44, 28, 232, 184);

      this.ctx.fillStyle = "#FFE033";
      this.ctx.font = "bold 11px 'Press Start 2P', monospace";
      this.ctx.textAlign = "center";
      this.ctx.fillText("PAUSE MENU", 160, 50);

      const items = ["RESUME GAME", "SAVE STATE (S0)", "LOAD STATE (S0)", "QUIT TO MENU"];
      for (let i = 0; i < items.length; i++) {
        const y = 76 + i * 24;
        const sel = i === this.pauseMenuIndex;
        this.ctx.fillStyle = sel ? "#FFE033" : "#CEF5E4";
        this.ctx.font = sel ? "bold 11px 'JetBrains Mono', monospace" : "11px 'JetBrains Mono', monospace";
        this.ctx.fillText(sel ? `▶ ${items[i]}` : items[i], 160, y);
      }

      const swapTxt = (this.wasmEngine && this.wasmEngine.swapAB) ? "ON" : "OFF";
      const bezTxt = (this.wasmEngine && this.wasmEngine.bezelEnabled) ? "ON" : "OFF";
      this.ctx.fillStyle = "#8CD7C2";
      this.ctx.font = "9px 'JetBrains Mono', monospace";
      this.ctx.fillText(`◄ A/B Swap: ${swapTxt}  |  ► Bezel: ${bezTxt}`, 160, 196);
    }
  }

  async connectWebSerial() {
    const statusEl = document.getElementById("webserial-status");
    const logEl = document.getElementById("webserial-log");
    if (!("serial" in navigator)) {
      if (logEl) logEl.textContent = "WebSerial API requires Chrome / Edge desktop browser.";
      return false;
    }
    try {
      this.serialPort = await navigator.serial.requestPort();
      await this.serialPort.open({ baudRate: 115200 });
      if (statusEl) statusEl.textContent = "Connected";
      if (logEl) logEl.textContent = "ESP32-S3 serial port opened. This page does not send firmware commands or parse telemetry.";
      return true;
    } catch (err) {
      if (logEl) logEl.textContent = `Serial connection cancelled: ${err.message || err}`;
      return false;
    }
  }

  async ingestUploadedRoms(fileList) {
    if (!fileList || fileList.length === 0) return 0;
    const added = [];
    const rejected = [];
    for (const file of Array.from(fileList)) {
      const dot = file.name.lastIndexOf(".");
      const ext = dot >= 0 ? file.name.slice(dot).toLowerCase() : "";
      const systemId = ext === ".gb" ? "ROM_GB"
        : ext === ".nes" ? "ROM_NES"
          : ext === ".a26" ? "ROM_ATARI"
              : ext === ".col" ? "ROM_COLEM" : "";
      if (!systemId) {
          rejected.push(`${file.name}: browser gameplay currently supports .gb, .nes, .a26, and .col ROMs only.`);
        continue;
      }
      if (file.size > MAX_BROWSER_ROM_SIZE) {
        rejected.push(`${file.name}: browser ROMs cannot exceed 8 MiB.`);
        continue;
      }
      const cleanTitle = file.name.replace(/\.[^/.]+$/, "");
      try {
        if (typeof file.arrayBuffer !== "function") throw new Error("This browser cannot read the selected file.");
        const bytes = new Uint8Array(await file.arrayBuffer());
        const entry = {
          title: cleanTitle,
          console: systemId === "ROM_GB" ? "GB"
            : systemId === "ROM_NES" ? "NES"
              : systemId === "ROM_ATARI" ? "ATARI" : "COLEM",
          systemId,
          isFav: false,
          isPopular: false,
          romBytes: bytes
        };
        this.gameLibrary[systemId].unshift(entry);
        added.push(entry);
      } catch (error) {
        rejected.push(`${file.name}: ${error.message}`);
      }
    }

    this.rebuildFavoritesInDatabase(this.gameLibrary);
    if (rejected.length > 0) this.showSimulatorMessage(rejected.join(" "), true);
    if (added.length > 0) {
      this.selectedConsoleIndex = CONSOLES_LIST.findIndex((consoleInfo) => consoleInfo.id === added[0].systemId);
      this.selectedGameIndex = 0;
      if (fileList.length === 1) {
        if (!(await this.launchSelectedGame(added[0].systemId, added[0]))) {
          this.gameLibrary[added[0].systemId] = this.gameLibrary[added[0].systemId]
            .filter((entry) => entry !== added[0]);
          this.rebuildFavoritesInDatabase(this.gameLibrary);
        }
      }
      else this.showSimulatorMessage(`Added ${added.length} local supported ROM(s). Select a title in its console library to play.`);
    } else if (rejected.length === 0) {
      this.showSimulatorMessage("No ROM files were selected.", true);
    }
    return added.length;
  }
}

// Instantiate Simulator on DOM Load
window.addEventListener("DOMContentLoaded", () => {
  window.simulator = new BmoSimulator();
  const btnSerial = document.getElementById("btn-webserial-connect");
  if (btnSerial) btnSerial.addEventListener("click", () => window.simulator.connectWebSerial());
  const fileInput = document.getElementById("rom-file-upload");
  if (fileInput) fileInput.addEventListener("change", (e) => {
    window.simulator.ingestUploadedRoms(e.target.files).catch((error) => window.simulator.showSimulatorMessage(error.message, true));
  });
  window.addEventListener("dragover", (e) => e.preventDefault());
  window.addEventListener("drop", (e) => {
    e.preventDefault();
    if (e.dataTransfer && e.dataTransfer.files) {
      window.simulator.ingestUploadedRoms(e.dataTransfer.files).catch((error) => window.simulator.showSimulatorMessage(error.message, true));
    }
  });
});
