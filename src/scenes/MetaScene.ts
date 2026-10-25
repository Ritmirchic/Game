import Phaser from 'phaser';
import { saveManager, soundEngine } from '../core/appContext';
import {
  ACHIEVEMENTS,
  ENEMIES,
  MODULE_LIST,
  RELIC_LIST,
  SKILL_TREE,
  WEATHERS,
} from '../core/content';
import {
  createOrnateButton,
  drawOrnatePanel,
  FONT_BODY,
  FONT_TITLE,
  showAchievementToast,
  TooltipOverlay,
} from '../ui/uiFactory';

type MetaTab = 'tree' | 'codex' | 'achievements' | 'settings';
type CodexCategory = 'modules' | 'relics' | 'enemies' | 'weather';

export class MetaScene extends Phaser.Scene {
  private currentTab: MetaTab = 'tree';
  private codexCat: CodexCategory = 'modules';
  private achPage = 0;
  private confirmReset = false;
  private tooltip!: TooltipOverlay;

  constructor() {
    super('MetaScene');
  }

  public init(data?: { tab?: MetaTab }): void {
    this.currentTab = data?.tab ?? 'tree';
    this.confirmReset = false;
  }

  public create(): void {
    this.renderAll();
  }

  private renderAll(): void {
    this.children.removeAll(true);
    const { width, height } = this.scale;
    const data = saveManager.getData();

    this.add.image(width / 2, height / 2, 'menu_hero_bg').setDisplaySize(width, height).setAlpha(0.24);
    this.tooltip = new TooltipOverlay(this);

    // Top Navigation Bar
    drawOrnatePanel(this, 24, 14, width - 48, 56, 0xc89b45, 0.94);

    createOrnateButton(this, {
      x: 104,
      y: 42,
      width: 132,
      height: 38,
      label: 'MAIN MENU',
      fontSize: 13,
      onClick: () => this.scene.start('MainMenuScene'),
    });

    const tabs: Array<{ id: MetaTab; label: string; icon: string }> = [
      { id: 'tree', label: 'SKILL TREE', icon: 'icon_tree' },
      { id: 'codex', label: 'CODEX', icon: 'icon_codex' },
      { id: 'achievements', label: 'ACHIEVEMENTS', icon: 'icon_trophy' },
      { id: 'settings', label: 'SETTINGS', icon: 'icon_settings' },
    ];

    tabs.forEach((t, idx) => {
      createOrnateButton(this, {
        x: 298 + idx * 204,
        y: 42,
        width: 192,
        height: 38,
        primary: this.currentTab === t.id,
        iconKey: t.icon,
        fontSize: 13,
        label: t.label,
        onClick: () => {
          this.currentTab = t.id;
          this.confirmReset = false;
          this.renderAll();
        },
      });
    });

    // Starlight Currency Display
    this.add.image(width - 178, 42, 'icon_starlight').setDisplaySize(24, 24);
    this.add
      .text(width - 156, 42, `${data.starlight} Starlight`, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        fontStyle: 'bold',
        color: '#7ef9ff',
      })
      .setOrigin(0, 0.5);

    // Content Frame
    drawOrnatePanel(this, 24, 82, width - 48, height - 102, 0x5c79b8, 0.9);

    if (this.currentTab === 'tree') {
      this.renderSkillTree();
    } else if (this.currentTab === 'codex') {
      this.renderCodex();
    } else if (this.currentTab === 'achievements') {
      this.renderAchievements();
    } else {
      this.renderSettings();
    }
  }

  private renderSkillTree(): void {
    const areaX = 48;
    const areaY = 98;
    const areaW = 1184;
    const areaH = 580;

    // Branch Titles
    this.add
      .text(areaX + areaW * 0.18, areaY + 18, 'SOLAR ENGINEERING', {
        fontFamily: FONT_TITLE,
        fontSize: '16px',
        fontStyle: 'bold',
        color: '#ffd166',
      })
      .setOrigin(0.5);

    this.add
      .text(areaX + areaW * 0.5, areaY + 18, 'BASTION FORTIFICATION', {
        fontFamily: FONT_TITLE,
        fontSize: '16px',
        fontStyle: 'bold',
        color: '#7ef9ff',
      })
      .setOrigin(0.5);

    this.add
      .text(areaX + areaW * 0.82, areaY + 18, 'ASTRAL ECONOMY', {
        fontFamily: FONT_TITLE,
        fontSize: '16px',
        fontStyle: 'bold',
        color: '#d982ff',
      })
      .setOrigin(0.5);

    this.add
      .text(
        areaX + areaW * 0.5,
        areaY + 42,
        'Click any unlocked constellation node to invest permanent Starlight upgrades across all future runs.',
        {
          fontFamily: FONT_BODY,
          fontSize: '12.5px',
          color: '#b8c8eb',
        }
      )
      .setOrigin(0.5);

    // Draw prerequisite lines first
    const lineGfx = this.add.graphics();
    for (const node of SKILL_TREE) {
      if (!node.prereq) continue;
      const parent = SKILL_TREE.find((n) => n.id === node.prereq);
      if (!parent) continue;
      const x1 = areaX + parent.x * areaW;
      const y1 = areaY + parent.y * areaH;
      const x2 = areaX + node.x * areaW;
      const y2 = areaY + node.y * areaH;
      const unlocked = saveManager.getSkillRank(parent.id) > 0;
      lineGfx.lineStyle(3, unlocked ? 0xf2c464 : 0x324066, unlocked ? 0.9 : 0.6);
      lineGfx.lineBetween(x1, y1, x2, y2);
    }

    // Draw Skill Nodes
    for (const node of SKILL_TREE) {
      const nx = areaX + node.x * areaW;
      const ny = areaY + node.y * areaH;
      const rank = saveManager.getSkillRank(node.id);
      const maxed = rank >= node.maxRank;
      const canBuy = saveManager.canUpgradeSkill(node.id);
      const prereqMet = !node.prereq || saveManager.getSkillRank(node.prereq) > 0;
      const cost = saveManager.getSkillCost(node.id);

      const branchColor =
        node.branch === 'solar' ? 0xffbe3b : node.branch === 'bastion' ? 0x52e5ff : 0xd982ff;
      const g = this.add.graphics();
      g.fillStyle(maxed ? 0x2c2512 : prereqMet ? 0x152040 : 0x0d1224, 0.96);
      g.fillRoundedRect(nx - 88, ny - 36, 176, 72, 10);
      g.lineStyle(maxed ? 2.6 : canBuy ? 2.4 : 1.5, maxed ? 0xffdf73 : canBuy ? branchColor : 0x415078, 1);
      g.strokeRoundedRect(nx - 88, ny - 36, 176, 72, 10);

      this.add
        .text(nx, ny - 18, node.name, {
          fontFamily: FONT_TITLE,
          fontSize: '12.5px',
          fontStyle: 'bold',
          color: prereqMet ? '#fff3c4' : '#7b88a8',
        })
        .setOrigin(0.5);

      this.add
        .text(nx, ny + 2, `Rank ${rank} / ${node.maxRank}`, {
          fontFamily: FONT_BODY,
          fontSize: '11.5px',
          color: maxed ? '#69ff97' : '#8ef9ff',
        })
        .setOrigin(0.5);

      const costLabel = maxed ? 'MAXED' : `${cost} Starlight`;
      this.add
        .text(nx, ny + 20, costLabel, {
          fontFamily: FONT_BODY,
          fontSize: '11px',
          fontStyle: 'bold',
          color: maxed ? '#ffe38e' : canBuy ? '#ffd166' : '#8a96b8',
        })
        .setOrigin(0.5);

      const zone = this.add
        .zone(nx, ny, 176, 72)
        .setInteractive({ useHandCursor: canBuy });

      zone.on('pointerover', () => {
        soundEngine.playHover();
        this.tooltip.show(
          nx - 140,
          ny - 42,
          node.name,
          `Rank ${rank}/${node.maxRank}  •  ${maxed ? 'Complete' : `Cost: ${cost} Starlight`}`,
          `${node.description}${!prereqMet ? '\n(Requires previous node in branch)' : ''}`
        );
      });
      zone.on('pointerout', () => this.tooltip.hide());
      zone.on('pointerdown', () => {
        const res = saveManager.upgradeSkill(node.id);
        if (res.ok) {
          soundEngine.playMerge(2);
          this.renderAll();
          res.unlockedAchievements.forEach((a, i) => showAchievementToast(this, a, i));
        } else {
          soundEngine.playHover();
        }
      });
    }
  }

  private renderCodex(): void {
    const data = saveManager.getData();
    const subTabs: Array<{ id: CodexCategory; label: string }> = [
      { id: 'modules', label: `MODULES (${data.discoveredModules.length}/${MODULE_LIST.length})` },
      { id: 'relics', label: `RELICS (${data.discoveredRelics.length}/${RELIC_LIST.length})` },
      {
        id: 'enemies',
        label: `VOIDKIN (${data.discoveredEnemies.length}/${Object.keys(ENEMIES).length})`,
      },
      { id: 'weather', label: `ANOMALIES (${Object.keys(WEATHERS).length})` },
    ];

    subTabs.forEach((st, idx) => {
      createOrnateButton(this, {
        x: 185 + idx * 265,
        y: 118,
        width: 245,
        height: 36,
        primary: this.codexCat === st.id,
        fontSize: 12,
        label: st.label,
        onClick: () => {
          this.codexCat = st.id;
          this.renderAll();
        },
      });
    });

    if (this.codexCat === 'modules') {
      const cols = 5;
      const cw = 222;
      const ch = 162;
      MODULE_LIST.forEach((m, i) => {
        const col = i % cols;
        const row = Math.floor(i / cols);
        const x = 52 + col * (cw + 12);
        const y = 152 + row * (ch + 12);
        const known = data.discoveredModules.includes(m.id);

        const g = this.add.graphics();
        g.fillStyle(0x121b36, 0.95);
        g.fillRoundedRect(x, y, cw, ch, 9);
        g.lineStyle(1.6, m.rarity === 'rare' ? 0xd982ff : 0x5c79b8, 0.9);
        g.strokeRoundedRect(x, y, cw, ch, 9);

        const sp = this.add.image(x + 36, y + 38, m.spriteKey).setDisplaySize(52, 52);
        if (!known) sp.setTint(0x28324e);

        this.add.text(x + 68, y + 14, m.name, {
          fontFamily: FONT_TITLE,
          fontSize: '13px',
          fontStyle: 'bold',
          color: known ? '#ffe79e' : '#8b99bd',
          wordWrap: { width: cw - 76 },
        });
        this.add.text(x + 68, y + 46, m.tags.join(' • '), {
          fontFamily: FONT_BODY,
          fontSize: '10.5px',
          color: '#7ef9ff',
        });
        this.add.text(x + 12, y + 74, m.shortDesc, {
          fontFamily: FONT_BODY,
          fontSize: '11px',
          color: '#d4e2ff',
          wordWrap: { width: cw - 24 },
          lineSpacing: 2,
        });
      });
    } else if (this.codexCat === 'relics') {
      const cols = 6;
      const cw = 186;
      const ch = 158;
      RELIC_LIST.forEach((r, i) => {
        const col = i % cols;
        const row = Math.floor(i / cols);
        const x = 50 + col * (cw + 10);
        const y = 152 + row * (ch + 12);
        const known = data.discoveredRelics.includes(r.id);

        const border =
          r.rarity === 'legendary' ? 0xffd166 : r.rarity === 'rare' ? 0xd982ff : 0x5c79b8;
        const g = this.add.graphics();
        g.fillStyle(0x121b36, 0.95);
        g.fillRoundedRect(x, y, cw, ch, 9);
        g.lineStyle(1.6, border, 0.95);
        g.strokeRoundedRect(x, y, cw, ch, 9);

        const sp = this.add.image(x + 30, y + 32, r.spriteKey).setDisplaySize(42, 42);
        if (!known) sp.setTint(0x3a4668);

        this.add.text(x + 56, y + 12, r.name, {
          fontFamily: FONT_TITLE,
          fontSize: '12px',
          fontStyle: 'bold',
          color: '#ffe79e',
          wordWrap: { width: cw - 64 },
        });
        this.add.text(x + 56, y + 42, r.rarity.toUpperCase(), {
          fontFamily: FONT_BODY,
          fontSize: '10px',
          color: '#7ef9ff',
        });
        this.add.text(x + 10, y + 64, r.description, {
          fontFamily: FONT_BODY,
          fontSize: '10.8px',
          color: '#d4e2ff',
          wordWrap: { width: cw - 20 },
          lineSpacing: 2,
        });
      });
    } else if (this.codexCat === 'enemies') {
      const list = Object.values(ENEMIES);
      const cols = 4;
      const cw = 282;
      const ch = 158;
      list.forEach((en, i) => {
        const col = i % cols;
        const row = Math.floor(i / cols);
        const x = 52 + col * (cw + 14);
        const y = 152 + row * (ch + 14);

        const g = this.add.graphics();
        g.fillStyle(en.isBoss ? 0x221236 : 0x121b36, 0.95);
        g.fillRoundedRect(x, y, cw, ch, 9);
        g.lineStyle(1.8, en.isBoss ? 0xffd166 : 0x5c79b8, 0.95);
        g.strokeRoundedRect(x, y, cw, ch, 9);

        const spr = this.add.sprite(x + 40, y + 44, en.spriteKey, 0).setDisplaySize(58, 58);
        spr.play(`${en.spriteKey}_walk`);

        this.add.text(x + 76, y + 14, en.name, {
          fontFamily: FONT_TITLE,
          fontSize: '13.5px',
          fontStyle: 'bold',
          color: en.isBoss ? '#ffd166' : '#ffe79e',
          wordWrap: { width: cw - 86 },
        });
        this.add.text(
          x + 76,
          y + 48,
          `HP ${en.baseHp}  •  Armor ${en.armor}  •  Shield ${en.shield}`,
          {
            fontFamily: FONT_BODY,
            fontSize: '11px',
            color: '#7ef9ff',
          }
        );
        this.add.text(x + 14, y + 82, en.description, {
          fontFamily: FONT_BODY,
          fontSize: '11.5px',
          color: '#d4e2ff',
          wordWrap: { width: cw - 28 },
        });
      });
    } else {
      Object.values(WEATHERS).forEach((w, i) => {
        const x = 64;
        const y = 158 + i * 98;
        const g = this.add.graphics();
        g.fillStyle(0x131d3b, 0.95);
        g.fillRoundedRect(x, y, 1150, 82, 10);
        g.lineStyle(1.8, 0x7ef9ff, 0.7);
        g.strokeRoundedRect(x, y, 1150, 82, 10);

        this.add.image(x + 44, y + 41, w.iconKey).setDisplaySize(42, 42);
        this.add.text(x + 84, y + 16, w.name, {
          fontFamily: FONT_TITLE,
          fontSize: '17px',
          fontStyle: 'bold',
          color: '#ffe79e',
        });
        this.add.text(x + 84, y + 44, w.description, {
          fontFamily: FONT_BODY,
          fontSize: '13.5px',
          color: '#d8e6ff',
        });
      });
    }
  }

  private renderAchievements(): void {
    const data = saveManager.getData();

    // Left Side: Achievements Grid (10 per page, 2 pages)
    const perPage = 10;
    const slice = ACHIEVEMENTS.slice(this.achPage * perPage, (this.achPage + 1) * perPage);

    this.add.text(54, 102, `SANCTUM ACHIEVEMENTS (${data.achievements.length} / ${ACHIEVEMENTS.length})`, {
      fontFamily: FONT_TITLE,
      fontSize: '17px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });

    createOrnateButton(this, {
      x: 560,
      y: 112,
      width: 110,
      height: 30,
      primary: this.achPage === 0,
      fontSize: 12,
      label: 'PAGE 1',
      onClick: () => {
        this.achPage = 0;
        this.renderAll();
      },
    });
    createOrnateButton(this, {
      x: 682,
      y: 112,
      width: 110,
      height: 30,
      primary: this.achPage === 1,
      fontSize: 12,
      label: 'PAGE 2',
      onClick: () => {
        this.achPage = 1;
        this.renderAll();
      },
    });

    slice.forEach((ach, idx) => {
      const col = idx % 2;
      const row = Math.floor(idx / 2);
      const x = 50 + col * 392;
      const y = 140 + row * 104;
      const unlocked = data.achievements.includes(ach.id);

      const g = this.add.graphics();
      g.fillStyle(unlocked ? 0x1d2b4f : 0x0f1529, 0.95);
      g.fillRoundedRect(x, y, 378, 92, 9);
      g.lineStyle(unlocked ? 2.2 : 1.3, unlocked ? 0xffd166 : 0x3b4970, 1);
      g.strokeRoundedRect(x, y, 378, 92, 9);

      const ic = this.add.image(x + 32, y + 46, 'icon_trophy').setDisplaySize(34, 34);
      if (!unlocked) ic.setTint(0x475375);

      this.add.text(x + 60, y + 14, ach.name, {
        fontFamily: FONT_TITLE,
        fontSize: '14px',
        fontStyle: 'bold',
        color: unlocked ? '#ffe79e' : '#8b99bd',
      });
      this.add.text(x + 60, y + 36, ach.description, {
        fontFamily: FONT_BODY,
        fontSize: '11.5px',
        color: '#d4e2ff',
        wordWrap: { width: 304 },
      });
      this.add.text(
        x + 60,
        y + 68,
        unlocked ? `UNLOCKED (+${ach.rewardStarlight} Starlight)` : `Reward: +${ach.rewardStarlight} Starlight`,
        {
          fontFamily: FONT_BODY,
          fontSize: '11px',
          fontStyle: 'bold',
          color: unlocked ? '#69ff97' : '#7ef9ff',
        }
      );
    });

    // Right Side: Lifetime Statistics Panel
    const sx = 846;
    const sy = 102;
    drawOrnatePanel(this, sx, sy, 384, 558, 0xc89b45, 0.95);
    this.add.text(sx + 24, sy + 20, 'LIFETIME TELEMETRY', {
      fontFamily: FONT_TITLE,
      fontSize: '17px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });

    const st = data.stats;
    const rows: Array<[string, string]> = [
      ['Expeditions Launched', `${st.runsStarted}`],
      ['Campaign / Trial Victories', `${st.victories}`],
      ['Highest Wave Reached', `${st.highestWave}`],
      ['Voidkin Vanquished', `${st.enemiesVanquished}`],
      ['Void Titans Slain', `${st.bossesVanquished}`],
      ['Peak Pulse Multiplier', `x${st.highestPulseMult.toFixed(2)}`],
      ['Highest Single Hit', `${st.highestSingleHit}`],
      ['Modules Merged', `${st.modulesMerged}`],
      ['Aether Pulses Emitted', `${st.pulsesEmitted}`],
      ['Total Gold Minted', `${st.totalGoldEarned}`],
      ['Total Starlight Earned', `${st.totalStarlightEarned}`],
    ];

    rows.forEach(([k, v], i) => {
      const ry = sy + 64 + i * 42;
      this.add.text(sx + 24, ry, k, {
        fontFamily: FONT_BODY,
        fontSize: '13.5px',
        color: '#c8d8ff',
      });
      this.add
        .text(sx + 360, ry, v, {
          fontFamily: FONT_TITLE,
          fontSize: '14.5px',
          fontStyle: 'bold',
          color: '#7ef9ff',
        })
        .setOrigin(1, 0);
    });
  }

  private renderSettings(): void {
    const s = saveManager.getData().settings;
    const cx = 140;

    this.add.text(cx, 112, 'AUDIO, VISUAL & SAVE CONFIGURATION', {
      fontFamily: FONT_TITLE,
      fontSize: '20px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });

    const makeSliderRow = (
      y: number,
      label: string,
      value: number,
      onChange: (nextVal: number) => void
    ) => {
      this.add.text(cx, y, label, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        color: '#e5efff',
      });

      createOrnateButton(this, {
        x: cx + 310,
        y: y + 10,
        width: 54,
        height: 34,
        label: '-',
        onClick: () => {
          onChange(Math.max(0, Math.round((value - 0.1) * 10) / 10));
        },
      });

      const barX = cx + 354;
      const barW = 280;
      const bar = this.add.graphics();
      bar.fillStyle(0x0f162b, 1);
      bar.fillRoundedRect(barX, y - 2, barW, 24, 6);
      bar.fillStyle(0x52e5ff, 1);
      bar.fillRoundedRect(barX + 3, y + 1, Math.max(4, (barW - 6) * value), 18, 4);
      bar.lineStyle(1.5, 0xc89b45, 0.9);
      bar.strokeRoundedRect(barX, y - 2, barW, 24, 6);

      const barZone = this.add
        .zone(barX + barW / 2, y + 10, barW, 28)
        .setInteractive({ useHandCursor: true });
      barZone.on('pointerdown', (ptr: Phaser.Input.Pointer) => {
        const ratio = Math.max(0, Math.min(1, (ptr.x - barX) / barW));
        onChange(Math.round(ratio * 20) / 20);
      });

      createOrnateButton(this, {
        x: barX + barW + 44,
        y: y + 10,
        width: 54,
        height: 34,
        label: '+',
        onClick: () => {
          onChange(Math.min(1, Math.round((value + 0.1) * 10) / 10));
        },
      });

      this.add.text(barX + barW + 90, y, `${Math.round(value * 100)}%`, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        color: '#ffe38e',
      });
    };

    makeSliderRow(172, 'Master Volume', s.masterVolume, (v) => {
      saveManager.updateSettings({ masterVolume: v });
      soundEngine.updateSettings(saveManager.getData().settings);
      soundEngine.playClick();
      this.renderAll();
    });

    makeSliderRow(236, 'Sound Effects Volume', s.sfxVolume, (v) => {
      saveManager.updateSettings({ sfxVolume: v });
      soundEngine.updateSettings(saveManager.getData().settings);
      soundEngine.playClick();
      this.renderAll();
    });

    makeSliderRow(300, 'Ambient Music Volume', s.musicVolume, (v) => {
      saveManager.updateSettings({ musicVolume: v });
      soundEngine.updateSettings(saveManager.getData().settings);
      soundEngine.playClick();
      this.renderAll();
    });

    // Toggles
    const toggles: Array<{ label: string; active: boolean; toggle: () => void }> = [
      {
        label: `Mute All Audio: ${s.muted ? 'ON' : 'OFF'}`,
        active: !s.muted,
        toggle: () => {
          saveManager.updateSettings({ muted: !s.muted });
          soundEngine.updateSettings(saveManager.getData().settings);
          this.renderAll();
        },
      },
      {
        label: `Impact Screen Shake: ${s.screenShake ? 'ON' : 'OFF'}`,
        active: s.screenShake,
        toggle: () => {
          saveManager.updateSettings({ screenShake: !s.screenShake });
          this.renderAll();
        },
      },
      {
        label: `Floating Damage Numbers: ${s.damageNumbers ? 'ON' : 'OFF'}`,
        active: s.damageNumbers,
        toggle: () => {
          saveManager.updateSettings({ damageNumbers: !s.damageNumbers });
          this.renderAll();
        },
      },
    ];

    toggles.forEach((tg, i) => {
      createOrnateButton(this, {
        x: cx + 155 + i * 335,
        y: 392,
        width: 310,
        height: 44,
        primary: tg.active,
        fontSize: 13.5,
        label: tg.label,
        onClick: tg.toggle,
      });
    });

    // Default Game Speed
    this.add.text(cx, 468, 'Default Simulation Speed:', {
      fontFamily: FONT_TITLE,
      fontSize: '15px',
      color: '#e5efff',
    });

    ([1, 1.5, 2] as const).forEach((spd, idx) => {
      createOrnateButton(this, {
        x: cx + 340 + idx * 145,
        y: 478,
        width: 128,
        height: 38,
        primary: s.defaultSpeed === spd,
        fontSize: 14,
        label: `${spd}x SPEED`,
        onClick: () => {
          saveManager.updateSettings({ defaultSpeed: spd });
          this.renderAll();
        },
      });
    });

    // Save Reset with Confirmation
    createOrnateButton(this, {
      x: cx + 190,
      y: 590,
      width: 380,
      height: 44,
      primary: this.confirmReset,
      fontSize: 13.5,
      label: this.confirmReset ? 'CONFIRM PERMANENT SAVE WIPE' : 'RESET ALL SAVE PROGRESSION',
      onClick: () => {
        if (!this.confirmReset) {
          this.confirmReset = true;
          this.renderAll();
        } else {
          saveManager.resetAll();
          soundEngine.updateSettings(saveManager.getData().settings);
          this.confirmReset = false;
          this.renderAll();
        }
      },
    });
  }
}
