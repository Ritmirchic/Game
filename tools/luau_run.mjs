#!/usr/bin/env node
/*
 * ASHES OF HUMANITY: LAST SIGNAL — Luau VM runner.
 *
 * Executes a self-contained .luau harness (the file tests/run_qa.py generates)
 * inside a real Luau VM (WebAssembly build of upstream Luau), so the QA suite
 * also runs in environments that do not ship the native `luau` CLI.
 *
 * Usage: node tools/luau_run.mjs <harness.luau>
 */
import { readFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL } from "node:url";

const TOOLCHAIN = process.env.ASHES_TOOLCHAIN || "/tmp/rbx_npm";
let Lua;
try {
  ({ Lua } = await import("@luau-rs/luau"));
} catch {
  const direct = join(TOOLCHAIN, "node_modules/@luau-rs/luau/dist/direct-api.js");
  if (!existsSync(direct)) {
    console.error(`Luau VM unavailable: install @luau-rs/luau or set ASHES_TOOLCHAIN (looked in ${TOOLCHAIN})`);
    process.exit(2);
  }
  ({ Lua } = await import(pathToFileURL(direct).href));
}

const file = process.argv[2];
if (!file) {
  console.error("usage: node tools/luau_run.mjs <harness.luau>");
  process.exit(2);
}
const source = readFileSync(file, "utf8");

const lua = await Lua.create({ sandbox: false });
const t0 = Date.now();
const globals = lua.globals;

// The native `luau` CLI exposes tick()/os.clock()/task.wait(); mirror the ones
// the WASM build leaves out so the same harness runs on both.
const nowSeconds = () => (Date.now() - t0) / 1000;
if (!globals.get("tick")) globals.set("tick", lua.createFunction(() => nowSeconds()));
if (!globals.get("os")) {
  const osTable = lua.createTable();
  osTable.set("clock", lua.createFunction(() => nowSeconds()));
  osTable.set("time", lua.createFunction(() => Math.floor(Date.now() / 1000)));
  globals.set("os", osTable);
} else {
  const osTable = globals.get("os");
  if (osTable && osTable.set && !osTable.get("clock")) {
    osTable.set("clock", lua.createFunction(() => nowSeconds()));
  }
}
if (!globals.get("task")) {
  const taskTable = lua.createTable();
  taskTable.set("wait", lua.createFunction(() => 0));
  taskTable.set("delay", lua.createFunction(() => 0));
  taskTable.set("spawn", lua.createFunction(() => 0));
  globals.set("task", taskTable);
}

globals.set("ASHES_VM_HOST", "luau-wasm");

let exitCode = 0;
lua.addEventListener("print", (event) => {
  const text = event.text ?? "";
  if (/\b(FAILED|ERROR|PANIC|Traceback)\b/i.test(text)) exitCode = 1;
  process.stdout.write(text + "\n");
});

try {
  lua.execute(source);
} catch (err) {
  const msg = String(err && err.message ? err.message : err);
  process.stdout.write("VM-ERROR: " + msg.replace(/\s+/g, " ").slice(0, 800) + "\n");
  exitCode = 1;
}
process.exit(exitCode);
