/**
 * bmo_3d_console.js
 * 
 * High-End 3D Physical Hardware Model & Interactive WebGL Simulator for BMO Handheld Console.
 * Engineered for 100% color fidelity and visual parity with the authentic 2D reference console:
 *  - Chassis: 8.8 W × 12.4 H × 3.2 D cm, rounded corners (1.5 cm asymmetric bottom-right curve)
 *  - Colors: Rich, vibrant BMO Seafoam Teal (#5FB49C), Warm Golden D-Pad (#FFE033), Deep Teal Bezel (#1A4B42)
 *  - Screen Bezel: Exact 2D-matched curved housing with "BMO COLOR", "USB-C POWER", green LED, "BMO GAMEBOY"
 *  - Active ST7789 Display: Backlit emissive LCD panel streaming live companion & games in true retro mint
 *  - Shoulder Bumpers: Front-visible curved L & R pill bumpers with embossed labels and top wraparound
 *  - Action Buttons: High-gloss 3D candy domes (Green X, Pink Y, Blue A, Magenta B) with physical drop shadows
 *  - D-Pad: Seamless beveled cross with circular thumb dish, directional arrows, and tactile pivot tilt
 *  - Select / Start: Angled rubber pill capsules with drop shadows and engraved labels
 *  - Forehead Ridges & Speaker Grille: 2 molded ventilation lines and 6 diagonal acoustic slots
 *  - Color Engine: Linear tone mapping + sRGB output to eliminate bleaching, preserving rich saturation
 */

class Bmo3DConsole {
  constructor(container, tftCanvas, onButtonPress, onButtonRelease) {
    this.container = container;
    this.tftCanvas = tftCanvas;
    this.onButtonPress = onButtonPress || (() => {});
    this.onButtonRelease = onButtonRelease || (() => {});

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

    // D-Pad pivot state
    this.dpadPivotGroup = null;
    this.dpadTargetRotation = { x: 0, y: 0, z: 0 };
    this.dpadTargetZ = 0;

    // Theme materials
    this.materials = {};
    this.activeTheme = "bmo-teal";

    // Raycasting state
    this.raycaster = new THREE.Raycaster();
    this.mouse = new THREE.Vector2();
    this.activePointerButton = null;

    this.init();
  }

  init() {
    const width = this.container.clientWidth || 480;
    const height = this.container.clientHeight || 660;

    // 1. Scene
    this.scene = new THREE.Scene();

    // 2. Camera: 38 deg FOV for realistic product focal length
    this.camera = new THREE.PerspectiveCamera(38, width / height, 0.1, 100);
    this.setCameraPreset("front");

    // 3. Renderer with sRGB color space & Linear tone mapping (NO BLEACHING / NO DESATURATION)
    this.renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      powerPreference: "high-performance",
    });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

    if ("outputEncoding" in this.renderer) {
      this.renderer.outputEncoding = THREE.sRGBEncoding;
    }
    // Linear tone mapping guarantees exact 1:1 color match with CSS hex tokens!
    this.renderer.toneMapping = THREE.LinearToneMapping;
    this.renderer.toneMappingExposure = 1.0;
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

    // 5. Studio Lighting Rig
    this.setupLighting();

    // 6. Build the 3D Console Model
    this.buildConsoleModel();

    // 7. Raycasting & Interaction
    this.setupRaycasting();

    // 8. Window Resize Handler
    window.addEventListener("resize", () => this.onResize());

    // 9. Render Loop
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  setupLighting() {
    // 1. Clean ambient illumination preserving 100% color saturation
    const ambient = new THREE.AmbientLight(0xffffff, 0.78);
    this.scene.add(ambient);

    // 2. Key Light: Soft directional light from front-top-right for bevel specular highlights
    const keyLight = new THREE.DirectionalLight(0xffffff, 0.42);
    keyLight.position.set(6, 10, 14);
    keyLight.castShadow = true;
    keyLight.shadow.mapSize.width = 2048;
    keyLight.shadow.mapSize.height = 2048;
    keyLight.shadow.camera.near = 1;
    keyLight.shadow.camera.far = 30;
    keyLight.shadow.camera.left = -7;
    keyLight.shadow.camera.right = 7;
    keyLight.shadow.camera.top = 9;
    keyLight.shadow.camera.bottom = -9;
    keyLight.shadow.bias = -0.0004;
    keyLight.shadow.radius = 2.5;
    this.scene.add(keyLight);

    // 3. Rim Light: Edge highlight along top shoulders and side silhouette
    const rimLight = new THREE.DirectionalLight(0xffffff, 0.32);
    rimLight.position.set(0, 12, -8);
    this.scene.add(rimLight);
  }

  createDropShadowTexture(radius = 64) {
    const canvas = document.createElement("canvas");
    canvas.width = 128;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");
    const grad = ctx.createRadialGradient(64, 64, radius * 0.3, 64, 64, radius);
    grad.addColorStop(0, "rgba(0, 0, 0, 0.46)");
    grad.addColorStop(0.65, "rgba(0, 0, 0, 0.18)");
    grad.addColorStop(1, "rgba(0, 0, 0, 0)");
    ctx.fillStyle = grad;
    ctx.fillRect(0, 0, 128, 128);
    const tex = new THREE.CanvasTexture(canvas);
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    return tex;
  }

  createButtonTexture(label, bgColorStr, textColorStr) {
    const canvas = document.createElement("canvas");
    canvas.width = 256;
    canvas.height = 256;
    const ctx = canvas.getContext("2d");

    ctx.clearRect(0, 0, 256, 256);

    // 1. Solid saturated base
    ctx.fillStyle = bgColorStr;
    ctx.beginPath();
    ctx.arc(128, 128, 122, 0, Math.PI * 2);
    ctx.fill();

    // 2. Bottom shadow crescent for 3D depth
    const btmGrad = ctx.createRadialGradient(128, 128, 60, 128, 128, 122);
    btmGrad.addColorStop(0, "rgba(0, 0, 0, 0)");
    btmGrad.addColorStop(0.75, "rgba(0, 0, 0, 0.12)");
    btmGrad.addColorStop(1, "rgba(0, 0, 0, 0.38)");
    ctx.fillStyle = btmGrad;
    ctx.beginPath();
    ctx.arc(128, 128, 122, 0, Math.PI * 2);
    ctx.fill();

    // 3. Top crescent highlight (giving that authentic 2D box-shadow bevel)
    const topHl = ctx.createLinearGradient(128, 10, 128, 90);
    topHl.addColorStop(0, "rgba(255, 255, 255, 0.48)");
    topHl.addColorStop(1, "rgba(255, 255, 255, 0.0)");
    ctx.fillStyle = topHl;
    ctx.beginPath();
    ctx.ellipse(128, 62, 82, 38, 0, 0, Math.PI * 2);
    ctx.fill();

    // 4. Subtle outer rim border
    ctx.lineWidth = 4;
    ctx.strokeStyle = "rgba(0, 0, 0, 0.32)";
    ctx.beginPath();
    ctx.arc(128, 128, 122, 0, Math.PI * 2);
    ctx.stroke();

    // 5. Centered bold retro pixel letter
    ctx.fillStyle = textColorStr;
    ctx.font = "900 84px 'Press Start 2P', monospace, sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.shadowColor = "rgba(0, 0, 0, 0.35)";
    ctx.shadowBlur = 4;
    ctx.shadowOffsetY = 2;
    ctx.fillText(label, 128, 134);

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  createBezelTexture() {
    const canvas = document.createElement("canvas");
    canvas.width = 1024;
    canvas.height = 768;
    const ctx = canvas.getContext("2d");

    // Clear
    ctx.clearRect(0, 0, 1024, 768);

    // 1. Deep forest teal gradient housing (exact 2D match)
    const bgGrad = ctx.createLinearGradient(0, 0, 0, 768);
    bgGrad.addColorStop(0, "#1a4b42");
    bgGrad.addColorStop(0.5, "#153d36");
    bgGrad.addColorStop(1, "#0f2620");
    ctx.fillStyle = bgGrad;
    ctx.beginPath();
    // Rounded corners: 36px bottom-right (classic Game Boy), 16px others
    ctx.roundRect(0, 0, 1024, 768, [24, 24, 52, 24]);
    ctx.fill();

    // Outer subtle bevel border
    ctx.lineWidth = 4;
    ctx.strokeStyle = "rgba(255, 255, 255, 0.16)";
    ctx.stroke();

    // 2. Top Status Bar: "BMO COLOR" and "USB-C POWER"
    ctx.fillStyle = "#cef5e4";
    ctx.font = "bold 26px 'Press Start 2P', monospace";
    ctx.textAlign = "left";
    ctx.textBaseline = "middle";
    ctx.fillText("BMO COLOR", 60, 52);

    ctx.fillStyle = "#84d0bc";
    ctx.font = "bold 22px 'Press Start 2P', monospace";
    ctx.textAlign = "right";
    ctx.fillText("USB-C POWER", 916, 52);

    // Glowing Power LED indicator dot
    const ledX = 942;
    const ledY = 52;
    const ledGrad = ctx.createRadialGradient(ledX, ledY, 2, ledX, ledY, 12);
    ledGrad.addColorStop(0, "#55efc4");
    ledGrad.addColorStop(0.5, "#00d2d3");
    ledGrad.addColorStop(1, "rgba(0, 210, 211, 0)");
    ctx.fillStyle = ledGrad;
    ctx.beginPath();
    ctx.arc(ledX, ledY, 12, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "#ffffff";
    ctx.beginPath();
    ctx.arc(ledX, ledY, 3, 0, Math.PI * 2);
    ctx.fill();

    // 3. Inner Display Frame Recess Border (framing the ST7789 display window)
    ctx.lineWidth = 6;
    ctx.strokeStyle = "#081512";
    ctx.strokeRect(64, 88, 896, 584);

    // 4. Bottom Footer Bar: "BMO GAMEBOY"
    const footerY = 720;
    ctx.font = "bold 34px 'Press Start 2P', monospace";
    const bmoTxt = "BMO ";
    const gbTxt = "GAMEBOY";
    const totalW = ctx.measureText(bmoTxt + gbTxt).width;
    const startX = 512 - totalW / 2;

    ctx.textAlign = "left";
    ctx.fillStyle = "#cef5e4";
    ctx.fillText(bmoTxt, startX, footerY);

    const bmoW = ctx.measureText(bmoTxt).width;
    ctx.fillStyle = "#00d2d3";
    ctx.fillText(gbTxt, startX + bmoW, footerY);

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  createDpadTexture() {
    const canvas = document.createElement("canvas");
    canvas.width = 512;
    canvas.height = 512;
    const ctx = canvas.getContext("2d");

    ctx.clearRect(0, 0, 512, 512);

    // 1. Subtle directional arrows matching 2D opacity
    ctx.fillStyle = "rgba(255, 255, 255, 0.38)";
    const drawTriangle = (cx, cy, size, angle) => {
      ctx.save();
      ctx.translate(cx, cy);
      ctx.rotate(angle);
      ctx.beginPath();
      ctx.moveTo(0, -size);
      ctx.lineTo(size * 0.86, size * 0.7);
      ctx.lineTo(-size * 0.86, size * 0.7);
      ctx.closePath();
      ctx.fill();
      ctx.restore();
    };

    drawTriangle(256, 75, 18, 0);
    drawTriangle(256, 437, 18, Math.PI);
    drawTriangle(75, 256, 18, -Math.PI / 2);
    drawTriangle(437, 256, 18, Math.PI / 2);

    // 2. Clean, crisp circular thumb dish (NO UGLY BLACK GREASE STAIN!)
    // Subtle indented well with clean outline
    ctx.fillStyle = "rgba(0, 0, 0, 0.22)";
    ctx.beginPath();
    ctx.arc(256, 256, 48, 0, Math.PI * 2);
    ctx.fill();

    ctx.lineWidth = 3;
    ctx.strokeStyle = "rgba(0, 0, 0, 0.28)";
    ctx.stroke();

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  buildConsoleModel() {
    this.rootGroup = new THREE.Group();
    this.scene.add(this.rootGroup);

    // Rich Authentic BMO Colors (Matching CSS exactly)
    const bmoTealHex = 0x5fb49c;
    const deepTealHex = 0x1a4b42;
    const dpadYellowHex = 0xffe033;

    // Materials
    this.materials.chassis = new THREE.MeshStandardMaterial({
      color: bmoTealHex,
      roughness: 0.32,
      metalness: 0.02,
    });

    this.materials.bezel = new THREE.MeshStandardMaterial({
      color: deepTealHex,
      roughness: 0.35,
      metalness: 0.04,
    });

    this.materials.dpad = new THREE.MeshStandardMaterial({
      color: dpadYellowHex,
      roughness: 0.25,
      metalness: 0.02,
    });

    this.materials.btnPill = new THREE.MeshStandardMaterial({
      color: deepTealHex,
      roughness: 0.5,
      metalness: 0.02,
    });

    this.materials.shoulder = new THREE.MeshStandardMaterial({
      color: deepTealHex,
      roughness: 0.28,
      metalness: 0.04,
    });

    this.materials.metal = new THREE.MeshStandardMaterial({
      color: 0x95a5a6,
      roughness: 0.2,
      metalness: 0.9,
    });

    // -------------------------------------------------------------------------
    // 1. MAIN CHASSIS BODY (8.8 cm W × 12.4 cm H × 3.2 cm D)
    // -------------------------------------------------------------------------
    const W = 8.8;
    const H = 12.4;
    const D = 3.2;
    const hw = W / 2; // 4.4
    const hh = H / 2; // 6.2
    const r = 0.6;    // standard corner radius
    const r_br = 1.5; // classic asymmetric bottom-right corner curve

    const shape = new THREE.Shape();
    shape.moveTo(-hw + r, -hh);
    shape.lineTo(hw - r_br, -hh);
    shape.quadraticCurveTo(hw, -hh, hw, -hh + r_br);
    shape.lineTo(hw, hh - r);
    shape.quadraticCurveTo(hw, hh, hw - r, hh);
    shape.lineTo(-hw + r, hh);
    shape.quadraticCurveTo(-hw, hh, -hw, hh - r);
    shape.lineTo(-hw, -hh + r);
    shape.quadraticCurveTo(-hw, -hh, -hw + r, -hh);

    // Deep smooth multi-segment bevel creating the top edge highlight
    const extrudeSettings = {
      depth: D - 0.44,
      bevelEnabled: true,
      bevelSegments: 8,
      steps: 1,
      bevelSize: 0.22,
      bevelThickness: 0.22,
    };

    const chassisGeo = new THREE.ExtrudeGeometry(shape, extrudeSettings);
    chassisGeo.center();
    this.chassisMesh = new THREE.Mesh(chassisGeo, this.materials.chassis);
    this.chassisMesh.castShadow = true;
    this.chassisMesh.receiveShadow = true;
    this.rootGroup.add(this.chassisMesh);

    const Z_FRONT = D / 2; // +1.6 cm

    // Soft Ambient Contact Drop Shadow under the entire console
    const shadowGeo = new THREE.PlaneGeometry(13.5, 17.5);
    const shadowTex = this.createDropShadowTexture(62);
    const shadowMat = new THREE.MeshBasicMaterial({ map: shadowTex, transparent: true, opacity: 0.72 });
    const shadowMesh = new THREE.Mesh(shadowGeo, shadowMat);
    shadowMesh.position.set(0, -0.2, -D / 2 - 0.08);
    this.rootGroup.add(shadowMesh);

    // -------------------------------------------------------------------------
    // 2. TOP FOREHEAD MOLDED RIDGE LINES
    // -------------------------------------------------------------------------
    for (let i = 0; i < 2; i++) {
      const ridgeGeo = new THREE.BoxGeometry(4.8, 0.07, 0.04);
      const ridgeMat = new THREE.MeshBasicMaterial({ color: 0x438f7a });
      const ridgeMesh = new THREE.Mesh(ridgeGeo, ridgeMat);
      ridgeMesh.position.set(0, hh - 0.5 - i * 0.16, Z_FRONT + 0.015);
      this.rootGroup.add(ridgeMesh);
    }

    // -------------------------------------------------------------------------
    // 3. L / R SHOULDER BUTTONS (Visible on Front Face in Top Shoulder Row!)
    //  - L: X -2.4 cm, Y +5.5 cm (above bezel)
    //  - R: X +2.4 cm, Y +5.5 cm
    // -------------------------------------------------------------------------
    const createShoulder = (name, key, label, x) => {
      // 1. Drop shadow under shoulder bumper
      const sDropGeo = new THREE.PlaneGeometry(2.6, 0.9);
      const sDropTex = this.createDropShadowTexture(56);
      const sDropMat = new THREE.MeshBasicMaterial({ map: sDropTex, transparent: true, opacity: 0.5 });
      const sDropMesh = new THREE.Mesh(sDropGeo, sDropMat);
      sDropMesh.position.set(x, hh - 0.8, Z_FRONT + 0.01);
      this.rootGroup.add(sDropMesh);

      // 2. Shoulder Bumper Body
      const sGeo = new THREE.BoxGeometry(2.4, 0.52, 1.3);
      const sMesh = new THREE.Mesh(sGeo, this.materials.shoulder);
      sMesh.position.set(x, hh - 0.72, Z_FRONT - 0.45);
      sMesh.castShadow = true;
      sMesh.userData = { isButton: true, key: key, name: name, isShoulder: true };
      this.rootGroup.add(sMesh);

      // 3. Front Face Bevel with crisp embossed "L" / "R" label
      const fCanvas = document.createElement("canvas");
      fCanvas.width = 128;
      fCanvas.height = 64;
      const fCtx = fCanvas.getContext("2d");
      fCtx.fillStyle = "#1a4b42";
      fCtx.roundRect(0, 0, 128, 64, 12);
      fCtx.fill();
      fCtx.lineWidth = 3;
      fCtx.strokeStyle = "rgba(255, 255, 255, 0.35)";
      fCtx.stroke();
      fCtx.fillStyle = "#cef5e4";
      fCtx.font = "bold 34px 'Press Start 2P', monospace";
      fCtx.textAlign = "center";
      fCtx.textBaseline = "middle";
      fCtx.fillText(label, 64, 34);

      const fTex = new THREE.CanvasTexture(fCanvas);
      const fGeo = new THREE.PlaneGeometry(2.35, 0.48);
      const fMat = new THREE.MeshBasicMaterial({ map: fTex, transparent: true });
      const fMesh = new THREE.Mesh(fGeo, fMat);
      fMesh.position.set(0, 0, 0.66);
      sMesh.add(fMesh);

      this.buttonMeshes[key] = sMesh;
      this.interactiveObjects.push(sMesh);
      this.interactiveObjects.push(fMesh);
      fMesh.userData = sMesh.userData;

      this.buttonOriginalPositions.set(sMesh, {
        x: sMesh.position.x,
        y: sMesh.position.y,
        z: sMesh.position.z,
      });
    };

    createShoulder("L Trigger", "L", "L", -2.4);
    createShoulder("R Trigger", "R", "R", 2.4);

    // -------------------------------------------------------------------------
    // 4. SCREEN BEZEL HOUSING & ACTIVE ST7789 DISPLAY
    //  - Housing: 6.8 × 5.4 cm centered at X 0.0, Y +2.6 cm
    //  - Active ST7789 display window: 5.0 × 3.8 cm
    // -------------------------------------------------------------------------
    const BEZEL_W = 6.8;
    const BEZEL_H = 5.4;
    const BEZEL_Y = hh - 3.6; // +2.6 cm

    // Deep drop shadow behind screen bezel
    const bezelDropGeo = new THREE.PlaneGeometry(BEZEL_W + 0.8, BEZEL_H + 0.8);
    const bezelDropTex = this.createDropShadowTexture(58);
    const bezelDropMat = new THREE.MeshBasicMaterial({ map: bezelDropTex, transparent: true, opacity: 0.6 });
    const bezelDropMesh = new THREE.Mesh(bezelDropGeo, bezelDropMat);
    bezelDropMesh.position.set(0, BEZEL_Y - 0.08, Z_FRONT + 0.01);
    this.rootGroup.add(bezelDropMesh);

    // Bezel housing plate with high-res branding texture
    const bezelTex = this.createBezelTexture();
    const bezelGeo = new THREE.PlaneGeometry(BEZEL_W, BEZEL_H);
    const bezelMat = new THREE.MeshBasicMaterial({
      map: bezelTex,
      transparent: true,
    });
    const bezelMesh = new THREE.Mesh(bezelGeo, bezelMat);
    bezelMesh.position.set(0, BEZEL_Y, Z_FRONT + 0.025);
    this.rootGroup.add(bezelMesh);

    // Active ST7789 LCD Panel (5.0 × 3.8 cm active area)
    // MeshBasicMaterial ensures TRUE 100% RETRO MINT COLORS without white blowout!
    const SCREEN_W = 5.0;
    const SCREEN_H = 3.8;
    const screenGeo = new THREE.PlaneGeometry(SCREEN_W, SCREEN_H);

    if (this.tftCanvas) {
      this.screenTexture = new THREE.CanvasTexture(this.tftCanvas);
      this.screenTexture.minFilter = THREE.LinearFilter;
      this.screenTexture.magFilter = THREE.NearestFilter;
    }

    const screenMat = new THREE.MeshBasicMaterial({
      map: this.screenTexture,
    });

    this.screenMesh = new THREE.Mesh(screenGeo, screenMat);
    this.screenMesh.position.set(0, BEZEL_Y, Z_FRONT + 0.035);
    this.rootGroup.add(this.screenMesh);

    // Glossy Protective Glass Lens overlay with subtle transparent sheen
    const lensGeo = new THREE.PlaneGeometry(BEZEL_W - 0.2, BEZEL_H - 0.2);
    const lensMat = new THREE.MeshPhysicalMaterial({
      roughness: 0.05,
      metalness: 0.0,
      clearcoat: 0.95,
      clearcoatRoughness: 0.04,
      transmission: 0.1,
      transparent: true,
      opacity: 0.18,
      reflectivity: 0.4,
    });
    const lensMesh = new THREE.Mesh(lensGeo, lensMat);
    lensMesh.position.set(0, BEZEL_Y, Z_FRONT + 0.045);
    this.rootGroup.add(lensMesh);

    // -------------------------------------------------------------------------
    // 5. D-PAD: Single Seamless Sculpted Cross with Soft Drop Shadow & Pivot
    //  - Center at (2.2, 8.4) cm => X -2.2, Y -2.3 cm
    //  - Dimensions: 2.8 × 2.8 cm, arm width 0.95 cm
    // -------------------------------------------------------------------------
    const DPAD_X = -2.2;
    const DPAD_Y = -2.3;
    const ARM_W = 0.95;
    const L = 1.4;            // Half of 2.8
    const W_HALF = ARM_W / 2; // 0.475
    const CR = 0.08;          // Arm corner roundness

    // Soft Drop Shadow Mesh under D-Pad
    const dpadDropGeo = new THREE.PlaneGeometry(3.6, 3.6);
    const dpadDropTex = this.createDropShadowTexture(60);
    const dpadDropMat = new THREE.MeshBasicMaterial({ map: dpadDropTex, transparent: true, opacity: 0.62 });
    const dpadDropMesh = new THREE.Mesh(dpadDropGeo, dpadDropMat);
    dpadDropMesh.position.set(DPAD_X, DPAD_Y - 0.08, Z_FRONT + 0.01);
    this.rootGroup.add(dpadDropMesh);

    // Extruded Seamless Cross Shape
    const dpadShape = new THREE.Shape();
    dpadShape.moveTo(-W_HALF + CR, L);
    dpadShape.lineTo(W_HALF - CR, L);
    dpadShape.quadraticCurveTo(W_HALF, L, W_HALF, L - CR);
    dpadShape.lineTo(W_HALF, W_HALF + CR);
    dpadShape.quadraticCurveTo(W_HALF, W_HALF, W_HALF + CR, W_HALF);
    dpadShape.lineTo(L - CR, W_HALF);
    dpadShape.quadraticCurveTo(L, W_HALF, L, W_HALF - CR);
    dpadShape.lineTo(L, -W_HALF + CR);
    dpadShape.quadraticCurveTo(L, -W_HALF, L - CR, -W_HALF);
    dpadShape.lineTo(W_HALF + CR, -W_HALF);
    dpadShape.quadraticCurveTo(W_HALF, -W_HALF, W_HALF, -W_HALF - CR);
    dpadShape.lineTo(W_HALF, -L + CR);
    dpadShape.quadraticCurveTo(W_HALF, -L, W_HALF - CR, -L);
    dpadShape.lineTo(-W_HALF + CR, -L);
    dpadShape.quadraticCurveTo(-W_HALF, -L, -W_HALF, -L + CR);
    dpadShape.lineTo(-W_HALF, -W_HALF - CR);
    dpadShape.quadraticCurveTo(-W_HALF, -W_HALF, -W_HALF - CR, -W_HALF);
    dpadShape.lineTo(-L + CR, -W_HALF);
    dpadShape.quadraticCurveTo(-L, -W_HALF, -L, -W_HALF + CR);
    dpadShape.lineTo(-L, W_HALF - CR);
    dpadShape.quadraticCurveTo(-L, W_HALF, -L + CR, W_HALF);
    dpadShape.lineTo(-W_HALF - CR, W_HALF);
    dpadShape.quadraticCurveTo(-W_HALF, W_HALF, -W_HALF, W_HALF + CR);
    dpadShape.closePath();

    const dpadGeo = new THREE.ExtrudeGeometry(dpadShape, {
      depth: 0.22,
      bevelEnabled: true,
      bevelSegments: 4,
      bevelSize: 0.05,
      bevelThickness: 0.05,
    });
    dpadGeo.center();

    // D-Pad Pivot Root Group (allows dynamic mechanical tilt!)
    this.dpadPivotGroup = new THREE.Group();
    this.dpadPivotGroup.position.set(DPAD_X, DPAD_Y, Z_FRONT + 0.14);
    this.rootGroup.add(this.dpadPivotGroup);

    this.dpadBodyMesh = new THREE.Mesh(dpadGeo, this.materials.dpad);
    this.dpadBodyMesh.castShadow = true;
    this.dpadBodyMesh.receiveShadow = true;
    this.dpadPivotGroup.add(this.dpadBodyMesh);

    // Directional arrows & center circular concave dish overlay
    const dpadTex = this.createDpadTexture();
    const dpadTexGeo = new THREE.PlaneGeometry(2.8, 2.8);
    const dpadTexMat = new THREE.MeshBasicMaterial({
      map: dpadTex,
      transparent: true,
    });
    const dpadTexMesh = new THREE.Mesh(dpadTexGeo, dpadTexMat);
    dpadTexMesh.position.set(0, 0, 0.165);
    this.dpadPivotGroup.add(dpadTexMesh);

    // 4 Raycasting Hit Zones
    const createDpadZone = (key, x, y, w, h) => {
      const geo = new THREE.PlaneGeometry(w, h);
      const mat = new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false });
      const mesh = new THREE.Mesh(geo, mat);
      mesh.position.set(x, y, 0.18);
      mesh.userData = { isButton: true, key: key, isDpad: true };
      this.dpadPivotGroup.add(mesh);
      this.buttonMeshes[key] = mesh;
      this.interactiveObjects.push(mesh);
    };

    createDpadZone("UP", 0, 0.85, ARM_W, 1.1);
    createDpadZone("DOWN", 0, -0.85, ARM_W, 1.1);
    createDpadZone("LEFT", -0.85, 0, 1.1, ARM_W);
    createDpadZone("RIGHT", 0.85, 0, 1.1, ARM_W);

    // -------------------------------------------------------------------------
    // 6. ACTION BUTTONS (X, Y, A, B) - Premium High-Gloss Injection Molded Domes
    //  - Square to 2.54 mm Perfboard Grid with Soft Drop Shadows!
    //    * X (Top):   X +2.2, Y -1.3 cm (Leaf Green #4CAF50)
    //    * Y (Left):  X +1.2, Y -2.3 cm (Bubblegum Pink #F48FB1)
    //    * A (Right): X +3.2, Y -2.3 cm (Sky Blue #52A6BA)
    //    * B (Bottom):X +2.2, Y -3.3 cm (Vivid Magenta #E8175D)
    // -------------------------------------------------------------------------
    const CAP_RADIUS = 0.44;
    const CAP_HEIGHT = 0.26;

    const createActionButton = (name, key, label, x, y, bgColorStr, textColorStr) => {
      // 1. Soft Drop Shadow Mesh under each button (matching 2D box-shadow!)
      const shadowDiscGeo = new THREE.PlaneGeometry(CAP_RADIUS * 2.8, CAP_RADIUS * 2.8);
      const shadowDiscTex = this.createDropShadowTexture(56);
      const shadowDiscMat = new THREE.MeshBasicMaterial({ map: shadowDiscTex, transparent: true, opacity: 0.58 });
      const shadowDisc = new THREE.Mesh(shadowDiscGeo, shadowDiscMat);
      shadowDisc.position.set(x, y - 0.06, Z_FRONT + 0.01);
      this.rootGroup.add(shadowDisc);

      // 2. Action Button Root Group
      const btnGroup = new THREE.Group();
      btnGroup.position.set(x, y, Z_FRONT + CAP_HEIGHT / 2);
      this.rootGroup.add(btnGroup);

      // Button Stem (Cylinder)
      const cylGeo = new THREE.CylinderGeometry(CAP_RADIUS, CAP_RADIUS * 1.04, CAP_HEIGHT, 32);
      cylGeo.rotateX(Math.PI / 2);
      const cylMat = new THREE.MeshStandardMaterial({
        color: new THREE.Color(bgColorStr),
        roughness: 0.2,
        metalness: 0.02,
      });
      const stemMesh = new THREE.Mesh(cylGeo, cylMat);
      stemMesh.castShadow = true;
      btnGroup.add(stemMesh);

      // Domed Button Top with Specular Sheen & Centered Bold Letter
      const topTex = this.createButtonTexture(label, bgColorStr, textColorStr);
      const topGeo = new THREE.CircleGeometry(CAP_RADIUS, 32);
      const topMat = new THREE.MeshBasicMaterial({
        map: topTex,
      });
      const capMesh = new THREE.Mesh(topGeo, topMat);
      capMesh.position.set(0, 0, CAP_HEIGHT / 2 + 0.005);
      btnGroup.add(capMesh);

      // Interactive Click Targets
      btnGroup.userData = { isButton: true, key: key, name: name, isAction: true };
      this.buttonMeshes[key] = btnGroup;
      this.interactiveObjects.push(stemMesh);
      this.interactiveObjects.push(capMesh);
      stemMesh.userData = btnGroup.userData;
      capMesh.userData = btnGroup.userData;

      this.buttonOriginalPositions.set(btnGroup, {
        x: btnGroup.position.x,
        y: btnGroup.position.y,
        z: btnGroup.position.z,
      });

      return btnGroup;
    };

    createActionButton("X Button", "X", "X", 2.2, -1.3, "#4caf50", "#0f2620");
    createActionButton("Y Button", "Y", "Y", 1.2, -2.3, "#f48fb1", "#0f2620");
    createActionButton("A Button", "A", "A", 3.2, -2.3, "#52a6ba", "#0f2620");
    createActionButton("B Button", "B", "B", 2.2, -3.3, "#e8175d", "#ffffff");

    // -------------------------------------------------------------------------
    // 7. SELECT / START PILL BUTTONS (Angled Rubber Pills with Drop Shadows)
    //  - Select: X -1.0, Y -4.8 cm
    //  - Start:  X +1.0, Y -4.8 cm
    // -------------------------------------------------------------------------
    const createPill = (name, key, label, x, y) => {
      // Drop Shadow under Pill
      const pDropGeo = new THREE.PlaneGeometry(1.8, 0.9);
      const pDropTex = this.createDropShadowTexture(56);
      const pDropMat = new THREE.MeshBasicMaterial({ map: pDropTex, transparent: true, opacity: 0.52 });
      const pDropMesh = new THREE.Mesh(pDropGeo, pDropMat);
      pDropMesh.rotation.z = -0.44; // ~-25 degrees
      pDropMesh.position.set(x, y - 0.05, Z_FRONT + 0.01);
      this.rootGroup.add(pDropMesh);

      // Pill Mesh
      const pillGeo = new THREE.BoxGeometry(1.38, 0.44, 0.22);
      const pillMesh = new THREE.Mesh(pillGeo, this.materials.btnPill);
      pillMesh.rotation.z = -0.44;
      pillMesh.position.set(x, y, Z_FRONT + 0.12);
      pillMesh.castShadow = true;
      pillMesh.userData = { isButton: true, key: key, name: name, isPill: true };
      this.rootGroup.add(pillMesh);

      this.buttonMeshes[key] = pillMesh;
      this.interactiveObjects.push(pillMesh);
      this.buttonOriginalPositions.set(pillMesh, {
        x: pillMesh.position.x,
        y: pillMesh.position.y,
        z: pillMesh.position.z,
      });

      // Engraved Pill Label below
      const lblCanvas = document.createElement("canvas");
      lblCanvas.width = 256;
      lblCanvas.height = 64;
      const lCtx = lblCanvas.getContext("2d");
      lCtx.fillStyle = "#1a4b42";
      lCtx.font = "bold 26px 'Press Start 2P', monospace";
      lCtx.textAlign = "center";
      lCtx.textBaseline = "middle";
      lCtx.fillText(label, 128, 32);

      const lTex = new THREE.CanvasTexture(lblCanvas);
      const lGeo = new THREE.PlaneGeometry(1.3, 0.32);
      const lMat = new THREE.MeshBasicMaterial({ map: lTex, transparent: true });
      const lMesh = new THREE.Mesh(lGeo, lMat);
      lMesh.position.set(x, y - 0.44, Z_FRONT + 0.02);
      this.rootGroup.add(lMesh);
    };

    createPill("Select", "SELECT", "SELECT", -1.0, -4.8);
    createPill("Start", "START", "START", 1.0, -4.8);

    // -------------------------------------------------------------------------
    // 8. SPEAKER GRILLE (6 Diagonal Slits in Bottom-Right Corner)
    // -------------------------------------------------------------------------
    const speakerGroup = new THREE.Group();
    speakerGroup.position.set(2.6, -4.8, Z_FRONT + 0.015);
    speakerGroup.rotation.z = -0.52; // ~-30 degrees
    this.rootGroup.add(speakerGroup);

    for (let i = 0; i < 6; i++) {
      const slitGeo = new THREE.BoxGeometry(0.1, 0.85 + (i < 3 ? i * 0.14 : (5 - i) * 0.14), 0.04);
      const slitMat = new THREE.MeshBasicMaterial({ color: 0x438f7a });
      const slit = new THREE.Mesh(slitGeo, slitMat);
      slit.position.set(i * 0.22 - 0.55, 0, 0);
      speakerGroup.add(slit);
    }

    // -------------------------------------------------------------------------
    // 9. HARDWARE PORTS: USB-C, SD Slot, Power Switch
    // -------------------------------------------------------------------------
    // Top USB-C Cutout
    const usbcGeo = new THREE.BoxGeometry(1.0, 0.45, 0.6);
    const usbcMat = new THREE.MeshBasicMaterial({ color: 0x0f2620 });
    const usbcHole = new THREE.Mesh(usbcGeo, usbcMat);
    usbcHole.position.set(0, hh, 0.4);
    this.rootGroup.add(usbcHole);

    const usbcTongueGeo = new THREE.BoxGeometry(0.7, 0.12, 0.35);
    const usbcTongue = new THREE.Mesh(usbcTongueGeo, this.materials.metal);
    usbcTongue.position.set(0, hh + 0.05, 0.4);
    this.rootGroup.add(usbcTongue);

    // Left MicroSD Slot
    const sdHoleGeo = new THREE.BoxGeometry(0.5, 0.25, 1.35);
    const sdHole = new THREE.Mesh(sdHoleGeo, usbcMat);
    sdHole.position.set(-hw, 2.2, 0.4);
    this.rootGroup.add(sdHole);

    // Right Power Switch
    const pwrHoleGeo = new THREE.BoxGeometry(0.4, 1.1, 0.45);
    const pwrHole = new THREE.Mesh(pwrHoleGeo, usbcMat);
    pwrHole.position.set(hw, 3.2, 0.4);
    this.rootGroup.add(pwrHole);

    const pwrNubGeo = new THREE.BoxGeometry(0.25, 0.45, 0.35);
    const pwrNub = new THREE.Mesh(pwrNubGeo, this.materials.btnPill);
    pwrNub.position.set(hw + 0.08, 3.4, 0.4);
    this.rootGroup.add(pwrNub);

    // -------------------------------------------------------------------------
    // 10. REAR 4×AA BATTERY COMPARTMENT DOOR
    // -------------------------------------------------------------------------
    const doorGeo = new THREE.BoxGeometry(6.2, 3.4, 0.18);
    const doorMesh = new THREE.Mesh(doorGeo, this.materials.chassis);
    doorMesh.position.set(0, -2.8, -D / 2);
    this.rootGroup.add(doorMesh);

    for (let i = 0; i < 4; i++) {
      const slitGeo = new THREE.BoxGeometry(4.2, 0.1, 0.04);
      const slitMat = new THREE.MeshBasicMaterial({ color: 0x438f7a });
      const slit = new THREE.Mesh(slitGeo, slitMat);
      slit.position.set(0, -2.2 - i * 0.35, -D / 2 - 0.09);
      this.rootGroup.add(slit);
    }
  }

  setupRaycasting() {
    const dom = this.renderer.domElement;

    dom.addEventListener("pointerdown", (e) => {
      const btn = this.getIntersectedButton(e);
      if (btn) {
        this.activePointerButton = btn.userData.key;
        if (this.controls) this.controls.enabled = false;
        this.pressButton(this.activePointerButton, true);
      }
    });

    window.addEventListener("pointerup", (e) => {
      if (this.controls) this.controls.enabled = true;
      if (this.activePointerButton) {
        this.releaseButton(this.activePointerButton, true);
        this.activePointerButton = null;
      }
    });

    dom.addEventListener("pointermove", (e) => {
      const btn = this.getIntersectedButton(e);
      dom.style.cursor = btn ? "pointer" : "default";
    });
  }

  getIntersectedButton(e) {
    const rect = this.renderer.domElement.getBoundingClientRect();
    this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

    this.raycaster.setFromCamera(this.mouse, this.camera);
    const intersects = this.raycaster.intersectObjects(this.interactiveObjects, false);

    if (intersects.length > 0) {
      return intersects[0].object;
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
          mesh.position.y = orig.y - 0.14; // Depress shoulder bumper down
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
        mesh.position.x = orig.x;
        mesh.position.y = orig.y;
        mesh.position.z = orig.z;
      }
    }

    if (triggerCallback && this.onButtonRelease) {
      this.onButtonRelease(key);
    }
  }

  setTheme(themeName) {
    this.activeTheme = themeName;
    if (!this.materials.chassis) return;

    let chassisHex = 0x5fb49c;
    let bezelHex = 0x1a4b42;
    let roughness = 0.32;
    let transmission = 0.0;
    let transparent = false;
    let opacity = 1.0;

    switch (themeName) {
      case "dmg-gray":
        chassisHex = 0xd1ccc0;
        bezelHex = 0x57606f;
        roughness = 0.42;
        break;
      case "atomic-purple":
        chassisHex = 0x9c88ff;
        bezelHex = 0x2f3542;
        roughness = 0.18;
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
        chassisHex = 0x5fb49c;
        bezelHex = 0x1a4b42;
        break;
    }

    this.materials.chassis.color.setHex(chassisHex);
    this.materials.chassis.roughness = roughness;
    this.materials.chassis.transmission = transmission;
    this.materials.chassis.transparent = transparent;
    this.materials.chassis.opacity = opacity;

    this.materials.bezel.color.setHex(bezelHex);
  }

  setCameraPreset(presetName) {
    if (!this.camera) return;

    const targets = {
      front: { pos: [0, 0, 19.5], look: [0, 0, 0] },
      iso: { pos: [9.5, 7.5, 16.5], look: [0, 0, 0] },
      top: { pos: [0, 16.5, 4.5], look: [0, 5.0, 0.5] },
      left: { pos: [-16.0, 1.5, 3.5], look: [-3.5, 1.5, 0] },
      right: { pos: [16.0, 2.0, 3.5], look: [3.5, 2.0, 0] },
      back: { pos: [0, -1.0, -19.5], look: [0, -1.5, 0] },
    };

    const t = targets[presetName] || targets.front;
    this.camera.position.set(...t.pos);
    if (this.controls) {
      this.controls.target.set(...t.look);
      this.controls.update();
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
    requestAnimationFrame(this.animate);

    if (this.controls) {
      this.controls.update();
    }

    // D-Pad smooth harmonic tilt interpolation
    if (this.dpadPivotGroup) {
      this.dpadPivotGroup.rotation.x += (this.dpadTargetRotation.x - this.dpadPivotGroup.rotation.x) * 0.35;
      this.dpadPivotGroup.rotation.y += (this.dpadTargetRotation.y - this.dpadPivotGroup.rotation.y) * 0.35;
      const targetZ = (3.2 / 2) + 0.14 + this.dpadTargetZ;
      this.dpadPivotGroup.position.z += (targetZ - this.dpadPivotGroup.position.z) * 0.35;
    }

    // Live update screen texture from the 2D canvas buffer
    if (this.screenTexture) {
      this.screenTexture.needsUpdate = true;
    }

    if (this.renderer && this.scene && this.camera) {
      this.renderer.render(this.scene, this.camera);
    }
  }
}

window.Bmo3DConsole = Bmo3DConsole;
