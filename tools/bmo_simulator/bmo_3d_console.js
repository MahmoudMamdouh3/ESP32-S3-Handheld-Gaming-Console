/**
 * bmo_3d_console.js
 * 
 * Ultra-Premium 3D Physical Hardware Model & Interactive WebGL Simulator for BMO Handheld Console.
 * Built with Three.js (r128) strictly adhering to the Physical Hardware Reference Specification:
 * 
 * =========================================================================================
 * PHYSICAL REFERENCE SPECIFICATION (Exact Centimeter Coordinates from Top-Left Front Face):
 * =========================================================================================
 *  - Body: 8.8 W × 12.4 H × 3.2 D cm, corner radius 0.6 cm (1.5 cm asymmetric bottom-right)
 *  - Display Window: 5.0 W × 3.8 H cm centered at (4.4, 3.3) -> 56.8% body width (real 2.4" ST7789)
 *  - Logo Strip: 5.0 W × 0.8 H cm at y 5.2-6.0 cm centered at (4.4, 5.6)
 *  - D-Pad: 2.8 × 2.8 cm (arm width 0.95 cm) centered at (2.2, 8.4)
 *  - ABXY Square Grid (2.54 mm perfboard aligned):
 *      * X (Top):    cap Ø0.9 cm at (6.6, 7.4) [Three.js X: +2.2, Y: -1.2]
 *      * Y (Left):   cap Ø0.9 cm at (5.6, 8.4) [Three.js X: +1.2, Y: -2.2]
 *      * A (Right):  cap Ø0.9 cm at (7.6, 8.4) [Three.js X: +3.2, Y: -2.2]
 *      * B (Bottom): cap Ø0.9 cm at (6.6, 9.4) [Three.js X: +2.2, Y: -3.2]
 *      * Spacing: gap ~0.5 cm, centers 1.41 cm apart, X directly above B, Y level with A
 *  - Select / Start: 1.4 × 0.5 cm pills at (3.4, 11.0) and (5.4, 11.0)
 *  - Top Edge L / R Shoulders: 2.2 × 0.6 cm side-mounted tact switches at (1.7, 0.3) and (7.1, 0.3)
 *  - Top Edge USB-C Cutout: 1.0 × 0.45 cm at (4.4, 0.0) between L and R
 *  - Left Edge MicroSD Slot: 1.3 × 0.25 cm at y ≈ 4.0 cm
 *  - Right Edge Power Slide Switch: 1.0 × 0.4 cm at y ≈ 3.0 cm
 *  - Rear Battery Door: 3.4 H × 6.2 W cm centered at (4.4, 9.0) across width for 4×AA holder
 * =========================================================================================
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

    // D-Pad pivot tilt state
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

    // 3. Renderer with sRGB color space & Linear Tone Mapping (NO BLEACHING / FULL VIBRANCY)
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

    // 6. Build the 3D Console Model strictly according to Reference Spec
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
    // 1. Clean ambient illumination preserving rich color saturation
    const ambient = new THREE.AmbientLight(0xffffff, 0.68);
    this.scene.add(ambient);

    // 2. Key Light: Warm studio directional light from front-top-right for bevel highlights
    const keyLight = new THREE.DirectionalLight(0xfffaf0, 0.58);
    keyLight.position.set(7, 12, 14);
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

    // 3. Fill Light: Soft cool fill from front-left
    const fillLight = new THREE.DirectionalLight(0xd4eaff, 0.32);
    fillLight.position.set(-10, -2, 10);
    this.scene.add(fillLight);

    // 4. Rim Light: Edge highlight along top shoulders and side silhouette
    const rimLight = new THREE.DirectionalLight(0xffffff, 0.48);
    rimLight.position.set(0, 14, -8);
    this.scene.add(rimLight);

    // 5. Bounce Light: Subtle warm ground bounce
    const bounceLight = new THREE.DirectionalLight(0x5fb49c, 0.18);
    bounceLight.position.set(0, -10, 4);
    this.scene.add(bounceLight);
  }

  createDropShadowTexture(radius = 64) {
    const canvas = document.createElement("canvas");
    canvas.width = 128;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");
    const grad = ctx.createRadialGradient(64, 64, radius * 0.25, 64, 64, radius);
    grad.addColorStop(0, "rgba(0, 0, 0, 0.48)");
    grad.addColorStop(0.65, "rgba(0, 0, 0, 0.16)");
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

    // 1. Rich saturated solid base
    ctx.fillStyle = bgColorStr;
    ctx.beginPath();
    ctx.arc(128, 128, 122, 0, Math.PI * 2);
    ctx.fill();

    // 2. Bottom shadow crescent for 3D depth
    const btmGrad = ctx.createRadialGradient(128, 128, 55, 128, 128, 122);
    btmGrad.addColorStop(0, "rgba(0, 0, 0, 0)");
    btmGrad.addColorStop(0.7, "rgba(0, 0, 0, 0.1)");
    btmGrad.addColorStop(1, "rgba(0, 0, 0, 0.35)");
    ctx.fillStyle = btmGrad;
    ctx.beginPath();
    ctx.arc(128, 128, 122, 0, Math.PI * 2);
    ctx.fill();

    // 3. Top crescent highlight (giving authentic 3D candy button sheen)
    const topHl = ctx.createLinearGradient(128, 10, 128, 90);
    topHl.addColorStop(0, "rgba(255, 255, 255, 0.46)");
    topHl.addColorStop(1, "rgba(255, 255, 255, 0.0)");
    ctx.fillStyle = topHl;
    ctx.beginPath();
    ctx.ellipse(128, 60, 80, 36, 0, 0, Math.PI * 2);
    ctx.fill();

    // 4. Subtle outer border rim
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

  createLogoStripTexture() {
    const canvas = document.createElement("canvas");
    canvas.width = 512;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");

    ctx.clearRect(0, 0, 512, 128);

    // Centered "BMO" and "GAMEBOY"
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.font = "bold 38px 'Press Start 2P', monospace";

    const bmoTxt = "BMO ";
    const gbTxt = "GAMEBOY";
    const totalW = ctx.measureText(bmoTxt + gbTxt).width;
    const startX = 256 - totalW / 2;

    ctx.textAlign = "left";
    ctx.fillStyle = "#1a4b42";
    ctx.fillText(bmoTxt, startX, 64);

    const bmoW = ctx.measureText(bmoTxt).width;
    ctx.fillStyle = "#00d2d3";
    ctx.fillText(gbTxt, startX + bmoW, 64);

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
    ctx.fillStyle = "rgba(255, 255, 255, 0.4)";
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

    // 2. Clean, subtle circular thumb dish (clean outline, no black grease stain)
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

    // Authentic Rich BMO Colors (Matching CSS exactly)
    const bmoTealHex = 0x5fb49c;
    const deepTealHex = 0x1a4b42;
    const dpadYellowHex = 0xffe033;

    // Materials: Physical PBR with satin ABS finish & clearcoat sheen
    this.materials.chassis = new THREE.MeshPhysicalMaterial({
      color: bmoTealHex,
      roughness: 0.28,
      metalness: 0.02,
      clearcoat: 0.18,
      clearcoatRoughness: 0.12,
    });

    this.materials.bezel = new THREE.MeshPhysicalMaterial({
      color: deepTealHex,
      roughness: 0.3,
      metalness: 0.04,
      clearcoat: 0.22,
    });

    this.materials.dpad = new THREE.MeshPhysicalMaterial({
      color: dpadYellowHex,
      roughness: 0.24,
      metalness: 0.02,
      clearcoat: 0.12,
    });

    this.materials.btnPill = new THREE.MeshStandardMaterial({
      color: deepTealHex,
      roughness: 0.52,
      metalness: 0.02,
    });

    this.materials.shoulder = new THREE.MeshPhysicalMaterial({
      color: deepTealHex,
      roughness: 0.26,
      metalness: 0.04,
      clearcoat: 0.2,
    });

    this.materials.metal = new THREE.MeshStandardMaterial({
      color: 0x95a5a6,
      roughness: 0.2,
      metalness: 0.9,
    });

    // -------------------------------------------------------------------------
    // 1. MAIN CHASSIS BODY: 8.8 W × 12.4 H × 3.2 D cm
    //    Corner radius: 0.6 cm (1.5 cm asymmetric bottom-right curve)
    // -------------------------------------------------------------------------
    const W = 8.8;
    const H = 12.4;
    const D = 3.2;
    const hw = W / 2; // 4.4
    const hh = H / 2; // 6.2
    const r = 0.6;    // standard corner radius
    const r_br = 1.5; // classic asymmetric bottom-right curve

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

    // Deep smooth multi-segment bevel
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
    // 2. DISPLAY WINDOW: 5.0 W × 3.8 H cm centered at (4.4, 3.3) cm
    //    Three.js: X = 0.0 cm, Y = 6.2 - 3.3 = +2.9 cm
    //    (Spans x 1.9-6.9, y 1.4-5.2 -> 56.8% of body width, real 2.4" ST7789)
    // -------------------------------------------------------------------------
    const DISP_W = 5.0;
    const DISP_H = 3.8;
    const DISP_Y = hh - 3.3; // +2.9 cm

    // Display Bezel Housing (subtle outer chamfer framing the 2.4" window)
    const BEZEL_W = 5.4;
    const BEZEL_H = 4.2;
    const bShape = new THREE.Shape();
    const b_hw = BEZEL_W / 2;
    const b_hh = BEZEL_H / 2;
    const b_r = 0.25;
    bShape.moveTo(-b_hw + b_r, -b_hh);
    bShape.lineTo(b_hw - b_r, -b_hh);
    bShape.quadraticCurveTo(b_hw, -b_hh, b_hw, -b_hh + b_r);
    bShape.lineTo(b_hw, b_hh - b_r);
    bShape.quadraticCurveTo(b_hw, b_hh, b_hw - b_r, b_hh);
    bShape.lineTo(-b_hw + b_r, b_hh);
    bShape.quadraticCurveTo(-b_hw, b_hh, -b_hw, b_hh - b_r);
    bShape.lineTo(-b_hw, -b_hh + b_r);
    bShape.quadraticCurveTo(-b_hw, -b_hh, -b_hw + b_r, -b_hh);

    const bezelGeo = new THREE.ExtrudeGeometry(bShape, {
      depth: 0.06,
      bevelEnabled: true,
      bevelSegments: 4,
      bevelSize: 0.04,
      bevelThickness: 0.04,
    });
    bezelGeo.center();
    const bezelMesh = new THREE.Mesh(bezelGeo, this.materials.bezel);
    bezelMesh.position.set(0, DISP_Y, Z_FRONT + 0.03);
    this.rootGroup.add(bezelMesh);

    // Active ST7789 LCD Panel (5.0 × 3.8 cm active display window)
    const screenGeo = new THREE.PlaneGeometry(DISP_W, DISP_H);

    if (this.tftCanvas) {
      this.screenTexture = new THREE.CanvasTexture(this.tftCanvas);
      this.screenTexture.minFilter = THREE.LinearFilter;
      this.screenTexture.magFilter = THREE.NearestFilter;
    }

    // MeshBasicMaterial ensures TRUE 100% RETRO MINT COLORS without white blowout!
    const screenMat = new THREE.MeshBasicMaterial({
      map: this.screenTexture,
    });

    this.screenMesh = new THREE.Mesh(screenGeo, screenMat);
    this.screenMesh.position.set(0, DISP_Y, Z_FRONT + 0.065);
    this.rootGroup.add(this.screenMesh);

    // Glossy Protective Glass Lens overlay with subtle transparent specular reflection
    const lensGeo = new THREE.PlaneGeometry(DISP_W + 0.1, DISP_H + 0.1);
    const lensMat = new THREE.MeshPhysicalMaterial({
      roughness: 0.04,
      metalness: 0.0,
      clearcoat: 0.98,
      clearcoatRoughness: 0.03,
      transmission: 0.08,
      transparent: true,
      opacity: 0.15,
      reflectivity: 0.45,
    });
    const lensMesh = new THREE.Mesh(lensGeo, lensMat);
    lensMesh.position.set(0, DISP_Y, Z_FRONT + 0.075);
    this.rootGroup.add(lensMesh);

    // -------------------------------------------------------------------------
    // 3. LOGO STRIP: 5.0 W × 0.8 H cm at y 5.2-6.0 cm (Center y = 5.6 cm)
    //    Three.js: X = 0.0 cm, Y = 6.2 - 5.6 = +0.6 cm
    // -------------------------------------------------------------------------
    const logoTex = this.createLogoStripTexture();
    const logoGeo = new THREE.PlaneGeometry(5.0, 0.8);
    const logoMat = new THREE.MeshBasicMaterial({ map: logoTex, transparent: true });
    const logoMesh = new THREE.Mesh(logoGeo, logoMat);
    logoMesh.position.set(0, hh - 5.6, Z_FRONT + 0.015);
    this.rootGroup.add(logoMesh);

    // -------------------------------------------------------------------------
    // 4. D-PAD: 2.8 × 2.8 cm, arm width 0.95 cm centered at (2.2, 8.4) cm
    //    Three.js: X = 2.2 - 4.4 = -2.2 cm, Y = 6.2 - 8.4 = -2.2 cm
    // -------------------------------------------------------------------------
    const DPAD_X = 2.2 - hw; // -2.2 cm
    const DPAD_Y = hh - 8.4; // -2.2 cm
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
    // 5. ABXY SQUARE GRID DIAMOND (Aligned to 2.54 mm perfboard grid):
    //    Cap Ø0.9 cm (radius 0.45 cm)
    //    * X (Top):    (6.6, 7.4) -> Three.js: X = +2.2 cm, Y = -1.2 cm
    //    * Y (Left):   (5.6, 8.4) -> Three.js: X = +1.2 cm, Y = -2.2 cm
    //    * A (Right):  (7.6, 8.4) -> Three.js: X = +3.2 cm, Y = -2.2 cm
    //    * B (Bottom): (6.6, 9.4) -> Three.js: X = +2.2 cm, Y = -3.2 cm
    // -------------------------------------------------------------------------
    const CAP_RADIUS = 0.45;
    const CAP_HEIGHT = 0.26;

    const createActionButton = (name, key, label, refX, refY, bgColorStr, textColorStr) => {
      const x = refX - hw;
      const y = hh - refY;

      // 1. Soft Drop Shadow Mesh under each button (matching 2D box-shadow elevation!)
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

      // Button Stem (Cylinder) with high-gloss injection molded finish
      const cylGeo = new THREE.CylinderGeometry(CAP_RADIUS, CAP_RADIUS * 1.04, CAP_HEIGHT, 32);
      cylGeo.rotateX(Math.PI / 2);
      const cylMat = new THREE.MeshPhysicalMaterial({
        color: new THREE.Color(bgColorStr),
        roughness: 0.16,
        metalness: 0.02,
        clearcoat: 0.45,
        clearcoatRoughness: 0.06,
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

    createActionButton("X Button", "X", "X", 6.6, 7.4, "#4caf50", "#0f2620"); // Top
    createActionButton("Y Button", "Y", "Y", 5.6, 8.4, "#f48fb1", "#0f2620"); // Left
    createActionButton("A Button", "A", "A", 7.6, 8.4, "#52a6ba", "#0f2620"); // Right
    createActionButton("B Button", "B", "B", 6.6, 9.4, "#e8175d", "#ffffff"); // Bottom

    // -------------------------------------------------------------------------
    // 6. SELECT / START PILL BUTTONS: 1.4 × 0.5 cm pills
    //    * Select: (3.4, 11.0) -> Three.js: X = -1.0 cm, Y = -4.8 cm
    //    * Start:  (5.4, 11.0) -> Three.js: X = +1.0 cm, Y = -4.8 cm
    // -------------------------------------------------------------------------
    const createPill = (name, key, label, refX, refY) => {
      const x = refX - hw;
      const y = hh - refY;

      // Drop Shadow under Pill
      const pDropGeo = new THREE.PlaneGeometry(1.8, 0.9);
      const pDropTex = this.createDropShadowTexture(56);
      const pDropMat = new THREE.MeshBasicMaterial({ map: pDropTex, transparent: true, opacity: 0.52 });
      const pDropMesh = new THREE.Mesh(pDropGeo, pDropMat);
      pDropMesh.rotation.z = -0.44; // ~-25 degrees
      pDropMesh.position.set(x, y - 0.05, Z_FRONT + 0.01);
      this.rootGroup.add(pDropMesh);

      // Pill Mesh
      const pillGeo = new THREE.BoxGeometry(1.4, 0.5, 0.22);
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

    createPill("Select", "SELECT", "SELECT", 3.4, 11.0);
    createPill("Start", "START", "START", 5.4, 11.0);

    // -------------------------------------------------------------------------
    // 7. L / R SHOULDER BUTTONS: Side-Mounted Tact Switches on the Top Edge!
    //    Size: 2.2 × 0.6 cm each
    //    * L: (1.7, 0.3) -> Three.js: X = 1.7 - 4.4 = -2.7 cm, Y = 6.2 - 0.3 = +5.9 cm
    //    * R: (7.1, 0.3) -> Three.js: X = 7.1 - 4.4 = +2.7 cm, Y = 6.2 - 0.3 = +5.9 cm
    // -------------------------------------------------------------------------
    const createShoulder = (name, key, label, refX) => {
      const x = refX - hw;
      const y = hh - 0.3; // +5.9 cm

      // Ergonomic Bumper Mesh protruding above the top edge
      const sGeo = new THREE.BoxGeometry(2.2, 0.6, 1.4);
      const sMesh = new THREE.Mesh(sGeo, this.materials.shoulder);
      sMesh.position.set(x, y + 0.28, 0.5);
      sMesh.castShadow = true;
      sMesh.userData = { isButton: true, key: key, name: name, isShoulder: true };
      this.rootGroup.add(sMesh);

      // Embossed "L" / "R" Label on front face
      const fCanvas = document.createElement("canvas");
      fCanvas.width = 128;
      fCanvas.height = 64;
      const fCtx = fCanvas.getContext("2d");
      fCtx.fillStyle = "#1a4b42";
      fCtx.roundRect(0, 0, 128, 64, 10);
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
      const fGeo = new THREE.PlaneGeometry(2.1, 0.52);
      const fMat = new THREE.MeshBasicMaterial({ map: fTex, transparent: true });
      const fMesh = new THREE.Mesh(fGeo, fMat);
      fMesh.position.set(0, 0, 0.71);
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

    createShoulder("L Trigger", "L", "L", 1.7);
    createShoulder("R Trigger", "R", "R", 7.1);

    // -------------------------------------------------------------------------
    // 8. TOP EDGE USB-C CUTOUT: 1.0 × 0.45 cm at (4.4, 0.0) between L and R
    //    Three.js: X = 0.0 cm, Y = +6.2 cm
    // -------------------------------------------------------------------------
    const usbcGeo = new THREE.BoxGeometry(1.0, 0.45, 0.6);
    const usbcMat = new THREE.MeshBasicMaterial({ color: 0x0f2620 });
    const usbcHole = new THREE.Mesh(usbcGeo, usbcMat);
    usbcHole.position.set(0, hh, 0.4);
    this.rootGroup.add(usbcHole);

    const usbcTongueGeo = new THREE.BoxGeometry(0.7, 0.12, 0.35);
    const usbcTongue = new THREE.Mesh(usbcTongueGeo, this.materials.metal);
    usbcTongue.position.set(0, hh + 0.05, 0.4);
    this.rootGroup.add(usbcTongue);

    // -------------------------------------------------------------------------
    // 9. LEFT EDGE MICROSD SLOT: 1.3 × 0.25 cm at y ≈ 4.0 cm
    //    Three.js: X = -4.4 cm, Y = 6.2 - 4.0 = +2.2 cm
    // -------------------------------------------------------------------------
    const sdHoleGeo = new THREE.BoxGeometry(0.5, 0.25, 1.35);
    const sdHole = new THREE.Mesh(sdHoleGeo, usbcMat);
    sdHole.position.set(-hw, hh - 4.0, 0.4);
    this.rootGroup.add(sdHole);

    // -------------------------------------------------------------------------
    // 10. RIGHT EDGE POWER SLIDE SWITCH: ~1.0 × 0.4 cm at y ≈ 3.0 cm
    //     Three.js: X = +4.4 cm, Y = 6.2 - 3.0 = +3.2 cm
    // -------------------------------------------------------------------------
    const pwrHoleGeo = new THREE.BoxGeometry(0.4, 1.0, 0.45);
    const pwrHole = new THREE.Mesh(pwrHoleGeo, usbcMat);
    pwrHole.position.set(hw, hh - 3.0, 0.4);
    this.rootGroup.add(pwrHole);

    const pwrNubGeo = new THREE.BoxGeometry(0.25, 0.45, 0.35);
    const pwrNub = new THREE.Mesh(pwrNubGeo, this.materials.btnPill);
    pwrNub.position.set(hw + 0.08, hh - 2.8, 0.4);
    this.rootGroup.add(pwrNub);

    // -------------------------------------------------------------------------
    // 11. REAR 4×AA BATTERY DOOR: 3.4 H × 6.2 W cm centered at x 4.4, y 9.0 cm
    //     Three.js: X = 0.0 cm, Y = 6.2 - 9.0 = -2.8 cm, Z = -1.6 cm
    // -------------------------------------------------------------------------
    const doorGeo = new THREE.BoxGeometry(6.2, 3.4, 0.18);
    const doorMesh = new THREE.Mesh(doorGeo, this.materials.chassis);
    doorMesh.position.set(0, hh - 9.0, -D / 2);
    this.rootGroup.add(doorMesh);

    // Textured battery grip ridges
    for (let i = 0; i < 4; i++) {
      const slitGeo = new THREE.BoxGeometry(4.2, 0.1, 0.04);
      const slitMat = new THREE.MeshBasicMaterial({ color: 0x438f7a });
      const slit = new THREE.Mesh(slitGeo, slitMat);
      slit.position.set(0, hh - 9.0 + 0.6 - i * 0.35, -D / 2 - 0.09);
      this.rootGroup.add(slit);
    }

    // -------------------------------------------------------------------------
    // 12. SPEAKER GRILLE: 6 Diagonal Slits in Bottom-Right Corner
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
    let roughness = 0.28;
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
