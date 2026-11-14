/**
 * simulator/js/museum.js
 * Console Museum sidebar inspector, TFT modal renderer, and pixel-art illustrations.
 */
(function (root) {
  "use strict";

  root.BmoSimModules = root.BmoSimModules || {};

  root.BmoSimModules.museumMethods = {
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
    },

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
    },

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
  };
}(typeof globalThis !== "undefined" ? globalThis : window));
