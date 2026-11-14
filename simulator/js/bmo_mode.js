/**
 * simulator/js/bmo_mode.js
 * Firmware BMO Companion expression, gaze, dance, quote, and HUD helpers.
 */
(function (root) {
  "use strict";

  root.BmoSimModules = root.BmoSimModules || {};

  root.BmoSimModules.bmoModeMethods = {
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
    },

    enterBmoMode() {
      this.enterBmoCompanion(true, true);
    },

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
    },

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
    },

    requireBmoEngine() {
      const engine = this.wasmEngine;
      if (engine && engine.bmoReady) return engine;
      const message = this.engineLoadFailed
        ? (this.engineLoadError || "The firmware BMO WebAssembly core failed to load.")
        : "The firmware BMO companion is still loading.";
      this.showSimulatorMessage(message, this.engineLoadFailed);
      return null;
    },

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
    },

    setBmoModeGaze(gx, gy) {
      const clampedX = Math.max(-1, Math.min(1, Number(gx) || 0));
      const clampedY = Math.max(-1, Math.min(1, Number(gy) || 0));
      this.bmoGazeX = clampedX;
      this.bmoGazeY = clampedY;
      this.updateBmoGazePadVisual(clampedX, clampedY);
      this.runBmoAction((engine) => engine.setBmoGaze(clampedX, clampedY));
    },

    updateBmoGazePadVisual(gx, gy) {
      const dot = document.getElementById("bmo-gaze-dot");
      if (dot) {
        dot.style.left = `${((gx + 1) * 50).toFixed(1)}%`;
        dot.style.top = `${((1 - gy) * 50).toFixed(1)}%`;
      }
      const label = document.getElementById("val-bmo-gaze");
      if (label) label.textContent = `${gx.toFixed(2)}, ${gy.toFixed(2)}`;
    },

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
    },

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
    },

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
    },

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
    },

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
  };
}(typeof globalThis !== "undefined" ? globalThis : window));
