/**
 * simulator/js/internals_3d_board.js
 * Fasteners, ST7789 display module, tactile switch plane, and perfboard mesh builders.
 */

import { DEFAULT_LAYOUT_DIMS, createSilkscreenTexture, getThree, px } from "./internals_3d_data.js";

function createM2ScrewGroup(THREE, steelMat, darkSlotMat, spec) {
  const { x, y, assembledZ, threadLen } = spec;
  const g = new THREE.Group();
  g.position.set(x, y, assembledZ);
  const head = new THREE.Mesh(new THREE.CylinderGeometry(0.16, 0.16, 0.10, 20), steelMat);
  head.rotation.x = Math.PI / 2;
  g.add(head);

  const slotA = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.035, 0.02), darkSlotMat);
  slotA.position.z = -0.048;
  const slotB = new THREE.Mesh(new THREE.BoxGeometry(0.035, 0.18, 0.02), darkSlotMat);
  slotB.position.z = -0.048;
  g.add(slotA, slotB);

  const shaft = new THREE.Mesh(new THREE.CylinderGeometry(0.085, 0.085, threadLen, 16), steelMat);
  shaft.rotation.x = Math.PI / 2;
  shaft.position.z = threadLen / 2 + 0.05;
  g.add(shaft);
  return { group: g, head };
}

export function buildFasteners(owner) {
  const THREE = getThree();
  const steelMat = new THREE.MeshStandardMaterial({ color: px(0xc2c9d1), metalness: 0.88, roughness: 0.24 });
  const darkSlotMat = new THREE.MeshStandardMaterial({ color: px(0x1b2126), roughness: 0.8 });
  const addScrew = (x, y, assembledZ, threadLen, kind, label) => {
    const { group, head } = createM2ScrewGroup(THREE, steelMat, darkSlotMat, { x, y, assembledZ, threadLen });
    owner.registerInteractive(head, { name: label, role: "M2 Machine Screw", detail: kind, groupTag: "screws" });
    owner.layers.fastenersFront.add(group);
    owner.screwItems.push({ group, baseZ: assembledZ, kind });
  };
  for (const x of [-3.3, 3.3]) {
    for (const y of [-5.4, 5.4]) {
      addScrew(x, y, -1.28, 0.78, "case", `M2x8 Case Screw (${x > 0 ? "R" : "L"} ${y > 0 ? "Top" : "Btm"})`);
    }
  }
  for (const x of [-2.2, 2.2]) {
    for (const y of [-5.75, -0.25]) {
      addScrew(x, y, -1.29, 0.48, "door", `M2x5 Door Screw (${x > 0 ? "R" : "L"} ${y > -3 ? "Top" : "Btm"})`);
    }
  }
}

function createDisplayCarrierTexture() {
  return createSilkscreenTexture(512, 340, (ctx, w, h) => {
    ctx.fillStyle = "#b51d24";
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "#cf363c";
    ctx.lineWidth = 3;
    for (let i = 0; i < 8; i++) {
      ctx.beginPath();
      ctx.moveTo(196 + i * 17, h - 18);
      ctx.lineTo(196 + i * 17, h - 55);
      ctx.lineTo(w / 2 + (i - 3.5) * 14, h * 0.55);
      ctx.stroke();
    }
    const labels = ["GND", "VCC", "SCK", "MOSI", "RES", "DC", "CS", "BLK"];
    ctx.fillStyle = "#f7f9fa";
    ctx.font = "bold 12px monospace";
    ctx.textAlign = "center";
    labels.forEach((lbl, idx) => ctx.fillText(lbl, 188 + idx * 19.5, h - 30));
    ctx.font = "bold 16px monospace";
    ctx.fillText("2.4\" ST7789 SPI TFT 320x240", w / 2, 34);
    ctx.font = "13px monospace";
    ctx.fillStyle = "#ffe5e5";
    ctx.fillText("VCC = 3.3V ONLY | FSPI 80 MHz", w / 2, 56);
  });
}

export function buildDisplayModule(owner) {
  const THREE = getThree();
  const g = owner.layers.display;
  const pcb = new THREE.Mesh(
    new THREE.BoxGeometry(6.70, 4.30, 0.16),
    new THREE.MeshStandardMaterial({ map: createDisplayCarrierTexture(), roughness: 0.42, metalness: 0.08 })
  );
  pcb.position.set(0, 2.90, 0.68);
  pcb.castShadow = true;
  pcb.receiveShadow = true;
  g.add(pcb);
  owner.registerInteractive(pcb, {
    name: "2.4\" ST7789 SPI TFT Display",
    role: "Display Carrier (67 x 43 mm)",
    detail: "GPIO 8(DC), 9(RST), 10(CS), 11(MOSI), 12(SCK), 3V3, GND. Runs 80 MHz SPI.",
    groupTag: "display",
  });

  const steelShield = new THREE.Mesh(
    new THREE.BoxGeometry(6.02, 4.22, 0.22),
    new THREE.MeshStandardMaterial({ color: px(0xd5dbe0), metalness: 0.82, roughness: 0.28 })
  );
  steelShield.position.set(0, 2.90, 0.86);
  const lcdGlass = new THREE.Mesh(
    new THREE.BoxGeometry(5.68, 3.98, 0.05),
    new THREE.MeshPhysicalMaterial({ color: px(0x0b0f14), roughness: 0.12, metalness: 0.05, clearcoat: 0.9, clearcoatRoughness: 0.08 })
  );
  lcdGlass.position.set(0, 2.90, 0.985);
  const lcdActiveArea = new THREE.Mesh(
    new THREE.PlaneGeometry(5.00, 3.75),
    new THREE.MeshStandardMaterial({ color: px(0x16242a), roughness: 0.35, metalness: 0.1 })
  );
  lcdActiveArea.position.set(0, 2.90, 1.015);
  const fpc = new THREE.Mesh(
    new THREE.BoxGeometry(2.6, 0.65, 0.05),
    new THREE.MeshStandardMaterial({ color: px(0xc87018), roughness: 0.35, metalness: 0.2 })
  );
  fpc.position.set(0, 1.02, 0.80);
  const ldo = new THREE.Mesh(
    new THREE.BoxGeometry(0.45, 0.30, 0.14),
    new THREE.MeshStandardMaterial({ color: px(0x1a1e24), roughness: 0.48 })
  );
  ldo.position.set(-2.2, 1.85, 0.56);
  const plasticBase = new THREE.Mesh(
    new THREE.BoxGeometry(2.08, 0.26, 0.25),
    new THREE.MeshStandardMaterial({ color: px(0x14171a), roughness: 0.72 })
  );
  plasticBase.position.set(0, 0.86, 0.50);
  g.add(steelShield, lcdGlass, lcdActiveArea, fpc, ldo, plasticBase);

  const pinGeo = new THREE.CylinderGeometry(0.032, 0.032, 0.52, 8);
  pinGeo.rotateX(Math.PI / 2);
  const headerPins = new THREE.InstancedMesh(
    pinGeo,
    new THREE.MeshStandardMaterial({ color: px(0xe5b949), metalness: 0.90, roughness: 0.22 }),
    8
  );
  const dummy = new THREE.Object3D();
  for (let i = 0; i < 8; i++) {
    dummy.position.set(-0.889 + i * 0.254, 0.86, 0.44);
    dummy.updateMatrix();
    headerPins.setMatrixAt(i, dummy.matrix);
  }
  g.add(headerPins);
}

export function buildSwitchPlane(owner) {
  const THREE = getThree();
  const g = owner.layers.switches;
  const carrierMat = new THREE.MeshStandardMaterial({ color: px(0x145238), roughness: 0.55 });
  const carriers = [
    [2.60, 2.60, -1.90, -2.20],
    [1.90, 2.00, 2.10, -2.70],
    [2.40, 0.85, -1.85, -4.35],
  ];
  for (const [w, h, x, y] of carriers) {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, 0.08), carrierMat);
    mesh.position.set(x, y, 0.52);
    g.add(mesh);
  }
  const switchCoords = [
    { key: "UP", gpio: "GPIO 4", pos: [-1.90, -1.35] },
    { key: "DOWN", gpio: "GPIO 5", pos: [-1.90, -3.05] },
    { key: "LEFT", gpio: "GPIO 6", pos: [-2.75, -2.20] },
    { key: "RIGHT", gpio: "GPIO 7", pos: [-1.05, -2.20] },
    { key: "A", gpio: "GPIO 16", pos: [2.70, -2.20] },
    { key: "B", gpio: "GPIO 17", pos: [1.80, -3.20] },
    { key: "START", gpio: "GPIO 18", pos: [-1.15, -4.35] },
    { key: "SELECT", gpio: "GPIO 21", pos: [-2.55, -4.35] },
  ];
  const bodyMat = new THREE.MeshStandardMaterial({ color: px(0x1f2428), roughness: 0.52 });
  const capMat = new THREE.MeshStandardMaterial({ color: px(0xced4da), metalness: 0.72, roughness: 0.28 });
  const stemMat = new THREE.MeshStandardMaterial({ color: px(0x121518), roughness: 0.45 });
  const legMat = new THREE.MeshStandardMaterial({ color: px(0xd0d7de), metalness: 0.85, roughness: 0.25 });

  for (const sw of switchCoords) {
    const swGroup = new THREE.Group();
    swGroup.position.set(sw.pos[0], sw.pos[1], 0.68);
    const body = new THREE.Mesh(new THREE.BoxGeometry(0.60, 0.60, 0.26), bodyMat);
    const plate = new THREE.Mesh(new THREE.BoxGeometry(0.56, 0.56, 0.04), capMat);
    plate.position.z = 0.14;
    const plunger = new THREE.Mesh(new THREE.CylinderGeometry(0.16, 0.16, 0.16, 18), stemMat);
    plunger.rotation.x = Math.PI / 2;
    plunger.position.z = 0.24;
    for (const lx of [-0.32, 0.32]) {
      for (const ly of [-0.20, 0.20]) {
        const leg = new THREE.Mesh(new THREE.BoxGeometry(0.10, 0.06, 0.18), legMat);
        leg.position.set(lx, ly, -0.05);
        swGroup.add(leg);
      }
    }
    swGroup.add(body, plate, plunger);
    owner.registerInteractive(body, {
      name: `Tactile Switch: ${sw.key}`,
      role: "Omron B3F 6x6x5 mm",
      detail: `Wired ${sw.gpio} <-> GND (internal INPUT_PULLUP, active-LOW).`,
      groupTag: "buttons",
    });
    g.add(swGroup);
  }
}

export function buildPerfboardGrid(owner) {
  const THREE = getThree();
  const g = owner.layers.perfboard;
  const dims = owner.layoutDims || DEFAULT_LAYOUT_DIMS;
  const { perfboardCols: cols, perfboardRows: rows, perfboardPitchCm: pitch } = dims;
  const { perfboardWidthCm: boardW, perfboardHeightCm: boardH, perfboardThicknessCm: boardT } = dims;
  const boardTex = createSilkscreenTexture(512, 760, (ctx, w, h) => {
    ctx.fillStyle = "#166842";
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "#e6efe9";
    ctx.lineWidth = 3;
    ctx.strokeRect(10, 10, w - 20, h - 20);
    ctx.fillStyle = "#e6efe9";
    ctx.font = "bold 16px monospace";
    ctx.textAlign = "center";
    ctx.fillText(`${cols}x${rows} DOUBLE-SIDED PERFBOARD`, w / 2, 34);
    ctx.font = "13px monospace";
    ctx.fillText(`${(boardW * 10).toFixed(2)} x ${(boardH * 10).toFixed(2)} mm (${(pitch * 10).toFixed(2)} mm PITCH)`, w / 2, h - 18);
  });

  const board = new THREE.Mesh(
    new THREE.BoxGeometry(boardW, boardH, boardT),
    new THREE.MeshStandardMaterial({ map: boardTex, roughness: 0.48, metalness: 0.06 })
  );
  board.position.set(0, 0, 0.25);
  board.castShadow = true;
  board.receiveShadow = true;
  g.add(board);
  owner.registerInteractive(board, {
    name: `${cols}x${rows} Prototype Perfboard`,
    role: `Main Solder Backplane (${(boardW * 10).toFixed(2)} x ${(boardH * 10).toFixed(2)} mm)`,
    detail: `${dims.totalVias} plated ${(pitch * 10).toFixed(2)} mm vias (${cols}x${rows} grid). M2 mounting holes at columns 2/30, rows 2/45.`,
    groupTag: "perfboard",
  });

  const padGeo = new THREE.CylinderGeometry(0.082, 0.082, boardT + 0.008, 10);
  padGeo.rotateX(Math.PI / 2);
  const holeGeo = new THREE.CylinderGeometry(0.04, 0.04, boardT + 0.012, 8);
  holeGeo.rotateX(Math.PI / 2);
  const pads = new THREE.InstancedMesh(padGeo, new THREE.MeshStandardMaterial({ color: px(0xd9ae48), metalness: 0.85, roughness: 0.25 }), cols * rows);
  const holes = new THREE.InstancedMesh(holeGeo, new THREE.MeshStandardMaterial({ color: px(0x0b1411), roughness: 0.9 }), cols * rows);
  const dummy = new THREE.Object3D();
  let idx = 0;
  for (let c = 0; c < cols; c++) {
    for (let r = 0; r < rows; r++) {
      dummy.position.set((c - (cols - 1) / 2) * pitch, (r - (rows - 1) / 2) * pitch, 0.25);
      dummy.updateMatrix();
      pads.setMatrixAt(idx, dummy.matrix);
      holes.setMatrixAt(idx, dummy.matrix);
      idx++;
    }
  }
  g.add(pads, holes);
}
