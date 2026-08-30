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
  { id: "ROM_GB", name: "Game Boy", year: "1989", ext: ".gb", badge: "DMG", color: "#48DBFB", count: 602 },
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
      const history = CONSOLE_HISTORY_DATABASE[c.id];
      const games = [];
      if (history && history.hallmarkGames) {
        history.hallmarkGames.forEach(hg => games.push(`${hg.title} (${c.year})`));
      }
      for (let i = 1; i <= 25; i++) {
        games.push(`${c.name} Classic Title ${i} [USA]`);
      }
      db[c.id] = games;
    });
    return db;
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
      } else if (key === "B") {
        this.currentState = STATE.CONSOLE_SELECT;
      } else if (key === "A" || key === "START") {
        this.currentState = STATE.EMULATION_VIEW;
        this.bmo.expression = "HAPPY";
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
    this.drawBmoFace(100, 50, 2.5);

    // Title
    this.ctx.fillStyle = "#ffffff";
    this.ctx.font = "bold 16px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText("BMO GAMEBOY", 160, 160);

    this.ctx.fillStyle = "#00d2d3";
    this.ctx.font = "10px 'Outfit', sans-serif";
    this.ctx.fillText("15 CONSOLES • 17,700+ GAMES • OCTAL PSRAM", 160, 185);

    // Press A Prompt
    if (Math.floor(this.bootTimer * 2) % 2 === 0) {
      this.ctx.fillStyle = "#feca57";
      this.ctx.font = "bold 11px 'Press Start 2P', monospace";
      this.ctx.fillText("PRESS ANY BUTTON", 160, 215);
    }
  }

  renderConsoleSelectMenu() {
    // Header Bar
    this.ctx.fillStyle = "#1e272e";
    this.ctx.fillRect(0, 0, 320, 42);

    // Mini BMO Face
    this.drawBmoFace(8, 5, 1.0);

    // Console Index / Counter
    this.ctx.fillStyle = "#a4b0be";
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

    this.ctx.fillStyle = "#182130";
    this.ctx.beginPath();
    this.ctx.roundRect(cardX, cardY, cardW, cardH, 12);
    this.ctx.fill();

    this.ctx.strokeStyle = c.color;
    this.ctx.lineWidth = 2;
    this.ctx.stroke();

    // Console Badge
    this.ctx.fillStyle = c.color;
    this.ctx.beginPath();
    this.ctx.roundRect(cardX + 16, cardY + 16, 54, 24, 6);
    this.ctx.fill();

    this.ctx.fillStyle = "#0b0f19";
    this.ctx.font = "bold 11px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText(c.badge, cardX + 43, cardY + 33);

    // Year
    this.ctx.fillStyle = "#feca57";
    this.ctx.font = "bold 12px 'JetBrains Mono', monospace";
    this.ctx.textAlign = "right";
    this.ctx.fillText(c.year, cardX + cardW - 20, cardY + 33);

    // Full Name
    this.ctx.fillStyle = "#ffffff";
    this.ctx.font = "bold 15px 'Outfit', sans-serif";
    this.ctx.textAlign = "left";
    this.ctx.fillText(c.name, cardX + 16, cardY + 74);

    // Extension & Format Tag
    this.ctx.fillStyle = "#747d8c";
    this.ctx.font = "11px 'JetBrains Mono', monospace";
    this.ctx.fillText(`Format: ${c.ext}`, cardX + 16, cardY + 95);

    // Game Count Tag
    this.ctx.fillStyle = "#1dd1a1";
    this.ctx.font = "bold 13px 'JetBrains Mono', monospace";
    this.ctx.fillText(`★ ${c.count.toLocaleString()} Games Ready`, cardX + 16, cardY + 124);

    // Carousel Navigation Arrows
    this.ctx.fillStyle = "#00d2d3";
    this.ctx.font = "bold 18px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("◀", 18, 134);
    this.ctx.fillText("▶", 302, 134);

    // Footer Instruction Bar
    this.ctx.fillStyle = "#121824";
    this.ctx.fillRect(0, 214, 320, 26);

    this.ctx.fillStyle = "#a4b0be";
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

    // Header Bar
    this.ctx.fillStyle = "#1e272e";
    this.ctx.fillRect(0, 0, 320, 34);

    this.ctx.fillStyle = c.color;
    this.ctx.font = "bold 12px 'Outfit', sans-serif";
    this.ctx.textAlign = "left";
    this.ctx.fillText(`📂 ${c.name} Games`, 12, 22);

    this.ctx.fillStyle = "#a4b0be";
    this.ctx.font = "bold 10px 'JetBrains Mono', monospace";
    this.ctx.textAlign = "right";
    this.ctx.fillText(`${this.selectedGameIndex + 1}/${games.length}`, 310, 22);

    // List rendering (6 visible items per page)
    const visibleCount = 6;
    const startIdx = Math.max(0, Math.min(this.selectedGameIndex - 2, games.length - visibleCount));
    const endIdx = Math.min(games.length, startIdx + visibleCount);

    for (let i = startIdx; i < endIdx; i++) {
      const y = 42 + (i - startIdx) * 28;
      const isSelected = (i === this.selectedGameIndex);

      if (isSelected) {
        this.ctx.fillStyle = c.color;
        this.ctx.beginPath();
        this.ctx.roundRect(8, y, 304, 24, 6);
        this.ctx.fill();
        this.ctx.fillStyle = "#0b0f19";
      } else {
        this.ctx.fillStyle = "rgba(255, 255, 255, 0.04)";
        this.ctx.beginPath();
        this.ctx.roundRect(8, y, 304, 24, 6);
        this.ctx.fill();
        this.ctx.fillStyle = "#f1f2f6";
      }

      this.ctx.font = isSelected ? "bold 12px 'Outfit', sans-serif" : "12px 'Outfit', sans-serif";
      this.ctx.textAlign = "left";
      const title = games[i] || `Game ${i + 1}`;
      this.ctx.fillText(`${isSelected ? '▶ ' : '  '}${title}`, 16, y + 16);
    }

    // Footer Bar
    this.ctx.fillStyle = "#121824";
    this.ctx.fillRect(0, 214, 320, 26);
    this.ctx.fillStyle = "#a4b0be";
    this.ctx.font = "10px 'Outfit', sans-serif";
    this.ctx.textAlign = "center";
    this.ctx.fillText("A: Launch  •  B: Back  •  ◄/►: Jump ±8", 160, 231);
  }

  renderEmulationView() {
    const c = CONSOLES_LIST[this.selectedConsoleIndex];

    // Simulated Emulation Screen
    this.ctx.fillStyle = "#000000";
    this.ctx.fillRect(0, 0, 320, 240);

    // Render retro viewport simulation
    this.ctx.fillStyle = c.color;
    this.ctx.font = "bold 14px 'Press Start 2P', monospace";
    this.ctx.textAlign = "center";
    this.ctx.fillText(`EMULATING ${c.name}`, 160, 100);

    this.ctx.fillStyle = "#ffffff";
    this.ctx.font = "11px 'Press Start 2P', monospace";
    this.ctx.fillText("60 FPS • SPI DMA ACTIVE", 160, 130);

    this.ctx.fillStyle = "#feca57";
    this.ctx.font = "10px 'Outfit', sans-serif";
    this.ctx.fillText("Press SELECT or B to Return to Menu", 160, 160);
  }
}

// Instantiate Simulator on DOM Load
window.addEventListener("DOMContentLoaded", () => {
  window.simulator = new BmoSimulator();
});
