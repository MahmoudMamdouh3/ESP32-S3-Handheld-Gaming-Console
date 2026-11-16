/**
 * simulator/js/sim_screens.js
 * Canvas UI & mascot screen renderers for boot splash, console carousel, game list, mascot mode, and pause HUD.
 */

import { BMO_EXPRESSION_NAMES } from "../wasm_playtest_engine.js";
import { CONSOLES_LIST } from "./constants.js";

/** @type {Map<string, string>} */
const TRUNCATED_TITLE_CACHE = new Map();
const MAX_TITLE_CACHE_ENTRIES = 256;

function fitTitleCached(ctx, title, maxWidth) {
  const key = `${title}|${maxWidth}`;
  const cached = TRUNCATED_TITLE_CACHE.get(key);
  if (cached !== undefined) return cached;
  let shown = title;
  while (ctx.measureText(shown).width > maxWidth && shown.length > 4) {
    shown = shown.slice(0, shown.length - 4) + "...";
  }
  if (TRUNCATED_TITLE_CACHE.size >= MAX_TITLE_CACHE_ENTRIES) {
    const oldest = TRUNCATED_TITLE_CACHE.keys().next().value;
    if (oldest !== undefined) TRUNCATED_TITLE_CACHE.delete(oldest);
  }
  TRUNCATED_TITLE_CACHE.set(key, shown);
  return shown;
}

export function renderBootSplashImpl(sim, dt) {
  const ctx = sim.ctx;
  sim.bootTimer += dt;
  sim.drawBmoFace(100, 42, 2.5);

  ctx.fillStyle = "#ffffff";
  ctx.font = "bold 16px 'Press Start 2P', monospace";
  ctx.textAlign = "center";
  ctx.fillText("BMO HANDHELD", 160, 156);

  ctx.fillStyle = "#cef5e4";
  ctx.font = "11px 'Outfit', sans-serif";
  ctx.textAlign = "center";
  ctx.fillText("14 Emulation Cores • Thousands of Adventures", 160, 182);

  if (Math.floor(sim.bootTimer * 2) % 2 === 0) {
    ctx.fillStyle = "#ffe033";
    ctx.font = "bold 11px 'Press Start 2P', monospace";
    ctx.fillText("PRESS ANY BUTTON", 160, 218);
  }
}

export function renderConsoleSelectMenuImpl(sim) {
  const ctx = sim.ctx;
  ctx.fillStyle = "#1a4b42";
  ctx.fillRect(0, 0, 320, 42);
  sim.drawBmoFace(8, 5, 1.0);

  ctx.fillStyle = "#cef5e4";
  ctx.font = "bold 11px 'JetBrains Mono', monospace";
  ctx.textAlign = "right";
  ctx.fillText(`SYSTEM ${sim.selectedConsoleIndex + 1}/${CONSOLES_LIST.length}`, 310, 26);

  const c = CONSOLES_LIST[sim.selectedConsoleIndex];
  const cardX = 35;
  const cardY = 56;
  const cardW = 250;
  const cardH = 145;
  const accentColor = c.id === "ROM_FAVORITES" ? "#ffe033" : c.color;

  ctx.fillStyle = "#1a4b42";
  ctx.beginPath();
  ctx.roundRect(cardX, cardY, cardW, cardH, 12);
  ctx.fill();
  ctx.strokeStyle = accentColor;
  ctx.lineWidth = 2;
  ctx.stroke();

  ctx.fillStyle = accentColor;
  ctx.beginPath();
  ctx.roundRect(cardX + 16, cardY + 16, 54, 24, 6);
  ctx.fill();
  ctx.fillStyle = "#0b0f19";
  ctx.font = "bold 11px 'Press Start 2P', monospace";
  ctx.textAlign = "center";
  ctx.fillText(c.badge, cardX + 43, cardY + 33);

  ctx.fillStyle = "#ffe033";
  ctx.font = "bold 12px 'JetBrains Mono', monospace";
  ctx.textAlign = "right";
  ctx.fillText(c.year, cardX + cardW - 20, cardY + 33);
  sim.drawConsolePixelArt(c, cardX + 175, cardY + 50, 58, 74);

  ctx.fillStyle = "#ffffff";
  ctx.font = "bold 15px 'Outfit', sans-serif";
  ctx.textAlign = "left";
  ctx.fillText(c.name, cardX + 16, cardY + 74);
  ctx.fillStyle = "#8cd7c2";
  ctx.font = "11px 'JetBrains Mono', monospace";
  ctx.fillText(c.browserSupported ? `Browser core: ${c.ext}` : `Firmware core: ${c.ext}`, cardX + 16, cardY + 95);
  ctx.fillStyle = "#ffe033";
  ctx.font = "bold 13px 'JetBrains Mono', monospace";
  ctx.fillText(`${c.count.toLocaleString()} LOCAL ROMS`, cardX + 16, cardY + 124);

  ctx.fillStyle = "#5fb49c";
  ctx.font = "bold 18px 'Outfit', sans-serif";
  ctx.textAlign = "center";
  ctx.fillText("◀", 18, 134);
  ctx.fillText("▶", 302, 134);
  ctx.fillStyle = "#1a4b42";
  ctx.fillRect(0, 214, 320, 26);
  ctx.fillStyle = "#cef5e4";
  ctx.font = "10px 'Outfit', sans-serif";
  ctx.fillText("A: Browse Games  •  SELECT: History & Specs  •  ◄/►: Console", 160, 231);
}

function renderBmoUnreadyPlaceholder(sim) {
  const ctx = sim.ctx;
  ctx.fillStyle = "#8AD5C3";
  ctx.fillRect(0, 0, 320, 240);
  ctx.fillStyle = "#0F2620";
  ctx.textAlign = "center";
  if (sim.engineLoadFailed) {
    ctx.font = "bold 11px monospace";
    ctx.fillText("BMO WASM CORE FAILED TO LOAD", 160, 102);
    ctx.font = "9px monospace";
    const errLines = sim.wrapSpeechLines(sim.engineLoadError || "Run: python simulator/run_simulator.py", 280, 3);
    errLines.forEach((line, idx) => ctx.fillText(line, 160, 122 + idx * 14));
  } else {
    ctx.font = "bold 10px monospace";
    ctx.fillText("LOADING FIRMWARE BMO...", 160, 120);
  }
}

function updateBmoMascotInputsAndState(sim, engine, now) {
  if (!sim.bmoSessionStarted) {
    if (sim.bmoEntryGreeting) {
      engine.setBmoExpression("HAPPY");
      engine.sayBmoQuote(0, 3500);
    } else {
      engine.setBmoExpression("SLEEPY");
    }
    sim.bmoSessionStarted = true;
    sim.bmoLastInputMs = now;
  }

  const mask = engine.buttonsMask;
  const gazeX = ((mask & 0x04) ? -0.85 : 0) + ((mask & 0x08) ? 0.85 : 0);
  const gazeY = ((mask & 0x01) ? 0.85 : 0) + ((mask & 0x02) ? -0.85 : 0);
  if (gazeX !== 0 || gazeY !== 0) {
    engine.setBmoGaze(gazeX, gazeY);
    sim.updateBmoGazePadVisual(gazeX, gazeY);
    sim.bmoLastInputMs = now;
  }

  if ((mask & 0x20) && sim.bmoButtonHoldStart !== 0 && now - sim.bmoButtonHoldStart > 600) {
    sim.exitBmoCompanion();
    return false;
  }

  if (sim.bmoAutoCycle && now - sim.bmoLastAutoCycleMs >= 2500) {
    const cycleNames = BMO_EXPRESSION_NAMES.filter((n) => n !== "HIDDEN");
    if (cycleNames.length > 0) {
      engine.setBmoExpression(cycleNames[sim.bmoAutoCycleIndex % cycleNames.length]);
      sim.bmoAutoCycleIndex = (sim.bmoAutoCycleIndex + 1) % cycleNames.length;
      sim.bmoLastAutoCycleMs = now;
      sim.bmoLastInputMs = now;
    }
  }

  const idleSeconds = Math.floor((now - sim.bmoLastInputMs) / 1000);
  if (sim.bmoAutoSleep) {
    if (idleSeconds > 45 && engine.bmoExpression() !== 10) engine.setBmoExpression("SLEEPING");
    else if (idleSeconds > 20 && idleSeconds <= 45 && engine.bmoExpression() === 0) engine.setBmoExpression("SLEEPY");
  }
  return idleSeconds;
}

function renderBmoSpeechOrHud(sim, engine, idleSeconds) {
  const ctx = sim.ctx;
  if (engine.bmoHasSpeech()) {
    let remaining = Math.max(0, engine.bmoVisibleChars());
    const lines = sim.wrapSpeechLines(engine.bmoCurrentQuote(), 116, 2);
    const shownLines = [];
    for (const line of lines) {
      if (remaining <= 0) break;
      shownLines.push(line.slice(0, remaining));
      remaining -= line.length + 1;
    }
    ctx.fillStyle = "#101E2B";
    ctx.font = "8px monospace";
    ctx.textAlign = "center";
    if (lines.length <= 1 && shownLines[0]) ctx.fillText(shownLines[0], 160, 38);
    else {
      if (shownLines[0]) ctx.fillText(shownLines[0], 160, 33);
      if (shownLines[1]) ctx.fillText(shownLines[1], 160, 42);
    }
  } else if (sim.bmoHudVisible) {
    ctx.fillStyle = "#1A4B42";
    ctx.fillRect(40, 8, 240, 22);
    ctx.strokeStyle = "#CEF5E4";
    ctx.lineWidth = 1;
    ctx.strokeRect(40, 8, 240, 22);
    const title = engine.bmoIsDancing() ? "* BMO DANCE PARTY! *"
      : sim.bmoModeActive ? `BMO MODE: ${engine.bmoExpressionName()}`
      : (sim.bmoAutoSleep && idleSeconds > 40) ? "BMO IS SLEEPING..."
      : (sim.bmoAutoSleep && idleSeconds > 20) ? "BMO IS SLEEPY..." : "LIVING BMO COMPANION";
    ctx.fillStyle = "#FFE033";
    ctx.font = "bold 12px Arial, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText(title, 160, 24);
  }
  if (!sim.bmoHudVisible) return;
  ctx.fillStyle = "#0F2620";
  ctx.fillRect(6, 214, 308, 22);
  ctx.strokeStyle = "#5FB49C";
  ctx.lineWidth = 1;
  ctx.strokeRect(6, 214, 308, 22);
  ctx.fillStyle = "#CEF5E4";
  ctx.font = "8px monospace";
  ctx.textAlign = "center";
  ctx.fillText("D-PAD:LOOK/PET  A:WINK  B:BLUSH  SEL:TALK  UP:DANCE  START:BACK", 160, 228);
}

export function renderIdleMascotScreenImpl(sim, now = performance.now()) {
  const engine = sim.wasmEngine;
  if (!engine || !engine.bmoReady) {
    renderBmoUnreadyPlaceholder(sim);
    return;
  }
  const idleSeconds = updateBmoMascotInputsAndState(sim, engine, now);
  if (idleSeconds === false) return;
  engine.renderBmoFrame(sim.ctx, now);
  renderBmoSpeechOrHud(sim, engine, idleSeconds);
  if (now - sim.bmoLastUiSyncMs >= 120) {
    sim.bmoLastUiSyncMs = now;
    sim.syncBmoModeUI();
  }
}

const THEME_T = {
  black: "#0f2620", deepTeal: "#1a4b42", teal: "#5fb49c", mint: "#cef5e4",
  yellow: "#ffe033", coral: "#ff91a5", blue: "#52a6ba", white: "#f8fdfa",
};

function drawMenuPill(ctx, x, y, glyph, label, bg, glyphCol, labelCol) {
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
}

function renderEmptyGameLibrary(sim, ctx, c) {
  sim.updateHdCoverPanel(null, "");
  ctx.fillStyle = THEME_T.white;
  ctx.font = "bold 16px 'Outfit', sans-serif";
  if (c.id === "ROM_FAVORITES") {
    ctx.fillText("NO FAVORITES YET!", 160, 100);
    ctx.fillStyle = THEME_T.mint;
    ctx.font = "12px 'Outfit', sans-serif";
    ctx.fillText("Load a supported ROM, then press SELECT", 160, 128);
    ctx.fillText("in its library to star it *", 160, 148);
  } else {
    ctx.fillText("NO LOCAL ROMS", 160, 100);
    ctx.fillStyle = THEME_T.mint;
    ctx.font = "12px 'Outfit', sans-serif";
    const browserRom = c.browserSupported === true;
    ctx.fillText(browserRom ? `Load a ${c.ext} file to play it here.` : "This system is not supported in browser gameplay yet.", 160, 138);
    if (browserRom) ctx.fillText("Use the Load ROM button beside the simulator.", 160, 158);
  }
  ctx.fillStyle = THEME_T.black;
  ctx.fillRect(0, 212, 320, 28);
  ctx.fillStyle = "#a4b0be";
  ctx.font = "10px 'JetBrains Mono', monospace";
  ctx.fillText("B: BACK", 160, 230);
}

function renderGameCardDetails(sim, ctx, c, games, idx) {
  const game = games[idx];
  const title = (typeof game === "string") ? game : (game.title || `Game ${idx + 1}`);
  const isFav = (typeof game === "object") ? Boolean(game.isFav) : false;
  const isPop = (typeof game === "object") ? Boolean(game.isPopular) : false;
  const badge = (typeof game === "object" && game.console) ? game.console : c.badge;

  ctx.fillStyle = THEME_T.deepTeal;
  ctx.beginPath();
  ctx.roundRect(81, 46, 158, 114, 12);
  ctx.fill();
  ctx.strokeStyle = isFav ? THEME_T.yellow : THEME_T.teal;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.roundRect(81.5, 46.5, 157, 113, 12);
  ctx.stroke();

  const cover = sim.getCoverImage(title);
  sim.updateHdCoverPanel(cover, title);
  if (cover.ready) {
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = "high";
    ctx.drawImage(cover.img, 128, 54, 64, 64);
    ctx.strokeStyle = isFav ? THEME_T.yellow : THEME_T.teal;
    ctx.strokeRect(127.5, 53.5, 65, 65);
  } else {
    ctx.fillStyle = THEME_T.black;
    ctx.fillRect(102, 62, 116, 52);
    ctx.fillStyle = THEME_T.yellow;
    ctx.font = "bold 16px 'Outfit', sans-serif";
    ctx.fillText(badge, 160, 94);
  }

  ctx.strokeStyle = isFav ? THEME_T.yellow : (isPop ? THEME_T.mint : THEME_T.teal);
  ctx.beginPath();
  ctx.roundRect(112.5, 126.5, 95, 17, 8);
  ctx.stroke();
  ctx.font = "10px 'JetBrains Mono', monospace";
  ctx.fillStyle = isFav ? THEME_T.yellow : (isPop ? THEME_T.mint : THEME_T.white);
  ctx.fillText(isFav ? "* FAVORITE" : (isPop ? "TOP PICK" : badge), 160, 138);

  ctx.font = "12px 'Outfit', sans-serif";
  ctx.fillStyle = THEME_T.white;
  const shown = fitTitleCached(ctx, title, 290);
  ctx.fillText(shown, 160, 184);
  ctx.font = "10px 'JetBrains Mono', monospace";
  ctx.fillStyle = THEME_T.mint;
  ctx.fillText(`${idx + 1} / ${games.length}  [${badge}${sim.topPicksFilter ? " TOP" : ""}]`, 160, 206);
  return isFav;
}

export function renderGameSelectMenuImpl(sim) {
  const c = CONSOLES_LIST[sim.selectedConsoleIndex];
  const games = sim.getVisibleGames(c.id);
  const ctx = sim.ctx;

  ctx.fillStyle = THEME_T.black;
  ctx.fillRect(0, 0, 320, 240);
  ctx.fillStyle = THEME_T.deepTeal;
  ctx.fillRect(0, 0, 320, 38);
  sim.drawBmoFace(8, 4, 1);

  const heading = c.id === "ROM_FAVORITES" ? "* FAVORITE GAMES" : `${c.name.toUpperCase()} ${sim.topPicksFilter ? "TOP PICKS" : "LIBRARY"}`;
  ctx.fillStyle = THEME_T.yellow;
  ctx.font = "bold 12px 'Outfit', sans-serif";
  ctx.textAlign = "center";
  ctx.fillText(heading, 160, 25);

  if (games.length === 0) {
    renderEmptyGameLibrary(sim, ctx, c);
    return;
  }

  const idx = Math.min(Math.max(sim.selectedGameIndex, 0), games.length - 1);
  const isFav = renderGameCardDetails(sim, ctx, c, games, idx);
  if (games.length > 1) {
    const trackX = 60, trackW = 200, trackY = 168;
    ctx.fillStyle = THEME_T.deepTeal;
    ctx.fillRect(trackX, trackY, trackW, 2);
    const thumbW = Math.max(14, Math.floor(trackW / games.length));
    const thumbX = trackX + Math.floor((idx / (games.length - 1)) * (trackW - thumbW));
    ctx.fillStyle = isFav ? THEME_T.yellow : THEME_T.mint;
    ctx.beginPath();
    ctx.roundRect(thumbX, trackY - 1, thumbW, 4, 2);
    ctx.fill();
  }

  ctx.fillStyle = THEME_T.deepTeal;
  ctx.fillRect(0, 214, 320, 26);
  ctx.fillStyle = THEME_T.teal;
  ctx.fillRect(0, 213, 320, 1);
  drawMenuPill(ctx, 6, 220, "A", "PLAY", THEME_T.coral, THEME_T.black, THEME_T.white);
  drawMenuPill(ctx, 56, 220, "B", "BACK", THEME_T.mint, THEME_T.black, THEME_T.white);
  drawMenuPill(ctx, 106, 220, "SEL", isFav ? "UNSTAR" : "STAR", THEME_T.yellow, THEME_T.black, THEME_T.white);
  drawMenuPill(ctx, 180, 220, "ST", sim.topPicksFilter ? "ALL" : "TOP", THEME_T.blue, THEME_T.white, THEME_T.white);
  drawMenuPill(ctx, 236, 220, "<>", "A-Z", THEME_T.teal, THEME_T.black, THEME_T.white);
}

export function renderEmulationViewImpl(sim) {
  const c = CONSOLES_LIST[sim.selectedConsoleIndex];
  const pal = sim.palettes[sim.activePalette] || sim.palettes.classic;
  const ctx = sim.ctx;

  if (sim.wasmEngine && sim.wasmEngine.ready) {
    sim.wasmEngine.renderToCanvas(ctx, pal, sim.isPausedInGame);
  } else {
    ctx.fillStyle = pal[3];
    ctx.fillRect(0, 0, 320, 240);
    ctx.fillStyle = pal[0];
    ctx.fillRect(40, 12, 240, 216);
    ctx.fillStyle = pal[3];
    ctx.font = "bold 13px 'Press Start 2P', monospace";
    ctx.textAlign = "center";
    ctx.fillText(c.name, 160, 115);
  }

  if (!sim.isPausedInGame) return;
  ctx.fillStyle = "rgba(10, 18, 20, 0.86)";
  ctx.fillRect(44, 28, 232, 184);
  ctx.strokeStyle = "#FFE033";
  ctx.lineWidth = 2;
  ctx.strokeRect(44, 28, 232, 184);
  ctx.fillStyle = "#FFE033";
  ctx.font = "bold 11px 'Press Start 2P', monospace";
  ctx.textAlign = "center";
  ctx.fillText("PAUSE MENU", 160, 50);

  const items = ["RESUME GAME", "SAVE STATE (S0)", "LOAD STATE (S0)", "QUIT TO MENU"];
  for (let i = 0; i < items.length; i++) {
    const sel = i === sim.pauseMenuIndex;
    ctx.fillStyle = sel ? "#FFE033" : "#CEF5E4";
    ctx.font = sel ? "bold 11px 'JetBrains Mono', monospace" : "11px 'JetBrains Mono', monospace";
    ctx.fillText(sel ? `▶ ${items[i]}` : items[i], 160, 76 + i * 24);
  }
  const swapTxt = (sim.wasmEngine && sim.wasmEngine.swapAB) ? "ON" : "OFF";
  const bezTxt = (sim.wasmEngine && sim.wasmEngine.bezelEnabled) ? "ON" : "OFF";
  ctx.fillStyle = "#8CD7C2";
  ctx.font = "9px 'JetBrains Mono', monospace";
  ctx.fillText(`◄ A/B Swap: ${swapTxt}  |  ► Bezel: ${bezTxt}`, 160, 196);
}
