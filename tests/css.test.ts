// @vitest-environment jsdom
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { installAll } from './dom-setup';
import { createGame } from '../src/main';

installAll();

const css = readFileSync(resolve(process.cwd(), 'src/styles.css'), 'utf8');

describe('interface contract', () => {
  it('lets clicks fall through the battle wrapper to the canvas', () => {
    document.body.innerHTML = '<div id="app"><canvas id="stage"></canvas><div id="ui"></div></div>';
    const game = createGame() as unknown as {
      run: { offers: { type: string; label: string }[] } | null;
      showHeroes(): void;
      startRun(id: string): void;
      showRoute(): void;
      screen: string;
    };
    game.showHeroes();
    game.startRun('keeper');
    game.run!.offers = [{ type: 'battle', label: 'Ash Hall' }];
    game.showRoute();
    (document.querySelector('[data-action="pick-room"]') as HTMLElement).dispatchEvent(
      new window.MouseEvent('click', { bubbles: true }),
    );
    expect(game.screen).toBe('battle');
    const screen = document.querySelector('.screen')!;
    expect(screen.classList.contains('passthrough')).toBe(true);
    // The HUD must remain clickable even though the wrapper is transparent.
    expect(document.getElementById('end-turn')).toBeTruthy();
  });

  it('ships the pointer-events rules that make the board clickable', () => {
    expect(css).toContain('.screen.passthrough');
    expect(css).toContain('pointer-events: none');
    expect(css).toMatch(/\.screen\.passthrough \.hud-bottom[\s\S]{0,200}pointer-events: auto/);
  });

  it('has no unresolved template markers or emoji-as-art in the stylesheet', () => {
    expect(css.includes('undefined')).toBe(false);
    expect(css.includes('__')).toBe(false);
  });
});
