/**
 * simulator/js/sim_controls.js
 * URL state parsing, keyboard/pointer/button listeners, state machine input dispatch, and UI controls.
 */

import { CONSOLES_LIST, STATE } from "./constants.js";

const KEY_BIT_MAP = {
  ArrowUp: 1, ArrowDown: 2, ArrowLeft: 4, ArrowRight: 8,
  z: 16, Z: 16, x: 32, X: 32, Enter: 64, Shift: 128,
  s: 256, S: 256, a: 512, A: 512, q: 1024, Q: 1024, w: 2048, W: 2048,
};

const KEY_TO_BUTTON = {
  ArrowUp: "UP", ArrowDown: "DOWN", ArrowLeft: "LEFT", ArrowRight: "RIGHT",
  z: "A", Z: "A", x: "B", X: "B", Enter: "START", Shift: "SELECT",
  s: "X", S: "X", a: "Y", A: "Y", q: "L", Q: "L", w: "R", W: "R",
  " ": "MUSEUM", m: "MUSEUM", M: "MUSEUM",
};

const RELEASE_KEY_MAP = {
  ArrowUp: "UP", ArrowDown: "DOWN", ArrowLeft: "LEFT", ArrowRight: "RIGHT",
  z: "A", Z: "A", x: "B", X: "B", Enter: "START", Shift: "SELECT",
  s: "X", S: "X", a: "Y", A: "Y", q: "L", Q: "L", w: "R", W: "R",
};

const PHYSICAL_BUTTONS = [
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
  { id: "btn-r", key: "R", bit: 2048 },
];

const KEY_TO_DOM_ID = Object.fromEntries(PHYSICAL_BUTTONS.map((b) => [b.key, b.id]));

const INTERNALS_URL_PRESETS = new Set(["core", "unscrew", "wiring", "display"]);

function applyGameplayUrlState(sim, hasConsoleParam) {
  if (!hasConsoleParam) sim.selectedConsoleIndex = 1;
  const cid = CONSOLES_LIST[sim.selectedConsoleIndex].id;
  const games = sim.gameLibrary[cid] || [];
  if (games.length > 0) sim.launchSelectedGame(cid, games[0]);
  else sim.currentState = STATE.GAME_SELECT;
}

export function applyUrlStateParamsImpl(sim) {
  if (typeof window === "undefined" || !window.location) return;
  const params = new URLSearchParams(window.location.search);
  const consoleParam = params.get("console");
  const hasConsole = consoleParam !== null;
  if (hasConsole) {
    const idx = parseInt(consoleParam, 10);
    if (!isNaN(idx) && idx >= 0 && idx < CONSOLES_LIST.length) {
      sim.selectedConsoleIndex = idx;
      sim.updateSidebarMuseumView();
    }
  }
  const gameParam = params.get("game");
  if (gameParam !== null) {
    const gi = parseInt(gameParam, 10);
    if (!isNaN(gi) && gi >= 0) sim.selectedGameIndex = gi;
  }

  const stateParam = (params.get("state") || "").toLowerCase();
  if (!stateParam) return;
  if (stateParam === "splash") { sim.currentState = STATE.BOOT_SPLASH; return; }
  if (stateParam === "bmo") { sim.enterBmoMode(); return; }
  if (stateParam === "mascot") { sim.enterBmoCompanion(true, false); return; }
  if (stateParam === "internals" || stateParam === "dissect") { sim.pendingInternalsPreset = "exploded"; return; }
  if (INTERNALS_URL_PRESETS.has(stateParam)) { sim.pendingInternalsPreset = stateParam; return; }

  const listDefaults = {
    carousel: [STATE.CONSOLE_SELECT, false, true],
    library: [STATE.GAME_SELECT, false, false],
    toppicks: [STATE.GAME_SELECT, true, false],
    museum: [STATE.CONSOLE_MUSEUM, false, true],
  };
  const cfg = listDefaults[stateParam];
  if (cfg) {
    sim.currentState = cfg[0];
    if (!hasConsole) sim.selectedConsoleIndex = 2;
    if (cfg[0] === STATE.GAME_SELECT) sim.topPicksFilter = cfg[1];
    if (cfg[2]) sim.updateSidebarMuseumView();
    return;
  }
  if (stateParam === "gameplay") applyGameplayUrlState(sim, hasConsole);
}

function isEditableTarget(target) {
  if (!target) return false;
  const tag = target.tagName ? target.tagName.toUpperCase() : "";
  return Boolean(target.isContentEditable || tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT");
}

function addListener(sim, target, type, handler, options) {
  if (typeof sim.trackListener === "function") {
    sim.trackListener(target, type, handler, options);
  } else if (target && typeof target.addEventListener === "function") {
    target.addEventListener(type, handler, options);
  }
}

function bindKeyboardHandlers(sim) {
  addListener(sim, window, "keydown", (e) => {
    if (isEditableTarget(e.target)) return;
    if (e.repeat && sim.currentState === STATE.IDLE_MASCOT) return;
    if (sim.wasmEngine && KEY_BIT_MAP[e.key]) sim.wasmEngine.setButtonBit(KEY_BIT_MAP[e.key], true);
    const button = KEY_TO_BUTTON[e.key];
    if (!button) return;
    sim.handleButtonPress(button);
    if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
    if (e.key.startsWith("Arrow")) e.preventDefault();
  });

  addListener(sim, window, "keyup", (e) => {
    if (isEditableTarget(e.target)) return;
    if (sim.wasmEngine && KEY_BIT_MAP[e.key]) sim.wasmEngine.setButtonBit(KEY_BIT_MAP[e.key], false);
    const releasedKey = RELEASE_KEY_MAP[e.key];
    if (releasedKey) {
      sim.handleButtonRelease(releasedKey);
      if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
    }
  });
}

function bindPhysicalButtonHandlers(sim) {
  PHYSICAL_BUTTONS.forEach((b) => {
    const el = document.getElementById(b.id);
    if (!el) return;
    addListener(sim, el, "mousedown", () => {
      el.classList.add("pressed");
      sim._activeMouseButton = { el, b };
      if (sim.wasmEngine) sim.wasmEngine.setButtonBit(b.bit, true);
      sim.handleButtonPress(b.key);
      if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
    });
    addListener(sim, el, "touchstart", (e) => {
      e.preventDefault();
      el.classList.add("pressed");
      if (sim.wasmEngine) sim.wasmEngine.setButtonBit(b.bit, true);
      sim.handleButtonPress(b.key);
      if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
    });
    addListener(sim, el, "touchend", () => {
      el.classList.remove("pressed");
      if (sim.wasmEngine) sim.wasmEngine.setButtonBit(b.bit, false);
      sim.handleButtonRelease(b.key);
      if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
    });
  });

  addListener(sim, window, "mouseup", () => {
    if (!sim._activeMouseButton) return;
    const { el, b } = sim._activeMouseButton;
    sim._activeMouseButton = null;
    el.classList.remove("pressed");
    if (sim.wasmEngine) sim.wasmEngine.setButtonBit(b.bit, false);
    sim.handleButtonRelease(b.key);
    if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
  });
}

function bindScreenPointerEvents(sim) {
  if (!sim.canvas) return;
  let draggingScreen = false;
  let screenMoved = false;
  let startX = 0;
  let startY = 0;
  const aimFromCanvasEvent = (e) => {
    const rect = sim.canvas.getBoundingClientRect();
    if (!rect.width || !rect.height) return;
    sim.setBmoModeGaze(((e.clientX - rect.left) / rect.width) * 2 - 1, 1 - ((e.clientY - rect.top) / rect.height) * 2);
  };
  addListener(sim, sim.canvas, "pointerdown", (e) => {
    if (sim.currentState === STATE.BOOT_SPLASH) {
      sim.handleButtonPress("A");
      if (typeof sim.markRenderDirty === "function") sim.markRenderDirty();
      return;
    }
    if (sim.currentState !== STATE.IDLE_MASCOT) return;
    draggingScreen = true;
    screenMoved = false;
    startX = e.clientX;
    startY = e.clientY;
    aimFromCanvasEvent(e);
  });
  addListener(sim, window, "pointermove", (e) => {
    if (!draggingScreen || sim.currentState !== STATE.IDLE_MASCOT) return;
    if (Math.hypot(e.clientX - startX, e.clientY - startY) > 4) screenMoved = true;
    aimFromCanvasEvent(e);
  });
  addListener(sim, window, "pointerup", () => {
    if (!draggingScreen) return;
    draggingScreen = false;
    if (!screenMoved && sim.currentState === STATE.IDLE_MASCOT) {
      sim.runBmoAction((engine) => engine.petBmoCompanion(), false);
    }
  });
}

export function setupInputHandlersImpl(sim) {
  if (!Array.isArray(sim._cleanupFns)) sim._cleanupFns = [];
  bindKeyboardHandlers(sim);
  bindPhysicalButtonHandlers(sim);
  bindScreenPointerEvents(sim);
}

function handleConsoleSelectButton(sim, key) {
  if (key === "UP") { sim.enterBmoCompanion(); return; }
  if (key === "RIGHT") {
    sim.selectedConsoleIndex = (sim.selectedConsoleIndex + 1) % CONSOLES_LIST.length;
    sim.updateSidebarMuseumView();
    return;
  }
  if (key === "LEFT") {
    sim.selectedConsoleIndex = (sim.selectedConsoleIndex - 1 + CONSOLES_LIST.length) % CONSOLES_LIST.length;
    sim.updateSidebarMuseumView();
    return;
  }
  if (key === "A" || key === "START") {
    sim.currentState = STATE.GAME_SELECT;
    sim.selectedGameIndex = 0;
    sim.gameListScrollOffset = 0;
    return;
  }
  if (key === "SELECT" || key === "MUSEUM") sim.currentState = STATE.CONSOLE_MUSEUM;
}

function toggleSelectedFavorite(sim, activeConsole, games) {
  const game = games[sim.selectedGameIndex];
  if (!game) return;
  game.isFav = !game.isFav;
  sim.rebuildFavoritesInDatabase(sim.gameLibrary);
  sim.bmo.expression = game.isFav ? "HAPPY" : "SURPRISED";
  if (activeConsole.id === "ROM_FAVORITES") {
    const favCount = (sim.gameLibrary.ROM_FAVORITES || []).length;
    if (sim.selectedGameIndex >= favCount) sim.selectedGameIndex = Math.max(0, favCount - 1);
  }
}

function handleGameSelectButton(sim, key) {
  const activeConsole = CONSOLES_LIST[sim.selectedConsoleIndex];
  const games = sim.getVisibleGames(activeConsole.id);
  if (key === "UP") { if (sim.selectedGameIndex > 0) sim.selectedGameIndex--; return; }
  if (key === "DOWN") { if (sim.selectedGameIndex < games.length - 1) sim.selectedGameIndex++; return; }
  if (key === "RIGHT") { sim.selectedGameIndex = Math.min(games.length - 1, sim.selectedGameIndex + 8); return; }
  if (key === "LEFT") { sim.selectedGameIndex = Math.max(0, sim.selectedGameIndex - 8); return; }
  if (key === "START" && activeConsole.id !== "ROM_FAVORITES") {
    sim.topPicksFilter = !sim.topPicksFilter;
    sim.selectedGameIndex = 0;
    return;
  }
  if (key === "SELECT" && games.length > 0) { toggleSelectedFavorite(sim, activeConsole, games); return; }
  if (key === "B") {
    sim.topPicksFilter = false;
    sim.currentState = STATE.CONSOLE_SELECT;
    sim.bmo.expression = "IDLE";
    return;
  }
  if (key === "A" && games.length > 0) sim.launchSelectedGame(activeConsole.id, games[sim.selectedGameIndex]);
}

function executePauseMenuAction(sim) {
  const eng = sim.wasmEngine;
  if (sim.pauseMenuIndex === 0) { sim.isPausedInGame = false; return; }
  if (sim.pauseMenuIndex === 1 && eng) {
    const saved = eng.saveState(eng.activeSlot);
    sim.showSimulatorMessage(saved ? "Game state saved for this browser session." : "Could not save the current game state.", !saved);
    sim.isPausedInGame = false;
    return;
  }
  if (sim.pauseMenuIndex === 2 && eng) {
    const loaded = eng.loadState(eng.activeSlot);
    sim.showSimulatorMessage(loaded ? "Game state restored." : "No saved state exists for this game in this browser session.", !loaded);
    sim.isPausedInGame = false;
    return;
  }
  if (sim.pauseMenuIndex === 3) {
    sim.isPausedInGame = false;
    sim.currentState = STATE.GAME_SELECT;
  }
}

function handleEmulationButton(sim, key) {
  if (!sim.isPausedInGame) return;
  if (key === "UP") { sim.pauseMenuIndex = (sim.pauseMenuIndex + 3) % 4; return; }
  if (key === "DOWN") { sim.pauseMenuIndex = (sim.pauseMenuIndex + 1) % 4; return; }
  if (key === "LEFT" && sim.wasmEngine) { sim.wasmEngine.swapAB = !sim.wasmEngine.swapAB; return; }
  if (key === "RIGHT" && sim.wasmEngine) { sim.wasmEngine.bezelEnabled = !sim.wasmEngine.bezelEnabled; return; }
  if (key === "B") { sim.isPausedInGame = false; return; }
  if (key === "A" || key === "START") executePauseMenuAction(sim);
}

const STATE_INPUT_HANDLERS = {
  [STATE.CONSOLE_SELECT]: handleConsoleSelectButton,
  [STATE.CONSOLE_MUSEUM]: (sim, key) => {
    if (["B", "SELECT", "A", "START"].includes(key)) sim.currentState = STATE.CONSOLE_SELECT;
  },
  [STATE.GAME_SELECT]: handleGameSelectButton,
  [STATE.IDLE_MASCOT]: (sim, key) => sim.handleBmoButtonPress(key),
  [STATE.EMULATION_VIEW]: handleEmulationButton,
};

export function handleButtonPressImpl(sim, key) {
  sim.lastInputActivityMs = performance.now();
  if (sim.bmo3D) sim.bmo3D.pressButton(key, false);
  const domId = KEY_TO_DOM_ID[key];
  if (domId) {
    const el = document.getElementById(domId);
    if (el) {
      el.classList.add("pressed");
      setTimeout(() => el.classList.remove("pressed"), 120);
    }
  }
  if (sim.currentState === STATE.BOOT_SPLASH) {
    sim.currentState = STATE.CONSOLE_SELECT;
    sim.bmo.expression = "HAPPY";
    return;
  }
  if (sim.currentState === STATE.EMULATION_VIEW && !sim.isPausedInGame && sim.wasmEngine && (sim.wasmEngine.buttonsMask & 0xC0) === 0xC0) {
    sim.isPausedInGame = true;
    sim.pauseMenuIndex = 0;
    return;
  }
  const handler = STATE_INPUT_HANDLERS[sim.currentState];
  if (handler) handler(sim, key);
}

function bindViewAndThemeControls(sim) {
  document.querySelectorAll(".scale-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".scale-btn").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      document.documentElement.style.setProperty("--scale-multiplier", btn.getAttribute("data-scale"));
      if (sim.bmo3D) sim.bmo3D.onResize();
    });
  });

  const themeSelect = document.getElementById("theme-select");
  if (themeSelect) {
    themeSelect.addEventListener("change", (e) => {
      const target = /** @type {HTMLSelectElement} */ (e.target);
      document.body.setAttribute("data-theme", target.value);
      if (sim.bmo3D) sim.bmo3D.setTheme(target.value);
    });
  }

  document.querySelectorAll(".cam-preset-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".cam-preset-btn").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      const cam = btn.getAttribute("data-cam");
      if (!sim.is3DView) { sim.is3DView = true; sim.updateViewMode(); }
      if (cam === "core") { sim.applyInternalsPresetUI("core"); return; }
      if (cam === "reset") {
        sim.applyInternalsPresetUI("assembled");
        if (sim.bmo3D) sim.bmo3D.setCameraPreset("front");
        return;
      }
      if (sim.bmo3D) sim.bmo3D.setCameraPreset(cam);
    });
  });

  const btnToggleView = document.getElementById("btn-toggle-viewmode");
  if (btnToggleView) {
    btnToggleView.addEventListener("click", () => {
      sim.is3DView = !sim.is3DView;
      sim.updateViewMode();
    });
  }
}

function bindStudioSliders(sim) {
  const sliderLighting = /** @type {HTMLInputElement | null} */ (document.getElementById("slider-lighting"));
  const sliderSaturation = /** @type {HTMLInputElement | null} */ (document.getElementById("slider-saturation"));
  const sliderTemperature = /** @type {HTMLInputElement | null} */ (document.getElementById("slider-temperature"));
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
    if (valTemperature) valTemperature.textContent = tVal > 0 ? `+${tVal} Warm` : (tVal < 0 ? `${tVal} Cool` : "Neutral");
    if (sim.bmo3D) sim.bmo3D.setStudioAdjustments({ brightness: bVal / 100, saturation: sVal / 100, temperature: tVal / 100 });
    const chassis2D = document.getElementById("gameboy-chassis");
    if (chassis2D) chassis2D.style.filter = `brightness(${bVal / 100}) saturate(${sVal / 100})`;
  };

  if (sliderLighting) sliderLighting.addEventListener("input", updateStudio);
  if (sliderSaturation) sliderSaturation.addEventListener("input", updateStudio);
  if (sliderTemperature) sliderTemperature.addEventListener("input", updateStudio);
  if (btnResetStudio) {
    btnResetStudio.addEventListener("click", () => {
      if (sliderLighting) sliderLighting.value = "100";
      if (sliderSaturation) sliderSaturation.value = "100";
      if (sliderTemperature) sliderTemperature.value = "0";
      updateStudio();
    });
  }
  updateStudio();
}

function bindWasmAndFilterControls(sim) {
  const paletteSelect = document.getElementById("palette-select");
  if (paletteSelect) {
    paletteSelect.addEventListener("change", (e) => {
      sim.activePalette = /** @type {HTMLSelectElement} */ (e.target).value;
    });
  }
  const filterSelect = document.getElementById("filter-select");
  if (filterSelect) {
    filterSelect.addEventListener("change", (e) => {
      const val = /** @type {HTMLSelectElement} */ (e.target).value;
      document.body.classList.remove("body-crt", "body-lcd");
      if (val === "crt" || val === "lcd") document.body.classList.add(`body-${val}`);
    });
  }
  document.querySelectorAll(".state-nav-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      const stateName = btn.getAttribute("data-state");
      if (stateName && STATE[stateName] !== undefined) sim.currentState = STATE[stateName];
    });
  });

  const bindLogBtn = (id, fn) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener("click", () => {
      if (!sim.wasmEngine) return;
      const msg = fn(sim.wasmEngine);
      const log = document.getElementById("playtest-engine-log");
      if (log) log.textContent = msg;
    });
  };
  bindLogBtn("btn-wasm-save-state", (eng) => {
    eng.saveState(0);
    return `State saved to Slot 0 for ${eng.activeTitle}`;
  });
  bindLogBtn("btn-wasm-load-state", (eng) => (
    eng.loadState(0) ? `State loaded from Slot 0 (Frame ${eng.frameCount})` : "No saved state in Slot 0"
  ));
  bindLogBtn("btn-wasm-toggle-bezel", (eng) => {
    eng.bezelEnabled = !eng.bezelEnabled;
    return `1:1 Console Bezel: ${eng.bezelEnabled ? "ON" : "OFF"}`;
  });
  bindLogBtn("btn-wasm-toggle-ab", (eng) => {
    eng.swapAB = !eng.swapAB;
    return `Gamepad A/B Swap: ${eng.swapAB ? "ON" : "OFF"}`;
  });
}

export function setupUIControlsImpl(sim) {
  bindViewAndThemeControls(sim);
  bindStudioSliders(sim);
  bindWasmAndFilterControls(sim);
  sim.setupBmoModeControls();
  sim.setupInternalsControls();
  sim.updateSidebarMuseumView();
}
