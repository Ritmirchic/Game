import Phaser from 'phaser';
import { saveManager, soundEngine } from '../core/appContext';
import { ACHIEVEMENTS } from '../core/content';
import { createOrnateButton, drawOrnatePanel, FONT_BODY, FONT_TITLE } from '../ui/uiFactory';

export class MainMenuScene extends Phaser.Scene {
  constructor() {
    super('MainMenuScene');
  }

  public create(): void {
    const { width, height } = this.scale;
    const data = saveManager.getData();

    soundEngine.setMusicMood('menu');
    this.input.once('pointerdown', () => soundEngine.unlock());
    this.input.keyboard?.once('keydown', () => soundEngine.unlock());

    // Painted Hero Background with subtle slow breath zoom
    const bg = this.add.image(width / 2, height / 2, 'menu_hero_bg').setDisplaySize(width, height);
    this.tweens.add({
      targets: bg,
      scaleX: bg.scaleX * 1.035,
      scaleY: bg.scaleY * 1.035,
      duration: 9500,
      yoyo: true,
      repeat: -1,
      ease: 'Sine.easeInOut',
    });

    // Subtle dark gradient vignette on left side for crisp menu legibility
    const leftShade = this.add.graphics();
    leftShade.fillGradientStyle(0x050813, 0x050813, 0x050813, 0x050813, 0.88, 0.25, 0.88, 0.25);
    leftShade.fillRect(0, 0, 620, height);

    // Drifting ambient starlight particles
    this.add.particles(0, 0, 'vfx_spark', {
      x: { min: 40, max: width - 40 },
      y: { min: height + 10, max: height + 30 },
      speedY: { min: -38, max: -14 },
      speedX: { min: -10, max: 10 },
      scale: { start: 0.42, end: 0.05 },
      alpha: { start: 0.65, end: 0 },
      tint: [0x6ef2ff, 0xffd86b, 0xd982ff],
      lifespan: 6500,
      frequency: 140,
      blendMode: 'ADD',
    });

    // Top-right Starlight & Achievement summary pill
    drawOrnatePanel(this, width - 344, 18, 322, 46, 0xc89b45, 0.88);
    this.add.image(width - 318, 41, 'icon_starlight').setDisplaySize(26, 26);
    this.add
      .text(width - 296, 41, `${data.starlight} Starlight`, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        fontStyle: 'bold',
        color: '#88f7ff',
      })
      .setOrigin(0, 0.5);

    this.add.image(width - 154, 41, 'icon_trophy').setDisplaySize(24, 24);
    this.add
      .text(width - 134, 41, `${data.achievements.length} / ${ACHIEVEMENTS.length}`, {
        fontFamily: FONT_TITLE,
        fontSize: '15px',
        fontStyle: 'bold',
        color: '#ffe38e',
      })
      .setOrigin(0, 0.5);

    // Quick mute toggle top-right corner
    const soundBtn = this.add
      .image(width - 42, 41, data.settings.muted ? 'icon_sound_off' : 'icon_sound_on')
      .setDisplaySize(26, 26)
      .setInteractive({ useHandCursor: true });
    soundBtn.on('pointerdown', () => {
      const nextMuted = !saveManager.getData().settings.muted;
      saveManager.updateSettings({ muted: nextMuted });
      soundEngine.updateSettings(saveManager.getData().settings);
      soundBtn.setTexture(nextMuted ? 'icon_sound_off' : 'icon_sound_on').setDisplaySize(26, 26);
    });

    // Rotating Arcane Crest + Title Block
    const crest = this.add.image(118, 128, 'logo_crest').setDisplaySize(108, 108);
    this.tweens.add({
      targets: crest,
      angle: 360,
      duration: 38000,
      repeat: -1,
    });

    this.add.text(186, 86, 'PRISMFORGE', {
      fontFamily: FONT_TITLE,
      fontSize: '46px',
      fontStyle: 'bold',
      color: '#ffe79e',
      stroke: '#070b18',
      strokeThickness: 6,
    });

    this.add.text(190, 140, 'A E T H E R   B A S T I O N', {
      fontFamily: FONT_TITLE,
      fontSize: '17px',
      fontStyle: 'bold',
      color: '#6ef2ff',
      stroke: '#070b18',
      strokeThickness: 4,
    });

    this.add.text(
      68,
      194,
      'Route kinetic Aether Pulses through splitters, mirrors, and elemental crucibles\nto unleash exponential turret volleys against the Voidkin siege.',
      {
        fontFamily: FONT_BODY,
        fontSize: '14px',
        color: '#d8e4ff',
        lineSpacing: 4,
      }
    );

    // Navigation Buttons Column
    let startY = 278;
    const gap = 58;

    if (data.activeRun) {
      createOrnateButton(this, {
        x: 224,
        y: startY,
        width: 312,
        height: 48,
        primary: true,
        iconKey: 'ability_surge',
        label: `CONTINUE (WAVE ${data.activeRun.wave})`,
        onClick: () => {
          this.cameras.main.fadeOut(220, 5, 8, 18);
          this.cameras.main.once('camerafadeoutcomplete', () => {
            this.scene.start('GameScene', { resumeSaved: true });
          });
        },
      });
      startY += gap;
    }

    createOrnateButton(this, {
      x: 224,
      y: startY,
      width: 312,
      height: 50,
      primary: !data.activeRun,
      iconKey: 'icon_sword',
      label: 'NEW EXPEDITION',
      fontSize: 17,
      onClick: () => {
        this.cameras.main.fadeOut(200, 5, 8, 18);
        this.cameras.main.once('camerafadeoutcomplete', () => {
          this.scene.start('WardenSelectScene');
        });
      },
    });

    createOrnateButton(this, {
      x: 224,
      y: startY + gap,
      width: 312,
      height: 46,
      iconKey: 'icon_tree',
      label: 'STARFORGE SKILL TREE',
      onClick: () => this.scene.start('MetaScene', { tab: 'tree' }),
    });

    createOrnateButton(this, {
      x: 224,
      y: startY + gap * 2,
      width: 312,
      height: 46,
      iconKey: 'icon_codex',
      label: 'ARCANE CODEX',
      onClick: () => this.scene.start('MetaScene', { tab: 'codex' }),
    });

    createOrnateButton(this, {
      x: 224,
      y: startY + gap * 3,
      width: 312,
      height: 46,
      iconKey: 'icon_trophy',
      label: 'ACHIEVEMENTS & STATS',
      onClick: () => this.scene.start('MetaScene', { tab: 'achievements' }),
    });

    createOrnateButton(this, {
      x: 224,
      y: startY + gap * 4,
      width: 312,
      height: 46,
      iconKey: 'icon_settings',
      label: 'SETTINGS & AUDIO',
      onClick: () => this.scene.start('MetaScene', { tab: 'settings' }),
    });

    // Bottom Run Telemetry Summary Card on Right
    drawOrnatePanel(this, width - 418, height - 122, 396, 98, 0x5c79b8, 0.84);
    this.add.text(width - 398, height - 108, 'SANCTUM CHRONICLES', {
      fontFamily: FONT_TITLE,
      fontSize: '13px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });
    this.add.text(
      width - 398,
      height - 86,
      `Best Wave: ${data.stats.highestWave}   •   Victories: ${data.stats.victories}   •   Peak Mult: x${data.stats.highestPulseMult.toFixed(1)}\nVoidkin Vanquished: ${data.stats.enemiesVanquished}   •   Best Single Hit: ${data.stats.highestSingleHit}`,
      {
        fontFamily: FONT_BODY,
        fontSize: '12.5px',
        color: '#d3e2ff',
        lineSpacing: 6,
      }
    );

    if (saveManager.recoveredFromCorruption) {
      this.add
        .text(width / 2, height - 16, 'Notice: Corrupted save data was backed up and safely reset.', {
          fontFamily: FONT_BODY,
          fontSize: '12px',
          color: '#ff9e7d',
        })
        .setOrigin(0.5);
    }

    this.cameras.main.fadeIn(240, 5, 8, 18);
  }
}
