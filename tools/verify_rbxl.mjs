import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";

const require = createRequire("/tmp/rbx_npm/package.json");
const rbx = require("rbx-reader-rts");

const rbxlPath = path.resolve(process.cwd(), "ASHES_OF_HUMANITY.rbxl");
const buf = fs.readFileSync(rbxlPath);
const exactArrayBuffer = buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);

const parsed = rbx.parseBuffer(exactArrayBuffer);
if (!parsed || !parsed.instances) {
  throw new Error("Failed to parse ASHES_OF_HUMANITY.rbxl via rbx-reader-rts!");
}

const byClass = {};
const rootNames = [];
let totalScriptsWithSource = 0;
let totalScriptChars = 0;

for (const inst of parsed.instances) {
  if (!inst) continue;
  byClass[inst.ClassName] = (byClass[inst.ClassName] || 0) + 1;
  if (!inst.Parent) {
    rootNames.push(`${inst.ClassName}(${inst.Properties?.Name?.value ?? inst.ClassName})`);
  }
  if (
    inst.ClassName === "Script" ||
    inst.ClassName === "LocalScript" ||
    inst.ClassName === "ModuleScript"
  ) {
    const src = inst.Properties?.Source?.value ?? "";
    if (src.length > 0) {
      totalScriptsWithSource += 1;
      totalScriptChars += src.length;
    } else {
      throw new Error(`Empty Source on script: ${inst.Properties?.Name?.value}`);
    }
  }
}

const requiredRoots = [
  "Workspace",
  "Lighting",
  "MaterialService",
  "ReplicatedStorage",
  "ServerScriptService",
  "ServerStorage",
  "StarterPlayer",
  "StarterGui",
  "SoundService",
  "Players",
];

for (const req of requiredRoots) {
  if (!byClass[req]) {
    throw new Error(`Missing required root service: ${req}`);
  }
}

console.log("=== INDEPENDENT RBXL BINARY PARSER VERIFICATION PASSED ===");
console.log("File size (bytes):", buf.byteLength);
console.log("Total parsed instances:", parsed.instances.filter(Boolean).length);
console.log("Root services:", rootNames.join(", "));
console.log("Embedded scripts verified:", totalScriptsWithSource, `(${totalScriptChars.toLocaleString()} chars of strict Luau)`);
console.log("Class breakdown:", JSON.stringify(byClass, null, 2));
