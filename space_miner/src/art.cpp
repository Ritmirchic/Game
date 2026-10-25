// ============================================================
// SPACE MINER  -  art.cpp
// Parametric construction of all game sprites.
// ============================================================
#include "art.h"

namespace sm {

using tex::Poly;
using tex::PolySet;

static const float ROCK_SPRITE_R = 100.f;   // radius used when painting rocks

// ---------------------------------------------------------------- helpers
static void emissiveVent(Canvas& c, Vec2 a, Vec2 b, Color col, float width) {
    c.line(a, b, withAlpha(col, 200), width);
    c.line(a, b, withAlpha(scaleColor(col, 1.6f), 90), width * 3.f);
}

static void warningStripes(Canvas& c, float x, float y, float w, float h, float ang) {
    Vec2 dir(cosf(ang), sinf(ang));
    Vec2 n = perp(dir);
    for (float t = 0; t < w; t += 7.f) {
        Vec2 a = Vec2(x, y) + dir * t;
        Vec2 b = Vec2(x, y) + dir * (t + 3.5f);
        Color col = rgba(240, 190, 60, 210);
        c.line(a, b, col, h);
    }
    (void)n;
}

// ================================================================ ships
// Ship art convention: nose points +X, sprite centre = hull centre.
void buildShipArt(ShipArt& out, const ShipDef& def, int index) {
    const int S = 320;
    out.body.resize(S, S);
    out.body.clear(0);
    out.engines.clear();
    out.hardpoints.clear();
    const float cx = S * .5f, cy = S * .5f;
    float L = 100.f;
    switch (index) {
        case SHIP_PROSPECTOR: L = 95.f; break;
        case SHIP_HARVESTER:  L = 104.f; break;
        case SHIP_LANCER:     L = 108.f; break;
        case SHIP_VOIDCRAFT:  L = 116.f; break;
    }
    auto P = [&](float x, float y) { return Vec2(cx + x * L, cy + y * L); };

    // reference half-width profile of the fuselage
    float noseTip = 1.00f, noseW = .13f, shoulder = .30f, midW = .34f, tailW = .26f, tailX = -.80f;
    if (index == SHIP_HARVESTER) { noseTip = .92f; noseW = .20f; shoulder = .40f; midW = .44f; tailW = .34f; }
    if (index == SHIP_LANCER)    { noseTip = 1.04f; noseW = .10f; shoulder = .26f; midW = .28f; tailW = .22f; }
    if (index == SHIP_VOIDCRAFT) { noseTip = 1.02f; noseW = .17f; shoulder = .34f; midW = .40f; tailW = .30f; }

    tex::HullStyle hs;
    hs.base = index == SHIP_VOIDCRAFT ? rgba(104, 132, 140) : rgba(132, 142, 158);
    hs.dark = index == SHIP_VOIDCRAFT ? rgba(12, 26, 32) : rgba(22, 28, 38);
    hs.accent = def.accent;
    hs.panelSize = index == SHIP_HARVESTER ? 15.f : 11.f;
    hs.seed = 100 + index * 17;
    hs.rim = .95f;
    hs.ao = .44f;
    hs.bevel = 4.f;
    hs.noiseScale = .05f;
    hs.greebleCount = index == SHIP_HARVESTER ? 52 : 36;
    hs.greebleDensity = .7f;

    std::vector<tex::PolySet> layers;
    // ---- wing plate (drawn first so the fuselage overlaps it)
    {
        tex::PolySet wings;
        float wingSpan = (index == SHIP_LANCER) ? .62f : (index == SHIP_HARVESTER ? .40f : .50f);
        float wingRoot = .22f;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            tex::Poly w = {
                P(wingRoot, sgn * shoulder * .55f),
                P(.02f, sgn * (shoulder + wingSpan * .55f)),
                P(-.30f, sgn * (shoulder + wingSpan)),
                P(-.62f, sgn * (shoulder + wingSpan * .92f)),
                P(-.66f, sgn * shoulder * .7f),
                P(-.36f, sgn * shoulder * .82f),
            };
            if (sgn > 0) std::reverse(w.begin(), w.end());   // keep winding consistent
            wings.push_back(w);
        }
        layers.push_back(wings);
    }
    // ---- main fuselage
    {
        tex::Poly h = {
            P(noseTip, 0),
            P(.62f, -noseW * .75f),
            P(.34f, -noseW * 1.35f),
            P(.18f, -shoulder * .78f),
            P(-.14f, -midW),
            P(-.48f, -midW * .95f),
            P(-.72f, -tailW * .9f),
            P(tailX, -tailW * .62f),
            P(tailX - .04f, 0),
            P(tailX, tailW * .62f),
            P(-.72f, tailW * .9f),
            P(-.48f, midW * .95f),
            P(-.14f, midW),
            P(.18f, shoulder * .78f),
            P(.34f, noseW * 1.35f),
            P(.62f, noseW * .75f),
        };
        layers.push_back(tex::PolySet{h});
    }
    // ---- dorsal spine
    {
        tex::Poly spine = {P(.52f, -.045f), P(-.10f, -.075f), P(-.62f, -.075f), P(-.62f, .075f),
                           P(-.10f, .075f), P(.52f, .045f)};
        layers.push_back(tex::PolySet{spine});
    }
    // ---- engine nacelles
    int engineCount = def.engineCount;
    std::vector<float> nacY;
    if (index == SHIP_HARVESTER) nacY = {-.44f, .44f};
    else if (index == SHIP_LANCER) nacY = {-.40f, .40f};
    else if (index == SHIP_VOIDCRAFT) nacY = {-.46f, 0.f, .46f};
    else nacY = {-.38f, .38f};
    {
        tex::PolySet nac;
        for (float y : nacY) {
            if (y == 0.f) continue;
            float r = (index == SHIP_HARVESTER) ? .13f : .10f;
            nac.push_back({P(-.34f, y - r * .7f), P(-.94f, y - r), P(-1.00f, y), P(-.94f, y + r),
                           P(-.34f, y + r * .7f)});
        }
        layers.push_back(nac);
    }
    if (index == SHIP_HARVESTER) {
        tex::PolySet tanks;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            tanks.push_back({P(.10f, sgn * (midW + .06f)), P(-.44f, sgn * (midW + .10f)),
                             P(-.56f, sgn * (midW + .02f)), P(-.30f, sgn * (midW - .04f)),
                             P(.06f, sgn * (midW - .02f))});
        }
        layers.push_back(tanks);
    }
    if (index == SHIP_LANCER) {
        tex::PolySet guns;
        for (int sgn = -1; sgn <= 1; sgn += 2)
            guns.push_back({P(.92f, sgn * .10f), P(.20f, sgn * .16f), P(.20f, sgn * .06f), P(.90f, sgn * .03f)});
        layers.push_back(guns);
    }
    if (index == SHIP_VOIDCRAFT) {
        tex::PolySet prongs;
        for (int sgn = -1; sgn <= 1; sgn += 2)
            prongs.push_back({P(1.10f, sgn * .34f), P(.46f, sgn * .30f), P(.46f, sgn * .21f), P(1.06f, sgn * .25f)});
        layers.push_back(prongs);
    }

    // merge every layer into one polygon set for a single seamless hull paint
    tex::PolySet all;
    for (auto& l : layers) for (auto& p : l) all.push_back(p);
    tex::paintHull(out.body, all, hs);

    // ---- cockpit canopy near the nose
    {
        std::vector<Vec2> canopy = {P(.58f, 0), P(.40f, -noseW * 1.15f), P(.24f, -noseW * 1.05f),
                                    P(.24f, noseW * 1.05f), P(.40f, noseW * 1.15f)};
        for (auto& p : canopy) p = {p.x, p.y};
        std::vector<float> mask;
        tex::rasterizeMask(tex::PolySet{canopy}, S, S, mask, 3);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                float m = mask[(size_t)y * S + x];
                if (m <= .01f) continue;
                float t = saturate((x - (cx + .24f * L)) / (.34f * L));
                Color g = mixColor(rgba(16, 34, 52), def.accent, .18f + t * .55f);
                float refl = saturate(1.f - fabsf((y - cy + (x - cx) * .30f) / (L * .045f)));
                g = addColor(g, scaleColor(rgba(200, 244, 255), refl * .40f * t));
                g = scaleColor(g, .85f + .3f * (1.f - fabsf(t * 2 - 1)));
                out.body.blend(x, y, withAlpha(g, (int)(m * 242)));
            }
        for (size_t i = 0; i < canopy.size(); i++)
            out.body.line(canopy[i], canopy[(i + 1) % canopy.size()], rgba(24, 34, 46, 200), 1.5f);
    }

    // ---- accent racing stripe along the spine
    out.body.line(P(.50f, -.02f), P(-.70f, -.09f), withAlpha(def.accent, 190), 2.6f);
    out.body.line(P(.50f, .02f), P(-.70f, .09f), withAlpha(scaleColor(def.accent, .8f), 130), 1.6f);
    // panel hatch details
    for (int i = 0; i < 3; i++) {
        float x0 = -.46f + i * .16f;
        out.body.rectOutline(cx + x0 * L, cy - .11f * L, .10f * L, .22f * L, 1.f, rgba(30, 38, 48, 120));
    }
    // hull number decal
    for (int i = 0; i < 5; i++)
        out.body.fillRect((int)(cx + (-.30f + i * .035f) * L), (int)(cy + .16f * L), (int)(.014f * L), (int)(.05f * L),
                          rgba(232, 240, 250, 150));

    // ---- engine nozzles
    auto addNozzle = [&](Vec2 pos, float radius, Color col) {
        out.body.circleFill(pos.x, pos.y, radius, rgba(126, 138, 154, 255));
        out.body.ring(pos.x, pos.y, radius, 1.6f, rgba(214, 226, 240, 210));
        out.body.circleFill(pos.x + 1, pos.y, radius * .78f, rgba(12, 16, 22, 255));
        for (int i = 0; i < 6; i++) {
            float a = i * TAU / 6;
            out.body.line(pos + Vec2(cosf(a) * radius * .25f, sinf(a) * radius * .25f),
                          pos + Vec2(cosf(a) * radius * .72f, sinf(a) * radius * .72f), rgba(80, 92, 108, 160), 1.2f);
        }
        out.body.circleFill(pos.x + 1, pos.y, radius * .38f, withAlpha(col, 200));
        out.body.radialGlow(pos.x - radius * .2f, pos.y, radius * 2.4f, withAlpha(col, 120), 1.f);
        EnginePort ep;
        ep.pos = pos - Vec2(cx, cy);
        ep.angle = PI;
        ep.size = radius;
        ep.color = col;
        out.engines.push_back(ep);
    };
    float nz = L * .085f + index * .7f;
    if (index == SHIP_VOIDCRAFT) {
        addNozzle(P(-1.02f, 0.f), nz * 1.2f, rgba(150, 255, 230));
    }
    for (float y : nacY) addNozzle(P(-1.00f, y), nz, rgba(120, 205, 255));
    if (index != SHIP_VOIDCRAFT) addNozzle(P(-.86f, 0.f), nz * .8f, rgba(140, 220, 255));

    // ---- weapon hardpoints
    out.hardpoints.push_back(Vec2(.86f * L, -.10f * L));
    out.hardpoints.push_back(Vec2(.86f * L, .10f * L));
    if (def.hardpoints > 1) {
        out.hardpoints.push_back(Vec2(.40f * L, -.42f * L));
        out.hardpoints.push_back(Vec2(.40f * L, .42f * L));
    }
    out.turretPos = Vec2(-.06f * L, 0);

    // ---- mining emitter dome amidships
    {
        Vec2 tp = P(-.02f, 0);
        out.body.circleFill(tp.x, tp.y, L * .15f, rgba(30, 38, 50, 255));
        out.body.ring(tp.x, tp.y, L * .15f, 1.8f, rgba(196, 212, 230, 200));
        out.body.circleFill(tp.x, tp.y, L * .10f, scaleColor(def.accent, .30f));
        out.body.ring(tp.x, tp.y, L * .07f, 1.2f, withAlpha(def.accent, 220));
        out.body.radialGlow(tp.x, tp.y, L * .26f, withAlpha(def.accent, 130), 1.f);
        out.body.circleFill(tp.x, tp.y, L * .025f, rgba(255, 255, 255, 210));
    }
    // ---- nav lights
    {
        Vec2 port = P(-.20f, .30f), star = P(-.20f, -.30f);
        out.body.circleFill(port.x, port.y, 2.6f, rgba(255, 84, 72, 255));
        out.body.radialGlow(port.x, port.y, 12.f, withAlpha(rgba(255, 84, 72), 150), 1.f);
        out.body.circleFill(star.x, star.y, 2.6f, rgba(90, 255, 140, 255));
        out.body.radialGlow(star.x, star.y, 12.f, withAlpha(rgba(90, 255, 140), 150), 1.f);
    }
    // ---- running lights along the wing edges
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        for (int i = 0; i < 4; i++) {
            Vec2 p = P(-.10f - i * .12f, sgn * (shoulder + .30f + i * .05f));
            out.body.circleFill(p.x, p.y, 1.6f, withAlpha(def.accent, 220));
        }
    }

    out.spriteRadius = L * 1.06f;

    // ---- separate mining turret head sprite
    out.turret.resize(64, 64);
    out.turret.clear(0);
    {
        Canvas& t = out.turret;
        t.circleFill(22, 32, 12, rgba(38, 48, 60, 255));
        t.ring(22, 32, 12, 1.6f, rgba(186, 202, 220, 210));
        t.rectOutline(22, 26, 26, 12, 1.f, rgba(150, 164, 182, 210));
        t.fillRect(24, 27, 24, 10, rgba(96, 108, 124, 255));
        t.fillRect(46, 28, 8, 8, rgba(64, 74, 88, 255));
        t.fillRect(24, 30, 20, 4, withAlpha(def.accent, 190));
        t.fillRect(48, 30, 5, 4, withAlpha(def.accent, 240));
        t.radialGlow(52, 32, 13, withAlpha(def.accent, 150), 1.f);
        t.circleFill(22, 32, 5, scaleColor(def.accent, 1.2f));
        t.circleFill(22, 32, 2, rgba(255, 255, 255, 220));
    }
}

// ================================================================ enemies
void buildEnemyArt(Canvas& body, Canvas& glow, int type) {
    const int S = 260;
    body.resize(S, S);
    body.clear(0);
    glow.resize(S, S);
    glow.clear(0);
    float cx = S * .5f, cy = S * .5f;
    const EnemyDef& def = ENEMIES[type];

    tex::HullStyle hs;
    hs.seed = 300 + type * 13;
    hs.accent = def.tint;
    hs.rim = 1.f;
    hs.ao = .42f;
    hs.bevel = 3.2f;
    hs.panelSize = 9.f;
    hs.greebleCount = 14;
    hs.noiseScale = .07f;
    tex::PolySet polys;
    float k = (type == EN_SCOUT || type == EN_PLASMA) ? 1.42f : (type == EN_SENTINEL ? 1.12f : 1.f);
    auto P = [&](float x, float y) { return Vec2(cx + x * k, cy + y * k); };
    auto quad = [&](float x0, float y0, float x1, float y1, float w) {   // thick bar
        Vec2 a(x0, y0), b(x1, y1);
        Vec2 d = normalize(b - a), n = perp(d) * (w * .5f);
        return tex::Poly{P(x0, y0) + n, P(x1, y1) + n, P(x1, y1) - n, P(x0, y0) - n};
    };

    switch (type) {
        case EN_SCOUT: {
            hs.base = rgba(176, 138, 112);
            hs.dark = rgba(30, 20, 16);
            // insectoid: slim pod, four angled spars
            polys.push_back({P(34, 0), P(20, -9), P(-14, -13), P(-30, -7), P(-30, 7), P(-14, 13), P(20, 9)});
            polys.push_back(quad(6, 8, 34, 30, 6));
            polys.push_back(quad(6, -8, 34, -30, 6));
            polys.push_back(quad(-6, 9, -28, 26, 5));
            polys.push_back(quad(-6, -9, -28, -26, 5));
            // spar tips
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sy = -1; sy <= 1; sy += 2) {
                    Vec2 tip = P(sx > 0 ? 34 : -28, sy * (sx > 0 ? 30 : 26));
                    polys.push_back({tip + Vec2(sx * 6, sy * 6), tip + Vec2(-sx * 4, sy * 9),
                                     tip + Vec2(-sx * 4, sy * 2)});
                }
            break;
        }
        case EN_RAIDER: {
            hs.base = rgba(186, 152, 100);
            hs.dark = rgba(34, 24, 14);
            hs.panelSize = 11.f;
            // pirate skiff: long nose blade, broad swept wings, heavy tail
            polys.push_back({P(62, 2), P(30, -10), P(4, -18), P(-20, -16), P(-44, -10), P(-52, 0),
                             P(-44, 12), P(-14, 18), P(8, 16), P(34, 10)});
            // wings
            polys.push_back({P(6, -12), P(-22, -44), P(-46, -40), P(-34, -12)});
            polys.push_back({P(6, 12), P(-22, 44), P(-46, 40), P(-34, 12)});
            // wing tip blades
            polys.push_back({P(-40, -40), P(-58, -52), P(-50, -32)});
            polys.push_back({P(-40, 40), P(-58, 52), P(-50, 32)});
            // underslung cannon
            polys.push_back(quad(18, 22, 52, 22, 8));
            polys.push_back({P(52, 18), P(62, 20), P(62, 24), P(52, 26)});
            break;
        }
        case EN_HEAVY: {
            hs.base = rgba(152, 158, 170);
            hs.dark = rgba(18, 22, 30);
            hs.panelSize = 13.f;
            hs.greebleCount = 26;
            // armored bunker with twin cannons
            polys.push_back({P(30, -22), P(40, -12), P(40, 12), P(30, 22), P(-20, 26), P(-48, 22),
                             P(-56, 8), P(-56, -8), P(-48, -22), P(-20, -26)});
            polys.push_back({P(-20, -26), P(4, -36), P(20, -30), P(2, -22)});
            polys.push_back({P(-20, 26), P(4, 36), P(20, 30), P(2, 22)});
            // cannon barrels
            polys.push_back(quad(30, -16, 62, -16, 10));
            polys.push_back(quad(30, 16, 62, 16, 10));
            polys.push_back({P(62, -21), P(72, -19), P(72, -13), P(62, -11)});
            polys.push_back({P(62, 21), P(72, 19), P(72, 13), P(62, 11)});
            // armor ribs
            polys.push_back(quad(-44, -24, -44, 24, 7));
            break;
        }
        case EN_PLASMA: {
            hs.base = rgba(140, 106, 152);
            hs.dark = rgba(22, 12, 28);
            hs.panelSize = 8.f;
            // crescent ring, open towards the front
            tex::Poly ring;
            int segs = 22;
            float rin = 22.f, rout = 34.f;
            for (int i = 0; i <= segs; i++) {
                float a = -2.1f + (4.2f * i / segs);
                ring.push_back(P(cosf(a) * rout + 6, sinf(a) * rout));
            }
            for (int i = segs; i >= 0; i--) {
                float a = -2.1f + (4.2f * i / segs);
                ring.push_back(P(cosf(a) * rin + 6, sinf(a) * rin));
            }
            polys.push_back(ring);
            // front claws
            polys.push_back({P(24, -22), P(56, -14), P(50, -4), P(24, -8)});
            polys.push_back({P(24, 22), P(56, 14), P(50, 4), P(24, 8)});
            // rear thruster block
            polys.push_back({P(-24, -12), P(-38, -8), P(-38, 8), P(-24, 12)});
            break;
        }
        default: {  // EN_SENTINEL
            hs.base = rgba(104, 134, 142);
            hs.dark = rgba(8, 22, 26);
            hs.panelSize = 14.f;
            hs.greebleCount = 20;
            tex::Poly hex;
            for (int i = 0; i < 6; i++) {
                float a = i * TAU / 6 + PI / 6;
                hex.push_back({cx + cosf(a) * 30, cy + sinf(a) * 30});
            }
            polys.push_back(hex);
            // four long spars with blade fins
            for (int i = 0; i < 4; i++) {
                float a = i * TAU / 4 + PI / 4;
                Vec2 d(cosf(a), sinf(a)), n = perp(d);
                Vec2 b0 = Vec2(cx, cy) + d * 28.f;
                Vec2 b1 = Vec2(cx, cy) + d * 62.f;
                polys.push_back({b0 + n * 6.f, b1 + n * 4.f, b1 - n * 4.f, b0 - n * 6.f});
                Vec2 t = b1 + n * 16.f;
                polys.push_back({b1 + n * 2.f, t, b1 + d * 14.f - n * 4.f});
            }
            break;
        }
    }
    tex::paintHull(body, polys, hs);

    // ---- emissive details (glow canvas is aligned 1:1 with body)
    auto eye = [&](Vec2 p, float r, Color col) {
        body.circleFill(p.x, p.y, r * 1.7f, rgba(10, 12, 16, 255));
        body.ring(p.x, p.y, r * 1.7f, 1.2f, scaleColor(col, .75f));
        glow.radialGlow(p.x, p.y, r * 4.5f, withAlpha(col, 120), 1.f);
        glow.circleFill(p.x, p.y, r, withAlpha(col, 235));
        glow.circleFill(p.x, p.y, r * .40f, rgba(255, 255, 255, 245));
    };
    auto vent = [&](Vec2 a, Vec2 b, Color col, float w) {
        body.line(a, b, rgba(18, 20, 26, 210), w * 2.6f);
        glow.line(a, b, withAlpha(col, 190), w);
    };
    switch (type) {
        case EN_SCOUT:
            eye(P(22, 0), 6.f, rgba(255, 112, 68));
            for (int sy = -1; sy <= 1; sy += 2) {
                vent(P(30, sy * 27), P(34, sy * 30), rgba(255, 150, 90), 2.4f);
                vent(P(-26, sy * 23), P(-28, sy * 26), rgba(255, 130, 80), 1.8f);
            }
            break;
        case EN_RAIDER:
            eye(P(26, -2), 5.5f, rgba(255, 200, 96));
            vent(P(-44, 6), P(-30, 10), rgba(255, 150, 60), 2.f);
            for (int i = 0; i < 4; i++) glow.fillRect((int)cx - 46 + i * 7, (int)cy - 2, 4, 3, withAlpha(def.tint, 200));
            glow.radialGlow(cx + 58, cy + 22, 16, withAlpha(rgba(255, 190, 90), 140), 1.f);
            break;
        case EN_HEAVY:
            eye(P(24, 0), 7.f, rgba(140, 200, 255));
            body.circleFill(cx + 68, cy - 16, 6, rgba(14, 18, 24, 255));
            glow.circleFill(cx + 68, cy - 16, 4.2f, withAlpha(rgba(160, 215, 255), 220));
            body.circleFill(cx + 68, cy + 16, 6, rgba(14, 18, 24, 255));
            glow.circleFill(cx + 68, cy + 16, 4.2f, withAlpha(rgba(160, 215, 255), 220));
            for (int i = 0; i < 3; i++) vent(P(-52 + i * 3, -18), P(-52 + i * 3, 18), rgba(120, 170, 255), 1.6f);
            break;
        case EN_PLASMA:
            for (int i = 0; i < 3; i++) glow.ring(cx + 6, cy, 26 + i * 5, 1.6f, withAlpha(def.tint, 90 - i * 22));
            glow.circleFill(cx + 6, cy, 16, withAlpha(def.tint, 80));
            glow.circleFill(cx + 6, cy, 9, withAlpha(def.tint, 190));
            glow.circleFill(cx + 6, cy, 4, rgba(255, 245, 255, 235));
            eye(P(40, 0), 4.5f, def.tint);
            break;
        default:
            glow.circleFill(cx, cy, 26, withAlpha(def.tint, 45));
            glow.circleFill(cx, cy, 15, withAlpha(def.tint, 130));
            glow.circleFill(cx, cy, 7, rgba(235, 255, 250, 230));
            glow.ring(cx, cy, 28, 1.4f, withAlpha(def.tint, 150));
            for (int i = 0; i < 4; i++) {
                float a = i * TAU / 4 + PI / 4;
                Vec2 d(cosf(a), sinf(a));
                glow.line(Vec2(cx, cy) + d * 30.f, Vec2(cx, cy) + d * 58.f, withAlpha(def.tint, 110), 1.6f);
                glow.circleFill(cx + d.x * 62.f, cy + d.y * 62.f, 2.6f, withAlpha(def.tint, 220));
            }
            break;
    }
    if (type != EN_SENTINEL && type != EN_PLASMA)
        glow.radialGlow(cx - 34, cy, 24, withAlpha(rgba(120, 180, 255), 90), 1.f);
}

// ================================================================ drones
void buildDroneArt(Canvas& out, float tier) {
    const int S = 120;
    out.resize(S, S);
    out.clear(0);
    float cx = S * .5f, cy = S * .5f;
    Color acc = tier < .5f ? rgba(120, 230, 200) : (tier < 1.5f ? rgba(120, 190, 255) : rgba(255, 210, 120));

    tex::HullStyle hs;
    hs.base = rgba(140, 154, 170);
    hs.dark = rgba(24, 30, 40);
    hs.accent = acc;
    hs.seed = 900 + (int)(tier * 17);
    hs.panelSize = 7.f;
    hs.greebleCount = 12;
    hs.bevel = 3.f;
    PolySet polys;
    // hexagonal chassis
    Poly hex;
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6 + PI / 6;
        float r = (i % 2 == 0) ? 26.f : 22.f;
        hex.push_back({cx + cosf(a) * r, cy + sinf(a) * r});
    }
    polys.push_back(hex);
    // side arms with emitter pods
    polys.push_back({{cx + 20, cy - 12}, {cx + 38, cy - 22}, {cx + 42, cy - 14}, {cx + 24, cy - 6}});
    polys.push_back({{cx + 20, cy + 12}, {cx + 38, cy + 22}, {cx + 42, cy + 14}, {cx + 24, cy + 6}});
    // rear antenna
    polys.push_back({{cx - 20, cy - 3}, {cx - 38, cy - 8}, {cx - 38, cy + 8}, {cx - 20, cy + 3}});
    tex::paintHull(out, polys, hs);

    // emitter eye at the front
    out.circleFill(cx + 26, cy, 8.f, rgba(14, 18, 24, 255));
    out.ring(cx + 26, cy, 8.f, 1.3f, scaleColor(acc, .8f));
    out.radialGlow(cx + 26, cy, 22, withAlpha(acc, 120), 1.f);
    out.circleFill(cx + 26, cy, 4.f, withAlpha(acc, 240));
    // rotor ring
    out.ring(cx, cy, 17.f, 1.2f, withAlpha(acc, 130));
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6;
        out.line({cx + cosf(a) * 17, cy + sinf(a) * 17}, {cx + cosf(a) * 21, cy + sinf(a) * 21},
                 withAlpha(acc, 150), 1.f);
    }
    // nav lights
    out.circleFill(cx - 24, cy, 2.f, rgba(255, 90, 80, 230));
    out.circleFill(cx + 8, cy - 18, 1.6f, rgba(120, 255, 160, 220));
}

// ================================================================ station
void buildStationArt(Canvas& body, Canvas& glow) {
    const int S = 420;
    body.resize(S, S);
    body.clear(0);
    float cx = S * .5f, cy = S * .5f;
    tex::HullStyle hs;
    hs.base = rgba(150, 158, 172);
    hs.dark = rgba(28, 34, 44);
    hs.accent = rgba(120, 220, 255);
    hs.seed = 4242;
    hs.panelSize = 20.f;
    hs.greebleCount = 80;
    hs.bevel = 6.f;
    hs.rim = 1.f;

    PolySet polys;
    // outer torus ring approximated with a polygon annulus
    auto ringPoly = [&](float rInner, float rOuter, int segs, float a0, float a1) {
        Poly p;
        for (int i = 0; i <= segs; i++) {
            float a = a0 + (a1 - a0) * i / segs;
            p.push_back({cx + cosf(a) * rOuter, cy + sinf(a) * rOuter});
        }
        for (int i = segs; i >= 0; i--) {
            float a = a0 + (a1 - a0) * i / segs;
            p.push_back({cx + cosf(a) * rInner, cy + sinf(a) * rInner});
        }
        return p;
    };
    polys.push_back(ringPoly(128.f, 150.f, 48, 0, TAU));
    // spokes
    for (int i = 0; i < 4; i++) {
        float a = i * TAU / 4 + PI / 4;
        Vec2 d(cosf(a), sinf(a)), n = perp(d);
        polys.push_back({Vec2(cx, cy) + d * 20 + n * 9, Vec2(cx, cy) + d * 132 + n * 7,
                         Vec2(cx, cy) + d * 132 - n * 7, Vec2(cx, cy) + d * 20 - n * 9});
    }
    // central hub
    Poly hub;
    for (int i = 0; i < 8; i++) {
        float a = i * TAU / 8 + PI / 8;
        hub.push_back({cx + cosf(a) * 46, cy + sinf(a) * 46});
    }
    polys.push_back(hub);
    // habitat blocks
    for (int i = 0; i < 8; i++) {
        float a = i * TAU / 8 + .3f;
        float r = 168.f;
        Vec2 p(cx + cosf(a) * r, cy + sinf(a) * r);
        Vec2 d(cosf(a), sinf(a)), n = perp(d);
        polys.push_back({p + n * 13 + d * 20, p - n * 13 + d * 20, p - n * 13 - d * 20, p + n * 13 - d * 20});
    }
    // docking arms (long, pointing up)
    polys.push_back({{cx - 14, cy - 150}, {cx + 14, cy - 150}, {cx + 9, cy - 196}, {cx - 9, cy - 196}});
    polys.push_back({{cx - 46, cy - 196}, {cx + 46, cy - 196}, {cx + 40, cy - 206}, {cx - 40, cy - 206}});
    polys.push_back({{cx - 100, cy - 150}, {cx - 118, cy - 150}, {cx - 112, cy - 176}, {cx - 96, cy - 176}});
    tex::paintHull(body, polys, hs);

    // ---- emissive layer: windows, beacons, docking lights
    glow.resize(S, S);
    glow.clear(0);
    Rng rng;
    rng.seed(999);
    for (int i = 0; i < 220; i++) {
        float a = rng.range(0, TAU);
        float r = rng.range(132.f, 146.f);
        Vec2 p(cx + cosf(a) * r, cy + sinf(a) * r);
        Color c = rng.chance(.7f) ? rgba(255, 220, 150) : rgba(140, 220, 255);
        glow.fillRect((int)p.x, (int)p.y, rng.irange(1, 3), rng.irange(1, 2), withAlpha(c, rng.irange(120, 240)));
    }
    for (int i = 0; i < 40; i++) {
        float a = rng.range(0, TAU);
        float r = rng.range(24.f, 42.f);
        Vec2 p(cx + cosf(a) * r, cy + sinf(a) * r);
        glow.fillRect((int)p.x, (int)p.y, 2, 2, withAlpha(rgba(190, 240, 255), 200));
    }
    // hub core glow
    glow.radialGlow(cx, cy, 62, withAlpha(rgba(120, 220, 255), 90), 1.f);
    glow.circleFill(cx, cy, 16, rgba(180, 240, 255, 230));
    // docking port light bar
    glow.fillRect((int)cx - 40, (int)cy - 199, 80, 3, withAlpha(rgba(120, 255, 200), 200));
    glow.radialGlow(cx, cy - 200, 44, withAlpha(rgba(120, 255, 200), 90), 1.f);
    // beacons
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6 + .2f;
        Vec2 p(cx + cosf(a) * 155, cy + sinf(a) * 155);
        glow.circleFill(p.x, p.y, 3.f, withAlpha(rgba(255, 90, 90), 230));
        glow.radialGlow(p.x, p.y, 14, withAlpha(rgba(255, 80, 80), 90), 1.f);
    }
    // solar panel shimmer
    glow.line({cx - 118, cy - 163}, {cx - 96, cy - 163}, withAlpha(rgba(140, 200, 255), 120), 2.f);
}

// ================================================================ planets
void buildPlanet(Canvas& body, Canvas& glow, int sector, uint32_t seed) {
    // The canvas is larger than the disc so ring systems fit without clipping.
    const int S = 560;
    body.resize(S, S);
    body.clear(0);
    glow.resize(S, S);
    glow.clear(0);
    float cx = S * .5f, cy = S * .5f, R = S * .225f;
    const SectorDef& sd = SECTORS[sector];
    Vec2 light = normalize(Vec2(-.62f, -.72f));

    // palette per sector
    Color c1, c2, c3, atmo;
    switch (sector) {
        case 0: c1 = rgba(150, 92, 52); c2 = rgba(84, 46, 30); c3 = rgba(210, 150, 96); atmo = rgba(255, 170, 110); break;
        case 1: c1 = rgba(66, 96, 190); c2 = rgba(28, 34, 96); c3 = rgba(150, 210, 255); atmo = rgba(130, 200, 255); break;
        case 2: c1 = rgba(104, 108, 118); c2 = rgba(40, 44, 52); c3 = rgba(170, 180, 196); atmo = rgba(180, 200, 230); break;
        case 3: c1 = rgba(150, 40, 120); c2 = rgba(60, 16, 60); c3 = rgba(255, 130, 220); atmo = rgba(255, 120, 210); break;
        default: c1 = rgba(28, 52, 62); c2 = rgba(8, 16, 22); c3 = rgba(90, 220, 200); atmo = rgba(110, 255, 220); break;
    }
    float rot = sector * .7f;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + .5f - cx) / R, dy = (y + .5f - cy) / R;
            float d2 = dx * dx + dy * dy;
            if (d2 >= 1.f) continue;
            float z = sqrtf(1.f - d2);
            Vec2 n(dx, dy);
            float ndl = saturate(sm::dot(n, light) * .8f + z * .55f);
            float lam = powf(ndl, .85f);
            // spherical mapping: longitude from the visible hemisphere only, so
            // there is no singularity in the middle of the disc (a planar atan2
            // pinches all longitudes into the centre point).
            float u = atan2f(dx, z + .002f);
            float lat = asinf(clampf(dy, -1.f, 1.f));
            float bands = sinf(lat * 9.f + fbm2(u * 2.2f + rot, lat * 3.f, 4, 2.f, .5f, seed) * 5.f) * .5f + .5f;
            float turb = fbm2(u * 4.5f + rot + z * 2.f, lat * 5.f, 5, 2.2f, .55f, seed + 3);
            Color surf;
            if (sector == 2 || sector == 4) {
                // cratered rock world
                float cr = worley2(dx * 5.5f + 3.f, dy * 5.5f + 7.f, seed + 11);
                float crater = saturate((cr - .3f) * 3.f);
                surf = mixColor(c2, c1, saturate(turb * 1.3f + .15f));
                surf = mixColor(surf, c1, crater * .4f);
                surf = scaleColor(surf, .82f + .3f * (1.f - crater));
            } else {
                surf = mixColor(c2, c1, saturate(bands * .7f + turb * .5f));
                surf = mixColor(surf, c3, saturate((turb - .55f) * 2.2f) * .55f);
            }
            // terminator with a warm limb
            Color out_c = scaleColor(surf, .04f + 1.05f * lam);
            float fres = powf(1.f - z, 2.6f);
            out_c = addColor(out_c, scaleColor(atmo, fres * saturate(ndl + .25f) * .55f));
            float spec = powf(saturate(sm::dot(Vec2(dx, dy), light) + z * .6f), 32.f);
            if (sector == 1) out_c = addColor(out_c, scaleColor(rgba(220, 245, 255), spec * .5f));
            body.px[(size_t)y * S + x] = withAlpha(out_c, 255);
            // atmosphere halo outside the disc handled below
        }
    // atmosphere glow ring
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + .5f - cx) / R, dy = (y + .5f - cy) / R;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < 1.f || d > 1.22f) continue;
            float t = 1.f - (d - 1.f) / .22f;
            float ang = saturate(sm::dot(normalize(Vec2(dx, dy)), light) * .5f + .5f);
            glow.px[(size_t)y * S + x] = withAlpha(atmo, (int)(powf(t, 2.4f) * 120 * ang));
        }
    // rings for the rocky and crystal worlds
    if (sector == 0 || sector == 1) {
        Color ringHi = mixColor(c3, rgba(255, 255, 255), .25f);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                float dx = (x + .5f - cx) / R, dy = (y + .5f - cy) / R;
                Vec2 p(dx, dy / .20f);
                float d = length(p);
                if (d < 1.28f || d > 2.16f) continue;
                float band = fbm2(d * 12.f, atan2f(p.y, p.x) * .5f, 3, 2.f, .5f, seed + 5);
                // density profile: soft inner edge, a Cassini-style gap, thin outer edge
                float prof = saturate(1.f - fabsf(d - 1.68f) / .48f);
                prof = powf(prof, .7f);
                float gap = 1.f - .85f * expf(-powf((d - 1.86f) / .035f, 2.f));
                float a = saturate(.30f + .70f * band) * prof * gap;
                if (a <= .01f) continue;
                Color rc = mixColor(c1, ringHi, saturate(band * .8f + .2f));
                float d2 = dx * dx + dy * dy;
                int al = (int)(a * 165);
                if (d2 < 1.f) {
                    // only the near half of the ring passes in front of the planet
                    if (dy < 0.f) continue;
                    body.blend(x, y, withAlpha(rc, (int)(al * .85f)));
                } else {
                    body.blend(x, y, withAlpha(rc, al));
                }
            }
    }
    // lightning for plasma worlds
    if (sector == 3) {
        Rng rng;
        rng.seed(seed + 21);
        for (int i = 0; i < 3; i++) {
            float a = rng.range(0, TAU), rr = rng.range(.1f, .8f);
            Vec2 p(cx + cosf(a) * R * rr, cy + sinf(a) * R * rr);
            for (int k = 0; k < 5; k++) {
                Vec2 q = p + Vec2(rng.range(-R * .18f, R * .18f), rng.range(-R * .12f, R * .12f));
                body.line(p, q, withAlpha(rgba(255, 200, 255), 105), 1.1f);
                glow.line(p, q, withAlpha(rgba(255, 130, 240), 55), 2.4f);
                p = q;
            }
        }
    }
    // void rift for the frontier
    if (sector == 4) {
        Vec2 p(cx + R * .25f, cy - R * .15f);
        for (int i = 0; i < 40; i++) {
            float a = i * TAU / 40;
            Vec2 q = p + Vec2(cosf(a) * R * .55f, sinf(a) * R * .10f);
            glow.line(p, q, withAlpha(rgba(150, 255, 230), 60), 1.2f);
        }
        glow.circleFill(p.x, p.y, 8, withAlpha(rgba(180, 255, 240), 220));
    }
}

// ================================================================ debris
void buildDebris(Canvas& out, int sector, uint32_t seed) {
    const int S = 160;
    out.resize(S, S);
    out.clear(0);
    Rng rng;
    rng.seed(seed * 13 + 5);
    tex::HullStyle hs;
    hs.accent = SECTORS[sector].accent;
    hs.seed = seed;
    hs.panelSize = 9.f;
    hs.greebleCount = 16;
    hs.bevel = 3.f;
    hs.base = sector == 2 ? rgba(148, 150, 158) : rgba(120, 118, 112);
    hs.dark = rgba(20, 22, 26);
    PolySet polys;
    int parts = rng.irange(2, 4);
    for (int i = 0; i < parts; i++) {
        Vec2 c = Vec2(S * .5f, S * .5f) + rng.insideCircle(S * .22f);
        float w = rng.range(10.f, 34.f), h = rng.range(8.f, 26.f);
        float rot = rng.range(0, TAU);
        Poly p;
        int n = rng.irange(4, 6);
        for (int k = 0; k < n; k++) {
            float a = k * TAU / n + rot;
            Vec2 off(cosf(a) * w, sinf(a) * h);
            p.push_back(c + off);
        }
        polys.push_back(p);
    }
    tex::paintHull(out, polys, hs);
    // scorch marks
    for (int i = 0; i < 8; i++) {
        Vec2 p = Vec2(S * .5f, S * .5f) + rng.insideCircle(S * .25f);
        out.radialGlow(p.x, p.y, rng.range(6.f, 20.f), withAlpha(rgba(12, 10, 14), 120), 1.f);
    }
    // a few lit windows still alive
    for (int i = 0; i < 5; i++) {
        Vec2 p = Vec2(S * .5f, S * .5f) + rng.insideCircle(S * .22f);
        if (ca(out.get((int)p.x, (int)p.y)) < 20) continue;
        out.fillRect((int)p.x, (int)p.y, 2, 2, withAlpha(rgba(255, 200, 120), 200));
    }
}

// ================================================================ icons
void buildResourceIcon(Canvas& out, int res, int size) {
    out.resize(size, size);
    out.clear(0);
    const ResourceDef& rd = RESOURCES[res];
    float cx = size * .5f, cy = size * .5f;
    Color base = rd.color;
    bool crystalline = (res == RES_CRYSTAL || res == RES_VOIDCRYSTAL || res == RES_QUANTUM || res == RES_IRIDIUM);
    bool gaslike = (res == RES_PLASMA || res == RES_DARKMATTER);
    if (crystalline) {
        // faceted gem
        Poly top = {{cx, cy - size * .40f}, {cx + size * .28f, cy - size * .10f},
                    {cx, cy + size * .34f}, {cx - size * .28f, cy - size * .10f}};
        Poly left = {{cx - size * .28f, cy - size * .10f}, {cx, cy + size * .34f}, {cx - size * .2f, cy + size * .2f}};
        out.polyFill(top.data(), 4, scaleColor(base, .85f));
        out.polyFill(left.data(), 3, scaleColor(base, .55f));
        Poly right = {{cx + size * .28f, cy - size * .10f}, {cx, cy + size * .34f}, {cx + size * .2f, cy + size * .2f}};
        out.polyFill(right.data(), 3, scaleColor(base, 1.15f));
        Poly topf = {{cx, cy - size * .40f}, {cx + size * .28f, cy - size * .10f}, {cx - size * .28f, cy - size * .10f}};
        out.polyFill(topf.data(), 3, scaleColor(base, 1.35f));
        out.line({cx, cy - size * .40f}, {cx, cy + size * .34f}, withAlpha(rgba(255, 255, 255), 120), 1.f);
        out.polyOutline(topf.data(), 3, withAlpha(scaleColor(base, 1.6f), 180), 1.f);
        out.radialGlow(cx, cy, size * .5f, withAlpha(base, 70), 1.f);
    } else if (gaslike) {
        out.radialGlow(cx, cy, size * .48f, withAlpha(base, 150), .7f);
        out.circleFill(cx, cy, size * .26f, withAlpha(base, 210));
        out.circleFill(cx, cy, size * .14f, withAlpha(scaleColor(base, 1.7f), 255));
        for (int i = 0; i < 3; i++) {
            float a = i * TAU / 3;
            out.line({cx, cy}, {cx + cosf(a) * size * .38f, cy + sinf(a) * size * .38f},
                     withAlpha(base, 150), 1.4f);
        }
    } else {
        // rough ore nugget
        Poly nug = {{cx - size * .30f, cy + size * .10f}, {cx - size * .16f, cy - size * .24f},
                    {cx + size * .12f, cy - size * .30f}, {cx + size * .30f, cy - size * .04f},
                    {cx + size * .18f, cy + size * .26f}, {cx - size * .12f, cy + size * .30f}};
        out.polyFill(nug.data(), 6, scaleColor(base, .75f));
        Poly hi = {{cx - size * .16f, cy - size * .24f}, {cx + size * .12f, cy - size * .30f},
                   {cx + size * .06f, cy - size * .04f}, {cx - size * .18f, cy + size * .02f}};
        out.polyFill(hi.data(), 4, scaleColor(base, 1.25f));
        Poly lo = {{cx - size * .30f, cy + size * .10f}, {cx - size * .18f, cy + size * .02f},
                   {cx - size * .12f, cy + size * .30f}};
        out.polyFill(lo.data(), 3, scaleColor(base, .45f));
        out.polyOutline(nug.data(), 6, withAlpha(scaleColor(base, .3f), 220), 1.f);
        out.radialGlow(cx, cy, size * .46f, withAlpha(base, 55), 1.f);
        // sparkle
        out.line({cx + size * .10f, cy - size * .34f}, {cx + size * .14f, cy - size * .20f},
                 withAlpha(rgba(255, 255, 255), 190), 1.f);
    }
}

void buildUpgradeIcon(Canvas& out, int upg, int size) {
    out.resize(size, size);
    out.clear(0);
    float cx = size * .5f, cy = size * .5f;
    Color acc = rgba(120, 220, 255);
    float u = size / 48.f;
    auto L = [&](float x0, float y0, float x1, float y1, Color c, float t = 2.f) {
        out.line({cx + x0 * u, cy + y0 * u}, {cx + x1 * u, cy + y1 * u}, c, t * u);
    };
    Color line = rgba(210, 232, 246);
    switch (upg) {
        case UPG_LASER:
            L(-18, 14, 10, -10, line, 2.4f);
            out.circleFill(cx + 10 * u, cy - 10 * u, 5 * u, withAlpha(acc, 220));
            out.radialGlow(cx + 10 * u, cy - 10 * u, 16 * u, withAlpha(acc, 130), 1.f);
            L(6, 10, 16, 20, line, 2.f);
            break;
        case UPG_SPEED:
            for (int i = 0; i < 3; i++)
                L(-14 + i * 10, -12, -4 + i * 10, 0, line, 2.2f), L(-4 + i * 10, 0, -14 + i * 10, 12, line, 2.2f);
            break;
        case UPG_CARGO:
            out.rectOutline(cx - 14 * u, cy - 12 * u, 28 * u, 24 * u, 1.8f * u, line);
            L(-14, 0, 14, 0, line, 1.4f);
            L(0, -12, 0, 12, line, 1.4f);
            L(-10, -18, 10, -18, withAlpha(acc, 200), 2.f);
            break;
        case UPG_ENGINE:
            out.circleFill(cx - 4 * u, cy, 11 * u, rgba(30, 40, 52, 255));
            out.ring(cx - 4 * u, cy, 11 * u, 1.6f * u, line);
            L(-4, -11, 18, -16, withAlpha(acc, 190), 2.f);
            L(-4, 11, 18, 16, withAlpha(acc, 190), 2.f);
            out.circleFill(cx + 20 * u, cy, 4 * u, withAlpha(acc, 160));
            out.radialGlow(cx + 16 * u, cy, 14 * u, withAlpha(acc, 90), 1.f);
            break;
        case UPG_HULL: {
            Poly p = {{cx, cy - 18 * u}, {cx + 16 * u, cy - 10 * u}, {cx + 12 * u, cy + 12 * u},
                      {cx, cy + 20 * u}, {cx - 12 * u, cy + 12 * u}, {cx - 16 * u, cy - 10 * u}};
            out.polyFill(p.data(), 6, rgba(60, 78, 96, 220));
            out.polyOutline(p.data(), 6, line, 1.8f * u);
            L(-8, 0, 8, 0, withAlpha(acc, 160), 1.4f);
            break;
        }
        case UPG_SHIELD: {
            out.ring(cx, cy, 16 * u, 2.f * u, line);
            out.arc(cx, cy, 16 * u, -2.4f, -.4f, 3.f * u, withAlpha(acc, 230));
            out.circleFill(cx, cy, 5 * u, withAlpha(acc, 120));
            break;
        }
        case UPG_ENERGY: {
            Poly bolt = {{cx + 4 * u, cy - 18 * u}, {cx - 10 * u, cy + 2 * u}, {cx, cy + 2 * u},
                         {cx - 5 * u, cy + 18 * u}, {cx + 11 * u, cy - 2 * u}, {cx + 1 * u, cy - 2 * u}};
            out.polyFill(bolt.data(), 6, withAlpha(acc, 235));
            out.radialGlow(cx, cy, 22 * u, withAlpha(acc, 90), 1.f);
            break;
        }
        case UPG_SCANNER: {
            out.ring(cx, cy, 16 * u, 1.6f * u, line);
            out.line({cx, cy}, {cx + 12 * u, cy - 10 * u}, withAlpha(acc, 220), 2.f * u);
            out.arc(cx, cy, 11 * u, -.9f, .3f, 2.f * u, withAlpha(acc, 120));
            out.circleFill(cx + 12 * u, cy - 10 * u, 2.6f * u, withAlpha(acc, 240));
            break;
        }
        case UPG_DRONES: {
            Poly hex;
            for (int i = 0; i < 6; i++) hex.push_back({cx + cosf(i * TAU / 6) * 11 * u, cy + sinf(i * TAU / 6) * 11 * u});
            out.polyFill(hex.data(), 6, rgba(50, 66, 80, 230));
            out.polyOutline(hex.data(), 6, line, 1.6f * u);
            out.circleFill(cx, cy, 4 * u, withAlpha(acc, 230));
            out.ring(cx, cy, 16 * u, 1.2f * u, withAlpha(acc, 100));
            break;
        }
        case UPG_WEAPON: {
            out.rectOutline(cx - 16 * u, cy - 6 * u, 24 * u, 12 * u, 1.8f * u, line);
            L(8, -3, 22, -3, line, 3.f * u);
            L(8, 3, 22, 3, line, 3.f * u);
            out.ring(cx - 4 * u, cy, 18 * u, 1.2f * u, withAlpha(acc, 90));
            break;
        }
        case UPG_MAGNET: {
            out.arc(cx, cy, 15 * u, PI, TAU, 5.f * u, line);
            out.fillRect((int)(cx - 17 * u), (int)(cy - 2 * u), (int)(6 * u), (int)(10 * u), line);
            out.fillRect((int)(cx + 11 * u), (int)(cy - 2 * u), (int)(6 * u), (int)(10 * u), line);
            out.fillRect((int)(cx - 17 * u), (int)(cy - 2 * u), (int)(6 * u), (int)(4 * u), withAlpha(acc, 230));
            out.fillRect((int)(cx + 11 * u), (int)(cy - 2 * u), (int)(6 * u), (int)(4 * u), withAlpha(acc, 230));
            break;
        }
        default: {  // REFINERY
            for (int i = 0; i < 8; i++) {
                float a = i * TAU / 8;
                L(cosf(a) * 13 * u, sinf(a) * 13 * u, cosf(a) * 18 * u, sinf(a) * 18 * u, line, 3.f * u);
            }
            out.ring(cx, cy, 11 * u, 2.f * u, withAlpha(acc, 220));
            out.circleFill(cx, cy, 4 * u, withAlpha(acc, 160));
            break;
        }
    }
    out.radialGlow(cx, cy, size * .46f, withAlpha(acc, 26), 1.f);
}

void buildAchievementIcon(Canvas& out, int ach, int size) {
    out.resize(size, size);
    out.clear(0);
    float cx = size * .5f, cy = size * .5f;
    Color col = ACHIEVEMENTS[ach].color;
    Poly hex;
    for (int i = 0; i < 6; i++) hex.push_back({cx + cosf(i * TAU / 6 - PI / 2) * size * .42f,
                                              cy + sinf(i * TAU / 6 - PI / 2) * size * .42f});
    out.polyFill(hex.data(), 6, scaleColor(col, .28f));
    out.polyOutline(hex.data(), 6, scaleColor(col, .95f), std::max(1.f, size * .045f));
    // star
    Poly star;
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5;
        float r = (i % 2 == 0) ? size * .26f : size * .11f;
        star.push_back({cx + cosf(a) * r, cy + sinf(a) * r});
    }
    out.polyFill(star.data(), 10, withAlpha(scaleColor(col, 1.25f), 235));
    out.radialGlow(cx, cy, size * .48f, withAlpha(col, 60), 1.f);
}

// ================================================================ bank
void AssetBank::init(int stage) {
    // stage < 0 builds everything, otherwise just one group (used by the loading screen)
    const bool all = stage < 0;
    // ---- glows / generic sprites
    if (all || stage == 0) {
    tex::makeGlow(glowSoft, 128, 2.4f, rgba(255, 255, 255));
    tex::makeGlow(glowSharp, 64, 1.4f, rgba(255, 255, 255));
    tex::makeGlow(halo, 256, 3.2f, rgba(255, 255, 255));
    tex::makeSmoke(smoke, 96, 17);
    tex::makeSpark(spark, 64, 10, rgba(255, 250, 220));

    // ---- ships
    ships.resize(SHIP_COUNT);
    for (int i = 0; i < SHIP_COUNT; i++) buildShipArt(ships[i], SHIPS[i], i);

    // ---- enemies
    enemies.resize(EN_TYPE_COUNT);
    enemyGlow.resize(EN_TYPE_COUNT);
    for (int i = 0; i < EN_TYPE_COUNT; i++) buildEnemyArt(enemies[i], enemyGlow[i], i);

    // ---- drones
    drones.resize(3);
    for (int i = 0; i < 3; i++) buildDroneArt(drones[i], (float)i);

    }
    // ---- station
    if (all || stage == 1) {
    buildStationArt(station, stationGlow);

    // ---- planets
    planets.resize(5);
    planetsGlow.resize(5);
    for (int i = 0; i < 5; i++) buildPlanet(planets[i], planetsGlow[i], i, 1000u + i * 77u);

    // ---- debris
    debris.resize(5);
    for (int i = 0; i < 5; i++) buildDebris(debris[i], i, 55u + i * 9u);

    }
    // ---- rock variants per sector
    if (all || stage == 2) {
    for (int s = 0; s < 5; s++) {
        const SectorDef& sd = SECTORS[s];
        rocks[s].clear();
        rockRes[s].clear();
        int variantsPerRes = 2;
        for (int r = 0; r < 4; r++) {
            int res = sd.resList[r];
            if (res < 0) continue;
            for (int v = 0; v < variantsPerRes; v++) {
                const int SZ = 224;
                Canvas c(SZ, SZ);
                c.clear(0);
                tex::RockStyle rs;
                rs.seed = 600u + s * 31u + res * 17u + v * 5u;
                rs.veinColor = withAlpha(RESOURCES[res].color, 255);
                rs.veinAmount = .45f + .5f * (RESOURCES[res].rarity / 5.f);
                rs.rimColor = sd.pal.starTint;
                rs.craterAmount = .5f + .25f * (v % 2);
                switch (sd.rockStyle) {
                    case 0:
                        rs.rockLight = rgba(128, 112, 98);
                        rs.rockDark = rgba(34, 28, 26);
                        break;
                    case 1:
                        rs.rockLight = rgba(92, 128, 168);
                        rs.rockDark = rgba(22, 30, 54);
                        break;
                    case 2:
                        rs.rockLight = rgba(126, 134, 146);
                        rs.rockDark = rgba(26, 30, 38);
                        break;
                    case 3:
                        rs.rockLight = rgba(126, 82, 128);
                        rs.rockDark = rgba(34, 16, 40);
                        break;
                    default:
                        rs.rockLight = rgba(70, 104, 108);
                        rs.rockDark = rgba(12, 26, 30);
                        break;
                }
                rs.noiseScale = .042f + .006f * v;
                rs.rim = .85f;
                tex::paintRock(c, Vec2(SZ * .5f, SZ * .5f), ROCK_SPRITE_R, rs);
                if (RESOURCES[res].rarity >= 4) {
                    tex::paintCrystalCluster(c, Vec2(SZ * .5f, SZ * .5f), 74.f, RESOURCES[res].color,
                                             RESOURCES[res].color, 33u + v);
                }
                rocks[s].push_back(std::move(c));
                rockRes[s].push_back(res);
            }
        }
    }

    }
    // ---- nebula layers + starfields
    if (all || stage == 3) {
    stars.resize(5);
    for (int s = 0; s < 5; s++) {
        const SectorDef& sd = SECTORS[s];
        nebula[s].resize(2);
        for (int l = 0; l < 2; l++) {
            Canvas& nb = nebula[s][l];
            nb.resize(1000, 620);
            nb.clear(0);
            Color a = l == 0 ? sd.pal.nebA : sd.pal.nebB;
            Color b = l == 0 ? sd.pal.nebB : sd.pal.nebA;
            tex::paintNebula(nb, 300u + s * 41u + l * 7u, withAlpha(a, 255), withAlpha(b, 255),
                             l == 0 ? .8f : .5f, l == 0 ? 1.f : 2.2f);
        }
        Canvas& st = stars[s];
        st.resize(1280, 720);
        st.clear(0);
        tex::paintStarfield(st, 900u + s, 420 + sd.nebDensity * 4, sd.pal.starTint);
    }

    }
    // ---- icons
    if (all || stage == 4) {
    resIcon.resize(RES_COUNT);
    for (int i = 0; i < RES_COUNT; i++) buildResourceIcon(resIcon[i], i, 48);
    upgIcon.resize(UPG_COUNT);
    for (int i = 0; i < UPG_COUNT; i++) buildUpgradeIcon(upgIcon[i], i, 56);
    achIcon.resize(ACH_COUNT);
    for (int i = 0; i < ACH_COUNT; i++) buildAchievementIcon(achIcon[i], i, 44);

    // ---- cursor: small reticle
    cursor.resize(32, 32);
    cursor.clear(0);
    cursor.ring(16, 16, 8, 1.2f, withAlpha(rgba(200, 255, 240), 210));
    cursor.line({16, 0}, {16, 6}, withAlpha(rgba(200, 255, 240), 200), 1.2f);
    cursor.line({16, 26}, {16, 32}, withAlpha(rgba(200, 255, 240), 200), 1.2f);
    cursor.line({0, 16}, {6, 16}, withAlpha(rgba(200, 255, 240), 200), 1.2f);
    cursor.line({26, 16}, {32, 16}, withAlpha(rgba(200, 255, 240), 200), 1.2f);
    cursor.circleFill(16, 16, 1.6f, rgba(255, 255, 255, 230));
    }
}

}  // namespace sm
