import Phaser from 'phaser';
import { ENEMIES, MODULE_LIST, RELIC_LIST } from '../core/content';
import { FONT_BODY, FONT_TITLE } from '../ui/uiFactory';

export class BootScene extends Phaser.Scene {
  constructor() {
    super('BootScene');
  }

  public preload(): void {
    const { width, height } = this.scale;

    const bg = this.add.graphics();
    bg.fillStyle(0x070b17, 1);
    bg.fillRect(0, 0, width, height);

    const title = this.add
      .text(width / 2, height / 2 - 54, 'PRISMFORGE: AETHER BASTION', {
        fontFamily: FONT_TITLE,
        fontSize: '28px',
        fontStyle: 'bold',
        color: '#ffe38e',
      })
      .setOrigin(0.5);

    const sub = this.add
      .text(width / 2, height / 2 - 18, 'Attuning Runic Conduits...', {
        fontFamily: FONT_BODY,
        fontSize: '14px',
        color: '#88c0f7',
      })
      .setOrigin(0.5);

    const barFrame = this.add.graphics();
    barFrame.lineStyle(2, 0xc89b45, 0.9);
    barFrame.strokeRoundedRect(width / 2 - 210, height / 2 + 12, 420, 20, 8);

    const barFill = this.add.graphics();
    this.load.on('progress', (val: number) => {
      barFill.clear();
      barFill.fillStyle(0x52e5ff, 1);
      barFill.fillRoundedRect(width / 2 - 206, height / 2 + 16, Math.max(8, 412 * val), 12, 5);
      sub.setText(`Attuning Runic Conduits... ${Math.round(val * 100)}%`);
    });

    this.load.once('complete', () => {
      title.destroy();
      sub.destroy();
      barFrame.destroy();
      barFill.destroy();
    });

    // 1. Key Art & Portraits
    this.load.image('menu_hero_bg', 'assets/art/menu_hero_bg.jpg');
    this.load.image('battle_sanctum_bg', 'assets/art/battle_sanctum_bg.jpg');
    this.load.image('warden_lyra', 'assets/art/warden_lyra.jpg');
    this.load.image('warden_vex', 'assets/art/warden_vex.jpg');
    this.load.image('warden_kael', 'assets/art/warden_kael.jpg');
    this.load.image('warden_nyx', 'assets/art/warden_nyx.jpg');

    // 2. Grid Tiles & Core Structures
    for (const t of ['normal', 'radiant', 'haste', 'prismatic', 'locked', 'hover']) {
      this.load.image(`tile_${t}`, `assets/sprites/tile_${t}.png`);
    }
    this.load.image('emitter_solar', 'assets/sprites/emitter_solar.png');
    this.load.image('bastion_core', 'assets/sprites/bastion_core.png');
    this.load.image('void_rift', 'assets/sprites/void_rift.png');

    // 3. Modules & Relics
    for (const m of MODULE_LIST) {
      this.load.image(m.spriteKey, `assets/sprites/${m.spriteKey}.png`);
    }
    for (const r of RELIC_LIST) {
      this.load.image(r.spriteKey, `assets/sprites/${r.spriteKey}.png`);
    }

    // 4. Multi-frame Enemy & Boss Spritesheets
    for (const en of Object.values(ENEMIES)) {
      const frameSize = en.isBoss ? 120 : 80;
      this.load.spritesheet(en.spriteKey, `assets/sprites/${en.spriteKey}.png`, {
        frameWidth: frameSize,
        frameHeight: frameSize,
      });
    }

    // 5. Projectiles, Loot & VFX
    for (const sp of [
      'proj_pulse',
      'proj_blaze',
      'proj_frost',
      'proj_venom',
      'proj_mortar',
      'proj_arcane',
      'vfx_spark',
      'vfx_ring',
      'loot_gold',
      'loot_starlight',
      'reticle_target',
    ]) {
      this.load.image(sp, `assets/sprites/${sp}.png`);
    }

    // 6. UI Buttons & Icons
    for (const ui of [
      'logo_crest',
      'btn_primary',
      'btn_primary_hover',
      'btn_secondary',
      'btn_secondary_hover',
      'icon_gold',
      'icon_starlight',
      'icon_heart',
      'icon_shield',
      'icon_sword',
      'icon_reroll',
      'icon_lock',
      'icon_unlock',
      'icon_sell',
      'icon_rotate',
      'icon_sound_on',
      'icon_sound_off',
      'icon_pause',
      'icon_trophy',
      'icon_codex',
      'icon_tree',
      'icon_settings',
      'icon_weather_clear',
      'icon_weather_solar',
      'icon_weather_storm',
      'icon_weather_cryo',
      'icon_weather_eclipse',
      'ability_surge',
      'ability_nova',
      'ability_ult_lyra',
      'ability_ult_vex',
      'ability_ult_kael',
      'ability_ult_nyx',
    ]) {
      this.load.image(ui, `assets/ui/${ui}.png`);
    }
  }

  public create(): void {
    // Register multi-frame animations for every Enemy & Boss
    for (const en of Object.values(ENEMIES)) {
      const animKey = `${en.spriteKey}_walk`;
      if (!this.anims.exists(animKey)) {
        this.anims.create({
          key: animKey,
          frames: this.anims.generateFrameNumbers(en.spriteKey, { start: 0, end: 2 }),
          frameRate: en.isBoss ? 5 : 8,
          repeat: -1,
          yoyo: true,
        });
      }
    }

    this.scene.start('MainMenuScene');
  }
}
