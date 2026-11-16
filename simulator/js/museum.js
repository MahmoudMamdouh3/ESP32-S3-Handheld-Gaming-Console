/**
 * simulator/js/museum.js
 * Console Museum sidebar inspector, TFT modal renderer, and pixel-art illustrations.
 */
import { CONSOLE_HISTORY_DATABASE } from "../console_history_data.js";
import { CONSOLES_LIST } from "./constants.js";
import { PIXEL_ART_PAINTERS, drawGenericCartridgeIcon } from "./museum_icons.js";

export const museumMethods = {
    updateSidebarMuseumView(historyDb = CONSOLE_HISTORY_DATABASE, consolesList = CONSOLES_LIST) {
      const currentConsole = consolesList[this.selectedConsoleIndex];
      const data = currentConsole ? historyDb[currentConsole.id] : null;
      if (!data) return;

      const titleEl = document.getElementById("museum-sidebar-title");
      const taglineEl = document.getElementById("museum-sidebar-tagline");
      const cpuEl = document.getElementById("museum-sidebar-cpu");
      const ramEl = document.getElementById("museum-sidebar-ram");
      const resEl = document.getElementById("museum-sidebar-res");
      const historyEl = document.getElementById("museum-sidebar-history");
      const gamesContainer = document.getElementById("museum-sidebar-games");

      if (titleEl) titleEl.textContent = `${data.name} (${data.year})`;
      if (taglineEl) taglineEl.textContent = data.tagline;
      if (cpuEl) cpuEl.textContent = data.specs.cpu || "N/A";
      if (ramEl) ramEl.textContent = data.specs.ram || "N/A";
      if (resEl) resEl.textContent = data.specs.resolution || "N/A";
      if (historyEl) historyEl.textContent = data.historicalSignificance;

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
      const ctx = this.ctx;
      ctx.save();
      ctx.translate(x, y);
      ctx.fillStyle = "rgba(10, 24, 30, 0.6)";
      ctx.beginPath();
      ctx.roundRect(0, 0, w, h, 8);
      ctx.fill();
      ctx.strokeStyle = "rgba(95, 180, 156, 0.3)";
      ctx.lineWidth = 1;
      ctx.stroke();

      const painter = PIXEL_ART_PAINTERS[c.id];
      if (painter) painter(ctx, c);
      else drawGenericCartridgeIcon(ctx, c);
      ctx.restore();
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
