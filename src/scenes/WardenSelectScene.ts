import Phaser from 'phaser';
import { saveManager, soundEngine } from '../core/appContext';
import { GAME_MODE_LIST, GAME_MODES, MODULES, WARDEN_LIST, WARDENS } from '../core/content';
import { GameModeId, WardenId } from '../core/types';
import {
  createOrnateButton,
  drawOrnatePanel,
  FONT_BODY,
  FONT_TITLE,
  showAchievementToast,
} from '../ui/uiFactory';

export class WardenSelectScene extends Phaser.Scene {
  private selectedWarden: WardenId = 'lyra';
  private selectedMode: GameModeId = 'campaign';

  constructor() {
    super('WardenSelectScene');
  }

  public create(): void {
    this.renderScene();
    this.cameras.main.fadeIn(200, 5, 8, 18);
  }

  private renderScene(): void {
    this.children.removeAll(true);
    const { width, height } = this.scale;
    const data = saveManager.getData();

    this.add.image(width / 2, height / 2, 'menu_hero_bg').setDisplaySize(width, height).setAlpha(0.32);

    // Header
    drawOrnatePanel(this, 24, 16, width - 48, 56, 0xc89b45, 0.92);
    createOrnateButton(this, {
      x: 112,
      y: 44,
      width: 146,
      height: 38,
      label: 'BACK',
      fontSize: 14,
      onClick: () => this.scene.start('MainMenuScene'),
    });

    this.add
      .text(width / 2, 44, 'CHOOSE YOUR BASTION WARDEN & EXPEDITION TRIAL', {
        fontFamily: FONT_TITLE,
        fontSize: '20px',
        fontStyle: 'bold',
        color: '#ffe38e',
      })
      .setOrigin(0.5);

    this.add.image(width - 175, 44, 'icon_starlight').setDisplaySize(24, 24);
    this.add
      .text(width - 154, 44, `${data.starlight} Starlight`, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        fontStyle: 'bold',
        color: '#7ef9ff',
      })
      .setOrigin(0, 0.5);

    // 4 Warden Cards Row (y: 88..430)
    const cardW = 286;
    const cardH = 342;
    const startX = 42;
    const gapX = 18;

    WARDEN_LIST.forEach((w, idx) => {
      const x = startX + idx * (cardW + gapX);
      const y = 88;
      const unlocked = data.unlockedWardens.includes(w.id);
      const isSelected = this.selectedWarden === w.id;

      const borderCol = isSelected ? 0xffdf73 : unlocked ? 0x5c79b8 : 0x39425e;
      const panel = drawOrnatePanel(this, x, y, cardW, cardH, borderCol, isSelected ? 0.96 : 0.88);
      if (isSelected) {
        panel.lineStyle(3, 0x7ef9ff, 0.85);
        panel.strokeRoundedRect(x - 2, y - 2, cardW + 4, cardH + 4, 14);
      }

      const portrait = this.add
        .image(x + cardW / 2, y + 86, w.portraitKey)
        .setDisplaySize(134, 134);
      if (!unlocked) portrait.setTint(0x555f7a);

      // Portrait frame border
      const pf = this.add.graphics();
      pf.lineStyle(2.5, isSelected ? 0xffd86b : 0x8a9dc7, 1);
      pf.strokeRect(x + cardW / 2 - 68, y + 18, 136, 136);

      this.add
        .text(x + cardW / 2, y + 168, w.name, {
          fontFamily: FONT_TITLE,
          fontSize: '16px',
          fontStyle: 'bold',
          color: unlocked ? '#ffe79e' : '#9aa6c4',
        })
        .setOrigin(0.5);

      this.add
        .text(x + cardW / 2, y + 188, w.title, {
          fontFamily: FONT_BODY,
          fontSize: '11.5px',
          color: '#7ef9ff',
        })
        .setOrigin(0.5);

      this.add.text(x + 14, y + 206, `Trait — ${w.passiveName}:`, {
        fontFamily: FONT_BODY,
        fontSize: '11.5px',
        fontStyle: 'bold',
        color: '#ffd166',
      });
      this.add.text(x + 14, y + 222, w.passiveDesc, {
        fontFamily: FONT_BODY,
        fontSize: '11px',
        color: '#d8e4ff',
        wordWrap: { width: cardW - 28 },
      });

      this.add.image(x + 24, y + 275, w.ultIconKey).setDisplaySize(22, 22);
      this.add.text(x + 40, y + 266, `Ult — ${w.ultName}`, {
        fontFamily: FONT_BODY,
        fontSize: '11.5px',
        fontStyle: 'bold',
        color: '#a8f9ff',
      });
      this.add.text(x + 14, y + 284, w.ultDesc, {
        fontFamily: FONT_BODY,
        fontSize: '10.5px',
        color: '#b8c8eb',
        wordWrap: { width: cardW - 28 },
      });

      // Interactive Selection or Unlock Button
      if (unlocked) {
        const hitZone = this.add
          .zone(x + cardW / 2, y + cardH / 2, cardW, cardH)
          .setInteractive({ useHandCursor: true });
        hitZone.on('pointerdown', () => {
          soundEngine.playClick();
          this.selectedWarden = w.id;
          this.renderScene();
        });
      } else {
        const canAfford = data.starlight >= w.unlockCost;
        createOrnateButton(this, {
          x: x + cardW / 2,
          y: y + cardH - 24,
          width: 220,
          height: 36,
          primary: canAfford,
          iconKey: 'icon_starlight',
          fontSize: 13,
          label: `UNLOCK (${w.unlockCost} Starlight)`,
          onClick: () => {
            const res = saveManager.unlockWarden(w.id);
            if (res.ok) {
              this.selectedWarden = w.id;
              this.renderScene();
              res.unlockedAchievements.forEach((a, i) => showAchievementToast(this, a, i));
            }
          },
        });
      }
    });

    // Game Mode / Trial Selector Section (y: 446..608)
    drawOrnatePanel(this, 42, 444, width - 84, 166, 0x6482c4, 0.9);
    this.add.text(62, 456, 'SELECT EXPEDITION PROTOCOL', {
      fontFamily: FONT_TITLE,
      fontSize: '15px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });

    const modeCardW = 224;
    GAME_MODE_LIST.forEach((m, idx) => {
      const mx = 58 + idx * (modeCardW + 10);
      const my = 484;
      const active = this.selectedMode === m.id;
      const g = this.add.graphics();
      g.fillStyle(active ? 0x243666 : 0x111933, 0.95);
      g.fillRoundedRect(mx, my, modeCardW, 74, 9);
      g.lineStyle(active ? 2.5 : 1.5, active ? 0xffd86b : 0x4b5f94, 1);
      g.strokeRoundedRect(mx, my, modeCardW, 74, 9);

      this.add.text(mx + 12, my + 10, m.name, {
        fontFamily: FONT_TITLE,
        fontSize: '13.5px',
        fontStyle: 'bold',
        color: active ? '#ffe89c' : '#dce8ff',
      });
      this.add.text(mx + 12, my + 30, m.subtitle, {
        fontFamily: FONT_BODY,
        fontSize: '11px',
        color: '#7ef9ff',
      });
      this.add.text(mx + 12, my + 48, `Starlight x${m.starlightMult.toFixed(2)}`, {
        fontFamily: FONT_BODY,
        fontSize: '11px',
        color: '#ffd166',
      });

      const hz = this.add
        .zone(mx + modeCardW / 2, my + 37, modeCardW, 74)
        .setInteractive({ useHandCursor: true });
      hz.on('pointerdown', () => {
        soundEngine.playClick();
        this.selectedMode = m.id;
        this.renderScene();
      });
    });

    const activeMode = GAME_MODES[this.selectedMode];
    const activeWarden = WARDENS[this.selectedWarden];
    const starterNames = activeWarden.startingModules.map((s) => MODULES[s.defId].name).join(' + ');

    this.add.text(
      62,
      572,
      `Mode Rules: ${activeMode.rulesDesc}   |   Starting Loadout: ${starterNames}`,
      {
        fontFamily: FONT_BODY,
        fontSize: '12.5px',
        color: '#d8e6ff',
      }
    );

    // Launch Button
    createOrnateButton(this, {
      x: width / 2,
      y: 662,
      width: 360,
      height: 54,
      primary: true,
      iconKey: 'ability_surge',
      fontSize: 19,
      label: 'IGNITE THE SUNSPIRE',
      onClick: () => {
        saveManager.saveActiveRun(null);
        this.cameras.main.fadeOut(220, 5, 8, 18);
        this.cameras.main.once('camerafadeoutcomplete', () => {
          this.scene.start('GameScene', {
            wardenId: this.selectedWarden,
            modeId: this.selectedMode,
            resumeSaved: false,
          });
        });
      },
    });
  }
}
