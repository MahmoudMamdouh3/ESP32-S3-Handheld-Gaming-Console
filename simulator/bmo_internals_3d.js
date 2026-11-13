/**
 * Internal electronics stack, fasteners, 3D wire harnesses, and planned expansion
 * assemblies for the BMO Handheld interactive teardown view.
 *
 * All dimensions are in centimetres (1 cm = 10 mm) matching Bmo3DConsole and the
 * shell CAD in hardware/enclosure/generate_bmo_enclosure.py. Pin assignments match
 * firmware/BmoGameboy/src/hal/config.h.
 */

(function (globalScope) {
  'use strict';

  const px = (hex) => new THREE.Color(hex).convertSRGBToLinear();

  // Official Espressif ESP32-S3-DevKitC-1 v1.1 header pinout (top-to-bottom, antenna +Y, USB -Y).
  // J1 is on local -X when viewing the WROOM-1 component side; because the board mounts on the
  // rear (-Z) side of the perfboard facing -Z (rotated Y = PI), J1 sits at world +X = +1.143 cm
  // and J3 sits at world -X = -1.143 cm.
  const DEVKIT_J1_PINS = [
    "3V3", "3V3", "RST", "4", "5", "6", "7", "15", "16", "17", "18",
    "8", "3", "46", "9", "10", "11", "12", "13", "14", "5V", "GND",
  ];
  const DEVKIT_J3_PINS = [
    "GND", "43/TX", "44/RX", "1", "2", "42", "41", "40", "39", "38", "37",
    "36", "35", "0", "45", "48", "47", "21", "20", "19", "GND", "GND",
  ];

  // Physical coordinates of the DevKitC-1 PCB inside the assembled enclosure.
  const DEVKIT_X = 0.0;
  const DEVKIT_Y = -2.813;
  const DEVKIT_PCB_Z = -0.02;
  const DEVKIT_PIN_PITCH = 0.254;
  const DEVKIT_FIRST_PIN_OFFSET_Y = 2.667; // 10.5 * 0.254 cm from DevKit center
  const DEVKIT_ROW_HALF_X = 1.143;         // 9 * 0.254 / 2 cm

  function getDevkitPinWorldPos(headerSide, pinName, explodeZ = 0) {
    const list = headerSide === "J1" ? DEVKIT_J1_PINS : DEVKIT_J3_PINS;
    const idx = list.indexOf(pinName);
    const clampedIdx = idx >= 0 ? idx : 0;
    // With board rotated Y = Math.PI (facing -Z), local -X (J1) maps to world +X.
    const x = headerSide === "J1" ? DEVKIT_X + DEVKIT_ROW_HALF_X : DEVKIT_X - DEVKIT_ROW_HALF_X;
    const y = DEVKIT_Y + DEVKIT_FIRST_PIN_OFFSET_Y - clampedIdx * DEVKIT_PIN_PITCH;
    return [x, y, DEVKIT_PCB_Z + explodeZ];
  }

  // Color-coded harness definitions synchronized with firmware config.h.
  const WIRE_DEFINITIONS = [
    // Power & Common Ground
    { id: "wire_3v3_tft", group: "power", color: 0xe63946, label: "3V3 -> ST7789 VCC & BLK",
      from: ["MCU J1 #1", "3V3"], to: ["TFT J1 #2/#8", "VCC + BLK"], awg: "26 AWG",
      points: [
        getDevkitPinWorldPos("J1", "3V3", 0.22),
        [1.143, 0.35, 0.36],
        [0.45, 0.72, 0.44],
        [-0.635, 0.86, 0.50],
      ] },
    { id: "wire_gnd_tft", group: "power", color: 0x1e252b, label: "GND -> ST7789 GND",
      from: ["MCU J3 #1", "GND"], to: ["TFT J1 #1", "GND"], awg: "26 AWG",
      points: [
        getDevkitPinWorldPos("J3", "GND", 0.22),
        [-1.143, 0.20, 0.36],
        [-0.95, 0.68, 0.44],
        [-0.889, 0.86, 0.50],
      ] },
    { id: "wire_5v_sd", group: "power", color: 0xf4a261, label: "5V -> MicroSD VCC (onboard LDO)",
      from: ["MCU J1 #21", "5V"], to: ["SD J1 #2", "VCC"], awg: "26 AWG",
      points: [
        getDevkitPinWorldPos("J1", "5V", 0.16),
        [0.55, -5.60, 0.28],
        [-1.05, -5.58, 0.28],
        [-1.56, -4.40, -0.02],
        [-1.56, -3.78, -0.10],
      ] },
    { id: "wire_gnd_sd", group: "power", color: 0x1e252b, label: "GND -> MicroSD GND",
      from: ["MCU J3 #21", "GND"], to: ["SD J1 #1", "GND"], awg: "26 AWG",
      points: [
        getDevkitPinWorldPos("J3", "GND", 0.16),
        [-1.35, -5.20, 0.24],
        [-1.56, -4.55, 0.08],
        [-1.56, -4.03, -0.10],
      ] },

    // Shared FSPI Display (ST7789, 80 MHz)
    { id: "wire_tft_sck", group: "display", color: 0xf4d03f, label: "GPIO 12 -> ST7789 SCK (FSPI CLK)",
      from: ["MCU J1 #18", "GPIO 12"], to: ["TFT J1 #3", "SCK"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "12", 0.22),
        [1.48, -4.46, 0.34],
        [1.48, 0.55, 0.42],
        [-0.381, 0.72, 0.48],
        [-0.381, 0.86, 0.50],
      ] },
    { id: "wire_tft_mosi", group: "display", color: 0x2ec4b6, label: "GPIO 11 -> ST7789 MOSI (FSPI D)",
      from: ["MCU J1 #17", "GPIO 11"], to: ["TFT J1 #4", "MOSI"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "11", 0.22),
        [1.62, -4.21, 0.35],
        [1.62, 0.48, 0.43],
        [-0.127, 0.68, 0.48],
        [-0.127, 0.86, 0.50],
      ] },
    { id: "wire_tft_rst", group: "display", color: 0x4cc9f0, label: "GPIO 9 -> ST7789 RES",
      from: ["MCU J1 #15", "GPIO 9"], to: ["TFT J1 #5", "RES"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "9", 0.22),
        [1.76, -3.70, 0.36],
        [1.76, 0.40, 0.44],
        [0.127, 0.64, 0.49],
        [0.127, 0.86, 0.50],
      ] },
    { id: "wire_tft_dc", group: "display", color: 0x9b5de5, label: "GPIO 8 -> ST7789 DC (RS)",
      from: ["MCU J1 #12", "GPIO 8"], to: ["TFT J1 #6", "DC"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "8", 0.22),
        [1.90, -2.94, 0.37],
        [1.90, 0.33, 0.44],
        [0.381, 0.61, 0.49],
        [0.381, 0.86, 0.50],
      ] },
    { id: "wire_tft_cs", group: "display", color: 0x3a86ff, label: "GPIO 10 -> ST7789 CS",
      from: ["MCU J1 #16", "GPIO 10"], to: ["TFT J1 #7", "CS"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "10", 0.22),
        [2.04, -3.95, 0.38],
        [2.04, 0.25, 0.45],
        [0.635, 0.58, 0.49],
        [0.635, 0.86, 0.50],
      ] },

    // Shared FSPI MicroSD Breakout (26 MHz, FEATURE_SD_CARD=1)
    { id: "wire_sd_sck", group: "microsd", color: 0xf4d03f, label: "GPIO 12 -> MicroSD SCK (shared bus)",
      from: ["MCU J1 #18", "GPIO 12"], to: ["SD J1 #4", "SCK"], awg: "28 AWG",
      points: [
        [1.143, -4.46, 0.16],
        [0.65, -4.55, 0.28],
        [-0.95, -4.55, 0.28],
        [-1.38, -3.55, 0.08],
        [-1.56, -3.27, -0.10],
      ] },
    { id: "wire_sd_mosi", group: "microsd", color: 0x2ec4b6, label: "GPIO 11 -> MicroSD MOSI (shared bus)",
      from: ["MCU J1 #17", "GPIO 11"], to: ["SD J1 #5", "MOSI"], awg: "28 AWG",
      points: [
        [1.143, -4.21, 0.16],
        [0.65, -4.30, 0.29],
        [-0.95, -4.30, 0.29],
        [-1.38, -3.30, 0.09],
        [-1.56, -3.02, -0.10],
      ] },
    { id: "wire_sd_miso", group: "microsd", color: 0xf72585, label: "GPIO 15 -> MicroSD MISO",
      from: ["MCU J1 #8", "GPIO 15"], to: ["SD J1 #3", "MISO"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "15", 0.22),
        [0.45, -1.92, 0.30],
        [-1.05, -2.55, 0.24],
        [-1.56, -3.53, -0.10],
      ] },
    { id: "wire_sd_cs", group: "microsd", color: 0xeaf4f4, label: "GPIO 13 -> MicroSD CS",
      from: ["MCU J1 #19", "GPIO 13"], to: ["SD J1 #6", "CS"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "13", 0.22),
        [0.35, -4.72, 0.30],
        [-0.95, -4.65, 0.24],
        [-1.40, -2.85, 0.08],
        [-1.56, -2.76, -0.10],
      ] },

    // Wired Tactile Buttons (Active Low to GND via INPUT_PULLUP)
    { id: "wire_btn_up", group: "buttons", color: 0x8ac926, label: "GPIO 4 -> D-Pad UP",
      from: ["MCU J1 #4", "GPIO 4"], to: ["SW_UP", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "4", 0.24),
        [0.35, -0.90, 0.38],
        [-1.55, -0.95, 0.45],
        [-1.90, -1.35, 0.56],
      ] },
    { id: "wire_btn_down", group: "buttons", color: 0x8ac926, label: "GPIO 5 -> D-Pad DOWN",
      from: ["MCU J1 #5", "GPIO 5"], to: ["SW_DOWN", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "5", 0.24),
        [0.30, -1.16, 0.38],
        [-1.50, -3.05, 0.45],
        [-1.90, -3.05, 0.56],
      ] },
    { id: "wire_btn_left", group: "buttons", color: 0x8ac926, label: "GPIO 6 -> D-Pad LEFT",
      from: ["MCU J1 #6", "GPIO 6"], to: ["SW_LEFT", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "6", 0.24),
        [0.25, -1.42, 0.39],
        [-2.40, -1.80, 0.46],
        [-2.75, -2.20, 0.56],
      ] },
    { id: "wire_btn_right", group: "buttons", color: 0x8ac926, label: "GPIO 7 -> D-Pad RIGHT",
      from: ["MCU J1 #7", "GPIO 7"], to: ["SW_RIGHT", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "7", 0.24),
        [0.20, -1.67, 0.39],
        [-0.75, -2.20, 0.46],
        [-1.05, -2.20, 0.56],
      ] },
    { id: "wire_btn_a", group: "buttons", color: 0xff595e, label: "GPIO 16 -> A Button",
      from: ["MCU J1 #9", "GPIO 16"], to: ["SW_A", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "16", 0.24),
        [2.25, -2.18, 0.40],
        [2.70, -2.20, 0.56],
      ] },
    { id: "wire_btn_b", group: "buttons", color: 0xff924c, label: "GPIO 17 -> B Button",
      from: ["MCU J1 #10", "GPIO 17"], to: ["SW_B", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "17", 0.24),
        [1.80, -2.45, 0.40],
        [1.80, -3.20, 0.56],
      ] },
    { id: "wire_btn_start", group: "buttons", color: 0x1982c4, label: "GPIO 18 -> START Pill",
      from: ["MCU J1 #11", "GPIO 18"], to: ["SW_START", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J1", "18", 0.24),
        [0.25, -2.75, 0.40],
        [-0.90, -4.35, 0.48],
        [-1.15, -4.35, 0.56],
      ] },
    { id: "wire_btn_select", group: "buttons", color: 0x6a4c93, label: "GPIO 21 -> SELECT Pill",
      from: ["MCU J3 #18", "GPIO 21"], to: ["SW_SELECT", "Active Leg"], awg: "28 AWG",
      points: [
        getDevkitPinWorldPos("J3", "21", 0.24),
        [-1.95, -4.46, 0.36],
        [-2.55, -4.35, 0.56],
      ] },
    { id: "wire_btn_gnd_bus", group: "power", color: 0x1e252b, label: "Common Button GND Daisy-Chain",
      from: ["MCU J3 #22", "GND"], to: ["All 8 Switches", "GND Side"], awg: "26 AWG",
      points: [
        getDevkitPinWorldPos("J3", "GND", 0.24),
        [-2.55, -4.55, 0.49],
        [-1.90, -3.25, 0.49],
        [-2.75, -2.40, 0.49],
        [-1.90, -1.55, 0.49],
        [1.80, -3.40, 0.49],
        [2.70, -2.40, 0.49],
      ] },
  ];

  // Beginner-oriented assembly steps grounded directly in config.h & real board spacing.
  const BUILD_GUIDE_STEPS = [
    {
      id: "step_1",
      title: "1. Prep Perfboard & Headers",
      preset: "core",
      highlight: "all",
      summary: "Mount the 5x7 cm double-sided perfboard (50x70 mm, 18x24 hole grid) behind the switch plane. Leave a 14 mm left corridor for the MicroSD board.",
      tip: "Tin your iron tip cleanly first; never bridge adjacent 2.54 mm pads unless intentionally making a shared SPI or GND bus.",
    },
    {
      id: "step_2",
      title: "2. Seat ESP32-S3-DevKitC-1",
      preset: "core",
      highlight: "esp32",
      summary: "Mount DevKitC-1 vertically on the rear of the perfboard (USB-C downward at y = -59.5 mm) so the bottom port aligns with the shell cutout.",
      tip: "Using direct solder or low-profile headers keeps Z height compact; tall 8.5 mm sockets reduce rear clearance for later batteries.",
    },
    {
      id: "step_3",
      title: "3. Wire Power & Ground Buses",
      preset: "wiring",
      highlight: "power",
      summary: "Bridge one column as your common GND bus (J3 GND pins) and route 3V3 (J1 #1) to Display VCC+BLK and 5V (J1 #21) to MicroSD VCC.",
      tip: "Always check continuity between 3V3, 5V, and GND with a multimeter before plugging USB-C into your laptop.",
    },
    {
      id: "step_4",
      title: "4. Solder 2.4\" ST7789 Display (FSPI)",
      preset: "display",
      highlight: "display",
      summary: "Connect top perfboard strip to the 8-pin ST7789 header: SCK=GPIO12, MOSI=GPIO11, CS=GPIO10, DC=GPIO8, RES=GPIO9, VCC=3V3, BLK=3V3, GND.",
      tip: "All 5 display data pins sit on J1 (pins 12-18). Keep these wires short (< 35 mm) for clean 80 MHz SPI edges.",
    },
    {
      id: "step_5",
      title: "5. Solder MicroSD Breakout (Shared FSPI)",
      preset: "core",
      highlight: "microsd",
      summary: "Place the 24x42 mm SD module along the lower-left edge (y = -34 mm, below the BMO side logo). Share SCK=GPIO12 & MOSI=GPIO11; wire MISO=15, CS=13.",
      tip: "Never push the SD slot behind the 'O' letter; keep it at y = -34 mm so the card clears both 'BMO' and DevKitC-1.",
    },
    {
      id: "step_6",
      title: "6. Wire Active 8 Tactile Switches",
      preset: "wiring",
      highlight: "buttons",
      summary: "Each 6x6x5 mm switch connects one diagonal leg pair to common GND and the other to GPIO: UP=4, DOWN=5, LEFT=6, RIGHT=7, A=16, B=17, START=18, SELECT=21.",
      tip: "Omron 4-pin switches internally bridge pins horizontally across sides — verify diagonal continuity before soldering.",
    },
    {
      id: "step_7",
      title: "7. Forbidden Pins & Planned Expansion",
      preset: "exploded",
      highlight: "forbidden",
      summary: "DO NOT solder to GPIO 0, 3, 45, 46 (boot strapping) or GPIO 35-37 (Octal PSRAM). Leave X/Y/L/R, I2S audio (38-40), I2C (43-44), and ADC (1) unwired in Stage 1.",
      tip: "Enabling the Planned Expansion Layer previews where 3xAA cells (16 mm) would clash in Z unless swapped for a slim LiPo.",
    },
  ];

  function createSilkscreenTexture(widthPx, heightPx, drawFn) {
    const canvas = document.createElement("canvas");
    canvas.width = widthPx;
    canvas.height = heightPx;
    const ctx = canvas.getContext("2d");
    drawFn(ctx, widthPx, heightPx);
    const tex = new THREE.CanvasTexture(canvas);
    tex.encoding = THREE.sRGBEncoding;
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    return tex;
  }

  class BmoInternals3D {
    constructor(studioRoot) {
      this.parentRoot = studioRoot;
      this.rootGroup = new THREE.Group();
      this.rootGroup.name = "BMO_Internals_Root";
      this.parentRoot.add(this.rootGroup);

      // Layer groups arranged front-to-back along Z for clean Apple-style teardown motion
      this.layers = {
        fastenersFront: new THREE.Group(), // Case & battery-door M2 screws
        display: new THREE.Group(),        // 2.4" ST7789 SPI TFT carrier + shield + flex
        switches: new THREE.Group(),       // 8 core 6x6x5mm tactile switches + carrier PCB
        perfboard: new THREE.Group(),      // 5x7 cm emerald FR4 double-sided perfboard
        esp32: new THREE.Group(),          // ESP32-S3-DevKitC-1 N16R8
        microsd: new THREE.Group(),        // 6-pin 24x42mm MicroSD breakout
        wires: new THREE.Group(),          // 3D insulated jumper harnesses
        expansion: new THREE.Group(),      // Toggleable Planned Stage-2 expansion
      };

      Object.entries(this.layers).forEach(([k, g]) => {
        g.name = `layer_${k}`;
        this.rootGroup.add(g);
      });

      this.layerVisibility = {
        shells: true,
        screws: true,
        display: true,
        switches: true,
        perfboard: true,
        esp32: true,
        microsd: true,
        wires: true,
        expansion: false, // Hidden by default so Stage 1 active build matches config.h
      };

      // Staged explosion targets (cm) & spring state
      this.explodeTarget = 0.0;
      this.explodeCurrent = 0.0;
      this.selectedHighlight = "all";
      this.interactiveParts = [];
      this.wireMeshes = [];
      this.screwItems = [];
      this.partGroupsByTag = new Map();
      this.forbiddenMarkerGroup = new THREE.Group();

      this.buildAllInternals();
      this.setLayerVisible("expansion", false);
      this.applyExplosion(0.0);
    }

    registerInteractive(mesh, metadata) {
      mesh.userData = Object.assign({}, mesh.userData || {}, metadata, { isInternalPart: true });
      this.interactiveParts.push(mesh);
      if (metadata.groupTag) {
        if (!this.partGroupsByTag.has(metadata.groupTag)) {
          this.partGroupsByTag.set(metadata.groupTag, []);
        }
        this.partGroupsByTag.get(metadata.groupTag).push(mesh);
      }
    }

    buildAllInternals() {
      this.buildFasteners();
      this.buildDisplayModule();
      this.buildSwitchPlane();
      this.buildPerfboard5x7();
      this.buildEsp32DevKitC1();
      this.buildMicroSdModule();
      this.buildWireHarness();
      this.buildExpansionLayer();
    }

    buildFasteners() {
      const steelMat = new THREE.MeshStandardMaterial({
        color: px(0xc2c9d1),
        metalness: 0.88,
        roughness: 0.24,
      });
      const darkSlotMat = new THREE.MeshStandardMaterial({
        color: px(0x1b2126),
        roughness: 0.8,
      });

      const makeM2Screw = (x, y, assembledZ, threadLen, role, label) => {
        const g = new THREE.Group();
        g.position.set(x, y, assembledZ);
        // Pan head facing -Z
        const head = new THREE.Mesh(new THREE.CylinderGeometry(0.16, 0.16, 0.10, 20), steelMat);
        head.rotation.x = Math.PI / 2;
        head.position.z = 0.0;
        g.add(head);

        // Phillips cross recess on outer -Z head
        const slotA = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.035, 0.02), darkSlotMat);
        slotA.position.z = -0.048;
        const slotB = new THREE.Mesh(new THREE.BoxGeometry(0.035, 0.18, 0.02), darkSlotMat);
        slotB.position.z = -0.048;
        g.add(slotA, slotB);

        // Threaded shank pointing +Z into shell receiver
        const shaft = new THREE.Mesh(new THREE.CylinderGeometry(0.085, 0.085, threadLen, 16), steelMat);
        shaft.rotation.x = Math.PI / 2;
        shaft.position.z = threadLen / 2 + 0.05;
        g.add(shaft);

        this.registerInteractive(head, {
          name: label,
          role: "M2 Machine Screw",
          detail: role,
          groupTag: "screws",
        });

        this.layers.fastenersFront.add(g);
        this.screwItems.push({ group: g, baseZ: assembledZ, kind: role });
      };

      // 4 Case corner M2x8 screws
      for (const x of [-3.3, 3.3]) {
        for (const y of [-5.4, 5.4]) {
          makeM2Screw(x, y, -1.28, 0.78, "case", `M2x8 Case Screw (${x > 0 ? "R" : "L"} ${y > 0 ? "Top" : "Btm"})`);
        }
      }
      // 4 Battery door M2x5 screws
      for (const x of [-2.2, 2.2]) {
        for (const y of [-5.75, -0.25]) {
          makeM2Screw(x, y, -1.29, 0.48, "door", `M2x5 Door Screw (${x > 0 ? "R" : "L"} ${y > -3 ? "Top" : "Btm"})`);
        }
      }
    }

    buildDisplayModule() {
      const g = this.layers.display;
      // 2.4" ST7789 SPI TFT carrier PCB: 67 x 43 mm centered behind active window (0, +2.90 cm)
      const pcbTex = createSilkscreenTexture(512, 340, (ctx, w, h) => {
        ctx.fillStyle = "#b51d24"; // Classic red ILI9341/ST7789 breakout carrier
        ctx.fillRect(0, 0, w, h);
        // Subtle copper traces
        ctx.strokeStyle = "#cf363c";
        ctx.lineWidth = 3;
        for (let i = 0; i < 8; i++) {
          ctx.beginPath();
          ctx.moveTo(196 + i * 17, h - 18);
          ctx.lineTo(196 + i * 17, h - 55);
          ctx.lineTo(w / 2 + (i - 3.5) * 14, h * 0.55);
          ctx.stroke();
        }
        // Bottom 8-pin silkscreen legend (left-to-right GND..BLK)
        const labels = ["GND", "VCC", "SCK", "MOSI", "RES", "DC", "CS", "BLK"];
        ctx.fillStyle = "#f7f9fa";
        ctx.font = "bold 12px monospace";
        ctx.textAlign = "center";
        labels.forEach((lbl, idx) => {
          const pxX = 188 + idx * 19.5;
          ctx.fillText(lbl, pxX, h - 30);
        });
        ctx.font = "bold 16px monospace";
        ctx.fillText("2.4\" ST7789 SPI TFT 320x240", w / 2, 34);
        ctx.font = "13px monospace";
        ctx.fillStyle = "#ffe5e5";
        ctx.fillText("VCC = 3.3V ONLY | FSPI 80 MHz", w / 2, 56);
      });

      const pcbMat = new THREE.MeshStandardMaterial({ map: pcbTex, roughness: 0.42, metalness: 0.08 });
      const pcb = new THREE.Mesh(new THREE.BoxGeometry(6.70, 4.30, 0.16), pcbMat);
      pcb.position.set(0, 2.90, 0.68);
      pcb.castShadow = true;
      pcb.receiveShadow = true;
      g.add(pcb);
      this.registerInteractive(pcb, {
        name: "2.4\" ST7789 SPI TFT Display",
        role: "Display Carrier (67 x 43 mm)",
        detail: "GPIO 8(DC), 9(RST), 10(CS), 11(MOSI), 12(SCK), 3V3, GND. Runs 80 MHz SPI.",
        groupTag: "display",
      });

      // Stamped steel LCD panel cage on front face (+Z side facing screen bezel)
      const steelShield = new THREE.Mesh(
        new THREE.BoxGeometry(6.02, 4.22, 0.22),
        new THREE.MeshStandardMaterial({ color: px(0xd5dbe0), metalness: 0.82, roughness: 0.28 })
      );
      steelShield.position.set(0, 2.90, 0.86);
      g.add(steelShield);

      // Front ST7789 glass substrate + 320x240 active IPS matrix rectangle on +Z face
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
      g.add(lcdGlass, lcdActiveArea);

      // Amber Kapton FPC ribbon folding from LCD bottom onto rear carrier
      const fpcMat = new THREE.MeshStandardMaterial({ color: px(0xc87018), roughness: 0.35, metalness: 0.2 });
      const fpc = new THREE.Mesh(new THREE.BoxGeometry(2.6, 0.65, 0.05), fpcMat);
      fpc.position.set(0, 1.02, 0.80);
      g.add(fpc);

      // XC6206 / AMS1117 3.3V regulator + J1 bypass solder jumper on rear of TFT carrier
      const icMat = new THREE.MeshStandardMaterial({ color: px(0x1a1e24), roughness: 0.48 });
      const ldo = new THREE.Mesh(new THREE.BoxGeometry(0.45, 0.30, 0.14), icMat);
      ldo.position.set(-2.2, 1.85, 0.56);
      g.add(ldo);

      // 8-pin 2.54 mm gold header along bottom edge pointing -Z toward perfboard
      const plasticBase = new THREE.Mesh(
        new THREE.BoxGeometry(2.08, 0.26, 0.25),
        new THREE.MeshStandardMaterial({ color: px(0x14171a), roughness: 0.72 })
      );
      plasticBase.position.set(0, 0.86, 0.50);
      g.add(plasticBase);

      const goldMat = new THREE.MeshStandardMaterial({ color: px(0xe5b949), metalness: 0.90, roughness: 0.22 });
      const pinGeo = new THREE.CylinderGeometry(0.032, 0.032, 0.52, 8);
      pinGeo.rotateX(Math.PI / 2);
      const headerPins = new THREE.InstancedMesh(pinGeo, goldMat, 8);
      const dummy = new THREE.Object3D();
      for (let i = 0; i < 8; i++) {
        dummy.position.set(-0.889 + i * 0.254, 0.86, 0.44);
        dummy.updateMatrix();
        headerPins.setMatrixAt(i, dummy.matrix);
      }
      g.add(headerPins);
    }

    buildSwitchPlane() {
      const g = this.layers.switches;
      // Thin dark-green FR4 carrier strips holding the 8 wired tactile switches behind front caps
      const carrierMat = new THREE.MeshStandardMaterial({ color: px(0x145238), roughness: 0.55 });
      const dpadCarrier = new THREE.Mesh(new THREE.BoxGeometry(2.60, 2.60, 0.08), carrierMat);
      dpadCarrier.position.set(-1.90, -2.20, 0.52);
      const abCarrier = new THREE.Mesh(new THREE.BoxGeometry(1.90, 2.00, 0.08), carrierMat);
      abCarrier.position.set(2.10, -2.70, 0.52);
      const pillCarrier = new THREE.Mesh(new THREE.BoxGeometry(2.40, 0.85, 0.08), carrierMat);
      pillCarrier.position.set(-1.85, -4.35, 0.52);
      g.add(dpadCarrier, abCarrier, pillCarrier);

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

        // 4 gull-wing silver legs
        for (const lx of [-0.32, 0.32]) {
          for (const ly of [-0.20, 0.20]) {
            const leg = new THREE.Mesh(new THREE.BoxGeometry(0.10, 0.06, 0.18), legMat);
            leg.position.set(lx, ly, -0.05);
            swGroup.add(leg);
          }
        }
        swGroup.add(body, plate, plunger);
        this.registerInteractive(body, {
          name: `Tactile Switch: ${sw.key}`,
          role: "Omron B3F 6x6x5 mm",
          detail: `Wired ${sw.gpio} <-> GND (internal INPUT_PULLUP, active-LOW).`,
          groupTag: "buttons",
        });
        g.add(swGroup);
      }
    }

    buildPerfboard5x7() {
      const g = this.layers.perfboard;
      // Double-sided emerald green 5x7 cm prototype perfboard (50 x 70 x 1.6 mm)
      const boardTex = createSilkscreenTexture(512, 716, (ctx, w, h) => {
        ctx.fillStyle = "#166842";
        ctx.fillRect(0, 0, w, h);
        // White silkscreen border and corner M2 ring labels
        ctx.strokeStyle = "#e6efe9";
        ctx.lineWidth = 3;
        ctx.strokeRect(10, 10, w - 20, h - 20);
        ctx.fillStyle = "#e6efe9";
        ctx.font = "bold 16px monospace";
        ctx.textAlign = "center";
        ctx.fillText("5x7 cm DOUBLE-SIDED PERFBOARD", w / 2, 34);
        ctx.font = "13px monospace";
        ctx.fillText("18 x 24 GRID (2.54 mm PITCH)", w / 2, h - 18);
      });

      const boardMat = new THREE.MeshStandardMaterial({ map: boardTex, roughness: 0.48, metalness: 0.06 });
      const board = new THREE.Mesh(new THREE.BoxGeometry(5.00, 7.00, 0.16), boardMat);
      board.position.set(0, -2.40, 0.31); // Front face at +0.39 cm touches front standoffs
      board.castShadow = true;
      board.receiveShadow = true;
      g.add(board);
      this.registerInteractive(board, {
        name: "5x7 cm Prototype Perfboard",
        role: "Main Solder Backplane (50 x 70 mm)",
        detail: "432 plated 2.54 mm vias (18x24 grid). M2 corner standoffs at X=+/-21.5, Y=-52/+4 mm.",
        groupTag: "perfboard",
      });

      // Instanced 18 x 24 = 432 plated copper/gold annular pads on BOTH faces
      const padGeo = new THREE.CylinderGeometry(0.085, 0.085, 0.168, 10);
      padGeo.rotateX(Math.PI / 2);
      const padMat = new THREE.MeshStandardMaterial({ color: px(0xd9ae48), metalness: 0.85, roughness: 0.25 });
      const holeGeo = new THREE.CylinderGeometry(0.042, 0.042, 0.172, 8);
      holeGeo.rotateX(Math.PI / 2);
      const holeMat = new THREE.MeshStandardMaterial({ color: px(0x0b1411), roughness: 0.9 });

      const cols = 18;
      const rows = 24;
      const pads = new THREE.InstancedMesh(padGeo, padMat, cols * rows);
      const holes = new THREE.InstancedMesh(holeGeo, holeMat, cols * rows);
      const dummy = new THREE.Object3D();
      let idx = 0;
      for (let c = 0; c < cols; c++) {
        for (let r = 0; r < rows; r++) {
          const pxX = (c - (cols - 1) / 2) * 0.254;
          const pyY = -2.40 + (r - (rows - 1) / 2) * 0.254;
          dummy.position.set(pxX, pyY, 0.31);
          dummy.updateMatrix();
          pads.setMatrixAt(idx, dummy.matrix);
          holes.setMatrixAt(idx, dummy.matrix);
          idx++;
        }
      }
      g.add(pads, holes);
    }

    buildEsp32DevKitC1() {
      const g = this.layers.esp32;
      // ESP32-S3-DevKitC-1 N16R8: 25.40 x 62.74 x 1.60 mm matte-black PCB
      // Oriented facing -Z (WROOM-1 RF shield, buttons, LED, Dual USB-C visible from rear opening)
      const pcbTex = createSilkscreenTexture(320, 780, (ctx, w, h) => {
        ctx.fillStyle = "#16191f";
        ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = "#2d333d";
        ctx.lineWidth = 4;
        ctx.strokeRect(4, 4, w - 8, h - 8);
        ctx.fillStyle = "#d9e1e8";
        ctx.font = "bold 12px monospace";
        // Print official J1 & J3 header labels beside pads
        for (let i = 0; i < 22; i++) {
          const py = 122 + i * 28;
          // Drawn mirrored horizontally on texture? Box back face uses standard UV when rotated Y=PI
          ctx.textAlign = "left";
          ctx.fillStyle = ["3", "46", "0", "45", "35", "36", "37"].includes(DEVKIT_J1_PINS[i]) ? "#ff6b6b" : "#9fe3c2";
          ctx.fillText(DEVKIT_J1_PINS[i], 24, py + 4);
          ctx.textAlign = "right";
          const j3Name = DEVKIT_J3_PINS[i];
          const j3Base = j3Name.split("/")[0];
          ctx.fillStyle = ["0", "45", "35", "36", "37"].includes(j3Base) ? "#ff6b6b" : "#9ec8ff";
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

      const pcbMat = new THREE.MeshStandardMaterial({ map: pcbTex, roughness: 0.46, metalness: 0.1 });
      const pcb = new THREE.Mesh(new THREE.BoxGeometry(2.54, 6.274, 0.16), pcbMat);
      pcb.position.set(DEVKIT_X, DEVKIT_Y, DEVKIT_PCB_Z);
      pcb.rotation.y = Math.PI; // Components face -Z outward; J1 lands at +X, J3 at -X
      pcb.castShadow = true;
      pcb.receiveShadow = true;
      g.add(pcb);
      this.registerInteractive(pcb, {
        name: "ESP32-S3-DevKitC-1 N16R8",
        role: "Core MCU (25.4 x 62.74 mm)",
        detail: "2x22 headers (44 pins). Native USB OTG + UART CP2102N sit at bottom y=-59.5 mm. Avoid GPIO 0,3,35-37,45,46.",
        groupTag: "esp32",
      });

      // ESP32-S3-WROOM-1 RF Shield (18 x 19.2 mm metal can) + top exposed PCB meander antenna (18 x 6.3 mm)
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
      canMesh.position.set(0, -1.26, -0.22); // Upper body behind WROOM shield
      canMesh.rotation.y = Math.PI;
      g.add(canMesh);

      const antPcb = new THREE.Mesh(
        new THREE.BoxGeometry(1.80, 0.63, 0.08),
        new THREE.MeshStandardMaterial({ color: px(0x1b1e24), roughness: 0.5 })
      );
      antPcb.position.set(0, 0.01, -0.14);
      g.add(antPcb);
      // Copper meander antenna traces
      const goldMat = new THREE.MeshStandardMaterial({ color: px(0xe0b548), metalness: 0.88, roughness: 0.22 });
      const meander = new THREE.Mesh(new THREE.BoxGeometry(1.40, 0.06, 0.09), goldMat);
      meander.position.set(0, 0.16, -0.14);
      g.add(meander);

      // Dual USB-C stainless receptacles at bottom edge (-Y = -5.95 cm) aligned toward bottom port
      const usbShellMat = new THREE.MeshStandardMaterial({ color: px(0xb8c0c8), metalness: 0.90, roughness: 0.22 });
      const usbInteriorMat = new THREE.MeshStandardMaterial({ color: px(0x11151a), roughness: 0.8 });
      for (const ux of [-0.62, 0.62]) {
        const shell = new THREE.Mesh(new THREE.BoxGeometry(0.90, 0.74, 0.32), usbShellMat);
        shell.position.set(ux, -5.66, -0.24);
        const portHole = new THREE.Mesh(new THREE.BoxGeometry(0.76, 0.12, 0.20), usbInteriorMat);
        portHole.position.set(ux, -5.98, -0.24);
        g.add(shell, portHole);
      }

      // BOOT & RST micro tactile switches + CP2102N + AMS1117 + WS2812 RGB indicator
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

      // 44 Gold 2.54 mm header pins bridging from perfboard (+0.31) through DevKitC-1 (-0.02)
      const pinGeo = new THREE.CylinderGeometry(0.032, 0.032, 0.54, 8);
      pinGeo.rotateX(Math.PI / 2);
      const pinMesh = new THREE.InstancedMesh(pinGeo, goldMat, 44);
      const dummy = new THREE.Object3D();
      let pIdx = 0;
      for (let sideIdx = 0; sideIdx < 2; sideIdx++) {
        const pxX = sideIdx === 0 ? DEVKIT_ROW_HALF_X : -DEVKIT_ROW_HALF_X;
        for (let r = 0; r < 22; r++) {
          const pyY = DEVKIT_Y + DEVKIT_FIRST_PIN_OFFSET_Y - r * DEVKIT_PIN_PITCH;
          dummy.position.set(pxX, pyY, 0.12);
          dummy.updateMatrix();
          pinMesh.setMatrixAt(pIdx++, dummy.matrix);
        }
      }
      g.add(pinMesh);

      // Red Forbidden Pin Highlight Spheres (hidden unless 'forbidden' step/highlight is chosen)
      this.forbiddenMarkerGroup = new THREE.Group();
      const warnMat = new THREE.MeshStandardMaterial({
        color: px(0xff334b),
        emissive: px(0xff1e38),
        emissiveIntensity: 0.85,
        roughness: 0.3,
      });
      const forbiddenList = [
        ["J1", "3"], ["J1", "46"],
        ["J3", "0"], ["J3", "45"], ["J3", "35"], ["J3", "36"], ["J3", "37"],
      ];
      for (const [hdr, pName] of forbiddenList) {
        const [fx, fy] = getDevkitPinWorldPos(hdr, pName, 0);
        const marker = new THREE.Mesh(new THREE.SphereGeometry(0.11, 14, 14), warnMat);
        marker.position.set(fx, fy, -0.16);
        this.forbiddenMarkerGroup.add(marker);
      }
      this.forbiddenMarkerGroup.visible = false;
      g.add(this.forbiddenMarkerGroup);
    }

    buildMicroSdModule() {
      const g = this.layers.microsd;
      // Separate 6-pin SPI MicroSD adapter (24 x 42 mm) aligned to left port at y = -3.40 cm
      const sdTex = createSilkscreenTexture(240, 420, (ctx, w, h) => {
        ctx.fillStyle = "#194a8a";
        ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = "#e8f0f8";
        ctx.lineWidth = 3;
        ctx.strokeRect(4, 4, w - 8, h - 8);
        // With rotation.y = PI (component side facing -Z), local left (x = 18) maps to world +X (-1.56 cm pins)
        const pins = ["CS", "MOSI", "SCK", "MISO", "VCC", "GND"];
        ctx.fillStyle = "#f2f6fa";
        ctx.font = "bold 14px monospace";
        ctx.textAlign = "left";
        pins.forEach((lbl, idx) => {
          ctx.fillText(lbl, 16, 150 + idx * 25.4);
        });
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
      this.registerInteractive(pcb, {
        name: "6-Pin SPI MicroSD Breakout",
        role: "Storage Module (24 x 42 mm)",
        detail: "Mounted at y=-34 mm below BMO side glyphs. Shares SCK(12) & MOSI(11); MISO=15, CS=13, 5V VCC.",
        groupTag: "microsd",
      });

      // Push-push stainless SD card slot centered at z=0.0 aligned directly to left opening
      const steelCage = new THREE.Mesh(
        new THREE.BoxGeometry(1.50, 1.38, 0.22),
        new THREE.MeshStandardMaterial({ color: px(0xcbd3dc), metalness: 0.86, roughness: 0.24 })
      );
      steelCage.position.set(-3.12, -3.40, -0.02);
      g.add(steelCage);

      // Protruding black TF/MicroSD card dead-center inside the left y=-34 mm z=0 port cutout
      const card = new THREE.Mesh(
        new THREE.BoxGeometry(0.42, 1.12, 0.12),
        new THREE.MeshStandardMaterial({ color: px(0x1f2429), roughness: 0.55 })
      );
      card.position.set(-3.95, -3.40, 0.0);
      g.add(card);

      // 74LVC125A buffer IC + 3.3V AMS1117 regulator on visible -Z face
      const icMat = new THREE.MeshStandardMaterial({ color: px(0x1b1e23), roughness: 0.46 });
      const bufIc = new THREE.Mesh(new THREE.BoxGeometry(0.42, 0.56, 0.14), icMat);
      bufIc.position.set(-2.05, -2.05, -0.36);
      const ldoIc = new THREE.Mesh(new THREE.BoxGeometry(0.55, 0.40, 0.15), icMat);
      ldoIc.position.set(-2.05, -4.65, -0.36);
      g.add(bufIc, ldoIc);

      // 6 right-angle gold pins at inward edge x = -1.56 cm bridging toward perfboard
      const goldMat = new THREE.MeshStandardMaterial({ color: px(0xe0b548), metalness: 0.88, roughness: 0.22 });
      for (let i = 0; i < 6; i++) {
        const pin = new THREE.Mesh(new THREE.CylinderGeometry(0.032, 0.032, 0.34, 8), goldMat);
        pin.rotation.x = Math.PI / 2;
        pin.position.set(-1.56, -4.03 + i * 0.254, -0.14);
        g.add(pin);
      }
    }

    buildWireHarness() {
      const g = this.layers.wires;
      this.wireMeshes = [];

      for (const w of WIRE_DEFINITIONS) {
        const vectors = w.points.map((pt) => new THREE.Vector3(pt[0], pt[1], pt[2]));
        const curve = new THREE.CatmullRomCurve3(vectors, false, "catmullrom", 0.22);
        const tubeGeo = new THREE.TubeGeometry(curve, 28, 0.032, 8, false);
        const mat = new THREE.MeshStandardMaterial({
          color: px(w.color),
          roughness: 0.45,
          metalness: 0.05,
          emissive: px(w.color),
          emissiveIntensity: 0.0,
          transparent: true,
          opacity: 1.0,
        });
        const mesh = new THREE.Mesh(tubeGeo, mat);
        mesh.castShadow = true;
        this.registerInteractive(mesh, {
          isWire: true,
          wireId: w.id,
          wireGroup: w.group,
          name: w.label,
          role: `${w.awg} Hookup Wire`,
          detail: `${w.from[0]} (${w.from[1]})  -->  ${w.to[0]} (${w.to[1]})`,
          groupTag: w.group,
        });
        g.add(mesh);
        this.wireMeshes.push(mesh);
      }
    }

    buildExpansionLayer() {
      const g = this.layers.expansion;
      // Ghosted translucent amber/violet assemblies clearly marked DISABLED IN config.h
      const ghostMat = (hex, opacity = 0.55) => new THREE.MeshStandardMaterial({
        color: px(hex),
        transparent: true,
        opacity,
        roughness: 0.4,
        depthWrite: false,
      });

      // 1. Extra Tactile Switches X (14), Y (42), L (47), R (48)
      const xyCoords = [
        ["X (GPIO 14 - Simulated)", 1.80, -1.20, 0.68],
        ["Y (GPIO 42 - Simulated)", 0.90, -2.20, 0.68],
        ["L Shoulder (GPIO 47 - Disabled)", -2.70, 5.65, 0.68],
        ["R Shoulder (GPIO 48 - Disabled)", 2.70, 5.65, 0.68],
      ];
      for (const [name, x, y, z] of xyCoords) {
        const sw = new THREE.Mesh(new THREE.BoxGeometry(0.60, 0.60, 0.44), ghostMat(0xf59e0b, 0.72));
        sw.position.set(x, y, z);
        this.registerInteractive(sw, {
          name,
          role: "Planned Expansion Switch (DISABLED)",
          detail: "Not yet wired in Stage-1 solder build.",
          groupTag: "expansion",
        });
        g.add(sw);
      }

      // 2. MAX98357A I2S Amp (GPIO 38,39,40) + 28 mm 8-Ohm Speaker behind left speaker holes
      const maxAmp = new THREE.Mesh(new THREE.BoxGeometry(1.80, 1.90, 0.25), ghostMat(0xa855f7, 0.75));
      maxAmp.position.set(-2.20, 4.30, -0.25);
      this.registerInteractive(maxAmp, {
        name: "MAX98357A I2S Audio Amp (PLANNED)",
        role: "FEATURE_AUDIO = 0 (DISABLED)",
        detail: "Planned GPIO 38(BCLK), 39(LRC), 40(DIN). Note GPIO 38 shares onboard RGB LED on v1.1.",
        groupTag: "expansion",
      });
      g.add(maxAmp);

      const speaker = new THREE.Mesh(new THREE.CylinderGeometry(1.35, 1.35, 0.50, 28), ghostMat(0xc084fc, 0.65));
      speaker.rotation.z = Math.PI / 2;
      speaker.position.set(-3.55, 4.30, 0.0);
      g.add(speaker);

      // 3. I2C FRAM Breakout (GPIO 43 SDA, 44 SCL — UART0 warning)
      const fram = new THREE.Mesh(new THREE.BoxGeometry(1.60, 1.40, 0.20), ghostMat(0xf97316, 0.7));
      fram.position.set(2.40, 4.30, -0.25);
      this.registerInteractive(fram, {
        name: "I2C FRAM Save Module (PLANNED)",
        role: "Unwired | Shares UART0 TX/RX (43/44)",
        detail: "Using GPIO 43/44 disables Serial logging unless remapped.",
        groupTag: "expansion",
      });
      g.add(fram);

      // 4. 3xAA NiMH Battery Pack (54 x 47 x 16 mm) showing Z-interference warning inside 21.8 mm cavity
      const aaEnvelope = new THREE.Mesh(new THREE.BoxGeometry(5.40, 4.70, 1.60), ghostMat(0xef4444, 0.34));
      aaEnvelope.position.set(0.0, -3.00, -0.28);
      const wireFrame = new THREE.LineSegments(
        new THREE.EdgesGeometry(new THREE.BoxGeometry(5.40, 4.70, 1.60)),
        new THREE.LineBasicMaterial({ color: px(0xff4d6d) })
      );
      wireFrame.position.copy(aaEnvelope.position);
      this.registerInteractive(aaEnvelope, {
        name: "3xAA NiMH Bay Envelope (54x47x16 mm) — CONFLICT",
        role: "FEATURE_BATTERY_MONITOR = 0 (CLASH WARNING)",
        detail: "16 mm cell pack collides (-9.3 mm Z) with stacked DevKitC-1 + perfboard. Use flat LiPo (<= 6 mm) or deeper back shell.",
        groupTag: "expansion",
      });
      g.add(aaEnvelope, wireFrame);
    }

    setLayerVisible(layerName, visible) {
      if (layerName in this.layerVisibility) {
        this.layerVisibility[layerName] = Boolean(visible);
      }
      if (layerName === "screws") this.layers.fastenersFront.visible = Boolean(visible);
      else if (layerName === "display") this.layers.display.visible = Boolean(visible);
      else if (layerName === "switches") this.layers.switches.visible = Boolean(visible);
      else if (layerName === "perfboard") this.layers.perfboard.visible = Boolean(visible);
      else if (layerName === "esp32") this.layers.esp32.visible = Boolean(visible);
      else if (layerName === "microsd") this.layers.microsd.visible = Boolean(visible);
      else if (layerName === "wires") this.layers.wires.visible = Boolean(visible);
      else if (layerName === "expansion") this.layers.expansion.visible = Boolean(visible);
    }

    setHighlight(targetTag) {
      this.selectedHighlight = targetTag || "all";
      const isAll = this.selectedHighlight === "all";
      const isForbidden = this.selectedHighlight === "forbidden";
      if (this.forbiddenMarkerGroup) {
        this.forbiddenMarkerGroup.visible = isForbidden || this.selectedHighlight === "esp32";
      }

      for (const wireMesh of this.wireMeshes) {
        const ud = wireMesh.userData;
        const match = isAll
          || ud.wireGroup === this.selectedHighlight
          || ud.wireId === this.selectedHighlight;
        if (isAll) {
          wireMesh.material.opacity = 1.0;
          wireMesh.material.emissiveIntensity = 0.08;
        } else if (match) {
          wireMesh.material.opacity = 1.0;
          wireMesh.material.emissiveIntensity = 0.65;
        } else {
          wireMesh.material.opacity = 0.14;
          wireMesh.material.emissiveIntensity = 0.0;
        }
      }
    }

    setExplodeTarget(t) {
      this.explodeTarget = Math.max(0, Math.min(1, Number(t) || 0));
    }

    update(dt = 0.016) {
      const diff = this.explodeTarget - this.explodeCurrent;
      if (Math.abs(diff) > 0.0005) {
        const rate = 1 - Math.exp(-9.0 * dt);
        this.explodeCurrent += diff * rate;
        this.applyExplosion(this.explodeCurrent);
      }
    }

    applyExplosion(t) {
      this.explodeCurrent = t;
      // Stage 1: Screws unthread counter-clockwise and extract rearward (0.00 -> 0.30)
      const screwT = Math.min(1, t / 0.30);
      const easeScrew = 1 - Math.pow(1 - screwT, 3);
      for (const item of this.screwItems) {
        const extraPull = item.kind === "door" ? 7.4 : 6.0;
        item.group.position.z = item.baseZ - easeScrew * extraPull;
        // Counter-clockwise spin looking at screw head (-Z side)
        item.group.rotation.z = -easeScrew * Math.PI * 10.0;
      }

      // Stage 2: Internal electronics fan symmetrically across Z (0.20 -> 1.00)
      const stackT = Math.max(0, (t - 0.16) / 0.84);
      const easeStack = stackT < 0.5 ? 2 * stackT * stackT : 1 - Math.pow(-2 * stackT + 2, 2) / 2;

      this.layers.display.position.z = 2.60 * easeStack;
      this.layers.switches.position.z = 1.25 * easeStack;
      this.layers.perfboard.position.z = 0.0; // Core datum plane
      this.layers.esp32.position.z = -1.75 * easeStack;
      this.layers.microsd.position.z = -1.15 * easeStack;
      this.layers.microsd.position.x = 0.0;
      this.layers.wires.position.z = 0.0;
      // Gently stretch wires out slightly when exploded or keep tucked between perfboard & MCU
      this.layers.wires.scale.set(1, 1, 1 + 0.55 * easeStack);
      this.layers.expansion.position.z = -3.35 * easeStack;
    }

    getShellAndDoorOffsets(t) {
      const doorT = Math.min(1, t / 0.35);
      const easeDoor = 1 - Math.pow(1 - doorT, 2.5);
      const shellT = Math.max(0, (t - 0.12) / 0.88);
      const easeShell = 1 - Math.pow(1 - shellT, 2.8);
      return {
        frontShellZ: 5.40 * easeShell,
        buttonsZ: 4.10 * easeShell,
        backShellZ: -4.40 * easeShell,
        batteryDoorZ: -4.40 * easeShell - 1.65 * easeDoor,
      };
    }

    getStaticStats() {
      return {
        shellOuterMm: "82 x 124 x 26",
        cavityInnerMm: "77.8 x 119.8 x 21.8",
        perfboardMm: "50 x 70 x 1.6 (18x24)",
        esp32Mm: "25.40 x 62.74 x 4.8",
        microsdMm: "24 x 42 (y=-34 mm)",
        displayMm: "67 x 43 carrier (2.4\" ST7789)",
        rearFreeZMm: "+6.7 mm free (USB active); -9.3 mm conflict w/ 3xAA",
      };
    }
  }

  BmoInternals3D.BUILD_GUIDE_STEPS = BUILD_GUIDE_STEPS;
  BmoInternals3D.WIRE_DEFINITIONS = WIRE_DEFINITIONS;

  globalScope.BmoInternals3D = BmoInternals3D;
})(typeof window !== 'undefined' ? window : globalThis);
