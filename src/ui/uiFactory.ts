import Phaser from 'phaser';
import { soundEngine } from '../core/appContext';
import { AchievementDefinition } from '../core/types';

export const FONT_TITLE = '"Cinzel", "Palatino Linotype", "Book Antiqua", Georgia, serif';
export const FONT_BODY = '"Segoe UI", "Trebuchet MS", system-ui, sans-serif';

export interface OrnateButtonOptions {
  x: number;
  y: number;
  width?: number;
  height?: number;
  label: string;
  primary?: boolean;
  iconKey?: string;
  fontSize?: number;
  onClick: () => void;
}

export function createOrnateButton(
  scene: Phaser.Scene,
  opts: OrnateButtonOptions
): Phaser.GameObjects.Container {
  const w = opts.width ?? 230;
  const h = opts.height ?? 48;
  const baseTex = opts.primary ? 'btn_primary' : 'btn_secondary';
  const hoverTex = opts.primary ? 'btn_primary_hover' : 'btn_secondary_hover';

  const container = scene.add.container(opts.x, opts.y);
  const bg = scene.add.image(0, 0, baseTex).setDisplaySize(w, h);
  container.add(bg);

  let textOffsetX = 0;
  if (opts.iconKey) {
    const icon = scene.add
      .image(-w * 0.31, 0, opts.iconKey)
      .setDisplaySize(Math.min(26, h * 0.56), Math.min(26, h * 0.56));
    container.add(icon);
    textOffsetX = 12;
  }

  const label = scene.add
    .text(textOffsetX, 0, opts.label, {
      fontFamily: FONT_TITLE,
      fontSize: `${opts.fontSize ?? 16}px`,
      fontStyle: 'bold',
      color: opts.primary ? '#fff3c4' : '#e4eeff',
      stroke: '#070a14',
      strokeThickness: 3,
    })
    .setOrigin(0.5);
  container.add(label);

  bg.setInteractive({ useHandCursor: true });

  bg.on('pointerover', () => {
    bg.setTexture(hoverTex);
    bg.setDisplaySize(w, h);
    soundEngine.playHover();
    scene.tweens.add({
      targets: container,
      scaleX: 1.035,
      scaleY: 1.035,
      duration: 90,
      ease: 'Quad.easeOut',
    });
  });

  bg.on('pointerout', () => {
    bg.setTexture(baseTex);
    bg.setDisplaySize(w, h);
    scene.tweens.add({
      targets: container,
      scaleX: 1,
      scaleY: 1,
      duration: 90,
      ease: 'Quad.easeOut',
    });
  });

  bg.on('pointerdown', () => {
    soundEngine.playClick();
    scene.tweens.add({
      targets: container,
      scaleX: 0.96,
      scaleY: 0.96,
      duration: 55,
      yoyo: true,
      onComplete: () => opts.onClick(),
    });
  });

  container.setData('labelObj', label);
  container.setData('bgObj', bg);
  return container;
}

export function drawOrnatePanel(
  scene: Phaser.Scene,
  x: number,
  y: number,
  w: number,
  h: number,
  borderColor = 0xc89b45,
  alpha = 0.92
): Phaser.GameObjects.Graphics {
  const g = scene.add.graphics();
  g.fillStyle(0x0b1022, alpha);
  g.fillRoundedRect(x, y, w, h, 12);
  g.lineStyle(2, borderColor, 0.9);
  g.strokeRoundedRect(x, y, w, h, 12);
  g.lineStyle(1, 0x62ecff, 0.25);
  g.strokeRoundedRect(x + 5, y + 5, w - 10, h - 10, 9);
  return g;
}

export class TooltipOverlay {
  private container: Phaser.GameObjects.Container;
  private bg: Phaser.GameObjects.Graphics;
  private titleTxt: Phaser.GameObjects.Text;
  private subTxt: Phaser.GameObjects.Text;
  private bodyTxt: Phaser.GameObjects.Text;

  constructor(scene: Phaser.Scene) {
    this.bg = scene.add.graphics();
    this.titleTxt = scene.add.text(12, 10, '', {
      fontFamily: FONT_TITLE,
      fontSize: '15px',
      fontStyle: 'bold',
      color: '#ffe38e',
    });
    this.subTxt = scene.add.text(12, 30, '', {
      fontFamily: FONT_BODY,
      fontSize: '11px',
      color: '#7ef9ff',
    });
    this.bodyTxt = scene.add.text(12, 48, '', {
      fontFamily: FONT_BODY,
      fontSize: '12px',
      color: '#e5ecff',
      wordWrap: { width: 256 },
      lineSpacing: 3,
    });

    this.container = scene.add.container(0, 0, [this.bg, this.titleTxt, this.subTxt, this.bodyTxt]);
    this.container.setDepth(5000);
    this.container.setVisible(false);
  }

  public show(x: number, y: number, title: string, subtitle: string, body: string): void {
    this.titleTxt.setText(title);
    this.subTxt.setText(subtitle);
    this.bodyTxt.setText(body);

    const w = 280;
    const h = Math.max(82, 58 + this.bodyTxt.height);
    this.bg.clear();
    this.bg.fillStyle(0x090e1f, 0.96);
    this.bg.fillRoundedRect(0, 0, w, h, 9);
    this.bg.lineStyle(1.8, 0xf2c464, 0.95);
    this.bg.strokeRoundedRect(0, 0, w, h, 9);

    const cx = Math.min(1268 - w, Math.max(12, x));
    const cy = y - h - 12 < 8 ? Math.min(712 - h, y + 18) : y - h - 12;
    this.container.setPosition(cx, cy);
    this.container.setVisible(true);
  }

  public hide(): void {
    this.container.setVisible(false);
  }
}

export function showAchievementToast(scene: Phaser.Scene, ach: AchievementDefinition, index = 0): void {
  soundEngine.playAchievement();
  const yTarget = 74 + index * 64;
  const c = scene.add.container(1280 + 180, yTarget).setDepth(6000);

  const g = scene.add.graphics();
  g.fillStyle(0x131d3b, 0.96);
  g.fillRoundedRect(-170, -26, 340, 52, 10);
  g.lineStyle(2, 0xffd166, 1);
  g.strokeRoundedRect(-170, -26, 340, 52, 10);

  const icon = scene.add.image(-142, 0, 'icon_trophy').setDisplaySize(32, 32);
  const hdr = scene.add.text(-116, -17, `ACHIEVEMENT UNLOCKED  •  +${ach.rewardStarlight} Starlight`, {
    fontFamily: FONT_BODY,
    fontSize: '11px',
    fontStyle: 'bold',
    color: '#7ef9ff',
  });
  const name = scene.add.text(-116, 0, ach.name, {
    fontFamily: FONT_TITLE,
    fontSize: '15px',
    fontStyle: 'bold',
    color: '#ffe89c',
  });

  c.add([g, icon, hdr, name]);

  scene.tweens.add({
    targets: c,
    x: 1090,
    duration: 380,
    ease: 'Back.easeOut',
    hold: 2600,
    yoyo: true,
    onComplete: () => c.destroy(),
  });
}
