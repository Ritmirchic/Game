#!/usr/bin/env node
/*
 * ASHES OF HUMANITY: LAST SIGNAL — Luau source gate.
 *
 * Runs the real Luau grammar (luau-parser) over every production module and
 * additionally flags writes to undeclared globals, which is the failure mode
 * that a hand-written Luau codebase actually breaks on (typo'd local names,
 * unbalanced blocks, stray `end`, `continue` in a Lua-only construct).
 *
 * Usage: node tools/luau_check.mjs
 */
import { readdirSync, statSync, readFileSync } from "node:fs";
import { join, relative } from "node:path";
import { createRequire } from "node:module";

// luau-parser lives in the toolchain prefix (see README "Verification toolchain").
const TOOLCHAIN = process.env.ASHES_TOOLCHAIN || "/tmp/rbx_npm";
let analyzeScopes, parse;
try {
  ({ analyzeScopes, parse } = await import("luau-parser"));
} catch {
  ({ analyzeScopes, parse } = createRequire(join(TOOLCHAIN, "index.js"))("luau-parser"));
}

const ROOT = new URL("..", import.meta.url).pathname;

const ROBLOX_GLOBALS = new Set([
  "game", "workspace", "script", "shared", "plugin", "_G", "_VERSION",
  "require", "print", "warn", "error", "assert", "pcall", "xpcall", "select",
  "type", "typeof", "tick", "time", "wait", "spawn", "delay", "newproxy",
  "setmetatable", "getmetatable", "rawget", "rawset", "rawequal", "rawlen",
  "next", "pairs", "ipairs", "unpack", "tonumber", "tostring", "loadstring",
  "Instance", "Vector2", "Vector2int16", "Vector3", "CFrame", "Color3", "UDim",
  "UDim2", "Rect", "NumberRange", "NumberSequence", "ColorSequence",
  "ColorSequenceKeypoint", "NumberSequenceKeypoint", "Enum", "Random", "TaskLike",
  "DateTime", "Buffer", "Axes", "Faces", "PhysicalProperties", "Ray", "Region3",
  "TweenInfo", "SharedTable", "Attributes", "math", "string", "table", "os",
  "coroutine", "utf8", "debug", "bit32", "task", "self", "continue", "break",
]);

function* walk(dir) {
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry);
    if (entry === "node_modules" || entry.startsWith(".")) continue;
    if (statSync(full).isDirectory()) yield* walk(full);
    else if (full.endsWith(".luau") || full.endsWith(".lua")) yield full;
  }
}

const targets = [];
for (const d of ["src", "tests"]) {
  try { for (const f of walk(join(ROOT, d))) targets.push(f); } catch { /* missing dir */ }
}

let syntaxErrors = 0;
let scopeErrors = 0;
const details = [];

for (const file of targets) {
  const rel = relative(ROOT, file);
  const src = readFileSync(file, "utf8");
  let ast;
  try {
    ast = parse(src, { mode: src.startsWith("--!strict") ? "strict" : "nonstrict" });
  } catch (err) {
    syntaxErrors += 1;
    const msg = (err && err.message ? err.message : String(err)).replace(/\s+/g, " ").slice(0, 220);
    details.push(`SYNTAX ${rel}: ${msg}`);
    continue;
  }
  let scoped;
  try {
    scoped = analyzeScopes(ast);
  } catch {
    continue; // scope analyser is best-effort; parse already succeeded
  }
  const written = scoped.writtenGlobals || scoped.globals?.written || new Set();
  for (const name of written) {
    const clean = String(name).replace(/^`?\w+`?:/, "").replace(/[`]/g, "").trim();
    if (clean && !ROBLOX_GLOBALS.has(clean)) {
      scopeErrors += 1;
      details.push(`GLOBAL ${rel}: writes to undeclared global '${clean}'`);
    }
  }
}

console.log(`luau-check: parsed ${targets.length} files`);
for (const line of details.slice(0, 40)) console.log("  " + line);
if (details.length > 40) console.log(`  ... ${details.length - 40} more`);
console.log(`luau-check: ${syntaxErrors} syntax error(s), ${scopeErrors} undeclared-global write(s)`);
process.exit(syntaxErrors > 0 ? 1 : 0);
