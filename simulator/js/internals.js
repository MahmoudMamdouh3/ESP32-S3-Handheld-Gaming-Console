/**
 * simulator/js/internals.js
 * Interactive 3D teardown / internals presets, layer checkboxes, and build guide controls.
 */
(function (root) {
  "use strict";

  root.BmoSimModules = root.BmoSimModules || {};

  root.BmoSimModules.internalsMethods = {
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
    },

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
    },

    setInternalsHighlightUI(tag) {
      this.internalsHighlight = tag || "all";
      document.querySelectorAll(".internals-filter-btn").forEach((b) => {
        b.classList.toggle("active", b.getAttribute("data-highlight") === this.internalsHighlight);
      });
      if (this.bmo3D) {
        this.bmo3D.setInternalHighlight(this.internalsHighlight);
      }
    },

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
    },

    handleInternalPartSelected(meta) {
      const inspector = document.getElementById("internals-inspector");
      const metaEl = document.getElementById("internals-inspector-meta");
      if (!inspector || !metaEl || !meta) return;
      const titleEl = inspector.querySelector(".internals-inspector-title");
      if (titleEl) titleEl.textContent = `${meta.name} (${meta.role || "Component"})`;
      metaEl.textContent = meta.detail || "Inspecting selected assembly.";
    },

    syncExplosionReadout(t) {
      const pct = Math.round(Math.max(0, Math.min(1, Number(t) || 0)) * 100);
      const valEl = document.getElementById("val-explode");
      const sliderEl = document.getElementById("slider-explode");
      if (valEl) valEl.textContent = `${pct}%`;
      if (sliderEl && Math.abs(parseInt(sliderEl.value, 10) - pct) >= 1) {
        sliderEl.value = String(pct);
      }
    },

    syncLayerCheckboxes() {
      if (!this.bmo3D) return;
      const internalsVis = this.bmo3D.internals ? this.bmo3D.internals.layerVisibility : {};
      document.querySelectorAll(".internals-layer-chk").forEach((chk) => {
        const layer = chk.getAttribute("data-layer");
        if (!layer) return;
        if (layer in this.bmo3D.layerVis) chk.checked = Boolean(this.bmo3D.layerVis[layer]);
        else if (layer in internalsVis) chk.checked = Boolean(internalsVis[layer]);
      });
    },

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
  };
}(typeof globalThis !== "undefined" ? globalThis : window));
