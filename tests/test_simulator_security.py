import ast
import http.client
import io
import re
import shutil
import subprocess
import sys
import threading
import unittest
from pathlib import Path

from simulator import run_simulator

REPO_ROOT = Path(__file__).resolve().parents[1]
SIM_DIR = REPO_ROOT / "simulator"


class _DummySocket:
    def __init__(self, raw_request: bytes):
        self._rfile = io.BytesIO(raw_request)
        self.written = io.BytesIO()

    def makefile(self, mode, *args, **kwargs):
        return self._rfile if "r" in mode else self.written


class _NoopWriter:
    def __init__(self, buf: io.BytesIO):
        self._buf = buf

    def write(self, data: bytes):
        return self._buf.write(data)

    def flush(self):
        pass


class _TestableHandler(run_simulator.Handler):
    def __init__(self, method: str, path: str, host: str = "127.0.0.1:8080"):
        raw = f"{method} {path} HTTP/1.1\r\nHost: {host}\r\n\r\n".encode("ascii")
        self.connection = _DummySocket(raw)
        self.rfile = self.connection._rfile
        self.wfile = _NoopWriter(self.connection.written)
        self.client_address = ("127.0.0.1", 12345)
        self.server = type("Srv", (), {})()
        self.requestline = f"{method} {path} HTTP/1.1"
        self.command = method
        self.path = path
        self.request_version = "HTTP/1.1"
        self.headers = {"Host": host}
        self._headers_buffer = []

    def log_message(self, format, *args):
        pass


class SimulatorSecurityTests(unittest.TestCase):
    def test_only_canonical_loopback_host_and_origin_are_accepted(self):
        self.assertTrue(run_simulator.valid_host_header("127.0.0.1:8080"))
        self.assertFalse(run_simulator.valid_host_header("localhost:8080"))
        self.assertFalse(run_simulator.valid_host_header("attacker.example:8080"))
        self.assertTrue(run_simulator.valid_origin_header("http://127.0.0.1:8080"))
        self.assertFalse(run_simulator.valid_origin_header("http://attacker.example:8080"))

    def test_static_paths_cannot_escape_simulator_directory(self):
        self.assertIsNone(run_simulator.resolve_static_path("/%2e%2e/README.md"))
        self.assertIsNone(run_simulator.resolve_static_path("/../../AGENTS.md"))
        self.assertEqual(
            run_simulator.resolve_static_path("/index.html"),
            run_simulator.DIRECTORY / "index.html",
        )

    def test_server_is_static_readonly_and_rejects_api_and_post(self):
        source_path = Path(run_simulator.__file__)
        tree = ast.parse(source_path.read_text(encoding="utf-8"))
        imported_modules = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                imported_modules.update(alias.name for alias in node.names)
            elif isinstance(node, ast.ImportFrom) and node.module:
                imported_modules.add(node.module)
        self.assertNotIn("subprocess", imported_modules)
        self.assertFalse(hasattr(run_simulator, "allow_host_core_request"))

        get_api = _TestableHandler("GET", "/api/host-cores/token")
        get_api.do_GET()
        get_resp = get_api.connection.written.getvalue().decode("ascii", errors="ignore")
        self.assertTrue(get_resp.startswith("HTTP/1.0 404 ") or "404 " in get_resp.splitlines()[0])

        post_api = _TestableHandler("POST", "/api/host-cores/run")
        post_api.do_POST()
        post_resp = post_api.connection.written.getvalue().decode("ascii", errors="ignore")
        self.assertIn("405 ", post_resp.splitlines()[0])
        self.assertIn("Allow: GET, HEAD", post_resp)

    def test_csp_policy_and_zero_unsafe_dom_sinks(self):
        for required_directive in (
            "default-src 'self'",
            "object-src 'none'",
            "base-uri 'none'",
            "form-action 'none'",
        ):
            self.assertIn(required_directive, run_simulator.CSP_POLICY)

        html = (SIM_DIR / "index.html").read_text(encoding="utf-8")
        self.assertIn('http-equiv="Content-Security-Policy"', html)
        self.assertIn("object-src 'none'", html)
        self.assertNotIn("btn-verify-host-cores", html)

        sink_pattern = re.compile(r"\b(innerHTML|outerHTML|insertAdjacentHTML|document\.write)\b")
        for asset in sorted(SIM_DIR.rglob("*.js")) + sorted(SIM_DIR.glob("*.html")):
            matches = sink_pattern.findall(asset.read_text(encoding="utf-8"))
            self.assertEqual(matches, [], f"Unsafe DOM sink in {asset.relative_to(SIM_DIR)}: {matches}")

    def test_hostile_filenames_and_metadata_do_not_inject_dom_markup(self):
        node_bin = shutil.which("node")
        if not node_bin:
            self.skipTest("Node.js not installed")

        js_script = self._DOM_XSS_HARNESS_JS + self._DOM_XSS_ASSERTIONS_JS
        proc = subprocess.run(
            [node_bin, "--input-type=module", "-e", js_script, str(SIM_DIR)],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=20,
        )
        self.assertEqual(proc.returncode, 0, msg=f"{proc.stderr}\n{proc.stdout}")
        self.assertIn("SIMULATOR_XSS_DEFENSE_PASS", proc.stdout)

    _DOM_XSS_HARNESS_JS = r"""
const createdTags = [];
function makeEl(tag, id) {
  const el = {
    tagName: tag.toUpperCase(),
    id: id || "",
    className: "",
    children: [],
    dataset: {},
    style: {},
    value: "",
    _text: "",
    set textContent(v) { this._text = String(v); },
    get textContent() { return this._text; },
    set innerText(v) { this._text = String(v); },
    get innerText() { return this._text; },
    set innerHTML(_v) { throw new Error("innerHTML assignment forbidden"); },
    set outerHTML(_v) { throw new Error("outerHTML assignment forbidden"); },
    append(...nodes) { this.children.push(...nodes); },
    appendChild(n) { this.children.push(n); return n; },
    replaceChildren(...nodes) { this.children = [...nodes]; },
    classList: { toggle() {}, add() {}, remove() {} },
    setAttribute() {},
    addEventListener() {},
    getContext() { return { fillRect(){}, fillText(){}, clearRect(){}, save(){}, restore(){}, translate(){}, scale(){}, beginPath(){}, arc(){}, fill(){}, strokeRect(){}, measureText:()=>({width:10}), fillStyle:"", strokeStyle:"", font:"", textAlign:"" }; },
  };
  return el;
}
const byId = new Map();
for (const id of [
  "playtest-engine-log", "wasm-engine-status",
  "museum-sidebar-title", "museum-sidebar-tagline", "museum-sidebar-cpu",
  "museum-sidebar-ram", "museum-sidebar-res", "museum-sidebar-history", "museum-sidebar-games",
]) {
  byId.set(id, makeEl("div", id));
}
globalThis.window = globalThis;
globalThis.window.addEventListener = () => {};
globalThis.document = {
  getElementById: (id) => byId.get(id) || null,
  createElement: (tag) => { createdTags.push(tag.toLowerCase()); return makeEl(tag); },
  addEventListener() {},
  removeEventListener() {},
};
globalThis.performance = globalThis.performance || { now: () => 0 };
globalThis.requestAnimationFrame = () => 0;
"""

    _DOM_XSS_ASSERTIONS_JS = r"""
const { BmoSimulator } = await import("./simulator/simulator.js");
const proto = BmoSimulator.prototype;
const maliciousConsoles = [{ id: "ROM_GB", name: "Game Boy" }];
const maliciousDb = {
  ROM_GB: {
    name: "<img src=x onerror=alert(1)>",
    year: 1989,
    tagline: "</script><svg onload=alert(2)>",
    specs: { cpu: "LR35902", ram: "8KB", resolution: "160x144" },
    historicalSignificance: "<b>unsafe?</b>",
    hallmarkGames: [{
      title: "<img src=x onerror=alert(1)>.gb",
      year: 1990,
      significance: "</script><script>alert(3)</script>",
    }],
  },
};
const fakeSim = {
  selectedConsoleIndex: 0,
  selectedGameIndex: 0,
  currentState: "CONSOLE_SELECT",
  gameLibrary: { ROM_GB: [], ROM_NES: [], ROM_ATARI: [], ROM_COLEM: [] },
  rebuildFavoritesInDatabase() {},
  showSimulatorMessage: proto.showSimulatorMessage,
  updateSidebarMuseumView: proto.updateSidebarMuseumView,
  ingestUploadedRoms: proto.ingestUploadedRoms,
};
fakeSim.updateSidebarMuseumView(maliciousDb, maliciousConsoles);
const titleText = byId.get("museum-sidebar-title").textContent;
if (titleText !== "<img src=x onerror=alert(1)> (1989)") {
  throw new Error("Unexpected sidebar title textContent: " + titleText);
}
const gameItems = byId.get("museum-sidebar-games").children;
if (gameItems.length !== 1 || gameItems[0].children[0].textContent !== "<img src=x onerror=alert(1)>.gb (1990)") {
  throw new Error("Hallmark game payload was altered or injected HTML");
}
for (const tag of createdTags) {
  if (tag !== "div") throw new Error("Unexpected DOM tag created: " + tag);
}
await fakeSim.ingestUploadedRoms([{ name: "<img src=x onerror=alert(1)>.exe", size: 12 }]);
const logText = byId.get("playtest-engine-log").textContent;
if (!logText.includes("<img src=x onerror=alert(1)>.exe")) {
  throw new Error("Rejected filename missing from status log: " + logText);
}
console.log("SIMULATOR_XSS_DEFENSE_PASS");
"""

    def test_classic_bundle_boots_and_handles_controls_on_file_and_http(self):
        bundle_check = subprocess.run(
            [sys.executable, str(REPO_ROOT / "scripts" / "bundle_simulator.py"), "--check"],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(bundle_check.returncode, 0, msg=bundle_check.stdout + bundle_check.stderr)

        index_html = (SIM_DIR / "index.html").read_text(encoding="utf-8")
        self.assertIn('<script defer src="bmo_simulator_app.js"></script>', index_html)
        self.assertNotIn('<script type="module"', index_html)

        node_bin = shutil.which("node")
        if not node_bin:
            self.skipTest("Node.js not installed")

        proc = subprocess.run(
            [node_bin, "--input-type=module", "-e", self._CLASSIC_BUNDLE_BOOT_JS],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=20,
        )
        self.assertEqual(proc.returncode, 0, msg=f"{proc.stderr}\n{proc.stdout}")
        self.assertIn("CLASSIC_BUNDLE_CONTROLS_PASS", proc.stdout)

    _CLASSIC_BUNDLE_BOOT_JS = r"""
import fs from "node:fs";
import vm from "node:vm";

function makeClassList(el) {
  const set = new Set(String(el.className || "").split(/\s+/).filter(Boolean));
  return {
    add(...names) { names.forEach((n) => set.add(n)); },
    remove(...names) { names.forEach((n) => set.delete(n)); },
    contains(n) { return set.has(n); },
    toggle(n, force) {
      if (force === undefined) force = !set.has(n);
      if (force) set.add(n); else set.delete(n);
      return force;
    },
  };
}

function makeEl(id, cls = "", attrs = {}) {
  const listeners = new Map();
  const ctx2d = {
    fillRect() {}, clearRect() {}, strokeRect() {}, beginPath() {}, closePath() {},
    moveTo() {}, lineTo() {}, quadraticCurveTo() {}, arc() {}, ellipse() {}, fill() {}, stroke() {},
    save() {}, restore() {}, translate() {}, scale() {}, rotate() {}, roundRect() {},
    fillText() {}, drawImage() {}, measureText: (s) => ({ width: String(s).length * 6 }),
    createRadialGradient: () => ({ addColorStop() {} }),
    createLinearGradient: () => ({ addColorStop() {} }),
    fillStyle: "", strokeStyle: "", lineWidth: 1, font: "", textAlign: "", textBaseline: "",
  };
  const el = {
    id, className: cls, children: [], dataset: {}, style: { setProperty() {} },
    value: attrs.value ?? "100", checked: Boolean(attrs.checked), clientWidth: 480, clientHeight: 660,
    width: 320, height: 240, textContent: "", innerText: "", options: [],
    classList: null,
    getAttribute: (k) => (k in attrs ? attrs[k] : null),
    setAttribute: (k, v) => { attrs[k] = String(v); },
    addEventListener: (t, fn) => {
      if (!listeners.has(t)) listeners.set(t, []);
      listeners.get(t).push(fn);
    },
    removeEventListener() {},
    dispatchEvent: (ev) => {
      const e = ev && typeof ev === "object" ? ev : { type: String(ev) };
      if (!e.target) e.target = el;
      (listeners.get(e.type) || []).forEach((fn) => fn(e));
      return true;
    },
    click() { this.dispatchEvent({ type: "click", target: this }); },
    append(...nodes) { this.children.push(...nodes); },
    appendChild(n) { this.children.push(n); return n; },
    replaceChildren(...nodes) { this.children = [...nodes]; },
    querySelector: () => makeEl("sub"),
    scrollIntoView() {},
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 320, height: 240 }),
    getContext: () => ctx2d,
  };
  el.classList = makeClassList(el);
  return el;
}

const byId = new Map();
for (const id of [
  "stage-container", "three-viewport-container", "tft-display", "gameboy-chassis",
  "btn-toggle-viewmode", "btn-internals-mode", "btn-bmo-mode", "btn-internals-play",
  "btn-internals-assemble", "btn-internals-allwires", "btn-internals-next-step",
  "internals-panel", "internals-status-badge", "internals-guide-heading", "internals-guide-body",
  "internals-guide-tip", "bmo-mode-panel", "bmo-mode-status", "val-explode",
  "slider-lighting", "slider-saturation", "slider-temperature", "val-lighting", "val-saturation",
  "val-temperature", "btn-reset-studio", "palette-select", "theme-select", "filter-select",
  "wasm-engine-status", "playtest-engine-log", "museum-sidebar-title", "museum-sidebar-tagline",
  "museum-sidebar-cpu", "museum-sidebar-ram", "museum-sidebar-res", "museum-sidebar-history",
  "museum-sidebar-games",
]) {
  byId.set(id, makeEl(id, id === "stage-container" ? "view-3d" : ""));
}
const sliderExplode = makeEl("slider-explode", "", { value: "0" });
byId.set("slider-explode", sliderExplode);

const selectors = {
  ".cam-preset-btn": [makeEl("", "cam-preset-btn", { "data-cam": "front" }), makeEl("", "cam-preset-btn", { "data-cam": "iso" })],
  ".internals-preset-btn": [makeEl("", "internals-preset-btn", { "data-preset": "assembled" }), makeEl("", "internals-preset-btn", { "data-preset": "core" })],
  ".internals-step-btn": [makeEl("", "internals-step-btn", { "data-step": "0" }), makeEl("", "internals-step-btn", { "data-step": "1" })],
  ".internals-filter-btn": [makeEl("", "internals-filter-btn", { "data-highlight": "all" })],
  ".internals-layer-chk": [makeEl("", "internals-layer-chk", { "data-layer": "frontShell", checked: true })],
  ".scale-btn": [],
  ".state-nav-btn": [],
  ".bmo-expr-btn": [],
  ".bmo-gaze-btn": [],
};

const winListeners = new Map();
const docObj = {
  readyState: "loading",
  documentElement: { style: { setProperty() {} } },
  body: makeEl("body"),
  head: { appendChild(node) { if (typeof node?.onerror === "function") node.onerror(); } },
  getElementById: (id) => byId.get(id) || null,
  querySelectorAll: (sel) => selectors[sel] || [],
  createElement: (tag) => makeEl(tag),
  addEventListener() {}, removeEventListener() {},
};
const sandbox = {
  console, Math, Number, String, Boolean, Array, Object, Map, Set, Uint8Array, Uint16Array, Uint32Array, Int16Array, Float32Array, DataView, ArrayBuffer, Promise, Error, URLSearchParams,
  Event: class { constructor(t) { this.type = t; } },
  parseInt, parseFloat, isNaN, setTimeout: () => 0, clearTimeout() {},
  requestAnimationFrame: () => 1, cancelAnimationFrame() {},
  performance: { now: () => 100 }, navigator: { userAgent: "node" }, devicePixelRatio: 1,
  Image: class {}, atob: (s) => Buffer.from(s, "base64").toString("binary"),
  location: { protocol: "file:", search: "" },
  document: docObj,
  addEventListener: (t, fn) => { if (!winListeners.has(t)) winListeners.set(t, []); winListeners.get(t).push(fn); },
  removeEventListener() {},
};
sandbox.window = sandbox;
sandbox.self = sandbox;
sandbox.globalThis = sandbox;
const ctx = vm.createContext(sandbox);
for (const file of [
  "simulator/three.min.js",
  "simulator/OrbitControls.js",
  "simulator/bmo_enclosure_meshes.js",
]) {
  vm.runInContext(fs.readFileSync(file, "utf8"), ctx);
}
sandbox.THREE.WebGLRenderer = class {
  constructor() { this.domElement = makeEl("webgl"); this.shadowMap = {}; }
  setSize() {} setPixelRatio() {} render() {} dispose() {}
};
sandbox.THREE.PMREMGenerator = class {
  fromScene() { return { texture: new sandbox.THREE.Texture() }; }
  dispose() {}
};
vm.runInContext(fs.readFileSync("simulator/bmo_simulator_app.js", "utf8"), ctx);
docObj.readyState = "interactive";
(winListeners.get("DOMContentLoaded") || []).forEach((fn) => fn({ type: "DOMContentLoaded" }));

const sim = sandbox.simulator;
if (!sim || !sim.bmo3D) throw new Error("Expected BmoSimulator and Bmo3DConsole to initialize cleanly");
selectors[".cam-preset-btn"][1].click();
const camPos = sim.bmo3D.camera.position;
if (Math.abs(camPos.x - 11.5) > 0.01 || Math.abs(camPos.y - 8.5) > 0.01 || Math.abs(camPos.z - 20) > 0.01) {
  throw new Error(`3D Iso preset button failed to reposition camera: ${camPos.x}, ${camPos.y}, ${camPos.z}`);
}
byId.get("btn-toggle-viewmode").click();
if (sim.is3DView !== false || !byId.get("stage-container").classList.contains("view-2d")) {
  throw new Error("2D Flat View toggle failed");
}
byId.get("btn-toggle-viewmode").click();
if (sim.is3DView !== true || !byId.get("stage-container").classList.contains("view-3d")) {
  throw new Error("3D Console View toggle back failed");
}
byId.get("btn-internals-mode").click();
if (!sim.internalsModeActive || !byId.get("internals-panel").classList.contains("is-active")) {
  throw new Error("Dissect / Internals button failed");
}
selectors[".internals-preset-btn"][1].click();
if (sim.internalsPreset !== "core" || byId.get("internals-status-badge").textContent !== "Core PCB Open") {
  throw new Error("Core PCB + MCU preset button failed");
}
sliderExplode.value = "50";
sliderExplode.dispatchEvent({ type: "input" });
if (sim.bmo3D.explodeTarget !== 0.5 || byId.get("val-explode").textContent !== "50%") {
  throw new Error("Explode Z-Stack slider failed");
}
byId.get("btn-internals-next-step").click();
if (!byId.get("internals-guide-heading").textContent.startsWith("2.")) {
  throw new Error("Beginner Solder Guide Next Step button failed");
}
byId.get("btn-bmo-mode").click();
if (sim.currentState !== 5 || !sim.bmoModeActive || !byId.get("bmo-mode-panel").classList.contains("is-active")) {
  throw new Error(`BMO Mode toolbar toggle failed: state=${sim.currentState}`);
}
console.log("CLASSIC_BUNDLE_CONTROLS_PASS");
"""

    def test_live_loopback_server_ephemeral_port_http_contracts(self):
        httpd = run_simulator.LoopbackThreadingServer(("127.0.0.1", 0), run_simulator.Handler)
        port = httpd.server_address[1]
        thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        thread.start()
        try:
            conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.putrequest("GET", "/index.html", skip_host=True)
            conn.putheader("Host", f"127.0.0.1:{port}")
            conn.endheaders()
            resp = conn.getresponse()
            body = resp.read()
            self.assertEqual(resp.status, 200)
            self.assertIn(b"BMO Handheld", body)
            self.assertEqual(resp.getheader("Content-Security-Policy"), run_simulator.CSP_POLICY)
            self.assertEqual(resp.getheader("X-Content-Type-Options"), "nosniff")
            conn.close()

            conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.putrequest("GET", "/js/constants.js", skip_host=True)
            conn.putheader("Host", f"127.0.0.1:{port}")
            conn.endheaders()
            js_resp = conn.getresponse()
            js_body = js_resp.read()
            self.assertEqual(js_resp.status, 200)
            self.assertIn("application/javascript", js_resp.getheader("Content-Type", ""))
            self.assertIn(b"CONSOLES_LIST", js_body)
            conn.close()

            for blocked_path in ("/%2e%2e/README.md", "/../../AGENTS.md", "/hardware/.."):
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                conn.putrequest("GET", blocked_path, skip_host=True)
                conn.putheader("Host", f"127.0.0.1:{port}")
                conn.endheaders()
                trav_resp = conn.getresponse()
                trav_resp.read()
                self.assertEqual(trav_resp.status, 404, msg=blocked_path)
                conn.close()

            conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.putrequest("GET", "/hardware/layout.json", skip_host=True)
            conn.putheader("Host", f"127.0.0.1:{port}")
            conn.endheaders()
            layout_resp = conn.getresponse()
            layout_body = layout_resp.read()
            self.assertEqual(layout_resp.status, 200)
            self.assertIn(b"owner_reference_cm", layout_body)
            conn.close()

            for bad_host in (f"attacker.example:{port}", f"localhost:{port}"):
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                conn.putrequest("GET", "/index.html", skip_host=True)
                conn.putheader("Host", bad_host)
                conn.endheaders()
                host_resp = conn.getresponse()
                host_resp.read()
                self.assertEqual(host_resp.status, 403, msg=bad_host)
                conn.close()

            conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.putrequest("POST", "/index.html", skip_host=True)
            conn.putheader("Host", f"127.0.0.1:{port}")
            conn.putheader("Content-Length", "0")
            conn.endheaders()
            post_resp = conn.getresponse()
            post_resp.read()
            self.assertEqual(post_resp.status, 405)
            conn.close()
        finally:
            httpd.shutdown()
            httpd.server_close()
            thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
