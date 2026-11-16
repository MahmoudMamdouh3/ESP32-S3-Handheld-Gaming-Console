/**
 * simulator/js/console_3d_studio.js
 * Three.js studio lighting, PMREM softbox reflections, button/D-pad textures, and CAD mesh decode.
 */

function getThree() {
  return /** @type {any} */ (globalThis).THREE;
}

export function setupStudioEnvironment(scene, renderer) {
  const THREE = getThree();
  if (typeof THREE.PMREMGenerator !== "function") return;
  try {
    const pmrem = new THREE.PMREMGenerator(renderer);
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
    scene.environment = rt.texture;
    pmrem.dispose();
  } catch (_err) {
    // Fallback safely to direct lighting if PMREM is unavailable
  }
}

export function setupLightingImpl(owner) {
  const THREE = getThree();
  owner.ambientLight = new THREE.AmbientLight(0xffffff, 0.74);
  owner.scene.add(owner.ambientLight);

  owner.keyLight = new THREE.DirectionalLight(0xffffff, 0.32);
  owner.keyLight.position.set(3, 7, 10);
  owner.keyLight.castShadow = true;
  owner.keyLight.shadow.mapSize.width = 2048;
  owner.keyLight.shadow.mapSize.height = 2048;
  owner.keyLight.shadow.camera.near = 1;
  owner.keyLight.shadow.camera.far = 30;
  owner.keyLight.shadow.camera.left = -7;
  owner.keyLight.shadow.camera.right = 7;
  owner.keyLight.shadow.camera.top = 9;
  owner.keyLight.shadow.camera.bottom = -9;
  owner.keyLight.shadow.bias = -0.0004;
  owner.keyLight.shadow.radius = 2.0;
  owner.scene.add(owner.keyLight);

  owner.rimLight = new THREE.DirectionalLight(0xffffff, 0.50);
  owner.rimLight.position.set(0, 8, -12);
  owner.scene.add(owner.rimLight);

  owner.setupStudioEnvironment();
  owner.applyStudioAdjustments();
}

export function applyStudioAdjustmentsImpl(owner) {
  const b = owner.studioSettings.brightness;
  const t = owner.studioSettings.temperature;
  const s = owner.studioSettings.saturation;

  let r = 1.0;
  let g = 1.0;
  let bl = 1.0;
  if (t > 0) {
    g = 1.0 - 0.14 * t;
    bl = 1.0 - 0.40 * t;
  } else if (t < 0) {
    const cool = -t;
    r = 1.0 - 0.36 * cool;
    g = 1.0 - 0.10 * cool;
  }

  if (owner.ambientLight) {
    owner.ambientLight.color.setRGB(r, g, bl);
    owner.ambientLight.intensity = 0.74 * b;
  }
  if (owner.keyLight) {
    owner.keyLight.color.setRGB(r, g, bl);
    owner.keyLight.intensity = 0.32 * b;
  }
  if (owner.rimLight) {
    owner.rimLight.intensity = 0.50 * b;
  }
  if (owner.renderer && owner.renderer.domElement) {
    owner.renderer.domElement.style.filter = `saturate(${s})`;
  }
}

export function createDropShadowTextureImpl(radius = 64) {
  const THREE = getThree();
  const canvas = document.createElement("canvas");
  canvas.width = 128;
  canvas.height = 128;
  const ctx = canvas.getContext("2d");
  if (!ctx) return new THREE.Texture();
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

export function createButtonTextureImpl(label, bgColorStr, textColorStr) {
  const THREE = getThree();
  const canvas = document.createElement("canvas");
  canvas.width = 256;
  canvas.height = 256;
  const ctx = canvas.getContext("2d");
  if (!ctx) return new THREE.Texture();

  ctx.fillStyle = bgColorStr;
  ctx.fillRect(0, 0, 256, 256);
  const btmGrad = ctx.createRadialGradient(128, 128, 85, 128, 128, 126);
  btmGrad.addColorStop(0, "rgba(0, 0, 0, 0)");
  btmGrad.addColorStop(0.8, "rgba(0, 0, 0, 0.04)");
  btmGrad.addColorStop(1, "rgba(0, 0, 0, 0.12)");
  ctx.fillStyle = btmGrad;
  ctx.beginPath();
  ctx.arc(128, 128, 126, 0, Math.PI * 2);
  ctx.fill();

  ctx.save();
  ctx.lineWidth = 6;
  ctx.strokeStyle = "rgba(255, 255, 255, 0.40)";
  ctx.beginPath();
  ctx.arc(128, 128, 118, Math.PI * 1.15, Math.PI * 1.85);
  ctx.stroke();
  ctx.restore();

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

export function createDpadTextureImpl() {
  const THREE = getThree();
  const canvas = document.createElement("canvas");
  canvas.width = 512;
  canvas.height = 512;
  const ctx = canvas.getContext("2d");
  if (!ctx) return new THREE.Texture();
  ctx.clearRect(0, 0, 512, 512);
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

export function decodeCadMeshImpl(partName, layoutDims) {
  const THREE = getThree();
  const bundle = /** @type {any} */ (window).BMO_ENCLOSURE_MESHES;
  if (!bundle || !bundle.parts || !bundle.parts[partName]) return null;
  const part = bundle.parts[partName];

  const decodeB64 = (b64) => {
    const bin = atob(b64);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return bytes.buffer;
  };

  const positions = new Float32Array(decodeB64(part.posB64));
  const normI16 = new Int16Array(decodeB64(part.normI16B64));
  const normals = new Float32Array(normI16.length);
  for (let i = 0; i < normI16.length; i++) normals[i] = normI16[i] / 32767.0;
  const idxBuf = decodeB64(part.idxB64);
  const indices = part.indexType === "u32" ? new Uint32Array(idxBuf) : new Uint16Array(idxBuf);

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
  geometry.setAttribute("normal", new THREE.BufferAttribute(normals, 3));
  geometry.setIndex(new THREE.BufferAttribute(indices, 1));
  if (Array.isArray(part.groups)) {
    for (const [start, count, matIndex] of part.groups) geometry.addGroup(start, count, matIndex);
  }
  const cadDims = bundle.dimensionsMm;
  if (cadDims && cadDims.outerWidth && cadDims.outerHeight && cadDims.outerThickness && layoutDims) {
    geometry.scale(
      (layoutDims.shellWidthCm * 10) / cadDims.outerWidth,
      (layoutDims.shellHeightCm * 10) / cadDims.outerHeight,
      (layoutDims.shellDepthCm * 10) / cadDims.outerThickness
    );
  }
  return geometry;
}
