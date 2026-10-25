#!/usr/bin/env python3
"""
fontgen.py -- minimal TrueType glyph rasterizer + atlas baker.
Parses glyf/loca/cmap/hmtx/head/maxp tables, flattens quadratic contours,
scanline-fills with 4x4 supersampling and writes an anti-aliased alpha atlas
plus a .nfp metrics file consumed by the engine's Painter.

Usage: fontgen.py <font.ttf> <outdir> <basename> <size>
Writes <outdir>/<basename>_<size>.png and .nfp
"""
import sys, zlib, struct

FIRST, LAST = 32, 126

def u16(b, o): return struct.unpack_from('>H', b, o)[0]
def s16(b, o): return struct.unpack_from('>h', b, o)[0]
def u32(b, o): return struct.unpack_from('>I', b, o)[0]

class TTF:
    def __init__(self, data):
        self.d = data
        ntab = u16(data, 4)
        self.tabs = {}
        for i in range(ntab):
            o = 12 + 16 * i
            tag = data[o:o+4].decode('latin-1')
            self.tabs[tag] = (u32(data, o+8), u32(data, o+12))
        self.unitsPerEm = u16(data, self.tabs['head'][0] + 18)
        self.locaLong = s16(data, self.tabs['head'][0] + 50) == 1
        self.numGlyphs = u16(data, self.tabs['maxp'][0] + 4)
        self.buildCmap()

    def buildCmap(self):
        d = self.d
        base = self.tabs['cmap'][0]
        n = u16(d, base + 2)
        best = None
        for i in range(n):
            pid = u16(d, base + 4 + 8*i)
            eid = u16(d, base + 6 + 8*i)
            off = u32(d, base + 8 + 8*i)
            fmt = u16(d, base + off)
            score = {0: 1, 3: 2}.get(pid, 0)
            if fmt == 4 and (best is None or score >= best[0]):
                best = (score, base + off)
        self.cmap4 = best[1] if best else None
        if not self.cmap4:
            raise RuntimeError('no format 4 cmap')

    def glyphIndex(self, cp):
        d = self.d
        c = self.cmap4
        segX2 = u16(d, c + 6)
        seg = segX2 // 2
        endO = c + 14
        startO = endO + segX2 + 2
        deltaO = startO + segX2
        rangeO = deltaO + segX2
        for i in range(seg):
            end = u16(d, endO + 2*i)
            if cp <= end:
                start = u16(d, startO + 2*i)
                if cp < start:
                    return 0
                delta = s16(d, deltaO + 2*i)
                ro = u16(d, rangeO + 2*i)
                if ro == 0:
                    return (cp + delta) & 0xFFFF
                gi = u16(d, rangeO + 2*i + ro + 2*(cp - start))
                if gi == 0: return 0
                return (gi + delta) & 0xFFFF
        return 0

    def loca(self, gi):
        d = self.d
        o = self.tabs['loca'][0]
        if self.locaLong:
            a = u32(d, o + 4*gi); b = u32(d, o + 4*gi + 4)
        else:
            a = u16(d, o + 2*gi) * 2; b = u16(d, o + 2*gi + 2) * 2
        return self.tabs['glyf'][0] + a, b - a

    def advance(self, gi):
        d = self.d
        n = u16(d, self.tabs['hhea'][0] + 34)
        o = self.tabs['hmtx'][0] + 4 * min(gi, n - 1)
        return u16(d, o)

    def outline(self, gi):
        """Return list of contours (list of (x,y)) in font units, plus advance."""
        d = self.d
        off, ln = self.loca(gi)
        if ln == 0:
            return [], self.advance(gi)
        nc = s16(d, off)
        if nc < 0:
            return [], self.advance(gi)   # composite glyphs unsupported (not needed for ASCII)
        p = off + 10
        endPts = [u16(d, p + 2*i) for i in range(nc)]
        p += 2 * nc
        npts = endPts[-1] + 1 if nc else 0
        ilen = u16(d, p); p += 2 + ilen
        flags = []
        while len(flags) < npts:
            f = d[p]; p += 1
            flags.append(f)
            if f & 8:
                r = d[p]; p += 1
                flags.extend([f] * r)
        flags = flags[:npts]
        xs = []
        v = 0
        for f in flags:
            if f & 2:
                dx = d[p]; p += 1
                v += dx if f & 16 else -dx
            elif not (f & 16):
                v += s16(d, p); p += 2
            xs.append(v)
        ys = []
        v = 0
        for f in flags:
            if f & 4:
                dy = d[p]; p += 1
                v += dy if f & 32 else -dy
            elif not (f & 32):
                v += s16(d, p); p += 2
            ys.append(v)
        contours = []
        s = 0
        for e in endPts:
            pts = [(xs[i], ys[i], bool(flags[i] & 1)) for i in range(s, e + 1)]
            s = e + 1
            if len(pts) < 2:
                continue
            out = []
            n = len(pts)
            # build an explicit on/off sequence starting on-curve
            if all(not q[2] for q in pts):
                mid = ((pts[0][0] + pts[1][0]) / 2.0, (pts[0][1] + pts[1][1]) / 2.0)
                pts = [(mid[0], mid[1], True)] + pts
            if not pts[0][2]:
                mid = ((pts[-1][0] + pts[0][0]) / 2.0, (pts[-1][1] + pts[0][1]) / 2.0)
                pts = [(mid[0], mid[1], True)] + pts
            n = len(pts)
            cur = (pts[0][0], pts[0][1])
            seg = []
            i = 1
            while i <= n:
                q = pts[i % n]
                if q[2]:
                    seg.append((q[0], q[1])); i += 1
                else:
                    nxt = pts[(i + 1) % n]
                    seg.append((nxt[0], nxt[1])); i += 2
                # seg holds [control?, endpoint]
                if len(seg) == 2:
                    P1, P2 = seg
                    for t in (0.25, 0.5, 0.75):
                        mt = 1 - t
                        out.append((mt*mt*cur[0] + 2*mt*t*P1[0] + t*t*P2[0],
                                    mt*mt*cur[1] + 2*mt*t*P1[1] + t*t*P2[1]))
                    out.append(P2)
                elif len(seg) == 1:
                    P2 = seg[0]
                    out.append(P2)
                seg = []
                cur = out[-1] if out else cur
            contours.append(out)
        return contours, self.advance(gi)


def flatten(contours, steps=10):
    """Convert quadratic on-curve polygons (approximation of quadratic B-splines)."""
    polys = []
    for pts in contours:
        if len(pts) < 3:
            continue
        polys.append(pts)
    return polys


def rasterize(contours, bx0, by1, W, H, scale, ss=4):
    """Scanline fill with supersampling. Returns alpha bytes (W*H)."""
    # build edge list in pixel space
    edges = []
    for pts in contours:
        n = len(pts)
        for i in range(n):
            x0, y0 = pts[i]
            x1, y1 = pts[(i + 1) % n]
            px0 = (x0 - bx0) * scale + 1.0; py0 = (by1 - y0) * scale + 1.0
            px1 = (x1 - bx0) * scale + 1.0; py1 = (by1 - y1) * scale + 1.0
            if py0 == py1:
                continue
            wdir = 1 if py1 > py0 else -1
            if py0 > py1:
                px0, py0, px1, py1 = px1, py1, px0, py0
            edges.append((py0, py1, px0, px1, wdir))
    if not edges:
        return bytes(W * H)
    acc = [0] * (W * H)
    inv = 1.0 / ss
    for sy in range(H * ss):
        py = sy * inv + 0.5 * inv
        xs = []
        for (y0, y1, x0, x1, wd) in edges:
            if y0 <= py < y1:
                t = (py - y0) / (y1 - y0)
                xs.append((x0 + (x1 - x0) * t, wd))
        if not xs:
            continue
        xs.sort()
        wind = 0
        row = (sy // ss) * W
        for k in range(len(xs) - 1):
            wind += xs[k][1]
            if wind == 0:
                continue
            a = xs[k][0]; b = xs[k + 1][0]
            ia = max(0, int(round(a * ss)))
            ib = min(W * ss - 1, int(round(b * ss)))
            for px in range(ia, ib + 1):
                acc[row + px // ss] += 1
    norm = 255.0 / (ss * ss)
    out = bytearray(W * H)
    for i, v in enumerate(acc):
        out[i] = 0 if v <= 0 else min(255, int(v * norm + 0.5))
    return bytes(out)


def write_png(path, W, H, gray):
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        raw.extend(gray[y * W:(y + 1) * W])
    comp = zlib.compress(bytes(raw), 9)
    def chunk(t, data):
        return (struct.pack('>I', len(data)) + t + data +
                struct.pack('>I', zlib.crc32(t + data) & 0xFFFFFFFF))
    ihdr = struct.pack('>IIBBBBB', W, H, 8, 0, 0, 0, 0)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDAT', comp) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def main():
    ttf_path, outdir, base, size = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
    font = TTF(open(ttf_path, 'rb').read())
    scale = size / float(font.unitsPerEm)
    asc = int(round(0.78 * font.unitsPerEm * scale))
    desc = int(round(0.22 * font.unitsPerEm * scale))
    lineH = int(round(1.28 * size))

    glyphs = []
    for cp in range(FIRST, LAST + 1):
        gi = font.glyphIndex(cp)
        contours, adv = font.outline(gi)
        # tight bbox
        if contours:
            xs = [p[0] for c in contours for p in c]
            ys = [p[1] for c in contours for p in c]
            bx0, bx1 = min(xs), max(xs)
            by0, by1 = min(ys), max(ys)
        else:
            bx0 = bx1 = by0 = by1 = 0
        w = int(round((bx1 - bx0) * scale)) + 2
        h = int(round((by1 - by0) * scale)) + 2
        alpha = rasterize(contours, bx0, by1, max(w, 1), max(h, 1), scale) if contours else bytes(max(w, 1) * max(h, 1))
        ox = int(round(bx0 * scale)) - 1
        oy = asc - int(round(by1 * scale)) - 1
        if cp == 32:
            alpha = b''; w = h = 0; ox = 0; oy = 0
        glyphs.append((cp, w, h, ox, oy, int(round(adv * scale)), alpha))

    # pack into atlas: simple shelf packing
    AW = 512
    x = y = 0
    rowh = 0
    placed = []
    for g in glyphs:
        cp, w, h, ox, oy, adv, alpha = g
        if w > 0:
            if x + w + 1 > AW:
                x = 0; y += rowh + 1; rowh = 0
            placed.append((cp, x, y, w, h, ox, oy, adv, alpha))
            x += w + 1
            rowh = max(rowh, h)
        else:
            placed.append((cp, 0, 0, 0, 0, ox, oy, adv, b''))
    AH = y + rowh + 2
    AH = max(8, AH)
    atlas = bytearray(AW * AH)
    for (cp, px, py, w, h, ox, oy, adv, alpha) in placed:
        for gy in range(h):
            o = (py + gy) * AW + px
            atlas[o:o + w] = alpha[gy * w:(gy + 1) * w]

    write_png('%s/%s_%d.png' % (outdir, base, size), AW, AH, bytes(atlas))
    with open('%s/%s_%d.nfp' % (outdir, base, size), 'w') as f:
        f.write('font %s\n' % base)
        f.write('size %d %d %d %d\n' % (size, asc, desc, lineH))
        for (cp, px, py, w, h, ox, oy, adv, alpha) in placed:
            f.write('glyph %d %d %d %d %d %d %d %d\n' % (cp, px, py, w, h, ox, oy, adv))
    print('fontgen: %s %s %d -> atlas %dx%d, %d glyphs' % (ttf_path, base, size, AW, AH, len(placed)))


if __name__ == '__main__':
    main()
