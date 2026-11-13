/**
 * Interactive Three.js console view. Dimensions are a design model in centimetres;
 * the matching printable parts are generated in millimetres by hardware/enclosure/.
 * The soldered board, display module, port positions, and battery bay remain unmeasured.
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
    window.addEventListener("resize", () => this.onResize());

    // 9. Render Loop
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  setupLighting() {
    // Broad fill keeps the physical materials readable in the simulator.
    this.ambientLight = new THREE.AmbientLight(0xffffff, 0.74);
    this.scene.add(this.ambientLight);

    // Directional key light from the front-right adds shape to the molded edges.
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

    // Rim light lifts the rear edge during orbit views.
    this.rimLight = new THREE.DirectionalLight(0xffffff, 0.50);
    this.rimLight.position.set(0, 8, -12);
    this.scene.add(this.rimLight);

    // Subtle procedural studio softbox environment map for metal RF shields, screws, and glass
    this.setupStudioEnvironment();

    // Initial studio lighting calibration
    this.applyStudioAdjustments();
  }

  setupStudioEnvironment() {
    if (typeof THREE.PMREMGenerator !== "function") return;
    try {
      const pmrem = new THREE.PMREMGenerator(this.renderer);
      const envScene = new THREE.Scene();
      envScene.background = new THREE.Color(0x1b232e);
      const addPanel = (colorHex, w, h, d, x, y, z) => {
        const panel = new THREE.Mesh(
          new THREE.BoxGeometry(w, h, d),
          new THREE.MeshBasicMaterial({ color: colorHex })
        );
        panel.position.set(x, y, z);
        envScene.add(panel);
      };
      addPanel(0xffffff, 18, 12, 0.5, 0, 12, 8);
      addPanel(0xdbeeff, 14, 10, 0.5, -14, 4, 4);
      addPanel(0xfff1d6, 14, 10, 0.5, 14, 2, -4);
      addPanel(0x528294, 24, 24, 0.5, 0, -14, 0);
      const rt = pmrem.fromScene(envScene, 0.04);
      this.scene.environment = rt.texture;
      pmrem.dispose();
    } catch (_err) {
      // Fallback safely to direct lighting if PMREM is unavailable
    }
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
    tex.encoding = THREE.sRGBEncoding;
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    return tex;
  }

  createButtonTexture(label, bgColorStr, textColorStr) {
    const canvas = document.createElement("canvas");
    canvas.width = 256;
    canvas.height = 256;
    const ctx = canvas.getContext("2d");

    // Fill the cap texture with its base color before drawing the legend.
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
    texture.encoding = THREE.sRGBEncoding;
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
    texture.encoding = THREE.sRGBEncoding;
    texture.minFilter = THREE.LinearFilter;
    texture.magFilter = THREE.LinearFilter;
    return texture;
  }

  decodeCadMesh(partName) {
    const bundle = window.BMO_ENCLOSURE_MESHES;
    if (!bundle || !bundle.parts || !bundle.parts[partName]) return null;
    const part = bundle.parts[partName];

    const decodeB64 = (b64) => {
      const bin = atob(b64);
      const bytes = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) {
        bytes[i] = bin.charCodeAt(i);
      }
      return bytes.buffer;
    };

    const positions = new Float32Array(decodeB64(part.posB64));
    const normI16 = new Int16Array(decodeB64(part.normI16B64));
    const normals = new Float32Array(normI16.length);
    for (let i = 0; i < normI16.length; i++) {
      normals[i] = normI16[i] / 32767.0;
    }
    const idxBuf = decodeB64(part.idxB64);
    const indices = part.indexType === "u32" ? new Uint32Array(idxBuf) : new Uint16Array(idxBuf);

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute("normal", new THREE.BufferAttribute(normals, 3));
    geometry.setIndex(new THREE.BufferAttribute(indices, 1));
    if (Array.isArray(part.groups)) {
      for (const [start, count, matIndex] of part.groups) {
        geometry.addGroup(start, count, matIndex);
      }
    }
    return geometry;
  }

  buildConsoleModel() {
    this.rootGroup = new THREE.Group();
    this.scene.add(this.rootGroup);

    // Model coordinates are centimetres; the enclosure generator uses millimetres.
    const W = 8.2;
    const H = 12.4;
    const D = 2.6;
    const hw = W / 2;
    const hh = H / 2;
    const frontZ = D / 2;
    const backZ = -frontZ;
    const displayX = 0;
    const displayY = 2.9;
    const displayW = 5.0;
    const displayH = 3.75;
    const capRadius = 0.45;
    const capHeight = 0.22;
    const px = (hex) => new THREE.Color(hex).convertSRGBToLinear();

    this.frontShellGroup = new THREE.Group();
    this.frontShellGroup.name = "BMO_Front_Shell";
    this.buttonsGroup = new THREE.Group();
    this.buttonsGroup.name = "BMO_Front_Buttons";
    this.backShellGroup = new THREE.Group();
    this.backShellGroup.name = "BMO_Back_Shell";
    this.batteryDoorGroup = new THREE.Group();
    this.batteryDoorGroup.name = "BMO_Battery_Door";
    this.rootGroup.add(this.frontShellGroup, this.buttonsGroup, this.backShellGroup, this.batteryDoorGroup);

    this.modelDimensions = { width: W, height: H, depth: D, units: "cm" };
    this.materials.chassis = new THREE.MeshPhysicalMaterial({
      color: px(0x63c5b8),
      roughness: 0.58,
      metalness: 0,
      clearcoat: 0.08,
      clearcoatRoughness: 0.52,
    });
    this.materials.backShell = new THREE.MeshPhysicalMaterial({
      color: px(0x5ab5a9),
      roughness: 0.62,
      metalness: 0,
      clearcoat: 0.06,
      clearcoatRoughness: 0.56,
    });
    this.materials.batteryDoor = new THREE.MeshPhysicalMaterial({
      color: px(0x56aea3),
      roughness: 0.64,
      metalness: 0,
    });
    this.materials.glyphInk = new THREE.MeshStandardMaterial({
      color: px(0x101e26),
      roughness: 0.72,
      metalness: 0.04,
    });
    this.materials.bezel = new THREE.MeshStandardMaterial({ color: px(0x17353a), roughness: 0.52 });
    this.materials.dpad = new THREE.MeshPhysicalMaterial({ color: px(0xf4d03f), roughness: 0.48, clearcoat: 0.08 });
    this.materials.btnPill = new THREE.MeshPhysicalMaterial({ color: px(0x264b42), roughness: 0.5, clearcoat: 0.05 });
    this.materials.shoulder = new THREE.MeshPhysicalMaterial({ color: px(0x264b42), roughness: 0.48, clearcoat: 0.08 });
    this.materials.slot = new THREE.MeshStandardMaterial({ color: px(0x0f171a), roughness: 0.72 });

    const traceRoundedRect = (shape, width, height, radius, clockwise = false, centerX = 0, centerY = 0) => {
      const x = width / 2;
      const y = height / 2;
      const r = Math.min(radius, x, y);
      if (clockwise) {
        shape.moveTo(centerX - x + r, centerY + y);
        shape.lineTo(centerX + x - r, centerY + y);
        shape.quadraticCurveTo(centerX + x, centerY + y, centerX + x, centerY + y - r);
        shape.lineTo(centerX + x, centerY - y + r);
        shape.quadraticCurveTo(centerX + x, centerY - y, centerX + x - r, centerY - y);
        shape.lineTo(centerX - x + r, centerY - y);
        shape.quadraticCurveTo(centerX - x, centerY - y, centerX - x, centerY - y + r);
        shape.lineTo(centerX - x, centerY + y - r);
        shape.quadraticCurveTo(centerX - x, centerY + y, centerX - x + r, centerY + y);
        shape.closePath();
        return shape;
      }
      shape.moveTo(centerX - x + r, centerY - y);
      shape.lineTo(centerX + x - r, centerY - y);
      shape.quadraticCurveTo(centerX + x, centerY - y, centerX + x, centerY - y + r);
      shape.lineTo(centerX + x, centerY + y - r);
      shape.quadraticCurveTo(centerX + x, centerY + y, centerX + x - r, centerY + y);
      shape.lineTo(centerX - x + r, centerY + y);
      shape.quadraticCurveTo(centerX - x, centerY + y, centerX - x, centerY + y - r);
      shape.lineTo(centerX - x, centerY - y + r);
      shape.quadraticCurveTo(centerX - x, centerY - y, centerX - x + r, centerY - y);
      shape.closePath();
      return shape;
    };
    const roundedRect = (width, height, radius) => traceRoundedRect(new THREE.Shape(), width, height, radius);
    const extrudedRounded = (width, height, radius, depth, bevel = 0) => {
      const geometry = new THREE.ExtrudeGeometry(roundedRect(width, height, radius), {
        depth,
        bevelEnabled: bevel > 0,
        bevelSegments: bevel > 0 ? 4 : 1,
        bevelSize: bevel,
        bevelThickness: bevel,
        curveSegments: 12,
      });
      geometry.center();
      return geometry;
    };
    const ringGeometry = (outerW, outerH, innerW, innerH, radius) => {
      const shape = roundedRect(outerW, outerH, radius);
      const path = new THREE.Path();
      const x = innerW / 2;
      const y = innerH / 2;
      const r = Math.min(radius * 0.6, x, y);
      path.moveTo(-x + r, -y);
      path.lineTo(-x, -y + r);
      path.quadraticCurveTo(-x, -y, -x + r, -y);
      path.lineTo(x - r, -y);
      path.quadraticCurveTo(x, -y, x, -y + r);
      path.lineTo(x, y - r);
      path.quadraticCurveTo(x, y, x - r, y);
      path.lineTo(-x + r, y);
      path.quadraticCurveTo(-x, y, -x, y - r);
      path.lineTo(-x, -y + r);
      path.quadraticCurveTo(-x, -y, -x + r, -y);
      path.closePath();
      shape.holes.push(path);
      const geometry = new THREE.ExtrudeGeometry(shape, { depth: 0.02, bevelEnabled: false, curveSegments: 12 });
      geometry.center();
      return geometry;
    };
    const addFrontText = (text, width, height, fontSize, color) => {
      const canvas = document.createElement("canvas");
      canvas.width = 256;
      canvas.height = 96;
      const ctx = canvas.getContext("2d");
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.fillStyle = color;
      ctx.font = `bold ${fontSize}px 'Press Start 2P', monospace`;
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(text, canvas.width / 2, canvas.height / 2);
      const texture = new THREE.CanvasTexture(canvas);
      texture.encoding = THREE.sRGBEncoding;
      texture.minFilter = THREE.LinearFilter;
      texture.magFilter = THREE.LinearFilter;
      const material = new THREE.MeshBasicMaterial({ map: texture, transparent: true, depthWrite: false, side: THREE.DoubleSide });
      return new THREE.Mesh(new THREE.PlaneGeometry(width, height), material);
    };

    // Load true hollow, through-punched CAD shell halves and battery door when available.
    const frontCadGeo = this.decodeCadMesh("bmo_front_shell");
    const backCadGeo = this.decodeCadMesh("bmo_back_shell");
    const doorCadGeo = this.decodeCadMesh("bmo_battery_door");

    const shellShape = roundedRect(W - 0.2, H - 0.2, 0.82);
    const frontShape = roundedRect(W - 0.2, H - 0.2, 0.82);
    frontShape.holes.push(traceRoundedRect(new THREE.Path(), displayW, displayH, 0.12, true, displayX, displayY));
    const makeFallbackHalf = (z, material, shape = shellShape) => {
      const geometry = new THREE.ExtrudeGeometry(shape, {
        depth: D / 2 - 0.2,
        bevelEnabled: true,
        bevelSegments: 6,
        steps: 1,
        bevelSize: 0.1,
        bevelThickness: 0.1,
        curveSegments: 16,
      });
      geometry.center();
      const mesh = new THREE.Mesh(geometry, material);
      mesh.position.z = z;
      return mesh;
    };

    if (frontCadGeo) {
      this.chassisMesh = new THREE.Mesh(frontCadGeo, [this.materials.chassis, this.materials.glyphInk]);
    } else {
      this.chassisMesh = makeFallbackHalf(frontZ / 2, this.materials.chassis, frontShape);
    }
    this.chassisMesh.castShadow = true;
    this.chassisMesh.receiveShadow = true;
    this.frontShellGroup.add(this.chassisMesh);

    if (backCadGeo) {
      this.backShellMesh = new THREE.Mesh(backCadGeo, [this.materials.backShell, this.materials.glyphInk]);
    } else {
      this.backShellMesh = makeFallbackHalf(backZ / 2, this.materials.backShell);
    }
    this.backShellMesh.castShadow = true;
    this.backShellMesh.receiveShadow = true;
    this.backShellGroup.add(this.backShellMesh);

    if (doorCadGeo) {
      this.batteryDoorMesh = new THREE.Mesh(doorCadGeo, this.materials.batteryDoor);
      this.batteryDoorMesh.castShadow = true;
      this.batteryDoorMesh.receiveShadow = true;
      this.batteryDoorGroup.add(this.batteryDoorMesh);
    }

    // Internal dark acoustic dust-screen baffles mounted behind the inner side wall (no outer overlap).
    const feltMat = new THREE.MeshStandardMaterial({ color: px(0x0d1518), roughness: 0.92, side: THREE.DoubleSide });
    for (const side of [-1, 1]) {
      const felt = new THREE.Mesh(new THREE.BoxGeometry(0.05, 1.35, 1.35), feltMat);
      felt.position.set(side * 3.84, 4.30, 0);
      this.rootGroup.add(felt);
      this.acousticFeltMeshes = this.acousticFeltMeshes || [];
      this.acousticFeltMeshes.push(felt);
    }

    // Glass panel seated inside the front display recess with crisp nearest-neighbour TFT canvas.
    const glassBase = new THREE.Mesh(
      new THREE.PlaneGeometry(displayW + 0.20, displayH + 0.20),
      new THREE.MeshPhysicalMaterial({
        color: px(0x07080a),
        roughness: 0.12,
        metalness: 0,
        clearcoat: 0.9,
        clearcoatRoughness: 0.08,
        polygonOffset: true,
        polygonOffsetFactor: -1,
        polygonOffsetUnits: -1,
      })
    );
    glassBase.position.set(displayX, displayY, frontZ - 0.015);
    this.frontShellGroup.add(glassBase);

    const bezelRing = new THREE.Mesh(
      ringGeometry(5.48, 4.22, 5.20, 3.95, 0.24),
      this.materials.bezel
    );
    bezelRing.position.set(displayX, displayY, frontZ + 0.004);
    this.frontShellGroup.add(bezelRing);

    if (this.tftCanvas) {
      this.screenTexture = new THREE.CanvasTexture(this.tftCanvas);
      this.screenTexture.encoding = THREE.sRGBEncoding;
      this.screenTexture.minFilter = THREE.NearestFilter;
      this.screenTexture.magFilter = THREE.NearestFilter;
      this.screenTexture.generateMipmaps = false;
    }
    const screenMaterial = new THREE.MeshPhysicalMaterial({
      color: 0xffffff,
      map: this.screenTexture,
      roughness: 0.12,
      metalness: 0,
      clearcoat: 0.85,
      clearcoatRoughness: 0.08,
      polygonOffset: true,
      polygonOffsetFactor: -2,
      polygonOffsetUnits: -2,
    });
    this.screenMesh = new THREE.Mesh(new THREE.PlaneGeometry(displayW, displayH), screenMaterial);
    this.screenMesh.position.set(displayX, displayY, frontZ + 0.008);
    this.frontShellGroup.add(this.screenMesh);

    // Decorative front drive slot and low-profile blue button below the screen.
    const slot = new THREE.Mesh(new THREE.BoxGeometry(4.2, 0.2, 0.04), this.materials.slot);
    slot.position.set(-0.02, 0.42, frontZ + 0.025);
    this.frontShellGroup.add(slot);
    const eject = new THREE.Mesh(
      new THREE.CylinderGeometry(0.15, 0.15, 0.12, 40),
      new THREE.MeshPhysicalMaterial({ color: px(0x2874a6), roughness: 0.38, clearcoat: 0.25 })
    );
    eject.rotation.x = Math.PI / 2;
    eject.position.set(2.45, 0.42, frontZ + 0.07);
    this.frontShellGroup.add(eject);

    // Four separate raycast regions share one rounded cross with a central pivot dish.
    const dpadX = -1.9;
    const dpadY = -2.2;
    const armWidth = 0.95;
    const halfSpan = 1.4;
    const cr = 0.08;
    const dpadShape = new THREE.Shape();
    dpadShape.moveTo(-armWidth / 2 + cr, halfSpan);
    dpadShape.lineTo(armWidth / 2 - cr, halfSpan);
    dpadShape.quadraticCurveTo(armWidth / 2, halfSpan, armWidth / 2, halfSpan - cr);
    dpadShape.lineTo(armWidth / 2, armWidth / 2 + cr);
    dpadShape.quadraticCurveTo(armWidth / 2, armWidth / 2, armWidth / 2 + cr, armWidth / 2);
    dpadShape.lineTo(halfSpan - cr, armWidth / 2);
    dpadShape.quadraticCurveTo(halfSpan, armWidth / 2, halfSpan, armWidth / 2 - cr);
    dpadShape.lineTo(halfSpan, -armWidth / 2 + cr);
    dpadShape.quadraticCurveTo(halfSpan, -armWidth / 2, halfSpan - cr, -armWidth / 2);
    dpadShape.lineTo(armWidth / 2 + cr, -armWidth / 2);
    dpadShape.quadraticCurveTo(armWidth / 2, -armWidth / 2, armWidth / 2, -armWidth / 2 - cr);
    dpadShape.lineTo(armWidth / 2, -halfSpan + cr);
    dpadShape.quadraticCurveTo(armWidth / 2, -halfSpan, armWidth / 2 - cr, -halfSpan);
    dpadShape.lineTo(-armWidth / 2 + cr, -halfSpan);
    dpadShape.quadraticCurveTo(-armWidth / 2, -halfSpan, -armWidth / 2, -halfSpan + cr);
    dpadShape.lineTo(-armWidth / 2, -armWidth / 2 - cr);
    dpadShape.quadraticCurveTo(-armWidth / 2, -armWidth / 2, -armWidth / 2 - cr, -armWidth / 2);
    dpadShape.lineTo(-halfSpan + cr, -armWidth / 2);
    dpadShape.quadraticCurveTo(-halfSpan, -armWidth / 2, -halfSpan, -armWidth / 2 + cr);
    dpadShape.lineTo(-halfSpan, armWidth / 2 - cr);
    dpadShape.quadraticCurveTo(-halfSpan, armWidth / 2, -halfSpan + cr, armWidth / 2);
    dpadShape.lineTo(-armWidth / 2 - cr, armWidth / 2);
    dpadShape.quadraticCurveTo(-armWidth / 2, armWidth / 2, -armWidth / 2, armWidth / 2 + cr);
    dpadShape.lineTo(-armWidth / 2, halfSpan - cr);
    dpadShape.quadraticCurveTo(-armWidth / 2, halfSpan, -armWidth / 2 + cr, halfSpan);
    dpadShape.closePath();

    const dpadGeo = new THREE.ExtrudeGeometry(dpadShape, {
      depth: 0.2,
      bevelEnabled: true,
      bevelSegments: 4,
      bevelSize: 0.04,
      bevelThickness: 0.035,
      curveSegments: 12,
    });
    dpadGeo.center();
    this.dpadPivotGroup = new THREE.Group();
    this.dpadPivotGroup.position.set(dpadX, dpadY, frontZ + 0.12);
    this.dpadRestZ = frontZ + 0.12;
    this.buttonsGroup.add(this.dpadPivotGroup);
    this.dpadBodyMesh = new THREE.Mesh(dpadGeo, this.materials.dpad);
    this.dpadBodyMesh.castShadow = true;
    this.dpadBodyMesh.receiveShadow = true;
    this.dpadPivotGroup.add(this.dpadBodyMesh);
    const dpadTex = this.createDpadTexture();
    const dpadFace = new THREE.Mesh(new THREE.PlaneGeometry(2.8, 2.8), new THREE.MeshBasicMaterial({ map: dpadTex, transparent: true, depthWrite: false }));
    dpadFace.position.z = 0.145;
    this.dpadPivotGroup.add(dpadFace);

    const dpadZone = (key, x, y, width, height) => {
      const zone = new THREE.Mesh(new THREE.PlaneGeometry(width, height), new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false }));
      zone.position.set(x, y, 0.16);
      zone.userData = { isButton: true, key, isDpad: true };
      this.dpadPivotGroup.add(zone);
      this.buttonMeshes[key] = zone;
      this.interactiveObjects.push(zone);
    };
    dpadZone("UP", 0, 0.85, armWidth, 1.1);
    dpadZone("DOWN", 0, -0.85, armWidth, 1.1);
    dpadZone("LEFT", -0.85, 0, 1.1, armWidth);
    dpadZone("RIGHT", 0.85, 0, 1.1, armWidth);

    // Tactile face caps framed directly inside real CSG front shell bores.
    const actionButtons = [
      ["X", 1.8, -1.2, "#4caf50", "#10251f"],
      ["Y", 0.9, -2.2, "#f48fb1", "#10251f"],
      ["A", 2.7, -2.2, "#52a6ba", "#10251f"],
      ["B", 1.8, -3.2, "#ff91a5", "#10251f"],
    ];
    for (const [key, x, y, color, labelColor] of actionButtons) {
      const group = new THREE.Group();
      group.position.set(x, y, frontZ + capHeight / 2);
      this.buttonsGroup.add(group);
      const capShape = new THREE.LatheGeometry([
        new THREE.Vector2(0, -capHeight / 2),
        new THREE.Vector2(capRadius * 0.88, -capHeight / 2),
        new THREE.Vector2(capRadius * 0.98, -capHeight / 2 + 0.035),
        new THREE.Vector2(capRadius, -capHeight / 2 + 0.08),
        new THREE.Vector2(capRadius, capHeight / 2 - 0.055),
        new THREE.Vector2(capRadius * 0.91, capHeight / 2),
        new THREE.Vector2(0, capHeight / 2),
      ], 48);
      capShape.rotateX(Math.PI / 2);
      const cap = new THREE.Mesh(capShape, new THREE.MeshPhysicalMaterial({ color: px(color), roughness: 0.48, metalness: 0, clearcoat: 0.12, clearcoatRoughness: 0.38 }));
      cap.castShadow = true;
      group.add(cap);
      const face = new THREE.Mesh(new THREE.CircleGeometry(capRadius - 0.015, 48), new THREE.MeshStandardMaterial({ map: this.createButtonTexture(key, color, labelColor), roughness: 0.55, metalness: 0 }));
      face.position.z = capHeight / 2 + 0.002;
      group.add(face);
      const hit = new THREE.Mesh(new THREE.CylinderGeometry(capRadius + 0.03, capRadius + 0.03, 0.32, 32), new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false }));
      hit.rotation.x = Math.PI / 2;
      group.add(hit);
      const data = { isButton: true, key, name: `${key} Button`, isAction: true };
      cap.userData = data;
      face.userData = data;
      hit.userData = data;
      this.buttonMeshes[key] = group;
      this.interactiveObjects.push(cap, face, hit);
      this.buttonOriginalPositions.set(group, { x: group.position.x, y: group.position.y, z: group.position.z, rotX: 0, rotY: 0, rotZ: 0 });
    }

    const makePill = (key, label, x) => {
      const y = -4.35;
      const group = new THREE.Group();
      group.position.set(x, y, frontZ + 0.1);
      this.buttonsGroup.add(group);
      const pill = new THREE.Mesh(extrudedRounded(1.15, 0.40, 0.20, 0.16, 0.035), this.materials.btnPill);
      pill.castShadow = true;
      pill.userData = { isButton: true, key, name: label, isPill: true };
      group.add(pill);
      this.buttonMeshes[key] = group;
      this.interactiveObjects.push(pill);
      this.buttonOriginalPositions.set(group, { x: group.position.x, y: group.position.y, z: group.position.z, rotX: 0, rotY: 0, rotZ: 0 });
      const legend = addFrontText(label, 1.3, 0.28, 27, "#173b35");
      legend.position.set(x, y - 0.48, frontZ + 0.018);
      this.frontShellGroup.add(legend);
    };
    makePill("SELECT", "SELECT", -2.55);
    makePill("START", "START", -1.15);

    // Raised shoulders clear the uniform top fillet; the smaller legends sit on the visible face.
    const shoulderShape = roundedRect(1.8, 0.52, 0.14);
    const shoulderDepth = 0.68;
    for (const [key, x] of [["L", -2.7], ["R", 2.7]]) {
      const geometry = new THREE.ExtrudeGeometry(shoulderShape, {
        depth: shoulderDepth,
        bevelEnabled: true,
        bevelSegments: 4,
        bevelSize: 0.04,
        bevelThickness: 0.035,
        curveSegments: 12,
      });
      geometry.center();
      const shoulder = new THREE.Mesh(geometry, this.materials.shoulder);
      shoulder.position.set(x, hh + 0.20, 0.76);
      shoulder.castShadow = true;
      shoulder.userData = { isButton: true, key, name: `${key} Shoulder`, isShoulder: true };
      this.buttonsGroup.add(shoulder);
      const legend = addFrontText(key, 0.9, 0.34, 36, "#d7f2e9");
      legend.position.set(0, 0.05, shoulderDepth / 2 + 0.05);
      shoulder.add(legend);
      const hit = new THREE.Mesh(new THREE.BoxGeometry(2.0, 0.78, 0.9), new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false }));
      hit.userData = shoulder.userData;
      shoulder.add(hit);
      this.buttonMeshes[key] = shoulder;
      this.interactiveObjects.push(shoulder, hit);
      this.buttonOriginalPositions.set(shoulder, { x: shoulder.position.x, y: shoulder.position.y, z: shoulder.position.z, rotX: 0, rotY: 0, rotZ: 0 });
    }

    // Side speaker bores, engraved BMO glyphs, rear vents, battery door, M2 fasteners,
    // lower-left MicroSD slot, and bottom USB-C cutout are all part of the CSG shell
    // geometry and the layered electronics sub-assembly below.
    if (typeof window.BmoInternals3D === "function") {
      this.internals = new window.BmoInternals3D(this.rootGroup);
    }
  }
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
    let pointerDownPos = null;

    dom.addEventListener("pointerdown", (e) => {
      pointerDownPos = { x: e.clientX, y: e.clientY };
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
        pointerDownPos = null;
        return;
      }
      if (pointerDownPos && e.target === dom) {
        const dist = Math.hypot(e.clientX - pointerDownPos.x, e.clientY - pointerDownPos.y);
        if (dist < 6 && this.internals) {
          const hit = this.getIntersectedInternal(e);
          if (hit && hit.userData && hit.userData.isInternalPart) {
            const tag = hit.userData.wireId || hit.userData.groupTag || "all";
            this.internals.setHighlight(tag);
            if (typeof this.onInternalSelect === "function") {
              this.onInternalSelect(hit.userData);
            }
          }
        }
      }
      pointerDownPos = null;
    });

    dom.addEventListener("pointermove", (e) => {
      const btn = this.getIntersectedButton(e);
      if (btn) {
        dom.style.cursor = "pointer";
        return;
      }
      const internal = this.internals && (this.explodeCurrent > 0.02 || !this.layerVis.frontShell || !this.layerVis.backShell)
        ? this.getIntersectedInternal(e)
        : null;
      dom.style.cursor = internal ? "pointer" : "default";
    });
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

    this.materials.chassis.color.copy(new THREE.Color(chassisHex).convertSRGBToLinear());
    this.materials.chassis.roughness = roughness;
    this.materials.chassis.transmission = transmission;
    this.materials.chassis.transparent = transparent;
    this.materials.chassis.opacity = opacity;
    if (this.materials.backShell) {
      this.materials.backShell.color.copy(new THREE.Color(chassisHex).convertSRGBToLinear());
      this.materials.backShell.roughness = roughness + 0.04;
      this.materials.backShell.transmission = transmission;
      this.materials.backShell.transparent = transparent;
      this.materials.backShell.opacity = opacity;
    }
    if (this.materials.batteryDoor) {
      this.materials.batteryDoor.color.copy(new THREE.Color(chassisHex).convertSRGBToLinear());
    }

    this.materials.bezel.color.copy(new THREE.Color(bezelHex).convertSRGBToLinear());
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
    requestAnimationFrame(this.animate);
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
