import ast
import io
import re
import shutil
import subprocess
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
        for asset in sorted(SIM_DIR.glob("*.js")) + sorted(SIM_DIR.glob("*.html")):
            matches = sink_pattern.findall(asset.read_text(encoding="utf-8"))
            self.assertEqual(matches, [], f"Unsafe DOM sink in {asset.name}: {matches}")

    def test_hostile_filenames_and_metadata_do_not_inject_dom_markup(self):
        node_bin = shutil.which("node")
        if not node_bin:
            self.skipTest("Node.js not installed")

        js_script = r"""
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const simDir = path.resolve(process.argv[1]);
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

const sandbox = {
  console,
  Math,
  Date,
  URLSearchParams,
  Uint8Array,
  Uint16Array,
  Uint32Array,
  Int32Array,
  Array,
  Object,
  String,
  Number,
  Boolean,
  JSON,
  Error,
  setTimeout,
  clearTimeout,
  requestAnimationFrame: () => 0,
  navigator: {},
  performance: { now: () => 0 },
  document: {
    getElementById: (id) => byId.get(id) || null,
    createElement: (tag) => {
      createdTags.push(tag.toLowerCase());
      return makeEl(tag);
    },
    addEventListener() {},
  },
};
sandbox.window = sandbox;
sandbox.window.addEventListener = () => {};
sandbox.CONSOLES_LIST = [{ id: "ROM_GB", name: "Game Boy" }];
sandbox.GAME_LIBRARY = { ROM_GB: [] };
sandbox.CONSOLE_HISTORY_DATABASE = {
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

const simSource = fs.readFileSync(path.join(simDir, "simulator.js"), "utf8");
vm.runInNewContext(simSource + "\nwindow.__BmoSimulatorClass = BmoSimulator;", sandbox);

const proto = sandbox.window.__BmoSimulatorClass.prototype;
const fakeSim = {
  selectedConsoleIndex: 1,
  selectedGameIndex: 0,
  currentState: "CONSOLE_SELECT",
  gameLibrary: { ROM_GB: [], ROM_NES: [], ROM_ATARI: [], ROM_COLEM: [] },
  rebuildFavoritesInDatabase() {},
  showSimulatorMessage: proto.showSimulatorMessage,
  updateSidebarMuseumView: proto.updateSidebarMuseumView,
  ingestUploadedRoms: proto.ingestUploadedRoms,
};

fakeSim.updateSidebarMuseumView();
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

(async () => {
  await fakeSim.ingestUploadedRoms([{ name: "<img src=x onerror=alert(1)>.exe", size: 12 }]);
  const logText = byId.get("playtest-engine-log").textContent;
  if (!logText.includes("<img src=x onerror=alert(1)>.exe")) {
    throw new Error("Rejected filename missing from status log: " + logText);
  }
  console.log("SIMULATOR_XSS_DEFENSE_PASS");
})().catch((err) => { console.error(err); process.exit(1); });
"""
        proc = subprocess.run(
            [node_bin, "-e", js_script, str(SIM_DIR)],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=20,
        )
        self.assertEqual(proc.returncode, 0, msg=f"{proc.stderr}\n{proc.stdout}")
        self.assertIn("SIMULATOR_XSS_DEFENSE_PASS", proc.stdout)


if __name__ == "__main__":
    unittest.main()
