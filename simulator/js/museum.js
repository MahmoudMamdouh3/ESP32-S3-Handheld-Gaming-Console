/**
 * simulator/js/museum.js
 * Console Museum sidebar inspector, TFT modal renderer, and pixel-art illustrations.
 */
import { CONSOLE_HISTORY_DATABASE } from "../console_history_data.js";
import { CONSOLES_LIST } from "./constants.js";
import { PIXEL_ART_PAINTERS, drawGenericCartridgeIcon } from "./museum_icons.js";

function fitMuseumText(ctx, text, maxWidth) {
  let shown = String(text || "");
  if (ctx.measureText(shown).width <= maxWidth) return shown;
  while (shown.length > 3 && ctx.measureText(`${shown}...`).width > maxWidth) {
    shown = shown.slice(0, -1);
  }
  return shown.length > 0 ? `${shown}...` : "";
}

function wrapMuseumLines(ctx, text, maxWidth, maxLines) {
  const words = String(text || "").split(/\s+/).filter(Boolean);
  const lines = [];
  let current = "";
  for (const word of words) {
    const candidate = current ? `${current} ${word}` : word;
    if (ctx.measureText(candidate).width <= maxWidth) {
      current = candidate;
    } else {
      if (current) lines.push(current);
      if (lines.length >= maxLines) {
        current = "";
        break;
      }
      current = word;
    }
  }
  if (current && lines.length < maxLines) lines.push(current);
  return lines;
}

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
      const ctx = this.ctx;
      const c = CONSOLES_LIST[this.selectedConsoleIndex];
      const data = CONSOLE_HISTORY_DATABASE[c.id];
      ctx.save();

      // Background overlay
      ctx.fillStyle = "rgba(10, 14, 23, 0.95)";
      ctx.fillRect(0, 0, 320, 240);

      // Museum Card Frame
      ctx.fillStyle = "#151d2a";
      ctx.beginPath();
      ctx.roundRect(10, 10, 300, 220, 10);
      ctx.fill();
      ctx.strokeStyle = c.color;
      ctx.lineWidth = 1.5;
      ctx.stroke();

      // Title
      ctx.fillStyle = "#ffffff";
      ctx.font = "bold 13px 'Outfit', sans-serif";
      ctx.textAlign = "left";
      ctx.fillText(fitMuseumText(ctx, `${c.name} (${c.year})`, 210), 22, 32);

      ctx.fillStyle = c.color;
      ctx.font = "bold 9px 'Press Start 2P', monospace";
      ctx.textAlign = "right";
      ctx.fillText("MUSEUM", 295, 30);

      // Separator line
      ctx.strokeStyle = "rgba(255,255,255,0.1)";
      ctx.beginPath();
      ctx.moveTo(22, 42);
      ctx.lineTo(298, 42);
      ctx.stroke();

      // Restore left alignment before rendering all spec and body rows.
      ctx.textAlign = "left";

      if (data) {
        // Hardware Specs Box
        ctx.fillStyle = "#1e272e";
        ctx.beginPath();
        ctx.roundRect(22, 50, 276, 52, 6);
        ctx.fill();

        ctx.font = "10px 'JetBrains Mono', monospace";
        ctx.fillStyle = "#feca57";
        ctx.fillText(fitMuseumText(ctx, `CPU : ${data.specs.cpu || "Custom Architecture"}`, 264), 28, 66);
        ctx.fillStyle = "#00d2d3";
        ctx.fillText(fitMuseumText(ctx, `RAM : ${data.specs.ram || "8 KB"} | Res: ${data.specs.resolution || "256x224"}`, 264), 28, 80);
        ctx.fillStyle = "#ff9ff3";
        ctx.fillText(fitMuseumText(ctx, `Audio: ${data.specs.sound || "Stereo PSG/FM"}`, 264), 28, 94);

        // Historical Breakthrough Text
        ctx.fillStyle = "#dcdde1";
        ctx.font = "10px 'Outfit', sans-serif";
        const bodyLines = wrapMuseumLines(ctx, data.historicalSignificance || "", 274, 5);
        let yPos = 118;
        for (const textLine of bodyLines) {
          ctx.fillText(textLine, 22, yPos);
          yPos += 14;
        }

        // Hallmark Game Highlight
        if (data.hallmarkGames && data.hallmarkGames.length > 0) {
          const hg = data.hallmarkGames[0];
          ctx.fillStyle = "#feca57";
          ctx.font = "bold 10px 'Outfit', sans-serif";
          ctx.fillText(fitMuseumText(ctx, `Key Landmark: ${hg.title} (${hg.year})`, 274), 22, 198);
        }
      }

      // Dismiss Prompt
      ctx.fillStyle = "#a4b0be";
      ctx.font = "9px 'Press Start 2P', monospace";
      ctx.textAlign = "center";
      ctx.fillText("PRESS B TO RETURN", 160, 220);
      ctx.restore();
    }
  };
