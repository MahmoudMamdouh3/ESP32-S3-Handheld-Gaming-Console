/**
 * Internal electronics stack, fasteners, 3D wire harnesses, and planned expansion
 * assemblies for the BMO Handheld interactive teardown view.
 */

import { DEFAULT_HARDWARE_LAYOUT, resolveLayoutDimensions } from "./js/hardware_layout_data.js";
import { BUILD_GUIDE_STEPS, DEFAULT_LAYOUT_DIMS, WIRE_DEFINITIONS, getThree, makeBuildGuideSteps } from "./js/internals_3d_data.js";
import { buildDisplayModule, buildFasteners, buildPerfboardGrid, buildSwitchPlane } from "./js/internals_3d_board.js";
import { buildEsp32DevKitC1, buildExpansionLayer, buildMicroSdModule, buildWireHarness } from "./js/internals_3d_mcu.js";

export class BmoInternals3D {
  constructor(studioRoot, layout = DEFAULT_HARDWARE_LAYOUT) {
    const THREE = getThree();
    this.parentRoot = studioRoot;
    this.layout = layout;
    this.layoutDims = resolveLayoutDimensions(layout);
    this.rootGroup = new THREE.Group();
    this.rootGroup.name = "BMO_Internals_Root";
    this.parentRoot.add(this.rootGroup);

    this.layers = {
      fastenersFront: new THREE.Group(),
      display: new THREE.Group(),
      switches: new THREE.Group(),
      perfboard: new THREE.Group(),
      esp32: new THREE.Group(),
      microsd: new THREE.Group(),
      wires: new THREE.Group(),
      expansion: new THREE.Group(),
    };
    Object.entries(this.layers).forEach(([k, g]) => {
      g.name = `layer_${k}`;
      this.rootGroup.add(g);
    });

    this.layerVisibility = {
      shells: true, screws: true, display: true, switches: true,
      perfboard: true, esp32: true, microsd: true, wires: true, expansion: false,
    };
    this.explodeTarget = 0.0;
    this.explodeCurrent = 0.0;
    this.selectedHighlight = "all";
    this.interactiveParts = [];
    this.wireMeshes = [];
    this.screwItems = [];
    this.partGroupsByTag = new Map();
    this.forbiddenMarkerGroup = new THREE.Group();
    this.init();
  }

  init() {
    this.buildAllInternals();
    this.setLayerVisible("expansion", false);
    this.applyExplosion(0.0);
  }

  dispose() {
    if (!this.rootGroup) return;
    this.rootGroup.traverse((child) => {
      if (child.geometry && typeof child.geometry.dispose === "function") child.geometry.dispose();
      const mats = Array.isArray(child.material) ? child.material : [child.material];
      for (const mat of mats) {
        if (!mat) continue;
        if (mat.map && typeof mat.map.dispose === "function") mat.map.dispose();
        if (typeof mat.dispose === "function") mat.dispose();
      }
    });
    if (this.parentRoot && this.rootGroup.parent === this.parentRoot) this.parentRoot.remove(this.rootGroup);
    this.interactiveParts = [];
    this.wireMeshes = [];
    this.screwItems = [];
    this.partGroupsByTag.clear();
  }

  registerInteractive(mesh, metadata) {
    mesh.userData = Object.assign({}, mesh.userData || {}, metadata, { isInternalPart: true });
    this.interactiveParts.push(mesh);
    if (metadata.groupTag) {
      if (!this.partGroupsByTag.has(metadata.groupTag)) this.partGroupsByTag.set(metadata.groupTag, []);
      this.partGroupsByTag.get(metadata.groupTag).push(mesh);
    }
  }

  buildAllInternals() {
    buildFasteners(this);
    buildDisplayModule(this);
    buildSwitchPlane(this);
    buildPerfboardGrid(this);
    buildEsp32DevKitC1(this);
    buildMicroSdModule(this);
    buildWireHarness(this);
    buildExpansionLayer(this);
  }

  setLayerVisible(layerName, visible) {
    const shown = Boolean(visible);
    if (layerName in this.layerVisibility) this.layerVisibility[layerName] = shown;
    const layerMap = {
      screws: this.layers.fastenersFront,
      display: this.layers.display,
      switches: this.layers.switches,
      perfboard: this.layers.perfboard,
      esp32: this.layers.esp32,
      microsd: this.layers.microsd,
      wires: this.layers.wires,
      expansion: this.layers.expansion,
    };
    if (layerMap[layerName]) layerMap[layerName].visible = shown;
  }

  setHighlight(targetTag) {
    this.selectedHighlight = targetTag || "all";
    const isAll = this.selectedHighlight === "all";
    const isForbidden = this.selectedHighlight === "forbidden";
    if (this.forbiddenMarkerGroup) {
      this.forbiddenMarkerGroup.visible = isForbidden || this.selectedHighlight === "esp32";
    }
    for (const wireMesh of this.wireMeshes) {
      const ud = wireMesh.userData;
      const match = isAll || ud.wireGroup === this.selectedHighlight || ud.wireId === this.selectedHighlight;
      wireMesh.material.opacity = (isAll || match) ? 1.0 : 0.14;
      wireMesh.material.emissiveIntensity = isAll ? 0.08 : (match ? 0.65 : 0.0);
    }
  }

  setExplodeTarget(t) {
    this.explodeTarget = Math.max(0, Math.min(1, Number(t) || 0));
  }

  update(dt = 0.016) {
    const diff = this.explodeTarget - this.explodeCurrent;
    if (Math.abs(diff) > 0.0005) {
      const rate = 1 - Math.exp(-9.0 * dt);
      this.explodeCurrent += diff * rate;
      this.applyExplosion(this.explodeCurrent);
    }
  }

  applyExplosion(t) {
    this.explodeCurrent = t;
    const screwT = Math.min(1, t / 0.30);
    const easeScrew = 1 - Math.pow(1 - screwT, 3);
    for (const item of this.screwItems) {
      const extraPull = item.kind === "door" ? 7.4 : 6.0;
      item.group.position.z = item.baseZ - easeScrew * extraPull;
      item.group.rotation.z = -easeScrew * Math.PI * 10.0;
    }
    const stackT = Math.max(0, (t - 0.16) / 0.84);
    const easeStack = stackT < 0.5 ? 2 * stackT * stackT : 1 - Math.pow(-2 * stackT + 2, 2) / 2;
    this.layers.display.position.z = 2.60 * easeStack;
    this.layers.switches.position.z = 1.25 * easeStack;
    this.layers.perfboard.position.z = 0.0;
    this.layers.esp32.position.z = -1.75 * easeStack;
    this.layers.microsd.position.z = -1.15 * easeStack;
    this.layers.microsd.position.x = 0.0;
    this.layers.wires.position.z = 0.0;
    this.layers.wires.scale.set(1, 1, 1 + 0.55 * easeStack);
    this.layers.expansion.position.z = -3.35 * easeStack;
  }

  getShellAndDoorOffsets(t) {
    const doorT = Math.min(1, t / 0.35);
    const easeDoor = 1 - Math.pow(1 - doorT, 2.5);
    const shellT = Math.max(0, (t - 0.12) / 0.88);
    const easeShell = 1 - Math.pow(1 - shellT, 2.8);
    return {
      frontShellZ: 5.40 * easeShell,
      buttonsZ: 4.10 * easeShell,
      backShellZ: -4.40 * easeShell,
      batteryDoorZ: -4.40 * easeShell - 1.65 * easeDoor,
    };
  }

  getStaticStats() {
    const dims = (this && this.layoutDims) || DEFAULT_LAYOUT_DIMS;
    return {
      shellOuterMm: `${Math.round(dims.shellWidthCm * 10)} x ${Math.round(dims.shellHeightCm * 10)} x ${Math.round(dims.shellDepthCm * 10)}`,
      cavityInnerMm: `${(dims.innerWidthCm * 10).toFixed(1)} x ${(dims.innerHeightCm * 10).toFixed(1)} x ${(dims.innerDepthCm * 10).toFixed(1)}`,
      perfboardMm: `${(dims.perfboardWidthCm * 10).toFixed(2)} x ${(dims.perfboardHeightCm * 10).toFixed(2)} x ${(dims.perfboardThicknessCm * 10).toFixed(1)} (${dims.perfboardCols}x${dims.perfboardRows})`,
      perfboardCols: dims.perfboardCols,
      perfboardRows: dims.perfboardRows,
      totalVias: dims.totalVias,
      esp32Mm: "25.40 x 62.74 x 4.8",
      microsdMm: "24 x 42 (y=-34 mm)",
      displayMm: "67 x 43 carrier (2.4\" ST7789)",
      rearAvailZMm: `+${(dims.rearAvailDepthCm * 10).toFixed(1)} mm rear stack`,
    };
  }
}

BmoInternals3D.BUILD_GUIDE_STEPS = BUILD_GUIDE_STEPS;
BmoInternals3D.WIRE_DEFINITIONS = WIRE_DEFINITIONS;

export { BUILD_GUIDE_STEPS, WIRE_DEFINITIONS, makeBuildGuideSteps };
