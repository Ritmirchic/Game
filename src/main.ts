import Phaser from 'phaser';
import { BootScene } from './scenes/BootScene';
import { GameScene } from './scenes/GameScene';
import { MainMenuScene } from './scenes/MainMenuScene';
import { MetaScene } from './scenes/MetaScene';
import { WardenSelectScene } from './scenes/WardenSelectScene';

declare global {
  interface Window {
    __PRISMFORGE_HEADLESS__?: boolean;
    __PRISMFORGE_GAME__?: Phaser.Game;
  }
}

const config: Phaser.Types.Core.GameConfig = {
  type: window.__PRISMFORGE_HEADLESS__ ? Phaser.CANVAS : Phaser.AUTO,
  parent: 'game-root',
  width: 1280,
  height: 720,
  backgroundColor: '#070a14',
  scale: {
    mode: Phaser.Scale.FIT,
    autoCenter: Phaser.Scale.CENTER_BOTH,
  },
  render: {
    antialias: true,
    pixelArt: false,
    powerPreference: 'high-performance',
  },
  scene: [BootScene, MainMenuScene, WardenSelectScene, MetaScene, GameScene],
};

const game = new Phaser.Game(config);
window.__PRISMFORGE_GAME__ = game;

export default game;
