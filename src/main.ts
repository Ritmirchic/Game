// ── ANVILIGHT · application controller ───────────────────────────────────────
import './styles.css';
import { Renderer, enemyPortrait, heroPortrait } from './render';
import { icon } from './icons';
import { audio } from './audio';
import {
  ACHIEVEMENTS,
  ABILITIES,
  ENEMIES,
  HEROES,
  META_UNLOCKS,
  RELICS,
  abilityDef,
  abilityStats,
  enemyDef,
  heroDef,
  relicDef,
} from './core/content';
import type { RoomType, Rarity } from './core/content';
import type { Battle, BattleEvent, Vec } from './core/types';
import { eq, vkey } from './core/types';
import {
  abilityCost,
  abilityTargets,
  castAbility,
  endTurn as resolveTurn,
  enemyAt,
  heroReach,
  intentText,
  moveHero,
} from './core/battle';
import {
  MAX_ABILITIES,
  actName,
  buyItem,
  continueRoute,
  enterRoom,
  finishBattle,
  isBossRoom,
  makeEncounter,
  newRun,
  persistRng,
  rollShop,
  runRng,
  startBattle,
  type EncounterInfo,
  type RewardOffer,
  type RoomOffer,
  type RunState,
  type ShopItem,
} from './core/run';
import {
  checkAchievements,
  clearSave,
  loadSave,
  writeSave,
  type SaveData,
} from './core/save';
import { shortSeed } from './core/rng';

type Screen = 'title' | 'heroes' | 'route' | 'battle' | 'reward' | 'shop' | 'rest' | 'treasure' | 'result';

const esc = (s: string): string =>
  s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

const rarityLabel: Record<Rarity, string> = {
  starter: 'starter',
  common: 'common',
  rare: 'rare',
  epic: 'mythic',
};

class Game {
  private renderer: Renderer;
  private ui: HTMLElement;
  private toasts: HTMLElement;
  save: SaveData;
  run: RunState | null = null;
  battle: Battle | null = null;
  encounter: EncounterInfo | null = null;
  screen: Screen = 'title';

  private selected: number | null = null;
  private abilityTiles = new Set<string>();
  private moveTiles = new Map<string, number>();
  private hover: Vec | null = null;
  private busy = false;
  private audioStarted = false;
  private maxCasts = 0;
  private maxKillsTurn = 0;
  private lastOutcome: { won: boolean; embers: number; dust: number; reason: string; kills: number; shoveKills: number } | null = null;
  private modal: string | null = null;
  private modalReturn: 'pause' | null = null;
  private codexTab = 'heroes';
  private resetArmed = false;

  constructor() {
    this.save = loadSave();
    const app = document.getElementById('app')!;
    const canvas = document.getElementById('stage') as HTMLCanvasElement;
    this.ui = document.getElementById('ui') as HTMLElement;
    this.toasts = document.createElement('div');
    this.toasts.className = 'toasts';
    app.appendChild(this.toasts);
    this.renderer = new Renderer(canvas);
    this.applySettings();
    this.run = this.save.run;

    // Delegated from the app root so dialogs (which live outside #ui) stay live.
    app.addEventListener('click', (e) => this.onClick(e));
    canvas.addEventListener('pointermove', (e) => this.onPointerMove(e));
    canvas.addEventListener('pointerdown', (e) => this.onPointerDown(e));
    canvas.addEventListener('pointerleave', () => {
      this.hover = null;
      this.renderer.hover = null;
      this.renderer.path = [];
    });
    window.addEventListener('resize', () => this.onResize());
    window.addEventListener('keydown', (e) => this.onKey(e));
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) this.persist();
    });
    window.addEventListener('beforeunload', () => this.persist());

    requestAnimationFrame((t) => this.loop(t));
    this.showTitle();
    this.persist();
  }

  private loop(t: number): void {
    this.renderer.frame(t);
    requestAnimationFrame((x) => this.loop(x));
  }

  // ── settings & audio ─────────────────────────────────────────────────────
  private applySettings(): void {
    document.body.classList.toggle('reduce-motion', this.save.settings.reduceMotion);
    audio.setVolumes(this.save.settings.sfx, this.save.settings.music, this.save.settings.muted);
  }

  private unlockAudio(): void {
    audio.init();
    audio.setVolumes(this.save.settings.sfx, this.save.settings.music, this.save.settings.muted);
    if (!this.audioStarted) {
      audio.startAmbience();
      this.audioStarted = true;
    }
  }

  private persist(): void {
    this.save.run = this.run;
    this.save.lastHero = this.run?.heroId ?? this.save.lastHero;
    writeSave(this.save);
  }

  // ── utilities ────────────────────────────────────────────────────────────
  private toast(msg: string, iconId = 'flame'): void {
    const el = document.createElement('div');
    el.className = 'toast';
    el.innerHTML = `${icon(iconId)}<span>${esc(msg)}</span>`;
    this.toasts.appendChild(el);
    setTimeout(() => {
      el.style.transition = 'opacity .3s ease, transform .3s ease';
      el.style.opacity = '0';
      el.style.transform = 'translateY(8px)';
      setTimeout(() => el.remove(), 320);
    }, 2600);
  }

  private showAchievement(id: string): void {
    const def = ACHIEVEMENTS.find((a) => a.id === id);
    if (!def) return;
    const el = document.createElement('div');
    el.className = 'achievement-pop';
    el.innerHTML = `<div class="card-icon r-starter">${icon(def.icon)}</div><div><div class="t">Achievement unlocked</div><div class="n">${esc(def.name)}</div></div>`;
    document.getElementById('app')!.appendChild(el);
    setTimeout(() => {
      el.style.transition = 'opacity .4s ease, transform .4s ease';
      el.style.opacity = '0';
      el.style.transform = 'translate(-50%, -16px)';
      setTimeout(() => el.remove(), 420);
    }, 3600);
  }

  private rng() {
    return runRng(this.run!);
  }

  private setScreen(html: string, screen: Screen, battleView = false): void {
    this.screen = screen;
    this.ui.innerHTML = html;
    this.applyInsets(battleView);
    this.persist();
  }

  private applyInsets(battleView: boolean): void {
    if (!battleView) {
      this.renderer.setInsets(0, 0);
      return;
    }
    const narrow = window.innerWidth < 880;
    this.renderer.setInsets(narrow ? 74 : 76, narrow ? 236 : 152);
  }

  private onResize(): void {
    this.renderer.resize();
    this.applyInsets(this.screen === 'battle');
  }

  private onClick(e: MouseEvent): void {
    const el = (e.target as HTMLElement).closest('[data-action]') as HTMLElement | null;
    this.unlockAudio();
    if (!el) return;
    const action = el.dataset.action!;
    if (action.startsWith('ability:')) {
      audio.play('click');
      this.selectAbility(Number(el.dataset.index));
      return;
    }
    audio.play(action.includes('back') || action === 'close-modal' ? 'back' : 'click');
    switch (action) {
      case 'play':
        this.showHeroes();
        break;
      case 'continue-run':
        this.continueRun();
        break;
      case 'codex':
        this.openCodex();
        break;
      case 'settings':
        this.modalReturn = null;
        this.openSettings();
        break;
      case 'help':
        this.openHelp();
        break;
      case 'forgeworks':
        this.openForgeworks();
        break;
      case 'close-modal':
        this.closeModal();
        break;
      case 'back-title':
        this.showTitle();
        break;
      case 'pick-hero':
        this.startRun(el.dataset.hero!);
        break;
      case 'unlock-hero':
        this.unlockHero(el.dataset.hero!);
        break;
      case 'buy-unlock':
        this.buyMetaUnlock(el.dataset.id!);
        break;
      case 'pick-room':
        this.chooseRoom(el.dataset.room as RoomType);
        break;
      case 'abandon':
        this.openConfirmAbandon();
        break;
      case 'confirm-abandon':
        this.abandonRun();
        break;
      case 'end-turn':
        void this.doEndTurn();
        break;
      case 'pick-reward':
        this.pickReward(el.dataset.key!);
        break;
      case 'skip-reward':
        this.skipReward();
        break;
      case 'shop-buy':
        this.buyShop(el.dataset.key!);
        break;
      case 'shop-reroll':
        this.rerollShop();
        break;
      case 'shop-leave':
        this.leaveRoom();
        break;
      case 'pick-rest':
        this.pickRest(el.dataset.key!);
        break;
      case 'pause':
        this.openPause();
        break;
      case 'resume':
        this.closeModal();
        break;
      case 'result-title':
        this.showTitle();
        break;
      case 'result-again':
        this.showHeroes();
        break;
      case 'codex-tab':
        this.codexTab = el.dataset.tab!;
        this.openCodex();
        break;
      case 'toggle-mute':
        this.toggleMute();
        break;
      case 'toggle-motion':
        this.toggleMotion();
        break;
      case 'reset-progress':
        if (this.resetArmed) {
          this.doReset();
        } else {
          this.resetArmed = true;
          this.openSettings();
          this.toast('Click again to erase all progress.', 'lock');
        }
        break;
      case 'back-to-pause':
        this.openPause();
        break;
    }
  }

  // ── title ────────────────────────────────────────────────────────────────
  private showTitle(): void {
    this.battle = null;
    this.renderer.setBattle(null);
    this.selected = null;
    this.modal = null;
    this.resetArmed = false;
    const hasRun = !!this.save.run && this.save.run.status !== 'won' && this.save.run.status !== 'lost';
    const html = `
      <div class="screen">
        <div class="title-corner">
          <div class="dust-chip">${icon('coin')} ${this.save.meta.dust} dust</div>
          <button class="icon-btn" data-action="codex" title="Codex">${icon('book')}</button>
          <button class="icon-btn" data-action="settings" title="Settings">${icon('settings')}</button>
          <button class="icon-btn" data-action="toggle-mute" title="Sound">${icon(this.save.settings.muted ? 'sound' : 'music')}</button>
        </div>
        <div class="title-wrap">
          <div class="logo">
            <h1>ANVILIGHT</h1>
            <div class="sub">tactics of the last forge</div>
          </div>
          <p class="title-tag">
            The forge is the last warm thing in the world, and everything cold wants it.
            Read every enemy's next move, shove them into steam and stone, and keep the fire alive.
          </p>
          <div class="title-menu">
            ${hasRun ? `<button class="btn btn-primary" data-action="continue-run">${icon('arrow')} Continue run</button>` : ''}
            <button class="btn ${hasRun ? '' : 'btn-primary'}" data-action="play">${icon('flame')} New run</button>
            <button class="btn btn-ghost" data-action="help">${icon('book')} How to play</button>
            <button class="btn btn-ghost" data-action="forgeworks">${icon('anvil')} Forgeworks</button>
          </div>
          <div class="title-menu" style="margin-top:16px">
            <span class="dust-chip" title="Meta currency earned every run">${icon('star')} ${this.save.stats.wins} victories · best ${this.save.stats.bestRooms} rooms</span>
          </div>
        </div>
        <div class="title-foot">A tactical roguelite · everything runs in your browser · v1.0.0</div>
      </div>`;
    this.setScreen(html, 'title');
  }

  // ── hero select ──────────────────────────────────────────────────────────
  private showHeroes(): void {
    const cards = HEROES.map((h, i) => {
      const unlocked = this.save.meta.heroes.includes(h.id);
      const portrait = heroPortrait(h.id, 128);
      return `
        <div class="card hero-card ${unlocked ? '' : 'locked'}" style="animation-delay:${i * 0.06}s">
          <img class="hero-portrait" src="${portrait}" alt="${esc(h.name)}" />
          <div style="flex:1;min-width:0">
            <div class="card-title">${esc(h.name)}</div>
            <div class="card-desc" style="margin-bottom:6px">${esc(h.epithet)}</div>
            <div class="hero-stats">
              <span>${icon('heart')} <b>${h.hp}</b></span>
              <span>${icon('anvil')} <b>${h.forgeHp}</b></span>
              <span>${icon('core')} <b>${h.heatMax}</b> heat</span>
              <span>${icon('step')} <b>${h.moveRange}</b> move</span>
            </div>
            <div class="card-desc" style="margin-top:8px">${esc(h.passiveName)} — ${esc(h.passiveDesc)}</div>
            <div class="card-meta" style="margin-top:10px">
              ${h.starter.map((s) => `<span class="tag">${esc(abilityDef(s.id).name)}</span>`).join('')}
              <span class="tag ember">${esc(h.playstyle)}</span>
            </div>
            <div style="margin-top:12px">
              ${
                unlocked
                  ? `<button class="btn btn-primary btn-sm" data-action="pick-hero" data-hero="${h.id}">${icon('flame')} Take up the lantern</button>`
                  : `<button class="btn btn-sm" data-action="unlock-hero" data-hero="${h.id}" ${this.save.meta.dust < h.unlockDust ? 'aria-disabled="true"' : ''}>${icon('lock')} Unlock · ${h.unlockDust} dust</button>`
              }
            </div>
          </div>
        </div>`;
    }).join('');

    const html = `
      <div class="screen">
        <div class="menu">
          <div class="menu-head">
            <div>
              <div class="eyebrow">choose your keeper</div>
              <h2>Who tends the fire?</h2>
              <div class="sub">Each keeper brings a different body, a different heat, and a different answer to the same problem.</div>
            </div>
            <div style="display:flex;gap:9px;align-items:center">
              <div class="dust-chip">${icon('coin')} ${this.save.meta.dust} dust</div>
              <button class="btn btn-ghost btn-sm" data-action="back-title">${icon('close')} Back</button>
            </div>
          </div>
          <div class="menu-main">
            <div class="cards" style="grid-template-columns:1fr">${cards}</div>
          </div>
        </div>
      </div>`;
    this.setScreen(html, 'heroes');
    this.renderer.setBattle(null);
  }

  private unlockHero(id: string): void {
    const hero = heroDef(id);
    if (this.save.meta.heroes.includes(id)) return;
    if (this.save.meta.dust < hero.unlockDust) {
      this.toast(`Need ${hero.unlockDust - this.save.meta.dust} more dust.`, 'lock');
      return;
    }
    this.save.meta.dust -= hero.unlockDust;
    this.save.meta.heroes.push(id);
    writeSave(this.save);
    audio.play('reward');
    this.toast(`${hero.name} unlocked.`, 'star');
    this.showHeroes();
  }

  private startRun(heroId: string): void {
    const hero = heroDef(heroId);
    if (!this.save.meta.heroes.includes(heroId)) return;
    const unlocks = this.save.meta.unlocks;
    this.run = newRun(heroId, (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0, {
      extraHeat: unlocks.includes('extra_heat') ? 1 : 0,
      extraForge: unlocks.includes('extra_forge') ? 4 : 0,
      warmStart: unlocks.includes('start_common'),
    });
    this.save.stats.runs++;
    this.save.lastHero = heroId;
    this.persist();
    audio.play('reward');
    this.toast(`${hero.name} takes the lantern. Seed ${shortSeed(this.run.seed)}`, 'flame');
    this.showRoute();
  }

  // ── route ────────────────────────────────────────────────────────────────
  private roomIcon(type: RoomType): string {
    switch (type) {
      case 'battle':
        return 'sword';
      case 'elite':
        return 'skull';
      case 'forge':
        return 'anvil';
      case 'rest':
        return 'camp';
      case 'treasure':
        return 'chest';
      case 'boss':
        return 'boss';
    }
  }

  private roomBlurb(type: RoomType): string {
    switch (type) {
      case 'battle':
        return 'A handful of cinderspawn between you and the deep. Embers and a pick of loot.';
      case 'elite':
        return 'Something armoured is waiting. Worse odds, better relics.';
      case 'forge':
        return 'A cold anvil and a trader of salvage. Spend Embers on relics, repairs, and sharpening.';
      case 'rest':
        return 'A coal bed and quiet. Mend, repair the forge, or hone an ability.';
      case 'treasure':
        return 'A vault with three relics inside and room in your hands for one.';
      case 'boss':
        return 'The thing that wants the forge. There is no way around it.';
    }
  }

  private showRoute(): void {
    const run = this.run;
    if (!run) return this.showTitle();
    this.battle = null;
    this.renderer.setBattle(null);
    const hero = heroDef(run.heroId);
    const totalRooms = 4;
    const track: string[] = [];
    for (let i = 0; i < totalRooms; i++) {
      const cls = i < run.room ? 'done' : i === run.room ? 'current' : '';
      track.push(`<div class="track-node ${cls}">${i < run.room ? icon('flame') : i + 1}</div>`);
      track.push('<div class="track-line"></div>');
    }
    track.push(
      `<div class="track-node boss ${isBossRoom(run) ? 'current' : ''}" title="Boss">${icon('boss')}</div>`,
    );

    const offers = run.offers.length ? run.offers : [{ type: 'battle' as RoomType, label: 'Ash Hall' }];
    const cards = offers
      .map(
        (o: RoomOffer, i: number) => `
        <button class="card room-card" data-action="pick-room" data-room="${o.type}" style="animation-delay:${0.05 + i * 0.07}s">
          <div class="card-icon">${icon(this.roomIcon(o.type))}</div>
          <div class="card-title">${esc(o.label)} <span class="arrow">${icon('arrow')}</span></div>
          <div class="card-desc">${esc(this.roomBlurb(o.type))}</div>
          <div class="card-meta">
            <span class="tag">${o.type === 'boss' ? 'final' : o.type}</span>
            ${this.isDangerous(o.type) ? `<span class="tag bad">danger</span>` : ''}
          </div>
        </button>`,
      )
      .join('');

    const relics = run.relics
      .map((id) => `<div class="relic-chip r-${relicDef(id).rarity}" title="${esc(relicDef(id).name)} — ${esc(relicDef(id).desc)}">${icon(relicDef(id).icon)}</div>`)
      .join('');

    const abilities = run.abilities
      .map((a) => {
        const def = abilityDef(a.id);
        const cost = abilityStats(a.id, a.level).cost;
        return `<div class="ability-mini">
          <div class="card-icon r-${def.rarity}">${icon(def.icon)}</div>
          <div style="min-width:0"><div style="font-size:12.5px">${esc(def.name)}${a.level > 0 ? ' +' : ''}</div>
          <div style="font-size:10.5px;color:var(--muted-2)">${esc(def.text(a.level))}</div></div>
          <div class="cost">${cost}</div>
        </div>`;
      })
      .join('');

    const html = `
      <div class="screen">
        <div class="menu">
          <div class="menu-head">
            <div>
              <div class="eyebrow">${esc(actName(run))} · room ${Math.min(run.room + 1, 5)} of 5</div>
              <h2>${isBossRoom(run) ? 'The Heart of the Keep' : 'Choose a door'}</h2>
              <div class="sub">${isBossRoom(run) ? 'No detours. The forge\'s enemy is behind this one.' : 'Every door is a trade. Pick what you can survive.'}</div>
            </div>
            <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center">
              <div class="dust-chip">${icon('coin')} ${run.embers} embers</div>
              <button class="icon-btn" data-action="codex" title="Codex">${icon('book')}</button>
              <button class="icon-btn" data-action="settings" title="Settings">${icon('settings')}</button>
              <button class="btn btn-ghost btn-sm" data-action="abandon">${icon('close')} Abandon</button>
            </div>
          </div>
          <div class="route-track">${track.join('')}</div>
          <div class="menu-body">
            <div class="menu-main">
              <div class="cards">${cards}</div>
              <div class="hint" style="margin-top:6px">Tip: hover a foe in battle to read exactly where it will strike. Shoves and vents kill more efficiently than fire.</div>
            </div>
            <div class="menu-side">
              <div class="side-block">
                <h4>${esc(hero.name)}</h4>
                <div style="display:flex;gap:10px;align-items:center;margin-bottom:10px">
                  <img class="hero-portrait" style="width:52px;height:58px" src="${heroPortrait(run.heroId, 96)}" alt="" />
                  <div style="flex:1">
                    <div class="bar-label"><span>Health</span><b>${run.hp}/${run.maxHp}</b></div>
                    <div class="bar hero"><i style="width:${(run.hp / run.maxHp) * 100}%"></i></div>
                  </div>
                </div>
                <div class="bar-label"><span>Forge</span><b>${run.forgeHp}/${run.maxForgeHp}</b></div>
                <div class="bar forge"><i style="width:${(run.forgeHp / run.maxForgeHp) * 100}%"></i></div>
                <div class="stat-row" style="margin-top:8px"><span>Heat</span><b>${run.heatMax} max · +${run.heatRegen}/turn</b></div>
                <div class="stat-row"><span>Kills this run</span><b>${run.runKills}</b></div>
                <div class="stat-row"><span>Rooms cleared</span><b>${run.roomsCleared}</b></div>
              </div>
              <div class="side-block">
                <h4>Relics · ${run.relics.length}</h4>
                <div class="relic-row">${relics || '<span class="hint">No relics yet. They come from rewards and cold forges.</span>'}</div>
              </div>
              <div class="side-block">
                <h4>Belt · ${run.abilities.length}/${MAX_ABILITIES}</h4>
                ${abilities}
              </div>
            </div>
          </div>
        </div>
      </div>`;
    this.setScreen(html, 'route');
  }

  private isDangerous(type: RoomType): boolean {
    return type === 'elite' || type === 'boss';
  }

  private chooseRoom(type: RoomType): void {
    const run = this.run;
    if (!run) return;
    const rng = this.rng();
    enterRoom(run, rng, type);
    persistRng(run, rng);
    this.persist();
    if (run.status === 'battle') {
      this.beginBattle();
    } else if (run.status === 'forge') {
      this.showShop();
    } else if (run.status === 'rest') {
      this.showRest();
    } else if (run.status === 'treasure') {
      this.showTreasure();
    }
  }

  // ── battle ───────────────────────────────────────────────────────────────
  private beginBattle(): void {
    const run = this.run;
    if (!run) return this.showTitle();
    // Replaying a saved battle from its recorded RNG state keeps the arena,
    // enemy waves and loot identical after a reload.
    const pending = run.pendingBattle;
    if (pending) {
      run.currentRoom = pending.type;
      run.rngState = pending.rngState;
    }
    const rng = this.rng();
    const room = (run.currentRoom ?? run.pendingBattle?.type ?? 'battle') as RoomType;
    this.encounter = makeEncounter(run, rng, room);
    this.battle = startBattle(run, rng, this.encounter);
    persistRng(run, rng);
    this.maxCasts = 0;
    this.maxKillsTurn = 0;
    this.selected = null;
    this.busy = false;
    for (const g of this.encounter.waves) {
      for (const grp of g.groups) {
        if (!this.save.meta.enemiesSeen.includes(grp.typeId)) this.save.meta.enemiesSeen.push(grp.typeId);
      }
    }
    this.renderer.setBattle(this.battle);
    this.showBattleFrame();
    this.persist();

    const intro =
      this.encounter.kind === 'boss'
        ? `${this.encounter.title} · ${this.encounter.subtitle}`
        : `${this.encounter.title} — ${this.encounter.subtitle}`;
    this.toast(intro, this.encounter.kind === 'boss' ? 'boss' : 'sword');
    if (run.roomsCleared === 0 && this.encounter.kind === 'battle') {
      setTimeout(() => this.toast('Click a blue tile to move. Click a foe to read its plan.', 'step'), 1800);
    }
  }

  private showBattleFrame(): void {
    const b = this.battle;
    const run = this.run;
    if (!b || !run) return;
    const hero = heroDef(b.hero.heroId);
    const abilityCards = b.hero.abilities
      .map((a, i) => {
        const def = abilityDef(a.id);
        const cost = abilityCost(b, a);
        return `<button class="ability r-${def.rarity}" data-action="ability:${i}" data-index="${i}" data-slot="${i}">
          <span class="num">${i + 1}</span>
          <span class="head">${icon(def.icon)}<span class="cost">${cost}</span></span>
          <span class="title">${esc(def.name)}${a.level > 0 ? ' +' : ''}</span>
          <span class="desc">${esc(def.text(a.level))}</span>
        </button>`;
      })
      .join('');

    const pips = Array.from({ length: b.hero.maxHeat }, (_, i) => `<span class="pip" data-pip="${i}"></span>`).join('');

    const html = `
      <div class="screen passthrough">
        <div class="battle">
          <div class="hud-top">
            <div class="hero-plate">
              <img src="${heroPortrait(b.hero.heroId, 96)}" alt="" />
              <div class="info">
                <div class="name">${esc(hero.name)}</div>
                <div class="bar hero" id="hp-bar"><i id="hp-fill"></i></div>
                <div class="hpline"><span id="hp-text">${b.hero.hp}/${b.hero.maxHp}</span><span class="shield-chip" id="shield-chip"></span><span class="spacer"></span><span id="move-state" class="move-state"></span></div>
              </div>
            </div>
            <div class="hud-center">
              <div class="turn-chip">turn <b id="turn-num">${b.turn}</b></div>
              <div class="hint" id="wave-text" style="font-size:10.5px"></div>
            </div>
            <div class="hud-right">
              <div class="forge-plate">
                ${icon('anvil')}
                <div class="info">
                  <div class="name">The forge</div>
                  <div class="bar forge" id="forge-bar"><i id="forge-fill"></i></div>
                </div>
                <div style="font-family:var(--serif);font-size:13px;color:var(--ember-hot)" id="forge-text"></div>
              </div>
              <div class="hud-buttons">
                <button class="icon-btn" data-action="help" title="How to play">${icon('book')}</button>
                <button class="icon-btn" data-action="toggle-mute" title="Mute">${icon(this.save.settings.muted ? 'sound' : 'music')}</button>
                <button class="icon-btn" data-action="pause" title="Pause (Esc)">${icon('pause')}</button>
              </div>
            </div>
          </div>
          <div></div>
          <div class="hud-bottom">
            <div class="heat-plate">
              <div class="label">Heat</div>
              <div class="pips" id="pips">${pips}</div>
              <div class="heat-num" id="heat-num">${b.hero.heat}<small>/${b.hero.maxHeat}</small></div>
            </div>
            <div class="ability-bar" id="ability-bar">${abilityCards}</div>
            <div class="endturn-wrap">
              <button class="btn btn-primary endturn" data-action="end-turn" id="end-turn">${icon('flame')} End turn <span class="key">space</span></button>
              <div class="move-state" id="move-state-2"></div>
            </div>
          </div>
          <div class="inspect" id="inspect"></div>
        </div>
      </div>`;
    this.setScreen(html, 'battle', true);
    this.updateHud();
    this.refreshTargets();
    this.updateInspect(null);
  }

  private updateHud(): void {
    const b = this.battle;
    if (!b) return;
    const hpFill = document.getElementById('hp-fill');
    const hpText = document.getElementById('hp-text');
    const forgeFill = document.getElementById('forge-fill');
    const forgeText = document.getElementById('forge-text');
    const turnNum = document.getElementById('turn-num');
    const shield = document.getElementById('shield-chip');
    const moveState = document.getElementById('move-state');
    const moveState2 = document.getElementById('move-state-2');
    const heatNum = document.getElementById('heat-num');
    const pips = document.getElementById('pips');
    const endTurnBtn = document.getElementById('end-turn') as HTMLButtonElement | null;
    const waveText = document.getElementById('wave-text');

    if (hpFill) hpFill.style.width = `${Math.max(0, (b.hero.hp / b.hero.maxHp) * 100)}%`;
    if (hpText) hpText.textContent = `${Math.max(0, b.hero.hp)}/${b.hero.maxHp}`;
    if (forgeFill) forgeFill.style.width = `${Math.max(0, (b.forge.hp / b.forge.maxHp) * 100)}%`;
    if (forgeText) forgeText.textContent = `${Math.max(0, b.forge.hp)}`;
    if (turnNum) turnNum.textContent = String(b.turn);
    if (shield) shield.innerHTML = b.hero.shield > 0 ? `${icon('shield')} ${b.hero.shield}` : '';
    if (moveState) moveState.textContent = b.hero.movesLeft ? 'move ready' : 'move spent';
    if (moveState) moveState.className = `move-state ${b.hero.movesLeft ? 'ready' : ''}`;
    if (moveState2) moveState2.textContent = b.hero.movesLeft ? 'move ready' : 'move spent';
    if (moveState2) moveState2.className = `move-state ${b.hero.movesLeft ? 'ready' : ''}`;
    if (heatNum) heatNum.innerHTML = `${b.hero.heat}<small>/${b.hero.maxHeat}</small>`;
    if (pips) {
      for (const el of Array.from(pips.children) as HTMLElement[]) {
        const i = Number(el.dataset.pip);
        el.classList.toggle('on', i < b.hero.heat);
      }
    }
    if (endTurnBtn) endTurnBtn.disabled = this.busy || b.phase !== 'player';
    const living = b.enemies.filter((e) => e.hp > 0).length;
    const pending = b.waves.length - b.waveIndex;
    if (waveText) waveText.textContent = `${living} foe${living === 1 ? '' : 's'}${pending > 0 ? ` · ${pending} wave${pending === 1 ? '' : 's'} inbound` : ''}`;

    const abilityEls = Array.from(document.querySelectorAll('.ability')) as HTMLElement[];
    for (const el of abilityEls) {
      const i = Number(el.dataset.index);
      const inst = b.hero.abilities[i];
      if (!inst) continue;
      const cost = abilityCost(b, inst);
      el.classList.toggle('selected', this.selected === i);
      el.classList.toggle('unaffordable', cost > b.hero.heat);
      const costEl = el.querySelector('.cost');
      if (costEl) costEl.textContent = String(cost);
    }
  }

  private updateInspect(e: ReturnType<typeof enemyAt>): void {
    const b = this.battle;
    const el = document.getElementById('inspect');
    if (!b || !el) return;
    if (e && e.hp > 0) {
      const def = enemyDef(e.typeId);
      el.innerHTML = `
        <h5>${esc(def.name)}${e.elite ? ' <span class="tag bad">elite</span>' : ''}</h5>
        <div class="line"><span>Health</span><b>${e.hp}/${e.maxHp}${e.shield ? ` (+${e.shield} shield)` : ''}</b></div>
        <div class="line"><span>Strike</span><b>${e.dmg} · range ${e.range}</b></div>
        <div class="line"><span>Movement</span><b>${e.speed}/turn</b></div>
        <div class="intent">${icon('sword')} ${esc(intentText(b, e))}</div>
        <div style="margin-top:6px;font-size:11px;color:var(--muted-2);line-height:1.4">${esc(def.desc)}</div>`;
      return;
    }
    if (this.selected !== null) {
      const inst = b.hero.abilities[this.selected];
      if (inst) {
        const def = abilityDef(inst.id);
        el.innerHTML = `
          <h5>${esc(def.name)}${inst.level > 0 ? ' +' : ''}</h5>
          <div style="font-size:12px;color:var(--muted);line-height:1.45">${esc(def.text(inst.level))}</div>
          <div class="intent">${icon('arrow')} Click a highlighted tile to cast. Esc to cancel.</div>`;
        return;
      }
    }
    el.innerHTML = `
      <h5>The forge remembers</h5>
      <div style="font-size:11.5px;line-height:1.5;color:var(--muted)">
        Every red tile is a promise: a foe will strike it during the enemy phase.
        Move out of it, shove the attacker, or kill it first.
      </div>
      <div class="intent">${icon('vent')} Steam vents and walls do the heavy lifting. Shove foes into them.</div>`;
  }

  private refreshTargets(): void {
    const b = this.battle;
    if (!b) return;
    const r = this.renderer;
    r.selectedAbility = this.selected;
    if (this.selected !== null) {
      const inst = b.hero.abilities[this.selected];
      if (!inst) {
        this.selected = null;
        r.selectedAbility = null;
      } else {
        const tiles = abilityTargets(b, inst);
        this.abilityTiles = new Set(tiles.map(vkey));
        r.highlights = tiles;
        r.highlightKind = 'ability';
        r.path = [];
        this.updateHud();
        this.updateInspect(null);
        return;
      }
    }
    this.abilityTiles.clear();
    this.moveTiles = heroReach(b);
    r.highlights = Array.from(this.moveTiles.keys()).map((k) => {
      const [x, y] = k.split(',').map(Number);
      return { x, y };
    });
    r.highlightKind = 'move';
    r.path = [];
    this.updateHud();
  }

  private selectAbility(index: number): void {
    const b = this.battle;
    if (!b || this.busy || b.phase !== 'player') return;
    const inst = b.hero.abilities[index];
    if (!inst) return;
    this.selected = this.selected === index ? null : index;
    this.refreshTargets();
    if (this.selected === null) this.updateInspect(null);
  }

  private onPointerMove(e: PointerEvent): void {
    if (!this.battle) return;
    const p = this.renderer.toGrid(e.clientX, e.clientY);
    this.hover = p;
    this.renderer.hover = p;
    if (!p) {
      this.renderer.path = [];
      this.updateInspect(null);
      return;
    }
    this.updateInspect(enemyAt(this.battle, p));
    if (this.selected === null) {
      const key = vkey(p);
      if (this.moveTiles.has(key)) {
        const path = this.battle.board.pathTo(
          this.battle.hero.pos,
          (q) => eq(q, p),
          (q) => this.battle!.forge.tiles.some((f) => eq(f, q)) || !!enemyAt(this.battle!, q),
        );
        this.renderer.path = path ?? [];
      } else {
        this.renderer.path = [];
      }
    }
  }

  private onPointerDown(e: PointerEvent): void {
    if (!this.battle || this.busy || this.battle.phase !== 'player') return;
    const p = this.renderer.toGrid(e.clientX, e.clientY);
    if (!p) return;
    if (this.selected !== null) {
      if (this.abilityTiles.has(vkey(p))) {
        const index = this.selected;
        this.selected = null;
        this.castAt(index, p);
      } else {
        this.selected = null;
        this.refreshTargets();
        this.updateInspect(enemyAt(this.battle, p));
      }
      return;
    }
    if (eq(p, this.battle.hero.pos)) return;
    if (this.moveTiles.has(vkey(p))) {
      void this.doMove(p);
    }
  }

  private async doMove(target: Vec): Promise<void> {
    const b = this.battle;
    if (!b) return;
    const events = moveHero(b, target);
    if (!events.length) return;
    this.trackPeaks();
    await this.runEvents(events);
  }

  private castAt(index: number, target: Vec): void {
    const b = this.battle;
    if (!b) return;
    const events = castAbility(b, index, target);
    if (!events.length) {
      this.refreshTargets();
      return;
    }
    this.trackPeaks();
    void this.runEvents(events);
  }

  private trackPeaks(): void {
    if (!this.battle) return;
    this.maxCasts = Math.max(this.maxCasts, this.battle.castsThisTurn);
    this.maxKillsTurn = Math.max(this.maxKillsTurn, this.battle.killsThisTurn);
  }

  private async runEvents(events: BattleEvent[]): Promise<void> {
    this.busy = true;
    this.refreshTargets();
    this.updateHud();
    await this.renderer.playEvents(events);
    this.busy = false;
    if (!this.battle) return;
    this.selected = null;
    this.updateHud();
    this.refreshTargets();
    this.updateInspect(this.hover ? enemyAt(this.battle, this.hover) : null);
    if (this.battle.phase === 'won' || this.battle.phase === 'lost') {
      await this.finishBattleFlow();
    }
  }

  private async doEndTurn(): Promise<void> {
    const b = this.battle;
    if (!b || this.busy || b.phase !== 'player') return;
    this.trackPeaks();
    this.selected = null;
    this.refreshTargets();
    const events = resolveTurn(b);
    if (!events.length) return;
    await this.runEvents(events);
  }

  private onKey(e: KeyboardEvent): void {
    this.unlockAudio();
    if (e.key === 'Escape') {
      if (this.modal) {
        this.closeModal();
      } else if (this.screen === 'battle') {
        if (this.selected !== null) {
          this.selected = null;
          this.refreshTargets();
        } else {
          this.openPause();
        }
      }
      return;
    }
    if (this.modal) return;
    if (this.screen !== 'battle' || !this.battle) return;
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      void this.doEndTurn();
      return;
    }
    const n = Number(e.key);
    if (!Number.isNaN(n) && n >= 1 && n <= 9) {
      this.selectAbility(n - 1);
    }
    if (e.key.toLowerCase() === 'm') this.toggleMute();
  }

  private async finishBattleFlow(): Promise<void> {
    const run = this.run;
    const b = this.battle;
    if (!run || !b) return;
    const rng = this.rng();
    const bossDefeated = this.encounter?.bossId ?? null;
    const outcome = finishBattle(run, b, rng);
    persistRng(run, rng);
    this.lastOutcome = outcome;

    this.save.stats.kills += b.kills;
    this.save.stats.shoveKills += b.shoveKills;
    if (b.roomKind === 'elite') this.save.stats.eliteClears += outcome.won ? 1 : 0;
    this.save.stats.bestRooms = Math.max(this.save.stats.bestRooms, run.roomsCleared);
    this.save.stats.bestTurnKills = Math.max(this.save.stats.bestTurnKills, this.maxKillsTurn);
    this.save.stats.bestCasts = Math.max(this.save.stats.bestCasts, this.maxCasts);

    const unlocked = checkAchievements(this.save, {
      battleWon: outcome.won,
      runWon: run.status === 'won',
      heroId: run.heroId,
      bossDefeated: outcome.won ? bossDefeated : null,
      forgeUntouched: run.status === 'won' && run.forgeHp >= run.maxForgeHp,
      roomsCleared: run.roomsCleared,
      turnKills: this.maxKillsTurn,
      casts: this.maxCasts,
    });
    for (const id of unlocked) this.showAchievement(id);

    if (!outcome.won) {
      this.save.stats.defeats++;
      this.bankDust(run);
      this.run = null;
      this.save.run = null;
      this.battle = null;
      this.renderer.setBattle(null);
      this.showResult(false);
      return;
    }
    this.battle = null;
    this.renderer.setBattle(null);
    if (run.status === 'won') {
      this.save.stats.wins++;
      this.bankDust(run);
      this.run = null;
      this.save.run = null;
      this.showResult(true);
      return;
    }
    audio.play('reward');
    if (run.status === 'treasure') this.showTreasure();
    else this.showReward();
  }

  private bankDust(run: RunState): void {
    const dust = Math.max(0, Math.round(run.dustEarned));
    this.save.meta.dust += dust;
    this.save.stats.totalDust += dust;
    this.persist();
  }

  // ── rewards ──────────────────────────────────────────────────────────────
  private rewardCard(o: RewardOffer, action = 'pick-reward', note?: string): string {
    return `
      <button class="card r-${o.rarity}" data-action="${action}" data-key="${o.key}">
        <div class="card-icon">${icon(o.icon)}</div>
        <div class="card-title">${esc(o.title)}</div>
        <div class="card-desc">${esc(o.desc)}</div>
        <div class="card-meta">
          <span class="tag">${o.kind === 'upgrade' ? 'upgrade' : o.kind === 'relic' ? rarityLabel[o.rarity] : o.kind}</span>
          ${note ? `<span class="tag ember">${esc(note)}</span>` : ''}
        </div>
        <div class="pick">${icon('arrow')} Take it</div>
      </button>`;
  }

  private showReward(): void {
    const run = this.run;
    if (!run) return this.showTitle();
    const cards = run.rewards.map((r) => this.rewardCard(r)).join('');
    const html = `
      <div class="screen">
        <div class="reward-stage">
          <div class="reward-title">
            <div class="eyebrow">${esc(actName(run))} · room ${run.room} cleared</div>
            <h2>${this.lastOutcome?.won === false ? 'The hall falls quiet' : 'The hall falls quiet'}</h2>
            <p>Choose one thing to carry deeper.${this.lastOutcome ? ` Embers gained: <b>+${this.lastOutcome.embers}</b>.` : ''}</p>
          </div>
          <div class="reward-cards">${cards}</div>
          <div style="display:flex;gap:10px;flex-wrap:wrap;justify-content:center">
            <button class="btn btn-ghost" data-action="skip-reward">${icon('coin')} Skip · gain 12 embers</button>
            <div class="dust-chip">${icon('coin')} ${run.embers} embers</div>
          </div>
        </div>
      </div>`;
    this.setScreen(html, 'reward');
  }

  private pickReward(key: string): void {
    const run = this.run;
    if (!run) return;
    const offer = run.rewards.find((r) => r.key === key);
    if (!offer) return;
    const rng = this.rng();
    const msg = this.grant(run, offer, rng);
    persistRng(run, rng);
    audio.play('reward');
    this.toast(msg, offer.icon);
    this.afterRoom(rng);
  }

  private skipReward(): void {
    const run = this.run;
    if (!run) return;
    run.embers += 12;
    const rng = this.rng();
    persistRng(run, rng);
    this.toast('You bank 12 Embers instead.', 'coin');
    this.afterRoom(rng);
  }

  private grant(run: RunState, offer: RewardOffer, rng: ReturnType<typeof runRng>): string {
    // Re-implemented here so upgrades can respect specific abilities.
    if (offer.kind === 'relic') {
      const def = relicDef(offer.relicId!);
      if (!run.relics.includes(def.id)) {
        run.relics.push(def.id);
        this.save.meta.relicsSeen = Array.from(new Set([...this.save.meta.relicsSeen, def.id]));
        if (def.id === 'bellows2') run.heatMax += 2;
        if (def.id === 'forge_bond') {
          run.maxForgeHp += 6;
          run.forgeHp = run.maxForgeHp;
        }
        if (def.id === 'tempered') {
          run.maxHp += 3;
          run.hp = Math.min(run.maxHp, run.hp + 3);
        }
      }
      return `Relic: ${def.name}`;
    }
    if (offer.kind === 'ability') {
      const def = abilityDef(offer.abilityId!);
      if (run.abilities.length < MAX_ABILITIES && !run.abilities.some((a) => a.id === def.id)) {
        run.abilities.push({ id: def.id, level: 0 });
        this.save.meta.abilitiesSeen = Array.from(new Set([...this.save.meta.abilitiesSeen, def.id]));
        return `Learned ${def.name}`;
      }
      return 'Your belt is full.';
    }
    if (offer.kind === 'upgrade') {
      const upgradable = run.abilities.filter((a) => a.level < 1);
      if (!upgradable.length) return 'Everything is already sharp.';
      const pick = rng.pick(upgradable);
      pick.level = 1;
      return `Sharpened ${abilityDef(pick.id).name}`;
    }
    if (offer.kind === 'heal') {
      const before = run.hp;
      run.hp = Math.min(run.maxHp, run.hp + 6);
      return `Healed ${run.hp - before}`;
    }
    if (offer.kind === 'repair') {
      const before = run.forgeHp;
      run.forgeHp = Math.min(run.maxForgeHp, run.forgeHp + 6);
      return `Forge repaired by ${run.forgeHp - before}`;
    }
    run.embers += 25;
    return 'Gained 25 Embers';
  }

  private afterRoom(rng: ReturnType<typeof runRng>): void {
    const run = this.run;
    if (!run) return;
    continueRoute(run, rng);
    persistRng(run, rng);
    this.persist();
    this.showRoute();
  }

  // ── shop ─────────────────────────────────────────────────────────────────
  private showShop(): void {
    const run = this.run;
    if (!run) return this.showTitle();
    const items = run.shop
      .map(
        (item: ShopItem) => `
        <div class="card r-${item.rarity}" style="cursor:default">
          <div class="card-icon">${icon(item.icon)}</div>
          <div class="card-title">${esc(item.title)}</div>
          <div class="card-desc">${esc(item.desc)}</div>
          <div class="card-meta">
            <span class="tag">${item.kind}</span>
            <span class="spacer"></span>
            <button class="btn btn-sm ${run.embers >= item.price ? 'btn-primary' : ''}" data-action="shop-buy" data-key="${esc(item.key)}" ${run.embers < item.price ? 'aria-disabled="true"' : ''}>
              ${icon('coin')} ${item.price}
            </button>
          </div>
        </div>`,
      )
      .join('');
    const html = `
      <div class="screen">
        <div class="menu">
          <div class="menu-head">
            <div>
              <div class="eyebrow">cold forge · trade in salvage</div>
              <h2>The Cold Forge</h2>
              <div class="sub">Nothing here is free, but the price is only fire.</div>
            </div>
            <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
              <div class="dust-chip">${icon('coin')} ${run.embers} embers</div>
              <button class="btn btn-ghost btn-sm" data-action="shop-reroll">${icon('echo')} Reroll · ${run.rerollCost}</button>
              <button class="btn btn-primary btn-sm" data-action="shop-leave">${icon('arrow')} Leave</button>
            </div>
          </div>
          <div class="menu-body">
            <div class="menu-main">
              <div class="cards">${items}</div>
              <div class="hint">Relics are permanent for this run. Abilities go into your belt (max ${MAX_ABILITIES}). Repairs keep the forge alive.</div>
            </div>
            <div class="menu-side">
              <div class="side-block">
                <h4>Your fire</h4>
                <div class="bar-label"><span>Health</span><b>${run.hp}/${run.maxHp}</b></div>
                <div class="bar hero"><i style="width:${(run.hp / run.maxHp) * 100}%"></i></div>
                <div class="bar-label" style="margin-top:10px"><span>Forge</span><b>${run.forgeHp}/${run.maxForgeHp}</b></div>
                <div class="bar forge"><i style="width:${(run.forgeHp / run.maxForgeHp) * 100}%"></i></div>
                <div class="stat-row" style="margin-top:8px"><span>Abilities</span><b>${run.abilities.length}/${MAX_ABILITIES}</b></div>
              </div>
              <div class="side-block">
                <h4>Relics</h4>
                <div class="relic-row">${run.relics.map((id) => `<div class="relic-chip r-${relicDef(id).rarity}" title="${esc(relicDef(id).desc)}">${icon(relicDef(id).icon)}</div>`).join('') || '<span class="hint">None yet.</span>'}</div>
              </div>
            </div>
          </div>
        </div>
      </div>`;
    this.setScreen(html, 'shop');
    this.renderer.setBattle(null);
  }

  private buyShop(key: string): void {
    const run = this.run;
    if (!run) return;
    const item = run.shop.find((i) => i.key === key);
    if (!item) return;
    const rng = this.rng();
    const result = buyItem(run, item, rng);
    persistRng(run, rng);
    if (!result.ok) {
      audio.play('back');
      this.toast(result.message, 'close');
      return;
    }
    this.save.stats.totalEmbersSpent += item.price;
    run.shop = run.shop.filter((i) => i.key !== key);
    audio.play('reward');
    this.toast(result.message, item.icon);
    this.persist();
    this.showShop();
  }

  private rerollShop(): void {
    const run = this.run;
    if (!run) return;
    if (run.embers < run.rerollCost) {
      this.toast('Not enough Embers to reroll.', 'coin');
      return;
    }
    run.embers -= run.rerollCost;
    run.rerollCost += 6;
    const rng = this.rng();
    run.shop = rollShop(run, rng);
    persistRng(run, rng);
    this.persist();
    this.showShop();
  }

  private leaveRoom(): void {
    const rng = this.rng();
    this.afterRoom(rng);
  }

  // ── rest & treasure ──────────────────────────────────────────────────────
  private showRest(): void {
    const run = this.run;
    if (!run) return this.showTitle();
    const cards = run.restOptions
      .map(
        (o, i) => `
        <button class="card" data-action="pick-rest" data-key="${o.key}" style="animation-delay:${i * 0.06}s">
          <div class="card-icon">${icon(o.icon)}</div>
          <div class="card-title">${esc(o.title)}</div>
          <div class="card-desc">${esc(o.desc)}</div>
          <div class="pick">${icon('arrow')} Rest here</div>
        </button>`,
      )
      .join('');
    const html = `
      <div class="screen">
        <div class="reward-stage">
          <div class="reward-title">
            <div class="eyebrow">ember rest</div>
            <h2>An hour of warmth</h2>
            <p>The forge dims politely while you decide what to spend the quiet on.</p>
          </div>
          <div class="reward-cards">${cards}</div>
          <div style="display:flex;gap:12px;align-items:center">
            <div class="dust-chip">${icon('heart')} ${run.hp}/${run.maxHp}</div>
            <div class="dust-chip">${icon('anvil')} ${run.forgeHp}/${run.maxForgeHp}</div>
          </div>
        </div>
      </div>`;
    this.setScreen(html, 'rest');
    this.renderer.setBattle(null);
  }

  private pickRest(key: string): void {
    const run = this.run;
    if (!run) return;
    const option = run.restOptions.find((o) => o.key === key);
    if (!option) return;
    const rng = this.rng();
    if (option.kind === 'upgrade') {
      const upgradable = run.abilities.filter((a) => a.level < 1);
      if (upgradable.length) {
        const pick = rng.pick(upgradable);
        pick.level = 1;
        this.toast(`Sharpened ${abilityDef(pick.id).name}`, 'hammer');
      } else {
        run.embers += 30;
        this.toast('Everything is already sharp. +30 Embers.', 'coin');
      }
    } else if (option.kind === 'heal') {
      const before = run.hp;
      run.hp = Math.min(run.maxHp, run.hp + 7);
      this.toast(`Healed ${run.hp - before}`, 'heal');
    } else if (option.kind === 'repair') {
      const before = run.forgeHp;
      run.forgeHp = Math.min(run.maxForgeHp, run.forgeHp + 8);
      this.toast(`Forge repaired by ${run.forgeHp - before}`, 'anvil');
    } else {
      run.embers += 30;
      this.toast('Gained 30 Embers', 'coin');
    }
    audio.play('reward');
    persistRng(run, rng);
    this.afterRoom(rng);
  }

  private showTreasure(): void {
    const run = this.run;
    if (!run) return this.showTitle();
    const cards = run.rewards.map((r) => this.rewardCard(r)).join('');
    const html = `
      <div class="screen">
        <div class="reward-stage">
          <div class="reward-title">
            <div class="eyebrow">sealed vault</div>
            <h2>Three relics, one hand</h2>
            <p>The vault exhales dust and lets you choose.</p>
          </div>
          <div class="reward-cards">${cards}</div>
        </div>
      </div>`;
    this.setScreen(html, 'treasure');
    this.renderer.setBattle(null);
  }

  // ── results ──────────────────────────────────────────────────────────────
  private showResult(victory: boolean): void {
    const stats = this.save.stats;
    const dust = this.lastOutcome?.dust ?? 0;
    const html = `
      <div class="screen">
        <div class="reward-stage">
          <div class="reward-title">
            <div class="eyebrow">${victory ? 'the forge survives' : 'the fire goes out'}</div>
            <h1 style="font-size:clamp(34px,6vw,64px);background:linear-gradient(180deg,#fff3dd,#ffb26b 55%,#a63c14);-webkit-background-clip:text;background-clip:text;color:transparent;letter-spacing:.08em">
              ${victory ? 'THE FORGE LIVES' : 'ANVILIGHT FADES'}
            </h1>
            <p>${victory ? 'You carried the last coal through the rusted deep.' : esc(this.lastOutcome?.reason ?? 'The dark wins this time.')}</p>
          </div>
          <div class="result-stat-grid" style="max-width:640px;width:100%">
            <div class="result-stat"><div class="v">${this.lastOutcome?.kills ?? 0}</div><div class="k">kills</div></div>
            <div class="result-stat"><div class="v">${this.lastOutcome?.shoveKills ?? 0}</div><div class="k">terrain kills</div></div>
            <div class="result-stat"><div class="v">${this.save.stats.bestRooms}</div><div class="k">best rooms</div></div>
            <div class="result-stat"><div class="v">+${dust}</div><div class="k">dust earned</div></div>
          </div>
          <div class="hint">Total dust: ${this.save.meta.dust} · runs ${stats.runs} · victories ${stats.wins}</div>
          <div style="display:flex;gap:10px;flex-wrap:wrap;justify-content:center">
            <button class="btn btn-primary" data-action="result-again">${icon('flame')} Light it again</button>
            <button class="btn btn-ghost" data-action="result-title">${icon('anvil')} Main menu</button>
          </div>
        </div>
      </div>`;
    this.setScreen(html, 'result');
    this.renderer.setBattle(null);
    audio.play(victory ? 'victory' : 'defeat');
    this.persist();
  }

  // ── modals ───────────────────────────────────────────────────────────────
  private openModal(html: string, id: string): void {
    document.getElementById('modal')?.remove();
    this.modal = id;
    const wrap = document.createElement('div');
    wrap.className = 'overlay';
    wrap.id = 'modal';
    wrap.dataset.action = 'overlay';
    wrap.innerHTML = `<div class="dialog ${id === 'codex' ? 'wide' : ''}" role="dialog" aria-modal="true">${html}</div>`;
    wrap.addEventListener('click', (e) => {
      if (e.target === wrap) this.closeModal();
    });
    document.getElementById('app')!.appendChild(wrap);
  }

  private closeModal(keepSetting = false): void {
    document.getElementById('modal')?.remove();
    this.modal = null;
    if (!keepSetting) this.modalReturn = null;
  }

  private openHelp(): void {
    const html = `
      <div class="dialog-head">
        <h3>How to keep the forge alive</h3>
        <button class="icon-btn" data-action="close-modal">${icon('close')}</button>
      </div>
      <div class="help-grid">
        <div class="help-item"><h5>${icon('step')} Move</h5><p>Click any blue tile within range. You get one move per turn, then your abilities.</p></div>
        <div class="help-item"><h5>${icon('flame')} Heat</h5><p>Abilities cost Heat (the diamonds). You refill at the start of every turn, and kills refund extra.</p></div>
        <div class="help-item"><h5>${icon('sword')} Telegraphs</h5><p>Every enemy shows exactly which tile it will strike next phase. Move off that tile and it misses.</p></div>
        <div class="help-item"><h5>${icon('momentum')} Shove</h5><p>Pushing a foe into a wall deals impact damage. Pushing it into a steam vent deals 2 more. Pushing is often cheaper than killing.</p></div>
        <div class="help-item"><h5>${icon('anvil')} The forge</h5><p>You lose if the forge's health hits zero. Repair it in cold forges and rests.</p></div>
        <div class="help-item"><h5>${icon('route')} Rooms</h5><p>Each room offers three doors: fights, elites, shops, rests and vaults. The boss waits at the end of the act.</p></div>
      </div>
      <div class="rune-divider"></div>
      <div class="hint">Keyboard: <b>1–5</b> choose an ability, <b>Space</b> end turn, <b>Esc</b> cancel / pause, <b>M</b> mute.</div>
      <div class="dialog-actions"><button class="btn btn-primary" data-action="close-modal">Got it</button></div>`;
    this.openModal(html, 'help');
  }

  private openSettings(): void {
    const s = this.save.settings;
    const html = `
      <div class="dialog-head">
        <h3>Settings</h3>
        <button class="icon-btn" data-action="close-modal">${icon('close')}</button>
      </div>
      <div class="setting-row">
        <div class="label">Music<small>Generative forge drone</small></div>
        <input type="range" min="0" max="1" step="0.05" value="${s.music}" data-setting="music" id="set-music" />
      </div>
      <div class="setting-row">
        <div class="label">Effects<small>Casts, impacts and shoves</small></div>
        <input type="range" min="0" max="1" step="0.05" value="${s.sfx}" data-setting="sfx" id="set-sfx" />
      </div>
      <div class="setting-row">
        <div class="label">Mute everything<small>Silences both channels</small></div>
        <button class="switch ${s.muted ? 'on' : ''}" data-action="toggle-mute" aria-label="Toggle mute"></button>
      </div>
      <div class="setting-row">
        <div class="label">Reduce motion<small>Calms particles and transitions</small></div>
        <button class="switch ${s.reduceMotion ? 'on' : ''}" data-action="toggle-motion" aria-label="Toggle motion"></button>
      </div>
      <div class="rune-divider"></div>
      <div class="setting-row" style="border:none">
        <div class="label">Erase progression<small>Dust, unlocks, achievements and any run in progress</small></div>
        <button class="btn btn-ghost btn-sm" data-action="reset-progress">${this.resetArmed ? 'Confirm erase' : 'Reset'}</button>
      </div>
      <div class="dialog-actions">
        ${this.modalReturn === 'pause' ? `<button class="btn btn-ghost" data-action="back-to-pause">Back to pause</button>` : ''}
        <button class="btn btn-primary" data-action="close-modal">Done</button>
      </div>`;
    this.closeModal(true);
    this.openModal(html, 'settings');
    for (const key of ['music', 'sfx'] as const) {
      const input = document.getElementById(`set-${key}`) as HTMLInputElement | null;
      input?.addEventListener('input', () => {
        this.save.settings[key] = Number(input.value);
        this.applySettings();
        this.persist();
      });
    }
  }

  private toggleMute(): void {
    this.save.settings.muted = !this.save.settings.muted;
    this.applySettings();
    this.persist();
    this.toast(this.save.settings.muted ? 'Sound off' : 'Sound on', 'sound');
    if (this.modal === 'settings') this.openSettings();
    else if (this.screen === 'battle') this.refreshMuteButton();
  }

  private toggleMotion(): void {
    this.save.settings.reduceMotion = !this.save.settings.reduceMotion;
    this.applySettings();
    this.persist();
    if (this.modal === 'settings') this.openSettings();
  }

  private refreshMuteButton(): void {
    const buttons = Array.from(document.querySelectorAll('[data-action="toggle-mute"]'));
    for (const b of buttons) {
      const el = b as HTMLElement;
      if (el.classList.contains('switch')) {
        el.classList.toggle('on', this.save.settings.muted);
      } else {
        el.innerHTML = icon(this.save.settings.muted ? 'sound' : 'music');
      }
    }
  }

  private doReset(): void {
    this.save = clearSave();
    this.run = null;
    this.battle = null;
    this.resetArmed = false;
    this.applySettings();
    this.closeModal();
    this.toast('Progression erased. A cold start.', 'lock');
    this.showTitle();
  }

  private openForgeworks(): void {
    const cards = META_UNLOCKS.map((u) => {
      const owned = this.save.meta.unlocks.includes(u.id) || (u.id === 'hero_warden' && this.save.meta.heroes.includes('warden')) || (u.id === 'hero_wisp' && this.save.meta.heroes.includes('wisp'));
      const canBuy = !owned && this.save.meta.dust >= u.cost;
      return `<div class="card r-${u.cost > 200 ? 'epic' : 'rare'}" style="cursor:default">
        <div class="card-icon">${icon(u.icon)}</div>
        <div class="card-title">${esc(u.name)}</div>
        <div class="card-desc">${esc(u.desc)}</div>
        <div class="card-meta"><span class="tag">${owned ? 'unlocked' : `${u.cost} dust`}</span><span class="spacer"></span>
          ${owned ? `<span class="tag good">owned</span>` : `<button class="btn btn-sm ${canBuy ? 'btn-primary' : ''}" data-action="buy-unlock" data-id="${u.id}" ${canBuy ? '' : 'aria-disabled="true"'}>${icon('lock')} Unlock</button>`}
        </div>
      </div>`;
    }).join('');
    const html = `
      <div class="dialog-head">
        <div><h3>Forgeworks</h3><div class="hint">Permanent upgrades bought with dust, carried between lives.</div></div>
        <div style="display:flex;gap:8px;align-items:center">
          <div class="dust-chip">${icon('coin')} ${this.save.meta.dust}</div>
          <button class="icon-btn" data-action="close-modal">${icon('close')}</button>
        </div>
      </div>
      <div class="cards">${cards}</div>`;
    this.openModal(html, 'forgeworks');
  }

  private buyMetaUnlock(id: string): void {
    const def = META_UNLOCKS.find((u) => u.id === id);
    if (!def) return;
    if (this.save.meta.unlocks.includes(id)) return;
    if (this.save.meta.dust < def.cost) {
      this.toast('Not enough dust.', 'coin');
      return;
    }
    if (id === 'hero_warden' && this.save.meta.heroes.includes('warden')) return;
    if (id === 'hero_wisp' && this.save.meta.heroes.includes('wisp')) return;
    this.save.meta.dust -= def.cost;
    if (id === 'hero_warden') this.save.meta.heroes.push('warden');
    else if (id === 'hero_wisp') this.save.meta.heroes.push('wisp');
    else this.save.meta.unlocks.push(id);
    writeSave(this.save);
    audio.play('reward');
    this.toast(`${def.name} unlocked.`, 'star');
    this.closeModal();
    this.openForgeworks();
  }

  private openCodex(): void {
    const tabs: [string, string][] = [
      ['heroes', 'Keepers'],
      ['abilities', 'Abilities'],
      ['relics', 'Relics'],
      ['bestiary', 'Bestiary'],
      ['achievements', 'Achievements'],
      ['stats', 'Statistics'],
    ];
    let body = '';
    if (this.codexTab === 'heroes') {
      body = `<div class="codex-grid">${HEROES.map((h) => {
        const unlocked = this.save.meta.heroes.includes(h.id);
        return `<div class="codex-card ${unlocked ? '' : 'locked'}">
          <div class="card-icon">${icon(unlocked ? 'flame' : 'lock')}</div>
          <div><h5>${esc(h.name)}</h5><p>${esc(h.epithet)}<br/>${icon('heart')} ${h.hp} · ${icon('anvil')} ${h.forgeHp} · ${icon('core')} ${h.heatMax} heat<br/><b>${esc(h.passiveName)}:</b> ${esc(h.passiveDesc)}</p></div>
        </div>`;
      }).join('')}</div>`;
    } else if (this.codexTab === 'abilities') {
      body = `<div class="codex-grid">${ABILITIES.map((a) => {
        const seen = this.save.meta.abilitiesSeen.includes(a.id) || a.rarity === 'starter';
        return `<div class="codex-card ${seen ? '' : 'locked'}">
          <div class="card-icon r-${a.rarity}">${icon(seen ? a.icon : 'lock')}</div>
          <div><h5>${esc(a.name)} <span class="tag ember">${a.cost} heat</span></h5><p>${seen ? esc(a.text(1)) : 'Undiscovered.'}</p></div>
        </div>`;
      }).join('')}</div>`;
    } else if (this.codexTab === 'relics') {
      body = `<div class="codex-grid">${RELICS.map((r) => {
        const seen = this.save.meta.relicsSeen.includes(r.id);
        return `<div class="codex-card ${seen ? '' : 'locked'}">
          <div class="card-icon r-${r.rarity}">${icon(seen ? r.icon : 'lock')}</div>
          <div><h5>${esc(r.name)}</h5><p>${seen ? esc(r.desc) : 'Undiscovered relic.'}</p></div>
        </div>`;
      }).join('')}</div>`;
    } else if (this.codexTab === 'bestiary') {
      body = `<div class="codex-grid">${ENEMIES.map((e) => {
        const seen = this.save.meta.enemiesSeen.includes(e.id);
        return `<div class="codex-card ${seen ? '' : 'locked'}">
          <div class="card-icon r-${e.boss ? 'epic' : e.elite ? 'rare' : 'common'}">${seen ? `<img src="${enemyPortrait(e.id, 64)}" style="width:100%;height:100%;object-fit:cover;border-radius:8px" alt=""/>` : icon('lock')}</div>
          <div><h5>${seen ? esc(e.name) : 'Unknown'}</h5><p>${seen ? esc(e.desc) : 'Not yet met in the dark.'}${seen ? `<br/><span class="tag bad">${e.hp} hp · ${e.dmg} dmg</span>` : ''}</p></div>
        </div>`;
      }).join('')}</div>`;
    } else if (this.codexTab === 'achievements') {
      body = `<div class="codex-grid">${ACHIEVEMENTS.map((a) => {
        const got = !!this.save.achievements[a.id];
        return `<div class="ach-card ${got ? 'unlocked' : 'locked'}">
          <div class="card-icon r-${got ? 'starter' : 'common'}">${icon(got ? a.icon : 'lock')}</div>
          <div><h5 style="margin:0 0 2px;font-family:var(--serif);font-size:14px">${esc(a.name)}</h5><p style="margin:0;font-size:11.5px;color:var(--muted)">${a.secret && !got ? 'A hidden feat.' : esc(a.desc)}</p></div>
        </div>`;
      }).join('')}</div>`;
    } else {
      const s = this.save.stats;
      const rows: [string, string | number][] = [
        ['Runs started', s.runs],
        ['Victories', s.wins],
        ['Defeats', s.defeats],
        ['Total kills', s.kills],
        ['Kills by terrain', s.shoveKills],
        ['Elite rooms cleared', s.eliteClears],
        ['Best rooms in a run', s.bestRooms],
        ['Best kills in one turn', s.bestTurnKills],
        ['Most casts in one turn', s.bestCasts],
        ['Dust earned', s.totalDust],
        ['Dust in hand', this.save.meta.dust],
        ['Embers spent', s.totalEmbersSpent],
      ];
      body = `<div class="kv-list">${rows.map(([k, v]) => `<div class="kv"><span>${esc(k)}</span><b>${v}</b></div>`).join('')}</div>`;
    }

    const html = `
      <div class="dialog-head">
        <div><h3>Codex</h3><div class="hint">Everything the forge knows, and everything it remembers.</div></div>
        <button class="icon-btn" data-action="close-modal">${icon('close')}</button>
      </div>
      <div class="tabs">${tabs.map(([id, label]) => `<button class="tab ${this.codexTab === id ? 'active' : ''}" data-action="codex-tab" data-tab="${id}">${label}</button>`).join('')}</div>
      ${body}`;
    this.openModal(html, 'codex');
  }

  private openPause(): void {
    const run = this.run;
    const html = `
      <div class="dialog-head">
        <h3>Paused</h3>
        <button class="icon-btn" data-action="close-modal">${icon('close')}</button>
      </div>
      <div class="hint">The forge waits. Nothing moves until you do.</div>
      <div class="rune-divider"></div>
      <div class="kv-list">
        <div class="kv"><span>Act</span><b>${run ? esc(actName(run)) : '—'}</b></div>
        <div class="kv"><span>Room</span><b>${run ? `${Math.min(run.room + 1, 5)} / 5` : '—'}</b></div>
        <div class="kv"><span>Kills this run</span><b>${run?.runKills ?? 0}</b></div>
        <div class="kv"><span>Embers</span><b>${run?.embers ?? 0}</b></div>
      </div>
      <div class="dialog-actions">
        <button class="btn btn-ghost" data-action="abandon">Abandon run</button>
        <button class="btn btn-ghost" data-action="settings">Settings</button>
        <button class="btn btn-primary" data-action="resume">Resume</button>
      </div>`;
    this.closeModal(true);
    this.openModal(html, 'pause');
  }

  private openConfirmAbandon(): void {
    const html = `
      <div class="dialog-head">
        <h3>Abandon the run?</h3>
        <button class="icon-btn" data-action="close-modal">${icon('close')}</button>
      </div>
      <div class="hint">You keep half of the dust earned so far, but the lantern goes out and the run is lost.</div>
      <div class="dialog-actions">
        <button class="btn btn-ghost" data-action="close-modal">Keep fighting</button>
        <button class="btn btn-primary" data-action="confirm-abandon">${icon('close')} Abandon</button>
      </div>`;
    this.closeModal(true);
    this.openModal(html, 'confirm');
  }

  private abandonRun(): void {
    const run = this.run;
    if (run) {
      const dust = Math.max(0, Math.round(run.dustEarned / 2));
      this.save.meta.dust += dust;
      this.save.stats.totalDust += dust;
      this.toast(`Abandoned. +${dust} dust salvaged.`, 'coin');
    }
    this.run = null;
    this.battle = null;
    this.save.run = null;
    this.closeModal();
    this.renderer.setBattle(null);
    this.persist();
    this.showTitle();
  }

  private continueRun(): void {
    if (!this.save.run) return this.showHeroes();
    this.run = this.save.run;
    const status = this.run.status;
    if (status === 'battle') {
      this.beginBattle();
    } else if (status === 'reward') {
      this.showReward();
    } else if (status === 'forge') {
      this.showShop();
    } else if (status === 'rest') {
      this.showRest();
    } else if (status === 'treasure') {
      this.showTreasure();
    } else {
      this.showRoute();
    }
  }
}

// ── boot ─────────────────────────────────────────────────────────────────────
/** Constructs the game against the current document. Also used by UI tests. */
export function createGame(): Game {
  const game = new Game();
  (window as unknown as { anvilight: Game }).anvilight = game;
  return game;
}

function boot(): void {
  if (!document.getElementById('app')) return;
  try {
    createGame();
  } catch (err) {
    const app = document.getElementById('app');
    if (app) {
      app.innerHTML = `<div style="display:grid;place-items:center;height:100%;padding:32px;text-align:center;color:#f2e9d8;font-family:Georgia,serif">
        <div><h1 style="letter-spacing:.2em">ANVILIGHT</h1><p style="color:#8e97ad;max-width:520px;line-height:1.6">
        This game needs a modern browser with Canvas 2D and WebAudio. Something refused to start, so the forge stayed cold.</p>
        <pre style="color:#636d84;font-size:11px;white-space:pre-wrap;max-width:620px">${esc(String(err))}</pre></div></div>`;
    }
    throw err;
  }
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', boot);
} else {
  boot();
}

export { Game };
