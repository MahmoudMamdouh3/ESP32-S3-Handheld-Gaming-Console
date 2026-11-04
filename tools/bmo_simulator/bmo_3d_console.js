/**
 * bmo_3d_console.js
 * 
 * Ultra-Premium 3D Physical Hardware Model & Interactive WebGL Simulator for BMO Handheld Console.
 * Built with Three.js (r128) adhering strictly to exact physical centimeter specifications:
 *  - Body: 8.8 W × 12.4 H × 3.2 D cm with 0.6 cm corner radius (1.5 cm bottom-right)
 *  - Display window: 5.0 × 3.8 cm (active area 4.88 × 3.66 cm landscape 4:3 320×240 ST7789)
 *  - Screen Bezel: 6.6 × 5.2 cm with "BMO COLOR", "USB-C POWER", glowing LED, and "BMO GAMEBOY"
 *  - D-Pad: Single seamless 2.8 × 2.8 cm beveled cross with circular thumb well & tactile pivot tilt
 *  - ABXY Diamond: Square to 2.54 mm perfboard grid with molded sockets & high-gloss domed candy buttons
 *  - Select / Start: 1.4 × 0.5 cm angled rubber pills with molded well recesses
 *  - L / R Shoulders: 2.2 × 0.6 cm curved top-edge bumpers with tactile spring depression
 *  - Cutouts: USB-C port, MicroSD slot, Power switch, and rear 4×AA battery compartment door
 *  - Studio Lighting: Balanced three-point key/fill/rim with PCF soft shadows and sRGB color management
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

    // 3. Renderer with full sRGB color space & ACES tone mapping
    this.renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      powerPreference: "high-performance",
    });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    
    // Enable sRGB encoding so hex colors match CSS exactly without bleaching
    if ("outputEncoding" in this.renderer) {
      this.renderer.outputEncoding = THREE.sRGBEncoding;
    }
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
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

    // 5. Studio Lighting Setup
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
    // Soft natural ambient fill
    const ambient = new THREE.AmbientLight(0xffffff, 0.42);
    this.scene.add(ambient);

    // Key Light: Warm studio key from front-top-right
    const keyLight = new THREE.DirectionalLight(0xfffaf0, 0.88);
    keyLight.position.set(8, 14, 16);
    keyLight.castShadow = true;
    keyLight.shadow.mapSize.width = 2048;
    keyLight.shadow.mapSize.height = 2048;
    keyLight.shadow.camera.near = 0.5;
    keyLight.shadow.camera.far = 40;
    keyLight.shadow.camera.left = -9;
    keyLight.shadow.camera.right = 9;
    keyLight.shadow.camera.top = 9;
    keyLight.shadow.camera.bottom = -9;
    keyLight.shadow.bias = -0.0004;
    keyLight.shadow.radius = 2.5;
    this.scene.add(keyLight);

    // Fill Light: Soft cool fill from front-left
    const fillLight = new THREE.DirectionalLight(0xd8ecff, 0.38);
    fillLight.position.set(-12, -4, 12);
    this.scene.add(fillLight);

    // Rim Light: Backlight edge highlighting the top shoulders & silhouette
    const rimLight = new THREE.DirectionalLight(0xffffff, 0.5);
    rimLight.position.set(0, 16, -10);
    this.scene.add(rimLight);

    // Bounce Light: Subtle warm ground bounce
    const bounceLight = new THREE.DirectionalLight(0x5fb49c, 0.22);
    bounceLight.position.set(0, -12, 4);
    this.scene.add(bounceLight);
  }

  lightenColor(hex, percent) {
    const num = parseInt(hex.replace("#", ""), 16);
    const amt = Math.round(2.55 * percent);
    const R = Math.min(255, (num >> 16) + amt);
    const G = Math.min(255, ((num >> 8) & 0x00ff) + amt);
    const B = Math.min(255, (num & 0x0000ff) + amt);
    return `#${(0x1000000 + (R << 16) + (G << 8) + B).toString(16).slice(1)}`;
  }

  darkenColor(hex, percent) {
    const num = parseInt(hex.replace("#", ""), 16);
    const amt = Math.round(2.55 * percent);
    const R = Math.max(0, (num >> 16) - amt);
    const G = Math.max(0, ((num >> 8) & 0x00ff) - amt);
    const B = Math.max(0, (num & 0x0000ff) - amt);
    return `#${(0x1000000 + (R << 16) + (G << 8) + B).toString(16).slice(1)}`;
  }

  createButtonTexture(label, bgColorStr, textColorStr) {
    const canvas = document.createElement("canvas");
    canvas.width = 256;
    canvas.height = 256;
    const ctx = canvas.getContext("2d");

    // Clear
    ctx.clearRect(0, 0, 256, 256);

    // 1. Glossy dome radial gradient
    const grad = ctx.createRadialGradient(96, 88, 10, 128, 128, 122);
    grad.addColorStop(0, this.lightenColor(bgColorStr, 32));
    grad.addColorStop(0.55, bgColorStr);
    grad.addColorStop(1, this.darkenColor(bgColorStr, 28));

    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(128, 128, 120, 0, Math.PI * 2);
    ctx.fill();

    // 2. Outer chamfer bevel ring
    ctx.lineWidth = 5;
    ctx.strokeStyle = "rgba(0, 0, 0, 0.35)";
    ctx.stroke();

    // 3. Top crescent specular highlight (physical candy button sheen)
    const hlGrad = ctx.createLinearGradient(128, 14, 128, 88);
    hlGrad.addColorStop(0, "rgba(255, 255, 255, 0.55)");
    hlGrad.addColorStop(1, "rgba(255, 255, 255, 0.0)");
    ctx.fillStyle = hlGrad;
    ctx.beginPath();
    ctx.ellipse(128, 58, 76, 36, 0, 0, Math.PI * 2);
    ctx.fill();

    // 4. Centered bold letter label
    ctx.fillStyle = textColorStr;
    ctx.font = "900 82px 'Press Start 2P', monospace, sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.shadowColor = "rgba(0, 0, 0, 0.3)";
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

    // 1. Deep forest teal gradient housing
    const bgGrad = ctx.createLinearGradient(0, 0, 0, 768);
    bgGrad.addColorStop(0, "#1a4b42");
    bgGrad.addColorStop(0.6, "#143c34");
    bgGrad.addColorStop(1, "#0f2620");
    ctx.fillStyle = bgGrad;
    ctx.beginPath();
    ctx.roundRect(0, 0, 1024, 768, 44);
    ctx.fill();

    // Outer subtle bevel border
    ctx.lineWidth = 4;
    ctx.strokeStyle = "rgba(255, 255, 255, 0.15)";
    ctx.stroke();

    // 2. Top Status Bar: "BMO COLOR" and "USB-C POWER"
    ctx.fillStyle = "#cef5e4";
    ctx.font = "bold 24px 'Press Start 2P', monospace";
    ctx.textAlign = "left";
    ctx.textBaseline = "middle";
    ctx.fillText("BMO COLOR", 68, 52);

    ctx.fillStyle = "#84d0bc";
    ctx.font = "bold 20px 'Press Start 2P', monospace";
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

    // 3. Inner Display Frame Recess Border
    // Window area from y=88 to y=676, x=72 to x=952
    ctx.lineWidth = 6;
    ctx.strokeStyle = "#081512";
    ctx.strokeRect(72, 88, 880, 588);

    // 4. Bottom Footer Bar: "BMO GAMEBOY"
    const footerY = 722;
    ctx.font = "bold 30px 'Press Start 2P', monospace";
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

    // Directional arrows
    ctx.fillStyle = "rgba(0, 0, 0, 0.28)";
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

    // Up, Down, Left, Right arrows
    drawTriangle(256, 75, 18, 0);
    drawTriangle(256, 437, 18, Math.PI);
    drawTriangle(75, 256, 18, -Math.PI / 2);
    drawTriangle(437, 256, 18, Math.PI / 2);

    // Center circular concave shadow ring
    const radGrad = ctx.createRadialGradient(256, 256, 20, 256, 256, 68);
    radGrad.addColorStop(0, "rgba(0, 0, 0, 0.35)");
    radGrad.addColorStop(0.7, "rgba(0, 0, 0, 0.1)");
    radGrad.addColorStop(1, "rgba(0, 0, 0, 0)");
    ctx.fillStyle = radGrad;
    ctx.beginPath();
    ctx.arc(256, 256, 68, 0, Math.PI * 2);
    ctx.fill();

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  buildConsoleModel() {
    this.rootGroup = new THREE.Group();
    this.scene.add(this.rootGroup);

    // Authentic BMO Palette Colors
    const bmoTealHex = 0x5fb49c;
    const deepTealHex = 0x1a4b42;
    const dpadYellowHex = 0xffe033;
    const socketColorHex = 0x438f7a;

    // Physical PBR Materials
    this.materials.chassis = new THREE.MeshPhysicalMaterial({
      color: bmoTealHex,
      roughness: 0.28,
      metalness: 0.02,
      clearcoat: 0.16,
      clearcoatRoughness: 0.14,
    });

    this.materials.bezel = new THREE.MeshPhysicalMaterial({
      color: deepTealHex,
      roughness: 0.3,
      metalness: 0.04,
      clearcoat: 0.2,
    });

    this.materials.dpad = new THREE.MeshPhysicalMaterial({
      color: dpadYellowHex,
      roughness: 0.22,
      metalness: 0.02,
      clearcoat: 0.12,
      clearcoatRoughness: 0.1,
    });

    this.materials.socket = new THREE.MeshStandardMaterial({
      color: socketColorHex,
      roughness: 0.65,
      metalness: 0.05,
    });

    this.materials.btnPill = new THREE.MeshStandardMaterial({
      color: deepTealHex,
      roughness: 0.55,
      metalness: 0.02,
    });

    this.materials.shoulder = new THREE.MeshPhysicalMaterial({
      color: deepTealHex,
      roughness: 0.26,
      metalness: 0.05,
      clearcoat: 0.2,
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
    // Bottom-left corner
    shape.moveTo(-hw + r, -hh);
    // Bottom edge towards asymmetric corner
    shape.lineTo(hw - r_br, -hh);
    // Asymmetric bottom-right curve
    shape.quadraticCurveTo(hw, -hh, hw, -hh + r_br);
    // Right edge
    shape.lineTo(hw, hh - r);
    // Top-right curve
    shape.quadraticCurveTo(hw, hh, hw - r, hh);
    // Top edge
    shape.lineTo(-hw + r, hh);
    // Top-left curve
    shape.quadraticCurveTo(-hw, hh, -hw, hh - r);
    // Left edge
    shape.lineTo(-hw, -hh + r);
    // Bottom-left curve
    shape.quadraticCurveTo(-hw, -hh, -hw + r, -hh);

    const extrudeSettings = {
      depth: D - 0.36,
      bevelEnabled: true,
      bevelSegments: 7,
      steps: 1,
      bevelSize: 0.18,
      bevelThickness: 0.18,
    };

    const chassisGeo = new THREE.ExtrudeGeometry(shape, extrudeSettings);
    chassisGeo.center();
    this.chassisMesh = new THREE.Mesh(chassisGeo, this.materials.chassis);
    this.chassisMesh.castShadow = true;
    this.chassisMesh.receiveShadow = true;
    this.rootGroup.add(this.chassisMesh);

    const Z_FRONT = D / 2; // +1.6 cm

    // Soft Contact Shadow Plane beneath the console
    const shadowGeo = new THREE.PlaneGeometry(14, 18);
    const shadowCanvas = document.createElement("canvas");
    shadowCanvas.width = 256;
    shadowCanvas.height = 256;
    const sCtx = shadowCanvas.getContext("2d");
    const sGrad = sCtx.createRadialGradient(128, 128, 40, 128, 128, 120);
    sGrad.addColorStop(0, "rgba(0, 0, 0, 0.45)");
    sGrad.addColorStop(0.6, "rgba(0, 0, 0, 0.15)");
    sGrad.addColorStop(1, "rgba(0, 0, 0, 0)");
    sCtx.fillStyle = sGrad;
    sCtx.fillRect(0, 0, 256, 256);
    const shadowTex = new THREE.CanvasTexture(shadowCanvas);
    const shadowMat = new THREE.MeshBasicMaterial({ map: shadowTex, transparent: true, opacity: 0.65 });
    const shadowMesh = new THREE.Mesh(shadowGeo, shadowMat);
    shadowMesh.position.set(0, 0, -D / 2 - 0.05);
    this.rootGroup.add(shadowMesh);

    // -------------------------------------------------------------------------
    // 2. SCREEN BEZEL HOUSING & ACTIVE ST7789 DISPLAY
    //  - Housing: 6.6 × 5.2 cm centered at (4.4, 3.3) => X 0.0, Y +2.9 cm
    //  - Active ST7789 display window: 5.0 × 3.8 cm
    // -------------------------------------------------------------------------
    const BEZEL_W = 6.6;
    const BEZEL_H = 5.2;
    const BEZEL_Y = hh - 3.3; // +2.9 cm

    // Bezel housing plate with high-res branding texture
    const bezelTex = this.createBezelTexture();
    const bezelGeo = new THREE.PlaneGeometry(BEZEL_W, BEZEL_H);
    const bezelMat = new THREE.MeshStandardMaterial({
      map: bezelTex,
      roughness: 0.28,
      metalness: 0.05,
    });
    const bezelMesh = new THREE.Mesh(bezelGeo, bezelMat);
    bezelMesh.position.set(0, BEZEL_Y, Z_FRONT + 0.02);
    bezelMesh.receiveShadow = true;
    this.rootGroup.add(bezelMesh);

    // Active ST7789 LCD Panel (5.0 × 3.8 cm active area)
    const SCREEN_W = 5.0;
    const SCREEN_H = 3.8;
    const screenGeo = new THREE.PlaneGeometry(SCREEN_W, SCREEN_H);

    if (this.tftCanvas) {
      this.screenTexture = new THREE.CanvasTexture(this.tftCanvas);
      this.screenTexture.minFilter = THREE.LinearFilter;
      this.screenTexture.magFilter = THREE.NearestFilter;
    }

    const screenMat = new THREE.MeshStandardMaterial({
      map: this.screenTexture,
      roughness: 0.15,
      metalness: 0.0,
      emissive: 0xffffff,
      emissiveMap: this.screenTexture,
      emissiveIntensity: 0.95, // Backlit IPS panel glow
    });

    this.screenMesh = new THREE.Mesh(screenGeo, screenMat);
    this.screenMesh.position.set(0, BEZEL_Y, Z_FRONT + 0.035);
    this.rootGroup.add(this.screenMesh);

    // Glossy Protective Glass Lens overlay with subtle reflection sheen
    const lensGeo = new THREE.PlaneGeometry(BEZEL_W, BEZEL_H);
    const lensMat = new THREE.MeshPhysicalMaterial({
      roughness: 0.04,
      metalness: 0.0,
      clearcoat: 0.95,
      clearcoatRoughness: 0.04,
      transmission: 0.12,
      transparent: true,
      opacity: 0.25,
      reflectivity: 0.5,
    });
    const lensMesh = new THREE.Mesh(lensGeo, lensMat);
    lensMesh.position.set(0, BEZEL_Y, Z_FRONT + 0.045);
    this.rootGroup.add(lensMesh);

    // -------------------------------------------------------------------------
    // 3. TOP FOREHEAD MOLDED VENT RIDGES
    // -------------------------------------------------------------------------
    for (let i = 0; i < 3; i++) {
      const ridgeGeo = new THREE.BoxGeometry(4.2, 0.08, 0.04);
      const ridgeMesh = new THREE.Mesh(ridgeGeo, this.materials.socket);
      ridgeMesh.position.set(0, hh - 0.5 - i * 0.16, Z_FRONT + 0.01);
      this.rootGroup.add(ridgeMesh);
    }

    // -------------------------------------------------------------------------
    // 4. D-PAD: Single Seamless Sculpted Cross with Tactile Pivot Tilt
    //  - Center at (2.2, 8.4) cm => X -2.2, Y -2.2 cm
    //  - Dimensions: 2.8 × 2.8 cm, arm width 0.95 cm
    // -------------------------------------------------------------------------
    const DPAD_X = -2.2;
    const DPAD_Y = -2.2;
    const ARM_W = 0.95;
    const L = 1.4;        // Half of 2.8
    const W_HALF = ARM_W / 2; // 0.475
    const CR = 0.08;      // Arm corner roundness

    // Recessed Cross Socket in Chassis
    const socketShape = new THREE.Shape();
    const SL = L + 0.12;
    const SW = W_HALF + 0.1;
    socketShape.moveTo(-SW, SL);
    socketShape.lineTo(SW, SL);
    socketShape.lineTo(SW, SW);
    socketShape.lineTo(SL, SW);
    socketShape.lineTo(SL, -SW);
    socketShape.lineTo(SW, -SW);
    socketShape.lineTo(SW, -SL);
    socketShape.lineTo(-SW, -SL);
    socketShape.lineTo(-SW, -SW);
    socketShape.lineTo(-SL, -SW);
    socketShape.lineTo(-SL, SW);
    socketShape.lineTo(-SW, SW);
    socketShape.closePath();

    const socketGeo = new THREE.ShapeGeometry(socketShape);
    const socketMesh = new THREE.Mesh(socketGeo, this.materials.socket);
    socketMesh.position.set(DPAD_X, DPAD_Y, Z_FRONT + 0.01);
    this.rootGroup.add(socketMesh);

    // D-Pad Cross Shape
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

    // Directional arrows & center concave dish overlay
    const dpadTex = this.createDpadTexture();
    const dpadTexGeo = new THREE.PlaneGeometry(2.8, 2.8);
    const dpadTexMat = new THREE.MeshStandardMaterial({
      map: dpadTex,
      transparent: true,
      roughness: 0.22,
      metalness: 0.02,
    });
    const dpadTexMesh = new THREE.Mesh(dpadTexGeo, dpadTexMat);
    dpadTexMesh.position.set(0, 0, 0.165);
    this.dpadPivotGroup.add(dpadTexMesh);

    // 4 Invisible Click Zones for raycasting input (opacity 0.0001 ensures raycast hits reliably)
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
    // 5. ACTION BUTTONS (X, Y, A, B) - Premium High-Gloss Injection Molded Domes
    //  - Square to 2.54 mm Perfboard Grid!
    //    * X (Top):   (6.6, 7.4) => X +2.2, Y -1.2 cm
    //    * Y (Left):  (5.6, 8.4) => X +1.2, Y -2.2 cm
    //    * A (Right): (7.6, 8.4) => X +3.2, Y -2.2 cm
    //    * B (Bottom):(6.6, 9.4) => X +2.2, Y -3.2 cm
    // -------------------------------------------------------------------------
    const CAP_RADIUS = 0.44;
    const CAP_HEIGHT = 0.26;

    const createActionButton = (name, key, label, x, y, bgColorStr, textColorStr) => {
      // 1. Molded Socket Well in Chassis
      const socketRingGeo = new THREE.RingGeometry(CAP_RADIUS, CAP_RADIUS + 0.09, 32);
      const socketRing = new THREE.Mesh(socketRingGeo, this.materials.socket);
      socketRing.position.set(x, y, Z_FRONT + 0.015);
      this.rootGroup.add(socketRing);

      // 2. Action Button Root Group
      const btnGroup = new THREE.Group();
      btnGroup.position.set(x, y, Z_FRONT + CAP_HEIGHT / 2);
      this.rootGroup.add(btnGroup);

      // Button Stem (Cylinder)
      const cylGeo = new THREE.CylinderGeometry(CAP_RADIUS, CAP_RADIUS * 1.04, CAP_HEIGHT, 32);
      cylGeo.rotateX(Math.PI / 2);
      const cylMat = new THREE.MeshPhysicalMaterial({
        color: new THREE.Color(bgColorStr),
        roughness: 0.16,
        metalness: 0.02,
        clearcoat: 0.4,
        clearcoatRoughness: 0.06,
      });
      const stemMesh = new THREE.Mesh(cylGeo, cylMat);
      stemMesh.castShadow = true;
      stemMesh.receiveShadow = true;
      btnGroup.add(stemMesh);

      // Domed Button Top with Specular Sheen & Centered Pixel Label
      const topTex = this.createButtonTexture(label, bgColorStr, textColorStr);
      const topGeo = new THREE.CircleGeometry(CAP_RADIUS, 32);
      const topMat = new THREE.MeshStandardMaterial({
        map: topTex,
        roughness: 0.14,
        metalness: 0.02,
      });
      const capMesh = new THREE.Mesh(topGeo, topMat);
      capMesh.position.set(0, 0, CAP_HEIGHT / 2 + 0.005);
      btnGroup.add(capMesh);

      // Interactive Click Target
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

    createActionButton("X Button", "X", "X", 2.2, -1.2, "#4caf50", "#0f2620");
    createActionButton("Y Button", "Y", "Y", 1.2, -2.2, "#f48fb1", "#0f2620");
    createActionButton("A Button", "A", "A", 3.2, -2.2, "#52a6ba", "#0f2620");
    createActionButton("B Button", "B", "B", 2.2, -3.2, "#e8175d", "#ffffff");

    // -------------------------------------------------------------------------
    // 6. SELECT / START PILL BUTTONS (1.4 × 0.5 cm Angled Rubber Pills)
    //  - Select: (3.4, 11.0) => X -1.0, Y -4.8 cm
    //  - Start:  (5.4, 11.0) => X +1.0, Y -4.8 cm
    // -------------------------------------------------------------------------
    const createPill = (name, key, label, x, y) => {
      // Recessed Pill Socket
      const pSocketGeo = new THREE.PlaneGeometry(1.55, 0.6);
      const pSocket = new THREE.Mesh(pSocketGeo, this.materials.socket);
      pSocket.rotation.z = -0.44; // ~-25 degrees
      pSocket.position.set(x, y, Z_FRONT + 0.015);
      this.rootGroup.add(pSocket);

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

      // Engraved Pill Label
      const lblCanvas = document.createElement("canvas");
      lblCanvas.width = 256;
      lblCanvas.height = 64;
      const lCtx = lblCanvas.getContext("2d");
      lCtx.fillStyle = "#1a4b42";
      lCtx.font = "bold 28px 'Press Start 2P', monospace";
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
    // 7. L / R SHOULDER TRIGGERS (Top Edge Curved Bumpers with Embossed Labels)
    //  - L: (1.7, 0.3) => X -2.7, Y +6.2 cm
    //  - R: (7.1, 0.3) => X +2.7, Y +6.2 cm
    // -------------------------------------------------------------------------
    const createShoulder = (name, key, label, x) => {
      const triggerGeo = new THREE.BoxGeometry(2.3, 0.58, 1.25);
      const mesh = new THREE.Mesh(triggerGeo, this.materials.shoulder);
      mesh.position.set(x, hh + 0.22, 0.5);
      mesh.castShadow = true;
      mesh.userData = { isButton: true, key: key, name: name, isShoulder: true };
      this.rootGroup.add(mesh);

      this.buttonMeshes[key] = mesh;
      this.interactiveObjects.push(mesh);
      this.buttonOriginalPositions.set(mesh, {
        x: mesh.position.x,
        y: mesh.position.y,
        z: mesh.position.z,
      });

      // Embossed "L" / "R" Label on front-facing bevel
      const sCanvas = document.createElement("canvas");
      sCanvas.width = 128;
      sCanvas.height = 64;
      const sCtx = sCanvas.getContext("2d");
      sCtx.fillStyle = "#cef5e4";
      sCtx.font = "bold 36px 'Press Start 2P', monospace";
      sCtx.textAlign = "center";
      sCtx.textBaseline = "middle";
      sCtx.fillText(label, 64, 32);

      const sTex = new THREE.CanvasTexture(sCanvas);
      const sGeo = new THREE.PlaneGeometry(1.2, 0.4);
      const sMat = new THREE.MeshBasicMaterial({ map: sTex, transparent: true });
      const sMesh = new THREE.Mesh(sGeo, sMat);
      sMesh.position.set(0, 0, 0.63);
      mesh.add(sMesh);
    };

    createShoulder("L Trigger", "L", "L", -2.7);
    createShoulder("R Trigger", "R", "R", 2.7);

    // -------------------------------------------------------------------------
    // 8. SPEAKER GRILLE (6 Recessed Diagonal Slits at Bottom-Right Corner)
    // -------------------------------------------------------------------------
    const speakerGroup = new THREE.Group();
    speakerGroup.position.set(2.6, -4.8, Z_FRONT + 0.015);
    speakerGroup.rotation.z = -0.52; // ~-30 degrees
    this.rootGroup.add(speakerGroup);

    for (let i = 0; i < 6; i++) {
      const slitGeo = new THREE.BoxGeometry(0.12, 0.9 + (i < 3 ? i * 0.15 : (5 - i) * 0.15), 0.04);
      const slit = new THREE.Mesh(slitGeo, this.materials.socket);
      slit.position.set(i * 0.22 - 0.55, 0, 0);
      speakerGroup.add(slit);
    }

    // -------------------------------------------------------------------------
    // 9. HARDWARE PORTS: Top USB-C, Left SD Slot, Right Power Switch
    // -------------------------------------------------------------------------
    // USB-C Cutout
    const usbcGeo = new THREE.BoxGeometry(1.0, 0.45, 0.6);
    const usbcHole = new THREE.Mesh(usbcGeo, this.materials.socket);
    usbcHole.position.set(0, hh, 0.4);
    this.rootGroup.add(usbcHole);

    const usbcTongueGeo = new THREE.BoxGeometry(0.7, 0.12, 0.35);
    const usbcTongue = new THREE.Mesh(usbcTongueGeo, this.materials.metal);
    usbcTongue.position.set(0, hh + 0.05, 0.4);
    this.rootGroup.add(usbcTongue);

    // MicroSD Slot (Left Edge, Y +2.2 cm)
    const sdHoleGeo = new THREE.BoxGeometry(0.5, 0.25, 1.35);
    const sdHole = new THREE.Mesh(sdHoleGeo, this.materials.socket);
    sdHole.position.set(-hw, 2.2, 0.4);
    this.rootGroup.add(sdHole);

    // Power Slide Switch (Right Edge, Y +3.2 cm)
    const pwrHoleGeo = new THREE.BoxGeometry(0.4, 1.1, 0.45);
    const pwrHole = new THREE.Mesh(pwrHoleGeo, this.materials.socket);
    pwrHole.position.set(hw, 3.2, 0.4);
    this.rootGroup.add(pwrHole);

    const pwrNubGeo = new THREE.BoxGeometry(0.25, 0.45, 0.35);
    const pwrNub = new THREE.Mesh(pwrNubGeo, this.materials.btnPill);
    pwrNub.position.set(hw + 0.08, 3.4, 0.4);
    this.rootGroup.add(pwrNub);

    // -------------------------------------------------------------------------
    // 10. REAR 4×AA BATTERY COMPARTMENT DOOR (3.4 × 6.2 cm at x 4.4, y 9.0)
    // -------------------------------------------------------------------------
    const doorGeo = new THREE.BoxGeometry(6.2, 3.4, 0.18);
    const doorMesh = new THREE.Mesh(doorGeo, this.materials.chassis);
    doorMesh.position.set(0, -2.8, -D / 2);
    this.rootGroup.add(doorMesh);

    // Rear Grip Slits
    for (let i = 0; i < 4; i++) {
      const slitGeo = new THREE.BoxGeometry(4.2, 0.1, 0.04);
      const slit = new THREE.Mesh(slitGeo, this.materials.socket);
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
          mesh.position.y = orig.y - 0.16; // Depress shoulder trigger downward
        } else {
          mesh.position.z = orig.z - 0.14; // Depress face button into socket
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
      // Check if another D-pad direction is still held
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
