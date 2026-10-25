/* =========================================================================
   SPACE MINER — tiny PE resource injector
   Appends a .rsrc section carrying RT_ICON + RT_GROUP_ICON entries taken from
   win/icon.ico, so the shipped executables show the game icon in Explorer,
   the taskbar, shortcuts and Alt+Tab.

   Only what a loader needs is touched: the section table, SizeOfImage and the
   resource data directory. No relocations are added (the section has none).
   ========================================================================= */
const fs = require('fs');

const SECTION_HEADER = 40;

function align(v, a) { return Math.ceil(v / a) * a; }

function readIco(icoPath) {
  const buf = fs.readFileSync(icoPath);
  const count = buf.readUInt16LE(4);
  const out = [];
  for (let i = 0; i < count; i++) {
    const o = 6 + i * 16;
    const w = buf[o], h = buf[o + 1];
    const planes = buf.readUInt16LE(o + 4);
    const bpp = buf.readUInt16LE(o + 6);
    const size = buf.readUInt32LE(o + 8);
    const off = buf.readUInt32LE(o + 12);
    out.push({
      width: w === 0 ? 256 : w,
      height: h === 0 ? 256 : h,
      bytes: w, planes, bpp,
      data: buf.subarray(off, off + size)
    });
  }
  return out;
}

/* ---- resource section builder ---------------------------------------- */
/* groups: [{ type, items: [{ id, data }] }] */
function buildResourceSection(groups) {
  const chunks = [];
  let cursor = 0;

  const reserve = (n) => {
    cursor = align(cursor, 4);
    const at = cursor;
    cursor += n;
    return at;
  };
  const put = (buf) => {
    cursor = align(cursor, 4);
    const at = cursor;
    chunks.push({ at, buf });
    cursor += buf.length;
    return at;
  };

  const typeDirAt = reserve(16 + groups.length * 8);
  const typeEntries = [];
  for (const g of groups) {
    const nameDirAt = reserve(16 + g.items.length * 8);
    const items = [];
    for (const it of g.items) {
      const langDirAt = reserve(16 + 8);
      const dataEntryAt = reserve(16);
      items.push({ langDirAt, dataEntryAt, id: it.id, data: it.data });
    }
    typeEntries.push({ type: g.type, nameDirAt, items });
  }
  // blobs last so directory offsets stay small and tidy
  for (const t of typeEntries) {
    for (const it of t.items) it.blobAt = put(it.data);
  }

  const out = Buffer.alloc(cursor);
  const writeDir = (at, entries) => {
    out.writeUInt32LE(0, at + 4);                   // timestamp
    out.writeUInt16LE(0, at + 8);
    out.writeUInt16LE(0, at + 10);
    out.writeUInt16LE(0, at + 12);                  // named entries
    out.writeUInt16LE(entries.length, at + 14);     // id entries
    entries.forEach((e, i) => {
      const o = at + 16 + i * 8;
      out.writeUInt32LE(e.name >>> 0, o);
      out.writeUInt32LE(e.target >>> 0, o + 4);
    });
  };
  const HIGH = 0x80000000;

  writeDir(typeDirAt, typeEntries.map((t) => ({ name: t.type, target: t.nameDirAt | HIGH })));
  for (const t of typeEntries) {
    writeDir(t.nameDirAt, t.items.map((it) => ({ name: it.id, target: it.langDirAt | HIGH })));
    for (const it of t.items) writeDir(it.langDirAt, [{ name: 0x0409, target: it.dataEntryAt }]);
  }
  for (const t of typeEntries) {
    for (const it of t.items) {
      out.writeUInt32LE(it.blobAt, it.dataEntryAt);           // RVA patched later
      out.writeUInt32LE(it.data.length, it.dataEntryAt + 4);
      out.writeUInt32LE(0, it.dataEntryAt + 8);               // codepage
      out.writeUInt32LE(0, it.dataEntryAt + 12);
    }
  }
  for (const c of chunks) c.buf.copy(out, c.at);
  return { data: out, size: out.length };
}

function buildGroupIcon(images, ids) {
  const head = Buffer.alloc(6 + images.length * 14);
  head.writeUInt16LE(0, 0);
  head.writeUInt16LE(1, 2);
  head.writeUInt16LE(images.length, 4);
  images.forEach((im, i) => {
    const o = 6 + i * 14;
    head[o] = im.width >= 256 ? 0 : im.width;
    head[o + 1] = im.height >= 256 ? 0 : im.height;
    head[o + 2] = 0;
    head[o + 3] = 0;
    head.writeUInt16LE(im.planes || 1, o + 4);
    head.writeUInt16LE(im.bpp || 32, o + 6);
    head.writeUInt32LE(im.data.length, o + 8);
    head.writeUInt16LE(ids[i], o + 12);
  });
  return head;
}

/* ---- VS_VERSIONINFO (RT_VERSION = 16) -------------------------------- */
function pad4(bufs) {
  const merged = Buffer.concat(bufs);
  const padLen = align(merged.length, 4) - merged.length;
  return padLen ? Buffer.concat([merged, Buffer.alloc(padLen)]) : merged;
}

function utf16z(str) {
  const b = Buffer.alloc((str.length + 1) * 2);
  for (let i = 0; i < str.length; i++) b.writeUInt16LE(str.charCodeAt(i), i * 2);
  return b;
}

function versionStringBlock(key, value) {
  const keyBuf = utf16z(key);
  const valBuf = utf16z(value);
  const header = Buffer.alloc(6);
  // length filled by caller
  header.writeUInt16LE(valBuf.length, 2);         // wValueLength = bytes (UTF-16)
  header.writeUInt16LE(1, 4);                     // wType = text
  const body = pad4([header, keyBuf]);
  const full = Buffer.concat([body, valBuf]);
  const ending = align(full.length, 4) - full.length;
  const outBuf = ending ? Buffer.concat([full, Buffer.alloc(ending)]) : full;
  outBuf.writeUInt16LE(outBuf.length, 0);
  return outBuf;
}

function block(key, valueBuf, wValueLength, wType, children) {
  const keyBuf = utf16z(key);
  const header = Buffer.alloc(6);
  header.writeUInt16LE(wValueLength, 2);
  header.writeUInt16LE(wType, 4);
  const parts = [pad4([header, keyBuf])];
  if (valueBuf && valueBuf.length) parts.push(valueBuf);
  if (children) parts.push(children);
  let full = Buffer.concat(parts);
  const ending = align(full.length, 4) - full.length;
  if (ending) full = Buffer.concat([full, Buffer.alloc(ending)]);
  full.writeUInt16LE(full.length, 0);
  return full;
}

function buildVersionInfo(info) {
  const v = info.version.split('.').map((n) => parseInt(n, 10) || 0);
  while (v.length < 4) v.push(0);
  const fixed = Buffer.alloc(52);
  fixed.writeUInt32LE(0xfeef04bd, 0);                       // signature
  fixed.writeUInt32LE(0x00010000, 4);                       // struct version
  fixed.writeUInt32LE((v[0] << 16) | v[1], 8);              // file version MS
  fixed.writeUInt32LE((v[2] << 16) | v[3], 12);             // file version LS
  fixed.writeUInt32LE((v[0] << 16) | v[1], 16);             // product version MS
  fixed.writeUInt32LE((v[2] << 16) | v[3], 20);             // product version LS
  fixed.writeUInt32LE(0x3f, 24);                            // flags mask
  fixed.writeUInt32LE(0, 28);                               // flags
  fixed.writeUInt32LE(0x00040004, 32);                      // OS: NT, 32-bit
  fixed.writeUInt32LE(1, 36);                               // file type: app
  fixed.writeUInt32LE(0, 40);
  fixed.writeUInt32LE(0, 44);
  fixed.writeUInt32LE(0, 48);

  const table = Buffer.concat([
    versionStringBlock('CompanyName', info.company),
    versionStringBlock('FileDescription', info.description),
    versionStringBlock('FileVersion', info.version),
    versionStringBlock('InternalName', info.internal),
    versionStringBlock('LegalCopyright', info.copyright),
    versionStringBlock('OriginalFilename', info.filename),
    versionStringBlock('ProductName', info.product),
    versionStringBlock('ProductVersion', info.version),
    versionStringBlock('Comments', info.comments || '')
  ]);
  const stringTable = block('040904B0', null, 0, 1, table);
  const stringFileInfo = block('StringFileInfo', null, 0, 1, stringTable);
  if (info.variable) {
    const varTable = block('Translation', Buffer.from([0x09, 0x04, 0xb0, 0x04]), 4, 0, null);
    varTable.writeUInt16LE(varTable.length, 0);
    varTable.writeUInt16LE(4, 2);
    varTable.writeUInt16LE(0, 4);
    const varFileInfo = block('VarFileInfo', null, 0, 1, varTable);
    return block('VS_VERSION_INFO', fixed, 52, 0, Buffer.concat([stringFileInfo, varFileInfo]));
  }
  return block('VS_VERSION_INFO', fixed, 52, 0, stringFileInfo);
}

/* ---- PE surgery ------------------------------------------------------ */
function injectResources(exeBuf, icoPath, versionInfo) {
  const exe = Buffer.from(exeBuf);
  if (exe.readUInt16LE(0) !== 0x5a4d) throw new Error('not an MZ file');
  const peOff = exe.readUInt32LE(0x3c);
  if (exe.readUInt32LE(peOff) !== 0x00004550) throw new Error('not a PE file');
  const coff = peOff + 4;
  const numSections = exe.readUInt16LE(coff + 2);
  const optSize = exe.readUInt16LE(coff + 16);
  const opt = coff + 20;
  const magic = exe.readUInt16LE(opt);
  if (magic !== 0x20b) throw new Error('expected PE32+ (x64), got 0x' + magic.toString(16));

  const sizeOfImageOff = opt + 56;
  const sizeOfHeadersOff = opt + 60;
  const sizeOfInitDataOff = opt + 8;
  const sectionAlign = exe.readUInt32LE(opt + 32);
  const fileAlign = exe.readUInt32LE(opt + 36);
  const numDirs = exe.readUInt32LE(opt + 108);
  const dirOff = opt + 112;
  if (numDirs < 3) throw new Error('no resource data directory slot');
  if (exe.readUInt32LE(dirOff + 2 * 8) !== 0) throw new Error('resources already present');

  const tableEnd = opt + optSize;
  const sectionTableOff = tableEnd;
  const headersEnd = sectionTableOff + numSections * SECTION_HEADER;
  const sizeOfHeaders = exe.readUInt32LE(sizeOfHeadersOff);
  if (headersEnd + SECTION_HEADER > sizeOfHeaders) throw new Error('no room in headers for another section');

  const icoImages = readIco(icoPath);
  const ids = icoImages.map((_, i) => i + 1);
  const groups = [
    { type: 3, items: icoImages.map((im, i) => ({ id: ids[i], data: im.data })) },
    { type: 14, items: [{ id: 1, data: buildGroupIcon(icoImages, ids) }] }
  ];
  if (versionInfo) {
    groups.push({ type: 16, items: [{ id: 1, data: buildVersionInfo(versionInfo) }] });
  }
  const rs = buildResourceSection(groups);

  // section geometry
  const newVA = align(exe.readUInt32LE(sizeOfImageOff), sectionAlign);
  const rawSize = align(rs.size, fileAlign);
  const rawPointer = align(exe.length, fileAlign);

  const padded = Buffer.alloc(rawSize);
  rs.data.copy(padded, 0);

  // patch data entries to absolute RVAs: rebuild with a base offset
  // (walk the same layout: every OffsetToData points inside the section)
  const out = Buffer.concat([exe, padded]);
  const patchBase = rawPointer;

  // fix RVAs inside the data entries: they were written as section offsets
  // scan the resource tree we just produced and add newVA - 0 (they are offsets)
  // The tree is deterministic, so re-run the builder with the base applied.
  const rsFixed = buildResourceSection(groups);
  const patched = rebaseOffsets(rsFixed.data, newVA);
  patched.copy(out, rawPointer);

  // section header
  const sh = rawPointer - padded.length + rawSize; // not used
  void sh;
  const hdr = Buffer.alloc(SECTION_HEADER);
  hdr.write('.rsrc\0\0\0', 0, 'ascii');
  hdr.writeUInt32LE(rs.size, 8);              // VirtualSize
  hdr.writeUInt32LE(newVA, 12);               // VirtualAddress
  hdr.writeUInt32LE(rawSize, 16);             // SizeOfRawData
  hdr.writeUInt32LE(rawPointer, 20);          // PointerToRawData
  hdr.writeUInt32LE(0, 24);                   // relocs
  hdr.writeUInt32LE(0, 28);                   // line numbers
  hdr.writeUInt16LE(0, 32);
  hdr.writeUInt16LE(0, 34);
  hdr.writeUInt32LE(0x40000040, 36);          // initialized data | read
  hdr.copy(out, sectionTableOff + numSections * SECTION_HEADER);

  out.writeUInt16LE(numSections + 1, coff + 2);
  out.writeUInt32LE(newVA + align(rs.size, sectionAlign), sizeOfImageOff);
  out.writeUInt32LE(out.readUInt32LE(sizeOfInitDataOff) + rawSize, sizeOfInitDataOff);
  out.writeUInt32LE(newVA, dirOff + 2 * 8);              // resource RVA
  out.writeUInt32LE(rs.size, dirOff + 2 * 8 + 4);        // resource size

  return out;

  /* data entries hold section-relative offsets; convert to RVAs */
  function rebaseOffsets(section, baseRva) {
    const buf = Buffer.from(section);
    const walk = (dirOff, isLeaf) => {
      const named = buf.readUInt16LE(dirOff + 12);
      const ids = buf.readUInt16LE(dirOff + 14);
      for (let i = 0; i < named + ids; i++) {
        const e = dirOff + 16 + i * 8;
        const target = buf.readUInt32LE(e + 4);
        if (target & 0x80000000) {
          walk(target & 0x7fffffff, false);
        } else {
          buf.writeUInt32LE(buf.readUInt32LE(target) + baseRva, target);
        }
      }
    };
    walk(0, false);
    return buf;
  }
}

function dumpResources(exeBuf) {
  const exe = Buffer.from(exeBuf);
  const peOff = exe.readUInt32LE(0x3c);
  const coff = peOff + 4;
  const numSections = exe.readUInt16LE(coff + 2);
  const optSize = exe.readUInt16LE(coff + 16);
  const opt = coff + 20;
  const numDirs = exe.readUInt32LE(opt + 108);
  const dirOff = opt + 112;
  const resourceRva = numDirs > 2 ? exe.readUInt32LE(dirOff + 16) : 0;
  const resourceSize = numDirs > 2 ? exe.readUInt32LE(dirOff + 20) : 0;
  const sectionTable = opt + optSize;
  const sections = [];
  for (let i = 0; i < numSections; i++) {
    const o = sectionTable + i * 40;
    sections.push({
      name: exe.subarray(o, o + 8).toString('ascii').replace(/\0+$/, ''),
      vsize: exe.readUInt32LE(o + 8),
      va: exe.readUInt32LE(o + 12),
      rawSize: exe.readUInt32LE(o + 16),
      rawPtr: exe.readUInt32LE(o + 20),
      chars: exe.readUInt32LE(o + 36)
    });
  }
  const rvaToOff = (rva) => {
    for (const s of sections) {
      if (rva >= s.va && rva < s.va + Math.max(s.vsize, s.rawSize)) return s.rawPtr + (rva - s.va);
    }
    return -1;
  };
  const out = { sections, resourceRva, resourceSize, types: [] };
  if (!resourceRva) return out;
  const base = rvaToOff(resourceRva);
  if (base < 0) throw new Error('resource RVA outside sections');
  const readDir = (off) => {
    const named = exe.readUInt16LE(off + 12);
    const ids = exe.readUInt16LE(off + 14);
    const entries = [];
    for (let i = 0; i < named + ids; i++) {
      const e = off + 16 + i * 8;
      const name = exe.readUInt32LE(e);
      const target = exe.readUInt32LE(e + 4);
      entries.push({ name, target, isDir: !!(target & 0x80000000) });
    }
    return entries;
  };
  for (const t of readDir(base)) {
    const type = { id: t.name, children: [] };
    for (const c of readDir(base + (t.target & 0x7fffffff))) {
      for (const l of readDir(base + (c.target & 0x7fffffff))) {
        const dataEntry = base + l.target;
        const dataRva = exe.readUInt32LE(dataEntry);
        const size = exe.readUInt32LE(dataEntry + 4);
        const off = rvaToOff(dataRva);
        type.children.push({ id: c.name, lang: l.name, size, offset: off, rva: dataRva });
      }
    }
    out.types.push(type);
  }
  return out;
}

module.exports = { injectResources, dumpResources, readIco, buildResourceSection, buildGroupIcon, buildVersionInfo };

if (require.main === module) {
  const [cmd, exePath, icoPath] = process.argv.slice(2);
  if (cmd === 'dump') {
    const r = dumpResources(fs.readFileSync(exePath));
    console.log(JSON.stringify(r, null, 2));
  } else if (cmd === 'inject') {
    const out = injectResources(fs.readFileSync(exePath), icoPath);
    fs.writeFileSync(exePath, out);
    console.log('injected', exePath, out.length, 'bytes');
  } else {
    console.log('usage: node tools/pe.js dump <exe> | inject <exe> <ico>');
    process.exit(2);
  }
}
