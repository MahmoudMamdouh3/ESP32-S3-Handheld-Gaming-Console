/**
 * simulator/js/webserial.js
 * WebSerial port connection helper and browser ROM upload/dropzone ingestion.
 */
import { CONSOLES_LIST, MAX_BROWSER_ROM_SIZE } from "./constants.js";

const EXT_TO_SYSTEM = {
  ".gb": { systemId: "ROM_GB", console: "GB" },
  ".gbc": { systemId: "ROM_GBC", console: "GBC" },
  ".nes": { systemId: "ROM_NES", console: "NES" },
  ".sms": { systemId: "ROM_SMS", console: "SMS" },
  ".gg": { systemId: "ROM_GG", console: "GG" },
  ".a26": { systemId: "ROM_ATARI", console: "ATARI" },
  ".col": { systemId: "ROM_COLEM", console: "COLEM" },
};

export const webserialMethods = {
    async connectWebSerial() {
      const statusEl = document.getElementById("webserial-status");
      const logEl = document.getElementById("webserial-log");
      if (!("serial" in navigator)) {
        if (logEl) logEl.textContent = "WebSerial API requires Chrome / Edge desktop browser.";
        return false;
      }
      try {
        const navSerial = /** @type {any} */ (navigator).serial;
        this.serialPort = await navSerial.requestPort();
        await this.serialPort.open({ baudRate: 115200 });
        if (statusEl) statusEl.textContent = "Connected";
        if (logEl) logEl.textContent = "ESP32-S3 serial port opened. This page does not send firmware commands or parse telemetry.";
        return true;
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        if (logEl) logEl.textContent = `Serial connection cancelled: ${message}`;
        return false;
      }
    },

    async ingestUploadedRoms(fileList) {
      if (!fileList || fileList.length === 0) return 0;
      const added = [];
      const rejected = [];
      for (const file of Array.from(fileList)) {
        const dot = file.name.lastIndexOf(".");
        const ext = dot >= 0 ? file.name.slice(dot).toLowerCase() : "";
        const route = EXT_TO_SYSTEM[ext];
        if (!route) {
          rejected.push(`${file.name}: browser gameplay supports .gb, .gbc, .nes, .sms, .gg, .a26, and .col ROMs.`);
          continue;
        }
        const { systemId, console: consoleBadge } = route;
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
            console: consoleBadge,
            systemId,
            isFav: false,
            isPopular: false,
            romBytes: bytes
          };
          this.gameLibrary[systemId].unshift(entry);
          added.push(entry);
        } catch (error) {
          const message = error instanceof Error ? error.message : String(error);
          rejected.push(`${file.name}: ${message}`);
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
  };
