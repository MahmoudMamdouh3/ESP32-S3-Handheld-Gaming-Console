// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
/**
 * simulator/js/local_library_client.js
 * Auto-discovers local ROMs and box art via the loopback server or a one-click folder picker.
 */

import { CONSOLES_LIST, MAX_BROWSER_ROM_SIZE, slugifyTitle } from "./constants.js";

export const RAW_COVER_BYTES = 64 * 64 * 2;
const LARGE_UPLOAD_THRESHOLD = 12;

export const ALL_EXT_TO_SYSTEM = {
  ".gb": { systemId: "ROM_GB", console: "GB" },
  ".gbc": { systemId: "ROM_GBC", console: "GBC" },
  ".nes": { systemId: "ROM_NES", console: "NES" },
  ".wad": { systemId: "ROM_WAD", console: "DOOM" },
  ".sms": { systemId: "ROM_SMS", console: "SMS" },
  ".gg": { systemId: "ROM_GG", console: "GG" },
  ".pce": { systemId: "ROM_PCE", console: "PCE" },
  ".a26": { systemId: "ROM_ATARI", console: "A26" },
  ".p8": { systemId: "ROM_PICO8", console: "P8" },
  ".gen": { systemId: "ROM_GENESIS", console: "MD" },
  ".md": { systemId: "ROM_GENESIS", console: "MD" },
  ".smd": { systemId: "ROM_GENESIS", console: "MD" },
  ".sfc": { systemId: "ROM_SNES", console: "SNES" },
  ".smc": { systemId: "ROM_SNES", console: "SNES" },
  ".ws": { systemId: "ROM_WSWAN", console: "WS" },
  ".wsc": { systemId: "ROM_WSWAN", console: "WS" },
  ".ngp": { systemId: "ROM_NGP", console: "NGP" },
  ".ngc": { systemId: "ROM_NGP", console: "NGP" },
  ".lnx": { systemId: "ROM_LYNX", console: "LNX" },
  ".col": { systemId: "ROM_COLEM", console: "COL" },
  ".sg": { systemId: "ROM_COLEM", console: "COL" },
};

const SYSTEM_BADGE_MAP = {
  ROM_GB: "GB",
  ROM_GBC: "GBC",
  ROM_NES: "NES",
  ROM_SNES: "SNES",
  ROM_GENESIS: "MD",
  ROM_SMS: "SMS",
  ROM_GG: "GG",
  ROM_PCE: "PCE",
  ROM_ATARI: "A26",
  ROM_COLEM: "COL",
  ROM_NGP: "NGP",
  ROM_LYNX: "LNX",
  ROM_WSWAN: "WS",
  ROM_PICO8: "P8",
  ROM_WAD: "DOOM",
};

const COVER_IMAGE_EXTS = new Set([".png", ".jpg", ".jpeg", ".bmp"]);
let _crcTable = /** @type {Uint32Array | null} */ (null);

function getCrc32Table() {
  if (_crcTable) return _crcTable;
  const table = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    table[i] = c >>> 0;
  }
  _crcTable = table;
  return table;
}

/** @param {string} text */
function crc32Utf8Hex(text) {
  const table = getCrc32Table();
  const bytes = new TextEncoder().encode(text);
  let crc = 0xffffffff;
  for (let i = 0; i < bytes.length; i++) {
    crc = table[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8);
  }
  return ((crc ^ 0xffffffff) >>> 0).toString(16).padStart(8, "0");
}

/** @param {string} relativePath */
export function makeRomKeyJs(relativePath) {
  const norm = String(relativePath || "").replace(/\\/g, "/").replace(/^\/+/, "").toLowerCase();
  const base = norm.split("/").pop() || norm;
  const dot = base.lastIndexOf(".");
  const stem = dot >= 0 ? base.slice(0, dot) : base;
  let prefix = "";
  let prevUnder = false;
  for (let i = 0; i < stem.length && prefix.length < 23; i++) {
    const ch = stem[i];
    if ((ch >= "a" && ch <= "z") || (ch >= "0" && ch <= "9") || ch === "-") {
      prefix += ch;
      prevUnder = false;
    } else if (prefix.length > 0 && !prevUnder) {
      prefix += "_";
      prevUnder = true;
    }
  }
  prefix = prefix.replace(/_+$/, "") || "rom";
  return `${prefix}_${crc32Utf8Hex(norm)}`;
}

/**
 * Decode an 8,192-byte 64x64 big-endian RGB565 `.raw` cover file into a BMP Blob.
 * @param {ArrayBuffer | Uint8Array} input
 * @returns {Blob}
 */
export function decodeRawBgr565ToBmpBlob(input) {
  const raw = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (raw.length !== RAW_COVER_BYTES) {
    throw new Error(`Expected ${RAW_COVER_BYTES}-byte .raw cover, got ${raw.length}`);
  }
  const width = 64;
  const height = 64;
  const dataSize = width * height * 3;
  const bmp = new Uint8Array(54 + dataSize);
  const dv = new DataView(bmp.buffer);
  bmp[0] = 0x42;
  bmp[1] = 0x4d;
  dv.setUint32(2, bmp.length, true);
  dv.setUint32(10, 54, true);
  dv.setUint32(14, 40, true);
  dv.setInt32(18, width, true);
  dv.setInt32(22, height, true);
  dv.setUint16(26, 1, true);
  dv.setUint16(28, 24, true);
  dv.setUint32(34, dataSize, true);

  let dst = 54;
  for (let y = height - 1; y >= 0; y--) {
    const rowOff = y * width * 2;
    for (let x = 0; x < width; x++) {
      const idx = rowOff + x * 2;
      const val = (raw[idx] << 8) | raw[idx + 1];
      bmp[dst++] = (((val & 0x1f) * 255) / 31) | 0;
      bmp[dst++] = ((((val >> 5) & 0x3f) * 255) / 63) | 0;
      bmp[dst++] = ((((val >> 11) & 0x1f) * 255) / 31) | 0;
    }
  }
  return new Blob([bmp], { type: "image/bmp" });
}

/**
 * @param {any} sim
 * @param {Array<any>} incomingEntries
 */
export function mergeDiscoveredLibrary(sim, incomingEntries) {
  const db = sim.gameLibrary || {};
  /** @type {Record<string, Map<string, any>>} */
  const mapsBySys = {};
  for (const c of CONSOLES_LIST) {
    if (c.id === "ROM_FAVORITES") continue;
    const map = new Map();
    const existing = Array.isArray(db[c.id]) ? db[c.id] : [];
    for (const item of existing) {
      if (item && item.title) map.set(`${item.title.toLowerCase()}|${item.ext || ""}`, item);
    }
    mapsBySys[c.id] = map;
  }

  let added = 0;
  for (const item of incomingEntries) {
    const map = mapsBySys[item.systemId];
    if (!map) continue;
    const key = `${item.title.toLowerCase()}|${item.ext || ""}`;
    if (!map.has(key)) added++;
    map.set(key, item);
  }

  for (const [sysId, map] of Object.entries(mapsBySys)) {
    const list = Array.from(map.values());
    list.sort((a, b) => String(a.title || "").localeCompare(String(b.title || ""), undefined, { sensitivity: "base" }));
    db[sysId] = list;
  }
  sim.gameLibrary = db;
  sim.rebuildFavoritesInDatabase(db);
  sim.selectedGameIndex = 0;
  sim.markRenderDirty();
  return added;
}

/**
 * @param {any} sim
 * @param {boolean} [refresh]
 */
export async function fetchServerLocalLibrary(sim, refresh = false) {
  if (typeof window === "undefined" || !window.location || window.location.protocol === "file:") {
    return 0;
  }
  if (typeof fetch !== "function") return 0;
  const url = refresh ? "/local-library/manifest.json?refresh=1" : "/local-library/manifest.json";
  const resp = await fetch(url);
  if (!resp.ok) return 0;
  const payload = await resp.json();
  const rawEntries = Array.isArray(payload?.entries) ? payload.entries : [];
  if (rawEntries.length === 0) return 0;

  const built = rawEntries.map((row) => {
    const systemId = String(row[0] || "");
    const title = String(row[1] || "");
    const ext = String(row[2] || "");
    const romToken = String(row[3] || "");
    const coverToken = String(row[4] || "");
    const flags = Number(row[5] || 0);
    return {
      title,
      console: SYSTEM_BADGE_MAP[systemId] || systemId.replace("ROM_", ""),
      systemId,
      ext,
      isFav: Boolean(flags & 1),
      isPopular: Boolean(flags & 4),
      romUrl: `/local-library/rom/${romToken}`,
      coverUrl: coverToken ? `/local-library/cover/${coverToken}` : "",
      coverKey: coverToken || "",
    };
  });

  mergeDiscoveredLibrary(sim, built);
  const sources = Array.isArray(payload?.sources) && payload.sources.length > 0
    ? payload.sources.join(", ")
    : "local storage";
  const matched = Number(payload?.matchedCovers || 0).toLocaleString();
  const msg = `Auto-detected ${built.length.toLocaleString()} local ROMs (${matched} with box art) from ${sources}.`;
  sim.showSimulatorMessage(msg);
  const serialLog = typeof document !== "undefined" ? document.getElementById("webserial-log") : null;
  if (serialLog) serialLog.textContent = msg;
  if (typeof sim.updateLibraryStatsBanner === "function") sim.updateLibraryStatsBanner();
  return built.length;
}

/** @param {string} relPath */
function shouldSkipFolderPath(relPath) {
  const lower = relPath.toLowerCase();
  return (
    lower.includes("/.git/") ||
    lower.includes("/build/") ||
    lower.includes("/tests/") ||
    lower.includes("/third_party/") ||
    lower.includes("/_local_archived_assets/")
  );
}

/** @param {Array<File>} files */
function indexFolderCovers(files) {
  /** @type {Map<string, File>} */
  const imgByStem = new Map();
  /** @type {Map<string, File>} */
  const imgBySlug = new Map();
  /** @type {Map<string, File>} */
  const rawByKey = new Map();

  for (const file of files) {
    const name = file.name || "";
    const dot = name.lastIndexOf(".");
    if (dot <= 0) continue;
    const ext = name.slice(dot).toLowerCase();
    const stem = name.slice(0, dot).toLowerCase();
    if (COVER_IMAGE_EXTS.has(ext)) {
      if (!imgByStem.has(stem)) imgByStem.set(stem, file);
      const slug = slugifyTitle(stem);
      if (!imgBySlug.has(slug)) imgBySlug.set(slug, file);
    } else if (ext === ".raw" && file.size === RAW_COVER_BYTES) {
      rawByKey.set(stem, file);
    }
  }
  return { imgByStem, imgBySlug, rawByKey };
}

/** @param {File} file @param {string} cleanTitle @param {string} ext @param {{ imgByStem: Map<string, File>, imgBySlug: Map<string, File>, rawByKey: Map<string, File> }} covers */
function matchFolderCover(file, cleanTitle, ext, covers) {
  const name = file.name || "";
  const relPath = String(file.webkitRelativePath || name).replace(/\\/g, "/");
  const relNoTop = relPath.includes("/") ? relPath.slice(relPath.indexOf("/") + 1) : relPath;
  const coverFile = covers.imgByStem.get(cleanTitle.toLowerCase()) || covers.imgBySlug.get(slugifyTitle(cleanTitle)) || null;
  if (coverFile) return { coverFile, coverRawFile: null, coverKey: `f:${coverFile.name}:${coverFile.size}` };
  const rawMatch =
    covers.rawByKey.get(makeRomKeyJs(relNoTop)) ||
    covers.rawByKey.get(makeRomKeyJs(`${ext.slice(1)}/${name}`)) ||
    covers.rawByKey.get(makeRomKeyJs(relPath)) ||
    null;
  return { coverFile: null, coverRawFile: rawMatch, coverKey: rawMatch ? `r:${rawMatch.name}` : "" };
}

/** @param {File} file @param {{ imgByStem: Map<string, File>, imgBySlug: Map<string, File>, rawByKey: Map<string, File> }} covers */
function buildLazyFolderRomEntry(file, covers) {
  const name = file.name || "";
  const relPath = String(file.webkitRelativePath || name).replace(/\\/g, "/");
  if (shouldSkipFolderPath(relPath)) return null;
  const dot = name.lastIndexOf(".");
  if (dot <= 0) return null;
  const ext = name.slice(dot).toLowerCase();
  const route = ALL_EXT_TO_SYSTEM[ext];
  if (!route) return null;
  const cleanTitle = name.slice(0, dot);
  const { coverFile, coverRawFile, coverKey } = matchFolderCover(file, cleanTitle, ext, covers);
  return {
    title: cleanTitle,
    console: route.console,
    systemId: route.systemId,
    ext,
    isFav: false,
    isPopular: false,
    romFile: file,
    fileSize: file.size,
    coverFile,
    coverRawFile,
    coverKey,
  };
}

/**
 * Index cover image / `.raw` files from a FileList and match folder ROM entries lazily.
 * @param {any} sim
 * @param {Array<File>} files
 */
export function ingestLazyFolderFiles(sim, files) {
  const covers = indexFolderCovers(files);
  /** @type {Array<any>} */
  const romEntries = [];
  let matchedCovers = 0;

  for (const file of files) {
    const entry = buildLazyFolderRomEntry(file, covers);
    if (!entry) continue;
    if (entry.coverFile || entry.coverRawFile) matchedCovers++;
    romEntries.push(entry);
  }

  if (romEntries.length === 0) {
    sim.showSimulatorMessage("No supported ROM files were found inside that folder.", true);
    return 0;
  }

  mergeDiscoveredLibrary(sim, romEntries);
  const firstSupported = romEntries.find((e) => CONSOLES_LIST.some((c) => c.id === e.systemId && c.browserSupported)) || romEntries[0];
  const targetIdx = CONSOLES_LIST.findIndex((c) => c.id === firstSupported.systemId);
  if (targetIdx >= 0) sim.selectedConsoleIndex = targetIdx;
  if (typeof sim.updateLibraryStatsBanner === "function") sim.updateLibraryStatsBanner();
  const folderMsg = `Loaded ${romEntries.length.toLocaleString()} ROM(s) (${matchedCovers.toLocaleString()} matched covers) cleanly on demand.`;
  sim.showSimulatorMessage(folderMsg);
  const serialLog = typeof document !== "undefined" ? document.getElementById("webserial-log") : null;
  if (serialLog) serialLog.textContent = folderMsg;
  return romEntries.length;
}

/** @param {Array<File>} files */
export function isFolderOrBatchUpload(files) {
  if (!files || files.length === 0) return false;
  if (files.length >= LARGE_UPLOAD_THRESHOLD) return true;
  return files.some((file) => Boolean(file && file.webkitRelativePath && String(file.webkitRelativePath).includes("/")));
}

export { MAX_BROWSER_ROM_SIZE };
