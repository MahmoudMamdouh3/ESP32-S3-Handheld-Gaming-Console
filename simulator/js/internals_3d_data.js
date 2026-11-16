/**
 * simulator/js/internals_3d_data.js
 * Pinout coordinates, harness geometry data, assembly guide steps, and texture helpers.
 */

import { DEFAULT_HARDWARE_LAYOUT, resolveLayoutDimensions } from "./hardware_layout_data.js";

export const DEFAULT_LAYOUT_DIMS = resolveLayoutDimensions(DEFAULT_HARDWARE_LAYOUT);

export function getThree() {
  return /** @type {any} */ (globalThis).THREE;
}

export function px(hex) {
  const threeRef = getThree();
  return new threeRef.Color(hex).convertSRGBToLinear();
}

export const DEVKIT_J1_PINS = [
  "3V3", "3V3", "RST", "4", "5", "6", "7", "15", "16", "17", "18",
  "8", "3", "46", "9", "10", "11", "12", "13", "14", "5V", "GND",
];

export const DEVKIT_J3_PINS = [
  "GND", "43/TX", "44/RX", "1", "2", "42", "41", "40", "39", "38", "37",
  "36", "35", "0", "45", "48", "47", "21", "20", "19", "GND", "GND",
];

export const DEVKIT_X = 0.0;
export const DEVKIT_Y = -2.813;
export const DEVKIT_PCB_Z = -0.02;
export const DEVKIT_PIN_PITCH = 0.254;
export const DEVKIT_FIRST_PIN_OFFSET_Y = 2.667;
export const DEVKIT_ROW_HALF_X = 1.143;

export function getDevkitPinWorldPos(headerSide, pinName, explodeZ = 0) {
  const list = headerSide === "J1" ? DEVKIT_J1_PINS : DEVKIT_J3_PINS;
  const idx = list.indexOf(pinName);
  const clampedIdx = idx >= 0 ? idx : 0;
  const x = headerSide === "J1" ? DEVKIT_X + DEVKIT_ROW_HALF_X : DEVKIT_X - DEVKIT_ROW_HALF_X;
  const y = DEVKIT_Y + DEVKIT_FIRST_PIN_OFFSET_Y - clampedIdx * DEVKIT_PIN_PITCH;
  return [x, y, DEVKIT_PCB_Z + explodeZ];
}

export const WIRE_DEFINITIONS = [
  { id: "wire_3v3_tft", group: "power", color: 0xe63946, label: "3V3 -> ST7789 VCC & BLK",
    from: ["MCU J1 #1", "3V3"], to: ["TFT J1 #2/#8", "VCC + BLK"], awg: "26 AWG",
    points: [getDevkitPinWorldPos("J1", "3V3", 0.22), [1.143, 0.35, 0.36], [0.45, 0.72, 0.44], [-0.635, 0.86, 0.50]] },
  { id: "wire_gnd_tft", group: "power", color: 0x1e252b, label: "GND -> ST7789 GND",
    from: ["MCU J3 #1", "GND"], to: ["TFT J1 #1", "GND"], awg: "26 AWG",
    points: [getDevkitPinWorldPos("J3", "GND", 0.22), [-1.143, 0.20, 0.36], [-0.95, 0.68, 0.44], [-0.889, 0.86, 0.50]] },
  { id: "wire_5v_sd", group: "power", color: 0xf4a261, label: "5V -> MicroSD VCC (onboard LDO)",
    from: ["MCU J1 #21", "5V"], to: ["SD J1 #2", "VCC"], awg: "26 AWG",
    points: [getDevkitPinWorldPos("J1", "5V", 0.16), [0.55, -5.60, 0.28], [-1.05, -5.58, 0.28], [-1.56, -4.40, -0.02], [-1.56, -3.78, -0.10]] },
  { id: "wire_gnd_sd", group: "power", color: 0x1e252b, label: "GND -> MicroSD GND",
    from: ["MCU J3 #21", "GND"], to: ["SD J1 #1", "GND"], awg: "26 AWG",
    points: [getDevkitPinWorldPos("J3", "GND", 0.16), [-1.35, -5.20, 0.24], [-1.56, -4.55, 0.08], [-1.56, -4.03, -0.10]] },

  { id: "wire_tft_sck", group: "display", color: 0xf4d03f, label: "GPIO 12 -> ST7789 SCK (FSPI CLK)",
    from: ["MCU J1 #18", "GPIO 12"], to: ["TFT J1 #3", "SCK"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "12", 0.22), [1.48, -4.46, 0.34], [1.48, 0.55, 0.42], [-0.381, 0.72, 0.48], [-0.381, 0.86, 0.50]] },
  { id: "wire_tft_mosi", group: "display", color: 0x2ec4b6, label: "GPIO 11 -> ST7789 MOSI (FSPI D)",
    from: ["MCU J1 #17", "GPIO 11"], to: ["TFT J1 #4", "MOSI"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "11", 0.22), [1.62, -4.21, 0.35], [1.62, 0.48, 0.43], [-0.127, 0.68, 0.48], [-0.127, 0.86, 0.50]] },
  { id: "wire_tft_rst", group: "display", color: 0x4cc9f0, label: "GPIO 9 -> ST7789 RES",
    from: ["MCU J1 #15", "GPIO 9"], to: ["TFT J1 #5", "RES"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "9", 0.22), [1.76, -3.70, 0.36], [1.76, 0.40, 0.44], [0.127, 0.64, 0.49], [0.127, 0.86, 0.50]] },
  { id: "wire_tft_dc", group: "display", color: 0x9b5de5, label: "GPIO 8 -> ST7789 DC (RS)",
    from: ["MCU J1 #12", "GPIO 8"], to: ["TFT J1 #6", "DC"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "8", 0.22), [1.90, -2.94, 0.37], [1.90, 0.33, 0.44], [0.381, 0.61, 0.49], [0.381, 0.86, 0.50]] },
  { id: "wire_tft_cs", group: "display", color: 0x3a86ff, label: "GPIO 10 -> ST7789 CS",
    from: ["MCU J1 #16", "GPIO 10"], to: ["TFT J1 #7", "CS"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "10", 0.22), [2.04, -3.95, 0.38], [2.04, 0.25, 0.45], [0.635, 0.58, 0.49], [0.635, 0.86, 0.50]] },

  { id: "wire_sd_sck", group: "microsd", color: 0xf4d03f, label: "GPIO 12 -> MicroSD SCK (shared bus)",
    from: ["MCU J1 #18", "GPIO 12"], to: ["SD J1 #4", "SCK"], awg: "28 AWG",
    points: [[1.143, -4.46, 0.16], [0.65, -4.55, 0.28], [-0.95, -4.55, 0.28], [-1.38, -3.55, 0.08], [-1.56, -3.27, -0.10]] },
  { id: "wire_sd_mosi", group: "microsd", color: 0x2ec4b6, label: "GPIO 11 -> MicroSD MOSI (shared bus)",
    from: ["MCU J1 #17", "GPIO 11"], to: ["SD J1 #5", "MOSI"], awg: "28 AWG",
    points: [[1.143, -4.21, 0.16], [0.65, -4.30, 0.29], [-0.95, -4.30, 0.29], [-1.38, -3.30, 0.09], [-1.56, -3.02, -0.10]] },
  { id: "wire_sd_miso", group: "microsd", color: 0xf72585, label: "GPIO 15 -> MicroSD MISO",
    from: ["MCU J1 #8", "GPIO 15"], to: ["SD J1 #3", "MISO"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "15", 0.22), [0.45, -1.92, 0.30], [-1.05, -2.55, 0.24], [-1.56, -3.53, -0.10]] },
  { id: "wire_sd_cs", group: "microsd", color: 0xeaf4f4, label: "GPIO 13 -> MicroSD CS",
    from: ["MCU J1 #19", "GPIO 13"], to: ["SD J1 #6", "CS"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "13", 0.22), [0.35, -4.72, 0.30], [-0.95, -4.65, 0.24], [-1.40, -2.85, 0.08], [-1.56, -2.76, -0.10]] },

  { id: "wire_btn_up", group: "buttons", color: 0x8ac926, label: "GPIO 4 -> D-Pad UP",
    from: ["MCU J1 #4", "GPIO 4"], to: ["SW_UP", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "4", 0.24), [0.35, -0.90, 0.38], [-1.55, -0.95, 0.45], [-1.90, -1.35, 0.56]] },
  { id: "wire_btn_down", group: "buttons", color: 0x8ac926, label: "GPIO 5 -> D-Pad DOWN",
    from: ["MCU J1 #5", "GPIO 5"], to: ["SW_DOWN", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "5", 0.24), [0.30, -1.16, 0.38], [-1.50, -3.05, 0.45], [-1.90, -3.05, 0.56]] },
  { id: "wire_btn_left", group: "buttons", color: 0x8ac926, label: "GPIO 6 -> D-Pad LEFT",
    from: ["MCU J1 #6", "GPIO 6"], to: ["SW_LEFT", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "6", 0.24), [0.25, -1.42, 0.39], [-2.40, -1.80, 0.46], [-2.75, -2.20, 0.56]] },
  { id: "wire_btn_right", group: "buttons", color: 0x8ac926, label: "GPIO 7 -> D-Pad RIGHT",
    from: ["MCU J1 #7", "GPIO 7"], to: ["SW_RIGHT", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "7", 0.24), [0.20, -1.67, 0.39], [-0.75, -2.20, 0.46], [-1.05, -2.20, 0.56]] },
  { id: "wire_btn_a", group: "buttons", color: 0xff595e, label: "GPIO 16 -> A Button",
    from: ["MCU J1 #9", "GPIO 16"], to: ["SW_A", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "16", 0.24), [2.25, -2.18, 0.40], [2.70, -2.20, 0.56]] },
  { id: "wire_btn_b", group: "buttons", color: 0xff924c, label: "GPIO 17 -> B Button",
    from: ["MCU J1 #10", "GPIO 17"], to: ["SW_B", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "17", 0.24), [1.80, -2.45, 0.40], [1.80, -3.20, 0.56]] },
  { id: "wire_btn_start", group: "buttons", color: 0x1982c4, label: "GPIO 18 -> START Pill",
    from: ["MCU J1 #11", "GPIO 18"], to: ["SW_START", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J1", "18", 0.24), [0.25, -2.75, 0.40], [-0.90, -4.35, 0.48], [-1.15, -4.35, 0.56]] },
  { id: "wire_btn_select", group: "buttons", color: 0x6a4c93, label: "GPIO 21 -> SELECT Pill",
    from: ["MCU J3 #18", "GPIO 21"], to: ["SW_SELECT", "Active Leg"], awg: "28 AWG",
    points: [getDevkitPinWorldPos("J3", "21", 0.24), [-1.95, -4.46, 0.36], [-2.55, -4.35, 0.56]] },
  { id: "wire_btn_gnd_bus", group: "power", color: 0x1e252b, label: "Common Button GND Daisy-Chain",
    from: ["MCU J3 #22", "GND"], to: ["All 8 Switches", "GND Side"], awg: "26 AWG",
    points: [
      getDevkitPinWorldPos("J3", "GND", 0.24),
      [-2.55, -4.55, 0.49], [-1.90, -3.25, 0.49], [-2.75, -2.40, 0.49],
      [-1.90, -1.55, 0.49], [1.80, -3.40, 0.49], [2.70, -2.40, 0.49],
    ] },
];

export function makeBuildGuideSteps(dims = DEFAULT_LAYOUT_DIMS) {
  const perfWmm = (dims.perfboardWidthCm * 10).toFixed(2);
  const perfHmm = (dims.perfboardHeightCm * 10).toFixed(2);
  const cols = dims.perfboardCols;
  const rows = dims.perfboardRows;
  return [
    {
      id: "step_1",
      title: "1. Prep Perfboard & Headers",
      preset: "core",
      highlight: "all",
      summary: `Mount the ${cols}x${rows} double-sided perfboard (${perfWmm}x${perfHmm} mm, ${cols}x${rows} hole grid @ ${(dims.perfboardPitchCm * 10).toFixed(2)} mm pitch) behind the switch plane. Leave a 14 mm left corridor for the MicroSD board.`,
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
      summary: "DO NOT solder to GPIO 0, 3, 45, 46 (boot strapping), GPIO 26-32 (SPI Flash), or GPIO 33-37 (Octal PSRAM). Leave X/Y/L/R, I2S audio (38-40), I2C (43-44), and ADC (1) unwired in Stage 1.",
      tip: "Enabling the Planned Expansion Layer previews where 3xAA NiMH cells (16 mm) would clash in Z unless split to 2xAA in the lower bay or housed in a deeper back shell.",
    },
  ];
}

export const BUILD_GUIDE_STEPS = makeBuildGuideSteps(DEFAULT_LAYOUT_DIMS);

export function createSilkscreenTexture(widthPx, heightPx, drawFn) {
  const threeRef = getThree();
  const canvas = document.createElement("canvas");
  canvas.width = widthPx;
  canvas.height = heightPx;
  const ctx = canvas.getContext("2d");
  drawFn(ctx, widthPx, heightPx);
  const tex = new threeRef.CanvasTexture(canvas);
  tex.encoding = threeRef.sRGBEncoding;
  tex.minFilter = threeRef.LinearFilter;
  tex.magFilter = threeRef.LinearFilter;
  return tex;
}
