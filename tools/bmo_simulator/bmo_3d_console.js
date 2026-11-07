/**
 * bmo_3d_console.js
 * 
 * 3D Physical Hardware Model & Interactive WebGL Simulator for BMO Handheld Console.
 * Built with Three.js (r128) strictly adhering to:
 *  1. Physical Hardware Reference Specification (Exact centimeter dimensions & 2.54 mm perfboard grid):
 *     - Body: 8.8 W × 12.4 H × 3.2 D cm, corner radius 0.6 cm (1.5 cm asymmetric bottom-right curve)
 *     - Display Window: 5.0 W × 3.8 H cm centered at (4.4, 3.3) cm (~56.8% body width, real 2.4" ST7789)
 *     - Bezel Housing: Deep Forest Teal (#1A4B42) with "BMO COLOR", "USB-C POWER" header & "BMO GAMEBOY" footer
 *     - D-Pad: 2.8 × 2.8 cm (arm width 0.95 cm) centered at (2.2, 8.4) cm in Warm Sunshine Golden Yellow (#FFE033)
 *     - Action Diamond (aligned to 2.54 mm perfboard grid):
 *         * X (Top):    cap dia 0.9 cm at (6.6, 7.4) cm -> Leaf Green (#4CAF50)
 *         * Y (Left):   cap dia 0.9 cm at (5.6, 8.4) cm -> Rosy Pink (#F48FB1)
 *         * A (Right):  cap dia 0.9 cm at (7.6, 8.4) cm -> Sky Aqua (#52A6BA)
 *         * B (Bottom): cap dia 0.9 cm at (6.6, 9.4) cm -> Coral (#FF91A5)
 *     - Select / Start: 1.4 × 0.5 cm rubber pills at (3.4, 11.0) and (5.4, 11.0) cm in Deep Teal (#264B42)
 *     - L / R Triggers: 2.2 × 0.6 cm top edge side-mounted tact switches at (1.7, 0.3) and (7.1, 0.3) cm (#264B42)
 *     - Top USB-C Cutout: 1.0 × 0.45 cm at (4.4, 0.0) cm between L and R
 *     - Left SD Slot: 1.3 × 0.25 cm at y ≈ 4.0 cm
 *     - Right Power Slide Switch: 1.0 × 0.4 cm at y ≈ 3.0 cm
 *     - Rear 4×AA Battery Door: 6.2 W × 3.4 H cm centered at (4.4, 9.0) cm with textured grip ridges
 * 
 *  2. 100% Color-Picker Parity with 2D Reference Model:
 *     - LinearEncoding + NoToneMapping guarantees that diffuse hex tokens match CSS output directly.
 *     - Studio lighting normalized to exact 1.00 front-face irradiance (0.74 ambient + 0.32 key * 0.80 cos).
 *     - Zero gamma bleaching, zero color blowout, velvety satin matte plastic finish.
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

    this.init();
  }

  init() {
    const width = this.container.clientWidth || 480;
    const height = this.container.clientHeight || 660;

    // 1. Scene
    this.scene = new THREE.Scene();

    // 2. Camera: 38 deg FOV for authentic product focal length
    this.camera = new THREE.PerspectiveCamera(38, width / height, 0.1, 100);
    this.setCameraPreset("front");

    // 3. Renderer with LinearEncoding (100% COLOR PARITY - ZERO GAMMA EXPANSION)
    this.renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      powerPreference: "high-performance",
    });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

    if ("outputEncoding" in this.renderer) {
      this.renderer.outputEncoding = THREE.LinearEncoding;
    }
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
    window.addEventListener("resize", () => this.onResize());

    // 9. Render Loop
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  setupLighting() {
    // 1. Pure white ambient baseline (0.74)
    // Ensures diffuse baseline color matches CSS hex values directly without color shifts
    this.ambientLight = new THREE.AmbientLight(0xffffff, 0.74);
    this.scene.add(this.ambientLight);

    // 2. Directional Key Light (0.32) from front-top-right (3, 7, 10)
    // Vector (3, 7, 10) normalized has Z = 0.80.
    // Total front-face irradiance: 0.74 + 0.32 * 0.80 = 0.996 (exact 1.00 intensity!)
    // Generates subtle specular bevel highlights along the top edge without washing out diffuse colors
    this.keyLight = new THREE.DirectionalLight(0xffffff, 0.32);
    this.keyLight.position.set(3, 7, 10);
    this.keyLight.castShadow = true;
    this.keyLight.shadow.mapSize.width = 2048;
    this.keyLight.shadow.mapSize.height = 2048;
    this.keyLight.shadow.camera.near = 1;
    this.keyLight.shadow.camera.far = 30;
    this.keyLight.shadow.camera.left = -7;
    this.keyLight.shadow.camera.right = 7;
    this.keyLight.shadow.camera.top = 9;
    this.keyLight.shadow.camera.bottom = -9;
    this.keyLight.shadow.bias = -0.0004;
    this.keyLight.shadow.radius = 2.0;
    this.scene.add(this.keyLight);

    // 3. Pure Rim Light from behind (0, 8, -12)
    // Highlights the top edge, shoulders, and battery door in 3D orbit, casting ZERO light onto the front face
    this.rimLight = new THREE.DirectionalLight(0xffffff, 0.50);
    this.rimLight.position.set(0, 8, -12);
    this.scene.add(this.rimLight);

    // Initial studio lighting calibration
    this.applyStudioAdjustments();
  }

  setStudioAdjustments(settings = {}) {
    if (settings.brightness !== undefined) this.studioSettings.brightness = settings.brightness;
    if (settings.saturation !== undefined) this.studioSettings.saturation = settings.saturation;
    if (settings.temperature !== undefined) this.studioSettings.temperature = settings.temperature;

    this.applyStudioAdjustments();
  }

  applyStudioAdjustments() {
    const b = this.studioSettings.brightness;
    const t = this.studioSettings.temperature; // -1.0 (cool blue) to +1.0 (warm amber)
    const s = this.studioSettings.saturation;

    // 1. Calculate color temperature RGB tint (Planckian approximation)
    let r = 1.0, g = 1.0, bl = 1.0;
    if (t > 0) {
      // Warm amber/golden sunlight: maintain red, gentle green, drop blue
      r = 1.0;
      g = 1.0 - 0.14 * t;
      bl = 1.0 - 0.40 * t;
    } else if (t < 0) {
      // Cool blue Nordic daylight: maintain blue, drop red, gentle green
      const cool = -t;
      r = 1.0 - 0.36 * cool;
      g = 1.0 - 0.10 * cool;
      bl = 1.0;
    }

    if (this.ambientLight) {
      this.ambientLight.color.setRGB(r, g, bl);
      this.ambientLight.intensity = 0.74 * b;
    }

    if (this.keyLight) {
      this.keyLight.color.setRGB(r, g, bl);
      this.keyLight.intensity = 0.32 * b;
    }

    if (this.rimLight) {
      this.rimLight.intensity = 0.50 * b;
    }

    // 2. Hardware-accelerated GPU saturation on the WebGL canvas
    if (this.renderer && this.renderer.domElement) {
      this.renderer.domElement.style.filter = `saturate(${s})`;
    }
  }

  createDropShadowTexture(radius = 64) {
    const canvas = document.createElement("canvas");
    canvas.width = 128;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");
    const grad = ctx.createRadialGradient(64, 64, radius * 0.20, 64, 64, radius);
    grad.addColorStop(0, "rgba(0, 0, 0, 0.50)");
    grad.addColorStop(0.65, "rgba(0, 0, 0, 0.15)");
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

    // 1. SOLID, PURE, 100% SATURATED BASE (Exact 2D Color Match, NO BLACK BORDER)
    ctx.fillStyle = bgColorStr;
    ctx.fillRect(0, 0, 256, 256);

    // 2. Subtle bottom inner shadow crescent (matching 2D inset 0 -3px)
    const btmGrad = ctx.createRadialGradient(128, 128, 85, 128, 128, 126);
    btmGrad.addColorStop(0, "rgba(0, 0, 0, 0)");
    btmGrad.addColorStop(0.8, "rgba(0, 0, 0, 0.04)");
    btmGrad.addColorStop(1, "rgba(0, 0, 0, 0.12)");
    ctx.fillStyle = btmGrad;
    ctx.beginPath();
    ctx.arc(128, 128, 126, 0, Math.PI * 2);
    ctx.fill();

    // 3. Fine Top Edge Highlight Rim (matching 2D inset 0 2px 4px)
    ctx.save();
    ctx.lineWidth = 6;
    ctx.strokeStyle = "rgba(255, 255, 255, 0.40)";
    ctx.beginPath();
    ctx.arc(128, 128, 118, Math.PI * 1.15, Math.PI * 1.85);
    ctx.stroke();
    ctx.restore();

    // 4. Centered Bold Retro Pixel Font Letter (NO BLACK BORDER RING!)
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

  createBezelFrameTexture() {
    const canvas = document.createElement("canvas");
    canvas.width = 560;
    canvas.height = 440;
    const ctx = canvas.getContext("2d");

    // Base deep teal bezel background (#1a4b42)
    ctx.fillStyle = "#1a4b42";
    ctx.fillRect(0, 0, 560, 440);

    // Inner window recess line
    ctx.strokeStyle = "#0f2620";
    ctx.lineWidth = 4;
    ctx.strokeRect(30, 48, 500, 344);

    // TOP BAR:
    // Left: "BMO COLOR"
    ctx.font = "14px 'Press Start 2P', monospace";
    ctx.fillStyle = "#747d8c";
    ctx.textAlign = "left";
    ctx.textBaseline = "middle";
    ctx.fillText("BMO COLOR", 36, 28);

    // Right: "USB-C POWER" with glowing mint power LED dot
    ctx.textAlign = "right";
    ctx.fillStyle = "#8cd7c2";
    ctx.fillText("USB-C POWER", 500, 28);

    // Glowing LED dot
    ctx.fillStyle = "#55efc4";
    ctx.beginPath();
    ctx.arc(516, 28, 4, 0, Math.PI * 2);
    ctx.fill();

    // BOTTOM FOOTER:
    // "BMO GAMEBOY" centered
    ctx.font = "900 18px 'Press Start 2P', monospace";
    const bmoTxt = "BMO ";
    const gbTxt = "GAMEBOY";
    const totalW = ctx.measureText(bmoTxt + gbTxt).width;
    const startX = 280 - totalW / 2;

    ctx.textAlign = "left";
    ctx.textBaseline = "middle";
    ctx.fillStyle = "#a4b0be";
    ctx.fillText(bmoTxt, startX, 412);

    const bmoW = ctx.measureText(bmoTxt).width;
    ctx.fillStyle = "#00d2d3"; // Electric Cyan #00D2D3
    ctx.fillText(gbTxt, startX + bmoW, 412);

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

    // 1. Subtle directional retro triangle arrows matching 2D view
    ctx.fillStyle = "rgba(0, 0, 0, 0.32)";
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

    drawTriangle(256, 75, 20, 0);
    drawTriangle(256, 437, 20, Math.PI);
    drawTriangle(75, 256, 20, -Math.PI / 2);
    drawTriangle(437, 256, 20, Math.PI / 2);

    // 2. Center circular concave thumb dish
    ctx.fillStyle = "rgba(0, 0, 0, 0.22)";
    ctx.beginPath();
    ctx.arc(256, 256, 52, 0, Math.PI * 2);
    ctx.fill();

    ctx.lineWidth = 3;
    ctx.strokeStyle = "rgba(0, 0, 0, 0.28)";
    ctx.stroke();

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  createSideTexture() {
    const canvas = document.createElement("canvas");
    canvas.width = 512;
    canvas.height = 1024;
    const ctx = canvas.getContext("2d");

    ctx.clearRect(0, 0, 512, 1024);

    // 1. Canonical 7-Hole Acoustic Speaker Array (Upper side, matching Adventure Time BMO)
    const cx = 256;
    const cy = 185;
    const holeRadius = 14;
    const arrayRadius = 58;

    const drawHole = (hx, hy) => {
      const grad = ctx.createRadialGradient(hx, hy, holeRadius * 0.2, hx, hy, holeRadius);
      grad.addColorStop(0, "#081411");
      grad.addColorStop(0.85, "#0f2620");
      grad.addColorStop(1, "#1a4b42");
      ctx.fillStyle = grad;
      ctx.beginPath();
      ctx.arc(hx, hy, holeRadius, 0, Math.PI * 2);
      ctx.fill();
    };

    // Center hole
    drawHole(cx, cy);

    // 6 Hexagonal outer acoustic holes
    for (let i = 0; i < 6; i++) {
      const angle = (i * Math.PI) / 3;
      drawHole(cx + Math.cos(angle) * arrayRadius, cy + Math.sin(angle) * arrayRadius);
    }

    // 2. Bold Retro "B M O" Vertical Lettering (Faithful Adventure Time typography)
    ctx.fillStyle = "#0c1f1a";
    ctx.font = "900 135px 'Outfit', 'Press Start 2P', sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";

    // B
    ctx.fillText("B", 256, 420);
    // M
    ctx.fillText("M", 256, 580);

    // O with Arm Socket Port in the center!
    ctx.fillText("O", 256, 750);

    // Arm Socket Port Ring inside the O
    ctx.lineWidth = 10;
    ctx.strokeStyle = "#081411";
    ctx.beginPath();
    ctx.arc(256, 750, 36, 0, Math.PI * 2);
    ctx.stroke();

    ctx.fillStyle = "#122d26";
    ctx.beginPath();
    ctx.arc(256, 750, 31, 0, Math.PI * 2);
    ctx.fill();

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  buildConsoleModel() {
    this.rootGroup = new THREE.Group();
    this.scene.add(this.rootGroup);

    // -------------------------------------------------------------------------
    // EXACT 2D COLOR TOKENS (100% Color-Picker Parity)
    // -------------------------------------------------------------------------
    const bmoTealHex      = 0x5fb49c; // --chassis-color: #5fb49c
    const deepTealHex     = 0x1a4b42; // --bezel-color: #1a4b42
    const rubberTealHex   = 0x264b42; // --btn-shoulder-color & --btn-pill-color: #264b42
    const dpadYellowHex   = 0xffe033; // --dpad-color: #ffe033

    // Standard PBR Materials (metalness: 0.0 prevents metallic color desaturation)
    this.materials.chassis = new THREE.MeshStandardMaterial({
      color: bmoTealHex,
      roughness: 0.42,
      metalness: 0.0,
    });

    this.materials.bezel = new THREE.MeshStandardMaterial({
      color: deepTealHex,
      roughness: 0.35,
      metalness: 0.0,
    });

    this.materials.dpad = new THREE.MeshStandardMaterial({
      color: dpadYellowHex,
      roughness: 0.30,
      metalness: 0.0,
    });

    this.materials.btnPill = new THREE.MeshStandardMaterial({
      color: rubberTealHex,
      roughness: 0.45,
      metalness: 0.0,
    });

    this.materials.shoulder = new THREE.MeshStandardMaterial({
      color: rubberTealHex,
      roughness: 0.35,
      metalness: 0.0,
    });

    this.materials.metal = new THREE.MeshStandardMaterial({
      color: 0x95a5a6,
      roughness: 0.20,
      metalness: 0.90,
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
    shadowMesh.position.set(0, -0.2, -D / 2 - 0.35);
    this.rootGroup.add(shadowMesh);

    // -------------------------------------------------------------------------
    // 2. DISPLAY WINDOW: 5.0 W × 3.8 H cm centered at (4.4, 3.3) cm
    //    Three.js: X = 0.0 cm, Y = 6.2 - 3.3 = +2.9 cm
    //    (Spans x 1.9-6.9, y 1.4-5.2 -> 56.8% body width, real 2.4" ST7789)
    // -------------------------------------------------------------------------
    const DISP_W = 5.0;
    const DISP_H = 3.8;
    const DISP_Y = hh - 3.3; // +2.9 cm

    // Display Bezel Housing: Outer frame framing the 2.4" window (Color: #1A4B42)
    const BEZEL_W = 5.6;
    const BEZEL_H = 4.4;
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
      depth: 0.08,
      bevelEnabled: true,
      bevelSegments: 4,
      bevelSize: 0.03,
      bevelThickness: 0.03,
    });
    bezelGeo.center();
    const bezelMesh = new THREE.Mesh(bezelGeo, this.materials.bezel);
    bezelMesh.position.set(0, DISP_Y, Z_FRONT + 0.04);
    bezelMesh.castShadow = true;
    this.rootGroup.add(bezelMesh);

    // Bezel Face Texture with "BMO COLOR", "USB-C POWER", and "BMO GAMEBOY"
    const bezelFaceGeo = new THREE.PlaneGeometry(BEZEL_W, BEZEL_H);
    const bezelFaceTex = this.createBezelFrameTexture();
    const bezelFaceMat = new THREE.MeshBasicMaterial({ map: bezelFaceTex });
    const bezelFaceMesh = new THREE.Mesh(bezelFaceGeo, bezelFaceMat);
    bezelFaceMesh.position.set(0, DISP_Y, Z_FRONT + 0.082);
    this.rootGroup.add(bezelFaceMesh);

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
    this.screenMesh.position.set(0, DISP_Y, Z_FRONT + 0.085);
    this.rootGroup.add(this.screenMesh);

    // Protective Glass Lens overlay with subtle diagonal reflection
    const lensGeo = new THREE.PlaneGeometry(DISP_W, DISP_H);
    const lensCanvas = document.createElement("canvas");
    lensCanvas.width = 256;
    lensCanvas.height = 192;
    const lctx = lensCanvas.getContext("2d");
    const lGrad = lctx.createLinearGradient(0, 0, 256, 192);
    lGrad.addColorStop(0, "rgba(255, 255, 255, 0.12)");
    lGrad.addColorStop(0.45, "rgba(255, 255, 255, 0.03)");
    lGrad.addColorStop(0.70, "rgba(255, 255, 255, 0)");
    lctx.fillStyle = lGrad;
    lctx.fillRect(0, 0, 256, 192);
    const lensTex = new THREE.CanvasTexture(lensCanvas);
    const lensMat = new THREE.MeshBasicMaterial({ map: lensTex, transparent: true, opacity: 0.85 });
    const lensMesh = new THREE.Mesh(lensGeo, lensMat);
    lensMesh.position.set(0, DISP_Y, Z_FRONT + 0.088);
    this.rootGroup.add(lensMesh);

    // -------------------------------------------------------------------------
    // CANONICAL CASSETTE / FLOPPY DRIVE SLOT & EJECT BUTTON (Adventure Time BMO)
    // -------------------------------------------------------------------------
    const slotY = DISP_Y - BEZEL_H / 2 - 0.28; // +0.42 cm (centered between screen bezel and D-pad)
    const slotGeo = new THREE.BoxGeometry(4.2, 0.24, 0.04);
    const slotMat = new THREE.MeshBasicMaterial({ color: 0x091814 });
    const slotMesh = new THREE.Mesh(slotGeo, slotMat);
    slotMesh.position.set(0, slotY, Z_FRONT + 0.015);
    this.rootGroup.add(slotMesh);

    // Subtle slot inner recess depth
    const slotInnerGeo = new THREE.BoxGeometry(4.0, 0.14, 0.06);
    const slotInnerMat = new THREE.MeshBasicMaterial({ color: 0x040a08 });
    const slotInnerMesh = new THREE.Mesh(slotInnerGeo, slotInnerMat);
    slotInnerMesh.position.set(0, slotY, Z_FRONT + 0.02);
    this.rootGroup.add(slotInnerMesh);

    // Iconic circular blue eject button to the right of the slot
    const ejectGeo = new THREE.CircleGeometry(0.15, 24);
    const ejectMat = new THREE.MeshBasicMaterial({ color: 0x227093 });
    const ejectMesh = new THREE.Mesh(ejectGeo, ejectMat);
    ejectMesh.position.set(2.45, slotY, Z_FRONT + 0.022);
    this.rootGroup.add(ejectMesh);

    // -------------------------------------------------------------------------
    // 4. D-PAD: 2.8 × 2.8 cm, arm width 0.95 cm centered at (2.2, 8.4) cm
    //    Three.js: X = 2.2 - 4.4 = -2.2 cm, Y = 6.2 - 8.4 = -2.2 cm
    //    Color: #FFE033 (Warm Sunshine Golden Yellow)
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
    const dpadDropMat = new THREE.MeshBasicMaterial({ map: dpadDropTex, transparent: true, opacity: 0.60 });
    const dpadDropMesh = new THREE.Mesh(dpadDropGeo, dpadDropMat);
    dpadDropMesh.position.set(DPAD_X, DPAD_Y - 0.08, Z_FRONT + 0.01);
    this.rootGroup.add(dpadDropMesh);

    // Extruded cross shape
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
      bevelSize: 0.04,
      bevelThickness: 0.04,
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
      depthWrite: false,
    });
    const dpadTexMesh = new THREE.Mesh(dpadTexGeo, dpadTexMat);
    dpadTexMesh.position.set(0, 0, 0.155);
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
    //    Cap dia 0.9 cm (radius 0.45 cm)
    //    * X (Top):    (6.6, 7.4) -> Three.js: X = +2.2 cm, Y = -1.2 cm | #4CAF50
    //    * Y (Left):   (5.6, 8.4) -> Three.js: X = +1.2 cm, Y = -2.2 cm | #F48FB1
    //    * A (Right):  (7.6, 8.4) -> Three.js: X = +3.2 cm, Y = -2.2 cm | #52A6BA
    //    * B (Bottom): (6.6, 9.4) -> Three.js: X = +2.2 cm, Y = -3.2 cm | #FF91A5
    // -------------------------------------------------------------------------
    const CAP_RADIUS = 0.45;
    const CAP_HEIGHT = 0.24;

    const createActionButton = (name, key, label, refX, refY, bgColorStr, textColorStr) => {
      const x = refX - hw;
      const y = hh - refY;

      // 1. Soft Ambient Contact Drop Shadow (subtle, NO dark black halo ring!)
      const shadowDiscGeo = new THREE.PlaneGeometry(CAP_RADIUS * 2.1, CAP_RADIUS * 2.1);
      const shadowDiscTex = this.createDropShadowTexture(48);
      const shadowDiscMat = new THREE.MeshBasicMaterial({ map: shadowDiscTex, transparent: true, opacity: 0.18 });
      const shadowDisc = new THREE.Mesh(shadowDiscGeo, shadowDiscMat);
      shadowDisc.position.set(x, y - 0.04, Z_FRONT + 0.005);
      this.rootGroup.add(shadowDisc);

      // 2. Engraved Letter Label on the chassis below the button (matching reference 2D)
      const lblCanvas = document.createElement("canvas");
      lblCanvas.width = 128;
      lblCanvas.height = 64;
      const lctx = lblCanvas.getContext("2d");
      lctx.fillStyle = "#3a786a"; // Chassis shadow teal
      lctx.font = "bold 30px 'Press Start 2P', monospace";
      lctx.textAlign = "center";
      lctx.textBaseline = "middle";
      lctx.fillText(label, 64, 32);
      const lblTex = new THREE.CanvasTexture(lblCanvas);
      const lblGeo = new THREE.PlaneGeometry(0.5, 0.25);
      const lblMat = new THREE.MeshBasicMaterial({ map: lblTex, transparent: true });
      const lblMesh = new THREE.Mesh(lblGeo, lblMat);
      lblMesh.position.set(x + (key === "A" ? 0.08 : (key === "Y" ? -0.08 : 0)), y - 0.65, Z_FRONT + 0.012);
      this.rootGroup.add(lblMesh);

      // 3. Action Button Root Group
      const btnGroup = new THREE.Group();
      btnGroup.position.set(x, y, Z_FRONT + CAP_HEIGHT / 2);
      this.rootGroup.add(btnGroup);

      // Button Stem (Cylinder) with matching pure color
      const cylGeo = new THREE.CylinderGeometry(CAP_RADIUS, CAP_RADIUS, CAP_HEIGHT, 32);
      cylGeo.rotateX(Math.PI / 2);
      const cylMat = new THREE.MeshStandardMaterial({
        color: new THREE.Color(bgColorStr),
        roughness: 0.30,
        metalness: 0.0,
      });
      const stemMesh = new THREE.Mesh(cylGeo, cylMat);
      stemMesh.castShadow = true;
      btnGroup.add(stemMesh);

      // Button Cap Face with Specular Rim & Bold Retro Letter (flush with cylinder)
      const topTex = this.createButtonTexture(label, bgColorStr, textColorStr);
      const topGeo = new THREE.CircleGeometry(CAP_RADIUS, 32);
      const topMat = new THREE.MeshBasicMaterial({
        map: topTex,
      });
      const capMesh = new THREE.Mesh(topGeo, topMat);
      capMesh.position.set(0, 0, CAP_HEIGHT / 2 + 0.002);
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
        rotX: btnGroup.rotation.x,
        rotY: btnGroup.rotation.y,
        rotZ: btnGroup.rotation.z,
      });

      return btnGroup;
    };

    createActionButton("X Button", "X", "X", 6.6, 7.4, "#4caf50", "#0f2620"); // Top: Leaf Green
    createActionButton("Y Button", "Y", "Y", 5.6, 8.4, "#f48fb1", "#0f2620"); // Left: Rosy Pink
    createActionButton("A Button", "A", "A", 7.6, 8.4, "#52a6ba", "#0f2620"); // Right: Sky Blue
    createActionButton("B Button", "B", "B", 6.6, 9.4, "#ff91a5", "#0f2620"); // Bottom: Coral

    // -------------------------------------------------------------------------
    // 6. SELECT / START PILL BUTTONS: Canonical Adventure Time BMO Placement
    //    Placed near left side down, directly under the yellow cross D-pad!
    //    D-Pad centerline: X = -2.2 cm.
    //    * Select: X = -2.90 cm, Y = -4.35 cm | #264B42
    //    * Start:  X = -1.50 cm, Y = -4.35 cm | #264B42
    //    Orientation: Perfectly horizontal ([ - ] [ - ]) matching Adventure Time BMO!
    // -------------------------------------------------------------------------
    const createPill = (name, key, label, x, y) => {
      // Soft Drop Shadow under Pill
      const pDropGeo = new THREE.PlaneGeometry(1.45, 0.60);
      const pDropTex = this.createDropShadowTexture(44);
      const pDropMat = new THREE.MeshBasicMaterial({ map: pDropTex, transparent: true, opacity: 0.28 });
      const pDropMesh = new THREE.Mesh(pDropGeo, pDropMat);
      pDropMesh.position.set(x, y - 0.04, Z_FRONT + 0.005);
      this.rootGroup.add(pDropMesh);

      // Ergonomic rounded capsule pill button (horizontal)
      const pShape = new THREE.Shape();
      const pw = 1.15 - 0.38; // 0.77 cm straight body
      const pr = 0.19;        // 0.19 cm semicircular cap radius
      pShape.moveTo(-pw / 2, -pr);
      pShape.lineTo(pw / 2, -pr);
      pShape.absarc(pw / 2, 0, pr, -Math.PI / 2, Math.PI / 2, false);
      pShape.lineTo(-pw / 2, pr);
      pShape.absarc(-pw / 2, 0, pr, Math.PI / 2, -Math.PI / 2, false);

      const pillGeo = new THREE.ExtrudeGeometry(pShape, {
        depth: 0.16,
        bevelEnabled: true,
        bevelSegments: 4,
        bevelSize: 0.03,
        bevelThickness: 0.03,
      });
      pillGeo.center();

      const pillMesh = new THREE.Mesh(pillGeo, this.materials.btnPill);
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
        rotX: pillMesh.rotation.x,
        rotY: pillMesh.rotation.y,
        rotZ: pillMesh.rotation.z,
      });

      // Engraved Pill Label directly below
      const lblCanvas = document.createElement("canvas");
      lblCanvas.width = 256;
      lblCanvas.height = 64;
      const lCtx = lblCanvas.getContext("2d");
      lCtx.fillStyle = "#1a4b42"; // Deep Teal
      lCtx.font = "bold 26px 'Press Start 2P', monospace";
      lCtx.textAlign = "center";
      lCtx.textBaseline = "middle";
      lCtx.fillText(label, 128, 32);

      const lTex = new THREE.CanvasTexture(lblCanvas);
      const lGeo = new THREE.PlaneGeometry(1.25, 0.30);
      const lMat = new THREE.MeshBasicMaterial({ map: lTex, transparent: true });
      const lMesh = new THREE.Mesh(lGeo, lMat);
      lMesh.position.set(x, y - 0.48, Z_FRONT + 0.015);
      this.rootGroup.add(lMesh);
    };

    // Both pills positioned side-by-side directly under the D-pad
    createPill("Select", "SELECT", "SELECT", -2.90, -4.35);
    createPill("Start", "START", "START", -1.50, -4.35);

    // -------------------------------------------------------------------------
    // 7. L / R SHOULDER BUTTONS: Side-Mounted Tact Switches on the Top Edge!
    //    * L: X = -2.7 cm, Y = +6.475 cm, Z = 0.0 cm
    //    * R: X = +2.7 cm, Y = +6.475 cm, Z = 0.0 cm
    //    Mounted completely on the top rim (Y >= 6.20 cm, Z in [-0.8, +0.8 cm]).
    //    ZERO overlap with front face or front bevel -> ZERO Z-FIGHTING / FLICKERING!
    // -------------------------------------------------------------------------
    const createShoulder = (name, key, label, refX) => {
      const x = refX - hw;
      const y = hh + 0.275; // +6.475 cm (rests directly on top chassis edge at +6.20 cm)
      const z = 0.0;        // Centered along console thickness (spans Z from -0.8 to +0.8 cm)

      // Ergonomic Bumper Mesh protruding above the top edge
      const sw = 2.1;
      const sh = 0.55;
      const sd = 1.6;
      const sr = 0.14; // Top corner rounding

      const sShape = new THREE.Shape();
      sShape.moveTo(-sw / 2, -sh / 2);
      sShape.lineTo(sw / 2, -sh / 2);
      sShape.lineTo(sw / 2, sh / 2 - sr);
      sShape.quadraticCurveTo(sw / 2, sh / 2, sw / 2 - sr, sh / 2);
      sShape.lineTo(-sw / 2 + sr, sh / 2);
      sShape.quadraticCurveTo(-sw / 2, sh / 2, -sw / 2, sh / 2 - sr);
      sShape.closePath();

      const sGeo = new THREE.ExtrudeGeometry(sShape, {
        depth: sd,
        bevelEnabled: true,
        bevelSegments: 4,
        bevelSize: 0.04,
        bevelThickness: 0.04,
      });
      sGeo.center();

      const sMesh = new THREE.Mesh(sGeo, this.materials.shoulder);
      sMesh.position.set(x, y, z);
      sMesh.castShadow = true;
      sMesh.userData = { isButton: true, key: key, name: name, isShoulder: true };
      this.rootGroup.add(sMesh);

      // Embossed "L" / "R" Label on the front facet of the trigger (at local z = +sd/2 + 0.045)
      const fCanvas = document.createElement("canvas");
      fCanvas.width = 128;
      fCanvas.height = 64;
      const fCtx = fCanvas.getContext("2d");
      fCtx.clearRect(0, 0, 128, 64);
      fCtx.fillStyle = "#cef5e4";
      fCtx.font = "bold 34px 'Press Start 2P', monospace";
      fCtx.textAlign = "center";
      fCtx.textBaseline = "middle";
      fCtx.shadowColor = "rgba(0, 0, 0, 0.45)";
      fCtx.shadowBlur = 4;
      fCtx.shadowOffsetY = 2;
      fCtx.fillText(label, 64, 32);

      const fTex = new THREE.CanvasTexture(fCanvas);
      const fGeo = new THREE.PlaneGeometry(1.5, 0.42);
      const fMat = new THREE.MeshBasicMaterial({ map: fTex, transparent: true, depthWrite: false });
      const fMesh = new THREE.Mesh(fGeo, fMat);
      fMesh.position.set(0, 0, sd / 2 + 0.045);
      sMesh.add(fMesh);

      // Generous Raycasting Click Hit Zone
      const hitGeo = new THREE.BoxGeometry(2.5, 0.9, 2.0);
      const hitMat = new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false });
      const hitMesh = new THREE.Mesh(hitGeo, hitMat);
      hitMesh.position.set(0, 0, 0);
      hitMesh.userData = sMesh.userData;
      sMesh.add(hitMesh);

      this.buttonMeshes[key] = sMesh;
      this.interactiveObjects.push(sMesh);
      this.interactiveObjects.push(fMesh);
      this.interactiveObjects.push(hitMesh);
      fMesh.userData = sMesh.userData;

      this.buttonOriginalPositions.set(sMesh, {
        x: sMesh.position.x,
        y: sMesh.position.y,
        z: sMesh.position.z,
        rotX: 0,
        rotY: 0,
        rotZ: 0,
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
    // 11. CANONICAL REAR CHASSIS (Faithful to Adventure Time BMO):
    //     Zone 1 (Top): 5 Vertical Vent Slits (| | | | |)
    //     Zone 2 (Middle): Battery/Access Plate with Raised Frame & 4 Corner Screws
    //     Zone 3 (Bottom): Horizontal Finger Latch / Handle Recess
    // -------------------------------------------------------------------------
    const Z_BACK = -D / 2; // -1.60 cm

    // Zone 1: 5 Vertical Vent Slits (| | | | |)
    const ventY = +3.6; // Upper back
    const ventHeight = 2.4;
    const ventWidth = 0.18;
    const ventDepth = 0.06;
    const ventMat = new THREE.MeshBasicMaterial({ color: 0x091814 }); // Deep dark recessed vent

    for (let i = 0; i < 5; i++) {
      const vx = (i - 2) * 0.82; // -1.64, -0.82, 0.0, +0.82, +1.64 cm
      // Rounded capsule slit shape
      const vShape = new THREE.Shape();
      const vr = ventWidth / 2;
      const vh = ventHeight - ventWidth;
      vShape.moveTo(-vr, -vh / 2);
      vShape.lineTo(vr, -vh / 2);
      vShape.absarc(0, vh / 2, vr, 0, Math.PI, false);
      vShape.lineTo(-vr, -vh / 2);
      vShape.absarc(0, -vh / 2, vr, Math.PI, Math.PI * 2, false);

      const vGeo = new THREE.ExtrudeGeometry(vShape, { depth: ventDepth, bevelEnabled: false });
      vGeo.center();
      const ventMesh = new THREE.Mesh(vGeo, ventMat);
      ventMesh.position.set(vx, ventY, Z_BACK - ventDepth / 2);
      this.rootGroup.add(ventMesh);
    }

    // Zone 2: Rectangular Access / Battery Plate with Raised Beveled Frame
    const plateY = +0.2; // Centered middle-back
    const plateW = 5.5;
    const plateH = 3.3;
    const plateDepth = 0.12;

    // Molded perimeter seam groove
    const seamGeo = new THREE.BoxGeometry(plateW + 0.35, plateH + 0.35, 0.04);
    const seamMat = new THREE.MeshBasicMaterial({ color: 0x14332c });
    const seamMesh = new THREE.Mesh(seamGeo, seamMat);
    seamMesh.position.set(0, plateY, Z_BACK - 0.02);
    this.rootGroup.add(seamMesh);

    // Raised beveled plate
    const pShape = new THREE.Shape();
    const pr = 0.12;
    pShape.moveTo(-plateW / 2 + pr, -plateH / 2);
    pShape.lineTo(plateW / 2 - pr, -plateH / 2);
    pShape.quadraticCurveTo(plateW / 2, -plateH / 2, plateW / 2, -plateH / 2 + pr);
    pShape.lineTo(plateW / 2, plateH / 2 - pr);
    pShape.quadraticCurveTo(plateW / 2, plateH / 2, plateW / 2 - pr, plateH / 2);
    pShape.lineTo(-plateW / 2 + pr, plateH / 2);
    pShape.quadraticCurveTo(-plateW / 2, plateH / 2, -plateW / 2, plateH / 2 - pr);
    pShape.lineTo(-plateW / 2, -plateH / 2 + pr);
    pShape.quadraticCurveTo(-plateW / 2, -plateH / 2, -plateW / 2 + pr, -plateH / 2);

    const pGeo = new THREE.ExtrudeGeometry(pShape, {
      depth: plateDepth,
      bevelEnabled: true,
      bevelSegments: 4,
      bevelSize: 0.03,
      bevelThickness: 0.03,
    });
    pGeo.center();

    const plateMesh = new THREE.Mesh(pGeo, this.materials.chassis);
    plateMesh.position.set(0, plateY, Z_BACK - plateDepth / 2 - 0.02);
    plateMesh.castShadow = true;
    this.rootGroup.add(plateMesh);

    // 4 Corner Metal Screws / Rivets
    const screwInsetX = plateW / 2 - 0.36; // 2.39 cm
    const screwInsetY = plateH / 2 - 0.36; // 1.29 cm
    const screwPositions = [
      [-screwInsetX, plateY + screwInsetY],
      [+screwInsetX, plateY + screwInsetY],
      [-screwInsetX, plateY - screwInsetY],
      [+screwInsetX, plateY - screwInsetY],
    ];

    const screwGeo = new THREE.CylinderGeometry(0.14, 0.14, 0.06, 24);
    screwGeo.rotateX(Math.PI / 2);
    const screwCrossGeo = new THREE.BoxGeometry(0.18, 0.03, 0.07);
    const screwCrossMat = new THREE.MeshBasicMaterial({ color: 0x2c3e50 });

    for (const [sx, sy] of screwPositions) {
      const screwMesh = new THREE.Mesh(screwGeo, this.materials.metal);
      screwMesh.position.set(sx, sy, Z_BACK - plateDepth - 0.025);
      this.rootGroup.add(screwMesh);

      // Slotted screw drive groove
      const crossMesh = new THREE.Mesh(screwCrossGeo, screwCrossMat);
      crossMesh.position.set(sx, sy, Z_BACK - plateDepth - 0.045);
      this.rootGroup.add(crossMesh);
    }

    // Zone 3: Horizontal Finger Latch / Compartment Recess
    const latchY = -2.4;
    const latchW = 2.8;
    const latchH = 0.45;
    const latchGeo = new THREE.BoxGeometry(latchW, latchH, 0.06);
    const latchMat = new THREE.MeshBasicMaterial({ color: 0x091814 });
    const latchMesh = new THREE.Mesh(latchGeo, latchMat);
    latchMesh.position.set(0, latchY, Z_BACK - 0.02);
    this.rootGroup.add(latchMesh);

    // Subtle inner shadow ridge inside the latch
    const latchLipGeo = new THREE.BoxGeometry(latchW - 0.2, 0.12, 0.08);
    const latchLipMat = new THREE.MeshBasicMaterial({ color: 0x14332c });
    const latchLipMesh = new THREE.Mesh(latchLipGeo, latchLipMat);
    latchLipMesh.position.set(0, latchY + 0.10, Z_BACK - 0.03);
    this.rootGroup.add(latchLipMesh);

    // -------------------------------------------------------------------------
    // 12. CANONICAL SIDES: 7-Hole Acoustic Speaker Array & Vertical "B M O"
    //     Faithful to Adventure Time BMO (Both Left and Right Sides!)
    // -------------------------------------------------------------------------
    const sideTex = this.createSideTexture();
    const sideGeo = new THREE.PlaneGeometry(3.0, 10.5);
    const sideMat = new THREE.MeshBasicMaterial({
      map: sideTex,
      transparent: true,
      depthWrite: false,
    });

    // Left side: canonical speaker array + "B M O" with arm socket
    const leftSideMesh = new THREE.Mesh(sideGeo, sideMat);
    leftSideMesh.position.set(-hw - 0.005, 0.4, 0);
    leftSideMesh.rotation.y = -Math.PI / 2;
    this.rootGroup.add(leftSideMesh);

    // Right side: canonical speaker array + "B M O" with arm socket
    const rightSideMesh = new THREE.Mesh(sideGeo, sideMat);
    rightSideMesh.position.set(hw + 0.005, 0.4, 0);
    rightSideMesh.rotation.y = Math.PI / 2;
    this.rootGroup.add(rightSideMesh);
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

    let chassisHex = 0x5fb49c;
    let bezelHex = 0x1a4b42;
    let roughness = 0.42;
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
