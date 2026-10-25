import Phaser from 'phaser';
import { saveManager, soundEngine } from '../core/appContext';
import {
  BENCH_SIZE,
  CircuitEngine,
  CircuitModifiers,
  DIR_DELTA,
  GRID_COLS,
  GRID_ROWS,
} from '../core/circuitEngine';
import {
  CombatEngine,
  CORE_POS,
  getRerollCost,
  Point2D,
  RIFT_POS,
  rollShopOffers,
  VisualHitEvent,
} from '../core/combatEngine';
import {
  ENEMIES,
  GAME_MODES,
  MODULES,
  RELICS,
  WARDENS,
  WEATHERS,
} from '../core/content';
import {
  ActiveRunSave,
  Direction,
  GameModeId,
  PlacedModule,
  RelicId,
  ShopOffer,
  TileMutation,
  WardenId,
  WeatherId,
} from '../core/types';
import {
  createOrnateButton,
  drawOrnatePanel,
  FONT_BODY,
  FONT_TITLE,
  showAchievementToast,
  TooltipOverlay,
} from '../ui/uiFactory';

type SlotRef = { zone: 'grid'; col: number; row: number } | { zone: 'bench'; idx: number };

interface EnemyVisual {
  sprite: Phaser.GameObjects.Sprite;
  hpBg: Phaser.GameObjects.Graphics;
  hpFill: Phaser.GameObjects.Graphics;
  statusTxt: Phaser.GameObjects.Text;
}

interface LootParticle {
  sprite: Phaser.GameObjects.Image;
  vx: number;
  vy: number;
  gold: number;
  starlight: number;
  age: number;
  active: boolean;
}

interface FloatingNum {
  txt: Phaser.GameObjects.Text;
  vy: number;
  life: number;
  maxLife: number;
  active: boolean;
}

const GRID_ORIGIN_X = 206;
const GRID_ORIGIN_Y = 160;
const CELL_STEP = 74;
const BENCH_ORIGIN_X = 206;
const BENCH_Y = 492;

export class GameScene extends Phaser.Scene {
  private wardenId: WardenId = 'lyra';
  private modeId: GameModeId = 'campaign';
  private resumeSaved = false;

  private circuit!: CircuitEngine;
  private combat!: CombatEngine;

  private wave = 1;
  private inCombat = false;
  private isPaused = false;
  private runEnded = false;
  private endlessContinued = false;

  private gold = 60;
  private starlightEarnedThisRun = 0;
  private weatherId: WeatherId = 'clear';
  private relics: RelicId[] = [];
  private shopOffers: ShopOffer[] = [];
  private rerollCount = 0;
  private flawlessStreak = 0;
  private tookDamageThisWave = false;
  private peakMultThisRun = 1;
  private gameSpeed: 1 | 1.5 | 2 = 1;

  // Timers
  private emitterTimer = 0;
  private emitterInterval = 1.55;
  private manualClickCd = 0;
  private abilityCds: [number, number, number] = [0, 0, 0];
  private abilityMaxCds: [number, number, number] = [9, 15, 26];
  private pyroUltTimer = 0;
  private hasteUltTimer = 0;

  // Selection & Dragging
  private selectedSlot: SlotRef | null = null;
  private dragSource: SlotRef | null = null;
  private dragGhost!: Phaser.GameObjects.Image;

  // Visual Layers
  private bgImage!: Phaser.GameObjects.Image;
  private coreSprite!: Phaser.GameObjects.Image;
  private coreShieldRing!: Phaser.GameObjects.Graphics;
  private riftSprite!: Phaser.GameObjects.Image;
  private targetReticle!: Phaser.GameObjects.Image;

  private previewGfx!: Phaser.GameObjects.Graphics;
  private gridContainer!: Phaser.GameObjects.Container;
  private pulseContainer!: Phaser.GameObjects.Container;
  private enemyContainer!: Phaser.GameObjects.Container;
  private hudContainer!: Phaser.GameObjects.Container;
  private dockContainer!: Phaser.GameObjects.Container;
  private modalContainer!: Phaser.GameObjects.Container;
  private tooltip!: TooltipOverlay;

  private enemyVisuals = new Map<string, EnemyVisual>();
  private pulseSprites = new Map<
    string,
    { orb: Phaser.GameObjects.Image; badge: Phaser.GameObjects.Text }
  >();
  private relicIcons = new Map<RelicId, Phaser.GameObjects.Image>();

  // Object Pools
  private floatPool: FloatingNum[] = [];
  private lootPool: LootParticle[] = [];

  // Dynamic HUD Texts
  private hpText!: Phaser.GameObjects.Text;
  private hpBarFill!: Phaser.GameObjects.Graphics;
  private waveText!: Phaser.GameObjects.Text;
  private weatherText!: Phaser.GameObjects.Text;
  private weatherIcon!: Phaser.GameObjects.Image;
  private goldText!: Phaser.GameObjects.Text;
  private starText!: Phaser.GameObjects.Text;
  private multText!: Phaser.GameObjects.Text;
  private waveBtnLabel!: Phaser.GameObjects.Text;
  private hintBannerText!: Phaser.GameObjects.Text;
  private abilityCdTexts: Phaser.GameObjects.Text[] = [];
  private bossBarContainer!: Phaser.GameObjects.Container;
  private bossBarFill!: Phaser.GameObjects.Graphics;
  private bossNameText!: Phaser.GameObjects.Text;

  constructor() {
    super('GameScene');
  }

  public init(data?: { wardenId?: WardenId; modeId?: GameModeId; resumeSaved?: boolean }): void {
    this.resumeSaved = Boolean(data?.resumeSaved);
    this.wardenId = data?.wardenId ?? 'lyra';
    this.modeId = data?.modeId ?? 'campaign';
    this.inCombat = false;
    this.isPaused = false;
    this.runEnded = false;
    this.endlessContinued = false;
    this.selectedSlot = null;
    this.dragSource = null;
    this.enemyVisuals.clear();
    this.pulseSprites.clear();
    this.relicIcons.clear();
    this.floatPool = [];
    this.lootPool = [];
  }

  public create(): void {
    const { width, height } = this.scale;
    this.input.mouse?.disableContextMenu();

    const saved = saveManager.getData();
    this.gameSpeed = saved.settings.defaultSpeed;

    // Initialize Engines & Run State
    this.setupRunState();

    // 1. Battlefield Painted Background
    this.bgImage = this.add
      .image(width / 2, height / 2, 'battle_sanctum_bg')
      .setDisplaySize(width, height);
    this.bgImage.setTint(WEATHERS[this.weatherId].tintHex);

    // Subtle Ritual Chamber Label & Causeway Markers
    this.previewGfx = this.add.graphics().setDepth(15);

    // Sunspire Core & Void Rift Structures
    this.coreShieldRing = this.add.graphics().setDepth(22);
    this.coreSprite = this.add
      .image(CORE_POS.x, CORE_POS.y, 'bastion_core')
      .setDisplaySize(86, 86)
      .setDepth(24);
    this.tweens.add({
      targets: this.coreSprite,
      angle: 360,
      duration: 28000,
      repeat: -1,
    });

    this.riftSprite = this.add
      .image(RIFT_POS.x, RIFT_POS.y, 'void_rift')
      .setDisplaySize(96, 96)
      .setDepth(20);
    this.tweens.add({
      targets: this.riftSprite,
      scaleX: 0.82,
      scaleY: 0.9,
      duration: 1400,
      yoyo: true,
      repeat: -1,
      ease: 'Sine.easeInOut',
    });

    this.targetReticle = this.add
      .image(0, 0, 'reticle_target')
      .setDisplaySize(54, 54)
      .setDepth(42)
      .setVisible(false);
    this.tweens.add({
      targets: this.targetReticle,
      angle: 360,
      duration: 4000,
      repeat: -1,
    });

    // Containers
    this.gridContainer = this.add.container(0, 0).setDepth(25);
    this.pulseContainer = this.add.container(0, 0).setDepth(32);
    this.enemyContainer = this.add.container(0, 0).setDepth(30);
    this.hudContainer = this.add.container(0, 0).setDepth(100);
    this.dockContainer = this.add.container(0, 0).setDepth(100);
    this.modalContainer = this.add.container(0, 0).setDepth(2000);
    this.tooltip = new TooltipOverlay(this);

    this.dragGhost = this.add
      .image(0, 0, 'mod_amp_lens')
      .setDisplaySize(60, 60)
      .setAlpha(0.85)
      .setDepth(1500)
      .setVisible(false);

    // Pre-allocate Object Pools (zero runtime text/loot GC churn)
    for (let i = 0; i < 42; i++) {
      const txt = this.add
        .text(0, 0, '', {
          fontFamily: FONT_TITLE,
          fontSize: '15px',
          fontStyle: 'bold',
          color: '#ffffff',
          stroke: '#060913',
          strokeThickness: 3,
        })
        .setOrigin(0.5)
        .setDepth(90)
        .setVisible(false);
      this.floatPool.push({ txt, vy: -36, life: 0, maxLife: 0.75, active: false });
    }

    for (let i = 0; i < 48; i++) {
      const sprite = this.add
        .image(0, 0, 'loot_gold')
        .setDisplaySize(22, 22)
        .setDepth(60)
        .setVisible(false);
      this.lootPool.push({ sprite, vx: 0, vy: 0, gold: 0, starlight: 0, age: 0, active: false });
    }

    // Build UI & Interactive Battlefield
    this.buildTopHud();
    this.buildBossBar();
    this.rebuildGridVisuals();
    this.rebuildBottomDock();
    this.setupBattlefieldInput();
    this.setupKeyboardShortcuts();

    soundEngine.setMusicMood('menu');
    this.cameras.main.fadeIn(220, 5, 8, 18);
  }

  private setupRunState(): void {
    const saved = saveManager.getData();
    const activeSave = this.resumeSaved ? saved.activeRun : null;

    if (activeSave) {
      this.wardenId = activeSave.wardenId;
      this.modeId = activeSave.modeId;
      this.wave = activeSave.wave;
      this.gold = activeSave.gold;
      this.weatherId = activeSave.weatherId;
      this.relics = [...activeSave.relics];
      this.rerollCount = activeSave.rerollCount;

      this.circuit = new CircuitEngine(0, 0, false);
      for (const g of activeSave.grid) {
        const cell = this.circuit.getCell(g.col, g.row);
        if (cell) {
          cell.mutation = g.mutation;
          cell.module = g.module
            ? this.circuit.createModule(g.module.defId, g.module.tier, g.module.rotation)
            : null;
        }
      }
      activeSave.bench.forEach((b, idx) => {
        this.circuit.bench[idx] = b ? this.circuit.createModule(b.defId, b.tier, b.rotation) : null;
      });

      this.combat = new CombatEngine();
      this.combat.maxCoreHp = activeSave.maxCoreHp;
      this.combat.coreHp = activeSave.coreHp;
      this.combat.coreShield = activeSave.coreShield;
    } else {
      const warden = WARDENS[this.wardenId];
      const unlockedCorners = saveManager.getSkillRank('econ_unlock_tiles');
      const extraRadiant = saveManager.getSkillRank('econ_radiant');
      const isMirrorTrial = this.modeId === 'challenge_mirror';

      this.circuit = new CircuitEngine(unlockedCorners, extraRadiant, isMirrorTrial);
      this.combat = new CombatEngine();

      const bonusHp = saveManager.getSkillRank('bastion_hull') * 15;
      if (this.modeId === 'challenge_fragile') {
        this.combat.maxCoreHp = 25;
        this.combat.coreHp = 25;
      } else {
        this.combat.maxCoreHp = 100 + bonusHp;
        this.combat.coreHp = this.combat.maxCoreHp;
      }
      this.combat.coreShield = saveManager.getSkillRank('bastion_shield') * 8;

      this.wave = 1;
      this.weatherId = 'clear';
      this.relics = [];
      this.rerollCount = 0;

      const stipend = saveManager.getSkillRank('econ_stipend') * 12;
      this.gold =
        45 +
        warden.startingGoldBonus +
        stipend +
        (this.modeId === 'challenge_fragile' ? 55 : 0);

      // Place Warden starting modules
      for (const sm of warden.startingModules) {
        const cell = this.circuit.getCell(sm.col, sm.row);
        if (cell && cell.mutation !== 'locked') {
          cell.module = this.circuit.createModule(sm.defId, 1, sm.rotation ?? 0);
          saveManager.discoverModule(sm.defId);
        }
      }
      // Give every player a spare Bifrost Splitter on Bench slot 0 so they can try splitting immediately in <15s!
      this.circuit.bench[0] = this.circuit.createModule('splitter', 1, 0);

      if (isMirrorTrial) {
        this.circuit.bench[1] = this.circuit.createModule('mirror', 1, 0);
        this.circuit.bench[2] = this.circuit.createModule('mirror', 1, 1);
        this.circuit.bench[3] = this.circuit.createModule('mirror', 1, 2);
      }
      if (this.modeId === 'challenge_fragile') {
        this.gainRelic('relic_glass_lens');
      }

      const ach = saveManager.mutateStats((st) => {
        st.runsStarted += 1;
      });
      ach.forEach((a, i) => this.time.delayedCall(300, () => showAchievementToast(this, a, i)));
    }

    const rarityBoost = saveManager.getSkillRank('econ_rarity') * 0.12;
    this.shopOffers = rollShopOffers(this.wave, this.relics, this.getMaxRelics(), rarityBoost);
    this.recomputeEmitterInterval();
    this.persistActiveRun();
  }

  private getMaxRelics(): number {
    return 6 + saveManager.getSkillRank('bastion_relic_slot');
  }

  private getCircuitModifiers(): CircuitModifiers {
    const critRank = saveManager.getSkillRank('solar_crit') * 0.04;
    const hopsRank = saveManager.getSkillRank('solar_hops');
    const dualRank = saveManager.getSkillRank('solar_dual') > 0;
    return {
      wardenId: this.wardenId,
      weatherId: this.weatherId,
      relics: this.relics,
      bonusHops: hopsRank,
      baseCritChance:
        0.08 +
        critRank +
        (this.wardenId === 'kael' ? 0.15 : 0) +
        (this.relics.includes('relic_monocle') ? 0.18 : 0),
      pulseSpeedScale: this.hasteUltTimer > 0 ? 1.85 : 1.0,
      dualTopEmitter: dualRank,
      mirrorChallengeBonus: this.modeId === 'challenge_mirror',
    };
  }

  private recomputeEmitterInterval(): void {
    const cadenceRank = saveManager.getSkillRank('solar_cadence') * 0.06;
    const wardenScale = WARDENS[this.wardenId].emitterIntervalScale;
    const pendulumScale = this.relics.includes('relic_pendulum') ? 0.78 : 1.0;
    const solarScale = this.weatherId === 'solar_flare' ? 0.88 : 1.0;
    const swarmScale = this.modeId === 'challenge_swarm' ? 0.8 : 1.0;
    const ultScale = this.hasteUltTimer > 0 ? 0.5 : 1.0;
    this.emitterInterval = Math.max(
      0.38,
      (1.55 / (1 + cadenceRank)) * wardenScale * pendulumScale * solarScale * swarmScale * ultScale
    );
  }

  private persistActiveRun(): void {
    if (this.runEnded) {
      saveManager.saveActiveRun(null);
      return;
    }
    const gridDump: ActiveRunSave['grid'] = [];
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        const cell = this.circuit.cells[r][c];
        gridDump.push({
          col: c,
          row: r,
          mutation: cell.mutation,
          module: cell.module
            ? { defId: cell.module.defId, tier: cell.module.tier, rotation: cell.module.rotation }
            : null,
        });
      }
    }
    saveManager.saveActiveRun({
      wardenId: this.wardenId,
      modeId: this.modeId,
      wave: this.wave,
      gold: this.gold,
      coreHp: this.combat.coreHp,
      maxCoreHp: this.combat.maxCoreHp,
      coreShield: this.combat.coreShield,
      weatherId: this.weatherId,
      relics: [...this.relics],
      grid: gridDump,
      bench: this.circuit.bench.map((b) =>
        b ? { defId: b.defId, tier: b.tier, rotation: b.rotation } : null
      ),
      rerollCount: this.rerollCount,
    });
  }

  // ==========================================================================
  // TOP HUD & BOSS BAR
  // ==========================================================================
  private buildTopHud(): void {
    this.hudContainer.removeAll(true);
    const { width } = this.scale;

    const topPanel = drawOrnatePanel(this, 8, 6, width - 16, 48, 0xc89b45, 0.93);
    this.hudContainer.add(topPanel);

    // Warden Mini Portrait
    const port = this.add.image(34, 30, WARDENS[this.wardenId].portraitKey).setDisplaySize(38, 38);
    const portBorder = this.add.graphics();
    portBorder.lineStyle(2, 0xffd166, 1);
    portBorder.strokeRect(15, 11, 38, 38);
    this.hudContainer.add([port, portBorder]);

    // Core HP Bar
    this.hpBarFill = this.add.graphics();
    const hpFrame = this.add.graphics();
    hpFrame.lineStyle(1.5, 0x9ec5ff, 0.9);
    hpFrame.strokeRoundedRect(62, 13, 166, 16, 5);

    this.hpText = this.add.text(66, 14, '', {
      fontFamily: FONT_BODY,
      fontSize: '11px',
      fontStyle: 'bold',
      color: '#ffffff',
    });

    const targetW = GAME_MODES[this.modeId].targetWaves;
    this.waveText = this.add.text(62, 32, `WAVE ${this.wave}${targetW ? ` / ${targetW}` : ''}`, {
      fontFamily: FONT_TITLE,
      fontSize: '13px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });

    // Weather Badge
    const wDef = WEATHERS[this.weatherId];
    this.weatherIcon = this.add.image(252, 30, wDef.iconKey).setDisplaySize(24, 24);
    this.weatherText = this.add
      .text(268, 30, wDef.name, {
        fontFamily: FONT_BODY,
        fontSize: '12px',
        fontStyle: 'bold',
        color: '#88f7ff',
      })
      .setOrigin(0, 0.5);

    const weatherZone = this.add
      .zone(308, 30, 140, 38)
      .setInteractive({ useHandCursor: true });
    weatherZone.on('pointerover', () => {
      const cw = WEATHERS[this.weatherId];
      this.tooltip.show(240, 62, `Anomaly: ${cw.name}`, 'Celestial Forecast', cw.description);
    });
    weatherZone.on('pointerout', () => this.tooltip.hide());

    this.hudContainer.add([
      this.hpBarFill,
      hpFrame,
      this.hpText,
      this.waveText,
      this.weatherIcon,
      this.weatherText,
      weatherZone,
    ]);

    // Relic Slots Bar (x: 416..724)
    const maxR = this.getMaxRelics();
    for (let i = 0; i < maxR; i++) {
      const rx = 418 + i * 42;
      const ry = 30;
      const slotGfx = this.add.graphics();
      slotGfx.fillStyle(0x131d3b, 0.9);
      slotGfx.fillCircle(rx, ry, 17);
      slotGfx.lineStyle(1.5, 0x6482c4, 0.8);
      slotGfx.strokeCircle(rx, ry, 17);
      this.hudContainer.add(slotGfx);

      const relId = this.relics[i];
      if (relId) {
        const rDef = RELICS[relId];
        const img = this.add
          .image(rx, ry, rDef.spriteKey)
          .setDisplaySize(32, 32)
          .setInteractive({ useHandCursor: true });
        img.on('pointerover', () => {
          this.tooltip.show(rx - 100, 62, rDef.name, `${rDef.rarity.toUpperCase()} RELIC`, rDef.description);
        });
        img.on('pointerout', () => this.tooltip.hide());
        this.hudContainer.add(img);
        this.relicIcons.set(relId, img);
      }
    }

    // Gold, Starlight & Peak Multiplier Readouts
    this.hudContainer.add(this.add.image(742, 30, 'icon_gold').setDisplaySize(22, 22));
    this.goldText = this.add
      .text(758, 30, `${this.gold}g`, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        fontStyle: 'bold',
        color: '#ffd86b',
      })
      .setOrigin(0, 0.5);

    this.hudContainer.add(this.add.image(838, 30, 'icon_starlight').setDisplaySize(22, 22));
    this.starText = this.add
      .text(854, 30, `+${this.starlightEarnedThisRun}`, {
        fontFamily: FONT_TITLE,
        fontSize: '14px',
        fontStyle: 'bold',
        color: '#7ef9ff',
      })
      .setOrigin(0, 0.5);

    this.multText = this.add
      .text(922, 30, `PEAK x${this.peakMultThisRun.toFixed(1)}`, {
        fontFamily: FONT_TITLE,
        fontSize: '13.5px',
        fontStyle: 'bold',
        color: '#ff9ef5',
      })
      .setOrigin(0, 0.5);

    this.hudContainer.add([this.goldText, this.starText, this.multText]);

    // Speed Toggle, Mute Toggle, Pause Button
    const speedBtn = createOrnateButton(this, {
      x: 1094,
      y: 30,
      width: 92,
      height: 34,
      fontSize: 13,
      primary: this.gameSpeed > 1,
      label: `${this.gameSpeed}x SPD`,
      onClick: () => {
        this.gameSpeed = this.gameSpeed === 1 ? 1.5 : this.gameSpeed === 1.5 ? 2 : 1;
        this.buildTopHud();
      },
    });
    this.hudContainer.add(speedBtn);

    const isMuted = saveManager.getData().settings.muted;
    const muteIcon = this.add
      .image(1168, 30, isMuted ? 'icon_sound_off' : 'icon_sound_on')
      .setDisplaySize(26, 26)
      .setInteractive({ useHandCursor: true });
    muteIcon.on('pointerdown', () => {
      const next = !saveManager.getData().settings.muted;
      saveManager.updateSettings({ muted: next });
      soundEngine.updateSettings(saveManager.getData().settings);
      muteIcon.setTexture(next ? 'icon_sound_off' : 'icon_sound_on').setDisplaySize(26, 26);
    });
    this.hudContainer.add(muteIcon);

    const pauseBtn = createOrnateButton(this, {
      x: 1228,
      y: 30,
      width: 72,
      height: 34,
      fontSize: 12,
      label: 'MENU',
      onClick: () => this.togglePauseModal(),
    });
    this.hudContainer.add(pauseBtn);

    // Contextual Helper Banner above Circuit Grid
    this.hintBannerText = this.add
      .text(
        354,
        92,
        'Drag or Click Modules to route & merge (I+I -> II -> III)  •  Right-Click or [R] to Rotate Prisms',
        {
          fontFamily: FONT_BODY,
          fontSize: '12px',
          color: '#dce8ff',
          backgroundColor: 'rgba(9,14,30,0.78)',
          padding: { x: 10, y: 4 },
        }
      )
      .setOrigin(0.5);
    this.hudContainer.add(this.hintBannerText);

    this.refreshHudValues();
  }

  private buildBossBar(): void {
    this.bossBarContainer = this.add.container(912, 82).setDepth(95).setVisible(false);
    const bg = this.add.graphics();
    bg.fillStyle(0x140826, 0.92);
    bg.fillRoundedRect(-210, -20, 420, 40, 8);
    bg.lineStyle(2, 0xffd166, 0.95);
    bg.strokeRoundedRect(-210, -20, 420, 40, 8);

    this.bossBarFill = this.add.graphics();
    this.bossNameText = this.add
      .text(0, -10, '', {
        fontFamily: FONT_TITLE,
        fontSize: '12.5px',
        fontStyle: 'bold',
        color: '#ffe79e',
      })
      .setOrigin(0.5);

    this.bossBarContainer.add([bg, this.bossBarFill, this.bossNameText]);
  }

  private refreshHudValues(): void {
    if (!this.hpBarFill) return;
    this.hpBarFill.clear();
    const ratio = Math.max(0, Math.min(1, this.combat.coreHp / Math.max(1, this.combat.maxCoreHp)));
    const hpColor = ratio > 0.5 ? 0x38d399 : ratio > 0.25 ? 0xffbe0b : 0xff4d6d;
    this.hpBarFill.fillStyle(0x10172b, 1);
    this.hpBarFill.fillRoundedRect(63, 14, 164, 14, 4);
    this.hpBarFill.fillStyle(hpColor, 1);
    this.hpBarFill.fillRoundedRect(63, 14, Math.max(2, 164 * ratio), 14, 4);

    const shdStr = this.combat.coreShield > 0 ? ` +${Math.round(this.combat.coreShield)} SHD` : '';
    this.hpText.setText(`${Math.ceil(this.combat.coreHp)}/${this.combat.maxCoreHp} HP${shdStr}`);

    const targetW = GAME_MODES[this.modeId].targetWaves;
    this.waveText.setText(
      `WAVE ${this.wave}${targetW && !this.endlessContinued ? ` / ${targetW}` : ' (ENDLESS)'}`
    );

    const wDef = WEATHERS[this.weatherId];
    this.weatherIcon.setTexture(wDef.iconKey).setDisplaySize(24, 24);
    this.weatherText.setText(wDef.name);

    this.goldText.setText(`${this.gold}g`);
    this.starText.setText(`+${this.starlightEarnedThisRun}`);
    this.multText.setText(`PEAK x${this.peakMultThisRun.toFixed(1)}`);

    // Core shield ring visual
    this.coreShieldRing.clear();
    if (this.combat.coreShield > 0) {
      this.coreShieldRing.lineStyle(3, 0x62ecff, 0.85);
      this.coreShieldRing.strokeCircle(CORE_POS.x, CORE_POS.y, 48);
    }
  }

  // ==========================================================================
  // RUNIC CIRCUIT GRID & SPARE BENCH VISUALS + DRAG/CLICK/ROTATE
  // ==========================================================================
  private cellToScreen(col: number, row: number): Point2D {
    return {
      x: GRID_ORIGIN_X + col * CELL_STEP,
      y: GRID_ORIGIN_Y + row * CELL_STEP,
    };
  }

  private benchToScreen(idx: number): Point2D {
    return {
      x: BENCH_ORIGIN_X + idx * CELL_STEP,
      y: BENCH_Y,
    };
  }

  private rebuildGridVisuals(): void {
    this.gridContainer.removeAll(true);
    const mods = this.getCircuitModifiers();

    // Draw Emitters on Left Edge
    const emitters = this.circuit.getEmitterRows(mods);
    for (const em of emitters) {
      const pos = this.cellToScreen(-0.85, em.row);
      const emImg = this.add
        .image(pos.x, pos.y, 'emitter_solar')
        .setDisplaySize(58, 58)
        .setInteractive({ useHandCursor: true });
      emImg.on('pointerover', () => {
        this.tooltip.show(
          pos.x,
          pos.y - 24,
          'Solar Pulse Emitter',
          `Interval: ${this.emitterInterval.toFixed(2)}s  •  Output: ${Math.round(em.powerScale * 100)}%`,
          'Fires kinetic Aether Pulses East into the Runic Circuit. Click during combat to trigger manual Overpulse!'
        );
      });
      emImg.on('pointerout', () => this.tooltip.hide());
      emImg.on('pointerdown', () => {
        if (this.inCombat) this.tryCastAbility(0);
      });
      this.gridContainer.add(emImg);
    }

    // Draw 5x4 Grid Cells
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        const cell = this.circuit.cells[r][c];
        const pos = this.cellToScreen(c, r);
        const isSel =
          this.selectedSlot?.zone === 'grid' &&
          this.selectedSlot.col === c &&
          this.selectedSlot.row === r;

        const tileImg = this.add
          .image(pos.x, pos.y, isSel ? 'tile_hover' : `tile_${cell.mutation}`)
          .setDisplaySize(70, 70);
        this.gridContainer.add(tileImg);

        if (cell.module) {
          this.renderPlacedModuleAt(cell.module, pos.x, pos.y);
        }

        const hit = this.add
          .zone(pos.x, pos.y, 70, 70)
          .setInteractive({ useHandCursor: true, draggable: Boolean(cell.module) });

        this.bindSlotInteractions(hit, { zone: 'grid', col: c, row: r });
        this.gridContainer.add(hit);
      }
    }

    // Bench Label & 5 Bench Slots
    const benchLabel = this.add
      .text(BENCH_ORIGIN_X - 34, BENCH_Y - 48, 'SPARE MODULE BENCH (Drag onto Grid or Merge Identical Tiers)', {
        fontFamily: FONT_BODY,
        fontSize: '11px',
        fontStyle: 'bold',
        color: '#a8c5ff',
      });
    this.gridContainer.add(benchLabel);

    for (let i = 0; i < BENCH_SIZE; i++) {
      const pos = this.benchToScreen(i);
      const isSel = this.selectedSlot?.zone === 'bench' && this.selectedSlot.idx === i;
      const tileImg = this.add
        .image(pos.x, pos.y, isSel ? 'tile_hover' : 'tile_normal')
        .setDisplaySize(66, 66);
      this.gridContainer.add(tileImg);

      const mod = this.circuit.bench[i];
      if (mod) {
        this.renderPlacedModuleAt(mod, pos.x, pos.y, 54);
      }

      const hit = this.add
        .zone(pos.x, pos.y, 66, 66)
        .setInteractive({ useHandCursor: true, draggable: Boolean(mod) });
      this.bindSlotInteractions(hit, { zone: 'bench', idx: i });
      this.gridContainer.add(hit);
    }

    this.drawPreviewBeams();
  }

  private renderPlacedModuleAt(mod: PlacedModule, x: number, y: number, size = 58): void {
    const def = MODULES[mod.defId];

    // Tier II / III Aura Halo
    if (mod.tier >= 2) {
      const aura = this.add.graphics();
      aura.lineStyle(
        mod.tier === 3 ? 2.8 : 2,
        mod.tier === 3 ? 0xffdf73 : 0x6ef2ff,
        0.9
      );
      aura.strokeCircle(x, y, size * 0.49);
      this.gridContainer.add(aura);
    }

    const sp = this.add
      .image(x, y, def.spriteKey)
      .setDisplaySize(size, size)
      .setAngle(mod.rotation * 90);
    this.gridContainer.add(sp);

    const tierStr = mod.tier === 1 ? 'I' : mod.tier === 2 ? 'II' : 'III';
    const badge = this.add
      .text(x + size * 0.34, y + size * 0.32, tierStr, {
        fontFamily: FONT_TITLE,
        fontSize: '11px',
        fontStyle: 'bold',
        color: mod.tier === 3 ? '#ffe66d' : mod.tier === 2 ? '#7ef9ff' : '#dce8ff',
        stroke: '#050814',
        strokeThickness: 3,
      })
      .setOrigin(0.5);
    this.gridContainer.add(badge);
  }

  private getModuleAtSlot(slot: SlotRef): PlacedModule | null {
    if (slot.zone === 'bench') return this.circuit.bench[slot.idx] ?? null;
    return this.circuit.getCell(slot.col, slot.row)?.module ?? null;
  }

  private bindSlotInteractions(zoneObj: Phaser.GameObjects.Zone, slot: SlotRef): void {
    zoneObj.on('pointerover', () => {
      const mod = this.getModuleAtSlot(slot);
      if (mod) {
        const def = MODULES[mod.defId];
        const tIdx = mod.tier - 1;
        const statLine =
          def.category === 'turret'
            ? `Base Dmg: ${def.baseDamage[tIdx]}  •  Pass-Through: ${Math.round(def.passThroughRatio[tIdx] * 100)}%`
            : `Multiplier: x${def.multFactor[tIdx].toFixed(2)}${def.rotatable ? '  •  Rotatable [R / Right-Click]' : ''}`;
        this.tooltip.show(
          zoneObj.x - 90,
          zoneObj.y - 38,
          `${def.name} (Tier ${mod.tier})`,
          statLine,
          `${def.shortDesc}\nTotal Damage Dealt: ${Math.round(mod.totalDamageDealt)}`
        );
      } else if (slot.zone === 'grid') {
        const cell = this.circuit.getCell(slot.col, slot.row);
        if (cell && cell.mutation !== 'normal') {
          const mutTitles: Record<TileMutation, [string, string]> = {
            normal: ['Sanctum Tile', 'Standard Runic Conduit.'],
            radiant: ['Radiant Leyline', '+35% Power Multiplier to Pulses passing through this tile.'],
            haste: ['Chrono Leyline', 'Pulses cross 50% faster and gain +15% Critical Chance.'],
            prismatic: ['Prismatic Nexus', 'Infuses passing Pulses with Pyro, Cryo, and Volt simultaneously!'],
            locked: ['Crystal-Sealed Tile', 'Click to excavate and unlock this tile for 28 Gold.'],
          };
          const [t, desc] = mutTitles[cell.mutation];
          this.tooltip.show(zoneObj.x - 90, zoneObj.y - 38, t, 'Grid Leyline', desc);
        }
      }
    });

    zoneObj.on('pointerout', () => this.tooltip.hide());

    zoneObj.on('pointerdown', (ptr: Phaser.Input.Pointer) => {
      soundEngine.unlock();

      // Right-click rotates rotatable modules!
      if (ptr.rightButtonDown()) {
        if (this.circuit.rotateAt(slot)) {
          soundEngine.playRotate();
          this.rebuildGridVisuals();
          this.persistActiveRun();
        }
        return;
      }

      // Unlock Crystal-Locked tile
      if (slot.zone === 'grid') {
        const cell = this.circuit.getCell(slot.col, slot.row);
        if (cell && cell.mutation === 'locked') {
          if (this.gold >= 28) {
            this.gold -= 28;
            this.circuit.unlockTile(slot.col, slot.row);
            soundEngine.playPlace();
            this.spawnFloatingText(zoneObj.x, zoneObj.y, 'TILE UNLOCKED!', '#7ef9ff');
            this.rebuildGridVisuals();
            this.rebuildBottomDock();
            this.refreshHudValues();
            this.persistActiveRun();
          } else {
            this.spawnFloatingText(zoneObj.x, zoneObj.y, 'Need 28g', '#ff758f');
          }
          return;
        }
      }

      // Click-to-Select / Click-to-Move/Merge
      const clickedMod = this.getModuleAtSlot(slot);
      if (!this.selectedSlot) {
        if (clickedMod) {
          this.selectedSlot = slot;
          soundEngine.playClick();
          this.rebuildGridVisuals();
        }
      } else {
        const same =
          this.selectedSlot.zone === slot.zone &&
          ((slot.zone === 'bench' && this.selectedSlot.zone === 'bench' && slot.idx === this.selectedSlot.idx) ||
            (slot.zone === 'grid' &&
              this.selectedSlot.zone === 'grid' &&
              slot.col === this.selectedSlot.col &&
              slot.row === this.selectedSlot.row));

        if (same) {
          // Clicking same selected rotatable module rotates it for single-button convenience!
          if (this.circuit.rotateAt(slot)) {
            soundEngine.playRotate();
          }
          this.selectedSlot = null;
          this.rebuildGridVisuals();
          this.persistActiveRun();
        } else {
          this.performSlotTransfer(this.selectedSlot, slot);
          this.selectedSlot = null;
          this.rebuildGridVisuals();
        }
      }
    });

    this.input.setDraggable(zoneObj);
    zoneObj.on('dragstart', () => {
      const mod = this.getModuleAtSlot(slot);
      if (!mod) return;
      this.dragSource = slot;
      const def = MODULES[mod.defId];
      this.dragGhost
        .setTexture(def.spriteKey)
        .setDisplaySize(60, 60)
        .setAngle(mod.rotation * 90)
        .setPosition(zoneObj.x, zoneObj.y)
        .setVisible(true);
    });

    zoneObj.on('drag', (ptr: Phaser.Input.Pointer) => {
      if (!this.dragSource) return;
      this.dragGhost.setPosition(ptr.x, ptr.y);
    });

    zoneObj.on('dragend', (ptr: Phaser.Input.Pointer) => {
      if (!this.dragSource) return;
      this.dragGhost.setVisible(false);
      const src = this.dragSource;
      this.dragSource = null;

      // Check Sell Zone drop (x: 740..845, y: 595..710)
      if (ptr.x >= 732 && ptr.x <= 852 && ptr.y >= 588) {
        this.sellSlot(src);
        return;
      }

      // Check Grid Cell drop
      const gc = Math.round((ptr.x - GRID_ORIGIN_X) / CELL_STEP);
      const gr = Math.round((ptr.y - GRID_ORIGIN_Y) / CELL_STEP);
      if (
        gc >= 0 &&
        gc < GRID_COLS &&
        gr >= 0 &&
        gr < GRID_ROWS &&
        Math.hypot(ptr.x - (GRID_ORIGIN_X + gc * CELL_STEP), ptr.y - (GRID_ORIGIN_Y + gr * CELL_STEP)) < 42
      ) {
        this.performSlotTransfer(src, { zone: 'grid', col: gc, row: gr });
        this.selectedSlot = null;
        this.rebuildGridVisuals();
        return;
      }

      // Check Bench Slot drop
      const bi = Math.round((ptr.x - BENCH_ORIGIN_X) / CELL_STEP);
      if (
        bi >= 0 &&
        bi < BENCH_SIZE &&
        Math.hypot(ptr.x - (BENCH_ORIGIN_X + bi * CELL_STEP), ptr.y - BENCH_Y) < 40
      ) {
        this.performSlotTransfer(src, { zone: 'bench', idx: bi });
        this.selectedSlot = null;
        this.rebuildGridVisuals();
      }
    });
  }

  private performSlotTransfer(from: SlotRef, to: SlotRef): void {
    const res = this.circuit.moveOrMerge(from, to);
    if (!res.moved) return;

    if (res.mergedTier) {
      soundEngine.playMerge(res.mergedTier);
      const dstPos =
        to.zone === 'grid' ? this.cellToScreen(to.col, to.row) : this.benchToScreen(to.idx);
      this.spawnFloatingText(
        dstPos.x,
        dstPos.y - 12,
        `TIER ${res.mergedTier === 2 ? 'II' : 'III'} FUSION!`,
        '#ffe66d'
      );
      this.burstRingEffect(dstPos.x, dstPos.y, 0xffd86b, 56);

      const newly = saveManager.mutateStats((st) => {
        st.modulesMerged += 1;
      });
      newly.push(...saveManager.unlockAchievement('ach_first_merge'));
      if (res.mergedTier === 3) {
        newly.push(...saveManager.unlockAchievement('ach_tier3'));
      }
      newly.forEach((a, i) => showAchievementToast(this, a, i));
    } else {
      soundEngine.playPlace();
    }
    this.persistActiveRun();
  }

  private sellSlot(slot: SlotRef): void {
    const refund = this.circuit.sellAt(slot);
    if (refund > 0) {
      this.gold += refund;
      soundEngine.playCoin();
      this.selectedSlot = null;
      this.spawnFloatingText(790, 610, `+${refund}g SOLD`, '#ffd86b');
      this.rebuildGridVisuals();
      this.rebuildBottomDock();
      this.refreshHudValues();
      this.persistActiveRun();
    }
  }

  private drawPreviewBeams(): void {
    this.previewGfx.clear();
    const segs = this.circuit.tracePreviewPaths(this.getCircuitModifiers());

    for (const seg of segs) {
      const p1 = this.cellToScreen(seg.fromCol, seg.fromRow);
      const p2 = this.cellToScreen(seg.toCol, seg.toRow);

      let color = 0xffd166;
      if (seg.elements.includes('pyro')) color = 0xff6e27;
      else if (seg.elements.includes('cryo')) color = 0x6ef2ff;
      else if (seg.elements.includes('volt')) color = 0xc77dff;

      const width = Math.min(6, 2.2 + Math.log2(Math.max(1, seg.powerEstimate)) * 1.1);
      this.previewGfx.lineStyle(width + 3, color, 0.16);
      this.previewGfx.lineBetween(p1.x, p1.y, p2.x, p2.y);
      this.previewGfx.lineStyle(width, color, 0.52);
      this.previewGfx.lineBetween(p1.x, p1.y, p2.x, p2.y);
    }
  }

  // ==========================================================================
  // BOTTOM SHOP & TACTICAL COMMAND DOCK
  // ==========================================================================
  private rebuildBottomDock(): void {
    this.dockContainer.removeAll(true);
    const { width } = this.scale;

    const dockPanel = drawOrnatePanel(this, 8, 566, width - 16, 148, 0xc89b45, 0.95);
    this.dockContainer.add(dockPanel);

    // 4 Shop Offer Cards (x: 20..624)
    this.shopOffers.forEach((offer, idx) => {
      const cx = 20 + idx * 152;
      const cy = 576;
      const cw = 144;
      const ch = 128;

      const isRelic = offer.kind === 'relic';
      const defName = isRelic
        ? RELICS[offer.relicId!].name
        : MODULES[offer.moduleId!].name;
      const spriteKey = isRelic
        ? RELICS[offer.relicId!].spriteKey
        : MODULES[offer.moduleId!].spriteKey;
      const rarity = isRelic
        ? RELICS[offer.relicId!].rarity
        : MODULES[offer.moduleId!].rarity;
      const desc = isRelic
        ? RELICS[offer.relicId!].description
        : MODULES[offer.moduleId!].shortDesc;

      const borderCol = offer.purchased
        ? 0x2d3652
        : rarity === 'legendary'
        ? 0xffd166
        : rarity === 'rare'
        ? 0xd982ff
        : 0x5c79b8;

      const cardGfx = this.add.graphics();
      cardGfx.fillStyle(offer.purchased ? 0x0b1020 : 0x152040, 0.96);
      cardGfx.fillRoundedRect(cx, cy, cw, ch, 9);
      cardGfx.lineStyle(offer.locked ? 2.5 : 1.8, offer.locked ? 0x7ef9ff : borderCol, 1);
      cardGfx.strokeRoundedRect(cx, cy, cw, ch, 9);
      this.dockContainer.add(cardGfx);

      if (offer.purchased) {
        const soldTxt = this.add
          .text(cx + cw / 2, cy + ch / 2, 'ACQUIRED', {
            fontFamily: FONT_TITLE,
            fontSize: '13px',
            color: '#5a688c',
          })
          .setOrigin(0.5);
        this.dockContainer.add(soldTxt);
        return;
      }

      const icon = this.add.image(cx + 34, cy + 38, spriteKey).setDisplaySize(46, 46);
      const title = this.add.text(cx + 62, cy + 12, defName, {
        fontFamily: FONT_TITLE,
        fontSize: '11.5px',
        fontStyle: 'bold',
        color: '#ffe79e',
        wordWrap: { width: cw - 68 },
      });
      const tagTxt = this.add.text(
        cx + 62,
        cy + 48,
        isRelic ? 'RELIC' : MODULES[offer.moduleId!].category.toUpperCase(),
        {
          fontFamily: FONT_BODY,
          fontSize: '10px',
          color: '#7ef9ff',
        }
      );

      // Buy Button inside card
      const canAfford = this.gold >= offer.cost;
      const buyBtn = createOrnateButton(this, {
        x: cx + 56,
        y: cy + 102,
        width: 92,
        height: 32,
        primary: canAfford,
        fontSize: 12.5,
        label: `BUY ${offer.cost}g`,
        onClick: () => this.purchaseShopOffer(idx),
      });

      // Lock Icon toggle
      const lockIcon = this.add
        .image(cx + 122, cy + 102, offer.locked ? 'icon_lock' : 'icon_unlock')
        .setDisplaySize(24, 24)
        .setInteractive({ useHandCursor: true });
      lockIcon.on('pointerdown', () => {
        soundEngine.playClick();
        offer.locked = !offer.locked;
        this.rebuildBottomDock();
      });

      const hoverZone = this.add
        .zone(cx + cw / 2, cy + 44, cw, 72)
        .setInteractive({ useHandCursor: true });
      hoverZone.on('pointerover', () => {
        this.tooltip.show(cx, cy - 12, defName, `${rarity.toUpperCase()} ${offer.kind.toUpperCase()}`, desc);
      });
      hoverZone.on('pointerout', () => this.tooltip.hide());
      hoverZone.on('pointerdown', () => this.purchaseShopOffer(idx));

      this.dockContainer.add([icon, title, tagTxt, buyBtn, lockIcon, hoverZone]);
    });

    // Utility Column: REROLL, ROTATE [R], SELL VAULT (x: 634..848)
    const freeFirst = saveManager.getSkillRank('econ_reroll') > 0;
    const rerollCost = getRerollCost(this.rerollCount, this.wardenId, freeFirst);
    const rerollBtn = createOrnateButton(this, {
      x: 684,
      y: 598,
      width: 96,
      height: 38,
      primary: this.gold >= rerollCost,
      fontSize: 11.5,
      label: rerollCost === 0 ? 'REROLL FREE' : `REROLL ${rerollCost}g`,
      onClick: () => {
        if (this.gold < rerollCost) {
          this.spawnFloatingText(684, 575, 'Not enough Gold', '#ff758f');
          return;
        }
        this.gold -= rerollCost;
        this.rerollCount += 1;
        const rarityBoost = saveManager.getSkillRank('econ_rarity') * 0.12;
        this.shopOffers = rollShopOffers(
          this.wave,
          this.relics,
          this.getMaxRelics(),
          rarityBoost,
          this.shopOffers
        );
        this.rebuildBottomDock();
        this.refreshHudValues();
        this.persistActiveRun();
      },
    });

    const rotBtn = createOrnateButton(this, {
      x: 684,
      y: 642,
      width: 96,
      height: 36,
      fontSize: 11.5,
      label: 'ROTATE [R]',
      onClick: () => this.rotateSelectedOrFirstPrism(),
    });

    const sellBtn = createOrnateButton(this, {
      x: 792,
      y: 620,
      width: 102,
      height: 82,
      iconKey: 'icon_sell',
      fontSize: 12,
      label: 'SELL',
      onClick: () => {
        if (this.selectedSlot) {
          this.sellSlot(this.selectedSlot);
        } else {
          this.spawnFloatingText(792, 580, 'Drag or Select Module to Sell', '#ffd166');
        }
      },
    });

    this.dockContainer.add([rerollBtn, rotBtn, sellBtn]);

    // Tactical Abilities Trio ([1], [2], [3]) (x: 856..1068)
    this.abilityCdTexts = [];
    const warden = WARDENS[this.wardenId];
    const abilities = [
      {
        key: '1',
        name: 'Pulse Surge',
        icon: 'ability_surge',
        desc: 'Immediately emits an Overcharged x1.75 Aether Pulse from all Solar Emitters.',
      },
      {
        key: '2',
        name: 'Solar Nova',
        icon: 'ability_nova',
        desc: 'Blasts Voidkin near the Sunspire Core, pushing them back and igniting Burn.',
      },
      {
        key: '3',
        name: warden.ultName,
        icon: warden.ultIconKey,
        desc: warden.ultDesc,
      },
    ];

    abilities.forEach((ab, i) => {
      const ax = 890 + i * 68;
      const ay = 620;
      const g = this.add.graphics();
      g.fillStyle(0x162242, 0.96);
      g.fillRoundedRect(ax - 29, ay - 38, 58, 76, 8);
      g.lineStyle(2, 0xf2c464, 0.9);
      g.strokeRoundedRect(ax - 29, ay - 38, 58, 76, 8);

      const ic = this.add.image(ax, ay - 8, ab.icon).setDisplaySize(34, 34);
      const hk = this.add
        .text(ax, ay + 22, `[${ab.key}]`, {
          fontFamily: FONT_TITLE,
          fontSize: '11px',
          fontStyle: 'bold',
          color: '#ffe38e',
        })
        .setOrigin(0.5);

      const cdTxt = this.add
        .text(ax, ay - 8, '', {
          fontFamily: FONT_TITLE,
          fontSize: '14px',
          fontStyle: 'bold',
          color: '#ffffff',
          stroke: '#000000',
          strokeThickness: 3,
        })
        .setOrigin(0.5);
      this.abilityCdTexts.push(cdTxt);

      const z = this.add
        .zone(ax, ay, 58, 76)
        .setInteractive({ useHandCursor: true });
      z.on('pointerover', () => {
        this.tooltip.show(
          ax - 120,
          ay - 48,
          `[${ab.key}] ${ab.name}`,
          `Cooldown: ${this.abilityMaxCds[i]}s`,
          ab.desc
        );
      });
      z.on('pointerout', () => this.tooltip.hide());
      z.on('pointerdown', () => this.tryCastAbility(i as 0 | 1 | 2));

      this.dockContainer.add([g, ic, hk, cdTxt, z]);
    });

    // Primary Wave Launch / Live Pulse Button (x: 1084..1264)
    const launchBtn = createOrnateButton(this, {
      x: 1172,
      y: 638,
      width: 180,
      height: 92,
      primary: true,
      fontSize: 15,
      label: this.inCombat ? 'EMIT PULSE\n[SPACE]' : `START WAVE ${this.wave}\n[SPACE]`,
      onClick: () => {
        if (!this.inCombat) {
          this.beginCombatWave();
        } else {
          this.tryCastAbility(0);
        }
      },
    });
    this.waveBtnLabel = launchBtn.getData('labelObj');
    this.dockContainer.add(launchBtn);
  }

  private purchaseShopOffer(slotIdx: number): void {
    const offer = this.shopOffers[slotIdx];
    if (!offer || offer.purchased) return;
    if (this.gold < offer.cost) {
      this.spawnFloatingText(100 + slotIdx * 150, 560, 'Insufficient Gold!', '#ff758f');
      return;
    }

    if (offer.kind === 'relic' && offer.relicId) {
      if (this.relics.length >= this.getMaxRelics()) {
        this.spawnFloatingText(520, 72, 'Relic Vault Full!', '#ff758f');
        return;
      }
      this.gold -= offer.cost;
      offer.purchased = true;
      offer.locked = false;
      this.gainRelic(offer.relicId);
      soundEngine.playMerge(2);
    } else if (offer.kind === 'module' && offer.moduleId) {
      if (!this.circuit.hasSpaceForPurchase(offer.moduleId)) {
        this.spawnFloatingText(350, 480, 'Grid & Bench Full!', '#ff758f');
        return;
      }
      this.gold -= offer.cost;
      offer.purchased = true;
      offer.locked = false;
      const bought = this.circuit.buyOrAutoMergeModule(offer.moduleId);
      saveManager.discoverModule(offer.moduleId);
      if (bought.mergedToTier) {
        soundEngine.playMerge(bought.mergedToTier as 2 | 3);
        const ach = saveManager.unlockAchievement('ach_first_merge');
        ach.forEach((a, i) => showAchievementToast(this, a, i));
      } else {
        soundEngine.playPlace();
      }
      this.rebuildGridVisuals();
    }

    this.buildTopHud();
    this.rebuildBottomDock();
    this.persistActiveRun();
  }

  private gainRelic(relicId: RelicId): void {
    if (this.relics.includes(relicId)) return;
    this.relics.push(relicId);
    saveManager.discoverRelic(relicId);

    if (relicId === 'relic_solar_heart') {
      this.combat.maxCoreHp += 35;
      this.combat.coreHp = Math.min(this.combat.maxCoreHp, this.combat.coreHp + 20);
      this.combat.coreShield += 12;
    } else if (relicId === 'relic_compass') {
      this.circuit.upgradeRandomNormalTiles(2, 'radiant');
      this.rebuildGridVisuals();
    }

    this.recomputeEmitterInterval();
    if (this.relics.length >= 5) {
      const ach = saveManager.unlockAchievement('ach_relics_5');
      ach.forEach((a, i) => showAchievementToast(this, a, i));
    }
  }

  private rotateSelectedOrFirstPrism(): void {
    if (this.selectedSlot && this.circuit.rotateAt(this.selectedSlot)) {
      soundEngine.playRotate();
      this.rebuildGridVisuals();
      this.persistActiveRun();
      return;
    }
    for (let r = 0; r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        const m = this.circuit.cells[r][c].module;
        if (m && MODULES[m.defId].rotatable) {
          m.rotation = ((m.rotation + 1) % 4) as Direction;
          soundEngine.playRotate();
          this.rebuildGridVisuals();
          this.persistActiveRun();
          return;
        }
      }
    }
  }

  // ==========================================================================
  // BATTLEFIELD INPUT, KEYBOARD & TACTICAL ABILITIES
  // ==========================================================================
  private setupBattlefieldInput(): void {
    this.input.on('pointerdown', (ptr: Phaser.Input.Pointer) => {
      if (this.isPaused || this.runEnded) return;
      // Right-half battlefield click -> Focus Target + Manual Overwatch Shot!
      if (ptr.x > 610 && ptr.y > 58 && ptr.y < 562) {
        let closestId: string | null = null;
        let bestDist = 72;
        for (const en of this.combat.enemies) {
          const d = Math.hypot(en.x - ptr.x, en.y - ptr.y);
          if (d < bestDist) {
            bestDist = d;
            closestId = en.uid;
          }
        }
        if (closestId) {
          this.combat.priorityEnemyUid = closestId;
          soundEngine.playClick();
        }
        if (this.inCombat && this.manualClickCd <= 0 && this.combat.enemies.length > 0) {
          this.manualClickCd = 0.42;
          const target = this.combat.selectTarget(false);
          if (target) {
            const overwatchRank = saveManager.getSkillRank('bastion_focus');
            const clickDmg = (14 + this.wave * 3) * (1 + overwatchRank * 0.3);
            const dummyEvents = {
              visuals: [] as VisualHitEvent[],
              lootDrops: [],
              slainIds: [],
              coreHitDamage: 0,
              phoenixTriggered: false,
              highestHit: 0,
              relicsTriggered: [],
            };
            const dealt = this.combat.applyDamage(
              target,
              clickDmg,
              [],
              false,
              { wardenId: this.wardenId, weatherId: this.weatherId, relics: this.relics },
              dummyEvents
            );
            this.renderVisualHit({
              kind: 'projectile',
              fromX: CORE_POS.x,
              fromY: CORE_POS.y,
              toX: target.x,
              toY: target.y,
              color: 0x7ef9ff,
              damage: dealt,
              isCrit: false,
              spriteKey: 'proj_arcane',
            });
          }
        }
      }
    });
  }

  private setupKeyboardShortcuts(): void {
    this.input.keyboard?.on('keydown', (ev: KeyboardEvent) => {
      soundEngine.unlock();
      if (ev.key === 'Escape') {
        this.togglePauseModal();
        return;
      }
      if (this.isPaused || this.runEnded) return;

      if (ev.key === 'r' || ev.key === 'R') {
        this.rotateSelectedOrFirstPrism();
      } else if (ev.key === ' ') {
        ev.preventDefault();
        if (!this.inCombat) this.beginCombatWave();
        else this.tryCastAbility(0);
      } else if (ev.key === '1') {
        this.tryCastAbility(0);
      } else if (ev.key === '2') {
        this.tryCastAbility(1);
      } else if (ev.key === '3') {
        this.tryCastAbility(2);
      }
    });
  }

  private tryCastAbility(slot: 0 | 1 | 2): void {
    if (this.isPaused || this.runEnded) return;
    if (!this.inCombat) {
      // Allow testing Pulse Surge even before starting wave!
      if (slot === 0) {
        this.beginCombatWave();
      } else {
        this.spawnFloatingText(950, 555, 'Start Wave First!', '#ffd166');
      }
      return;
    }
    if (this.abilityCds[slot] > 0) return;

    const cdScale =
      (1 - saveManager.getSkillRank('bastion_tactical') * 0.08) *
      (this.relics.includes('relic_archon_shard') ? 0.7 : 1.0);
    this.abilityCds[slot] = Math.max(3, this.abilityMaxCds[slot] * cdScale);
    soundEngine.playAbility();

    if (slot === 0) {
      // [1] PULSE SURGE
      const count = this.circuit.emitPulseWave(this.getCircuitModifiers(), 1.75);
      saveManager.mutateStats((st) => {
        st.pulsesEmitted += count;
      });
      this.spawnFloatingText(280, 130, 'OVERPULSE x1.75!', '#ffe66d');
    } else if (slot === 1) {
      // [2] SOLAR NOVA
      this.burstRingEffect(CORE_POS.x, CORE_POS.y, 0xff7b29, 210);
      this.maybeShake(0.006, 180);
      const dummy = {
        visuals: [] as VisualHitEvent[],
        lootDrops: [],
        slainIds: [],
        coreHitDamage: 0,
        phoenixTriggered: false,
        highestHit: 0,
        relicsTriggered: [],
      };
      for (const en of this.combat.enemies) {
        const dist = Math.hypot(en.x - CORE_POS.x, en.y - CORE_POS.y);
        if (dist <= 280) {
          if (!en.isBoss) en.progress = Math.max(0, en.progress - 0.14);
          const d = this.combat.applyDamage(
            en,
            55 + this.wave * 14,
            ['pyro'],
            false,
            { wardenId: this.wardenId, weatherId: this.weatherId, relics: this.relics },
            dummy
          );
          this.spawnDamageNumber(en.x, en.y, d, false, 0xff9f1c);
        }
      }
      this.spawnFloatingText(CORE_POS.x + 40, CORE_POS.y - 50, 'SOLAR NOVA!', '#ff9f1c');
    } else {
      // [3] WARDEN ULTIMATE
      this.maybeShake(0.009, 260);
      const dummy = {
        visuals: [] as VisualHitEvent[],
        lootDrops: [],
        slainIds: [],
        coreHitDamage: 0,
        phoenixTriggered: false,
        highestHit: 0,
        relicsTriggered: [],
      };
      if (this.wardenId === 'lyra') {
        this.pyroUltTimer = 6.0;
        this.spawnFloatingText(900, 140, 'CATACLYSM RAIN!', '#ff6e27');
        for (let m = 0; m < 8; m++) {
          this.time.delayedCall(m * 110, () => {
            const target = this.combat.enemies[m % Math.max(1, this.combat.enemies.length)];
            const tx = target ? target.x : 780 + Math.random() * 280;
            const ty = target ? target.y : 200 + Math.random() * 300;
            this.burstRingEffect(tx, ty, 0xff5400, 95);
            for (const en of this.combat.enemies) {
              if (Math.hypot(en.x - tx, en.y - ty) <= 95) {
                const d = this.combat.applyDamage(
                  en,
                  48 + this.wave * 12,
                  ['pyro'],
                  true,
                  { wardenId: this.wardenId, weatherId: this.weatherId, relics: this.relics },
                  dummy
                );
                this.spawnDamageNumber(en.x, en.y, d, true, 0xffbe3b);
              }
            }
          });
        }
      } else if (this.wardenId === 'vex') {
        this.hasteUltTimer = 6.0;
        this.recomputeEmitterInterval();
        for (const en of this.combat.enemies) {
          en.freezeTimer = Math.max(en.freezeTimer, 3.8);
        }
        this.burstRingEffect(900, 355, 0x6ef2ff, 300);
        this.spawnFloatingText(900, 140, 'CHRONO STASIS!', '#7ef9ff');
      } else if (this.wardenId === 'kael') {
        this.spawnFloatingText(900, 140, 'TEMPEST MATRIX!', '#9b72ff');
        const turretScale = 1 + saveManager.getSkillRank('solar_potency') * 0.1;
        for (let r = 0; r < GRID_ROWS; r++) {
          for (let c = 0; c < GRID_COLS; c++) {
            const m = this.circuit.cells[r][c].module;
            if (m && MODULES[m.defId].category === 'turret') {
              const origin = this.cellToScreen(c, r);
              const dealt = this.combat.executeTurretFire(
                {
                  moduleUid: m.uid,
                  defId: m.defId,
                  tier: m.tier,
                  col: c,
                  row: r,
                  powerMult: 2.2,
                  critChance: 1.0,
                  elements: ['volt'],
                  comboChain: 6,
                },
                origin,
                {
                  wardenId: this.wardenId,
                  weatherId: this.weatherId,
                  relics: this.relics,
                  turretDamageScale: turretScale,
                },
                dummy
              );
              m.totalDamageDealt += dealt;
            }
          }
        }
        dummy.visuals.forEach((v) => this.renderVisualHit(v));
      } else {
        // Nyx Singularity Transmute
        this.burstRingEffect(900, 355, 0xd982ff, 260);
        this.gold += 18;
        this.spawnFloatingText(900, 140, 'SINGULARITY +18g!', '#df80ff');
        for (const en of this.combat.enemies) {
          if (!en.isBoss) en.progress = Math.max(0.1, en.progress - 0.12);
          const d = this.combat.applyDamage(
            en,
            65 + this.wave * 15,
            ['venom'],
            true,
            { wardenId: this.wardenId, weatherId: this.weatherId, relics: this.relics },
            dummy
          );
          this.spawnDamageNumber(en.x, en.y, d, true, 0x70ffba);
        }
        this.refreshHudValues();
      }
    }
  }

  // ==========================================================================
  // WAVE LIFECYCLE & UPDATE LOOP
  // ==========================================================================
  public beginCombatWave(): void {
    if (this.inCombat || this.runEnded) return;
    this.inCombat = true;
    this.tookDamageThisWave = false;
    this.emitterTimer = this.emitterInterval * 0.85; // Fire almost immediately!

    const shieldPerWave =
      saveManager.getSkillRank('bastion_shield') * 8 +
      (this.relics.includes('relic_solar_heart') ? 12 : 0);
    this.combat.startWave(this.wave, this.modeId, shieldPerWave);

    const isBossWave = this.wave % 5 === 0;
    soundEngine.setMusicMood(isBossWave ? 'boss' : 'combat');
    soundEngine.playClick();

    if (this.waveBtnLabel) {
      this.waveBtnLabel.setText('EMIT PULSE\n[SPACE]');
    }
    this.hintBannerText.setText(
      isBossWave
        ? `WARNING: VOID TITAN APPROACHING ON WAVE ${this.wave}!`
        : `Wave ${this.wave} Siege Active — Click Voidkin to Focus (+20% Dmg) & collect Gold!`
    );
    this.refreshHudValues();
  }

  public update(_time: number, deltaMs: number): void {
    if (this.isPaused || this.runEnded) return;

    const rawDt = Math.min(0.05, deltaMs / 1000);
    const dt = rawDt * this.gameSpeed;

    // Cooldowns & Buff Timers
    if (this.manualClickCd > 0) this.manualClickCd = Math.max(0, this.manualClickCd - rawDt);
    for (let i = 0; i < 3; i++) {
      if (this.abilityCds[i] > 0) {
        this.abilityCds[i] = Math.max(0, this.abilityCds[i] - dt);
      }
      if (this.abilityCdTexts[i]) {
        this.abilityCdTexts[i].setText(
          this.abilityCds[i] > 0 ? this.abilityCds[i].toFixed(1) : ''
        );
      }
    }
    if (this.pyroUltTimer > 0) this.pyroUltTimer = Math.max(0, this.pyroUltTimer - dt);
    if (this.hasteUltTimer > 0) {
      this.hasteUltTimer = Math.max(0, this.hasteUltTimer - dt);
      if (this.hasteUltTimer === 0) this.recomputeEmitterInterval();
    }

    // Update Object Pools (Floating Text & Loot Magnet)
    this.stepFloatingPool(rawDt);
    this.stepLootPool(rawDt);

    if (!this.inCombat) return;

    // 1. Emit Kinetic Aether Pulses on cadence
    this.emitterTimer += dt;
    if (this.emitterTimer >= this.emitterInterval) {
      this.emitterTimer -= this.emitterInterval;
      const emitted = this.circuit.emitPulseWave(this.getCircuitModifiers(), 1.0);
      saveManager.mutateStats((st) => {
        st.pulsesEmitted += emitted;
      });
    }

    // 2. Step Circuit Pulses & Fire Energized Turrets
    const mods = this.getCircuitModifiers();
    const circuitRes = this.circuit.stepPulses(dt, mods, this.pyroUltTimer > 0);

    if (circuitRes.goldMinted > 0) {
      this.gold += circuitRes.goldMinted;
      soundEngine.playCoin();
      this.spawnFloatingText(356, 150, `+${circuitRes.goldMinted}g`, '#ffd86b');
    }

    if (circuitRes.peakMultReached > this.peakMultThisRun) {
      this.peakMultThisRun = circuitRes.peakMultReached;
      this.checkMultiplierAchievements(this.peakMultThisRun);
    }
    if (circuitRes.maxElementsOnPulse >= 3) {
      saveManager
        .unlockAchievement('ach_trinity_elem')
        .forEach((a, i) => showAchievementToast(this, a, i));
    }
    circuitRes.triggeredRelics.forEach((r) => this.pulseRelicIcon(r));

    const turretScale = 1 + saveManager.getSkillRank('solar_potency') * 0.1;
    const bountyScale = 1 + saveManager.getSkillRank('econ_bounty') * 0.1;

    // 3. Step Combat Simulation
    const combatEvents = this.combat.stepCombat(dt, this.wave, this.modeId, {
      wardenId: this.wardenId,
      weatherId: this.weatherId,
      relics: this.relics,
      bountyScale,
    });

    for (const cmd of circuitRes.fireCommands) {
      soundEngine.playPulseCombo(cmd.comboChain);
      soundEngine.playTurretFire(cmd.defId);
      const origin = this.cellToScreen(cmd.col, cmd.row);
      const dealt = this.combat.executeTurretFire(
        cmd,
        origin,
        {
          wardenId: this.wardenId,
          weatherId: this.weatherId,
          relics: this.relics,
          turretDamageScale: turretScale,
        },
        combatEvents
      );
      const cell = this.circuit.getCell(cmd.col, cmd.row);
      if (cell?.module) cell.module.totalDamageDealt += dealt;
    }

    // 4. Render Visuals, Loot, Stats & Check Wave End / Defeat
    combatEvents.relicsTriggered.forEach((r) => this.pulseRelicIcon(r));
    combatEvents.visuals.forEach((v) => this.renderVisualHit(v));

    for (const drop of combatEvents.lootDrops) {
      this.spawnLootOrb(drop.x, drop.y, drop.gold, drop.starlight);
    }

    if (combatEvents.slainIds.length > 0 || combatEvents.highestHit > 0) {
      combatEvents.slainIds.forEach((id) => saveManager.discoverEnemy(id));
      const bossKills = combatEvents.slainIds.filter((id) => ENEMIES[id].isBoss).length;

      const unlocked = saveManager.mutateStats((st) => {
        st.enemiesVanquished += combatEvents.slainIds.length;
        st.bossesVanquished += bossKills;
        if (combatEvents.highestHit > st.highestSingleHit) {
          st.highestSingleHit = combatEvents.highestHit;
        }
        if (this.peakMultThisRun > st.highestPulseMult) {
          st.highestPulseMult = this.peakMultThisRun;
        }
      });

      if (combatEvents.highestHit >= 1000) {
        unlocked.push(...saveManager.unlockAchievement('ach_hit_1000'));
      }
      if (combatEvents.slainIds.includes('boss_colossus')) {
        unlocked.push(...saveManager.unlockAchievement('ach_boss_1'));
      }
      if (combatEvents.slainIds.includes('boss_weaver')) {
        unlocked.push(...saveManager.unlockAchievement('ach_boss_2'));
      }
      unlocked.forEach((a, i) => showAchievementToast(this, a, i));
    }

    if (combatEvents.phoenixTriggered) {
      this.burstRingEffect(CORE_POS.x, CORE_POS.y, 0xffbe0b, 380);
      this.spawnFloatingText(CORE_POS.x + 30, CORE_POS.y - 60, 'PHOENIX REBIRTH!', '#ffe66d');
    }

    if (combatEvents.coreHitDamage > 0) {
      this.tookDamageThisWave = true;
      soundEngine.playCoreHit();
      this.maybeShake(0.01, 220);
      this.spawnFloatingText(
        CORE_POS.x,
        CORE_POS.y - 36,
        `-${combatEvents.coreHitDamage} CORE`,
        '#ff4d6d'
      );
    }

    this.syncPulseSprites();
    this.syncEnemySprites();
    this.refreshHudValues();

    if (this.combat.coreHp <= 0) {
      this.handleRunEnd(false);
      return;
    }

    if (this.combat.isWaveComplete()) {
      this.handleWaveCleared();
    }
  }

  private handleWaveCleared(): void {
    this.inCombat = false;
    this.circuit.activePulses = [];
    this.syncPulseSprites();
    this.autoCollectRemainingLoot();
    soundEngine.playWaveClear();
    soundEngine.setMusicMood('menu');

    // Wave Clear Rewards & Interest
    const bountyScale = 1 + saveManager.getSkillRank('econ_bounty') * 0.1;
    const waveGoldBonus = Math.round((16 + this.wave * 3) * bountyScale);
    const interest = this.relics.includes('relic_abacus')
      ? Math.min(25, Math.floor(this.gold * 0.15))
      : 0;
    if (interest > 0) this.pulseRelicIcon('relic_abacus');

    this.gold += waveGoldBonus + interest;

    const weatherStarMult = this.weatherId === 'blood_eclipse' ? 1.25 : 1.0;
    const starGain = Math.max(
      2,
      Math.round((3 + Math.floor(this.wave * 0.75)) * GAME_MODES[this.modeId].starlightMult * bountyScale * weatherStarMult)
    );
    this.starlightEarnedThisRun += starGain;
    saveManager.addStarlight(starGain);

    // Achievements check
    const newly: ReturnType<typeof saveManager.unlockAchievement> = [];
    newly.push(...saveManager.unlockAchievement('ach_first_wave'));
    if (!this.tookDamageThisWave) {
      this.flawlessStreak += 1;
      if (this.flawlessStreak >= 5) {
        newly.push(...saveManager.unlockAchievement('ach_flawless_5'));
      }
    } else {
      this.flawlessStreak = 0;
    }
    if (this.gold >= 300) {
      newly.push(...saveManager.unlockAchievement('ach_gold_300'));
    }

    saveManager.mutateStats((st) => {
      st.totalGoldEarned += waveGoldBonus + interest;
      if (this.wave > st.highestWave) st.highestWave = this.wave;
    });

    if (this.wave >= 20) {
      newly.push(...saveManager.unlockAchievement('ach_endless_20'));
    }

    const targetWaves = GAME_MODES[this.modeId].targetWaves;
    if (targetWaves > 0 && this.wave >= targetWaves && !this.endlessContinued) {
      newly.push(...saveManager.unlockAchievement('ach_victory'));
      if (this.modeId.startsWith('challenge_')) {
        newly.push(...saveManager.unlockAchievement('ach_challenge_win'));
      }
      newly.forEach((a, i) => showAchievementToast(this, a, i));
      this.handleRunEnd(true);
      return;
    }

    newly.forEach((a, i) => showAchievementToast(this, a, i));

    // Advance to Next Wave
    this.wave += 1;
    this.rerollCount = 0;

    // Forecast Celestial Weather every 2 waves
    if (this.wave % 2 === 0) {
      const pool: WeatherId[] = ['clear', 'solar_flare', 'ion_storm', 'cryo_nebula', 'blood_eclipse'];
      this.weatherId = pool[(this.wave * 3) % pool.length];
      this.bgImage.setTint(WEATHERS[this.weatherId].tintHex);
      if (this.weatherId === 'ion_storm') {
        this.circuit.upgradeRandomNormalTiles(1, 'haste');
      } else if (this.weatherId === 'solar_flare') {
        this.circuit.upgradeRandomNormalTiles(1, 'radiant');
      }
    }

    this.recomputeEmitterInterval();
    const rarityBoost = saveManager.getSkillRank('econ_rarity') * 0.12;
    this.shopOffers = rollShopOffers(
      this.wave,
      this.relics,
      this.getMaxRelics(),
      rarityBoost,
      this.shopOffers
    );

    this.spawnFloatingText(
      640,
      260,
      `WAVE CLEARED! +${waveGoldBonus + interest}g  +${starGain} Starlight`,
      '#69ff97'
    );

    this.rebuildGridVisuals();
    this.buildTopHud();
    this.rebuildBottomDock();
    this.persistActiveRun();
  }

  private checkMultiplierAchievements(mult: number): void {
    const u = [];
    if (mult >= 3) u.push(...saveManager.unlockAchievement('ach_mult_3'));
    if (mult >= 8) u.push(...saveManager.unlockAchievement('ach_mult_8'));
    if (mult >= 20) u.push(...saveManager.unlockAchievement('ach_mult_20'));
    u.forEach((a, i) => showAchievementToast(this, a, i));
  }

  // ==========================================================================
  // SPRITE SYNCHRONIZATION & VISUAL EFFECTS
  // ==========================================================================
  private syncPulseSprites(): void {
    const activeIds = new Set<string>();
    for (const p of this.circuit.activePulses) {
      activeIds.add(p.id);
      const cellCenter = this.cellToScreen(p.col, p.row);
      const delta = DIR_DELTA[p.dir];
      const offset = (p.progress - 0.5) * CELL_STEP;
      const px = cellCenter.x + delta.dc * offset;
      const py = cellCenter.y + delta.dr * offset;

      let tint = 0xffe680;
      if (p.elements.size >= 2) tint = 0xff8af2;
      else if (p.elements.has('pyro')) tint = 0xff6e27;
      else if (p.elements.has('cryo')) tint = 0x6ef2ff;
      else if (p.elements.has('volt')) tint = 0xb96bff;

      let pair = this.pulseSprites.get(p.id);
      if (!pair) {
        const orb = this.add
          .image(px, py, 'proj_pulse')
          .setDisplaySize(32, 32)
          .setBlendMode(Phaser.BlendModes.ADD);
        const badge = this.add
          .text(px, py - 18, '', {
            fontFamily: FONT_TITLE,
            fontSize: '10.5px',
            fontStyle: 'bold',
            color: '#fff6c4',
            stroke: '#050814',
            strokeThickness: 3,
          })
          .setOrigin(0.5);
        this.pulseContainer.add([orb, badge]);
        pair = { orb, badge };
        this.pulseSprites.set(p.id, pair);
      }

      const scale = Math.min(1.6, 0.8 + Math.log2(Math.max(1, p.powerMult)) * 0.16);
      pair.orb.setPosition(px, py).setTint(tint).setScale(scale);
      pair.badge
        .setPosition(px, py - 18)
        .setText(p.powerMult >= 1.15 ? `x${p.powerMult.toFixed(1)}` : '');
    }

    for (const [id, pair] of this.pulseSprites.entries()) {
      if (!activeIds.has(id)) {
        pair.orb.destroy();
        pair.badge.destroy();
        this.pulseSprites.delete(id);
      }
    }
  }

  private syncEnemySprites(): void {
    const aliveIds = new Set<string>();
    let activeBoss = null;

    for (const en of this.combat.enemies) {
      aliveIds.add(en.uid);
      if (en.isBoss) activeBoss = en;

      const def = ENEMIES[en.defId];
      let vis = this.enemyVisuals.get(en.uid);
      if (!vis) {
        const basePx = (en.isBoss ? 88 : 54) * def.scale;
        const sprite = this.add.sprite(en.x, en.y, def.spriteKey, 0).setDisplaySize(basePx, basePx);
        sprite.play(`${def.spriteKey}_walk`);
        const hpBg = this.add.graphics();
        const hpFill = this.add.graphics();
        const statusTxt = this.add
          .text(en.x, en.y - basePx * 0.65, '', {
            fontFamily: FONT_BODY,
            fontSize: '9.5px',
            fontStyle: 'bold',
            color: '#9efcff',
            stroke: '#050814',
            strokeThickness: 2.5,
          })
          .setOrigin(0.5);
        this.enemyContainer.add([sprite, hpBg, hpFill, statusTxt]);
        vis = { sprite, hpBg, hpFill, statusTxt };
        this.enemyVisuals.set(en.uid, vis);
      }

      vis.sprite.setPosition(en.x, en.y);
      vis.sprite.setAlpha(en.isPhased ? 0.48 : 1);

      if (en.freezeTimer > 0) vis.sprite.setTint(0x7ef9ff);
      else if (en.burnTimer > 0) vis.sprite.setTint(0xff9e66);
      else if (en.venomStacks > 0) vis.sprite.setTint(0x98ff96);
      else vis.sprite.clearTint();

      const barW = en.isBoss ? 68 : 40;
      const barY = en.y - (en.isBoss ? 48 : 30);
      vis.hpBg.clear();
      vis.hpBg.fillStyle(0x090d1a, 0.85);
      vis.hpBg.fillRect(en.x - barW / 2, barY, barW, 5);

      vis.hpFill.clear();
      const hpRatio = Math.max(0, en.hp / en.maxHp);
      vis.hpFill.fillStyle(en.isBoss ? 0xff4d79 : 0xff5964, 1);
      vis.hpFill.fillRect(en.x - barW / 2, barY, barW * hpRatio, 5);

      if (en.shield > 0 && en.maxShield > 0) {
        const shRatio = Math.max(0, en.shield / en.maxShield);
        vis.hpFill.fillStyle(0x62ecff, 1);
        vis.hpFill.fillRect(en.x - barW / 2, barY - 4, barW * shRatio, 3);
      }

      const tags: string[] = [];
      if (en.freezeTimer > 0) tags.push('FROZEN');
      else if (en.chillStacks > 0) tags.push(`CHILL ${en.chillStacks}`);
      if (en.burnTimer > 0) tags.push('BURN');
      if (en.venomStacks > 0) tags.push(`VENOM ${en.venomStacks}`);
      vis.statusTxt.setPosition(en.x, barY - 10).setText(tags.join(' '));
    }

    for (const [uid, vis] of this.enemyVisuals.entries()) {
      if (!aliveIds.has(uid)) {
        vis.sprite.destroy();
        vis.hpBg.destroy();
        vis.hpFill.destroy();
        vis.statusTxt.destroy();
        this.enemyVisuals.delete(uid);
      }
    }

    // Priority Target Reticle
    const prio = this.combat.priorityEnemyUid
      ? this.combat.enemies.find((e) => e.uid === this.combat.priorityEnemyUid)
      : null;
    if (prio) {
      this.targetReticle.setPosition(prio.x, prio.y).setVisible(true);
    } else {
      this.targetReticle.setVisible(false);
    }

    // Boss Overhead Bar
    if (activeBoss) {
      this.bossBarContainer.setVisible(true);
      this.bossNameText.setText(
        `${ENEMIES[activeBoss.defId].name}  (${Math.ceil(activeBoss.hp)} / ${activeBoss.maxHp})`
      );
      this.bossBarFill.clear();
      const r = Math.max(0, activeBoss.hp / activeBoss.maxHp);
      this.bossBarFill.fillStyle(0xd90429, 1);
      this.bossBarFill.fillRoundedRect(-198, 2, 396 * r, 11, 4);
    } else {
      this.bossBarContainer.setVisible(false);
    }
  }

  private renderVisualHit(v: VisualHitEvent): void {
    if (v.kind === 'projectile' && v.spriteKey) {
      const p = this.add
        .image(v.fromX, v.fromY, v.spriteKey)
        .setDisplaySize(26, 26)
        .setDepth(45)
        .setRotation(Math.atan2(v.toY - v.fromY, v.toX - v.fromX));
      this.tweens.add({
        targets: p,
        x: v.toX,
        y: v.toY,
        duration: 140,
        onComplete: () => {
          p.destroy();
          if (v.radius) this.burstRingEffect(v.toX, v.toY, v.color, v.radius);
        },
      });
    } else if (v.kind === 'beam') {
      const g = this.add.graphics().setDepth(44);
      g.lineStyle(8, v.color, 0.35);
      g.lineBetween(v.fromX, v.fromY, v.toX, v.toY);
      g.lineStyle(3.5, 0xffffff, 0.95);
      g.lineBetween(v.fromX, v.fromY, v.toX, v.toY);
      this.tweens.add({
        targets: g,
        alpha: 0,
        duration: 180,
        onComplete: () => g.destroy(),
      });
    } else if (v.kind === 'chain') {
      const g = this.add.graphics().setDepth(44);
      g.lineStyle(2.5, v.color, 0.95);
      const midX = (v.fromX + v.toX) / 2 + (Math.random() - 0.5) * 26;
      const midY = (v.fromY + v.toY) / 2 + (Math.random() - 0.5) * 26;
      g.beginPath();
      g.moveTo(v.fromX, v.fromY);
      g.lineTo(midX, midY);
      g.lineTo(v.toX, v.toY);
      g.strokePath();
      this.tweens.add({
        targets: g,
        alpha: 0,
        duration: 140,
        onComplete: () => g.destroy(),
      });
    } else if (v.kind === 'shatter' || v.kind === 'heal') {
      this.burstRingEffect(v.toX, v.toY, v.color, v.radius ?? 55);
    }

    if (v.damage > 0) {
      this.spawnDamageNumber(v.toX, v.toY, v.damage, v.isCrit, v.color);
    }
  }

  private burstRingEffect(x: number, y: number, color: number, targetRadius: number): void {
    const ring = this.add
      .image(x, y, 'vfx_ring')
      .setTint(color)
      .setDisplaySize(16, 16)
      .setAlpha(0.85)
      .setBlendMode(Phaser.BlendModes.ADD)
      .setDepth(46);
    this.tweens.add({
      targets: ring,
      displayWidth: targetRadius * 2,
      displayHeight: targetRadius * 2,
      alpha: 0,
      duration: 260,
      ease: 'Quad.easeOut',
      onComplete: () => ring.destroy(),
    });
  }

  private pulseRelicIcon(id: RelicId): void {
    const img = this.relicIcons.get(id);
    if (!img) return;
    this.tweens.add({
      targets: img,
      scaleX: img.scaleX * 1.28,
      scaleY: img.scaleY * 1.28,
      duration: 95,
      yoyo: true,
    });
  }

  private spawnDamageNumber(x: number, y: number, dmg: number, isCrit: boolean, tintHex: number): void {
    if (!saveManager.getData().settings.damageNumbers) return;
    const hexStr = isCrit ? '#ffe66d' : `#${tintHex.toString(16).padStart(6, '0')}`;
    this.spawnFloatingText(
      x + (Math.random() - 0.5) * 20,
      y - 16,
      isCrit ? `${dmg}!` : `${dmg}`,
      hexStr,
      isCrit ? 17 : 13
    );
  }

  private spawnFloatingText(x: number, y: number, text: string, color: string, size = 14): void {
    const item = this.floatPool.find((f) => !f.active);
    if (!item) return;
    item.active = true;
    item.life = 0;
    item.maxLife = 0.78;
    item.vy = -42;
    item.txt
      .setPosition(x, y)
      .setText(text)
      .setColor(color)
      .setFontSize(size)
      .setAlpha(1)
      .setVisible(true);
  }

  private stepFloatingPool(dt: number): void {
    for (const f of this.floatPool) {
      if (!f.active) continue;
      f.life += dt;
      f.txt.y += f.vy * dt;
      f.txt.setAlpha(Math.max(0, 1 - f.life / f.maxLife));
      if (f.life >= f.maxLife) {
        f.active = false;
        f.txt.setVisible(false);
      }
    }
  }

  private spawnLootOrb(x: number, y: number, gold: number, starlight: number): void {
    const orb = this.lootPool.find((l) => !l.active);
    if (!orb) {
      this.gold += gold;
      if (starlight > 0) {
        this.starlightEarnedThisRun += starlight;
        saveManager.addStarlight(starlight);
      }
      return;
    }
    orb.active = true;
    orb.gold = gold;
    orb.starlight = starlight;
    orb.age = 0;
    orb.vx = (Math.random() - 0.5) * 90;
    orb.vy = -40 - Math.random() * 50;
    orb.sprite
      .setTexture(starlight > 0 ? 'loot_starlight' : 'loot_gold')
      .setDisplaySize(starlight > 0 ? 26 : 20, starlight > 0 ? 26 : 20)
      .setPosition(x, y)
      .setAlpha(1)
      .setVisible(true);
  }

  private stepLootPool(dt: number): void {
    const ptr = this.input.activePointer;
    for (const l of this.lootPool) {
      if (!l.active) continue;
      l.age += dt;
      const targetX = l.age > 1.3 ? 750 : ptr.x;
      const targetY = l.age > 1.3 ? 30 : ptr.y;
      const dx = targetX - l.sprite.x;
      const dy = targetY - l.sprite.y;
      const dist = Math.hypot(dx, dy);

      if (dist < 170 || l.age > 1.3) {
        const pull = l.age > 1.3 ? 640 : 420;
        l.vx = (dx / Math.max(1, dist)) * pull;
        l.vy = (dy / Math.max(1, dist)) * pull;
      } else {
        l.vx *= 0.92;
        l.vy = l.vy * 0.92 + 55 * dt;
      }

      l.sprite.x += l.vx * dt;
      l.sprite.y += l.vy * dt;

      if (dist < 26 || l.age > 3.2) {
        this.collectLootOrb(l);
      }
    }
  }

  private collectLootOrb(l: LootParticle): void {
    if (!l.active) return;
    l.active = false;
    l.sprite.setVisible(false);
    this.gold += l.gold;
    saveManager.mutateStats((st) => {
      st.totalGoldEarned += l.gold;
    });
    if (l.starlight > 0) {
      this.starlightEarnedThisRun += l.starlight;
      saveManager.addStarlight(l.starlight);
    }
    soundEngine.playCoin();
    this.refreshHudValues();
  }

  private autoCollectRemainingLoot(): void {
    for (const l of this.lootPool) {
      if (l.active) this.collectLootOrb(l);
    }
  }

  private maybeShake(intensity: number, durationMs: number): void {
    if (saveManager.getData().settings.screenShake) {
      this.cameras.main.shake(durationMs, intensity);
    }
  }

  // ==========================================================================
  // PAUSE, VICTORY & DEFEAT MODALS
  // ==========================================================================
  private togglePauseModal(): void {
    if (this.runEnded) return;
    this.isPaused = !this.isPaused;
    this.modalContainer.removeAll(true);
    if (!this.isPaused) return;

    const { width, height } = this.scale;
    const dim = this.add
      .rectangle(width / 2, height / 2, width, height, 0x040711, 0.76)
      .setInteractive();
    const panel = drawOrnatePanel(this, width / 2 - 220, height / 2 - 190, 440, 380, 0xc89b45, 0.97);

    const title = this.add
      .text(width / 2, height / 2 - 144, 'EXPEDITION PAUSED', {
        fontFamily: FONT_TITLE,
        fontSize: '24px',
        fontStyle: 'bold',
        color: '#ffe38e',
      })
      .setOrigin(0.5);

    const bResume = createOrnateButton(this, {
      x: width / 2,
      y: height / 2 - 68,
      width: 280,
      height: 46,
      primary: true,
      label: 'RESUME DEFENSE',
      onClick: () => this.togglePauseModal(),
    });

    const bRestart = createOrnateButton(this, {
      x: width / 2,
      y: height / 2 - 8,
      width: 280,
      height: 44,
      label: 'RESTART EXPEDITION',
      onClick: () => {
        saveManager.saveActiveRun(null);
        this.scene.restart({ wardenId: this.wardenId, modeId: this.modeId, resumeSaved: false });
      },
    });

    const bTree = createOrnateButton(this, {
      x: width / 2,
      y: height / 2 + 52,
      width: 280,
      height: 44,
      label: 'SAVE & SKILL TREE',
      onClick: () => {
        this.persistActiveRun();
        this.scene.start('MetaScene', { tab: 'tree' });
      },
    });

    const bMenu = createOrnateButton(this, {
      x: width / 2,
      y: height / 2 + 112,
      width: 280,
      height: 44,
      label: 'SAVE & MAIN MENU',
      onClick: () => {
        this.persistActiveRun();
        this.scene.start('MainMenuScene');
      },
    });

    this.modalContainer.add([dim, panel, title, bResume, bRestart, bTree, bMenu]);
  }

  private handleRunEnd(victory: boolean): void {
    this.runEnded = true;
    this.inCombat = false;
    this.autoCollectRemainingLoot();
    saveManager.saveActiveRun(null);

    if (victory) {
      soundEngine.playVictory();
      const bonusStarlight = Math.round(25 * GAME_MODES[this.modeId].starlightMult);
      this.starlightEarnedThisRun += bonusStarlight;
      saveManager.addStarlight(bonusStarlight);
      saveManager.mutateStats((st) => {
        st.victories += 1;
        if (this.wave > st.highestWave) st.highestWave = this.wave;
      });
    } else {
      soundEngine.playDefeat();
      saveManager.mutateStats((st) => {
        if (this.wave > st.highestWave) st.highestWave = this.wave;
      });
    }

    this.modalContainer.removeAll(true);
    const { width, height } = this.scale;

    const dim = this.add
      .rectangle(width / 2, height / 2, width, height, 0x040711, 0.82)
      .setInteractive();
    const panel = drawOrnatePanel(
      this,
      width / 2 - 260,
      height / 2 - 220,
      520,
      440,
      victory ? 0xffd166 : 0xff4d6d,
      0.97
    );

    const header = this.add
      .text(width / 2, height / 2 - 172, victory ? 'SUNSPIRE TRIUMPHANT!' : 'THE BASTION HAS FALLEN', {
        fontFamily: FONT_TITLE,
        fontSize: '28px',
        fontStyle: 'bold',
        color: victory ? '#ffe79e' : '#ff758f',
      })
      .setOrigin(0.5);

    const sub = this.add
      .text(
        width / 2,
        height / 2 - 134,
        `${WARDENS[this.wardenId].name}  •  ${GAME_MODES[this.modeId].name}`,
        {
          fontFamily: FONT_BODY,
          fontSize: '14px',
          color: '#7ef9ff',
        }
      )
      .setOrigin(0.5);

    const statsSummary = this.add
      .text(
        width / 2,
        height / 2 - 48,
        `Waves Reached: ${this.wave}\nPeak Pulse Multiplier: x${this.peakMultThisRun.toFixed(2)}\nRelics Attuned: ${this.relics.length}\nStarlight Harvested: +${this.starlightEarnedThisRun}`,
        {
          fontFamily: FONT_TITLE,
          fontSize: '16px',
          color: '#e8f1ff',
          align: 'center',
          lineSpacing: 10,
        }
      )
      .setOrigin(0.5);

    this.modalContainer.add([dim, panel, header, sub, statsSummary]);

    if (victory) {
      const bEndless = createOrnateButton(this, {
        x: width / 2,
        y: height / 2 + 66,
        width: 320,
        height: 46,
        primary: true,
        label: 'CONTINUE INTO ENDLESS',
        onClick: () => {
          this.runEnded = false;
          this.endlessContinued = true;
          this.wave += 1;
          this.modalContainer.removeAll(true);
          this.buildTopHud();
          this.rebuildBottomDock();
          this.persistActiveRun();
        },
      });
      this.modalContainer.add(bEndless);
    } else {
      const bRetry = createOrnateButton(this, {
        x: width / 2,
        y: height / 2 + 66,
        width: 320,
        height: 46,
        primary: true,
        label: 'LAUNCH NEW EXPEDITION',
        onClick: () => {
          this.scene.restart({ wardenId: this.wardenId, modeId: this.modeId, resumeSaved: false });
        },
      });
      this.modalContainer.add(bRetry);
    }

    const bSkills = createOrnateButton(this, {
      x: width / 2,
      y: height / 2 + 122,
      width: 320,
      height: 42,
      iconKey: 'icon_tree',
      label: 'INVEST STARLIGHT IN TREE',
      onClick: () => this.scene.start('MetaScene', { tab: 'tree' }),
    });

    const bMenu = createOrnateButton(this, {
      x: width / 2,
      y: height / 2 + 174,
      width: 320,
      height: 42,
      label: 'MAIN MENU',
      onClick: () => this.scene.start('MainMenuScene'),
    });

    this.modalContainer.add([bSkills, bMenu]);
  }
}
