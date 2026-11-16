/**
 * simulator/js/console_3d_model.js
 * Modular 3D enclosure shell, display recess, D-pad, action caps, pills, and shoulders.
 */

import { BmoInternals3D } from "../bmo_internals_3d.js";
import { DEFAULT_HARDWARE_LAYOUT, resolveLayoutDimensions } from "./hardware_layout_data.js";

function getThree() {
  return /** @type {any} */ (globalThis).THREE;
}

const px = (hex) => new (getThree()).Color(hex).convertSRGBToLinear();

function traceRoundedRect(shape, width, height, radius, clockwise = false, centerX = 0, centerY = 0) {
  const x = width / 2;
  const y = height / 2;
  const r = Math.min(radius, x, y);
  const corners = clockwise
    ? [
        [-x + r, y, x - r, y], [x, y, x, y - r],
        [x, -y + r, x, -y], [x - r, -y, -x + r, -y],
        [-x, -y, -x, -y + r], [-x, y - r, -x, y],
      ]
    : [
        [-x + r, -y, x - r, -y], [x, -y, x, -y + r],
        [x, y - r, x, y], [x - r, y, -x + r, y],
        [-x, y, -x, y - r], [-x, -y + r, -x, -y],
      ];
  shape.moveTo(centerX + corners[0][0], centerY + corners[0][1]);
  shape.lineTo(centerX + corners[0][2], centerY + corners[0][3]);
  shape.quadraticCurveTo(centerX + corners[1][0], centerY + corners[1][1], centerX + corners[1][2], centerY + corners[1][3]);
  shape.lineTo(centerX + corners[2][0], centerY + corners[2][1]);
  shape.quadraticCurveTo(centerX + corners[2][2], centerY + corners[2][3], centerX + corners[3][0], centerY + corners[3][1]);
  shape.lineTo(centerX + corners[3][2], centerY + corners[3][3]);
  shape.quadraticCurveTo(centerX + corners[4][0], centerY + corners[4][1], centerX + corners[4][2], centerY + corners[4][3]);
  shape.lineTo(centerX + corners[5][0], centerY + corners[5][1]);
  shape.quadraticCurveTo(centerX + corners[5][2], centerY + corners[5][3], centerX + corners[0][0], centerY + corners[0][1]);
  shape.closePath();
  return shape;
}

function roundedRect(width, height, radius) {
  const THREE = getThree();
  return traceRoundedRect(new THREE.Shape(), width, height, radius);
}

function extrudedRounded(width, height, radius, depth, bevel = 0) {
  const THREE = getThree();
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
}

function ringGeometry(outerW, outerH, innerW, innerH, radius) {
  const THREE = getThree();
  const shape = roundedRect(outerW, outerH, radius);
  const hole = traceRoundedRect(new THREE.Path(), innerW, innerH, Math.min(radius * 0.6, innerW / 2, innerH / 2), true);
  shape.holes.push(hole);
  const geometry = new THREE.ExtrudeGeometry(shape, { depth: 0.02, bevelEnabled: false, curveSegments: 12 });
  geometry.center();
  return geometry;
}

function addFrontText(text, width, height, fontSize, color) {
  const THREE = getThree();
  const canvas = document.createElement("canvas");
  canvas.width = 256;
  canvas.height = 96;
  const ctx = canvas.getContext("2d");
  if (!ctx) return new THREE.Object3D();
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
  return new THREE.Mesh(
    new THREE.PlaneGeometry(width, height),
    new THREE.MeshBasicMaterial({ map: texture, transparent: true, depthWrite: false, side: THREE.DoubleSide })
  );
}

function initModelMaterialsAndGroups(owner, dims) {
  const THREE = getThree();
  owner.rootGroup = new THREE.Group();
  owner.scene.add(owner.rootGroup);
  owner.frontShellGroup = new THREE.Group();
  owner.frontShellGroup.name = "BMO_Front_Shell";
  owner.buttonsGroup = new THREE.Group();
  owner.buttonsGroup.name = "BMO_Front_Buttons";
  owner.backShellGroup = new THREE.Group();
  owner.backShellGroup.name = "BMO_Back_Shell";
  owner.batteryDoorGroup = new THREE.Group();
  owner.batteryDoorGroup.name = "BMO_Battery_Door";
  owner.rootGroup.add(owner.frontShellGroup, owner.buttonsGroup, owner.backShellGroup, owner.batteryDoorGroup);

  const { shellWidthCm: W, shellHeightCm: H, shellDepthCm: D } = dims;
  owner.modelDimensions = { width: W, height: H, depth: D, units: "cm" };
  owner.materials.chassis = new THREE.MeshPhysicalMaterial({ color: px(0x63c5b8), roughness: 0.58, metalness: 0, clearcoat: 0.08, clearcoatRoughness: 0.52 });
  owner.materials.backShell = new THREE.MeshPhysicalMaterial({ color: px(0x5ab5a9), roughness: 0.62, metalness: 0, clearcoat: 0.06, clearcoatRoughness: 0.56 });
  owner.materials.batteryDoor = new THREE.MeshPhysicalMaterial({ color: px(0x56aea3), roughness: 0.64, metalness: 0 });
  owner.materials.glyphInk = new THREE.MeshStandardMaterial({ color: px(0x101e26), roughness: 0.72, metalness: 0.04 });
  owner.materials.bezel = new THREE.MeshStandardMaterial({ color: px(0x17353a), roughness: 0.52 });
  owner.materials.dpad = new THREE.MeshPhysicalMaterial({ color: px(0xf4d03f), roughness: 0.48, clearcoat: 0.08 });
  owner.materials.btnPill = new THREE.MeshPhysicalMaterial({ color: px(0x264b42), roughness: 0.5, clearcoat: 0.05 });
  owner.materials.shoulder = new THREE.MeshPhysicalMaterial({ color: px(0x264b42), roughness: 0.48, clearcoat: 0.08 });
  owner.materials.slot = new THREE.MeshStandardMaterial({ color: px(0x0f171a), roughness: 0.72 });
}

function buildShellHalvesAndFelt(owner, dims, disp) {
  const THREE = getThree();
  const { shellWidthCm: W, shellHeightCm: H, shellDepthCm: D, shellSideWallCm } = dims;
  const frontZ = D / 2;
  const backZ = -frontZ;

  const frontCadGeo = owner.decodeCadMesh("bmo_front_shell");
  const backCadGeo = owner.decodeCadMesh("bmo_back_shell");
  const doorCadGeo = owner.decodeCadMesh("bmo_battery_door");
  const shellShape = roundedRect(W - 0.2, H - 0.2, 0.82);
  const frontShape = roundedRect(W - 0.2, H - 0.2, 0.82);
  frontShape.holes.push(traceRoundedRect(new THREE.Path(), disp.w, disp.h, 0.12, true, disp.x, disp.y));

  const makeFallbackHalf = (z, material, shape = shellShape) => {
    const geometry = new THREE.ExtrudeGeometry(shape, {
      depth: D / 2 - 0.2, bevelEnabled: true, bevelSegments: 6, steps: 1,
      bevelSize: 0.1, bevelThickness: 0.1, curveSegments: 16,
    });
    geometry.center();
    const mesh = new THREE.Mesh(geometry, material);
    mesh.position.z = z;
    return mesh;
  };

  owner.chassisMesh = frontCadGeo
    ? new THREE.Mesh(frontCadGeo, [owner.materials.chassis, owner.materials.glyphInk])
    : makeFallbackHalf(frontZ / 2, owner.materials.chassis, frontShape);
  owner.chassisMesh.castShadow = true;
  owner.chassisMesh.receiveShadow = true;
  owner.frontShellGroup.add(owner.chassisMesh);

  owner.backShellMesh = backCadGeo
    ? new THREE.Mesh(backCadGeo, [owner.materials.backShell, owner.materials.glyphInk])
    : makeFallbackHalf(backZ / 2, owner.materials.backShell);
  owner.backShellMesh.castShadow = true;
  owner.backShellMesh.receiveShadow = true;
  owner.backShellGroup.add(owner.backShellMesh);

  if (doorCadGeo) {
    owner.batteryDoorMesh = new THREE.Mesh(doorCadGeo, owner.materials.batteryDoor);
    owner.batteryDoorMesh.castShadow = true;
    owner.batteryDoorMesh.receiveShadow = true;
    owner.batteryDoorGroup.add(owner.batteryDoorMesh);
  }

  const feltMat = new THREE.MeshStandardMaterial({ color: px(0x0d1518), roughness: 0.92, side: THREE.DoubleSide });
  for (const side of [-1, 1]) {
    const felt = new THREE.Mesh(new THREE.BoxGeometry(0.05, 1.35, 1.35), feltMat);
    felt.position.set(side * (W / 2 - shellSideWallCm - 0.05), 4.30, 0);
    owner.rootGroup.add(felt);
    owner.acousticFeltMeshes = owner.acousticFeltMeshes || [];
    owner.acousticFeltMeshes.push(felt);
  }
}

function buildDisplayAndDecor(owner, frontZ, disp) {
  const THREE = getThree();
  const glassBase = new THREE.Mesh(
    new THREE.PlaneGeometry(disp.w + 0.20, disp.h + 0.20),
    new THREE.MeshPhysicalMaterial({
      color: px(0x07080a), roughness: 0.12, metalness: 0, clearcoat: 0.9, clearcoatRoughness: 0.08,
      polygonOffset: true, polygonOffsetFactor: -1, polygonOffsetUnits: -1,
    })
  );
  glassBase.position.set(disp.x, disp.y, frontZ - 0.015);
  const bezelRing = new THREE.Mesh(ringGeometry(5.48, 4.22, 5.20, 3.95, 0.24), owner.materials.bezel);
  bezelRing.position.set(disp.x, disp.y, frontZ + 0.004);
  owner.frontShellGroup.add(glassBase, bezelRing);

  if (owner.tftCanvas) {
    owner.screenTexture = new THREE.CanvasTexture(owner.tftCanvas);
    owner.screenTexture.encoding = THREE.sRGBEncoding;
    owner.screenTexture.minFilter = THREE.NearestFilter;
    owner.screenTexture.magFilter = THREE.NearestFilter;
    owner.screenTexture.generateMipmaps = false;
  }
  const screenMaterial = new THREE.MeshPhysicalMaterial({
    color: 0xffffff, map: owner.screenTexture, roughness: 0.12, metalness: 0, clearcoat: 0.85,
    clearcoatRoughness: 0.08, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2,
  });
  owner.screenMesh = new THREE.Mesh(new THREE.PlaneGeometry(disp.w, disp.h), screenMaterial);
  owner.screenMesh.position.set(disp.x, disp.y, frontZ + 0.008);
  const slot = new THREE.Mesh(new THREE.BoxGeometry(4.2, 0.2, 0.04), owner.materials.slot);
  slot.position.set(-0.02, 0.42, frontZ + 0.025);
  const eject = new THREE.Mesh(
    new THREE.CylinderGeometry(0.15, 0.15, 0.12, 40),
    new THREE.MeshPhysicalMaterial({ color: px(0x2874a6), roughness: 0.38, clearcoat: 0.25 })
  );
  eject.rotation.x = Math.PI / 2;
  eject.position.set(2.45, 0.42, frontZ + 0.07);
  owner.frontShellGroup.add(owner.screenMesh, slot, eject);
}

function buildDpadAssembly(owner, frontZ) {
  const THREE = getThree();
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
    depth: 0.2, bevelEnabled: true, bevelSegments: 4, bevelSize: 0.04, bevelThickness: 0.035, curveSegments: 12,
  });
  dpadGeo.center();
  owner.dpadPivotGroup = new THREE.Group();
  owner.dpadPivotGroup.position.set(-1.9, -2.2, frontZ + 0.12);
  owner.dpadRestZ = frontZ + 0.12;
  owner.buttonsGroup.add(owner.dpadPivotGroup);
  owner.dpadBodyMesh = new THREE.Mesh(dpadGeo, owner.materials.dpad);
  owner.dpadBodyMesh.castShadow = true;
  owner.dpadBodyMesh.receiveShadow = true;
  const dpadFace = new THREE.Mesh(
    new THREE.PlaneGeometry(2.8, 2.8),
    new THREE.MeshBasicMaterial({ map: owner.createDpadTexture(), transparent: true, depthWrite: false })
  );
  dpadFace.position.z = 0.145;
  owner.dpadPivotGroup.add(owner.dpadBodyMesh, dpadFace);

  const dpadZone = (key, x, y, width, height) => {
    const zone = new THREE.Mesh(
      new THREE.PlaneGeometry(width, height),
      new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false })
    );
    zone.position.set(x, y, 0.16);
    zone.userData = { isButton: true, key, isDpad: true };
    owner.dpadPivotGroup.add(zone);
    owner.buttonMeshes[key] = zone;
    owner.interactiveObjects.push(zone);
  };
  dpadZone("UP", 0, 0.85, armWidth, 1.1);
  dpadZone("DOWN", 0, -0.85, armWidth, 1.1);
  dpadZone("LEFT", -0.85, 0, 1.1, armWidth);
  dpadZone("RIGHT", 0.85, 0, 1.1, armWidth);
}

function buildActionPillsAndShoulders(owner, frontZ, hh) {
  const THREE = getThree();
  const capRadius = 0.45;
  const capHeight = 0.22;
  const actionButtons = [
    ["X", 1.8, -1.2, "#4caf50", "#10251f"],
    ["Y", 0.9, -2.2, "#f48fb1", "#10251f"],
    ["A", 2.7, -2.2, "#52a6ba", "#10251f"],
    ["B", 1.8, -3.2, "#ff91a5", "#10251f"],
  ];
  for (const [key, x, y, color, labelColor] of actionButtons) {
    const group = new THREE.Group();
    group.position.set(x, y, frontZ + capHeight / 2);
    owner.buttonsGroup.add(group);
    const capShape = new THREE.LatheGeometry([
      new THREE.Vector2(0, -capHeight / 2), new THREE.Vector2(capRadius * 0.88, -capHeight / 2),
      new THREE.Vector2(capRadius * 0.98, -capHeight / 2 + 0.035), new THREE.Vector2(capRadius, -capHeight / 2 + 0.08),
      new THREE.Vector2(capRadius, capHeight / 2 - 0.055), new THREE.Vector2(capRadius * 0.91, capHeight / 2),
      new THREE.Vector2(0, capHeight / 2),
    ], 48);
    capShape.rotateX(Math.PI / 2);
    const cap = new THREE.Mesh(capShape, new THREE.MeshPhysicalMaterial({ color: px(color), roughness: 0.48, metalness: 0, clearcoat: 0.12, clearcoatRoughness: 0.38 }));
    cap.castShadow = true;
    const face = new THREE.Mesh(new THREE.CircleGeometry(capRadius - 0.015, 48), new THREE.MeshStandardMaterial({ map: owner.createButtonTexture(key, color, labelColor), roughness: 0.55, metalness: 0 }));
    face.position.z = capHeight / 2 + 0.002;
    const hit = new THREE.Mesh(new THREE.CylinderGeometry(capRadius + 0.03, capRadius + 0.03, 0.32, 32), new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false }));
    hit.rotation.x = Math.PI / 2;
    group.add(cap, face, hit);
    const data = { isButton: true, key, name: `${key} Button`, isAction: true };
    cap.userData = data;
    face.userData = data;
    hit.userData = data;
    owner.buttonMeshes[key] = group;
    owner.interactiveObjects.push(cap, face, hit);
    owner.buttonOriginalPositions.set(group, { x: group.position.x, y: group.position.y, z: group.position.z, rotX: 0, rotY: 0, rotZ: 0 });
  }

  for (const [key, x] of [["SELECT", -2.55], ["START", -1.15]]) {
    const y = -4.35;
    const group = new THREE.Group();
    group.position.set(x, y, frontZ + 0.1);
    owner.buttonsGroup.add(group);
    const pill = new THREE.Mesh(extrudedRounded(1.15, 0.40, 0.20, 0.16, 0.035), owner.materials.btnPill);
    pill.castShadow = true;
    pill.userData = { isButton: true, key, name: key, isPill: true };
    group.add(pill);
    owner.buttonMeshes[key] = group;
    owner.interactiveObjects.push(pill);
    owner.buttonOriginalPositions.set(group, { x: group.position.x, y: group.position.y, z: group.position.z, rotX: 0, rotY: 0, rotZ: 0 });
    const legend = addFrontText(key, 1.3, 0.28, 27, "#173b35");
    legend.position.set(x, y - 0.48, frontZ + 0.018);
    owner.frontShellGroup.add(legend);
  }

  const shoulderDepth = 0.68;
  for (const [key, x] of [["L", -2.7], ["R", 2.7]]) {
    const geometry = new THREE.ExtrudeGeometry(roundedRect(1.8, 0.52, 0.14), {
      depth: shoulderDepth, bevelEnabled: true, bevelSegments: 4, bevelSize: 0.04, bevelThickness: 0.035, curveSegments: 12,
    });
    geometry.center();
    const shoulder = new THREE.Mesh(geometry, owner.materials.shoulder);
    shoulder.position.set(x, hh + 0.20, 0.76);
    shoulder.castShadow = true;
    shoulder.userData = { isButton: true, key, name: `${key} Shoulder`, isShoulder: true };
    owner.buttonsGroup.add(shoulder);
    const legend = addFrontText(key, 0.9, 0.34, 36, "#d7f2e9");
    legend.position.set(0, 0.05, shoulderDepth / 2 + 0.05);
    const hit = new THREE.Mesh(new THREE.BoxGeometry(2.0, 0.78, 0.9), new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.0001, depthWrite: false }));
    hit.userData = shoulder.userData;
    shoulder.add(legend, hit);
    owner.buttonMeshes[key] = shoulder;
    owner.interactiveObjects.push(shoulder, hit);
    owner.buttonOriginalPositions.set(shoulder, { x: shoulder.position.x, y: shoulder.position.y, z: shoulder.position.z, rotX: 0, rotY: 0, rotZ: 0 });
  }
}

export function buildConsoleModelImpl(owner) {
  const dims = owner.layoutDims || resolveLayoutDimensions(DEFAULT_HARDWARE_LAYOUT);
  const disp = { x: 0, y: 2.9, w: 5.0, h: 3.75 };
  const frontZ = dims.shellDepthCm / 2;
  initModelMaterialsAndGroups(owner, dims);
  buildShellHalvesAndFelt(owner, dims, disp);
  buildDisplayAndDecor(owner, frontZ, disp);
  buildDpadAssembly(owner, frontZ);
  buildActionPillsAndShoulders(owner, frontZ, dims.shellHeightCm / 2);
  owner.internals = new BmoInternals3D(owner.rootGroup, owner.layout);
}
