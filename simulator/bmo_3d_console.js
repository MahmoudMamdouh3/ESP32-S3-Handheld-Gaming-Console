/**
 * Interactive Three.js console view. Dimensions are a design model in centimetres;
 * shell and perfboard parameters are driven dynamically from hardware/layout.json.
 */
import { BmoInternals3D } from "./bmo_internals_3d.js";
import { buildConsoleModelImpl } from "./js/console_3d_model.js";
import {
  applyStudioAdjustmentsImpl,
  createButtonTextureImpl,
  createDpadTextureImpl,
  createDropShadowTextureImpl,
  decodeCadMeshImpl,
  setupLightingImpl,
  setupStudioEnvironment,
} from "./js/console_3d_studio.js";
import { DEFAULT_HARDWARE_LAYOUT, resolveLayoutDimensions } from "./js/hardware_layout_data.js";

const THREE = /** @type {any} */ (globalThis).THREE;

export class Bmo3DConsole {
  constructor(container, tftCanvas, onButtonPress, onButtonRelease, layout = DEFAULT_HARDWARE_LAYOUT) {
    this.container = container;
    this.tftCanvas = tftCanvas;
    this.onButtonPress = onButtonPress || (() => {});
    this.onButtonRelease = onButtonRelease || (() => {});
    this.layout = layout || DEFAULT_HARDWARE_LAYOUT;
    this.layoutDims = resolveLayoutDimensions(this.layout);
    this._rafId = null;
    this._onResizeBound = null;
    this._onPointerDownBound = null;
    this._onPointerUpBound = null;
    this._onPointerMoveBound = null;

    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.controls = null;
    this.screenMesh = null;
    this.screenTexture = null;

    // Interactive button meshes mapped by simulator key name
    this.buttonMeshes = {};
    this.interactiveObjects = [];
    this.pressedButtons = new Set();
    this.buttonOriginalPositions = new Map();

    // D-Pad pivot tilt state
    this.dpadPivotGroup = null;
    this.dpadTargetRotation = { x: 0, y: 0, z: 0 };
    this.dpadTargetZ = 0;

    // Theme materials
    this.materials = {};
    this.activeTheme = "bmo-teal";

    // Assembly / Dissect view state
    this.frontShellGroup = null;
    this.buttonsGroup = null;
    this.backShellGroup = null;
    this.batteryDoorGroup = null;
    this.internals = null;
    this.explodeTarget = 0.0;
    this.explodeCurrent = 0.0;
    this.autoPlayDisassembly = false;
    this.autoPlayPhase = 0.0;
    this.lastFrameTime = performance.now();
    this.onInternalSelect = null;
    this.onExplosionChange = null;
    this.onScreenClick = null;
    /** @type {any[] | null} */
    this.acousticFeltMeshes = null;
    this.dpadRestZ = 1.42;
    this.layerVis = {
      shells: true,
      frontShell: true,
      backShell: true,
      buttons: true,
    };

    // Studio Lighting, Saturation & Temperature State
    this.ambientLight = null;
    this.keyLight = null;
    this.rimLight = null;
    this.studioSettings = {
      brightness: 1.0,
      saturation: 1.0,
      temperature: 0.0,
    };

    // Raycasting state
    this.raycaster = new THREE.Raycaster();
    this.mouse = new THREE.Vector2();
    this.activePointerButton = null;
    this._tmpColor = new THREE.Color();
    this._lastHoverRaycastMs = 0;
    /** @type {(() => boolean) | null} */
    this.shouldUpdateScreenTexture = null;

    this.init();
  }

  init() {
    const width = this.container.clientWidth || 480;
    const height = this.container.clientHeight || 660;

    // 1. Scene
    this.scene = new THREE.Scene();

    // 2. Camera: 42 deg FOV frames full chassis including L/R shoulders and SELECT/START pills
    this.camera = new THREE.PerspectiveCamera(42, width / height, 0.1, 100);
    this.setCameraPreset("front");

    // Use an sRGB output transform for display and canvas textures.
    this.renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      powerPreference: "high-performance",
    });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

    if ("outputEncoding" in this.renderer) this.renderer.outputEncoding = THREE.sRGBEncoding;
    this.renderer.toneMapping = THREE.NoToneMapping;
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;

    this.container.appendChild(this.renderer.domElement);

    // 4. Orbit Controls
    if (typeof THREE.OrbitControls !== "undefined") {
      this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.08;
      this.controls.minDistance = 9;
      this.controls.maxDistance = 32;
      this.controls.target.set(0, 0, 0);
    }

    // 5. Studio Lighting Rig: Normalized to 1.00 front-face irradiance
    this.setupLighting();

    // 6. Build the 3D Console Model strictly according to Reference Spec & 2D Palette
    this.buildConsoleModel();

    // 7. Raycasting & Interaction
    this.setupRaycasting();

    // 8. Window Resize Handler
    this._onResizeBound = () => this.onResize();
    window.addEventListener("resize", this._onResizeBound);

    // 9. Render Loop
    this.animate = this.animate.bind(this);
    this._rafId = requestAnimationFrame(this.animate);
  }

  dispose() {
    if (this._rafId !== null) {
      cancelAnimationFrame(this._rafId);
      this._rafId = null;
    }
    if (this._onResizeBound) {
      window.removeEventListener("resize", this._onResizeBound);
      this._onResizeBound = null;
    }
    const dom = this.renderer ? this.renderer.domElement : null;
    if (dom && this._onPointerDownBound) {
      dom.removeEventListener("pointerdown", this._onPointerDownBound);
      this._onPointerDownBound = null;
    }
    if (this._onPointerUpBound) {
      window.removeEventListener("pointerup", this._onPointerUpBound);
      this._onPointerUpBound = null;
    }
    if (dom && this._onPointerMoveBound) {
      dom.removeEventListener("pointermove", this._onPointerMoveBound);
      this._onPointerMoveBound = null;
    }
    if (this.internals && typeof this.internals.dispose === "function") {
      this.internals.dispose();
    }
    if (this.controls && typeof this.controls.dispose === "function") {
      this.controls.dispose();
    }
    if (this.scene) {
      this.scene.traverse((obj) => {
        if (obj.geometry && typeof obj.geometry.dispose === "function") obj.geometry.dispose();
        const mats = Array.isArray(obj.material) ? obj.material : [obj.material];
        for (const mat of mats) {
          if (!mat) continue;
          if (mat.map && typeof mat.map.dispose === "function") mat.map.dispose();
          if (typeof mat.dispose === "function") mat.dispose();
        }
      });
    }
    if (this.screenTexture && typeof this.screenTexture.dispose === "function") {
      this.screenTexture.dispose();
      this.screenTexture = null;
    }
    if (this.renderer) {
      if (dom && dom.parentNode) dom.parentNode.removeChild(dom);
      this.renderer.dispose();
      this.renderer = null;
    }
  }

  setupLighting() { setupLightingImpl(this); }

  setupStudioEnvironment() { setupStudioEnvironment(this.scene, this.renderer); }

  setStudioAdjustments(settings = {}) {
    if (settings.brightness !== undefined) this.studioSettings.brightness = settings.brightness;
    if (settings.saturation !== undefined) this.studioSettings.saturation = settings.saturation;
    if (settings.temperature !== undefined) this.studioSettings.temperature = settings.temperature;
    this.applyStudioAdjustments();
  }

  applyStudioAdjustments() { applyStudioAdjustmentsImpl(this); }

  createDropShadowTexture(radius = 64) { return createDropShadowTextureImpl(radius); }

  createButtonTexture(label, bgColorStr, textColorStr) { return createButtonTextureImpl(label, bgColorStr, textColorStr); }

  createDpadTexture() { return createDpadTextureImpl(); }

  decodeCadMesh(partName) { return decodeCadMeshImpl(partName, this.layoutDims); }

  buildConsoleModel() { buildConsoleModelImpl(this); }
  isObjectChainVisible(obj) {
    let curr = obj;
    while (curr) {
      if (curr.visible === false) return false;
      curr = curr.parent;
    }
    return true;
  }

  setupRaycasting() {
    const dom = this.renderer.domElement;
    /** @type {{ x: number, y: number } | null} */
    let pointerDownPos = null;

    this._onPointerDownBound = (e) => {
      pointerDownPos = { x: e.clientX, y: e.clientY };
      const btn = this.getIntersectedButton(e);
      if (btn) {
        this.activePointerButton = btn.userData.key;
        if (this.controls) this.controls.enabled = false;
        this.pressButton(this.activePointerButton, true);
      }
    };
    dom.addEventListener("pointerdown", this._onPointerDownBound);

    this._onPointerUpBound = (e) => {
      if (this.controls) this.controls.enabled = true;
      if (this.activePointerButton) {
        this.releaseButton(this.activePointerButton, true);
        this.activePointerButton = null;
        pointerDownPos = null;
        return;
      }
      if (pointerDownPos && e.target === dom) {
        const dist = Math.hypot(e.clientX - pointerDownPos.x, e.clientY - pointerDownPos.y);
        if (dist < 6) {
          const hit = this.internals ? this.getIntersectedInternal(e) : null;
          if (hit && hit.userData && hit.userData.isInternalPart) {
            const tag = hit.userData.wireId || hit.userData.groupTag || "all";
            this.internals.setHighlight(tag);
            if (typeof this.onInternalSelect === "function") {
              this.onInternalSelect(hit.userData);
            }
          } else if (this.screenMesh && typeof this.onScreenClick === "function") {
            const rect = dom.getBoundingClientRect();
            this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
            this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
            this.raycaster.setFromCamera(this.mouse, this.camera);
            if (this.raycaster.intersectObject(this.screenMesh, false).length > 0) {
              this.onScreenClick();
            }
          }
        }
      }
      pointerDownPos = null;
    };
    window.addEventListener("pointerup", this._onPointerUpBound);

    this._onPointerMoveBound = (e) => {
      const nowMs = performance.now();
      if (nowMs - this._lastHoverRaycastMs < 32) return;
      this._lastHoverRaycastMs = nowMs;
      const btn = this.getIntersectedButton(e);
      if (btn) {
        dom.style.cursor = "pointer";
        return;
      }
      const internal = this.internals && (this.explodeCurrent > 0.02 || !this.layerVis.frontShell || !this.layerVis.backShell)
        ? this.getIntersectedInternal(e)
        : null;
      dom.style.cursor = internal ? "pointer" : "default";
    };
    dom.addEventListener("pointermove", this._onPointerMoveBound);
  }

  getIntersectedButton(e) {
    const rect = this.renderer.domElement.getBoundingClientRect();
    this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

    this.raycaster.setFromCamera(this.mouse, this.camera);
    const intersects = this.raycaster.intersectObjects(this.interactiveObjects, false);

    for (const hit of intersects) {
      if (this.isObjectChainVisible(hit.object)) {
        return hit.object;
      }
    }
    return null;
  }

  getIntersectedInternal(e) {
    if (!this.internals || !this.internals.interactiveParts.length) return null;
    const rect = this.renderer.domElement.getBoundingClientRect();
    this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

    this.raycaster.setFromCamera(this.mouse, this.camera);
    const intersects = this.raycaster.intersectObjects(this.internals.interactiveParts, false);
    for (const hit of intersects) {
      if (this.isObjectChainVisible(hit.object)) {
        return hit.object;
      }
    }
    return null;
  }

  pressButton(key, triggerCallback = false) {
    this.pressedButtons.add(key);

    // 1. D-Pad pivot mechanics
    if (key === "UP") {
      this.dpadTargetRotation.x = -0.07;
      this.dpadTargetZ = -0.06;
    } else if (key === "DOWN") {
      this.dpadTargetRotation.x = 0.07;
      this.dpadTargetZ = -0.06;
    } else if (key === "LEFT") {
      this.dpadTargetRotation.y = -0.07;
      this.dpadTargetZ = -0.06;
    } else if (key === "RIGHT") {
      this.dpadTargetRotation.y = 0.07;
      this.dpadTargetZ = -0.06;
    }

    // 2. Face & shoulder button mechanical depression
    const mesh = this.buttonMeshes[key];
    if (mesh) {
      const orig = this.buttonOriginalPositions.get(mesh);
      if (orig) {
        if (mesh.userData.isShoulder) {
          mesh.position.y = orig.y - 0.16; // Tactile downward click
          mesh.rotation.x = -0.06;         // Subtle microswitch tilt
        } else if (mesh.userData.isPill) {
          mesh.position.z = orig.z - 0.12; // Pill button depression
        } else {
          mesh.position.z = orig.z - 0.14; // Depress face button into body
        }
      }
    }

    if (triggerCallback && this.onButtonPress) {
      this.onButtonPress(key);
    }
  }

  releaseButton(key, triggerCallback = false) {
    this.pressedButtons.delete(key);

    // 1. D-Pad return to center
    if (key === "UP" || key === "DOWN" || key === "LEFT" || key === "RIGHT") {
      if (this.pressedButtons.has("UP")) {
        this.dpadTargetRotation.x = -0.07;
      } else if (this.pressedButtons.has("DOWN")) {
        this.dpadTargetRotation.x = 0.07;
      } else {
        this.dpadTargetRotation.x = 0;
      }

      if (this.pressedButtons.has("LEFT")) {
        this.dpadTargetRotation.y = -0.07;
      } else if (this.pressedButtons.has("RIGHT")) {
        this.dpadTargetRotation.y = 0.07;
      } else {
        this.dpadTargetRotation.y = 0;
      }

      if (!this.pressedButtons.has("UP") && !this.pressedButtons.has("DOWN") &&
          !this.pressedButtons.has("LEFT") && !this.pressedButtons.has("RIGHT")) {
        this.dpadTargetZ = 0;
      }
    }

    // 2. Spring return for face and shoulder buttons
    const mesh = this.buttonMeshes[key];
    if (mesh) {
      const orig = this.buttonOriginalPositions.get(mesh);
      if (orig) {
        mesh.position.set(orig.x, orig.y, orig.z);
        mesh.rotation.set(orig.rotX || 0, orig.rotY || 0, orig.rotZ || 0);
      }
    }

    if (triggerCallback && this.onButtonRelease) {
      this.onButtonRelease(key);
    }
  }

  setTheme(themeName) {
    this.activeTheme = themeName;
    if (!this.materials.chassis) return;

    let chassisHex = 0x63c5b8;
    let bezelHex = 0x17353a;
    let roughness = 0.62;
    let transmission = 0.0;
    let transparent = false;
    let opacity = 1.0;

    switch (themeName) {
      case "dmg-gray":
        chassisHex = 0xd1ccc0;
        bezelHex = 0x57606f;
        roughness = 0.45;
        break;
      case "atomic-purple":
        chassisHex = 0x9c88ff;
        bezelHex = 0x2f3542;
        roughness = 0.20;
        transmission = 0.72;
        transparent = true;
        opacity = 0.88;
        break;
      case "oled-black":
        chassisHex = 0x1e272e;
        bezelHex = 0x111418;
        roughness = 0.25;
        break;
      case "bmo-teal":
      default:
        chassisHex = 0x63c5b8;
        bezelHex = 0x17353a;
        break;
    }

    this._tmpColor.setHex(chassisHex).convertSRGBToLinear();
    this.materials.chassis.color.copy(this._tmpColor);
    this.materials.chassis.roughness = roughness;
    this.materials.chassis.transmission = transmission;
    this.materials.chassis.transparent = transparent;
    this.materials.chassis.opacity = opacity;
    if (this.materials.backShell) {
      this.materials.backShell.color.copy(this._tmpColor);
      this.materials.backShell.roughness = roughness + 0.04;
      this.materials.backShell.transmission = transmission;
      this.materials.backShell.transparent = transparent;
      this.materials.backShell.opacity = opacity;
    }
    if (this.materials.batteryDoor) {
      this.materials.batteryDoor.color.copy(this._tmpColor);
    }

    this._tmpColor.setHex(bezelHex).convertSRGBToLinear();
    this.materials.bezel.color.copy(this._tmpColor);
  }

  setCameraPreset(presetName) {
    if (!this.camera) return;

    const targets = {
      front: { pos: [0, 0, 22.5], look: [0, 0, 0] },
      iso: { pos: [11.5, 8.5, 20], look: [0, 0, 0] },
      top: { pos: [0, 25, 0.5], look: [0, 0, 0] },
      bottom: { pos: [0, -22, 3.0], look: [0, -2.0, 0] },
      left: { pos: [-19.5, 0.2, 4.0], look: [-2.0, 0.2, 0] },
      right: { pos: [19.5, 0.2, 4.0], look: [2.0, 0.2, 0] },
      back: { pos: [0, -1.0, -20.5], look: [0, -1.0, 0] },
      core: { pos: [10.0, -1.5, -18.5], look: [0, -2.0, 0] },
      display: { pos: [9.0, 3.5, 14.5], look: [0, 2.0, 0.8] },
      unscrew: { pos: [-12.5, -3.0, -22.0], look: [0, -2.0, -1.5] },
      exploded: { pos: [19.5, 3.0, 25.5], look: [-0.4, -0.4, 0.2] },
    };

    const t = targets[presetName] || targets.front;
    this.camera.up.set(0, 1, 0);
    if (presetName === "top") this.camera.up.set(0, 0, 1);
    else if (presetName === "bottom") this.camera.up.set(0, 0, -1);
    this.camera.position.set(...t.pos);
    if (this.controls) {
      this.controls.target.set(...t.look);
      this.controls.update();
    }
  }

  setExplode(value01) {
    this.autoPlayDisassembly = false;
    this.explodeTarget = Math.max(0, Math.min(1, Number(value01) || 0));
    if (this.internals) {
      this.internals.setExplodeTarget(this.explodeTarget);
    }
  }

  playDisassemblySequence(toExploded = true) {
    this.autoPlayDisassembly = true;
    this.autoPlayDirection = toExploded ? 1 : -1;
    this.autoPlayPhase = this.explodeCurrent;
    if (toExploded && this.camera && Math.abs(this.camera.position.x) < 1.5 && this.camera.position.z > 15) {
      this.setCameraPreset("exploded");
    }
  }

  setLayerVisible(layerName, visible) {
    const show = Boolean(visible);
    if (layerName === "shells") {
      this.layerVis.shells = show;
      this.layerVis.frontShell = show;
      this.layerVis.backShell = show;
      if (this.frontShellGroup) this.frontShellGroup.visible = show;
      if (this.backShellGroup) this.backShellGroup.visible = show;
      if (this.batteryDoorGroup) this.batteryDoorGroup.visible = show;
    } else if (layerName === "frontShell") {
      this.layerVis.frontShell = show;
      if (this.frontShellGroup) this.frontShellGroup.visible = show;
    } else if (layerName === "backShell") {
      this.layerVis.backShell = show;
      if (this.backShellGroup) this.backShellGroup.visible = show;
      if (this.batteryDoorGroup) this.batteryDoorGroup.visible = show;
    } else if (layerName === "buttons") {
      this.layerVis.buttons = show;
      if (this.buttonsGroup) this.buttonsGroup.visible = show;
    } else if (this.internals) {
      this.internals.setLayerVisible(layerName, show);
    }
  }

  setInternalHighlight(tagOrWireId) {
    if (this.internals) {
      this.internals.setHighlight(tagOrWireId);
    }
  }

  setOnInternalSelect(cb) {
    this.onInternalSelect = cb;
  }

  applyInternalsPreset(presetName) {
    const expState = this.internals ? this.internals.layerVisibility.expansion : false;
    // Reset base component layers visible; expansion keeps user choice.
    for (const k of ["screws", "display", "switches", "perfboard", "esp32", "microsd", "wires"]) {
      this.setLayerVisible(k, true);
    }
    this.setLayerVisible("expansion", expState);

    switch (presetName) {
      case "unscrew":
        this.setLayerVisible("shells", true);
        this.setLayerVisible("buttons", true);
        this.setExplode(0.28);
        this.setInternalHighlight("all");
        this.setCameraPreset("unscrew");
        break;
      case "display":
        this.setLayerVisible("frontShell", false);
        this.setLayerVisible("backShell", true);
        this.setLayerVisible("buttons", false);
        this.setExplode(0.45);
        this.setInternalHighlight("display");
        this.setCameraPreset("display");
        break;
      case "core":
        this.setLayerVisible("frontShell", true);
        this.setLayerVisible("buttons", true);
        this.setLayerVisible("backShell", false);
        this.setExplode(0.42);
        this.setInternalHighlight("all");
        this.setCameraPreset("core");
        break;
      case "wiring":
        this.setLayerVisible("shells", false);
        this.setLayerVisible("buttons", false);
        this.setExplode(0.32);
        this.setInternalHighlight("all");
        this.setCameraPreset("core");
        break;
      case "exploded":
        this.setLayerVisible("shells", true);
        this.setLayerVisible("buttons", true);
        this.setExplode(1.0);
        this.setInternalHighlight("all");
        this.setCameraPreset("exploded");
        break;
      case "assembled":
      default:
        this.setLayerVisible("shells", true);
        this.setLayerVisible("buttons", true);
        this.setExplode(0.0);
        this.setInternalHighlight("all");
        this.setCameraPreset("iso");
        break;
    }
  }

  applyExplosionPositions(t) {
    this.explodeCurrent = t;
    if (this.internals) {
      const off = this.internals.getShellAndDoorOffsets(t);
      if (this.frontShellGroup) this.frontShellGroup.position.z = off.frontShellZ;
      if (this.buttonsGroup) this.buttonsGroup.position.z = off.buttonsZ;
      if (this.backShellGroup) this.backShellGroup.position.z = off.backShellZ;
      if (this.batteryDoorGroup) this.batteryDoorGroup.position.z = off.batteryDoorZ;
      this.internals.applyExplosion(t);
    }
    if (this.acousticFeltMeshes) {
      const showFelt = t < 0.08 && this.layerVis.frontShell && this.layerVis.backShell;
      for (const felt of this.acousticFeltMeshes) {
        felt.visible = showFelt;
      }
    }
    if (typeof this.onExplosionChange === "function") {
      this.onExplosionChange(t);
    }
  }

  updateScreenTexture() {
    if (this.screenTexture) {
      this.screenTexture.needsUpdate = true;
    }
  }

  onResize() {
    if (!this.renderer || !this.camera || !this.container) return;
    const width = this.container.clientWidth || 480;
    const height = this.container.clientHeight || 660;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }

  animate() {
    this._rafId = requestAnimationFrame(this.animate);
    const now = performance.now();
    const dt = Math.min(0.05, Math.max(0.001, (now - this.lastFrameTime) / 1000));
    this.lastFrameTime = now;

    if (this.controls) {
      this.controls.update();
    }

    // Smooth choreographed disassembly playback or damped manual slider tracking
    if (this.autoPlayDisassembly) {
      this.autoPlayPhase += (this.autoPlayDirection || 1) * dt * 0.48;
      if (this.autoPlayPhase >= 1.0) {
        this.autoPlayPhase = 1.0;
        this.explodeTarget = 1.0;
        this.autoPlayDisassembly = false;
      } else if (this.autoPlayPhase <= 0.0) {
        this.autoPlayPhase = 0.0;
        this.explodeTarget = 0.0;
        this.autoPlayDisassembly = false;
      } else {
        this.explodeTarget = this.autoPlayPhase;
      }
    }

    if (Math.abs(this.explodeTarget - this.explodeCurrent) > 0.0005) {
      const rate = 1 - Math.exp(-8.5 * dt);
      const next = this.explodeCurrent + (this.explodeTarget - this.explodeCurrent) * rate;
      this.applyExplosionPositions(next);
    } else if (this.explodeCurrent !== this.explodeTarget) {
      this.applyExplosionPositions(this.explodeTarget);
    }

    // D-Pad smooth harmonic tilt interpolation
    if (this.dpadPivotGroup) {
      this.dpadPivotGroup.rotation.x += (this.dpadTargetRotation.x - this.dpadPivotGroup.rotation.x) * 0.35;
      this.dpadPivotGroup.rotation.y += (this.dpadTargetRotation.y - this.dpadPivotGroup.rotation.y) * 0.35;
      const targetZ = (this.dpadRestZ || 1.42) + this.dpadTargetZ;
      this.dpadPivotGroup.position.z += (targetZ - this.dpadPivotGroup.position.z) * 0.35;
    }

    // Live update screen texture only when the 2D canvas painted new pixels
    if (this.screenTexture && (!this.shouldUpdateScreenTexture || this.shouldUpdateScreenTexture())) {
      this.screenTexture.needsUpdate = true;
    }

    if (this.renderer && this.scene && this.camera) {
      this.renderer.render(this.scene, this.camera);
    }
  }
}
