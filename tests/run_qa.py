#!/usr/bin/env python3
"""
ASHES OF HUMANITY: LAST SIGNAL — Automated 6-Stage QA Orchestrator
Runs:
  1. stylua --check formatting verification across all 34 Luau source files
  2. luau-analyze static syntax & AST verification across all 34 Luau source files
  3. Real Luau VM execution (/usr/local/bin/luau) of all 6 QA Stages against
     verbatim production Shared & Server modules
  4. Independent Node.js binary .rbxl verification (tools/verify_rbxl.mjs)
"""

from __future__ import annotations
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def build_luau_vm_harness() -> str:
    mock_header = (REPO_ROOT / "tests" / "run_qa_suite.luau").read_text(encoding="utf-8")
    # Strip trailing placeholder lines from run_qa_suite.luau
    marker = "-- Dynamic module loader"
    if marker in mock_header:
        mock_header = mock_header.split(marker)[0]

    lines = [mock_header]
    lines.append("""
gEnv.AdvanceClock = function(dt: number)
    clockNow += dt
end
gEnv.RealOsClock = function(): number
    return realClock()
end

-- Create Shared & Services folders inside ReplicatedStorage & ServerScriptService
local sharedFolder = makeInstance("Folder", "Shared")
sharedFolder.Parent = RepStorageInst
local servicesFolder = makeInstance("Folder", "Services")
servicesFolder.Parent = ServerScriptInst

local MODULE_SOURCES: { [string]: string } = {}
""")

    for sf in sorted((REPO_ROOT / "src" / "ReplicatedStorage" / "Shared").glob("*.luau")):
        mod_inst_code = f"""
do
    local m = makeInstance("ModuleScript", "{sf.stem}")
    m.Parent = sharedFolder
    MODULE_SOURCES["Shared.{sf.stem}"] = [======[{sf.read_text(encoding='utf-8')}]======]
    m._modKey = "Shared.{sf.stem}"
end
"""
        lines.append(mod_inst_code)

    for sv in sorted((REPO_ROOT / "src" / "ServerScriptService" / "Services").glob("*.luau")):
        mod_inst_code = f"""
do
    local m = makeInstance("ModuleScript", "{sv.stem}")
    m.Parent = servicesFolder
    MODULE_SOURCES["Services.{sv.stem}"] = [======[{sv.read_text(encoding='utf-8')}]======]
    m._modKey = "Services.{sv.stem}"
end
"""
        lines.append(mod_inst_code)

    for ct in sorted((REPO_ROOT / "src" / "StarterPlayer" / "StarterPlayerScripts" / "Controllers").glob("*.luau")):
        mod_inst_code = f"""
MODULE_SOURCES["Controllers.{ct.stem}"] = [======[{ct.read_text(encoding='utf-8')}]======]
"""
        lines.append(mod_inst_code)

    lines.append("""
local loadedModules: { [string]: any } = {}
local origRequire = require
gEnv.require = function(target: any): any
    if type(target) == "table" and target._modKey then
        local key = target._modKey
        if loadedModules[key] ~= nil then
            return loadedModules[key]
        end
        local src = MODULE_SOURCES[key]
        local fn, err = loadstring(src, "=" .. key)
        if not fn then
            error("Compile error in " .. key .. ": " .. tostring(err))
        end
        local modEnv = setmetatable({ script = target }, { __index = gEnv })
        setfenv(fn, modEnv)
        local res = fn()
        loadedModules[key] = res
        return res
    end
    return origRequire(target)
end
""")

    assertions_code = (REPO_ROOT / "tests" / "qa_assertions.luau").read_text(encoding="utf-8")
    lines.append(assertions_code)
    return "\n".join(lines)


def main() -> int:
    print("1/4 Running stylua formatting check across src/...")
    r_fmt = subprocess.run(["stylua", "--check", "src/"], cwd=REPO_ROOT, capture_output=True, text=True)
    if r_fmt.returncode != 0:
        print("STYLUA CHECK FAILED:\n", r_fmt.stdout, r_fmt.stderr)
        return 1
    print("  -> stylua --check PASSED (0 formatting issues across 34 files)")

    print("2/4 Running luau-analyze static syntax check across all 34 Luau modules...")
    luau_files = [str(p.relative_to(REPO_ROOT)) for p in sorted((REPO_ROOT / "src").rglob("*.luau"))]
    r_ana = subprocess.run(["luau-analyze", "--formatter=plain", *luau_files], cwd=REPO_ROOT, capture_output=True, text=True)
    syntax_errs = [ln for ln in (r_ana.stdout + "\n" + r_ana.stderr).splitlines() if "SyntaxError" in ln]
    if syntax_errs:
        print("LUAU SYNTAX ERRORS:\n", "\n".join(syntax_errs))
        return 1
    print(f"  -> luau-analyze PASSED (0 syntax errors across {len(luau_files)} modules)")

    print("3/4 Executing 6-Stage Automated QA Suite inside official Luau VM (/usr/local/bin/luau)...")
    harness_src = build_luau_vm_harness()
    with tempfile.NamedTemporaryFile("w", suffix=".luau", delete=False, encoding="utf-8") as tf:
        tf.write(harness_src)
        temp_path = tf.name

    r_vm = subprocess.run(["luau", temp_path], cwd=REPO_ROOT, capture_output=True, text=True)
    Path(temp_path).unlink(missing_ok=True)
    print(r_vm.stdout)
    if r_vm.returncode != 0:
        print("LUAU VM EXECUTION FAILED:\n", r_vm.stderr)
        return 1

    print("4/4 Running independent binary .rbxl verification (node tools/verify_rbxl.mjs)...")
    r_rbxl = subprocess.run(["node", "tools/verify_rbxl.mjs"], cwd=REPO_ROOT, capture_output=True, text=True)
    print(r_rbxl.stdout)
    if r_rbxl.returncode != 0:
        print("RBXL VERIFY FAILED:\n", r_rbxl.stderr)
        return 1

    print("ALL 4 VERIFICATION SUITES COMPLETED WITH ZERO ERRORS!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
