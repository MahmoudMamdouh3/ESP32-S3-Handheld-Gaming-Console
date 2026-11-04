/**
 * bmo_3d_console.js
 * 
 * 3D Physical Hardware Model & Interactive WebGL Simulator for BMO Handheld Console.
 * Built with Three.js (r128) adhering strictly to exact centimeter physical specifications:
 *  - Body: 8.8 W × 12.4 H × 3.2 D cm with 0.6 cm corner radius (1.5 cm bottom-right)
 *  - Display window: 5.0 × 3.8 cm (active area 4.88 × 3.66 cm landscape 4:3 320×240 ST7789)
 *  - D-Pad: 2.8 × 2.8 cm centered at (2.2, 8.4) cm
 *  - ABXY Diamond: Square to 2.54 mm perfboard grid (centers: X 6.6,7.4; Y 5.6,8.4; A 7.6,8.4; B 6.6,9.4)
 *  - Select / Start: 1.4 × 0.5 cm pills at (3.4, 11.0) and (5.4, 11.0) cm
 *  - L / R Shoulders: 2.2 × 0.6 cm side-actuated bumpers on top edge at (1.7, 0.3) and (7.1, 0.3)
 *  - USB-C cutout at (4.4, 0.0), MicroSD slot on left edge, Power switch on right edge
 *  - 4×AA Battery door on rear (3.4 × 6.2 cm at x 4.4, y 9.0)
 *  - Live ST7789 screen canvas texture mapping with emissive backlight
 *  - Raycasting interactive 3D button depression + spring mechanics
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

    // Theme materials
    this.materials = {};
    this.activeTheme = "bmo-teal";

    // Raycasting state
    this.raycaster = new THREE.Raycaster();
    this.mouse = new THREE.Vector2();
    this.pointerDownPos = { x: 0, y: 0 };
    this.hoveredButton = null;
    this.activePointerButton = null;

    this.init();
  }

  init() {
    const width = this.container.clientWidth || 440;
    const height = this.container.clientHeight || 640;

    // 1. Scene
    this.scene = new THREE.Scene();

    // 2. Camera
    this.camera = new THREE.PerspectiveCamera(40, width / height, 0.1, 100);
    this.setCameraPreset("front");

    // 3. Renderer
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: "high-performance" });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.05;
    this.container.appendChild(this.renderer.domElement);

    // 4. Orbit Controls
    if (typeof THREE.OrbitControls !== "undefined") {
      this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.08;
      this.controls.minDistance = 8;
      this.controls.maxDistance = 35;
      this.controls.target.set(0, 0, 0);
    }

    // 5. Lighting
    this.setupLighting();

    // 6. Build 3D Console Model
    this.buildConsoleModel();

    // 7. Input Event Listeners
    this.setupRaycasting();

    // 8. Handle Window Resize
    window.addEventListener("resize", () => this.onResize());

    // 9. Start Render Loop
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  setupLighting() {
    // Ambient light
    const ambient = new THREE.AmbientLight(0xffffff, 0.7);
    this.scene.add(ambient);

    // Key light (soft front-top-right)
    const keyLight = new THREE.DirectionalLight(0xffffff, 0.85);
    keyLight.position.set(10, 15, 20);
    keyLight.castShadow = true;
    keyLight.shadow.mapSize.width = 1024;
    keyLight.shadow.mapSize.height = 1024;
    keyLight.shadow.bias = -0.001;
    this.scene.add(keyLight);

    // Fill light (cool blue-ish front-left)
    const fillLight = new THREE.DirectionalLight(0xb4d4ff, 0.4);
    fillLight.position.set(-15, -5, 12);
    this.scene.add(fillLight);

    // Rim light (warm rear-top-left for edge silhouette)
    const rimLight = new THREE.DirectionalLight(0xfff5d0, 0.5);
    rimLight.position.set(-10, 12, -15);
    this.scene.add(rimLight);

    // Soft bottom bounce
    const bounceLight = new THREE.DirectionalLight(0x5fb49c, 0.25);
    bounceLight.position.set(0, -15, -5);
    this.scene.add(bounceLight);
  }

  createButtonTexture(label, bgColorHex, textColorHex) {
    const canvas = document.createElement("canvas");
    canvas.width = 128;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");

    // Circular background
    ctx.fillStyle = bgColorHex;
    ctx.beginPath();
    ctx.arc(64, 64, 60, 0, Math.PI * 2);
    ctx.fill();

    // Outer subtle bevel ring
    ctx.lineWidth = 4;
    ctx.strokeStyle = "rgba(255, 255, 255, 0.4)";
    ctx.stroke();

    // Text label
    ctx.fillStyle = textColorHex;
    ctx.font = "900 48px 'Press Start 2P', monospace, sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(label, 64, 68);

    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = THREE.LinearFilter;
    return texture;
  }

  buildConsoleModel() {
    this.rootGroup = new THREE.Group();
    this.scene.add(this.rootGroup);

    // Colors
    const bmoTeal = 0x5fb49c;
    const deepTeal = 0x1a4b42;
    const dpadYellow = 0xffe033;
    const btnAColor = 0x52a6ba;
    const btnBColor = 0xe8175d;
    const btnXColor = 0x4caf50;
    const btnYColor = 0xf48fb1;
    const darkForest = 0x0f2620;

    // Shared base materials
    this.materials.chassis = new THREE.MeshStandardMaterial({
      color: bmoTeal,
      roughness: 0.35,
      metalness: 0.05,
    });

    this.materials.bezel = new THREE.MeshStandardMaterial({
      color: deepTeal,
      roughness: 0.45,
      metalness: 0.1,
    });

    this.materials.dpad = new THREE.MeshStandardMaterial({
      color: dpadYellow,
      roughness: 0.3,
      metalness: 0.05,
    });

    this.materials.btnPill = new THREE.MeshStandardMaterial({
      color: deepTeal,
      roughness: 0.5,
      metalness: 0.1,
    });

    this.materials.shoulder = new THREE.MeshStandardMaterial({
      color: deepTeal,
      roughness: 0.35,
      metalness: 0.15,
    });

    this.materials.metal = new THREE.MeshStandardMaterial({
      color: 0x95a5a6,
      roughness: 0.25,
      metalness: 0.85,
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
    const r_br = 1.5; // asymmetrical bottom-right corner radius (classic Game Boy)

    const shape = new THREE.Shape();
    // Bottom-left corner
    shape.moveTo(-hw + r, -hh);
    // Bottom edge towards bottom-right
    shape.lineTo(hw - r_br, -hh);
    // Asymmetric bottom-right curve (radius 1.5)
    shape.quadraticCurveTo(hw, -hh, hw, -hh + r_br);
    // Right edge
    shape.lineTo(hw, hh - r);
    // Top-right curve (radius 0.6)
    shape.quadraticCurveTo(hw, hh, hw - r, hh);
    // Top edge
    shape.lineTo(-hw + r, hh);
    // Top-left curve (radius 0.6)
    shape.quadraticCurveTo(-hw, hh, -hw, hh - r);
    // Left edge
    shape.lineTo(-hw, -hh + r);
    // Bottom-left curve (radius 0.6)
    shape.quadraticCurveTo(-hw, -hh, -hw + r, -hh);

    const extrudeSettings = {
      depth: D - 0.3,
      bevelEnabled: true,
      bevelSegments: 5,
      steps: 1,
      bevelSize: 0.15,
      bevelThickness: 0.15,
    };

    const chassisGeo = new THREE.ExtrudeGeometry(shape, extrudeSettings);
    chassisGeo.center();
    this.chassisMesh = new THREE.Mesh(chassisGeo, this.materials.chassis);
    this.chassisMesh.castShadow = true;
    this.chassisMesh.receiveShadow = true;
    this.rootGroup.add(this.chassisMesh);

    // Front Face Plate (Z = +1.6)
    const Z_FRONT = D / 2; // 1.6 cm

    // Top ridges (air vents)
    for (let i = 0; i < 3; i++) {
      const ridgeGeo = new THREE.BoxGeometry(5.0, 0.08, 0.06);
      const ridgeMat = new THREE.MeshStandardMaterial({ color: 0x438f7a, roughness: 0.5 });
      const ridge = new THREE.Mesh(ridgeGeo, ridgeMat);
      ridge.position.set(0, 5.75 - i * 0.16, Z_FRONT + 0.02);
      this.rootGroup.add(ridge);
    }

    // -------------------------------------------------------------------------
    // 2. DISPLAY BEZEL & ST7789 SCREEN (Window 5.0 × 3.8 cm at center 4.4, 3.3)
    // In 3D: X = 0, Y = 6.2 - 3.3 = +2.9 cm
    // -------------------------------------------------------------------------
    const BEZEL_W = 5.6;
    const BEZEL_H = 4.4;
    const SCREEN_Y = 2.9;

    // Recessed Bezel Housing
    const bezelGeo = new THREE.BoxGeometry(BEZEL_W, BEZEL_H, 0.12);
    const bezelMesh = new THREE.Mesh(bezelGeo, this.materials.bezel);
    bezelMesh.position.set(0, SCREEN_Y, Z_FRONT + 0.04);
    bezelMesh.receiveShadow = true;
    this.rootGroup.add(bezelMesh);

    // Active ST7789 Display Surface (4.88 cm × 3.66 cm landscape 4:3)
    if (this.tftCanvas) {
      this.screenTexture = new THREE.CanvasTexture(this.tftCanvas);
      this.screenTexture.minFilter = THREE.LinearFilter;
      this.screenTexture.magFilter = THREE.NearestFilter; // pixelated retro look
    }

    const screenGeo = new THREE.PlaneGeometry(4.88, 3.66);
    this.screenMaterial = new THREE.MeshStandardMaterial({
      map: this.screenTexture,
      emissive: 0xffffff,
      emissiveMap: this.screenTexture,
      emissiveIntensity: 0.95,
      roughness: 0.15,
      metalness: 0.0,
    });

    this.screenMesh = new THREE.Mesh(screenGeo, this.screenMaterial);
    this.screenMesh.position.set(0, SCREEN_Y, Z_FRONT + 0.11);
    this.rootGroup.add(this.screenMesh);

    // Specular Glass Lens Cover
    const glassGeo = new THREE.PlaneGeometry(4.94, 3.72);
    const glassMat = new THREE.MeshPhysicalMaterial({
      color: 0xffffff,
      transparent: true,
      opacity: 0.16,
      roughness: 0.05,
      transmission: 0.9,
      reflectivity: 0.8,
    });
    const glassMesh = new THREE.Mesh(glassGeo, glassMat);
    glassMesh.position.set(0, SCREEN_Y, Z_FRONT + 0.12);
    this.rootGroup.add(glassMesh);

    // Power LED (Green glow)
    const ledGeo = new THREE.CircleGeometry(0.08, 16);
    const ledMat = new THREE.MeshBasicMaterial({ color: 0x8cd7c2 });
    const ledMesh = new THREE.Mesh(ledGeo, ledMat);
    ledMesh.position.set(2.4, SCREEN_Y + 1.85, Z_FRONT + 0.11);
    this.rootGroup.add(ledMesh);

    // -------------------------------------------------------------------------
    // 3. LOGO STRIP (5.0 × 0.8 cm at y 5.2 - 6.0 => Y = +0.6 cm)
    // -------------------------------------------------------------------------
    const logoCanvas = document.createElement("canvas");
    logoCanvas.width = 256;
    logoCanvas.height = 64;
    const lCtx = logoCanvas.getContext("2d");
    lCtx.fillStyle = "#5fb49c";
    lCtx.fillRect(0, 0, 256, 64);
    lCtx.fillStyle = "#1a4b42";
    lCtx.font = "900 28px 'Press Start 2P', monospace";
    lCtx.textAlign = "center";
    lCtx.textBaseline = "middle";
    lCtx.fillText("BMO", 128, 34);

    const logoTex = new THREE.CanvasTexture(logoCanvas);
    const logoGeo = new THREE.PlaneGeometry(2.4, 0.6);
    const logoMat = new THREE.MeshStandardMaterial({ map: logoTex, roughness: 0.4 });
    const logoMesh = new THREE.Mesh(logoGeo, logoMat);
    logoMesh.position.set(0, 0.55, Z_FRONT + 0.02);
    this.rootGroup.add(logoMesh);

    // -------------------------------------------------------------------------
    // 4. D-PAD (2.8 × 2.8 cm, arm width 0.95 cm, center: x 2.2, y 8.4 => X -2.2, Y -2.2)
    // -------------------------------------------------------------------------
    const DPAD_X = -2.2;
    const DPAD_Y = -2.2;
    const ARM_W = 0.95;
    const DPAD_LEN = 2.8;

    this.dpadGroup = new THREE.Group();
    this.dpadGroup.position.set(DPAD_X, DPAD_Y, Z_FRONT);
    this.rootGroup.add(this.dpadGroup);

    // Individual clickable arms for 3D tactile feedback
    const createDpadArm = (name, key, w, h, x, y) => {
      const geo = new THREE.BoxGeometry(w, h, 0.28);
      const mesh = new THREE.Mesh(geo, this.materials.dpad);
      mesh.position.set(x, y, 0.14);
      mesh.castShadow = true;
      mesh.receiveShadow = true;
      mesh.userData = { isButton: true, key: key, name: name };
      this.dpadGroup.add(mesh);
      this.registerButton(key, mesh);
      return mesh;
    };

    createDpadArm("D-Pad Up", "UP", ARM_W, 0.95, 0, 0.9);
    createDpadArm("D-Pad Down", "DOWN", ARM_W, 0.95, 0, -0.9);
    createDpadArm("D-Pad Left", "LEFT", 0.95, ARM_W, -0.9, 0);
    createDpadArm("D-Pad Right", "RIGHT", 0.95, ARM_W, 0.9, 0);

    // Center hub with circular concave thumb dish
    const centerGeo = new THREE.CylinderGeometry(0.4, 0.45, 0.28, 24);
    centerGeo.rotateX(Math.PI / 2);
    const centerMesh = new THREE.Mesh(centerGeo, this.materials.dpad);
    centerMesh.position.set(0, 0, 0.14);
    this.dpadGroup.add(centerMesh);

    // -------------------------------------------------------------------------
    // 5. ABXY FACE BUTTONS (Square to 2.54 mm perfboard grid!)
    //  - X: cap Ø0.9 cm at (6.6, 7.4) => X +2.2, Y -1.2 (Top)
    //  - Y: cap Ø0.9 cm at (5.6, 8.4) => X +1.2, Y -2.2 (Left)
    //  - A: cap Ø0.9 cm at (7.6, 8.4) => X +3.2, Y -2.2 (Right)
    //  - B: cap Ø0.9 cm at (6.6, 9.4) => X +2.2, Y -3.2 (Bottom)
    // -------------------------------------------------------------------------
    const CAP_RADIUS = 0.45;
    const CAP_HEIGHT = 0.28;

    const createFaceButton = (name, key, label, x, y, bgColorHex, textColorHex) => {
      const btnGroup = new THREE.Group();
      btnGroup.position.set(x, y, Z_FRONT);

      // Button cylinder base
      const cylGeo = new THREE.CylinderGeometry(CAP_RADIUS, CAP_RADIUS * 1.05, CAP_HEIGHT, 32);
      cylGeo.rotateX(Math.PI / 2);

      const tex = this.createButtonTexture(label, bgColorHex, textColorHex);
      const capMat = new THREE.MeshStandardMaterial({
        color: bgColorHex,
        roughness: 0.3,
        metalness: 0.05,
      });

      const topMat = new THREE.MeshStandardMaterial({
        map: tex,
        roughness: 0.3,
        metalness: 0.05,
      });

      // Multi-material for cylinder sides vs top face
      const mesh = new THREE.Mesh(cylGeo, [capMat, topMat, capMat]);
      mesh.position.set(0, 0, CAP_HEIGHT / 2);
      mesh.castShadow = true;
      mesh.receiveShadow = true;
      mesh.userData = { isButton: true, key: key, name: name };

      btnGroup.add(mesh);
      this.rootGroup.add(btnGroup);
      this.registerButton(key, mesh);
      return mesh;
    };

    createFaceButton("X Button", "X", "X", 2.2, -1.2, btnXColor, "#0f2620");
    createFaceButton("Y Button", "Y", "Y", 1.2, -2.2, btnYColor, "#0f2620");
    createFaceButton("A Button", "A", "A", 3.2, -2.2, btnAColor, "#0f2620");
    createFaceButton("B Button", "B", "B", 2.2, -3.2, btnBColor, "#ffffff");

    // -------------------------------------------------------------------------
    // 6. SELECT / START PILL BUTTONS (1.4 × 0.5 cm pills)
    //  - Select: (3.4, 11.0) => X -1.0, Y -4.8 cm
    //  - Start:  (5.4, 11.0) => X +1.0, Y -4.8 cm
    // -------------------------------------------------------------------------
    const createPillButton = (name, key, label, x, y) => {
      const pillGeo = new THREE.BoxGeometry(1.4, 0.48, 0.22);
      const mesh = new THREE.Mesh(pillGeo, this.materials.btnPill);
      mesh.rotation.z = -0.42; // ~-24 degrees
      mesh.position.set(x, y, Z_FRONT + 0.11);
      mesh.castShadow = true;
      mesh.userData = { isButton: true, key: key, name: name };

      this.rootGroup.add(mesh);
      this.registerButton(key, mesh);

      // Engraved text label below pill
      const lblCanvas = document.createElement("canvas");
      lblCanvas.width = 128;
      lblCanvas.height = 32;
      const c = lblCanvas.getContext("2d");
      c.fillStyle = "#438f7a";
      c.font = "900 16px 'Press Start 2P', monospace";
      c.textAlign = "center";
      c.textBaseline = "middle";
      c.fillText(label, 64, 16);

      const lTex = new THREE.CanvasTexture(lblCanvas);
      const lGeo = new THREE.PlaneGeometry(1.2, 0.3);
      const lMat = new THREE.MeshBasicMaterial({ map: lTex, transparent: true });
      const lMesh = new THREE.Mesh(lGeo, lMat);
      lMesh.position.set(x, y - 0.45, Z_FRONT + 0.02);
      this.rootGroup.add(lMesh);

      return mesh;
    };

    createPillButton("Select", "SELECT", "SELECT", -1.0, -4.8);
    createPillButton("Start", "START", "START", 1.0, -4.8);

    // -------------------------------------------------------------------------
    // 7. L / R SHOULDER TRIGGERS (Top Edge, Side-actuated switches)
    //  - L: 2.2 × 0.6 cm at (1.7, 0.3) => X -2.7, Y +6.2 cm
    //  - R: 2.2 × 0.6 cm at (7.1, 0.3) => X +2.7, Y +6.2 cm
    // -------------------------------------------------------------------------
    const createShoulderTrigger = (name, key, label, x) => {
      const triggerGeo = new THREE.BoxGeometry(2.2, 0.55, 1.2);
      const mesh = new THREE.Mesh(triggerGeo, this.materials.shoulder);
      mesh.position.set(x, hh + 0.22, 0.5);
      mesh.castShadow = true;
      mesh.userData = { isButton: true, key: key, name: name, isShoulder: true };

      this.rootGroup.add(mesh);
      this.registerButton(key, mesh);
      return mesh;
    };

    createShoulderTrigger("L Trigger", "L", "L", -2.7);
    createShoulderTrigger("R Trigger", "R", "R", 2.7);

    // -------------------------------------------------------------------------
    // 8. TOP EDGE USB-C CUTOUT (1.0 × 0.45 cm at x 4.4, y 0.0 => X 0.0, Y +6.2)
    // -------------------------------------------------------------------------
    const usbcHoleGeo = new THREE.BoxGeometry(1.0, 0.45, 0.6);
    const usbcHoleMat = new THREE.MeshStandardMaterial({ color: 0x0f2620, roughness: 0.8 });
    const usbcHole = new THREE.Mesh(usbcHoleGeo, usbcHoleMat);
    usbcHole.position.set(0, hh, 0.4);
    this.rootGroup.add(usbcHole);

    // USB-C metal tongue
    const usbcTongueGeo = new THREE.BoxGeometry(0.7, 0.12, 0.35);
    const usbcTongue = new THREE.Mesh(usbcTongueGeo, this.materials.metal);
    usbcTongue.position.set(0, hh + 0.05, 0.4);
    this.rootGroup.add(usbcTongue);

    // -------------------------------------------------------------------------
    // 9. LEFT EDGE MICROSD SLOT (1.3 × 0.25 cm at y 4.0 => X -4.4, Y +2.2)
    // -------------------------------------------------------------------------
    const sdSlotGeo = new THREE.BoxGeometry(0.3, 1.3, 0.25);
    const sdSlotMat = new THREE.MeshStandardMaterial({ color: 0x0f2620, roughness: 0.9 });
    const sdSlot = new THREE.Mesh(sdSlotGeo, sdSlotMat);
    sdSlot.position.set(-hw, 2.2, 0);
    this.rootGroup.add(sdSlot);

    // SD Card edge visible inside
    const sdCardGeo = new THREE.BoxGeometry(0.08, 1.1, 0.18);
    const sdCardMat = new THREE.MeshStandardMaterial({ color: 0x34495e, roughness: 0.4 });
    const sdCard = new THREE.Mesh(sdCardGeo, sdCardMat);
    sdCard.position.set(-hw + 0.04, 2.2, 0);
    this.rootGroup.add(sdCard);

    // -------------------------------------------------------------------------
    // 10. RIGHT EDGE POWER SLIDE SWITCH (1.0 × 0.4 cm at y 3.0 => X +4.4, Y +3.2)
    // -------------------------------------------------------------------------
    const pwrBaseGeo = new THREE.BoxGeometry(0.25, 1.0, 0.45);
    const pwrBaseMat = new THREE.MeshStandardMaterial({ color: 0x1a4b42, roughness: 0.7 });
    const pwrBase = new THREE.Mesh(pwrBaseGeo, pwrBaseMat);
    pwrBase.position.set(hw, 3.2, 0);
    this.rootGroup.add(pwrBase);

    // Slide switch knob
    const pwrKnobGeo = new THREE.BoxGeometry(0.2, 0.4, 0.35);
    const pwrKnobMat = new THREE.MeshStandardMaterial({ color: 0xe74c3c, roughness: 0.3 }); // Red power slider
    const pwrKnob = new THREE.Mesh(pwrKnobGeo, pwrKnobMat);
    pwrKnob.position.set(hw + 0.08, 3.4, 0);
    this.rootGroup.add(pwrKnob);

    // -------------------------------------------------------------------------
    // 11. REAR BATTERY DOOR (3.4 × 6.2 cm at x 4.4, y 9.0 => X 0.0, Y -2.8, Z -1.6)
    // -------------------------------------------------------------------------
    const Z_BACK = -D / 2; // -1.6 cm

    const battDoorGeo = new THREE.BoxGeometry(3.4, 6.2, 0.15);
    const battDoorMat = new THREE.MeshStandardMaterial({
      color: 0x56a68f,
      roughness: 0.45,
      metalness: 0.05,
    });
    const battDoor = new THREE.Mesh(battDoorGeo, battDoorMat);
    battDoor.position.set(0, -2.8, Z_BACK - 0.06);
    this.rootGroup.add(battDoor);

    // Battery door finger latch
    const latchGeo = new THREE.BoxGeometry(1.2, 0.35, 0.08);
    const latchMat = new THREE.MeshStandardMaterial({ color: 0x3d7b69, roughness: 0.6 });
    const latch = new THREE.Mesh(latchGeo, latchMat);
    latch.position.set(0, 0.05, Z_BACK - 0.12);
    this.rootGroup.add(latch);

    // Battery grip ribs
    for (let r = 0; r < 4; r++) {
      const ribGeo = new THREE.BoxGeometry(2.4, 0.1, 0.06);
      const rib = new THREE.Mesh(ribGeo, latchMat);
      rib.position.set(0, -1.8 - r * 0.35, Z_BACK - 0.12);
      this.rootGroup.add(rib);
    }

    // -------------------------------------------------------------------------
    // 12. FRONT SPEAKER GRILLE SLITS (Bottom right)
    // -------------------------------------------------------------------------
    for (let s = 0; s < 5; s++) {
      const slitGeo = new THREE.BoxGeometry(0.8, 0.08, 0.05);
      const slitMat = new THREE.MeshStandardMaterial({ color: 0x438f7a });
      const slit = new THREE.Mesh(slitGeo, slitMat);
      slit.rotation.z = -0.55;
      slit.position.set(2.8 + s * 0.18, -4.7 - s * 0.22, Z_FRONT + 0.02);
      this.rootGroup.add(slit);
    }
  }

  registerButton(key, mesh) {
    this.buttonMeshes[key] = mesh;
    this.interactiveObjects.push(mesh);
    this.buttonOriginalPositions.set(mesh, {
      x: mesh.position.x,
      y: mesh.position.y,
      z: mesh.position.z,
    });
  }

  setupRaycasting() {
    const dom = this.renderer.domElement;

    dom.addEventListener("pointerdown", (e) => {
      this.pointerDownPos = { x: e.clientX, y: e.clientY };
      const btn = this.getIntersectedButton(e);
      if (btn) {
        this.activePointerButton = btn.userData.key;
        this.pressButton(this.activePointerButton, true);
      }
    });

    window.addEventListener("pointerup", (e) => {
      if (this.activePointerButton) {
        this.releaseButton(this.activePointerButton, true);
        this.activePointerButton = null;
      }
    });

    dom.addEventListener("pointermove", (e) => {
      const btn = this.getIntersectedButton(e);
      if (btn) {
        dom.style.cursor = "pointer";
      } else {
        dom.style.cursor = "default";
      }
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
    const mesh = this.buttonMeshes[key];
    if (!mesh) return;

    this.pressedButtons.add(key);
    const orig = this.buttonOriginalPositions.get(mesh);
    if (orig) {
      if (mesh.userData.isShoulder) {
        mesh.position.y = orig.y - 0.16; // Depress shoulder bumper down
      } else {
        mesh.position.z = orig.z - 0.14; // Depress face button into body
      }
    }

    if (triggerCallback && this.onButtonPress) {
      this.onButtonPress(key);
    }
  }

  releaseButton(key, triggerCallback = false) {
    const mesh = this.buttonMeshes[key];
    if (!mesh) return;

    this.pressedButtons.delete(key);
    const orig = this.buttonOriginalPositions.get(mesh);
    if (orig) {
      mesh.position.x = orig.x;
      mesh.position.y = orig.y;
      mesh.position.z = orig.z;
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
    let roughness = 0.35;
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
        roughness = 0.18;
        transmission = 0.72;
        transparent = true;
        opacity = 0.88;
        break;
      case "oled-black":
        chassisHex = 0x1e272e;
        bezelHex = 0x111418;
        roughness = 0.3;
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
    const width = this.container.clientWidth || 440;
    const height = this.container.clientHeight || 640;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }

  animate() {
    requestAnimationFrame(this.animate);

    if (this.controls) {
      this.controls.update();
    }

    // Refresh canvas texture if emulator or BMO face updated
    if (this.screenTexture) {
      this.screenTexture.needsUpdate = true;
    }

    if (this.renderer && this.scene && this.camera) {
      this.renderer.render(this.scene, this.camera);
    }
  }
}

window.Bmo3DConsole = Bmo3DConsole;

