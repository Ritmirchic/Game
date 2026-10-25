// ── Canvas renderer: hand-built procedural art, particles, telegraphs ────────
import { eq, type Battle, type BattleEvent, type EnemyState, type Vec } from './core/types';
import { GRID_H, GRID_W } from './core/arena';
import { enemyDef, heroDef } from './core/content';
import { audio } from './audio';

const TAU = Math.PI * 2;

const C = {
  bg0: '#070910',
  bg1: '#131a29',
  tile0: '#2b3648',
  tile1: '#212a39',
  tileEdge: '#3b4761',
  wall0: '#6a7387',
  wall1: '#333b4e',
  ember: '#ff8b3d',
  emberHot: '#ffd98a',
  rust: '#c2613f',
  teal: '#63e0cf',
  blood: '#e2504f',
  ink: '#05070c',
  paper: '#efe6d4',
};

interface Particle {
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  max: number;
  size: number;
  color: string;
  grav: number;
  drag: number;
  kind: 'ember' | 'smoke' | 'spark' | 'debris';
}

interface FloatText {
  x: number;
  y: number;
  text: string;
  life: number;
  max: number;
  tone: 'good' | 'bad' | 'ember';
  size: number;
}

interface Visual {
  x: number;
  y: number;
  tx: number;
  ty: number;
  born: number;
  flash: number;
  wobble: number;
}

interface Dying {
  typeId: string;
  x: number;
  y: number;
  at: number;
}

interface Beam {
  from: Vec;
  to: Vec;
  at: number;
  color: string;
}

export class Renderer {
  readonly canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private dpr = 1;
  tile = 64;
  private ox = 0;
  private oy = 0;
  private width = 0;
  private height = 0;
  private insetTop = 0;
  private insetBottom = 0;

  battle: Battle | null = null;
  private boardCache: HTMLCanvasElement | null = null;

  private heroVis: Visual | null = null;
  private enemyVis = new Map<number, Visual>();
  private dying: Dying[] = [];
  private particles: Particle[] = [];
  private floats: FloatText[] = [];
  private beams: Beam[] = [];
  private ambient: Particle[] = [];
  private shake = 0;
  private time = 0;
  private lastFrame = 0;

  highlights: Vec[] = [];
  highlightKind: 'ability' | 'move' | 'danger' = 'ability';
  hover: Vec | null = null;
  path: Vec[] = [];
  selectedAbility: number | null = null;
  ghostIntents = false;

  private queue: { ev: BattleEvent; at: number }[] = [];
  private nextAt = 0;
  private resolvers: (() => void)[] = [];

  constructor(canvas: HTMLCanvasElement) {
    this.canvas = canvas;
    const ctx = canvas.getContext('2d', { alpha: true });
    if (!ctx) throw new Error('Canvas 2D is unavailable in this browser.');
    this.ctx = ctx;
    this.resize();
    this.seedAmbient();
  }

  // ── layout ────────────────────────────────────────────────────────────────
  resize(): void {
    const rect = this.canvas.getBoundingClientRect();
    this.dpr = Math.min(2, window.devicePixelRatio || 1);
    this.width = Math.max(320, rect.width);
    this.height = Math.max(320, rect.height);
    this.canvas.width = Math.floor(this.width * this.dpr);
    this.canvas.height = Math.floor(this.height * this.dpr);
    const availH = Math.max(240, this.height - this.insetTop - this.insetBottom);
    const size = Math.min(this.width, availH);
    const margin = this.battle ? size * 0.04 : 0;
    const bw = this.battle?.board.w ?? GRID_W;
    const bh = this.battle?.board.h ?? GRID_H;
    this.tile = Math.max(24, Math.floor((size - margin * 2) / Math.max(bw, bh)));
    this.ox = Math.floor((this.width - this.tile * bw) / 2);
    this.oy = Math.floor(this.insetTop + (availH - this.tile * bh) / 2);
    this.boardCache = null;
  }

  setInsets(top: number, bottom: number): void {
    if (this.insetTop === top && this.insetBottom === bottom) return;
    this.insetTop = top;
    this.insetBottom = bottom;
    this.resize();
  }

  setBattle(b: Battle | null): void {
    this.battle = b;
    this.enemyVis.clear();
    this.dying = [];
    this.particles = [];
    this.floats = [];
    this.beams = [];
    this.heroVis = null;
    this.highlights = [];
    this.path = [];
    this.selectedAbility = null;
    this.resize();
    if (b) {
      this.heroVis = { x: b.hero.pos.x, y: b.hero.pos.y, tx: b.hero.pos.x, ty: b.hero.pos.y, born: 0, flash: 0, wobble: 0 };
      this.invalidate();
    }
  }

  invalidate(): void {
    this.boardCache = null;
  }

  tileCenter(p: Vec): { x: number; y: number } {
    return { x: this.ox + (p.x + 0.5) * this.tile, y: this.oy + (p.y + 0.5) * this.tile };
  }

  toGrid(clientX: number, clientY: number): Vec | null {
    const rect = this.canvas.getBoundingClientRect();
    const x = ((clientX - rect.left) / rect.width) * this.width;
    const y = ((clientY - rect.top) / rect.height) * this.height;
    const gx = Math.floor((x - this.ox) / this.tile);
    const gy = Math.floor((y - this.oy) / this.tile);
    const bw = this.battle?.board.w ?? GRID_W;
    const bh = this.battle?.board.h ?? GRID_H;
    if (gx < 0 || gy < 0 || gx >= bw || gy >= bh) return null;
    return { x: gx, y: gy };
  }

  // ── effects ───────────────────────────────────────────────────────────────
  shakeScreen(power: number): void {
    this.shake = Math.min(26, this.shake + power);
  }

  private seedAmbient(): void {
    this.ambient = [];
    for (let i = 0; i < 70; i++) {
      this.ambient.push({
        x: Math.random(),
        y: Math.random(),
        vx: (Math.random() - 0.5) * 0.01,
        vy: -0.01 - Math.random() * 0.028,
        life: 1,
        max: 1,
        size: 0.6 + Math.random() * 2.4,
        color: Math.random() > 0.7 ? C.emberHot : C.ember,
        grav: 0,
        drag: 1,
        kind: 'ember',
      });
    }
  }

  private spawnParticles(at: { x: number; y: number }, count: number, kind: Particle['kind'], color: string, spread = 1, power = 1): void {
    for (let i = 0; i < count; i++) {
      const a = Math.random() * TAU;
      const sp = (20 + Math.random() * 150) * power;
      this.particles.push({
        x: at.x + (Math.random() - 0.5) * this.tile * 0.35,
        y: at.y + (Math.random() - 0.5) * this.tile * 0.35,
        vx: Math.cos(a) * sp * spread,
        vy: Math.sin(a) * sp * spread - (kind === 'smoke' ? 40 : 10),
        life: kind === 'smoke' ? 0.9 + Math.random() * 0.7 : 0.4 + Math.random() * 0.7,
        max: 1.6,
        size: kind === 'smoke' ? 6 + Math.random() * 10 : 1.5 + Math.random() * 3.4,
        color,
        grav: kind === 'smoke' ? -30 : 190,
        drag: kind === 'smoke' ? 0.96 : 0.9,
        kind,
      });
    }
  }

  private addFloat(at: { x: number; y: number }, text: string, tone: FloatText['tone'], size = 1): void {
    this.floats.push({ x: at.x, y: at.y, text, life: 1.05, max: 1.05, tone, size: this.tile * 0.30 * size });
  }

  // ── event playback ────────────────────────────────────────────────────────
  private delayFor(ev: BattleEvent): number {
    switch (ev.t) {
      case 'move':
      case 'enemyMove':
        return 230;
      case 'hit':
        return 85;
      case 'death':
        return 130;
      case 'push':
        return 170;
      case 'slam':
        return 120;
      case 'spawn':
        return 110;
      case 'turn':
        return 180;
      case 'wave':
        return 200;
      case 'explode':
        return 150;
      case 'lost':
      case 'won':
        return 320;
      default:
        return 70;
    }
  }

  playEvents(events: BattleEvent[]): Promise<void> {
    if (!events.length) return Promise.resolve();
    const now = performance.now();
    let at = Math.max(now, this.nextAt);
    for (const ev of events) {
      this.queue.push({ ev, at });
      at += this.delayFor(ev);
    }
    this.nextAt = at;
    return new Promise((resolve) => {
      this.resolvers.push(resolve);
    });
  }

  get busy(): boolean {
    return this.queue.length > 0;
  }

  private flushEvents(now: number): void {
    while (this.queue.length && this.queue[0].at <= now) {
      const item = this.queue.shift()!;
      this.applyEvent(item.ev);
    }
    if (!this.queue.length && this.resolvers.length) {
      this.nextAt = 0;
      const list = this.resolvers;
      this.resolvers = [];
      for (const r of list) r();
    }
  }

  private visualFor(e: EnemyState, now: number): Visual {
    let v = this.enemyVis.get(e.uid);
    if (!v) {
      v = { x: e.pos.x, y: e.pos.y, tx: e.pos.x, ty: e.pos.y, born: now, flash: 0, wobble: Math.random() * 6 };
      this.enemyVis.set(e.uid, v);
    }
    return v;
  }

  private applyEvent(ev: BattleEvent): void {
    const now = performance.now();
    const b = this.battle;
    switch (ev.t) {
      case 'move': {
        if (this.heroVis) {
          this.heroVis.tx = ev.to.x;
          this.heroVis.ty = ev.to.y;
        }
        const to = this.tileCenter(ev.to);
        this.spawnParticles(to, 8, 'smoke', 'rgba(120,140,170,0.5)', 0.6, 0.5);
        break;
      }
      case 'enemyMove': {
        const v = this.enemyVis.get(ev.uid);
        if (v) {
          v.tx = ev.to.x;
          v.ty = ev.to.y;
        }
        const to = this.tileCenter(ev.to);
        this.spawnParticles(to, 6, 'smoke', 'rgba(110,120,140,0.45)', 0.5, 0.4);
        break;
      }
      case 'hit': {
        const at = this.tileCenter(ev.at);
        if (ev.blocked) {
          this.spawnParticles(at, 12, 'spark', '#9fd8ff', 1, 1);
          this.addFloat(at, `-${ev.amount}`, 'good', 0.85);
          audio.play('shield');
        } else {
          this.spawnParticles(at, 16, 'spark', ev.forge ? C.emberHot : '#ffd9a0', 1, 1.4);
          this.spawnParticles(at, 6, 'debris', ev.forge ? '#8a4a2a' : '#b34b3a', 0.9, 1);
          this.addFloat(at, `-${ev.amount}`, ev.hero || ev.forge ? 'bad' : 'good', 1);
          audio.play(ev.forge ? 'forgeHit' : 'hit');
          this.shakeScreen(ev.hero || ev.forge ? 6 + ev.amount * 2 : 3);
        }
        if (ev.uid !== undefined) {
          const v = this.enemyVis.get(ev.uid);
          if (v) v.flash = 1;
        }
        if (ev.hero && this.heroVis) this.heroVis.flash = 1;
        break;
      }
      case 'heal': {
        const at = this.tileCenter(ev.at);
        this.spawnParticles(at, 14, 'spark', '#8ef0b8', 0.6, 0.7);
        this.addFloat(at, `+${ev.amount}`, 'good');
        audio.play('heal');
        break;
      }
      case 'shield': {
        const at = this.tileCenter(ev.at);
        this.spawnParticles(at, 10, 'spark', '#a8d8ff', 0.9, 0.6);
        this.addFloat(at, `+${ev.amount}`, 'good', 0.8);
        audio.play('shield');
        break;
      }
      case 'push': {
        const v = this.enemyVis.get(ev.uid);
        if (v) {
          v.tx = ev.to.x;
          v.ty = ev.to.y;
        }
        this.spawnParticles(this.tileCenter(ev.to), 8, 'smoke', 'rgba(160,150,140,0.5)', 0.7, 0.7);
        audio.play('shove');
        break;
      }
      case 'slam': {
        const at = this.tileCenter(ev.at);
        if (ev.kind.startsWith('cast:')) {
          this.spawnParticles(at, 20, 'ember', C.ember, 0.8, 1.1);
          audio.play('cast');
        } else if (ev.kind === 'beam') {
          this.beams.push({ from: ev.at, to: ev.at, at: now, color: C.blood });
          this.spawnParticles(at, 12, 'spark', C.emberHot, 0.9, 1);
        } else if (ev.kind === 'slash') {
          this.spawnParticles(at, 10, 'spark', C.blood, 1, 1);
        } else if (ev.kind === 'forgeAtk') {
          this.spawnParticles(at, 14, 'spark', C.emberHot, 1, 1.2);
        } else if (ev.kind === 'wall') {
          this.spawnParticles(at, 10, 'debris', '#7a8496', 0.7, 0.8);
          audio.play('wall');
        } else {
          this.spawnParticles(at, 8, 'ember', C.ember, 0.8, 0.8);
          audio.play('cast');
        }
        break;
      }
      case 'death': {
        const at = this.tileCenter(ev.at);
        const v = this.enemyVis.get(ev.uid);
        const typeId = b?.enemies.find((e) => e.uid === ev.uid)?.typeId ?? this.lastType.get(ev.uid) ?? 'grunt';
        this.dying.push({ typeId, x: v?.x ?? ev.at.x, y: v?.y ?? ev.at.y, at: now });
        this.enemyVis.delete(ev.uid);
        this.spawnParticles(at, ev.boss ? 60 : 22, 'ember', C.ember, 0.8, ev.boss ? 2 : 1.1);
        this.spawnParticles(at, ev.boss ? 26 : 8, 'smoke', 'rgba(90,80,80,0.6)', 0.6, ev.boss ? 1.6 : 0.8);
        if (ev.boss) this.shakeScreen(20);
        audio.play('death');
        break;
      }
      case 'spawn': {
        this.spawnParticles(this.tileCenter(ev.at), 12, 'smoke', 'rgba(140,110,90,0.55)', 0.6, 0.8);
        audio.play('spawn');
        break;
      }
      case 'bomb': {
        const at = this.tileCenter(ev.at);
        this.spawnParticles(at, 8, 'spark', C.emberHot, 0.6, 0.6);
        break;
      }
      case 'explode': {
        const at = this.tileCenter(ev.at);
        this.spawnParticles(at, 40, 'ember', C.emberHot, 1, 1.8);
        this.spawnParticles(at, 18, 'smoke', 'rgba(60,50,50,0.7)', 1, 1.4);
        this.shakeScreen(12);
        audio.play('bomb');
        break;
      }
      case 'vent': {
        const at = this.tileCenter(ev.at);
        this.spawnParticles(at, 18, 'smoke', 'rgba(230,220,210,0.5)', 0.7, 1.2);
        this.spawnParticles(at, 10, 'ember', C.emberHot, 0.8, 0.9);
        audio.play('vent');
        break;
      }
      case 'wall': {
        this.invalidate();
        this.spawnParticles(this.tileCenter(ev.at), 18, 'debris', '#7d8798', 0.8, 1);
        audio.play('wall');
        break;
      }
      case 'burn': {
        const v = this.enemyVis.get(ev.uid);
        if (v) v.flash = 0.6;
        this.spawnParticles(this.tileCenter(ev.at), 6, 'ember', C.ember, 0.5, 0.6);
        break;
      }
      case 'mark': {
        this.spawnParticles(this.tileCenter(ev.at), 10, 'spark', C.emberHot, 0.7, 0.8);
        this.addFloat(this.tileCenter(ev.at), 'BRAND', 'ember', 0.7);
        break;
      }
      case 'wave': {
        this.addFloat({ x: this.width / 2, y: this.oy - this.tile * 0.2 }, 'REINFORCEMENTS', 'bad', 0.9);
        audio.play('turn');
        break;
      }
      case 'turn': {
        audio.play('turn');
        break;
      }
      case 'shake': {
        this.shakeScreen(ev.power);
        break;
      }
      case 'text': {
        this.addFloat(this.tileCenter(ev.at), ev.text, ev.tone, 0.7);
        break;
      }
      case 'won':
      case 'lost':
        break;
    }
  }

  /** Remembers enemy types after they are removed from the logical state. */
  private lastType = new Map<number, string>();

  // ── frame ─────────────────────────────────────────────────────────────────
  frame(now: number): void {
    const dt = this.lastFrame ? Math.min(0.05, (now - this.lastFrame) / 1000) : 0.016;
    this.lastFrame = now;
    this.time = now / 1000;
    this.flushEvents(now);
    this.step(dt);
    this.draw();
  }

  private step(dt: number): void {
    const b = this.battle;
    if (b) {
      for (const e of b.enemies) {
        if (e.hp <= 0) continue;
        this.lastType.set(e.uid, e.typeId);
        const v = this.visualFor(e, performance.now());
        v.tx = e.pos.x;
        v.ty = e.pos.y;
        v.x += (v.tx - v.x) * Math.min(1, dt * 14);
        v.y += (v.ty - v.y) * Math.min(1, dt * 14);
        v.flash = Math.max(0, v.flash - dt * 3.2);
      }
      if (this.heroVis) {
        this.heroVis.tx = b.hero.pos.x;
        this.heroVis.ty = b.hero.pos.y;
        this.heroVis.x += (this.heroVis.tx - this.heroVis.x) * Math.min(1, dt * 14);
        this.heroVis.y += (this.heroVis.ty - this.heroVis.y) * Math.min(1, dt * 14);
        this.heroVis.flash = Math.max(0, this.heroVis.flash - dt * 3.2);
      }
      for (const uid of [...this.enemyVis.keys()]) {
        if (!b.enemies.some((e) => e.uid === uid && e.hp > 0)) this.enemyVis.delete(uid);
      }
    }
    this.shake = Math.max(0, this.shake - dt * 42);
    this.dying = this.dying.filter((d) => performance.now() - d.at < 700);

    for (const p of this.particles) {
      p.life -= dt;
      p.vy += p.grav * dt;
      p.vx *= p.drag;
      p.vy *= p.drag;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
    }
    this.particles = this.particles.filter((p) => p.life > 0);
    for (const f of this.floats) {
      f.life -= dt;
      f.y -= dt * this.tile * 0.9;
    }
    this.floats = this.floats.filter((f) => f.life > 0);
    this.beams = this.beams.filter((x) => performance.now() - x.at < 400);

    for (const p of this.ambient) {
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      if (p.y < -0.05) {
        p.y = 1.05;
        p.x = Math.random();
      }
      if (p.x < -0.05) p.x = 1.05;
      if (p.x > 1.05) p.x = -0.05;
    }
  }

  private draw(): void {
    const ctx = this.ctx;
    ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    ctx.clearRect(0, 0, this.width, this.height);
    this.drawBackdrop(ctx);
    if (!this.battle) {
      this.drawTitleArt(ctx);
      this.drawParticles(ctx);
      this.drawAmbient(ctx);
      return;
    }
    const sx = this.shake ? (Math.random() - 0.5) * this.shake : 0;
    const sy = this.shake ? (Math.random() - 0.5) * this.shake : 0;
    ctx.save();
    ctx.translate(sx, sy);
    this.drawBoardFrame(ctx);
    this.drawTelegraphs(ctx);
    this.drawHighlights(ctx);
    this.drawForge(ctx);
    this.drawEntities(ctx);
    this.drawParticles(ctx);
    this.drawBeams(ctx);
    this.drawFloats(ctx);
    this.drawHover(ctx);
    ctx.restore();
    this.drawVignette(ctx);
  }

  private drawBackdrop(ctx: CanvasRenderingContext2D): void {
    const g = ctx.createLinearGradient(0, 0, 0, this.height);
    g.addColorStop(0, '#0b0f18');
    g.addColorStop(0.55, '#0a0c12');
    g.addColorStop(1, '#07080d');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, this.width, this.height);

    const glowX = this.width * 0.5;
    const glowY = this.height * (this.battle ? 0.94 : 0.72);
    const rg = ctx.createRadialGradient(glowX, glowY, 0, glowX, glowY, this.width * 0.62);
    rg.addColorStop(0, `rgba(255,122,46,${this.battle ? 0.16 : 0.22})`);
    rg.addColorStop(0.35, 'rgba(160,60,25,0.07)');
    rg.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = rg;
    ctx.fillRect(0, 0, this.width, this.height);

    // faint distant arches
    ctx.save();
    ctx.globalAlpha = 0.16;
    ctx.strokeStyle = '#2a3448';
    const step = Math.max(120, this.width / 9);
    for (let x = -step; x < this.width + step; x += step) {
      ctx.beginPath();
      ctx.moveTo(x, this.height);
      ctx.lineTo(x, this.height * 0.35);
      ctx.quadraticCurveTo(x + step / 2, this.height * 0.1, x + step, this.height * 0.35);
      ctx.lineTo(x + step, this.height);
      ctx.stroke();
    }
    ctx.restore();
  }

  private drawAmbient(ctx: CanvasRenderingContext2D): void {
    ctx.save();
    for (const p of this.ambient) {
      const x = p.x * this.width;
      const y = p.y * this.height;
      const flicker = 0.35 + 0.45 * Math.abs(Math.sin(this.time * 1.7 + p.x * 20));
      ctx.globalAlpha = flicker * 0.8;
      ctx.fillStyle = p.color;
      ctx.beginPath();
      ctx.arc(x, y, p.size, 0, TAU);
      ctx.fill();
    }
    ctx.restore();
  }

  // ── board ─────────────────────────────────────────────────────────────────
  private ensureBoardCache(): void {
    if (this.boardCache || !this.battle) return;
    const b = this.battle;
    const size = Math.floor(this.tile * b.board.w * this.dpr);
    const canvas = document.createElement('canvas');
    canvas.width = size;
    canvas.height = size;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.scale(this.dpr, this.dpr);
    const t = this.tile;
    const round = (x: number, y: number, w: number, h: number, r: number) => {
      ctx.beginPath();
      ctx.moveTo(x + r, y);
      ctx.arcTo(x + w, y, x + w, y + h, r);
      ctx.arcTo(x + w, y + h, x, y + h, r);
      ctx.arcTo(x, y + h, x, y, r);
      ctx.arcTo(x, y, x + w, y, r);
      ctx.closePath();
    };

    const bw = b.board.w;
    const bh = b.board.h;
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        const tile = b.board.tiles[y * bw + x];
        const px = x * t;
        const py = y * t;
        const inset = Math.max(1.5, t * 0.028);

        // Foundation shadow / seam
        ctx.fillStyle = '#080a10';
        round(px + inset, py + inset, t - inset * 2, t - inset * 2, t * 0.09);
        ctx.fill();

        if (tile.kind === 'wall') {
          const topH = t * 0.6;
          // contact shadow
          ctx.fillStyle = 'rgba(0,0,0,0.55)';
          round(px + inset * 1.6, py + inset * 1.2, t - inset * 3.2, t - inset * 1.6, t * 0.09);
          ctx.fill();
          // front face
          const gf = ctx.createLinearGradient(px, py + topH, px, py + t - inset);
          gf.addColorStop(0, '#3b4457');
          gf.addColorStop(1, '#222936');
          ctx.fillStyle = gf;
          round(px + inset * 1.3, py + topH - t * 0.04, t - inset * 2.6, t - topH - inset * 0.6, t * 0.05);
          ctx.fill();
          // top face
          const gt = ctx.createLinearGradient(px, py, px, py + topH);
          gt.addColorStop(0, '#7b8598');
          gt.addColorStop(0.55, '#5f6a7d');
          gt.addColorStop(1, '#4a5468');
          ctx.fillStyle = gt;
          round(px + inset * 1.3, py + inset * 1.6, t - inset * 2.6, topH, t * 0.07);
          ctx.fill();
          ctx.strokeStyle = 'rgba(10,12,18,0.85)';
          ctx.lineWidth = Math.max(1, t * 0.02);
          ctx.stroke();
          // brick seams on top
          ctx.strokeStyle = 'rgba(18,22,30,0.55)';
          ctx.lineWidth = Math.max(1, t * 0.014);
          ctx.beginPath();
          ctx.moveTo(px + t * 0.5, py + inset * 1.7);
          ctx.lineTo(px + t * 0.5, py + topH * 0.55);
          ctx.moveTo(px + inset * 1.5, py + topH * 0.55);
          ctx.lineTo(px + t - inset * 1.5, py + topH * 0.55);
          ctx.moveTo(px + t * 0.3, py + topH * 0.55);
          ctx.lineTo(px + t * 0.3, py + topH);
          ctx.moveTo(px + t * 0.7, py + topH * 0.55);
          ctx.lineTo(px + t * 0.7, py + topH);
          ctx.stroke();
          // bevel light along top edge
          ctx.strokeStyle = 'rgba(210,225,245,0.4)';
          ctx.lineWidth = Math.max(1, t * 0.02);
          ctx.beginPath();
          ctx.moveTo(px + inset * 2.4, py + inset * 2.4);
          ctx.lineTo(px + t - inset * 2.4, py + inset * 2.4);
          ctx.stroke();
          // rivets
          ctx.fillStyle = 'rgba(200,210,230,0.25)';
          for (const [rx, ry] of [
            [0.22, 0.78],
            [0.78, 0.78],
          ]) {
            ctx.beginPath();
            ctx.arc(px + t * rx, py + t * ry, Math.max(1, t * 0.018), 0, TAU);
            ctx.fill();
          }
          // crack
          ctx.strokeStyle = 'rgba(12,14,20,0.45)';
          ctx.lineWidth = Math.max(1, t * 0.014);
          ctx.beginPath();
          ctx.moveTo(px + t * (0.3 + tile.variant * 0.3), py + topH + t * 0.06);
          ctx.lineTo(px + t * (0.42 + tile.variant * 0.2), py + t * 0.9);
          ctx.stroke();
          continue;
        }

        const variant = tile.variant;
        const checker = (x + y) % 2 === 0;
        const g = ctx.createLinearGradient(px, py, px, py + t);
        g.addColorStop(0, checker ? C.tile0 : C.tile1);
        g.addColorStop(1, checker ? '#1d2634' : '#19212d');
        ctx.fillStyle = g;
        round(px + inset, py + inset, t - inset * 2, t - inset * 2, t * 0.07);
        ctx.fill();
        ctx.strokeStyle = 'rgba(120,150,200,0.22)';
        ctx.lineWidth = Math.max(1, t * 0.012);
        ctx.stroke();

        // stone speckle
        ctx.fillStyle = 'rgba(160,180,210,0.05)';
        for (let i = 0; i < 4; i++) {
          const sv = (variant * (i + 3)) % 1;
          const sv2 = (variant * (i + 7)) % 1;
          ctx.beginPath();
          ctx.arc(px + t * (0.15 + sv * 0.7), py + t * (0.15 + sv2 * 0.7), Math.max(0.7, t * 0.012), 0, TAU);
          ctx.fill();
        }
        // subtle engraved line
        if (variant > 0.72) {
          ctx.strokeStyle = 'rgba(120,145,180,0.08)';
          ctx.beginPath();
          ctx.moveTo(px + t * 0.2, py + t * 0.7);
          ctx.lineTo(px + t * 0.8, py + t * 0.7);
          ctx.stroke();
        }

        const df = Math.abs(x - 3.5) + Math.abs(y - 7);
        if (df < 3.6) {
          const warm = ctx.createRadialGradient(px + t / 2, py + t / 2, t * 0.1, px + t / 2, py + t / 2, t * 0.9);
          warm.addColorStop(0, `rgba(255,140,60,${0.16 * (1 - df / 3.6)})`);
          warm.addColorStop(1, 'rgba(255,120,40,0)');
          ctx.fillStyle = warm;
          round(px + inset, py + inset, t - inset * 2, t - inset * 2, t * 0.07);
          ctx.fill();
        }

        if (tile.kind === 'vent') {
          const cx = px + t / 2;
          const cy = py + t / 2;
          // dark pit
          const grd = ctx.createRadialGradient(cx, cy, t * 0.03, cx, cy, t * 0.44);
          grd.addColorStop(0, '#000000');
          grd.addColorStop(0.5, '#150b07');
          grd.addColorStop(0.86, '#3a1f13');
          grd.addColorStop(1, '#57616f');
          ctx.fillStyle = grd;
          ctx.beginPath();
          ctx.ellipse(cx, cy, t * 0.37, t * 0.33, 0, 0, TAU);
          ctx.fill();
          // heat deep inside
          const heat = ctx.createRadialGradient(cx, cy + t * 0.1, 0, cx, cy + t * 0.1, t * 0.22);
          heat.addColorStop(0, 'rgba(255,120,40,0.5)');
          heat.addColorStop(1, 'rgba(255,90,30,0)');
          ctx.fillStyle = heat;
          ctx.beginPath();
          ctx.ellipse(cx, cy + t * 0.08, t * 0.26, t * 0.2, 0, 0, TAU);
          ctx.fill();
          // grate bars
          ctx.strokeStyle = 'rgba(150,158,176,0.5)';
          ctx.lineWidth = Math.max(1, t * 0.022);
          for (let i = -2; i <= 2; i++) {
            const yy = cy + i * t * 0.1;
            const halfW = t * 0.36 * Math.sqrt(Math.max(0, 1 - (i / 3.2) ** 2));
            ctx.beginPath();
            ctx.moveTo(cx - halfW, yy);
            ctx.lineTo(cx + halfW, yy);
            ctx.stroke();
          }
          // rim
          ctx.strokeStyle = 'rgba(255,140,60,0.55)';
          ctx.lineWidth = Math.max(1.4, t * 0.026);
          ctx.beginPath();
          ctx.ellipse(cx, cy, t * 0.37, t * 0.33, 0, 0, TAU);
          ctx.stroke();
          ctx.strokeStyle = 'rgba(120,130,150,0.35)';
          ctx.lineWidth = Math.max(1, t * 0.016);
          ctx.beginPath();
          ctx.ellipse(cx, cy, t * 0.42, t * 0.38, 0, 0, TAU);
          ctx.stroke();
        }
      }
    }
    this.boardCache = canvas;
  }

  private drawBoardFrame(ctx: CanvasRenderingContext2D): void {
    if (!this.battle) return;
    const t = this.tile;
    const w = t * this.battle.board.w;
    const h = t * this.battle.board.h;
    ctx.save();
    // outer frame
    ctx.fillStyle = 'rgba(6,8,13,0.85)';
    ctx.strokeStyle = 'rgba(120,140,175,0.14)';
    ctx.lineWidth = Math.max(1.5, t * 0.03);
    ctx.beginPath();
    ctx.rect(this.ox - t * 0.12, this.oy - t * 0.12, w + t * 0.24, h + t * 0.24);
    ctx.fill();
    ctx.stroke();
    this.ensureBoardCache();
    if (this.boardCache) ctx.drawImage(this.boardCache, this.ox, this.oy, w, h);
    // corner rivets
    ctx.fillStyle = 'rgba(190,160,120,0.35)';
    for (const [cx, cy] of [
      [this.ox - t * 0.12, this.oy - t * 0.12],
      [this.ox + w + t * 0.12, this.oy - t * 0.12],
      [this.ox - t * 0.12, this.oy + h + t * 0.12],
      [this.ox + w + t * 0.12, this.oy + h + t * 0.12],
    ]) {
      ctx.beginPath();
      ctx.arc(cx, cy, Math.max(2, t * 0.035), 0, TAU);
      ctx.fill();
    }
    ctx.restore();
  }

  // ── telegraphs & highlights ───────────────────────────────────────────────
  private drawTelegraphs(ctx: CanvasRenderingContext2D): void {
    const b = this.battle;
    if (!b) return;
    const t = this.tile;
    const pulse = 0.5 + 0.5 * Math.sin(this.time * 4.2);
    for (const e of b.enemies) {
      if (e.hp <= 0 || !e.intent) continue;
      const v = this.enemyVis.get(e.uid);
      const ex = this.ox + ((v?.x ?? e.pos.x) + 0.5) * t;
      const ey = this.oy + ((v?.y ?? e.pos.y) + 0.5) * t;

      if (e.intent.kind === 'attack' && e.intent.target) {
        const c = this.tileCenter(e.intent.target);
        ctx.save();
        ctx.globalAlpha = 0.16 + pulse * 0.12;
        ctx.fillStyle = C.blood;
        ctx.beginPath();
        ctx.rect(c.x - t / 2 + t * 0.04, c.y - t / 2 + t * 0.04, t - t * 0.08, t - t * 0.08);
        ctx.fill();
        ctx.globalAlpha = 0.5 + pulse * 0.4;
        ctx.strokeStyle = C.blood;
        ctx.lineWidth = Math.max(1.5, t * 0.035);
        ctx.setLineDash([t * 0.14, t * 0.1]);
        ctx.strokeRect(c.x - t / 2 + t * 0.07, c.y - t / 2 + t * 0.07, t - t * 0.14, t - t * 0.14);
        ctx.restore();
        // dashed arrow
        ctx.save();
        ctx.globalAlpha = 0.55;
        ctx.strokeStyle = e.intent.style === 'ranged' ? '#ff9d63' : C.blood;
        ctx.lineWidth = Math.max(1.2, t * 0.022);
        ctx.setLineDash([t * 0.1, t * 0.12]);
        ctx.beginPath();
        ctx.moveTo(ex, ey);
        const midX = (ex + c.x) / 2 + (ey - c.y) * 0.14;
        const midY = (ey + c.y) / 2 + (c.x - ex) * 0.14;
        ctx.quadraticCurveTo(midX, midY, c.x, c.y);
        ctx.stroke();
        ctx.restore();
        // damage badge
        this.badge(ctx, c.x + t * 0.26, c.y - t * 0.26, String(e.dmg), C.blood);
      } else if (e.intent.kind === 'special') {
        const r = t * 2.2;
        ctx.save();
        ctx.globalAlpha = 0.12 + pulse * 0.12;
        ctx.fillStyle = C.blood;
        ctx.beginPath();
        ctx.arc(ex, ey, r, 0, TAU);
        ctx.fill();
        ctx.globalAlpha = 0.55;
        ctx.strokeStyle = C.blood;
        ctx.lineWidth = Math.max(2, t * 0.04);
        ctx.setLineDash([t * 0.16, t * 0.12]);
        ctx.beginPath();
        ctx.arc(ex, ey, r, 0, TAU);
        ctx.stroke();
        ctx.restore();
        this.badge(ctx, ex, ey - t * 0.62, '!', C.blood);
      } else if (e.intent.kind === 'move') {
        ctx.save();
        ctx.globalAlpha = 0.3;
        ctx.strokeStyle = 'rgba(220,150,120,0.8)';
        ctx.lineWidth = Math.max(1, t * 0.02);
        ctx.setLineDash([t * 0.08, t * 0.14]);
        ctx.beginPath();
        ctx.moveTo(ex, ey);
        for (const step of e.intent.path) {
          const c = this.tileCenter(step);
          ctx.lineTo(c.x, c.y);
        }
        ctx.stroke();
        ctx.restore();
      }
    }
  }

  private badge(ctx: CanvasRenderingContext2D, x: number, y: number, text: string, color: string): void {
    const t = this.tile;
    const r = t * 0.14;
    ctx.save();
    ctx.fillStyle = 'rgba(8,10,15,0.9)';
    ctx.strokeStyle = color;
    ctx.lineWidth = Math.max(1, t * 0.02);
    ctx.beginPath();
    ctx.arc(x, y, r, 0, TAU);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = color;
    ctx.font = `600 ${Math.max(9, t * 0.17)}px Georgia, serif`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, x, y + t * 0.005);
    ctx.restore();
  }

  private drawHighlights(ctx: CanvasRenderingContext2D): void {
    const t = this.tile;
    const pulse = 0.5 + 0.5 * Math.sin(this.time * 5);
    for (const p of this.highlights) {
      const c = this.tileCenter(p);
      ctx.save();
      const color = this.highlightKind === 'ability' ? C.emberHot : this.highlightKind === 'move' ? '#7fd8ff' : C.blood;
      ctx.globalAlpha = 0.1 + pulse * 0.06;
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.roundRect(c.x - t / 2 + t * 0.06, c.y - t / 2 + t * 0.06, t - t * 0.12, t - t * 0.12, t * 0.1);
      ctx.fill();
      ctx.globalAlpha = 0.75 + pulse * 0.25;
      ctx.strokeStyle = color;
      ctx.lineWidth = Math.max(1.6, t * 0.032);
      ctx.stroke();
      // corner ticks
      ctx.globalAlpha = 0.9;
      ctx.lineWidth = Math.max(1.2, t * 0.024);
      const cx0 = c.x - t / 2 + t * 0.06;
      const cy0 = c.y - t / 2 + t * 0.06;
      const cw = t - t * 0.12;
      const tick = t * 0.13;
      ctx.beginPath();
      ctx.moveTo(cx0, cy0 + tick); ctx.lineTo(cx0, cy0); ctx.lineTo(cx0 + tick, cy0);
      ctx.moveTo(cx0 + cw - tick, cy0); ctx.lineTo(cx0 + cw, cy0); ctx.lineTo(cx0 + cw, cy0 + tick);
      ctx.moveTo(cx0 + cw, cy0 + cw - tick); ctx.lineTo(cx0 + cw, cy0 + cw); ctx.lineTo(cx0 + cw - tick, cy0 + cw);
      ctx.moveTo(cx0 + tick, cy0 + cw); ctx.lineTo(cx0, cy0 + cw); ctx.lineTo(cx0, cy0 + cw - tick);
      ctx.stroke();
      ctx.restore();
    }
    if (this.path.length) {
      ctx.save();
      ctx.strokeStyle = 'rgba(140,220,255,0.7)';
      ctx.lineWidth = Math.max(2, t * 0.06);
      ctx.lineCap = 'round';
      ctx.beginPath();
      const start = this.tileCenter(this.battle!.hero.pos);
      ctx.moveTo(start.x, start.y);
      for (const step of this.path) {
        const c = this.tileCenter(step);
        ctx.lineTo(c.x, c.y);
      }
      ctx.stroke();
      ctx.restore();
    }
  }

  // ── forge ─────────────────────────────────────────────────────────────────
  private drawForge(ctx: CanvasRenderingContext2D): void {
    const b = this.battle;
    if (!b) return;
    const t = this.tile;
    const tiles = b.forge.tiles;
    if (!tiles.length) return;
    const xs = tiles.map((p) => p.x);
    const ys = tiles.map((p) => p.y);
    const minX = Math.min(...xs);
    const maxX = Math.max(...xs);
    const y = ys[0];
    const cx = this.ox + ((minX + maxX) / 2 + 0.5) * t;
    const cy = this.oy + (y + 0.5) * t;
    const w = (maxX - minX + 1) * t;
    const flicker = 0.85 + 0.15 * Math.sin(this.time * 5.3) + 0.05 * Math.sin(this.time * 13.7);
    const health = b.forge.hp / b.forge.maxHp;

    ctx.save();
    // ambient glow
    const glow = ctx.createRadialGradient(cx, cy, t * 0.1, cx, cy, t * 1.9);
    glow.addColorStop(0, `rgba(255,140,60,${0.32 * flicker})`);
    glow.addColorStop(0.45, `rgba(255,100,40,${0.12 * flicker})`);
    glow.addColorStop(1, 'rgba(255,80,30,0)');
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(cx, cy, t * 1.9, 0, TAU);
    ctx.fill();

    // ember pit beneath the anvil
    const pitGrad = ctx.createRadialGradient(cx, cy + t * 0.22, t * 0.02, cx, cy + t * 0.22, t * 0.42);
    pitGrad.addColorStop(0, '#ffe6a8');
    pitGrad.addColorStop(0.35, '#ff9a3e');
    pitGrad.addColorStop(0.75, '#8a2c10');
    pitGrad.addColorStop(1, 'rgba(60,20,8,0)');
    ctx.fillStyle = pitGrad;
    ctx.beginPath();
    ctx.ellipse(cx, cy + t * 0.22, w * 0.36, t * 0.19, 0, 0, TAU);
    ctx.fill();

    // coal bed
    ctx.fillStyle = 'rgba(30,16,10,0.9)';
    for (let i = 0; i < 7; i++) {
      const a = (i / 7) * TAU;
      ctx.beginPath();
      ctx.ellipse(cx + Math.cos(a) * w * 0.24, cy + t * 0.22 + Math.sin(a) * t * 0.1, t * 0.05, t * 0.035, a, 0, TAU);
      ctx.fill();
    }

    // plinth
    const plinth = ctx.createLinearGradient(0, cy - t * 0.1, 0, cy + t * 0.16);
    plinth.addColorStop(0, '#4a4f5f');
    plinth.addColorStop(1, '#20242e');
    ctx.fillStyle = plinth;
    ctx.beginPath();
    ctx.roundRect(cx - w * 0.3, cy - t * 0.02, w * 0.6, t * 0.2, t * 0.04);
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,170,100,0.35)';
    ctx.lineWidth = Math.max(1, t * 0.016);
    ctx.stroke();

    // anvil
    ctx.save();
    ctx.translate(cx, cy - t * 0.04);
    drawAnvil(ctx, t * 1.0, flicker, false);
    ctx.restore();

    // forge integrity cracks
    if (health < 0.66) {
      ctx.save();
      ctx.strokeStyle = `rgba(20,10,8,${0.75})`;
      ctx.lineWidth = Math.max(1.4, t * 0.03);
      const cracks = health < 0.33 ? 4 : 2;
      for (let i = 0; i < cracks; i++) {
        const bx = cx - w / 2 + t * (0.25 + i * 0.5);
        ctx.beginPath();
        ctx.moveTo(bx, cy - t * 0.5);
        ctx.lineTo(bx + t * 0.06, cy - t * 0.2);
        ctx.lineTo(bx - t * 0.04, cy + t * 0.05);
        ctx.stroke();
      }
      ctx.restore();
    }

    // rising embers from the forge
    if (Math.random() < 0.22) {
      this.particles.push({
        x: cx + (Math.random() - 0.5) * w * 0.8,
        y: cy - t * 0.1,
        vx: (Math.random() - 0.5) * 20,
        vy: -30 - Math.random() * 50,
        life: 1 + Math.random(),
        max: 2,
        size: 1 + Math.random() * 2.2,
        color: C.emberHot,
        grav: -14,
        drag: 0.99,
        kind: 'ember',
      });
    }
  }

  // ── entities ──────────────────────────────────────────────────────────────
  private drawEntities(ctx: CanvasRenderingContext2D): void {
    const b = this.battle;
    if (!b) return;
    const t = this.tile;
    const list: { y: number; draw: () => void }[] = [];

    for (const d of this.dying) {
      const age = (performance.now() - d.at) / 700;
      if (age >= 1) continue;
      list.push({
        y: d.y,
        draw: () => {
          const c = this.tileCenter({ x: d.x, y: d.y });
          ctx.save();
          ctx.globalAlpha = Math.max(0, 1 - age) * 0.8;
          ctx.translate(c.x, c.y + (d.y - Math.floor(d.y)) * 0);
          ctx.scale(1 + age * 0.3, 1 + age * 0.3);
          drawEnemyBody(ctx, d.typeId, t, 0, true);
          ctx.restore();
        },
      });
    }

    for (const e of b.enemies) {
      if (e.hp <= 0) continue;
      const v = this.enemyVis.get(e.uid);
      const gx = v?.x ?? e.pos.x;
      const gy = v?.y ?? e.pos.y;
      const c = this.tileCenter({ x: gx, y: gy });
      const spawnAge = v ? Math.min(1, (performance.now() - v.born) / 320) : 1;
      list.push({
        y: gy,
        draw: () => {
          if (e.boss || e.elite) this.drawTierRing(ctx, c.x, c.y, e.boss ? 'boss' : 'elite');
          ctx.save();
          ctx.translate(c.x, c.y);
          const tierScale = e.boss ? 1.5 : e.elite ? 1.12 : 1;
          const scale = (0.55 + 0.45 * easeOut(spawnAge)) * tierScale;
          ctx.scale(scale, scale);
          ctx.globalAlpha = easeOut(spawnAge);
          // shadow
          ctx.fillStyle = 'rgba(0,0,0,0.42)';
          ctx.beginPath();
          ctx.ellipse(0, t * 0.3, t * (e.boss ? 0.36 : 0.24), t * 0.09, 0, 0, TAU);
          ctx.fill();
          ctx.shadowColor = 'rgba(0,0,0,0.9)';
          ctx.shadowBlur = t * 0.16;
          ctx.shadowOffsetY = t * 0.03;
          drawEnemyBody(ctx, e.typeId, t, this.time + e.uid, false, v?.flash ?? 0);
          ctx.shadowBlur = 0;
          ctx.shadowOffsetY = 0;
          ctx.restore();
          this.drawEnemyStatus(ctx, e, c.x, c.y, e.boss ? 1.5 : e.elite ? 1.12 : 1);
          if (this.hover && eq(this.hover, e.pos)) {
            ctx.save();
            ctx.strokeStyle = 'rgba(255,235,200,0.75)';
            ctx.lineWidth = Math.max(1.4, t * 0.024);
            ctx.setLineDash([t * 0.08, t * 0.07]);
            ctx.beginPath();
            ctx.arc(c.x, c.y, t * (e.boss ? 0.52 : 0.4), 0, TAU);
            ctx.stroke();
            ctx.restore();
          }
        },
      });
    }

    if (this.heroVis) {
      const hv = this.heroVis;
      const c = this.tileCenter(hv);
      list.push({
        y: hv.y + 0.01,
        draw: () => {
          ctx.save();
          ctx.translate(c.x, c.y);
          ctx.fillStyle = 'rgba(0,0,0,0.45)';
          ctx.beginPath();
          ctx.ellipse(0, t * 0.31, t * 0.22, t * 0.08, 0, 0, TAU);
          ctx.fill();
          ctx.shadowColor = 'rgba(0,0,0,0.9)';
          ctx.shadowBlur = t * 0.18;
          ctx.shadowOffsetY = t * 0.03;
          drawHero(ctx, b.hero.heroId, t, this.time, hv.flash);
          ctx.shadowBlur = 0;
          ctx.shadowOffsetY = 0;
          ctx.restore();
          this.drawHeroGuard(ctx, b, c.x, c.y);
        },
      });
    }

    list.sort((a, c2) => a.y - c2.y);
    for (const item of list) item.draw();
  }

  private drawTierRing(ctx: CanvasRenderingContext2D, cx: number, cy: number, tier: 'elite' | 'boss'): void {
    const t = this.tile;
    const pulse = 0.5 + 0.5 * Math.sin(this.time * (tier === 'boss' ? 2.2 : 3.1));
    const r = t * (tier === 'boss' ? 0.52 : 0.38);
    ctx.save();
    const g = ctx.createRadialGradient(cx, cy, r * 0.2, cx, cy, r * 1.5);
    const tint = tier === 'boss' ? '160,70,200' : '255,190,110';
    g.addColorStop(0, `rgba(${tint},0.22)`);
    g.addColorStop(0.7, `rgba(${tint},0.06)`);
    g.addColorStop(1, `rgba(${tint},0)`);
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(cx, cy, r * 1.5, 0, TAU);
    ctx.fill();
    ctx.strokeStyle = tier === 'boss' ? `rgba(214,150,255,${0.35 + pulse * 0.3})` : `rgba(255,196,120,${0.3 + pulse * 0.25})`;
    ctx.lineWidth = Math.max(1.2, t * 0.02);
    ctx.setLineDash(tier === 'boss' ? [t * 0.1, t * 0.08] : [t * 0.05, t * 0.09]);
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, TAU);
    ctx.stroke();
    // rune ticks
    ctx.setLineDash([]);
    ctx.beginPath();
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * TAU + this.time * (tier === 'boss' ? 0.5 : 0.8);
      ctx.moveTo(cx + Math.cos(a) * r * 1.12, cy + Math.sin(a) * r * 1.12);
      ctx.lineTo(cx + Math.cos(a) * r * (tier === 'boss' ? 1.34 : 1.24), cy + Math.sin(a) * r * (tier === 'boss' ? 1.34 : 1.24));
    }
    ctx.stroke();
    ctx.restore();
  }

  private drawEnemyStatus(ctx: CanvasRenderingContext2D, e: EnemyState, cx: number, cy: number, tier = 1): void {
    const t = this.tile;
    const w = t * (e.boss ? 0.78 : 0.52);
    const h = Math.max(3, t * 0.058);
    const y = cy - t * 0.46 * tier;
    ctx.save();
    ctx.fillStyle = 'rgba(0,0,0,0.6)';
    ctx.beginPath();
    ctx.roundRect(cx - w / 2, y, w, h, h / 2);
    ctx.fill();
    const frac = Math.max(0, e.hp / e.maxHp);
    const g = ctx.createLinearGradient(cx - w / 2, 0, cx + w / 2, 0);
    g.addColorStop(0, '#e2504f');
    g.addColorStop(1, '#ff9d63');
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.roundRect(cx - w / 2, y, w * frac, h, h / 2);
    ctx.fill();
    if (e.shield > 0) {
      ctx.strokeStyle = 'rgba(150,215,255,0.9)';
      ctx.lineWidth = Math.max(1, t * 0.018);
      ctx.beginPath();
      ctx.roundRect(cx - w / 2 - h * 0.3, y - h * 0.3, w + h * 0.6, h + h * 0.6, h);
      ctx.stroke();
    }
    if (e.stun > 0) {
      ctx.fillStyle = '#ffd479';
      ctx.font = `600 ${t * 0.15}px Georgia, serif`;
      ctx.textAlign = 'center';
      ctx.fillText('✦', cx, y - h * 1.2);
    }
    ctx.restore();
  }

  private drawHeroGuard(ctx: CanvasRenderingContext2D, b: Battle, cx: number, cy: number): void {
    const t = this.tile;
    if (b.hero.shield > 0) {
      const pulse = 0.5 + 0.5 * Math.sin(this.time * 3);
      ctx.save();
      ctx.globalCompositeOperation = 'lighter';
      ctx.globalAlpha = 0.16 + pulse * 0.1;
      ctx.fillStyle = '#2f6f96';
      ctx.beginPath();
      ctx.arc(cx, cy, t * 0.36, 0, TAU);
      ctx.fill();
      ctx.globalAlpha = 0.55 + pulse * 0.3;
      ctx.strokeStyle = 'rgba(160,220,255,0.9)';
      ctx.lineWidth = Math.max(1.2, t * 0.022);
      ctx.setLineDash([t * 0.16, t * 0.1]);
      ctx.beginPath();
      ctx.arc(cx, cy, t * 0.37, 0, TAU);
      ctx.stroke();
      ctx.restore();
    }
    if (this.selectedAbility !== null && b.hero.abilities[this.selectedAbility]) {
      ctx.save();
      ctx.strokeStyle = 'rgba(255,200,120,0.55)';
      ctx.lineWidth = Math.max(1.2, t * 0.02);
      ctx.setLineDash([t * 0.09, t * 0.09]);
      ctx.beginPath();
      ctx.arc(cx, cy, t * 0.44, 0, TAU);
      ctx.stroke();
      ctx.restore();
    }
  }

  private drawParticles(ctx: CanvasRenderingContext2D): void {
    ctx.save();
    for (const p of this.particles) {
      const a = Math.max(0, Math.min(1, p.life / 0.9));
      ctx.globalAlpha = p.kind === 'smoke' ? a * 0.35 : a;
      ctx.fillStyle = p.color;
      if (p.kind === 'smoke') {
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size * (1.6 - a), 0, TAU);
        ctx.fill();
      } else if (p.kind === 'debris') {
        ctx.save();
        ctx.translate(p.x, p.y);
        ctx.rotate(p.x * 0.05 + p.life * 6);
        ctx.fillRect(-p.size / 2, -p.size / 2, p.size, p.size);
        ctx.restore();
      } else {
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size, 0, TAU);
        ctx.fill();
      }
    }
    ctx.restore();
  }

  private drawBeams(ctx: CanvasRenderingContext2D): void {
    for (const beam of this.beams) {
      const age = (performance.now() - beam.at) / 400;
      const a = this.tileCenter(beam.from);
      const c = this.tileCenter(beam.to);
      ctx.save();
      ctx.globalAlpha = Math.max(0, 1 - age);
      ctx.strokeStyle = beam.color;
      ctx.lineWidth = this.tile * 0.09 * (1 - age * 0.7);
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(c.x, c.y);
      ctx.stroke();
      ctx.restore();
    }
  }

  private drawFloats(ctx: CanvasRenderingContext2D): void {
    for (const f of this.floats) {
      const a = Math.max(0, Math.min(1, f.life / f.max));
      ctx.save();
      ctx.globalAlpha = a;
      ctx.font = `700 ${f.size}px Georgia, 'Times New Roman', serif`;
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      const color = f.tone === 'bad' ? '#ff8d80' : f.tone === 'good' ? '#ffe2a3' : C.emberHot;
      ctx.lineWidth = Math.max(2, f.size * 0.16);
      ctx.strokeStyle = 'rgba(6,8,12,0.9)';
      ctx.strokeText(f.text, f.x, f.y);
      ctx.fillStyle = color;
      ctx.fillText(f.text, f.x, f.y);
      ctx.restore();
    }
  }

  private drawHover(ctx: CanvasRenderingContext2D): void {
    if (!this.hover || !this.battle) return;
    const c = this.tileCenter(this.hover);
    const t = this.tile;
    ctx.save();
    ctx.strokeStyle = 'rgba(255,235,200,0.55)';
    ctx.lineWidth = Math.max(1, t * 0.022);
    ctx.beginPath();
    ctx.roundRect(c.x - t / 2 + t * 0.05, c.y - t / 2 + t * 0.05, t - t * 0.1, t - t * 0.1, t * 0.08);
    ctx.stroke();
    ctx.restore();
  }

  private drawVignette(ctx: CanvasRenderingContext2D): void {
    const g = ctx.createRadialGradient(this.width / 2, this.height / 2, Math.min(this.width, this.height) * 0.24, this.width / 2, this.height / 2, Math.max(this.width, this.height) * 0.78);
    g.addColorStop(0, 'rgba(0,0,0,0)');
    g.addColorStop(1, 'rgba(0,0,0,0.72)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, this.width, this.height);
  }

  private drawTitleArt(ctx: CanvasRenderingContext2D): void {
    const t = Math.min(this.width, this.height) * 0.075;
    const cx = this.width / 2;
    const cy = this.height * 0.86;
    // faint rune circle behind the logo area
    const rx = Math.min(this.width * 0.3, this.height * 0.36);
    const ry = rx;
    ctx.save();
    ctx.translate(this.width / 2, this.height * 0.4);
    ctx.globalAlpha = 0.12;
    ctx.strokeStyle = '#ff9a55';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.ellipse(0, 0, rx, ry, 0, 0, TAU);
    ctx.stroke();
    ctx.beginPath();
    ctx.ellipse(0, 0, rx * 0.86, ry * 0.86, 0, 0, TAU);
    ctx.stroke();
    for (let i = 0; i < 24; i++) {
      const a = (i / 24) * TAU + this.time * 0.04;
      const len = i % 3 === 0 ? 0.12 : 0.05;
      ctx.beginPath();
      ctx.moveTo(Math.cos(a) * rx * 0.86, Math.sin(a) * ry * 0.86);
      ctx.lineTo(Math.cos(a) * rx * (0.86 + len), Math.sin(a) * ry * (0.86 + len));
      ctx.stroke();
    }
    ctx.globalAlpha = 0.06;
    // hexagram sigil: two interlocked triangles, slowly counter-rotating
    for (const [offset, spin, alpha] of [
      [-Math.PI / 2, 0.02, 1],
      [Math.PI / 2, -0.02, 1],
    ]) {
      ctx.save();
      ctx.globalAlpha = 0.06 * alpha;
      ctx.beginPath();
      for (let i = 0; i < 3; i++) {
        const a = offset + this.time * spin + (i / 3) * TAU;
        const x = Math.cos(a) * rx * 0.92;
        const y = Math.sin(a) * rx * 0.92;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.closePath();
      ctx.stroke();
      ctx.restore();
    }
    ctx.restore();
    const flicker = 0.85 + 0.15 * Math.sin(this.time * 4.1);
    const glow = ctx.createRadialGradient(cx, cy, t * 0.2, cx, cy, t * 4.6);
    glow.addColorStop(0, `rgba(255,140,60,${0.26 * flicker})`);
    glow.addColorStop(0.5, 'rgba(200,80,30,0.08)');
    glow.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, this.width, this.height);

    ctx.save();
    ctx.translate(cx, cy);
    drawAnvil(ctx, t * 2.6, flicker, true);
    ctx.restore();
  }
}

// ── creature art ─────────────────────────────────────────────────────────────
function easeOut(v: number): number {
  return 1 - Math.pow(1 - v, 3);
}

function blob(ctx: CanvasRenderingContext2D, rx: number, ry: number, lobes: number, seed: number, wobble = 0): void {
  ctx.beginPath();
  for (let i = 0; i <= 24; i++) {
    const a = (i / 24) * TAU;
    const w = 1 + Math.sin(a * lobes + seed) * 0.07 + Math.sin(a * (lobes + 2) + seed * 2) * 0.04 + wobble;
    const x = Math.cos(a) * rx * w;
    const y = Math.sin(a) * ry * w;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.closePath();
}

function eyes(ctx: CanvasRenderingContext2D, x: number, y: number, spread: number, r: number, color = '#ffd479'): void {
  for (const dir of [-1, 1]) {
    const ex = x + dir * spread;
    // socket
    ctx.fillStyle = 'rgba(8,5,7,0.78)';
    ctx.beginPath();
    ctx.ellipse(ex, y, r * 1.9, r * 1.55, dir * 0.12, 0, TAU);
    ctx.fill();
    // ember glow
    const g = ctx.createRadialGradient(ex, y, 0, ex, y, r * 3.4);
    g.addColorStop(0, color);
    g.addColorStop(0.32, 'rgba(255,150,70,0.5)');
    g.addColorStop(1, 'rgba(255,110,40,0)');
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(ex, y, r * 3.4, 0, TAU);
    ctx.fill();
    // pupil slit
    ctx.fillStyle = 'rgba(30,12,6,0.9)';
    ctx.beginPath();
    ctx.ellipse(ex, y, r * 0.34, r * 0.92, 0, 0, TAU);
    ctx.fill();
    // hot core
    ctx.fillStyle = 'rgba(255,245,220,0.95)';
    ctx.beginPath();
    ctx.arc(ex - r * 0.22, y - r * 0.3, r * 0.3, 0, TAU);
    ctx.fill();
  }
}

function glowSprite(ctx: CanvasRenderingContext2D, r: number, color: string, alpha = 0.5): void {
  const g = ctx.createRadialGradient(0, 0, 0, 0, 0, r);
  g.addColorStop(0, color);
  g.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.globalAlpha = alpha;
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(0, 0, r, 0, TAU);
  ctx.fill();
  ctx.globalAlpha = 1;
}

function shadowFoot(ctx: CanvasRenderingContext2D, t: number, rx: number): void {
  ctx.fillStyle = 'rgba(0,0,0,0.45)';
  ctx.beginPath();
  ctx.ellipse(0, t * 0.32, t * rx, t * rx * 0.34, 0, 0, TAU);
  ctx.fill();
}

function crack(ctx: CanvasRenderingContext2D, pts: [number, number][], w: number, color = 'rgba(255,150,70,0.75)'): void {
  ctx.strokeStyle = color;
  ctx.lineWidth = w;
  ctx.lineCap = 'round';
  ctx.beginPath();
  pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
  ctx.stroke();
}

function teethRing(ctx: CanvasRenderingContext2D, cx: number, cy: number, rx: number, ry: number, count: number, len: number, color: string, w: number): void {
  ctx.strokeStyle = color;
  ctx.lineWidth = w;
  for (let i = 0; i < count; i++) {
    const a = (i / count) * TAU;
    const px = cx + Math.cos(a) * rx;
    const py = cy + Math.sin(a) * ry;
    ctx.beginPath();
    ctx.moveTo(px, py);
    ctx.lineTo(cx + Math.cos(a) * (rx - len), cy + Math.sin(a) * (ry - len));
    ctx.stroke();
  }
}

function innerEmber(ctx: CanvasRenderingContext2D, s: number, color = '255,150,70'): void {
  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  const g = ctx.createRadialGradient(0, -s * 0.02, 0, 0, -s * 0.02, s * 0.3);
  g.addColorStop(0, `rgba(${color},0.22)`);
  g.addColorStop(0.55, `rgba(${color},0.08)`);
  g.addColorStop(1, `rgba(${color},0)`);
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(0, 0, s * 0.3, 0, TAU);
  ctx.fill();
  ctx.restore();
}

function rimLight(ctx: CanvasRenderingContext2D, s: number, color = '255,190,130'): void {
  // Faint warm bounce from the forge below, hugging the lower silhouette.
  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  const g = ctx.createRadialGradient(0, s * 0.34, 0, 0, s * 0.34, s * 0.4);
  g.addColorStop(0, `rgba(${color},0.07)`);
  g.addColorStop(0.6, `rgba(${color},0.025)`);
  g.addColorStop(1, `rgba(${color},0)`);
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.ellipse(0, s * 0.3, s * 0.34, s * 0.14, 0, 0, TAU);
  ctx.fill();
  ctx.restore();
}

// ── props ────────────────────────────────────────────────────────────────────
/** Anvil silhouette in a roughly -0.62..0.44 unit box; `s` is its width scale. */
function anvilPath(ctx: CanvasRenderingContext2D, s: number): void {
  const pts: [number, number][] = [
    [-0.62, -0.23],
    [-0.4, -0.31],
    [-0.02, -0.31],
    [0.26, -0.31],
    [0.42, -0.23],
    [0.42, -0.14],
    [0.16, -0.08],
    [0.09, 0.06],
    [0.19, 0.22],
    [0.3, 0.36],
    [0.3, 0.44],
    [-0.28, 0.44],
    [-0.28, 0.36],
    [-0.17, 0.22],
    [-0.07, 0.06],
    [-0.19, -0.08],
    [-0.36, -0.13],
  ];
  ctx.beginPath();
  pts.forEach(([x, y], i) => (i ? ctx.lineTo(x * s, y * s) : ctx.moveTo(x * s, y * s)));
  ctx.closePath();
}

/** The ANVILIGHT forge-anvil: dark iron body, glowing working face, optional hammer. */
function drawAnvil(ctx: CanvasRenderingContext2D, s: number, flicker: number, withHammer = true): void {
  const body = ctx.createLinearGradient(-0.2 * s, -0.31 * s, 0.15 * s, 0.44 * s);
  body.addColorStop(0, '#2c313d');
  body.addColorStop(0.45, '#1b1f28');
  body.addColorStop(1, '#0b0d12');
  anvilPath(ctx, s);
  ctx.fillStyle = body;
  ctx.fill();
  ctx.strokeStyle = `rgba(255,178,96,${0.5 + 0.25 * flicker})`;
  ctx.lineWidth = Math.max(1.2, s * 0.016);
  ctx.stroke();

  ctx.strokeStyle = 'rgba(176,196,226,0.22)';
  ctx.lineWidth = Math.max(1, s * 0.012);
  ctx.beginPath();
  ctx.moveTo(-0.58 * s, -0.2 * s);
  ctx.lineTo(0.2 * s, -0.27 * s);
  ctx.stroke();

  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  const face = ctx.createLinearGradient(-0.6 * s, 0, 0.4 * s, 0);
  face.addColorStop(0, `rgba(255,140,50,${0.3 * flicker})`);
  face.addColorStop(0.55, `rgba(255,214,150,${0.8 * flicker})`);
  face.addColorStop(1, `rgba(255,130,45,${0.26 * flicker})`);
  ctx.strokeStyle = face;
  ctx.lineWidth = Math.max(1.8, s * 0.028);
  ctx.beginPath();
  ctx.moveTo(-0.57 * s, -0.25 * s);
  ctx.lineTo(0.4 * s, -0.26 * s);
  ctx.stroke();
  ctx.restore();

  ctx.fillStyle = `rgba(255,196,130,${0.5 * flicker})`;
  for (const [rx, ry] of [
    [-0.03, 0.0],
    [0.03, 0.0],
  ]) {
    ctx.beginPath();
    ctx.arc(rx * s, ry * s, Math.max(1, s * 0.012), 0, TAU);
    ctx.fill();
  }

  if (withHammer) {
    // hammer leaning with its head resting on the working face, haft up to the right
    ctx.save();
    ctx.translate(0.26 * s, -0.3 * s);
    ctx.rotate(-0.5);
    const haft = ctx.createLinearGradient(0, 0, 0, -0.62 * s);
    haft.addColorStop(0, '#33261b');
    haft.addColorStop(1, '#5c462f');
    ctx.strokeStyle = haft;
    ctx.lineWidth = Math.max(1.6, s * 0.032);
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(0, 0.02 * s);
    ctx.lineTo(-0.02 * s, -0.6 * s);
    ctx.stroke();
    const head = ctx.createLinearGradient(0, -0.1 * s, 0, 0.08 * s);
    head.addColorStop(0, '#838d9e');
    head.addColorStop(0.45, '#525b6a');
    head.addColorStop(1, '#282e3a');
    ctx.fillStyle = head;
    ctx.beginPath();
    ctx.roundRect(-0.17 * s, -0.08 * s, 0.34 * s, 0.16 * s, s * 0.025);
    ctx.fill();
    ctx.strokeStyle = `rgba(255,180,110,${0.32 + 0.2 * flicker})`;
    ctx.lineWidth = Math.max(1, s * 0.012);
    ctx.stroke();
    ctx.fillStyle = `rgba(255,214,150,${0.5 * flicker})`;
    ctx.beginPath();
    ctx.roundRect(-0.17 * s, -0.08 * s, 0.05 * s, 0.16 * s, s * 0.02);
    ctx.fill();
    ctx.restore();
  }
}

// ── creatures ────────────────────────────────────────────────────────────────
function drawEnemyBody(ctx: CanvasRenderingContext2D, typeId: string, t: number, time: number, dying = false, flash = 0): void {
  const s = t * 1.0;
  const bob = Math.sin(time * 2.6) * s * 0.012;
  ctx.save();
  ctx.translate(0, bob);
  switch (typeId) {
    case 'fly': {
      const wing = Math.sin(time * 26) * 0.6;
      ctx.fillStyle = 'rgba(255,170,100,0.24)';
      for (const dir of [-1, 1]) {
        ctx.save();
        ctx.rotate(dir * (0.5 + wing * 0.2));
        ctx.beginPath();
        ctx.ellipse(dir * 0.16 * s, -0.16 * s, 0.17 * s, 0.055 * s, 0, 0, TAU);
        ctx.fill();
        ctx.restore();
      }
      glowSprite(ctx, 0.34 * s, 'rgba(255,170,80,0.5)', 0.6);
      const g = ctx.createRadialGradient(-0.05 * s, -0.06 * s, 0.02 * s, 0, 0, 0.2 * s);
      g.addColorStop(0, '#8d4a22');
      g.addColorStop(1, '#2a1208');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.ellipse(0, 0, 0.14 * s, 0.16 * s, 0, 0, TAU);
      ctx.fill();
      crack(ctx, [[-0.06 * s, 0.02 * s], [0, 0.06 * s], [0.05 * s, 0.02 * s]], Math.max(1, s * 0.018));
      eyes(ctx, 0, -0.03 * s, 0.05 * s, s * 0.018, '#ffd479');
      break;
    }
    case 'grunt': {
      shadowFoot(ctx, t, 0.2);
      // two stubby legs
      ctx.strokeStyle = '#2e1c14';
      ctx.lineWidth = s * 0.09;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(-0.1 * s, 0.16 * s);
      ctx.lineTo(-0.16 * s, 0.3 * s);
      ctx.moveTo(0.1 * s, 0.16 * s);
      ctx.lineTo(0.16 * s, 0.3 * s);
      ctx.stroke();
      // shell: angular crab carapace
      const g = ctx.createLinearGradient(-0.2 * s, -0.26 * s, 0.18 * s, 0.22 * s);
      g.addColorStop(0, '#8a5236');
      g.addColorStop(0.5, '#53301f');
      g.addColorStop(1, '#251611');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(-0.27 * s, 0.1 * s);
      ctx.lineTo(-0.22 * s, -0.12 * s);
      ctx.lineTo(-0.08 * s, -0.24 * s);
      ctx.lineTo(0.12 * s, -0.22 * s);
      ctx.lineTo(0.26 * s, -0.04 * s);
      ctx.lineTo(0.2 * s, 0.14 * s);
      ctx.lineTo(0, 0.2 * s);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = 'rgba(14,8,6,0.7)';
      ctx.lineWidth = Math.max(1, s * 0.022);
      ctx.stroke();
      // ember seams
      crack(ctx, [[-0.14 * s, -0.14 * s], [-0.04 * s, -0.02 * s], [-0.12 * s, 0.09 * s]], Math.max(1.2, s * 0.026));
      crack(ctx, [[0.1 * s, -0.16 * s], [0.16 * s, -0.02 * s]], Math.max(1.2, s * 0.022));
      // little claws
      ctx.strokeStyle = '#6b4227';
      ctx.lineWidth = s * 0.05;
      ctx.beginPath();
      ctx.moveTo(-0.26 * s, 0.02 * s);
      ctx.lineTo(-0.34 * s, 0.1 * s);
      ctx.moveTo(0.25 * s, 0.0 * s);
      ctx.lineTo(0.34 * s, 0.08 * s);
      ctx.stroke();
      eyes(ctx, 0, -0.05 * s, 0.085 * s, s * 0.022, '#ffcf7a');
      break;
    }
    case 'brute': {
      shadowFoot(ctx, t, 0.32);
      // legs
      ctx.fillStyle = '#2a1d16';
      ctx.beginPath();
      ctx.roundRect(-0.24 * s, 0.18 * s, 0.16 * s, 0.16 * s, s * 0.04);
      ctx.roundRect(0.08 * s, 0.18 * s, 0.16 * s, 0.16 * s, s * 0.04);
      ctx.fill();
      // torso slab
      const g = ctx.createLinearGradient(-0.3 * s, -0.34 * s, 0.26 * s, 0.24 * s);
      g.addColorStop(0, '#8a6a4a');
      g.addColorStop(0.45, '#4e3727');
      g.addColorStop(1, '#241a15');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(-0.34 * s, 0.2 * s);
      ctx.lineTo(-0.3 * s, -0.16 * s);
      ctx.lineTo(-0.14 * s, -0.32 * s);
      ctx.lineTo(0.16 * s, -0.32 * s);
      ctx.lineTo(0.32 * s, -0.12 * s);
      ctx.lineTo(0.34 * s, 0.2 * s);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = 'rgba(12,8,6,0.65)';
      ctx.lineWidth = Math.max(1, s * 0.02);
      ctx.stroke();
      // iron harness
      ctx.fillStyle = '#5a534a';
      ctx.beginPath();
      ctx.moveTo(-0.26 * s, -0.02 * s);
      ctx.lineTo(0.28 * s, -0.08 * s);
      ctx.lineTo(0.28 * s, 0.04 * s);
      ctx.lineTo(-0.26 * s, 0.1 * s);
      ctx.closePath();
      ctx.fill();
      ctx.fillStyle = '#7c7264';
      ctx.beginPath();
      ctx.roundRect(-0.1 * s, -0.1 * s, 0.2 * s, 0.14 * s, s * 0.03);
      ctx.fill();
      // huge fists
      ctx.fillStyle = '#4a3a2c';
      for (const dir of [-1, 1]) {
        ctx.beginPath();
        ctx.ellipse(dir * 0.4 * s, 0.06 * s, 0.11 * s, 0.13 * s, dir * 0.2, 0, TAU);
        ctx.fill();
      }
      // head low and wide, tusks
      ctx.fillStyle = '#33251c';
      ctx.beginPath();
      ctx.ellipse(0, -0.2 * s, 0.17 * s, 0.13 * s, 0, 0, TAU);
      ctx.fill();
      ctx.strokeStyle = '#d9c7a4';
      ctx.lineWidth = s * 0.03;
      ctx.beginPath();
      ctx.moveTo(-0.09 * s, -0.13 * s);
      ctx.quadraticCurveTo(-0.13 * s, -0.04 * s, -0.07 * s, 0.0 * s);
      ctx.moveTo(0.09 * s, -0.13 * s);
      ctx.quadraticCurveTo(0.13 * s, -0.04 * s, 0.07 * s, 0.0 * s);
      ctx.stroke();
      eyes(ctx, 0, -0.22 * s, 0.075 * s, s * 0.024, '#ffb066');
      break;
    }
    case 'spitter': {
      shadowFoot(ctx, t, 0.24);
      // root stalk
      ctx.strokeStyle = '#2f3a22';
      ctx.lineWidth = s * 0.09;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(0, 0.28 * s);
      ctx.quadraticCurveTo(-0.04 * s, 0.12 * s, 0.02 * s, 0.0 * s);
      ctx.stroke();
      // leaves
      for (const [dir, ang] of [[-1, -0.5], [1, 0.5], [-1, -1.1], [1, 1.1]] as [number, number][]) {
        ctx.save();
        ctx.translate(0, 0.14 * s);
        ctx.rotate(ang);
        const lg = ctx.createLinearGradient(0, 0, dir * 0.26 * s, 0);
        lg.addColorStop(0, '#2f3f22');
        lg.addColorStop(1, '#54683a');
        ctx.fillStyle = lg;
        ctx.beginPath();
        ctx.ellipse(dir * 0.13 * s, 0, 0.14 * s, 0.045 * s, 0, 0, TAU);
        ctx.fill();
        ctx.restore();
      }
      // pod head
      const g = ctx.createRadialGradient(-0.04 * s, -0.14 * s, 0.02 * s, 0, -0.1 * s, 0.22 * s);
      g.addColorStop(0, '#6e5a34');
      g.addColorStop(1, '#221a12');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.ellipse(0, -0.1 * s, 0.19 * s, 0.17 * s, 0, 0, TAU);
      ctx.fill();
      // muzzle
      ctx.fillStyle = '#1b1610';
      ctx.beginPath();
      ctx.ellipse(0.02 * s, -0.2 * s, 0.07 * s, 0.08 * s, 0.2, 0, TAU);
      ctx.fill();
      glowSprite(ctx, 0.16 * s, 'rgba(255,150,70,0.55)', 0.7);
      ctx.fillStyle = '#ffb066';
      ctx.beginPath();
      ctx.arc(0.02 * s, -0.2 * s, 0.035 * s, 0, TAU);
      ctx.fill();
      // spines
      ctx.strokeStyle = '#8a7550';
      ctx.lineWidth = Math.max(1, s * 0.02);
      for (let i = 0; i < 5; i++) {
        const a = -2.6 + i * 0.42;
        ctx.beginPath();
        ctx.moveTo(Math.cos(a) * 0.16 * s, -0.1 * s + Math.sin(a) * 0.14 * s);
        ctx.lineTo(Math.cos(a) * 0.24 * s, -0.1 * s + Math.sin(a) * 0.21 * s);
        ctx.stroke();
      }
      eyes(ctx, 0, -0.08 * s, 0.07 * s, s * 0.019, '#ffb066');
      break;
    }
    case 'bloater': {
      shadowFoot(ctx, t, 0.26);
      const pulse = 1 + Math.sin(time * 4.6) * 0.045;
      glowSprite(ctx, 0.34 * s, 'rgba(255,120,50,0.35)', 0.6);
      const g = ctx.createRadialGradient(-0.08 * s, -0.1 * s, 0.03 * s, 0, 0, 0.3 * s);
      g.addColorStop(0, '#9c6f45');
      g.addColorStop(0.55, '#4d3423');
      g.addColorStop(1, '#1b110c');
      ctx.fillStyle = g;
      blob(ctx, 0.29 * s * pulse, 0.27 * s * pulse, 8, 4.4);
      ctx.fill();
      ctx.strokeStyle = 'rgba(14,8,6,0.6)';
      ctx.lineWidth = Math.max(1, s * 0.02);
      ctx.stroke();
      // swollen veins
      for (let i = 0; i < 5; i++) {
        const a = (i / 5) * TAU + 0.3;
        crack(
          ctx,
          [
            [Math.cos(a) * 0.26 * s, Math.sin(a) * 0.24 * s],
            [Math.cos(a + 0.6) * 0.16 * s, Math.sin(a + 0.6) * 0.15 * s],
            [Math.cos(a + 1.1) * 0.06 * s, Math.sin(a + 1.1) * 0.06 * s],
          ],
          Math.max(1, s * 0.018),
        );
      }
      // little mouth
      ctx.fillStyle = '#0b0606';
      ctx.beginPath();
      ctx.ellipse(0, 0.09 * s, 0.06 * s, 0.04 * s, 0, 0, TAU);
      ctx.fill();
      teethRing(ctx, 0, 0.09 * s, 0.062 * s, 0.042 * s, 6, 0.022 * s, '#e2d3b0', Math.max(1, s * 0.014));
      eyes(ctx, 0, -0.08 * s, 0.07 * s, s * 0.02, '#ffb066');
      break;
    }
    case 'chorister':
    case 'shielder': {
      const hov = Math.sin(time * 2) * s * 0.02;
      ctx.translate(0, hov);
      shadowFoot(ctx, t, 0.2);
      // robe
      const g = ctx.createLinearGradient(0, -0.4 * s, 0, 0.34 * s);
      g.addColorStop(0, '#4a4a6e');
      g.addColorStop(0.6, '#242438');
      g.addColorStop(1, '#101018');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(0, -0.42 * s);
      ctx.quadraticCurveTo(0.26 * s, -0.2 * s, 0.3 * s, 0.32 * s);
      ctx.lineTo(0.12 * s, 0.26 * s);
      ctx.lineTo(0, 0.32 * s);
      ctx.lineTo(-0.12 * s, 0.26 * s);
      ctx.lineTo(-0.3 * s, 0.32 * s);
      ctx.quadraticCurveTo(-0.26 * s, -0.2 * s, 0, -0.42 * s);
      ctx.fill();
      ctx.strokeStyle = 'rgba(150,180,230,0.28)';
      ctx.lineWidth = Math.max(1, s * 0.018);
      ctx.stroke();
      // aegis halo
      const hp = 0.5 + 0.5 * Math.sin(time * 3);
      ctx.strokeStyle = `rgba(150,215,255,${0.28 + hp * 0.3})`;
      ctx.lineWidth = Math.max(1.2, s * 0.018);
      ctx.beginPath();
      ctx.ellipse(0, -0.06 * s, 0.36 * s, 0.085 * s, 0, 0.12, Math.PI - 0.12);
      ctx.stroke();
      // censer on a chain
      const swing = Math.sin(time * 1.7) * 0.06;
      ctx.save();
      ctx.translate(0.2 * s, 0.1 * s);
      ctx.rotate(swing);
      ctx.strokeStyle = 'rgba(180,175,160,0.5)';
      ctx.lineWidth = Math.max(1, s * 0.012);
      ctx.beginPath();
      ctx.moveTo(0, -0.12 * s);
      ctx.lineTo(0, 0.02 * s);
      ctx.stroke();
      glowSprite(ctx, 0.14 * s, 'rgba(255,180,90,0.5)', 0.7);
      ctx.fillStyle = '#2b2b33';
      ctx.beginPath();
      ctx.roundRect(-0.05 * s, 0.02 * s, 0.1 * s, 0.09 * s, s * 0.02);
      ctx.fill();
      ctx.fillStyle = '#ffc46b';
      ctx.beginPath();
      ctx.arc(0, 0.065 * s, s * 0.02, 0, TAU);
      ctx.fill();
      ctx.restore();
      // bone mask
      ctx.fillStyle = '#d8d2c4';
      ctx.beginPath();
      ctx.ellipse(0, -0.21 * s, 0.1 * s, 0.13 * s, 0, 0, TAU);
      ctx.fill();
      ctx.fillStyle = '#08070c';
      for (const dir of [-1, 1]) {
        ctx.beginPath();
        ctx.ellipse(dir * 0.042 * s, -0.23 * s, 0.019 * s, 0.05 * s, 0, 0, TAU);
        ctx.fill();
      }
      ctx.fillStyle = '#a8d8ff';
      for (const dir of [-1, 1]) {
        ctx.beginPath();
        ctx.ellipse(dir * 0.042 * s, -0.24 * s, 0.008 * s, 0.02 * s, 0, 0, TAU);
        ctx.fill();
      }
      break;
    }
    case 'burrower': {
      shadowFoot(ctx, t, 0.24);
      // hole
      ctx.fillStyle = '#0a0705';
      ctx.beginPath();
      ctx.ellipse(0, 0.26 * s, 0.2 * s, 0.06 * s, 0, 0, TAU);
      ctx.fill();
      // segmented body
      for (let i = 4; i >= 0; i--) {
        const y = 0.24 * s - i * 0.1 * s;
        const rr = (0.15 - i * 0.014) * s;
        const g = ctx.createRadialGradient(-rr * 0.4, y - rr * 0.3, rr * 0.1, 0, y, rr * 1.2);
        g.addColorStop(0, '#6d4c34');
        g.addColorStop(1, '#22150f');
        ctx.fillStyle = g;
        ctx.beginPath();
        ctx.ellipse(0, y, rr, rr * 0.72, 0, 0, TAU);
        ctx.fill();
        ctx.strokeStyle = 'rgba(12,7,5,0.55)';
        ctx.lineWidth = Math.max(1, s * 0.014);
        ctx.stroke();
      }
      // mandibles
      ctx.strokeStyle = '#cfb992';
      ctx.lineWidth = s * 0.03;
      ctx.beginPath();
      ctx.moveTo(-0.08 * s, -0.18 * s);
      ctx.quadraticCurveTo(-0.2 * s, -0.26 * s, -0.1 * s, -0.36 * s);
      ctx.moveTo(0.08 * s, -0.18 * s);
      ctx.quadraticCurveTo(0.2 * s, -0.26 * s, 0.1 * s, -0.36 * s);
      ctx.stroke();
      eyes(ctx, 0, -0.14 * s, 0.06 * s, s * 0.019, '#ff8b5a');
      break;
    }
    case 'eater': {
      shadowFoot(ctx, t, 0.28);
      // legs
      ctx.strokeStyle = '#2a1a13';
      ctx.lineWidth = s * 0.07;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(-0.12 * s, 0.16 * s);
      ctx.lineTo(-0.2 * s, 0.3 * s);
      ctx.moveTo(0.12 * s, 0.16 * s);
      ctx.lineTo(0.2 * s, 0.3 * s);
      ctx.stroke();
      // body
      const g = ctx.createLinearGradient(0, -0.36 * s, 0, 0.24 * s);
      g.addColorStop(0, '#8a5c42');
      g.addColorStop(0.5, '#402a20');
      g.addColorStop(1, '#180f0c');
      ctx.fillStyle = g;
      blob(ctx, 0.3 * s, 0.29 * s, 7, 2.7);
      ctx.fill();
      ctx.strokeStyle = 'rgba(12,7,5,0.6)';
      ctx.lineWidth = Math.max(1, s * 0.02);
      ctx.stroke();
      // maw
      glowSprite(ctx, 0.2 * s, 'rgba(255,110,40,0.5)', 0.7);
      ctx.fillStyle = '#0b0404';
      ctx.beginPath();
      ctx.ellipse(0, 0.02 * s, 0.16 * s, 0.19 * s, 0, 0, TAU);
      ctx.fill();
      teethRing(ctx, 0, 0.02 * s, 0.16 * s, 0.19 * s, 9, 0.055 * s, '#e6d6b0', Math.max(1.2, s * 0.026));
      // eye cluster
      eyes(ctx, 0, -0.24 * s, 0.1 * s, s * 0.02, '#ffb066');
      break;
    }
    case 'sentinel': {
      shadowFoot(ctx, t, 0.26);
      // legs
      ctx.fillStyle = '#2b2b26';
      ctx.beginPath();
      ctx.roundRect(-0.16 * s, 0.16 * s, 0.12 * s, 0.16 * s, s * 0.03);
      ctx.roundRect(0.04 * s, 0.16 * s, 0.12 * s, 0.16 * s, s * 0.03);
      ctx.fill();
      // body armour
      const g = ctx.createLinearGradient(-0.2 * s, -0.34 * s, 0.2 * s, 0.2 * s);
      g.addColorStop(0, '#8d8875');
      g.addColorStop(0.5, '#4b4a41');
      g.addColorStop(1, '#1e1e1a');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(-0.2 * s, 0.2 * s);
      ctx.lineTo(-0.22 * s, -0.14 * s);
      ctx.lineTo(-0.1 * s, -0.3 * s);
      ctx.lineTo(0.1 * s, -0.3 * s);
      ctx.lineTo(0.22 * s, -0.14 * s);
      ctx.lineTo(0.2 * s, 0.2 * s);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = 'rgba(10,10,8,0.7)';
      ctx.lineWidth = Math.max(1, s * 0.018);
      ctx.stroke();
      // pauldron
      ctx.fillStyle = '#6e6a59';
      ctx.beginPath();
      ctx.ellipse(-0.2 * s, -0.13 * s, 0.1 * s, 0.07 * s, -0.3, 0, TAU);
      ctx.fill();
      // helm
      ctx.fillStyle = '#3a3a33';
      ctx.beginPath();
      ctx.roundRect(-0.11 * s, -0.44 * s, 0.22 * s, 0.2 * s, s * 0.05);
      ctx.fill();
      ctx.strokeStyle = 'rgba(255,180,110,0.3)';
      ctx.stroke();
      ctx.fillStyle = '#08070a';
      ctx.beginPath();
      ctx.roundRect(-0.09 * s, -0.34 * s, 0.18 * s, 0.05 * s, s * 0.02);
      ctx.fill();
      eyes(ctx, 0, -0.315 * s, 0.05 * s, s * 0.014, '#ffb066');
      // crest
      ctx.fillStyle = '#a8452e';
      ctx.beginPath();
      ctx.moveTo(0, -0.44 * s);
      ctx.quadraticCurveTo(0.05 * s, -0.56 * s, -0.04 * s, -0.6 * s);
      ctx.quadraticCurveTo(0.02 * s, -0.52 * s, -0.06 * s, -0.44 * s);
      ctx.fill();
      // sword
      ctx.save();
      ctx.translate(0.24 * s, 0.04 * s);
      ctx.rotate(0.22);
      ctx.fillStyle = '#c2ccdb';
      ctx.beginPath();
      ctx.moveTo(-0.03 * s, 0.22 * s);
      ctx.lineTo(-0.045 * s, -0.34 * s);
      ctx.lineTo(0, -0.42 * s);
      ctx.lineTo(0.045 * s, -0.34 * s);
      ctx.lineTo(0.03 * s, 0.22 * s);
      ctx.closePath();
      ctx.fill();
      ctx.fillStyle = '#8a7048';
      ctx.beginPath();
      ctx.roundRect(-0.09 * s, 0.2 * s, 0.18 * s, 0.04 * s, s * 0.015);
      ctx.fill();
      ctx.restore();
      break;
    }
    case 'witch': {
      const float = Math.sin(time * 2.2) * s * 0.03;
      ctx.translate(0, -0.04 * s + float);
      glowSprite(ctx, 0.36 * s, 'rgba(190,120,255,0.2)', 0.7);
      // robe
      const g = ctx.createLinearGradient(0, -0.4 * s, 0, 0.32 * s);
      g.addColorStop(0, '#4a3050');
      g.addColorStop(0.6, '#291a30');
      g.addColorStop(1, '#120a18');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(0, -0.44 * s);
      ctx.quadraticCurveTo(0.28 * s, -0.16 * s, 0.26 * s, 0.34 * s);
      ctx.quadraticCurveTo(0, 0.4 * s, -0.26 * s, 0.34 * s);
      ctx.quadraticCurveTo(-0.28 * s, -0.16 * s, 0, -0.44 * s);
      ctx.fill();
      ctx.strokeStyle = 'rgba(255,150,90,0.28)';
      ctx.lineWidth = Math.max(1, s * 0.018);
      ctx.stroke();
      // hood shadow + face
      ctx.fillStyle = '#07050c';
      ctx.beginPath();
      ctx.ellipse(0, -0.2 * s, 0.12 * s, 0.15 * s, 0, 0, TAU);
      ctx.fill();
      eyes(ctx, 0, -0.22 * s, 0.045 * s, s * 0.017, '#ffd479');
      // orb in hand
      glowSprite(ctx, 0.22 * s, 'rgba(255,150,60,0.55)', 0.85);
      ctx.fillStyle = '#ffc46b';
      ctx.beginPath();
      ctx.arc(0.24 * s, 0.06 * s, s * 0.035, 0, TAU);
      ctx.fill();
      // staff
      ctx.strokeStyle = '#3a2a22';
      ctx.lineWidth = s * 0.028;
      ctx.beginPath();
      ctx.moveTo(-0.22 * s, 0.32 * s);
      ctx.lineTo(-0.3 * s, -0.3 * s);
      ctx.stroke();
      break;
    }
    case 'maw': {
      ctx.scale(1.22, 1.22);
      shadowFoot(ctx, t, 0.34);
      // fleshy mound
      const g = ctx.createRadialGradient(-0.1 * s, -0.16 * s, 0.05 * s, 0, 0, 0.4 * s);
      g.addColorStop(0, '#6b3a29');
      g.addColorStop(0.55, '#37201a');
      g.addColorStop(1, '#150c0a');
      ctx.fillStyle = g;
      blob(ctx, 0.35 * s, 0.33 * s, 9, 5.2, Math.sin(time * 3) * 0.02);
      ctx.fill();
      // tentacle fringe
      ctx.strokeStyle = '#3d231b';
      ctx.lineWidth = s * 0.04;
      ctx.lineCap = 'round';
      for (let i = 0; i < 7; i++) {
        const a = (i / 7) * TAU + time * 0.2;
        ctx.beginPath();
        ctx.moveTo(Math.cos(a) * 0.3 * s, Math.sin(a) * 0.28 * s);
        ctx.quadraticCurveTo(Math.cos(a + 0.3) * 0.4 * s, Math.sin(a + 0.3) * 0.38 * s, Math.cos(a + 0.15) * 0.46 * s, Math.sin(a + 0.15) * 0.44 * s);
        ctx.stroke();
      }
      // the pit
      glowSprite(ctx, 0.3 * s, 'rgba(255,110,40,0.5)', 0.8);
      const gap = 0.02 + Math.abs(Math.sin(time * 2.2)) * 0.03;
      ctx.fillStyle = '#080303';
      ctx.beginPath();
      ctx.ellipse(0, 0.04 * s, 0.21 * s, 0.15 * s + gap * s, 0, 0, TAU);
      ctx.fill();
      teethRing(ctx, 0, 0.04 * s, 0.21 * s, 0.15 * s + gap * s, 12, 0.06 * s, '#e8dcc0', Math.max(1.2, s * 0.024));
      teethRing(ctx, 0, 0.04 * s, 0.13 * s, 0.09 * s + gap * s * 0.6, 9, 0.04 * s, '#cbbfa4', Math.max(1, s * 0.018));
      // eye cluster above
      for (const [ex, ey, rr] of [
        [-0.16, -0.2, 0.022],
        [-0.05, -0.25, 0.026],
        [0.07, -0.25, 0.026],
        [0.18, -0.19, 0.022],
        [0.01, -0.35, 0.018],
      ] as [number, number, number][]) {
        eyes(ctx, ex * s, ey * s, 0.001, s * rr * 0.6, '#ffd479');
      }
      break;
    }
    case 'regent': {
      ctx.scale(1.12, 1.12);
      shadowFoot(ctx, t, 0.34);
      // throne silhouette behind
      ctx.fillStyle = 'rgba(30,20,16,0.9)';
      ctx.beginPath();
      ctx.moveTo(-0.36 * s, 0.34 * s);
      ctx.lineTo(-0.3 * s, -0.3 * s);
      ctx.lineTo(-0.16 * s, -0.4 * s);
      ctx.lineTo(0.16 * s, -0.4 * s);
      ctx.lineTo(0.3 * s, -0.3 * s);
      ctx.lineTo(0.36 * s, 0.34 * s);
      ctx.closePath();
      ctx.fill();
      // body
      const g = ctx.createLinearGradient(0, -0.38 * s, 0, 0.3 * s);
      g.addColorStop(0, '#a07a4e');
      g.addColorStop(0.5, '#54382c');
      g.addColorStop(1, '#1e1512');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(-0.24 * s, 0.3 * s);
      ctx.lineTo(-0.2 * s, -0.2 * s);
      ctx.quadraticCurveTo(0, -0.34 * s, 0.2 * s, -0.2 * s);
      ctx.lineTo(0.24 * s, 0.3 * s);
      ctx.closePath();
      ctx.fill();
      // crown
      ctx.fillStyle = '#d9ae63';
      ctx.beginPath();
      ctx.moveTo(-0.15 * s, -0.3 * s);
      ctx.lineTo(-0.15 * s, -0.44 * s);
      ctx.lineTo(-0.05 * s, -0.36 * s);
      ctx.lineTo(0, -0.48 * s);
      ctx.lineTo(0.05 * s, -0.36 * s);
      ctx.lineTo(0.15 * s, -0.44 * s);
      ctx.lineTo(0.15 * s, -0.3 * s);
      ctx.closePath();
      ctx.fill();
      // face
      ctx.fillStyle = '#0c0806';
      ctx.beginPath();
      ctx.ellipse(0, -0.22 * s, 0.1 * s, 0.09 * s, 0, 0, TAU);
      ctx.fill();
      eyes(ctx, 0, -0.22 * s, 0.048 * s, s * 0.02, '#ffd479');
      // arms on armrests
      ctx.fillStyle = '#4a352a';
      for (const dir of [-1, 1]) {
        ctx.beginPath();
        ctx.ellipse(dir * 0.26 * s, 0.02 * s, 0.06 * s, 0.1 * s, 0, 0, TAU);
        ctx.fill();
      }
      glowSprite(ctx, 0.34 * s, 'rgba(255,150,60,0.22)', 0.8);
      break;
    }
    case 'unmaker': {
      ctx.scale(1.16, 1.16);
      const float = Math.sin(time * 1.8) * s * 0.03;
      ctx.translate(0, float);
      glowSprite(ctx, 0.44 * s, 'rgba(190,90,220,0.28)', 0.8);
      // void shroud
      const g = ctx.createLinearGradient(0, -0.5 * s, 0, 0.4 * s);
      g.addColorStop(0, '#33204a');
      g.addColorStop(0.5, '#1a1126');
      g.addColorStop(1, '#0a0610');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.moveTo(0, -0.52 * s);
      ctx.quadraticCurveTo(0.34 * s, -0.2 * s, 0.24 * s, 0.36 * s);
      ctx.quadraticCurveTo(0.12 * s, 0.44 * s, 0, 0.4 * s);
      ctx.quadraticCurveTo(-0.12 * s, 0.44 * s, -0.24 * s, 0.36 * s);
      ctx.quadraticCurveTo(-0.34 * s, -0.2 * s, 0, -0.52 * s);
      ctx.fill();
      // rift
      const rg = ctx.createLinearGradient(0, -0.36 * s, 0, 0.28 * s);
      rg.addColorStop(0, 'rgba(255,225,190,0.95)');
      rg.addColorStop(0.5, 'rgba(255,120,170,0.9)');
      rg.addColorStop(1, 'rgba(90,20,120,0.3)');
      ctx.strokeStyle = rg;
      ctx.lineWidth = s * 0.055;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(0.01 * s, -0.36 * s);
      ctx.lineTo(-0.04 * s, -0.06 * s);
      ctx.lineTo(0.05 * s, 0.24 * s);
      ctx.stroke();
      glowSprite(ctx, 0.26 * s, 'rgba(255,140,200,0.5)', 0.6);
      // tendrils
      ctx.strokeStyle = 'rgba(140,90,180,0.6)';
      ctx.lineWidth = Math.max(1, s * 0.022);
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * TAU + time * 0.3;
        ctx.beginPath();
        ctx.moveTo(Math.cos(a) * 0.16 * s, -0.05 * s + Math.sin(a) * 0.18 * s);
        ctx.quadraticCurveTo(Math.cos(a) * 0.34 * s, -0.05 * s + Math.sin(a) * 0.36 * s, Math.cos(a + 0.6) * 0.44 * s, -0.05 * s + Math.sin(a + 0.6) * 0.46 * s);
        ctx.stroke();
      }
      // porcelain mask with cracks
      ctx.fillStyle = '#efe6d2';
      ctx.beginPath();
      ctx.moveTo(0, -0.46 * s);
      ctx.quadraticCurveTo(0.14 * s, -0.38 * s, 0.11 * s, -0.24 * s);
      ctx.quadraticCurveTo(0.06 * s, -0.14 * s, 0, -0.13 * s);
      ctx.quadraticCurveTo(-0.06 * s, -0.14 * s, -0.11 * s, -0.24 * s);
      ctx.quadraticCurveTo(-0.14 * s, -0.38 * s, 0, -0.46 * s);
      ctx.fill();
      ctx.fillStyle = '#0a0610';
      for (const dir of [-1, 1]) {
        ctx.beginPath();
        ctx.ellipse(dir * 0.048 * s, -0.3 * s, 0.024 * s, 0.045 * s, dir * 0.2, 0, TAU);
        ctx.fill();
      }
      glowSprite(ctx, 0.09 * s, 'rgba(255,140,190,0.5)', 0.6);
      ctx.strokeStyle = 'rgba(60,30,60,0.55)';
      ctx.lineWidth = Math.max(1, s * 0.01);
      ctx.beginPath();
      ctx.moveTo(-0.02 * s, -0.46 * s);
      ctx.lineTo(0.01 * s, -0.38 * s);
      ctx.lineTo(-0.03 * s, -0.33 * s);
      ctx.moveTo(0.06 * s, -0.2 * s);
      ctx.lineTo(0.02 * s, -0.15 * s);
      ctx.stroke();
      break;
    }
    default: {
      shadowFoot(ctx, t, 0.22);
      ctx.fillStyle = '#4a3324';
      blob(ctx, 0.24 * s, 0.22 * s, 6, 1);
      ctx.fill();
      eyes(ctx, 0, -0.03 * s, 0.07 * s, s * 0.02);
    }
  }
  innerEmber(ctx, s);
  rimLight(ctx, s);
  if (flash > 0) {
    ctx.globalCompositeOperation = 'lighter';
    ctx.globalAlpha = flash * 0.5;
    ctx.fillStyle = '#ffd9a0';
    blob(ctx, 0.33 * s, 0.3 * s, 6, 1);
    ctx.fill();
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }
  if (dying) {
    ctx.globalCompositeOperation = 'lighter';
    ctx.globalAlpha = 0.45;
    ctx.fillStyle = C.ember;
    ctx.beginPath();
    ctx.arc(0, 0, s * 0.18, 0, TAU);
    ctx.fill();
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }
  ctx.restore();
}

// ── heroes ───────────────────────────────────────────────────────────────────
interface KeeperPalette {
  cloak0: string;
  cloak1: string;
  cloak2: string;
  trim: string;
  glow: string;
}

const KEEPERS: Record<string, KeeperPalette> = {
  keeper: { cloak0: '#4b5470', cloak1: '#232838', cloak2: '#0d1018', trim: 'rgba(255,190,120,0.5)', glow: 'rgba(255,180,90,0.42)' },
  warden: { cloak0: '#5b6478', cloak1: '#2b3140', cloak2: '#101318', trim: 'rgba(255,200,140,0.55)', glow: 'rgba(255,170,80,0.35)' },
  wisp: { cloak0: '#5c3346', cloak1: '#2a1626', cloak2: '#0f0810', trim: 'rgba(255,150,110,0.55)', glow: 'rgba(255,140,80,0.5)' },
};

function drawHero(ctx: CanvasRenderingContext2D, heroId: string, t: number, time: number, flash: number): void {
  const s = t * 1.0;
  const p = KEEPERS[heroId] ?? KEEPERS.keeper;
  const wisp = heroId === 'wisp';
  const warden = heroId === 'warden';
  const bob = Math.sin(time * 2.2) * s * 0.012;
  const sway = Math.sin(time * 1.6) * 0.012;

  ctx.save();
  ctx.translate(0, bob - (wisp ? s * 0.05 : 0));
  shadowFoot(ctx, t, wisp ? 0.16 : 0.2);

  // lantern glow behind the body
  const lanternX = warden ? -0.32 * s : 0.3 * s;
  ctx.save();
  ctx.translate(lanternX, 0.06 * s);
  glowSprite(ctx, 0.46 * s, p.glow, 0.75);
  ctx.restore();

  // boots
  if (!wisp) {
    ctx.fillStyle = '#191d27';
    ctx.beginPath();
    ctx.roundRect(-0.15 * s, 0.2 * s, 0.12 * s, 0.13 * s, s * 0.03);
    ctx.roundRect(0.03 * s, 0.2 * s, 0.12 * s, 0.13 * s, s * 0.03);
    ctx.fill();
  }

  // cloak with tattered hem
  const g = ctx.createLinearGradient(-0.1 * s, -0.42 * s, 0.1 * s, 0.3 * s);
  g.addColorStop(0, p.cloak0);
  g.addColorStop(0.55, p.cloak1);
  g.addColorStop(1, p.cloak2);
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.moveTo(0, -0.44 * s);
  ctx.quadraticCurveTo(0.16 * s, -0.34 * s, 0.2 * s, -0.14 * s);
  ctx.quadraticCurveTo(0.28 * s + sway * s, 0.12 * s, 0.26 * s, 0.28 * s);
  ctx.lineTo(0.14 * s, 0.2 * s);
  ctx.lineTo(0.05 * s, 0.3 * s);
  ctx.lineTo(-0.06 * s, 0.21 * s);
  ctx.lineTo(-0.16 * s, 0.3 * s);
  ctx.quadraticCurveTo(-0.26 * s, 0.12 * s, -0.2 * s, -0.14 * s);
  ctx.quadraticCurveTo(-0.16 * s, -0.34 * s, 0, -0.44 * s);
  ctx.fill();
  ctx.strokeStyle = 'rgba(10,12,18,0.55)';
  ctx.lineWidth = Math.max(1, s * 0.016);
  ctx.stroke();

  // mantle across the shoulders
  ctx.fillStyle = warden ? '#3d4557' : wisp ? '#3b2030' : '#2f3648';
  ctx.beginPath();
  ctx.moveTo(-0.21 * s, -0.16 * s);
  ctx.quadraticCurveTo(0, -0.05 * s, 0.21 * s, -0.17 * s);
  ctx.quadraticCurveTo(0, -0.24 * s, -0.21 * s, -0.16 * s);
  ctx.fill();
  ctx.strokeStyle = p.trim;
  ctx.lineWidth = Math.max(1, s * 0.016);
  ctx.stroke();

  // belt + ember clasp
  ctx.strokeStyle = 'rgba(140,105,70,0.65)';
  ctx.lineWidth = s * 0.035;
  ctx.beginPath();
  ctx.moveTo(-0.14 * s, 0.06 * s);
  ctx.lineTo(0.14 * s, 0.05 * s);
  ctx.stroke();
  ctx.fillStyle = '#ffb96b';
  ctx.beginPath();
  ctx.arc(0, 0.055 * s, s * 0.024, 0, TAU);
  ctx.fill();

  // hood with a peak
  ctx.fillStyle = wisp ? '#1b1020' : warden ? '#1a1f2a' : '#141821';
  ctx.beginPath();
  ctx.moveTo(0, -0.58 * s);
  ctx.quadraticCurveTo(0.19 * s, -0.44 * s, 0.2 * s, -0.2 * s);
  ctx.quadraticCurveTo(0.1 * s, -0.1 * s, 0, -0.1 * s);
  ctx.quadraticCurveTo(-0.1 * s, -0.1 * s, -0.2 * s, -0.2 * s);
  ctx.quadraticCurveTo(-0.19 * s, -0.44 * s, 0, -0.58 * s);
  ctx.fill();
  ctx.strokeStyle = p.trim;
  ctx.lineWidth = Math.max(1, s * 0.018);
  ctx.beginPath();
  ctx.moveTo(-0.19 * s, -0.24 * s);
  ctx.quadraticCurveTo(-0.17 * s, -0.45 * s, 0, -0.57 * s);
  ctx.stroke();

  // face void
  ctx.fillStyle = '#05060a';
  ctx.beginPath();
  ctx.ellipse(0, -0.26 * s, 0.12 * s, 0.14 * s, 0, 0, TAU);
  ctx.fill();
  eyes(ctx, 0, -0.26 * s, 0.048 * s, s * 0.019, wisp ? '#ffb066' : '#ffd479');

  // lantern
  ctx.save();
  ctx.translate(lanternX, 0.06 * s);
  ctx.strokeStyle = 'rgba(120,130,150,0.6)';
  ctx.lineWidth = Math.max(1, s * 0.014);
  ctx.beginPath();
  ctx.arc(0, -0.075 * s, s * 0.03, Math.PI * 0.9, Math.PI * 2.1);
  ctx.stroke();
  ctx.fillStyle = '#1a1f28';
  ctx.beginPath();
  ctx.roundRect(-0.062 * s, -0.06 * s, 0.124 * s, 0.14 * s, s * 0.022);
  ctx.fill();
  ctx.strokeStyle = 'rgba(255,200,130,0.6)';
  ctx.lineWidth = Math.max(1, s * 0.012);
  ctx.stroke();
  const flick = 0.85 + 0.15 * Math.sin(time * 9 + 1.3);
  ctx.fillStyle = `rgba(255,205,130,${0.9 * flick})`;
  ctx.beginPath();
  ctx.ellipse(0, 0.012 * s, 0.03 * s, 0.038 * s, 0, 0, TAU);
  ctx.fill();
  ctx.fillStyle = 'rgba(255,245,210,0.95)';
  ctx.beginPath();
  ctx.ellipse(0, 0.02 * s, 0.014 * s, 0.02 * s, 0, 0, TAU);
  ctx.fill();
  ctx.restore();

  // arms
  ctx.strokeStyle = wisp ? '#3a2430' : '#232936';
  ctx.lineWidth = s * 0.055;
  ctx.lineCap = 'round';
  ctx.beginPath();
  ctx.moveTo(-0.17 * s, -0.1 * s);
  ctx.lineTo(-0.25 * s, 0.02 * s);
  ctx.moveTo(0.17 * s, -0.1 * s);
  ctx.lineTo(0.26 * s, 0.0 * s);
  ctx.stroke();

  // warden shield and hammer
  if (warden) {
    ctx.save();
    ctx.translate(-0.31 * s, 0.06 * s);
    ctx.rotate(-0.1);
    const sg = ctx.createLinearGradient(0, -0.2 * s, 0, 0.22 * s);
    sg.addColorStop(0, '#59627a');
    sg.addColorStop(1, '#2a3140');
    ctx.fillStyle = sg;
    ctx.beginPath();
    ctx.moveTo(0, -0.22 * s);
    ctx.lineTo(0.15 * s, -0.14 * s);
    ctx.lineTo(0.13 * s, 0.12 * s);
    ctx.lineTo(0, 0.24 * s);
    ctx.lineTo(-0.13 * s, 0.12 * s);
    ctx.lineTo(-0.15 * s, -0.14 * s);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = 'rgba(230,200,150,0.5)';
    ctx.lineWidth = Math.max(1, s * 0.018);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(255,190,120,0.45)';
    ctx.beginPath();
    ctx.moveTo(0, -0.12 * s);
    ctx.lineTo(0, 0.14 * s);
    ctx.moveTo(-0.08 * s, 0.0 * s);
    ctx.lineTo(0.08 * s, 0.0 * s);
    ctx.stroke();
    ctx.restore();
    // hammer at the right hand
    ctx.save();
    ctx.translate(0.27 * s, 0.02 * s);
    ctx.rotate(-0.25);
    ctx.strokeStyle = '#4a3a2a';
    ctx.lineWidth = s * 0.035;
    ctx.beginPath();
    ctx.moveTo(0, -0.06 * s);
    ctx.lineTo(0, 0.16 * s);
    ctx.stroke();
    ctx.fillStyle = '#5a6274';
    ctx.beginPath();
    ctx.roundRect(-0.07 * s, -0.15 * s, 0.14 * s, 0.1 * s, s * 0.02);
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,180,110,0.4)';
    ctx.lineWidth = Math.max(1, s * 0.014);
    ctx.stroke();
    ctx.restore();
  }

  // wisp flame trails
  if (wisp) {
    ctx.globalCompositeOperation = 'lighter';
    for (let i = 0; i < 4; i++) {
      const a = time * 2 + i;
      ctx.globalAlpha = 0.1 + 0.09 * Math.sin(a);
      ctx.fillStyle = '#ff8b3d';
      ctx.beginPath();
      ctx.ellipse(Math.sin(a) * s * 0.1, s * 0.26 + i * s * 0.03, s * 0.05, s * 0.1, 0, 0, TAU);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }

  innerEmber(ctx, s, '255,190,110');
  rimLight(ctx, s, '255,200,140');

  if (flash > 0) {
    ctx.globalCompositeOperation = 'lighter';
    ctx.globalAlpha = flash * 0.55;
    ctx.fillStyle = '#ffd0a0';
    ctx.beginPath();
    ctx.ellipse(0, -0.1 * s, 0.28 * s, 0.36 * s, 0, 0, TAU);
    ctx.fill();
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  }
  ctx.restore();
}

export function drawPortrait(
  ctx: CanvasRenderingContext2D,
  kind: 'enemy' | 'hero',
  id: string,
  x: number,
  y: number,
  size: number,
  time = 0.6,
): void {
  const g = ctx.createRadialGradient(x + size / 2, y + size * 0.55, 0, x + size / 2, y + size * 0.55, size * 0.62);
  g.addColorStop(0, kind === 'hero' ? 'rgba(255,140,60,0.26)' : 'rgba(140,70,40,0.3)');
  g.addColorStop(1, 'rgba(10,12,18,0)');
  ctx.fillStyle = g;
  ctx.fillRect(x, y, size, size);
  ctx.save();
  ctx.translate(x + size / 2, y + size * 0.58);
  if (kind === 'hero') drawHero(ctx, id, size * 0.95, time, 0);
  else drawEnemyBody(ctx, id, size * 0.95, time, false, 0);
  ctx.restore();
}

function portrait(kind: 'enemy' | 'hero', id: string, size: number): string {
  const canvas = document.createElement('canvas');
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = size * dpr;
  canvas.height = size * dpr;
  const ctx = canvas.getContext('2d');
  if (!ctx) return '';
  ctx.scale(dpr, dpr);
  drawPortrait(ctx as unknown as CanvasRenderingContext2D, kind, id, 0, 0, size);
  return canvas.toDataURL();
}

export function enemyPortrait(typeId: string, size: number): string {
  return portrait('enemy', typeId, size);
}

export function heroPortrait(heroId: string, size: number): string {
  return portrait('hero', heroId, size);
}

export { enemyDef, heroDef };
