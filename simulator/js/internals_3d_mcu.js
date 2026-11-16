/**
 * simulator/js/internals_3d_mcu.js
 * ESP32-S3-DevKitC-1, MicroSD breakout, wire harness, and expansion layer builders.
 */

import {
  DEVKIT_FIRST_PIN_OFFSET_Y,
  DEVKIT_J1_PINS,
  DEVKIT_J3_PINS,
  DEVKIT_PCB_Z,
  DEVKIT_PIN_PITCH,
  DEVKIT_ROW_HALF_X,
  DEVKIT_X,
  DEVKIT_Y,
  WIRE_DEFINITIONS,
  createSilkscreenTexture,
  getDevkitPinWorldPos,
  getThree,
  px,
} from "./internals_3d_data.js";

function createDevKitPcbTexture() {
  return createSilkscreenTexture(320, 780, (ctx, w, h) => {
    ctx.fillStyle = "#16191f";
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "#2d333d";
    ctx.lineWidth = 4;
    ctx.strokeRect(4, 4, w - 8, h - 8);
    ctx.font = "bold 12px monospace";
    for (let i = 0; i < 22; i++) {
      const py = 122 + i * 28;
      ctx.textAlign = "left";
      ctx.fillStyle = ["3", "46", "0", "45", "35", "36", "37"].includes(DEVKIT_J1_PINS[i]) ? "#ff6b6b" : "#9fe3c2";
      ctx.fillText(DEVKIT_J1_PINS[i], 24, py + 4);
      ctx.textAlign = "right";
      const j3Name = DEVKIT_J3_PINS[i];
      ctx.fillStyle = ["0", "45", "35", "36", "37"].includes(j3Name.split("/")[0]) ? "#ff6b6b" : "#9ec8ff";
      ctx.fillText(j3Name, w - 24, py + 4);
    }
    ctx.fillStyle = "#b0bac4";
    ctx.font = "bold 13px monospace";
    ctx.textAlign = "center";
    ctx.fillText("ESP32-S3-DevKitC-1", w / 2, h - 78);
    ctx.font = "11px monospace";
    ctx.fillStyle = "#77c9bc";
    ctx.fillText("N16R8 (16M Flash / 8M PSRAM)", w / 2, h - 58);
    ctx.fillStyle = "#e8eef2";
    ctx.fillText("UART       USB", w / 2, h - 18);
  });
}

function addDevKitRfAndUsb(THREE, g) {
  const rfCanTex = createSilkscreenTexture(256, 256, (ctx, w, h) => {
    ctx.fillStyle = "#cfd5dc";
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = "#3a4048";
    ctx.font = "bold 20px monospace";
    ctx.textAlign = "center";
    ctx.fillText("ESP32-S3", w / 2, 102);
    ctx.fillText("WROOM-1", w / 2, 130);
    ctx.font = "bold 16px monospace";
    ctx.fillText("N16R8", w / 2, 162);
  });
  const canMesh = new THREE.Mesh(
    new THREE.BoxGeometry(1.80, 1.92, 0.24),
    new THREE.MeshStandardMaterial({ map: rfCanTex, metalness: 0.85, roughness: 0.24 })
  );
  canMesh.position.set(0, -1.26, -0.22);
  canMesh.rotation.y = Math.PI;
  const antPcb = new THREE.Mesh(
    new THREE.BoxGeometry(1.80, 0.63, 0.08),
    new THREE.MeshStandardMaterial({ color: px(0x1b1e24), roughness: 0.5 })
  );
  antPcb.position.set(0, 0.01, -0.14);
  const goldMat = new THREE.MeshStandardMaterial({ color: px(0xe0b548), metalness: 0.88, roughness: 0.22 });
  const meander = new THREE.Mesh(new THREE.BoxGeometry(1.40, 0.06, 0.09), goldMat);
  meander.position.set(0, 0.16, -0.14);
  g.add(canMesh, antPcb, meander);

  const usbShellMat = new THREE.MeshStandardMaterial({ color: px(0xb8c0c8), metalness: 0.90, roughness: 0.22 });
  const usbInteriorMat = new THREE.MeshStandardMaterial({ color: px(0x11151a), roughness: 0.8 });
  for (const ux of [-0.62, 0.62]) {
    const shell = new THREE.Mesh(new THREE.BoxGeometry(0.90, 0.74, 0.32), usbShellMat);
    shell.position.set(ux, -5.66, -0.24);
    const portHole = new THREE.Mesh(new THREE.BoxGeometry(0.76, 0.12, 0.20), usbInteriorMat);
    portHole.position.set(ux, -5.98, -0.24);
    g.add(shell, portHole);
  }
  return { goldMat, usbShellMat };
}

function addDevKitChipsAndHeaders(THREE, owner, g, goldMat, usbShellMat) {
  const chipMat = new THREE.MeshStandardMaterial({ color: px(0x1a1e24), roughness: 0.45 });
  const cp2102 = new THREE.Mesh(new THREE.BoxGeometry(0.50, 0.50, 0.10), chipMat);
  cp2102.position.set(-0.42, -4.40, -0.14);
  const ams1117 = new THREE.Mesh(new THREE.BoxGeometry(0.65, 0.45, 0.16), chipMat);
  ams1117.position.set(0.45, -3.55, -0.16);
  const ws2812 = new THREE.Mesh(
    new THREE.BoxGeometry(0.35, 0.35, 0.12),
    new THREE.MeshStandardMaterial({ color: px(0xf4fff8), emissive: px(0x47d7b8), emissiveIntensity: 0.55, roughness: 0.3 })
  );
  ws2812.position.set(0.15, -3.05, -0.15);
  g.add(cp2102, ams1117, ws2812);

  for (const bx of [-0.55, 0.55]) {
    const btn = new THREE.Mesh(new THREE.BoxGeometry(0.36, 0.26, 0.16), usbShellMat);
    btn.position.set(bx, -5.00, -0.16);
    g.add(btn);
  }

  const pinGeo = new THREE.CylinderGeometry(0.032, 0.032, 0.54, 8);
  pinGeo.rotateX(Math.PI / 2);
  const pinMesh = new THREE.InstancedMesh(pinGeo, goldMat, 44);
  const dummy = new THREE.Object3D();
  let pIdx = 0;
  for (let sideIdx = 0; sideIdx < 2; sideIdx++) {
    const pxX = sideIdx === 0 ? DEVKIT_ROW_HALF_X : -DEVKIT_ROW_HALF_X;
    for (let r = 0; r < 22; r++) {
      dummy.position.set(pxX, DEVKIT_Y + DEVKIT_FIRST_PIN_OFFSET_Y - r * DEVKIT_PIN_PITCH, 0.12);
      dummy.updateMatrix();
      pinMesh.setMatrixAt(pIdx++, dummy.matrix);
    }
  }
  g.add(pinMesh);

  owner.forbiddenMarkerGroup = new THREE.Group();
  const warnMat = new THREE.MeshStandardMaterial({
    color: px(0xff334b),
    emissive: px(0xff1e38),
    emissiveIntensity: 0.85,
    roughness: 0.3,
  });
  const forbiddenList = [["J1", "3"], ["J1", "46"], ["J3", "0"], ["J3", "45"], ["J3", "35"], ["J3", "36"], ["J3", "37"]];
  for (const [hdr, pName] of forbiddenList) {
    const [fx, fy] = getDevkitPinWorldPos(hdr, pName, 0);
    const marker = new THREE.Mesh(new THREE.SphereGeometry(0.11, 14, 14), warnMat);
    marker.position.set(fx, fy, -0.16);
    owner.forbiddenMarkerGroup.add(marker);
  }
  owner.forbiddenMarkerGroup.visible = false;
  g.add(owner.forbiddenMarkerGroup);
}

export function buildEsp32DevKitC1(owner) {
  const THREE = getThree();
  const g = owner.layers.esp32;
  const pcb = new THREE.Mesh(
    new THREE.BoxGeometry(2.54, 6.274, 0.16),
    new THREE.MeshStandardMaterial({ map: createDevKitPcbTexture(), roughness: 0.46, metalness: 0.1 })
  );
  pcb.position.set(DEVKIT_X, DEVKIT_Y, DEVKIT_PCB_Z);
  pcb.rotation.y = Math.PI;
  pcb.castShadow = true;
  pcb.receiveShadow = true;
  g.add(pcb);
  owner.registerInteractive(pcb, {
    name: "ESP32-S3-DevKitC-1 N16R8",
    role: "Core MCU (25.4 x 62.74 mm)",
    detail: "2x22 headers (44 pins). Native USB OTG + UART CP2102N sit at bottom y=-59.5 mm. Avoid GPIO 0, 3, 26-37, 45, 46.",
    groupTag: "esp32",
  });
  const { goldMat, usbShellMat } = addDevKitRfAndUsb(THREE, g);
  addDevKitChipsAndHeaders(THREE, owner, g, goldMat, usbShellMat);
}

export function buildMicroSdModule(owner) {
  const THREE = getThree();
  const g = owner.layers.microsd;
  const sdTex = createSilkscreenTexture(240, 420, (ctx, w, h) => {
    ctx.fillStyle = "#194a8a";
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "#e8f0f8";
    ctx.lineWidth = 3;
    ctx.strokeRect(4, 4, w - 8, h - 8);
    ctx.fillStyle = "#f2f6fa";
    ctx.font = "bold 14px monospace";
    ctx.textAlign = "left";
    ["CS", "MOSI", "SCK", "MISO", "VCC", "GND"].forEach((lbl, idx) => ctx.fillText(lbl, 16, 150 + idx * 25.4));
    ctx.save();
    ctx.translate(w - 34, h / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = "center";
    ctx.font = "bold 16px monospace";
    ctx.fillText("MICRO SD ADAPTER", 0, 0);
    ctx.restore();
  });

  const pcb = new THREE.Mesh(
    new THREE.BoxGeometry(2.40, 4.20, 0.16),
    new THREE.MeshStandardMaterial({ map: sdTex, roughness: 0.45, metalness: 0.1 })
  );
  pcb.position.set(-2.65, -3.40, -0.22);
  pcb.rotation.y = Math.PI;
  pcb.castShadow = true;
  pcb.receiveShadow = true;
  g.add(pcb);
  owner.registerInteractive(pcb, {
    name: "6-Pin SPI MicroSD Breakout",
    role: "Storage Module (24 x 42 mm)",
    detail: "Mounted at y=-34 mm below BMO side glyphs. Shares SCK(12) & MOSI(11); MISO=15, CS=13, 5V VCC.",
    groupTag: "microsd",
  });

  const steelCage = new THREE.Mesh(new THREE.BoxGeometry(1.50, 1.38, 0.22), new THREE.MeshStandardMaterial({ color: px(0xcbd3dc), metalness: 0.86, roughness: 0.24 }));
  steelCage.position.set(-3.12, -3.40, -0.02);
  const card = new THREE.Mesh(new THREE.BoxGeometry(0.42, 1.12, 0.12), new THREE.MeshStandardMaterial({ color: px(0x1f2429), roughness: 0.55 }));
  card.position.set(-3.95, -3.40, 0.0);
  const icMat = new THREE.MeshStandardMaterial({ color: px(0x1b1e23), roughness: 0.46 });
  const bufIc = new THREE.Mesh(new THREE.BoxGeometry(0.42, 0.56, 0.14), icMat);
  bufIc.position.set(-2.05, -2.05, -0.36);
  const ldoIc = new THREE.Mesh(new THREE.BoxGeometry(0.55, 0.40, 0.15), icMat);
  ldoIc.position.set(-2.05, -4.65, -0.36);
  g.add(steelCage, card, bufIc, ldoIc);

  const goldMat = new THREE.MeshStandardMaterial({ color: px(0xe0b548), metalness: 0.88, roughness: 0.22 });
  for (let i = 0; i < 6; i++) {
    const pin = new THREE.Mesh(new THREE.CylinderGeometry(0.032, 0.032, 0.34, 8), goldMat);
    pin.rotation.x = Math.PI / 2;
    pin.position.set(-1.56, -4.03 + i * 0.254, -0.14);
    g.add(pin);
  }
}

export function buildWireHarness(owner) {
  const THREE = getThree();
  const g = owner.layers.wires;
  owner.wireMeshes = [];
  for (const w of WIRE_DEFINITIONS) {
    const vectors = w.points.map((pt) => new THREE.Vector3(pt[0], pt[1], pt[2]));
    const curve = new THREE.CatmullRomCurve3(vectors, false, "catmullrom", 0.22);
    const mat = new THREE.MeshStandardMaterial({
      color: px(w.color), roughness: 0.45, metalness: 0.05,
      emissive: px(w.color), emissiveIntensity: 0.0, transparent: true, opacity: 1.0,
    });
    const mesh = new THREE.Mesh(new THREE.TubeGeometry(curve, 28, 0.032, 8, false), mat);
    mesh.castShadow = true;
    owner.registerInteractive(mesh, {
      isWire: true, wireId: w.id, wireGroup: w.group, name: w.label, role: `${w.awg} Hookup Wire`,
      detail: `${w.from[0]} (${w.from[1]})  -->  ${w.to[0]} (${w.to[1]})`, groupTag: w.group,
    });
    g.add(mesh);
    owner.wireMeshes.push(mesh);
  }
}

export function buildExpansionLayer(owner) {
  const THREE = getThree();
  const g = owner.layers.expansion;
  const ghostMat = (hex, opacity = 0.55) => new THREE.MeshStandardMaterial({ color: px(hex), transparent: true, opacity, roughness: 0.4, depthWrite: false });
  const xyCoords = [
    ["X (GPIO 14 - Simulated)", 1.80, -1.20, 0.68],
    ["Y (GPIO 42 - Simulated)", 0.90, -2.20, 0.68],
    ["L Shoulder (GPIO 47 - Disabled)", -2.70, 5.65, 0.68],
    ["R Shoulder (GPIO 48 - Disabled)", 2.70, 5.65, 0.68],
  ];
  for (const [name, x, y, z] of xyCoords) {
    const sw = new THREE.Mesh(new THREE.BoxGeometry(0.60, 0.60, 0.44), ghostMat(0xf59e0b, 0.72));
    sw.position.set(x, y, z);
    owner.registerInteractive(sw, { name, role: "Planned Expansion Switch (DISABLED)", detail: "Not yet wired in Stage-1 solder build.", groupTag: "expansion" });
    g.add(sw);
  }
  const maxAmp = new THREE.Mesh(new THREE.BoxGeometry(1.80, 1.90, 0.25), ghostMat(0xa855f7, 0.75));
  maxAmp.position.set(-2.20, 4.30, -0.25);
  owner.registerInteractive(maxAmp, { name: "MAX98357A I2S Audio Amp (PLANNED)", role: "FEATURE_AUDIO = 0 (DISABLED)", detail: "Planned GPIO 38(BCLK), 39(LRC), 40(DIN). Note GPIO 38 shares onboard RGB LED on v1.1.", groupTag: "expansion" });
  const speaker = new THREE.Mesh(new THREE.CylinderGeometry(1.35, 1.35, 0.50, 28), ghostMat(0xc084fc, 0.65));
  speaker.rotation.z = Math.PI / 2;
  speaker.position.set(-3.55, 4.30, 0.0);
  const fram = new THREE.Mesh(new THREE.BoxGeometry(1.60, 1.40, 0.20), ghostMat(0xf97316, 0.7));
  fram.position.set(2.40, 4.30, -0.25);
  owner.registerInteractive(fram, { name: "I2C FRAM Save Module (PLANNED)", role: "Unwired | Shares UART0 TX/RX (43/44)", detail: "Using GPIO 43/44 disables Serial logging unless remapped.", groupTag: "expansion" });
  const aaEnvelope = new THREE.Mesh(new THREE.BoxGeometry(5.40, 4.70, 1.60), ghostMat(0xef4444, 0.34));
  aaEnvelope.position.set(0.0, -3.00, -0.28);
  const wireFrame = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(5.40, 4.70, 1.60)), new THREE.LineBasicMaterial({ color: px(0xff4d6d) }));
  wireFrame.position.copy(aaEnvelope.position);
  owner.registerInteractive(aaEnvelope, {
    name: "3xAA NiMH Bay Envelope (54x47x16 mm) — CONFLICT",
    role: "FEATURE_BATTERY_MONITOR = 0 (CLASH WARNING)",
    detail: "16 mm cell pack collides (-9.3 mm Z) with stacked DevKitC-1 + perfboard. Place 2xAA cells in lower Zone C or use a deeper back shell.",
    groupTag: "expansion",
  });
  g.add(maxAmp, speaker, fram, aaEnvelope, wireFrame);
}
