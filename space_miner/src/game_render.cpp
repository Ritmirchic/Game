// ============================================================
// SPACE MINER  -  game_render.cpp
// World rendering: layered parallax backdrop, entities, beams, VFX.
// ============================================================
#include "game.h"
#include "content.h"
#include "art.h"
#include "platform.h"

namespace sm {

// ---------------------------------------------------------------- background
void Game::buildBackground() {
    const SectorDef& sd = SECTORS[sector];
    const int AW = 2048, AH = 1280;
    bgArena.resize(AW, AH);
    // sky gradient
    bgArena.vGradient(0, 0, AW, AH, sd.pal.skyTop, sd.pal.skyBottom);
    // nebula: two layers, scaled up and offset so the wrap looks seamless-ish
    for (int l = 0; l < 2; l++) {
        const Canvas& nb = assets->nebula[sector][l];
        int w = l == 0 ? AW + 260 : AW + 700;
        int h = l == 0 ? AH + 160 : AH + 460;
        bgArena.blitScaled(nb, l == 0 ? -130 : -350, l == 0 ? -80 : -230, w, h, l == 0 ? .8f : .45f, true);
    }
    // starfield: painted directly at arena resolution so single pixels stay crisp
    tex::paintStarfield(bgArena, 1200u + sector * 7u, 1500 + SECTORS[sector].nebDensity * 6,
                        sd.pal.starTint);
    // a second, denser and dimmer layer for depth
    {
        Rng r;
        r.seed(4400u + sector * 13u);
        for (int i = 0; i < 2600; i++) {
            float x = r.range(0, (float)AW), y = r.range(0, (float)AH);
            float b = powf(r.f(), 3.f);
            int br = (int)(40 + 110 * b);
            bgArena.blend((int)x, (int)y, withAlpha(scaleColor(sd.pal.starTint, br / 255.f * 1.2f),
                                                    (int)(70 + 120 * b)));
        }
    }
    // distant planet, positioned per sector
    {
        float px = AW * (sector == 1 ? .70f : sector == 4 ? .22f : .78f);
        float py = AH * (sector == 1 ? .30f : sector == 4 ? .68f : .24f);
        float scale = sector == 4 ? .85f : 1.05f;
        const Canvas& pl = assets->planets[sector];
        int pw = (int)(pl.w * scale), ph = (int)(pl.h * scale);
        tex::lensFlare(bgArena, px - pw * .42f, py - ph * .46f, 54.f, withAlpha(sd.pal.starTint, 90));
        bgArena.blitScaled(assets->planetsGlow[sector], (int)(px - pw * .5f), (int)(py - ph * .5f), pw, ph, .85f, true);
        bgArena.blitScaled(pl, (int)(px - pw * .5f), (int)(py - ph * .5f), pw, ph, 1.f, false);
    }
    // faint dust lanes
    for (int i = 0; i < 6; i++) {
        Rng r;
        r.seed(500u + sector * 7u + i);
        Vec2 a(r.range(0, (float)AW), r.range(0, (float)AH));
        Vec2 b = a + r.dir() * r.range(400.f, 1100.f);
        bgArena.line(a, b, withAlpha(sd.pal.nebB, 16), r.range(30.f, 90.f));
    }
    bgSector = sector;
}

void Game::renderStarfield(Canvas& fb) {
    if (bgSector != sector || bgArena.w == 0) buildBackground();
    // wrapped parallax sample
    int W = bgArena.w, H = bgArena.h;
    int ox = (int)(cameraPos.x * .35f) % W;
    int oy = (int)(cameraPos.y * .35f) % H;
    if (ox < 0) ox += W;
    if (oy < 0) oy += H;
    for (int y = 0; y < fb.h; y++) {
        int sy = y + oy;
        while (sy >= H) sy -= H;
        Color* dst = &fb.px[(size_t)y * fb.w];
        const Color* src = &bgArena.px[(size_t)sy * W];
        int first = std::min(fb.w, W - ox);
        for (int x = 0; x < first; x++) dst[x] = src[ox + x];
        for (int x = first; x < fb.w; x++) dst[x] = src[(ox + x) % W];
    }
}

// ---------------------------------------------------------------- helpers
static inline Vec2 toScreen(Vec2 p, Vec2 cam, float zoom, float sx, float sy) {
    return Vec2((p.x - cam.x) * zoom + VW * .5f + sx, (p.y - cam.y) * zoom + VH * .5f + sy);
}

// ---------------------------------------------------------------- world
void Game::renderWorld(Canvas& fb) {
    const SectorDef& sd = SECTORS[sector];
    Vec2 shake = cameraShake;
    if (cameraShake.x > 30) shake.x = 30;
    if (cameraShake.y > 30) shake.y = 30;
    if (cameraShake.x < -30) shake.x = -30;
    if (cameraShake.y < -30) shake.y = -30;
    renderStarfield(fb);
    renderEntityLayer(fb);
    renderBeams(fb);

    // ---- hazard lighting
    for (auto& h : hazards) {
        Vec2 s = toScreen(h.p, cameraPos, zoom, shake.x, shake.y);
        float pulse = h.kind == 0 ? powf(std::max(0.f, sinf(h.phase)), 8.f) : (sinf(h.phase) * .5f + .5f);
        float r = h.radius * zoom;
        if (s.x + r < 0 || s.y + r < 0 || s.x - r > VW || s.y - r > VH) continue;
        Color hc = sector == 3 ? rgba(255, 120, 230) : rgba(120, 180, 255);
        // swirling faint ring
        for (int i = 0; i < 2; i++) {
            float rr = r * (.55f + i * .26f);
            int seg = 26;
            for (int k = 0; k < seg; k++) {
                float a0 = k * TAU / seg + h.phase * (.4f + i * .2f);
                float a1 = (k + 1) * TAU / seg + h.phase * (.4f + i * .2f);
                float alpha = (18 + pulse * 90) * (1.f - i * .25f);
                fb.line({s.x + cosf(a0) * rr, s.y + sinf(a0) * rr}, {s.x + cosf(a1) * rr, s.y + sinf(a1) * rr},
                        withAlpha(hc, (int)alpha), 2.f);
            }
        }
        if (pulse > .05f && length(h.p - cameraPos) < 2200.f) {
            // electric arcs
            Rng r2;
            r2.seed((uint32_t)(h.phase * 13.f) + (uint32_t)(h.p.x * 7.f));
            for (int i = 0; i < 3; i++) {
                Vec2 p = s + r2.insideCircle(r * .8f);
                for (int k = 0; k < 5; k++) {
                    Vec2 q = p + r2.insideCircle(r * .25f);
                    fb.line(p, q, withAlpha(hc, (int)(120 * pulse)), 1.4f);
                    p = q;
                }
            }
            fb.radialGlow(s.x, s.y, r * .9f, withAlpha(hc, 40), pulse);
        }
    }
    // ambient colour grade (very subtle, keeps distant space readable)
    Color tint = withAlpha(sd.pal.fog, 16);
    fb.fillRect(0, 0, fb.w, fb.h, tint);
}

void Game::renderEntityLayer(Canvas& fb) {
    const SectorDef& sd = SECTORS[sector];
    Vec2 cam = cameraPos;
    const float z = zoom;
    Vec2 shake = cameraShake;
    auto S = [&](Vec2 p) { return toScreen(p, cam, z, shake.x, shake.y); };

    // ---------------- asteroids
    PROF(0);
    for (auto& a : asteroids) {
        if (!a.alive) continue;
        Vec2 s = S(a.p);
        float scr = a.radius * z * 1.06f;
        if (s.x + scr * .7f < 0 || s.y + scr * .7f < 0 || s.x - scr * .7f > VW || s.y - scr * .7f > VH) continue;
        if (a.sprite < 0 || a.sprite >= (int)assets->rocks[sector].size()) continue;
        const Canvas& rock = assets->rocks[sector][a.sprite];
        float scale = (a.radius / 100.f) * z;
        fb.blitRotated(rock, s, a.angle + a.wobble, scale, 1.f);
        // damage cracks
        if (a.crack > .05f) {
            Rng r;
            r.seed((uint32_t)(a.sprite * 977 + a.p.x * .07f + a.p.y * .13f) + 3);
            int n = 10;
            for (int i = 0; i < n; i++) {
                if (a.crack < (i + 1) / (float)n * .9f) break;
                float ang = r.range(0, TAU);
                Vec2 dir(cosf(ang), sinf(ang));
                Vec2 start = s + dir * (scr * r.range(.15f, .5f));
                Vec2 mid = start + dir * (scr * .18f) + perp(dir) * r.range(-scr * .12f, scr * .12f);
                Vec2 end = mid + dir * (scr * .22f) + perp(dir) * r.range(-scr * .1f, scr * .1f);
                fb.line(start, mid, withAlpha(rgba(14, 10, 12), 170), 1.6f);
                fb.line(mid, end, withAlpha(rgba(14, 10, 12), 140), 1.2f);
                fb.line(start, mid, withAlpha(RESOURCES[a.res[0]].color, (int)(90 * a.crack)), 1.f);
            }
        }
        // hit flash
        float glowT = std::max(0.f, 1.f - (time - a.lastHitT) * 4.f);
        if (glowT > 0) {
            fb.radialGlow(s.x, s.y, scr * 1.05f, withAlpha(RESOURCES[a.res[0]].color, (int)(70 * glowT)), 1.f);
        }
        // low health core glow
        if (a.crack > .55f) {
            float pulse = .5f + .5f * sinf(time * 6.f + a.p.x);
            fb.radialGlow(s.x, s.y, scr * (.35f + .1f * pulse), withAlpha(RESOURCES[a.res[0]].color, (int)(50 * a.crack)), 1.f);
        }
        // scanner reticle for nearby rocks: four dashed arcs instead of a full
        // circle, and only while the rock is actually on screen
        float distP = length(playerPos - a.p);
        bool onScreen = s.x > -60 && s.x < VW + 60 && s.y > -60 && s.y < VH + 60;
        if (distP < 780.f && onScreen) {
            float alpha = 85.f * (1.f - distP / 780.f);
            float rr = scr * .96f;
            int col = (int)alpha;
            for (int q = 0; q < 4; q++) {
                float a0 = q * (TAU / 4) + .30f, a1 = a0 + .95f;
                Vec2 prev(s.x + cosf(a0) * rr, s.y + sinf(a0) * rr);
                for (int k = 1; k <= 7; k++) {
                    float an = a0 + (a1 - a0) * k / 7.f;
                    Vec2 cur(s.x + cosf(an) * rr, s.y + sinf(an) * rr);
                    fb.line(prev, cur, withAlpha(sd.accent, col), 1.2f);
                    prev = cur;
                }
            }
            if (distP < 620.f) {
                std::string label = fmt("%s %.0f", RESOURCES[a.res[0]].code, a.amt[0]);
                drawText(fb, (int)(s.x + scr * .52f), (int)(s.y - scr * .52f), label,
                         withAlpha(RESOURCES[a.res[0]].color, 190), 1, 0);
            }
        }
    }

    // ---------------- station
    PROF(1);
    {
        Vec2 s = S(stationPos);
        float scale = 1.15f * z;
        if (s.x > -400 && s.y > -400 && s.x < VW + 400 && s.y < VH + 400) {
            fb.blitRotated(assets->station, s, stationAngle, scale, 1.f);
            fb.blitRotatedAdd(assets->stationGlow, s, stationAngle, scale, .85f);
            // docking beacon
            float pulse = .5f + .5f * sinf(uiTime * 3.f);
            fb.ring(s.x, s.y, 200.f * scale * .62f, 1.4f, withAlpha(sd.accent, (int)(40 + 40 * pulse)));
            if (length(playerPos - stationPos) < 460.f) {
                fb.ring(s.x, s.y, 230.f * scale * .62f, 1.8f, withAlpha(rgba(140, 255, 200), (int)(80 + 60 * pulse)));
                drawTextCenter(fb, (int)s.x, (int)(s.y - 210 * scale * .62f), "[ E ]  DOCK", rgba(180, 255, 220), 2, 1);
            }
        }
    }

    // ---------------- loot
    PROF(2);
    for (auto& l : loot) {
        Vec2 s = S(l.p);
        if (s.x < -40 || s.y < -40 || s.x > VW + 40 || s.y > VH + 40) continue;
        float float_ = sinf(time * 3.f + l.spin) * 3.f;
        Color c = RESOURCES[l.res].color;
        fb.radialGlow(s.x, s.y + float_, 16.f, withAlpha(c, 130), 1.f);
        const Canvas& icon = assets->resIcon[l.res];
        float sc = .34f + .06f * sinf(time * 4.f + l.spin);
        fb.blitRotated(icon, Vec2(s.x, s.y + float_), l.spin, sc, 1.f);
        if (l.life < 5.f) {
            float blink = (sinf(time * 10.f) > 0) ? 1.f : .3f;
            fb.ring(s.x, s.y + float_, 12.f, 1.f, withAlpha(c, (int)(140 * blink)));
        }
    }

    // ---------------- drones
    PROF(3);
    for (auto& d : drones) {
        Vec2 s = S(d.p);
        if (s.x < -80 || s.y < -80 || s.x > VW + 80 || s.y > VH + 80) continue;
        int tier = clampv((int)d.tier, 0, 2);
        float sc = (tier == 0 ? .34f : tier == 1 ? .40f : .46f) * z;
        fb.blitRotated(assets->drones[tier], s, d.angle, sc, 1.f);
        // rotor blur
        float spin = uiTime * 9.f;
        for (int i = 0; i < 3; i++) {
            float a = spin + i * TAU / 3;
            Vec2 p0 = s + Vec2(cosf(a), sinf(a)) * 18.f * sc * 2.f;
            Vec2 p1 = s + Vec2(cosf(a + 1.9f), sinf(a + 1.9f)) * 18.f * sc * 2.f;
            fb.line(p0, p1, withAlpha(rgba(180, 255, 240), 40), 1.f);
        }
        // engine halo
        fb.radialGlow(s.x - cosf(d.angle) * 10.f, s.y - sinf(d.angle) * 10.f, 16.f,
                      withAlpha(rgba(120, 220, 255), 110), 1.f);
        // mining beam
        if (d.beamT > .05f && d.target >= 0 && d.target < (int)asteroids.size() && asteroids[d.target].alive) {
            const Asteroid& a = asteroids[d.target];
            Vec2 t = S(a.p);
            Color bc = RESOURCES[a.res[0]].color;
            fb.glowLine(s + Vec2(cosf(d.angle), sinf(d.angle)) * 8.f, t, withAlpha(bc, (int)(210 * d.beamT)),
                        1.6f, 5.f, .8f);
            fb.radialGlow(t.x, t.y, 22.f, withAlpha(bc, (int)(90 * d.beamT)), 1.f);
        }
        // cargo indicator
        if (d.cargoAmt > .3f) {
            int tierR = clampv(d.cargoType, 0, RES_COUNT - 1);
            float frac = clampf(d.cargoAmt / DRONES[tier].capacity, 0.f, 1.f);
            fb.ring(s.x, s.y, 20.f, 2.f, withAlpha(rgba(20, 26, 32), 150));
            fb.arc(s.x, s.y, 20.f, -PI * .5f, -PI * .5f + TAU * frac, 2.f, withAlpha(RESOURCES[tierR].color, 230));
        }
    }

    // ---------------- enemies
    PROF(4);
    for (auto& e : enemies) {
        if (!e.alive) continue;
        Vec2 s = S(e.p);
        const EnemyDef& d = ENEMIES[e.type];
        float sizeWorld = d.size * 2.4f;
        float sc = sizeWorld / 110.f * z;
        if (s.x < -140 || s.y < -140 || s.x > VW + 140 || s.y > VH + 140) continue;
        fb.blitRotated(assets->enemies[e.type], s, e.angle, sc, 1.f);
        float flick = .8f + .25f * sinf(uiTime * (5.f + e.type));
        fb.blitRotatedAdd(assets->enemyGlow[e.type], s, e.angle, sc, flick);
        // hit flash
        if (e.hitFlash > 0) {
            fb.blitRotatedAdd(assets->enemies[e.type], s, e.angle, sc * 1.01f, e.hitFlash * .55f);
        }
        // engine trail
        Vec2 back = s - Vec2(cosf(e.angle), sinf(e.angle)) * 26.f * sc;
        fb.radialGlow(back.x, back.y, 18.f * sc * 2.f, withAlpha(d.tint, 110), 1.f);
        // shield bubble
        if (e.shield > 0) {
            fb.ring(s.x, s.y, 44.f * sc * 1.35f, 1.2f, withAlpha(rgba(120, 190, 255), 70));
        }
        // health bar
        float w = 54.f, h = 4.f;
        float frac = clampf(e.hp / e.maxHp, 0.f, 1.f);
        if (frac < .999f || length(playerPos - e.p) < 700.f) {
            float bx = s.x - w * .5f, by = s.y - 46.f * sc - 12.f;
            fb.fillRect((int)bx - 1, (int)by - 1, (int)w + 2, (int)h + 2, withAlpha(rgba(8, 10, 14), 170));
            fb.fillRect((int)bx, (int)by, (int)(w * frac), (int)h, mixColor(rgba(255, 90, 70), rgba(255, 210, 90), frac));
            if (e.maxShield > 0) {
                float sf = clampf(e.shield / e.maxShield, 0.f, 1.f);
                fb.fillRect((int)bx, (int)by - 3, (int)(w * sf), 2, withAlpha(rgba(120, 200, 255), 220));
            }
        }
        // aggro indicator
        if (e.state == 1 && length(playerPos - e.p) < 1400.f) {
            float pulse = .5f + .5f * sinf(uiTime * 6.f);
            fb.ring(s.x, s.y, 44.f * sc * 1.5f, 1.f, withAlpha(d.tint, (int)(50 * pulse)));
        }
    }

    // ---------------- player ship
    PROF(5);
    {
        Vec2 s = S(playerPos);
        const ShipArt& art = assets->ships[save.ship];
        float sc = 1.f * z;
        // engine flames (under the hull)
        for (auto& ep : art.engines) {
            Vec2 wp = s + rot(ep.pos, playerAngle) * sc;
            float thrust = .45f + throttle * .8f;
            float flick = .8f + .2f * sinf(uiTime * 30.f + ep.pos.x);
            Vec2 back = wp - fromAngle(playerAngle) * 22.f * thrust * sc;
            fb.glowLine(wp, back, withAlpha(ep.color, 200), 3.f * sc, 8.f * thrust, 1.2f);
            fb.radialGlow(wp.x, wp.y, 14.f * sc, withAlpha(ep.color, 150), 1.f);
            if (throttle > .55f) {
                fb.radialGlow(back.x, back.y, 18.f * sc, withAlpha(rgba(180, 240, 255), 90), 1.f);
            }
        }
        // shield bubble
        if (shield > 0) {
            float fr = shield / shieldMax;
            fb.ring(s.x, s.y, 62.f * sc, 2.f, withAlpha(rgba(120, 200, 255), (int)(30 + 60 * fr)));
            if (hud.hitT > 0) fb.ring(s.x, s.y, 62.f * sc, 3.f, withAlpha(rgba(180, 230, 255), (int)(180 * hud.hitT)));
        }
        // invulnerability shimmer
        float alpha = 1.f;
        if (invuln > 0) alpha = .55f + .45f * sinf(uiTime * 24.f);
        fb.blitRotated(art.body, s, playerAngle, sc, alpha);
        // turret head
        {
            Vec2 tp = s + rot(art.turretPos, playerAngle) * sc;
            fb.blitRotated(art.turret, tp, playerAngle, sc * .9f, 1.f);
        }
        // mining emitter charge glow
        if (miningBeamT > .02f) {
            Vec2 tp = s + rot(art.turretPos, playerAngle) * sc;
            fb.radialGlow(tp.x, tp.y, 26.f * miningBeamT, withAlpha(rgba(150, 240, 255), 150), 1.f);
        }
    }

    // ---------------- projectiles
    PROF(6);
    for (auto& sh : shots) {
        Vec2 s = S(sh.p);
        if (s.x < -40 || s.y < -40 || s.x > VW + 40 || s.y > VH + 40) continue;
        Vec2 tail = s - normalize(sh.v) * (sh.fromPlayer ? 26.f : 16.f);
        fb.glowLine(tail, s, withAlpha(sh.color, 235), sh.radius * .8f, 8.f, 1.3f);
        fb.radialGlow(s.x, s.y, sh.radius * 3.2f, withAlpha(sh.color, 150), 1.f);
    }

    // ---------------- particles
    PROF(7);
    for (auto& p : particles) {
        Vec2 s = S(p.p);
        if (s.x < -60 || s.y < -60 || s.x > VW + 60 || s.y > VH + 60) continue;
        float t = p.life / p.maxLife;
        switch (p.kind) {
            case 0: {
                float r = p.size * t * 2.2f * z;
                fb.radialGlow(s.x, s.y, r, withAlpha(p.color, (int)(190 * t)), 1.f);
                break;
            }
            case 1: {
                Vec2 tail = s - p.v * .022f * z;
                fb.glowLine(tail, s, withAlpha(p.color, (int)(230 * t)), 1.2f, 3.f, .8f);
                break;
            }
            case 2: {
                fb.blitScaled(assets->smoke, (int)(s.x - p.size * z), (int)(s.y - p.size * z),
                              (int)(p.size * 2 * z), (int)(p.size * 2 * z), .22f * t, false);
                break;
            }
            case 3: {
                float r = p.size * z;
                Vec2 pts[4] = {s + Vec2(cosf(p.rot) * r, sinf(p.rot) * r),
                               s + Vec2(cosf(p.rot + 2.1f) * r, sinf(p.rot + 2.1f) * r),
                               s + Vec2(cosf(p.rot + 4.2f) * r, sinf(p.rot + 4.2f) * r),
                               s + Vec2(cosf(p.rot + 5.6f) * r * .8f, sinf(p.rot + 5.6f) * r * .8f)};
                fb.polyFill(pts, 4, withAlpha(p.color, (int)(230 * t)));
                break;
            }
            case 4: {
                float rr = p.size * (1.f + (1.f - t) * 3.2f) * z;
                fb.ring(s.x, s.y, rr, 2.6f * t + .6f, withAlpha(p.color, (int)(200 * t * t)));
                break;
            }
            default: {
                float r = p.size * z;
                Vec2 a(s.x, s.y - r), b(s.x + r * .7f, s.y + r * .5f), c(s.x - r * .7f, s.y + r * .5f);
                fb.polyFill(std::vector<Vec2>{a, b, c}.data(), 3, withAlpha(p.color, (int)(220 * t)));
                break;
            }
        }
    }

    profFlush();
    // ---------------- boundary warning ring
    {
        float R = sd.worldRadius;
        Vec2 s = S(stationPos);
        float rr = R * z;
        for (int i = 0; i < 96; i++) {
            float a0 = i * TAU / 96, a1 = (i + 1) * TAU / 96;
            float dist = length(playerPos);
            if (dist < R * .93f) continue;
            fb.line({s.x + cosf(a0) * rr, s.y + sinf(a0) * rr}, {s.x + cosf(a1) * rr, s.y + sinf(a1) * rr},
                    withAlpha(rgba(255, 90, 70), 70), 2.f);
        }
    }
}

void Game::renderBeams(Canvas& fb) {
    Vec2 cam = cameraPos;
    Vec2 shake = cameraShake;
    auto S = [&](Vec2 p) { return toScreen(p, cam, zoom, shake.x, shake.y); };
    const ShipArt& art = assets->ships[save.ship];
    if (miningBeamT > .02f) {
        Vec2 start = S(playerPos + rot(art.turretPos, playerAngle));
        Vec2 dir = fromAngle(playerAngle);
        Vec2 end;
        if (miningTarget >= 0 && miningTarget < (int)asteroids.size()) {
            const Asteroid& a = asteroids[miningTarget];
            end = S(a.p - normalize(a.p - playerPos) * a.radius * .92f);
        } else {
            end = S(playerPos + dir * (statScan() * .85f + 180.f));
        }
        Color hot = rgba(150, 245, 255);
        float flick = .75f + .25f * sinf(uiTime * 40.f);
        // outer bloom
        fb.line(start, end, withAlpha(rgba(80, 200, 255), (int)(40 * miningBeamT)), 14.f);
        fb.line(start, end, withAlpha(rgba(140, 230, 255), (int)(90 * miningBeamT)), 6.f);
        fb.line(start, end, withAlpha(hot, (int)(230 * miningBeamT * flick)), 2.2f);
        fb.line(start, end, withAlpha(rgba(255, 255, 255), (int)(200 * miningBeamT)), .9f);
        // travelling pulses
        float len = length(end - start);
        for (int i = 0; i < 3; i++) {
            float t = fmodf(uiTime * 2.2f + i / 3.f, 1.f);
            Vec2 p = start + (end - start) * t;
            fb.radialGlow(p.x, p.y, 9.f, withAlpha(rgba(200, 255, 255), (int)(170 * miningBeamT)), 1.f);
        }
        if (miningTarget >= 0) {
            const Asteroid& a = asteroids[miningTarget];
            float r = a.radius * zoom;
            fb.radialGlow(end.x, end.y, r * .55f, withAlpha(RESOURCES[a.res[0]].color, (int)(110 * miningBeamT)), 1.f);
            for (int i = 0; i < 4; i++) {
                float ang = uiTime * 9.f + i * TAU / 4;
                Vec2 p = end + Vec2(cosf(ang), sinf(ang)) * r * .18f;
                fb.radialGlow(p.x, p.y, 8.f, withAlpha(rgba(255, 250, 220), 190), 1.f);
            }
        }
        (void)len;
    }
    // weapon muzzle flashes
    if (weaponCd > .19f) {
        for (auto& hp : art.hardpoints) {
            Vec2 s = S(playerPos + rot(hp, playerAngle));
            fb.radialGlow(s.x, s.y, 24.f * (weaponCd - .19f) / .07f, withAlpha(rgba(180, 250, 255), 210), 1.f);
        }
    }
}

// ---------------------------------------------------------------- frame
void Game::render(Canvas& fb) {
    switch (screen) {
        case 0: renderLoading(fb); break;
        case 1: renderMenu(fb); break;
        case 2: renderWorld(fb); renderHUD(fb); break;
        case 3: renderWorld(fb); renderStation(fb); break;
        case 4: renderWorld(fb); renderMap(fb); break;
        case 5: renderWorld(fb); renderSettings(fb); break;
        case 6: renderAchievements(fb); break;
        case 8: renderWorld(fb); renderVictory(fb); break;
        case 7: renderWorld(fb); renderDeath(fb); break;
        default: renderWorld(fb); renderHUD(fb); break;
    }
    // post: white flash
    if (flashWhite > .01f) fb.fillRect(0, 0, fb.w, fb.h, withAlpha(rgba(255, 255, 255), (int)(flashWhite * 255)));
    // screenshot mode hooks / screen-space effects
    fb.vignetteFast(.55f);
    if (save.settings.scanlines) fb.scanlines(3, withAlpha(rgba(0, 0, 0), 26));
    // cursor
    if (curInput) {
        fb.blitRotatedAdd(assets->cursor, Vec2(curInput->mouseX, curInput->mouseY), uiTime * .5f, .8f, .9f);
    }
}

// ---------------------------------------------------------------- HUD
void Game::renderHUD(Canvas& fb) {
    const SectorDef& sd = SECTORS[sector];
    const ShipDef& ship = SHIPS[save.ship];
    // ---------------- top-left: hull / shield / energy
    {
        int x = 24, y = 22;
        panel(fb, x - 8, y - 10, 296, 106, sd.accent, .78f);
        auto bar = [&](int bx, int by, int bw, float frac, Color c1, Color c2, const std::string& label,
                       const std::string& val) {
            fb.fillRect(bx, by, bw, 10, withAlpha(rgba(10, 14, 20), 210));
            fb.fillRect(bx, by, (int)(bw * clampf(frac, 0.f, 1.f)), 10, c1);
            fb.fillRect(bx, by, (int)(bw * clampf(frac, 0.f, 1.f)), 4, c2);
            drawText(fb, bx + 4, by + 3, label, rgba(240, 248, 255), 1, 1);
            drawTextRight(fb, bx + bw - 3, by + 3, val, rgba(235, 245, 255), 1, 1);
        };
        bar(x, y, 270, hull / std::max(1.f, hullMax), rgba(220, 70, 70), rgba(255, 140, 110), "HULL",
            fmt("%d/%d", (int)hull, (int)hullMax));
        bar(x, y + 18, 270, shieldMax > 0 ? shield / shieldMax : 0.f, rgba(60, 140, 230), rgba(140, 210, 255),
            "SHIELD", fmt("%d/%d", (int)shield, (int)shieldMax));
        bar(x, y + 36, 270, energy / std::max(1.f, energyMax), rgba(200, 170, 50), rgba(255, 236, 140), "ENERGY",
            fmt("%d", (int)energy));
        // cargo
        bar(x, y + 54, 270, cargoFraction(), rgba(90, 180, 130), rgba(160, 255, 200), "CARGO",
            fmt("%d/%d", (int)cargoUsed, (int)statCargo()));
        // credits + level
        drawText(fb, x, y + 74, "CREDITS", rgba(150, 170, 190), 1, 1);
        drawText(fb, x + 54, y + 73, money(save.credits), rgba(255, 216, 120), 2, 1);
        drawTextRight(fb, x + 270, y + 74, fmt("LV %d", save.level), sd.accent, 1, 1);
        int need = (int)(120.f * powf((float)save.level, 1.55f));
        fb.fillRect(x, y + 88, 270, 3, withAlpha(rgba(20, 26, 34), 200));
        fb.fillRect(x, y + 88, (int)(270.f * clampf(save.xp / (float)std::max(1, need), 0.f, 1.f)), 3,
                    withAlpha(sd.accent, 220));
    }
    // ---------------- top-centre: sector + ship
    {
        drawTextCenter(fb, VW / 2, 18, SECTORS[sector].name, sd.accent, 2, 2);
        drawTextCenter(fb, VW / 2, 36, fmt("%s  -  %s", SECTORS[sector].subtitle, upper(ship.name).c_str()),
                       withAlpha(rgba(200, 220, 240), 160), 1, 1);
        if (hud.warnT > 0) {
            float a = .5f + .5f * sinf(uiTime * 12.f);
            drawTextCenter(fb, VW / 2, 56, hud.warning, withAlpha(rgba(255, 90, 70), (int)(255 * a)), 2, 2);
        }
    }
    // ---------------- top-right: minimap
    {
        int mx = VW - 196, my = 22, ms = 174;
        panel(fb, mx - 6, my - 6, ms + 12, ms + 12, sd.accent, .72f);
        fb.fillRect(mx, my, ms, ms, withAlpha(rgba(6, 10, 16), 200));
        float R = SECTORS[sector].worldRadius;
        float scale = (ms * .46f) / R;
        Vec2 c(mx + ms * .5f, my + ms * .5f);
        // grid
        for (int i = 1; i < 4; i++) {
            fb.line({(float)mx + ms * i / 4.f, (float)my}, {(float)mx + ms * i / 4.f, (float)(my + ms)},
                    withAlpha(sd.accent, 22), 1.f);
            fb.line({(float)mx, (float)my + ms * i / 4.f}, {(float)(mx + ms), (float)my + ms * i / 4.f},
                    withAlpha(sd.accent, 22), 1.f);
        }
        // station
        Vec2 st = c + stationPos * scale;
        fb.ring(st.x, st.y, 4.f, 1.6f, withAlpha(rgba(140, 255, 200), 220));
        // asteroids
        for (auto& a : asteroids) {
            if (!a.alive) continue;
            Vec2 p = c + a.p * scale;
            Color rc = RESOURCES[a.res[0]].color;
            fb.fillRect((int)p.x, (int)p.y, 2, 2, withAlpha(rc, 150));
        }
        // enemies
        for (auto& e : enemies) {
            if (!e.alive) continue;
            Vec2 p = c + e.p * scale;
            fb.circleFill(p.x, p.y, 1.8f, withAlpha(rgba(255, 90, 80), 220));
        }
        // drones
        for (auto& d : drones) {
            Vec2 p = c + d.p * scale;
            fb.fillRect((int)p.x, (int)p.y, 1, 1, withAlpha(rgba(140, 255, 220), 180));
        }
        // player
        Vec2 pp = c + playerPos * scale;
        fb.circleFill(pp.x, pp.y, 2.6f, rgba(255, 255, 255, 240));
        fb.line(pp, pp + fromAngle(playerAngle) * 9.f, rgba(255, 255, 255, 200), 1.2f);
        drawText(fb, mx, my + ms + 12, "TACTICAL MAP", withAlpha(rgba(160, 190, 210), 160), 1, 1);
        drawTextRight(fb, mx + ms, my + ms + 12, fmt("DANGER %.0f%%", SECTORS[sector].danger * 100.f),
                      SECTORS[sector].accent, 1, 1);
    }
    // ---------------- bottom-left hints
    {
        int y = VH - 92;
        if (tutorialStep < 6 && tutorialT > 0) {
            float a = clampf(tutorialT / 1.5f, 0.f, 1.f) * clampf((14.f - tutorialT) / .5f, 0.f, 1.f);
            panel(fb, 22, y - 6, 520, 26, sd.accent, .55f * a);
            drawText(fb, 32, y + 2, TUTORIAL_HINTS[tutorialStep], withAlpha(rgba(210, 240, 250), (int)(220 * a)), 1, 1);
        }
        fb.fillRect(22, VH - 62, 460, 1, withAlpha(sd.accent, 40));
        drawText(fb, 22, VH - 52, "WASD FLY   MOUSE AIM   LMB MINE   RMB FIRE", withAlpha(rgba(170, 200, 215), 170), 1, 1);
        drawText(fb, 22, VH - 40, "E DOCK   TAB INVENTORY   M SECTOR CHART   ESC MENU", withAlpha(rgba(170, 200, 215), 140), 1, 1);
        if (hud.dockHintT > 0) {
            float pulse = .6f + .4f * sinf(uiTime * 5.f);
            drawText(fb, 22, VH - 74, "[ E ]  DOCK AT STATION", withAlpha(rgba(140, 255, 200), (int)(255 * pulse)), 2, 1);
        }
    }
    // ---------------- bottom-right: drones + alerts
    {
        int x = VW - 210, y = VH - 74;
        if (!drones.empty()) {
            drawText(fb, x, y, fmt("DRONES  %d", (int)drones.size()), withAlpha(rgba(160, 220, 210), 190), 1, 1);
            for (size_t i = 0; i < drones.size() && i < 8; i++) {
                Color c = drones[i].state == DR_MINE ? rgba(255, 210, 120)
                          : drones[i].state == DR_RETURN ? rgba(140, 200, 255) : rgba(120, 200, 180);
                fb.fillRect(x + (int)i * 12, y + 10, 8, 8, withAlpha(c, 200));
                fb.rectOutline((float)(x + (int)i * 12), (float)(y + 10), 8, 8, 1.f, withAlpha(rgba(0, 0, 0), 140));
            }
        }
        if (save.settings.showFps) {
            drawTextRight(fb, VW - 24, VH - 20, fmt("FPS %d   OBJ %d", (int)(1.f / std::max(.001f, lastDt)), (int)(asteroids.size() + enemies.size() + particles.size())),
                          withAlpha(rgba(120, 160, 180), 150), 1, 1);
        }
    }
    // ---------------- off-screen hostile indicators
    for (auto& e : enemies) {
        if (!e.alive) continue;
        Vec2 rel = (e.p - cameraPos) * zoom;
        bool off = fabsf(rel.x) > VW * .5f - 30 || fabsf(rel.y) > VH * .5f - 30;
        if (!off) continue;
        if (length(e.p - playerPos) > 1800.f) continue;
        float ang = angleOf(rel);
        Vec2 edge(VW * .5f + cosf(ang) * (VW * .46f), VH * .5f + sinf(ang) * (VH * .44f));
        Vec2 dir = fromAngle(ang);
        Vec2 p1 = edge + Vec2(-dir.y, dir.x) * 7.f - dir * 6.f;
        Vec2 p2 = edge + Vec2(dir.y, -dir.x) * 7.f - dir * 6.f;
        Vec2 p3 = edge + dir * 8.f;
        Vec2 tri[3] = {p1, p2, p3};
        fb.polyFill(tri, 3, withAlpha(rgba(255, 90, 80), 170));
    }
    // ---------------- toasts
    if (hud.toastT > 0) {
        float a = clampf(hud.toastT / .6f, 0.f, 1.f) * clampf((3.2f - hud.toastT) / .3f + .2f, 0.f, 1.f);
        int w = textWidth(hud.toast, 2, 1) + 40;
        panel(fb, VW / 2 - w / 2, 84, w, 30, hud.toastColor, .62f * a);
        drawTextCenter(fb, VW / 2, 94, hud.toast, withAlpha(hud.toastColor, (int)(255 * a)), 2, 1);
    }
    // ---------------- level up pop
    if (hud.levelPopT > 0) {
        float t = 1.f - hud.levelPopT / 3.2f;
        float a = clampf(hud.levelPopT / .8f, 0.f, 1.f);
        int y = (int)(VH * .30f - easeOutCubic(t * 2.f) * 40.f);
        drawHeading(fb, VW / 2, y, hud.levelPop, withAlpha(rgba(255, 248, 210), (int)(255 * a)),
                    withAlpha(rgba(255, 210, 90), (int)(160 * a)), 3, 8);
    }
    // ---------------- achievement popups
    {
        int slot = 0;
        for (int i = 0; i < ACH_COUNT; i++) {
            if (achievements[i].popupT <= 0) continue;
            float a = clampf(achievements[i].popupT / .5f, 0.f, 1.f);
            int y = 130 + slot * 54;
            int x = VW - 330 - (int)((1.f - easeOutCubic((4.2f - achievements[i].popupT) * 3.f)) * 60.f);
            panel(fb, x, y, 306, 46, ACHIEVEMENTS[i].color, .82f * a);
            fb.blitScaled(assets->achIcon[i], x + 6, y + 4, 38, 38, a, false);
            drawText(fb, x + 52, y + 8, "ACHIEVEMENT", withAlpha(rgba(190, 210, 230), (int)(200 * a)), 1, 1);
            drawText(fb, x + 52, y + 20, ACHIEVEMENTS[i].name, withAlpha(ACHIEVEMENTS[i].color, (int)(255 * a)), 2, 1);
            drawText(fb, x + 52, y + 34, ACHIEVEMENTS[i].desc, withAlpha(rgba(200, 220, 235), (int)(160 * a)), 1, 1);
            slot++;
        }
    }
    // ---------------- sector transition curtain
    if (sectorTransition > 0.f) {
        float t = clampf(sectorTransition, 0.f, 1.f);
        fb.fillRect(0, 0, VW, (int)(VH * t), withAlpha(rgba(4, 7, 12), 240));
        if (t > .25f) {
            drawHeading(fb, VW / 2, VH / 2 - 30, SECTORS[clampv(pendingSector >= 0 ? pendingSector : sector, 0, 4)].name,
                        SECTORS[clampv(pendingSector >= 0 ? pendingSector : sector, 0, 4)].accent, rgba(255, 255, 255), 3, 6);
            drawTextCenter(fb, VW / 2, VH / 2 + 8, "ENGAGING JUMP DRIVE", withAlpha(rgba(200, 230, 250), 180), 1, 2);
        }
    }
}

}  // namespace sm
