/**
 * simulator.js
 * High-fidelity 1:1 BMO Handheld Console Simulator and UI/UX Interactive Lab.
 * Replicates the firmware's 2D SDF BMO Mascot, ST7789 SPI rendering, Console Carousel,
 * Game Select Menu, and Gaming History Museum.
 */

// State Machine Constants (Synchronized with BmoGameboy.ino)
const STATE = {
  BOOT_SPLASH: 0,
  CONSOLE_SELECT: 1,
  GAME_SELECT: 2,
  CONSOLE_MUSEUM: 3,
  EMULATION_VIEW: 4
};

// Console Metadata
const CONSOLES_LIST = [
  { id: "ROM_FAVORITES", name: "★ Favorites", year: "Starred", ext: "★", badge: "FAV", color: "#FFE033", count: 5 },
  { id: "ROM_GB", name: "Game Boy", year: "1989", ext: ".gb", badge: "DMG", color: "#5FB49C", count: 603 },
  { id: "ROM_GBC", name: "Game Boy Color", year: "1998", ext: ".gbc", badge: "GBC", color: "#2ED573", count: 538 },
  { id: "ROM_NES", name: "Nintendo NES", year: "1983", ext: ".nes", badge: "NES", color: "#FF5E57", count: 578 },
  { id: "ROM_SNES", name: "Super Nintendo", year: "1990", ext: ".sfc", badge: "SNES", color: "#9C88FF", count: 818 },
  { id: "ROM_GENESIS", name: "Sega Genesis", year: "1988", ext: ".gen", badge: "MD", color: "#2E86DE", count: 773 },
  { id: "ROM_SMS", name: "Master System", year: "1985", ext: ".sms", badge: "SMS", color: "#3B82F6", count: 386 },
  { id: "ROM_GG", name: "Game Gear", year: "1990", ext: ".gg", badge: "GG", color: "#F368E0", count: 305 },
  { id: "ROM_PCE", name: "PC Engine / TG16", year: "1987", ext: ".pce", badge: "PCE", color: "#FF6B6B", count: 443 },
  { id: "ROM_ATARI", name: "Atari 2600", year: "1977", ext: ".a26", badge: "A26", color: "#E056FD", count: 279 },
  { id: "ROM_COLEM", name: "ColecoVision", year: "1982", ext: ".col", badge: "COL", color: "#00D2D3", count: 157 },
  { id: "ROM_NGP", name: "Neo Geo Pocket", year: "1998", ext: ".ngc", badge: "NGP", color: "#FFA502", count: 114 },
  { id: "ROM_LYNX", name: "Atari Lynx", year: "1989", ext: ".lnx", badge: "LNX", color: "#10AC84", count: 95 },
  { id: "ROM_WSWAN", name: "WonderSwan Color", year: "1999", ext: ".wsc", badge: "WS", color: "#ECCC68", count: 83 },
  { id: "ROM_PICO8", name: "PICO-8", year: "2015", ext: ".p8", badge: "P8", color: "#FF78C4", count: 12534 },
  { id: "ROM_WAD", name: "DOOM", year: "1993", ext: ".wad", badge: "WAD", color: "#FF4757", count: 3 }
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
    this.bootTimer = 0;
    this.lastFrameTime = performance.now();
    this.fps = 60;

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

    // DMG Runtime Palettes
    this.palettes = {
      classic: ["#9bbc0f", "#8bac0f", "#306230", "#0f380f"],
      bmo:     ["#dcf8f2", "#53c6b5", "#164848", "#051214"],
      pocket:  ["#f5f5f5", "#aaaaaa", "#555555", "#0a0a0a"],
      light:   ["#b4fff0", "#50c8d2", "#146478", "#021e28"],
      amber:   ["#ffd250", "#dc9614", "#784600", "#140a00"]
    };
    this.activePalette = "classic";

    // Battery State
    this.batteryState = 88;
    this.batteryLevel = 88;
    this.isCharging = false;

    // Audio SFX synthesis using Web Audio API
    this.initAudio();

    // Sample Game List Cache
    this.mockGames = this.generateMockGameDatabase();

    // Setup input listeners
    this.setupInputHandlers();
    this.setupUIControls();

    // Start 60 FPS Render Loop
    requestAnimationFrame(this.renderLoop.bind(this));
  }

  initAudio() {
    try {
      window.AudioContext = window.AudioContext || window.webkitAudioContext;
      this.audioCtx = new AudioContext();
    } catch (e) {
      this.audioCtx = null;
    }
  }

  playBeep(freq = 440, type = "square", duration = 0.04) {
    if (!this.audioCtx) return;
    if (this.audioCtx.state === "suspended") {
      this.audioCtx.resume();
    }
    const osc = this.audioCtx.createOscillator();
    const gain = this.audioCtx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, this.audioCtx.currentTime);
    gain.gain.setValueAtTime(0.08, this.audioCtx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, this.audioCtx.currentTime + duration);
    osc.connect(gain);
    gain.connect(this.audioCtx.destination);
    osc.start();
    osc.stop(this.audioCtx.currentTime + duration);
  }

  generateMockGameDatabase() {
    const db = {};
    CONSOLES_LIST.forEach(c => {
      if (c.id === "ROM_FAVORITES") return;
      const history = CONSOLE_HISTORY_DATABASE[c.id];
      const games = [];
      if (c.id === "ROM_GB") {
        games.push({ title: "Virtual BMO (Official Game)", console: "GB", isFav: true });
        games.push({ title: "Tetris (World) (Rev A)", console: "GB", isFav: true });
        games.push({ title: "Pokemon Red Version", console: "GB", isFav: false });
        games.push({ title: "The Legend of Zelda - Link's Awakening", console: "GB", isFav: false });
      } else if (c.id === "ROM_GBC") {
        games.push({ title: "Super Mario Bros Deluxe", console: "GBC", isFav: true });
        games.push({ title: "The Legend of Zelda - Oracle of Ages", console: "GBC", isFav: true });
        games.push({ title: "Pokemon Crystal Version", console: "GBC", isFav: false });
        games.push({ title: "Aladdin", console: "GBC", isFav: false });
      } else if (c.id === "ROM_NES") {
        games.push({ title: "Super Mario Bros 3", console: "NES", isFav: true });
        games.push({ title: "The Legend of Zelda", console: "NES", isFav: false });
        games.push({ title: "Mega Man 2", console: "NES", isFav: false });
      } else {
        if (history && history.hallmarkGames) {
          history.hallmarkGames.forEach(hg => games.push({ title: `${hg.title}`, console: c.badge, isFav: false }));
        }
        for (let i = 1; i <= 15; i++) {
          games.push({ title: `${c.name} Classic Title ${i}`, console: c.badge, isFav: false });
        }
      }
      db[c.id] = games;
    });
    this.rebuildFavoritesInDatabase(db);
    return db;
  }

  rebuildFavoritesInDatabase(db) {
    const favs = [];
    Object.keys(db).forEach(cid => {
      if (cid === "ROM_FAVORITES") return;
      db[cid].forEach(g => {
        if (g.isFav) favs.push(g);
      });
    });
    db["ROM_FAVORITES"] = favs;
    const favConsole = CONSOLES_LIST.find(c => c.id === "ROM_FAVORITES");
    if (favConsole) favConsole.count = favs.length;
  }

  setupInputHandlers() {
    // Keyboard Handler
    window.addEventListener("keydown", (e) => {
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
        case " ":
        case "m":
        case "M":
          this.handleButtonPress("MUSEUM");
          break;
      }
    });

    // Touch & Mouse On-Screen Physical Buttons
    const buttons = [
      { id: "btn-dpad-up", key: "UP" },
      { id: "btn-dpad-down", key: "DOWN" },
      { id: "btn-dpad-left", key: "LEFT" },
      { id: "btn-dpad-right", key: "RIGHT" },
      { id: "btn-a", key: "A" },
      { id: "btn-b", key: "B" },
      { id: "btn-start", key: "START" },
      { id: "btn-select", key: "SELECT" }
    ];

    buttons.forEach(b => {
      const el = document.getElementById(b.id);
      if (el) {
        el.addEventListener("mousedown", () => {
          el.classList.add("pressed");
          this.handleButtonPress(b.key);
        });
        window.addEventListener("mouseup", () => el.classList.remove("pressed"));
        el.addEventListener("touchstart", (e) => {
          e.preventDefault();
          el.classList.add("pressed");
          this.handleButtonPress(b.key);
        });
        el.addEventListener("touchend", () => el.classList.remove("pressed"));
      }
    });
  }

  handleButtonPress(key) {
    this.playBeep(key === "A" ? 520 : key === "B" ? 380 : 440, "square", 0.05);

    // Visual button reflection
    const btnMap = {
      "UP": "btn-dpad-up", "DOWN": "btn-dpad-down", "LEFT": "btn-dpad-left", "RIGHT": "btn-dpad-right",
      "A": "btn-a", "B": "btn-b", "START": "btn-start", "SELECT": "btn-select"
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

    if (this.currentState === STATE.CONSOLE_SELECT) {
      if (key === "RIGHT") {
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
      const games = this.mockGames[activeConsole.id] || [];
      
      if (key === "UP") {
        if (this.selectedGameIndex > 0) this.selectedGameIndex--;
      } else if (key === "DOWN") {
        if (this.selectedGameIndex < games.length - 1) this.selectedGameIndex++;
      } else if (key === "RIGHT") {
        this.selectedGameIndex = Math.min(games.length - 1, this.selectedGameIndex + 8);
      } else if (key === "LEFT") {
        this.selectedGameIndex = Math.max(0, this.selectedGameIndex - 8);
      } else if (key === "SELECT") {
        if (games.length > 0) {
          const game = games[this.selectedGameIndex];
          if (game) {
            game.isFav = !game.isFav;
            this.rebuildFavoritesInDatabase(this.mockGames);
            if (game.isFav) {
              this.playBeep(880, "triangle", 0.08);
              this.bmo.expression = "HAPPY";
            } else {
              this.playBeep(330, "sine", 0.08);
              this.bmo.expression = "SURPRISED";
            }
            if (activeConsole.id === "ROM_FAVORITES") {
              if (this.selectedGameIndex >= (this.mockGames["ROM_FAVORITES"] || []).length) {
                this.selectedGameIndex = Math.max(0, (this.mockGames["ROM_FAVORITES"] || []).length - 1);
              }
            }
          }
        }
      } else if (key === "B") {
        this.currentState = STATE.CONSOLE_SELECT;
        this.bmo.expression = "IDLE";
      } else if (key === "A" || key === "START") {
        if (games.length > 0) {
          this.currentState = STATE.EMULATION_VIEW;
          this.bmo.expression = "HAPPY";
        }
      }
    } else if (this.currentState === STATE.EMULATION_VIEW) {
      if (key === "SELECT" || key === "B") {
        this.currentState = STATE.GAME_SELECT;
      }
    }
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
      });
    }
    // Palette selector
    const paletteSelect = document.getElementById("palette-select");
    if (paletteSelect) {
      paletteSelect.addEventListener("change", (e) => {
        this.activePalette = e.target.value;
      });
    }

    // Battery indicator interactive cycle
    const battIndicator = document.getElementById("battery-indicator");
    if (battIndicator) {
      battIndicator.addEventListener("click", () => {
        const states = [100, 75, 45, 15, "charging"];
        const curIdx = states.indexOf(this.batteryState);
        const nextState = states[(curIdx + 1) % states.length];
        this.setBatteryState(nextState);
        this.playBeep(nextState === "charging" ? 750 : nextState <= 20 ? 300 : 550, "sine", 0.05);
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

    this.updateSidebarMuseumView();
  }

  setBatteryState(state) {
    this.batteryState = state;
    const fillEl = document.getElementById("chassis-battery-fill");
    const pctEl = document.getElementById("chassis-battery-pct");
    if (!fillEl || !pctEl) return;

    fillEl.className = "pixel-battery-fill";
    if (state === "charging") {
      fillEl.classList.add("charging");
      pctEl.innerText = "CHG";
      this.batteryLevel = 100;
      this.isCharging = true;
      this.bmo.expression = "HAPPY";
    } else {
      this.isCharging = false;
      this.batteryLevel = state;
      fillEl.style.width = `${state}%`;
      pctEl.innerText = `${state}%`;
      if (state <= 20) {
        fillEl.classList.add("charge-low");
        this.bmo.expression = "SLEEPY";
      } else if (state <= 50) {
        fillEl.classList.add("charge-med");
        this.bmo.expression = "IDLE";
      } else {
        this.bmo.expression = "IDLE";
      }
    }
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
      gamesContainer.innerHTML = data.hallmarkGames.map(g => `
        <div class="hallmark-game-item">
          <div class="hallmark-title">${g.title} (${g.year})</div>
          <div class="hallmark-desc">${g.significance}</div>
        </div>
      `).join("");
    }
  }

  // =========================================================================
  // RENDERING PIPELINE (320x240 Native ST7789 Resolution)
  // =========================================================================
  renderLoop(timestamp) {
    const dt = (timestamp - this.lastFrameTime) / 1000;
    this.lastFrameTime = timestamp;
    this.fps = Math.round(1 / (dt || 0.016));

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
    }

    // Telemetry display update
    const fpsVal = document.getElementById("telemetry-fps");
    if (fpsVal) fpsVal.innerText = `${this.fps} FPS`;

    requestAnimationFrame(this.renderLoop.bind(this));
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
    this.ctx.fillText("BMO GAMEBOY", 160, 156);

    this.ctx.fillStyle = "#cef5e4";
    this.ctx.font = "11px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("15 Retro Consoles • Thousands of Adventures", 160, 182);

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
      this.ctx.fillStyle = "#E8175D";
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
    this.ctx.fillText(`Format: ${c.ext}`, cardX + 16, cardY + 95);

    // Game Count Tag
    this.ctx.fillStyle = "#ffe033";
    this.ctx.font = "bold 13px 'JetBrains Mono', monospace";
    this.ctx.fillText(`★ ${c.count.toLocaleString()} Games Ready`, cardX + 16, cardY + 124);

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

  renderGameSelectMenu() {
    const c = CONSOLES_LIST[this.selectedConsoleIndex];
    const games = this.mockGames[c.id] || [];

    // Background - Authentic BMO Mint Screen
    this.ctx.fillStyle = "#cef5e4";
    this.ctx.fillRect(0, 0, 320, 240);

    // Header Bar - Deep Forest Teal
    this.ctx.fillStyle = "#1a4b42";
    this.ctx.fillRect(0, 0, 320, 34);

    this.ctx.fillStyle = (c.id === "ROM_FAVORITES") ? "#ffe033" : "#ffe033";
    this.ctx.font = "bold 13px 'Outfit', sans-serif";
    this.ctx.textAlign = "left";
    this.ctx.fillText(c.id === "ROM_FAVORITES" ? "★ FAVORITE GAMES" : `📂 ${c.name} Games`, 12, 22);

    this.ctx.fillStyle = "#cef5e4";
    this.ctx.font = "bold 10px 'JetBrains Mono', monospace";
    this.ctx.textAlign = "right";
    this.ctx.fillText(`${games.length > 0 ? this.selectedGameIndex + 1 : 0}/${games.length}`, 310, 22);

    if (games.length === 0) {
      this.ctx.fillStyle = "#0f2620";
      this.ctx.font = "bold 14px 'Outfit', sans-serif";
      this.ctx.textAlign = "center";
      this.ctx.fillText("NO FAVORITES YET!", 160, 105);
      this.ctx.fillStyle = "#1a4b42";
      this.ctx.font = "12px 'Outfit', sans-serif";
      this.ctx.fillText("Press SELECT on any game to star it ★", 160, 130);
    } else {
      // List rendering (5 visible items per page)
      const visibleCount = 5;
      const startIdx = Math.max(0, Math.min(this.selectedGameIndex - 2, games.length - visibleCount));
      const endIdx = Math.min(games.length, startIdx + visibleCount);

      for (let i = startIdx; i < endIdx; i++) {
        const y = 42 + (i - startIdx) * 33;
        const isSelected = (i === this.selectedGameIndex);
        const game = games[i];
        const title = (typeof game === "string") ? game : (game.title || `Game ${i + 1}`);
        const isFav = (typeof game === "object") ? game.isFav : false;
        const badge = (typeof game === "object" && game.console) ? game.console : c.badge;

        if (isSelected) {
          this.ctx.fillStyle = "#1a4b42";
          this.ctx.beginPath();
          this.ctx.roundRect(8, y, 304, 30, 8);
          this.ctx.fill();
          this.ctx.strokeStyle = "#ffe033";
          this.ctx.lineWidth = 1.5;
          this.ctx.stroke();
          this.ctx.fillStyle = "#ffe033";
        } else {
          this.ctx.fillStyle = "rgba(95, 180, 156, 0.25)";
          this.ctx.beginPath();
          this.ctx.roundRect(8, y, 304, 30, 8);
          this.ctx.fill();
          this.ctx.strokeStyle = "#5fb49c";
          this.ctx.lineWidth = 1;
          this.ctx.stroke();
          this.ctx.fillStyle = "#0f2620";
        }

        this.ctx.font = isSelected ? "bold 12px 'Outfit', sans-serif" : "12px 'Outfit', sans-serif";
        this.ctx.textAlign = "left";
        this.ctx.fillText(`${isSelected ? '▶ ' : '  '}${title}`, 16, y + 20);

        // Right-aligned Star & Badge
        this.ctx.textAlign = "right";
        if (isFav) {
          this.ctx.fillStyle = "#ffe033";
          this.ctx.font = "bold 12px 'Outfit', sans-serif";
          this.ctx.fillText(`★ [${badge}]`, 300, y + 20);
        } else {
          this.ctx.fillStyle = isSelected ? "#cef5e4" : "#438f7a";
          this.ctx.font = "10px 'JetBrains Mono', monospace";
          this.ctx.fillText(`[${badge}]`, 300, y + 20);
        }
      }
    }

    // Footer Bar
    this.ctx.fillStyle = "#1a4b42";
    this.ctx.fillRect(0, 214, 320, 26);
    this.ctx.fillStyle = "#cef5e4";
    this.ctx.font = "10px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("A: Play  •  B: Back  •  SELECT: ★ Favorite  •  ◄/►: Jump ±8", 160, 231);
  }

  renderEmulationView() {
    const c = CONSOLES_LIST[this.selectedConsoleIndex];
    const pal = this.palettes[this.activePalette] || this.palettes.classic;

    // Viewport background
    this.ctx.fillStyle = pal[3];
    this.ctx.fillRect(0, 0, 320, 240);

    // Centered Game Frame Window (240x216)
    this.ctx.fillStyle = pal[0];
    this.ctx.fillRect(40, 12, 240, 216);

    // Decorative retro elements
    this.ctx.fillStyle = pal[2];
    this.ctx.fillRect(50, 22, 220, 196);

    this.ctx.fillStyle = pal[1];
    this.ctx.fillRect(60, 32, 200, 176);

    this.ctx.fillStyle = pal[3];
    this.ctx.font = "bold 13px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText(c.name, 160, 95);

    this.ctx.fillStyle = pal[2];
    this.ctx.font = "10px 'Press Start 2P', monospace";
    this.ctx.fillText("60 FPS • SPI DMA", 160, 125);

    this.ctx.fillStyle = pal[3];
    this.ctx.font = "bold 11px 'Outfit', sans-serif";
    this.ctx.fillText(`Palette: ${this.activePalette.toUpperCase()}`, 160, 155);

    this.ctx.font = "10px 'Outfit', sans-serif";
    this.ctx.fillText("Hold SELECT + ◄/► to Fast-Forward | SELECT/B: Exit", 160, 185);
  }
}

// Instantiate Simulator on DOM Load
window.addEventListener("DOMContentLoaded", () => {
  window.simulator = new BmoSimulator();
});
