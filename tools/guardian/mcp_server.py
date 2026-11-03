"""
tools.guardian.mcp_server — Model Context Protocol (MCP) JSON-RPC 2.0 Server for BMO Guardian
Exposes the BMO Gameboy ground-truth knowledge graph, hardware pin map, AST linter,
doc truth verifier, memory budget analyzer, and 11-core native C test harness as
structured MCP tools for AI coding agents and IDEs.

Usage:
  python -m tools.guardian mcp
"""

import io
import json
import subprocess
import sys
from contextlib import redirect_stdout
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parents[2]


MCP_TOOLS_SCHEMA: List[Dict[str, Any]] = [
    {
        "name": "bmo_prompt_pack",
        "description": "Generate a self-contained task context packet (<3,000 tokens) with hardware pin ground truth, live C/C++ signatures, and verification steps for a task.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "task": {
                    "type": "string",
                    "description": "English description of the coding or debugging task.",
                }
            },
            "required": ["task"],
        },
    },
    {
        "name": "bmo_lookup",
        "description": "Look up any C/C++ symbol, GPIO pin macro, or emulator console platform in AGENT_KNOWLEDGE_GRAPH.json.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "symbol": {
                    "type": "string",
                    "description": "Symbol name, GPIO number, pin macro (e.g. TFT_CS), or file extension (e.g. .gb, .nes).",
                }
            },
            "required": ["symbol"],
        },
    },
    {
        "name": "bmo_explain",
        "description": "Return an architectural context card for a firmware file or symbol, including line counts, includes, pins accessed, and reverse dependencies.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {
                    "type": "string",
                    "description": "Relative file path substring (e.g. emu_nes.cpp) or symbol name.",
                }
            },
            "required": ["target"],
        },
    },
    {
        "name": "bmo_patch_check",
        "description": "Run AST lint rules, curly-brace balance checks, and Rule 43 buzzword verification on modified or specified files.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "files": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Optional list of file paths to check. Defaults to uncommitted git files.",
                },
                "strict": {
                    "type": "boolean",
                    "description": "Whether to treat AST warnings as errors.",
                },
            },
        },
    },
    {
        "name": "bmo_run_host_cores",
        "description": "Execute the compiled x64 native C 11-core emulator test suite (575 frames + NES save-state determinism verification).",
        "inputSchema": {
            "type": "object",
            "properties": {},
        },
    },
    {
        "name": "bmo_memory_budget",
        "description": "Inspect ESP32-S3 Flash partition headroom (8MB app0), SRAM/IRAM usage, and baked ROM sizes.",
        "inputSchema": {
            "type": "object",
            "properties": {},
        },
    },
    {
        "name": "bmo_doctor",
        "description": "Run the full hardware hard-stop preflight, pin parity check, DocTruthVerifier, AST linter, and UI design token audit.",
        "inputSchema": {
            "type": "object",
            "properties": {},
        },
    },
]


class _ArgsNamespace:
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)


def execute_mcp_tool(name: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
    """Executes a registered BMO Guardian MCP tool and returns an MCP CallToolResult payload."""
    from . import cli

    buf = io.StringIO()
    is_error = False

    try:
        if name == "bmo_prompt_pack":
            task_str = str(arguments.get("task", "general firmware task"))
            with redirect_stdout(buf):
                rc = cli.cmd_prompt_pack(_ArgsNamespace(query=task_str.split()))
            is_error = rc != 0
        elif name == "bmo_lookup":
            sym = str(arguments.get("symbol", ""))
            with redirect_stdout(buf):
                rc = cli.cmd_lookup(_ArgsNamespace(symbol=sym))
            is_error = rc != 0
        elif name == "bmo_explain":
            target = str(arguments.get("target", ""))
            with redirect_stdout(buf):
                rc = cli.cmd_explain(_ArgsNamespace(target=target))
            is_error = rc != 0
        elif name == "bmo_patch_check":
            files = arguments.get("files")
            strict = bool(arguments.get("strict", False))
            with redirect_stdout(buf):
                rc = cli.cmd_patch_check(_ArgsNamespace(files=files, strict=strict))
            is_error = rc != 0
        elif name == "bmo_memory_budget":
            with redirect_stdout(buf):
                rc = cli.cmd_budget(_ArgsNamespace())
            is_error = rc != 0
        elif name == "bmo_doctor":
            with redirect_stdout(buf):
                rc = cli.cmd_doctor(_ArgsNamespace())
            is_error = rc != 0
        elif name == "bmo_run_host_cores":
            exe_path = REPO_ROOT / "build" / "host_cores_test" / "host_cores_runner.exe"
            if not exe_path.exists():
                subprocess.run(
                    [sys.executable, "-m", "unittest", "tests/test_native_cores_execution.py"],
                    cwd=str(REPO_ROOT),
                    capture_output=True,
                    text=True,
                    timeout=120,
                )
            if exe_path.exists():
                res = subprocess.run([str(exe_path)], cwd=str(REPO_ROOT), capture_output=True, text=True, timeout=30)
                buf.write(res.stdout or "")
                if res.stderr:
                    buf.write("\n" + res.stderr)
                is_error = res.returncode != 0
            else:
                buf.write("host_cores_runner.exe could not be built or located.")
                is_error = True
        else:
            return {
                "content": [{"type": "text", "text": f"Unknown tool: {name}"}],
                "isError": True,
            }
    except Exception as exc:
        return {
            "content": [{"type": "text", "text": f"Tool execution error in {name}: {exc}"}],
            "isError": True,
        }

    return {
        "content": [{"type": "text", "text": buf.getvalue().strip()}],
        "isError": is_error,
    }


def handle_rpc_request(request: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Handles a single JSON-RPC 2.0 MCP message and returns a response dict (or None for notifications)."""
    method = request.get("method", "")
    req_id = request.get("id")
    params = request.get("params") or {}

    # Notifications have no id and expect no response
    if req_id is None and method.startswith("notifications/"):
        return None

    if method == "initialize":
        client_proto = params.get("protocolVersion", "2024-11-05")
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": client_proto,
                "capabilities": {
                    "tools": {"listChanged": False},
                },
                "serverInfo": {
                    "name": "bmo-guardian-mcp",
                    "version": "2.0.0",
                },
            },
        }

    if method == "ping":
        return {"jsonrpc": "2.0", "id": req_id, "result": {}}

    if method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {"tools": MCP_TOOLS_SCHEMA},
        }

    if method == "tools/call":
        tool_name = params.get("name", "")
        tool_args = params.get("arguments") or {}
        result = execute_mcp_tool(tool_name, tool_args)
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": result,
        }

    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {
            "code": -32601,
            "message": f"Method not found: {method}",
        },
    }


def run_stdio_server() -> int:
    """Runs the JSON-RPC 2.0 MCP server over stdin/stdout."""
    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line or line.lower().startswith("content-length:"):
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            continue
        resp = handle_rpc_request(req)
        if resp is not None:
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(run_stdio_server())
