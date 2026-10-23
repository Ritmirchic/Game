import '@fontsource-variable/manrope';
import '@fontsource-variable/unbounded';
import './style.css';
import './hud.css';
import { Game } from './game/Game';
import type { EndStats, GameSnapshot, GameState, RecipeId } from './game/types';

const icon = (name: string, className = ''): string => `<svg class="icon ${className}" aria-hidden="true"><use href="#i-${name}"></use></svg>`;
const formatTime = (seconds: number): string => {
  const rounded = Math.max(0, Math.ceil(seconds));
  return `${String(Math.floor(rounded / 60)).padStart(2, '0')}:${String(rounded % 60).padStart(2, '0')}`;
};

const app = document.querySelector<HTMLDivElement>('#app');
if (!app) throw new Error('App root not found');

app.innerHTML = `
  <svg class="svg-defs" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
    <defs>
      <symbol id="i-beacon" viewBox="0 0 24 24"><path d="M8.5 21h7L15 9H9l-.5 12Z"/><path d="M7 8h10l-2-5H9L7 8Z"/><path d="M5 21h14M4 6 1 4m19 2 3-2M12 1V0"/></symbol>
      <symbol id="i-tree" viewBox="0 0 24 24"><path d="m12 2-6 8h3l-5 7h6v5h4v-5h6l-5-7h3l-6-8Z"/></symbol>
      <symbol id="i-scrap" viewBox="0 0 24 24"><path d="m14.5 6.5 3-3 3 3-3 3M4 20l5-5m-3-9 8 8M3 3l7 2-2 5-5-7Zm12 12 6-1-1 6-5-5Z"/></symbol>
      <symbol id="i-crystal" viewBox="0 0 24 24"><path d="m12 2 7 6-3 12H8L5 8l7-6Z"/><path d="m5 8 7 4 7-4M12 2v10m-4 8 4-8 4 8"/></symbol>
      <symbol id="i-heart" viewBox="0 0 24 24"><path d="M20.8 4.6a5.5 5.5 0 0 0-7.8 0L12 5.7l-1.1-1.1a5.5 5.5 0 0 0-7.8 7.8L12 21l8.8-8.6a5.5 5.5 0 0 0 0-7.8Z"/></symbol>
      <symbol id="i-bolt" viewBox="0 0 24 24"><path d="m13 2-9 12h8l-1 8 9-12h-8l1-8Z"/></symbol>
      <symbol id="i-fire" viewBox="0 0 24 24"><path d="M12 22c4 0 7-2.8 7-7 0-3-1.8-5.7-5-8 .2 2-1 3.4-2 4C12.5 7.5 10 4 7 2c.3 4-2 6.2-2 10 0 5.5 3 10 7 10Z"/><path d="M12 22c-2 0-3-1.5-3-3.5 0-1.6 1-3 3-4.5 0 2 3 2.5 3 4.5 0 2-1 3.5-3 3.5Z"/></symbol>
      <symbol id="i-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M4.9 4.9l1.4 1.4m11.4 11.4 1.4 1.4M2 12h2m16 0h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></symbol>
      <symbol id="i-moon" viewBox="0 0 24 24"><path d="M21 12.8A9 9 0 1 1 11.2 3 7 7 0 0 0 21 12.8Z"/></symbol>
      <symbol id="i-volume" viewBox="0 0 24 24"><path d="M11 5 6 9H2v6h4l5 4V5Zm4 4a4 4 0 0 1 0 6m3-9a8 8 0 0 1 0 12"/></symbol>
      <symbol id="i-muted" viewBox="0 0 24 24"><path d="M11 5 6 9H2v6h4l5 4V5Zm5 5 5 5m0-5-5 5"/></symbol>
      <symbol id="i-pause" viewBox="0 0 24 24"><path d="M8 5v14M16 5v14"/></symbol>
      <symbol id="i-play" viewBox="0 0 24 24"><path d="m7 3 14 9-14 9V3Z"/></symbol>
      <symbol id="i-arrow" viewBox="0 0 24 24"><path d="M5 12h14m-6-6 6 6-6 6"/></symbol>
      <symbol id="i-book" viewBox="0 0 24 24"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20V3H6.5A2.5 2.5 0 0 0 4 5.5v14Zm0 0A2.5 2.5 0 0 0 6.5 22H20v-5"/></symbol>
      <symbol id="i-close" viewBox="0 0 24 24"><path d="m6 6 12 12M18 6 6 18"/></symbol>
      <symbol id="i-wall" viewBox="0 0 24 24"><path d="M3 21V5h18v16M3 9h18M3 14h18M8 5v4m8-4v4M6 9v5m9-5v5M8 14v7m8-7v7"/></symbol>
      <symbol id="i-turret" viewBox="0 0 24 24"><path d="M8 21h8m-4 0v-6M5 9h11v6H5V9Zm11 2h6M8 9V5h5v4"/></symbol>
      <symbol id="i-repair" viewBox="0 0 24 24"><path d="M14.7 6.3a4 4 0 0 0-5-5L12 3.6 9.6 6 7.3 3.7a4 4 0 0 0 5 5L4 17l3 3 7.7-8.3a4 4 0 0 0 5-5L17.4 9 15 6.6l2.3-2.3a4 4 0 0 0-2.6 2Z"/></symbol>
      <symbol id="i-medkit" viewBox="0 0 24 24"><path d="M3 7h18v14H3V7Zm6 0V3h6v4m-3 4v6m-3-3h6"/></symbol>
      <symbol id="i-skull" viewBox="0 0 24 24"><path d="M5 16a8 8 0 1 1 14 0v3h-3v3h-3v-3h-2v3H8v-3H5v-3Z"/><circle cx="9" cy="11" r="1"/><circle cx="15" cy="11" r="1"/></symbol>
      <symbol id="i-clock" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></symbol>
      <symbol id="i-restart" viewBox="0 0 24 24"><path d="M3 12a9 9 0 1 0 3-6.7L3 8m0-5v5h5"/></symbol>
      <symbol id="i-home" viewBox="0 0 24 24"><path d="m3 11 9-8 9 8M5 10v11h14V10M9 21v-7h6v7"/></symbol>
      <symbol id="i-check" viewBox="0 0 24 24"><path d="m4 12 5 5L20 6"/></symbol>
      <symbol id="i-mouse" viewBox="0 0 24 24"><rect x="6" y="2" width="12" height="20" rx="6"/><path d="M12 2v7"/></symbol>
    </defs>
  </svg>

  <main class="game-shell">
    <canvas id="game-canvas" aria-label="Трёхмерный остров Последнего маяка"></canvas>
    <div class="cinematic-grain"></div>
    <div id="damage-flash" class="damage-flash"></div>

    <section id="menu-screen" class="screen menu-screen">
      <header class="menu-nav">
        <div class="brand">
          <span class="brand-mark">${icon('beacon')}</span>
          <span><strong>BABYLON // SURVIVAL</strong><small>PBR edition · 02</small></span>
        </div>
        <div class="menu-nav-right">
          <span class="prototype-badge"><i></i> PBR · СКЕЛЕТНАЯ АНИМАЦИЯ</span>
          <button class="icon-button glass" data-action="sound" aria-label="Выключить звук">${icon('volume')}</button>
        </div>
      </header>

      <div class="menu-content">
        <div class="hero-copy">
          <div class="eyebrow"><span>ОСТРОВ ЗАТЯНУЛО ПЕПЛОМ</span><b>•</b><span>СПАСЕНИЯ НЕ БУДЕТ</span></div>
          <h1>ПОСЛЕДНИЙ<br><em>МАЯК</em></h1>
          <p class="hero-description">Днём собирай всё, что сможешь унести.<br>Ночью защищай единственный свет на острове.</p>
          <div class="hero-actions">
            <button class="primary-button" data-action="start" disabled><span>ЗАГРУЗКА 3D-МИРА…</span>${icon('arrow')}</button>
            <button class="secondary-button" data-action="how">${icon('book')}<span>КАК ВЫЖИТЬ</span></button>
          </div>
          <div class="quick-controls">
            <span><kbd>WASD</kbd> движение</span>
            <span><kbd>E</kbd> добыча</span>
            <span><kbd>F</kbd> атака</span>
            <span><kbd>1–5</kbd> крафт</span>
            <span><kbd>SPACE</kbd> уклонение</span>
          </div>
        </div>

        <aside class="field-card">
          <div class="field-card-top"><span>ДОСЬЕ ЭКСПЕДИЦИИ</span><b>№ 001</b></div>
          <div class="field-card-symbol">${icon('beacon')}</div>
          <h2>ОДИН ОСТРОВ.<br>ОДИН ШАНС.</h2>
          <p>Заряди маяк тремя энергоячейками и переживи финальный пепельный штурм.</p>
          <ol class="mission-steps">
            <li><span>01</span><div><strong>ДОБЫВАЙ</strong><small>Дерево, металл, осколки</small></div></li>
            <li><span>02</span><div><strong>УКРЕПЛЯЙ</strong><small>Турели, костры, баррикады</small></div></li>
            <li><span>03</span><div><strong>ДЕРЖИ СВЕТ</strong><small>Маяк не должен погаснуть</small></div></li>
          </ol>
          <div class="field-card-footer"><span>1–4 выживших в концепте</span><span>6–10 минут</span></div>
        </aside>
      </div>

      <footer class="menu-footer">
        <div><span class="status-dot"></span> PBR · отражения · мягкие тени</div>
        <div>День <b>/</b> Ночь <b>/</b> Оборона</div>
        <div class="desktop-only">Зажми кнопку мыши и веди, чтобы осмотреться</div>
      </footer>
    </section>

    <section id="hud" class="hud" hidden>
      <header class="hud-top">
        <div id="phase-card" class="phase-card">
          <span id="phase-icon" class="phase-symbol">${icon('sun')}</span>
          <div class="phase-info">
            <div class="phase-copy"><span id="phase-label">ДЕНЬ 1</span><small id="phase-hint">ДО НОЧИ</small></div>
            <strong id="phase-time">01:15</strong>
          </div>
          <div class="phase-progress"><i id="phase-progress"></i></div>
        </div>
        <div class="hud-right">
          <div class="resources-bar" aria-label="Инвентарь">
            <div id="resource-wood" class="resource-pill wood"><span class="resource-icon">${icon('tree')}</span><span class="resource-copy"><strong>4</strong><small>ДЕРЕВО</small></span></div>
            <div id="resource-scrap" class="resource-pill scrap"><span class="resource-icon">${icon('scrap')}</span><span class="resource-copy"><strong>2</strong><small>МЕТАЛЛ</small></span></div>
            <div id="resource-crystal" class="resource-pill crystal"><span class="resource-icon">${icon('crystal')}</span><span class="resource-copy"><strong>0</strong><small>ОСКОЛКИ</small></span></div>
          </div>
          <button class="icon-button glass" data-action="sound" aria-label="Выключить звук">${icon('volume')}</button>
          <button class="icon-button glass" data-action="pause" aria-label="Пауза">${icon('pause')}</button>
        </div>
      </header>

      <aside class="objective-card hud-panel">
        <div class="panel-kicker">${icon('beacon')} ТЕКУЩАЯ ЗАДАЧА</div>
        <h2 id="objective-title">Зарядите маяк</h2>
        <div class="core-row"><div class="core-socket"></div><div class="core-socket"></div><div class="core-socket"></div><span id="core-count">0 / 3</span></div>
        <p id="objective-detail">Найдите 6 металла и 4 осколка для каждой ячейки.</p>
      </aside>

      <aside class="beacon-card hud-panel">
        <div class="beacon-card-head"><span>${icon('beacon')} МАЯК</span><strong id="beacon-percent">100%</strong></div>
        <div class="meter beacon-meter"><i id="beacon-health-bar"></i></div>
        <div class="beacon-card-footer"><span id="enemy-status">${icon('sun')} Остров спокоен</span><span id="kill-count">0 убито</span></div>
      </aside>

      <div id="player-status" class="player-status hud-panel">
        <div class="status-header"><span>ВЫЖИВШИЙ · 01</span><strong id="condition-value">СТАБИЛЬНО</strong></div>
        <div class="stat-line health-line">
          <span class="stat-icon">${icon('heart')}</span>
          <div class="stat-content"><div class="stat-label"><span>ЗДОРОВЬЕ</span><strong id="health-value">100</strong></div><div class="meter health-meter"><i id="health-bar"></i></div></div>
        </div>
        <div class="stat-line stamina-line">
          <span class="stat-icon">${icon('bolt')}</span>
          <div class="stat-content"><div class="stat-label"><span>ВЫНОСЛИВОСТЬ</span><strong id="stamina-value">100</strong></div><div class="meter stamina-meter"><i id="stamina-bar"></i></div></div>
        </div>
        <div class="stat-line warmth-line">
          <span class="stat-icon">${icon('fire')}</span>
          <div class="stat-content"><div class="stat-label"><span>ТЕПЛО</span><strong id="warmth-value">100</strong></div><div class="meter warmth-meter"><i id="warmth-bar"></i></div></div>
        </div>
        <div id="dodge-status" class="dodge-status"><kbd>SPACE</kbd><span>СЛАЙД</span><strong id="dodge-value">ГОТОВО</strong></div>
      </div>

      <div id="interaction-prompt" class="interaction-prompt" hidden>
        <span class="prompt-key">E</span><div><strong id="prompt-title"></strong><small id="prompt-detail"></small></div>
      </div>

      <div class="hotbar" aria-label="Быстрый крафт">
        <button class="recipe-button" data-recipe="barricade" aria-keyshortcuts="1" aria-label="Баррикада: 5 дерева и 2 металла, клавиша 1"><kbd>1</kbd><span class="recipe-icon">${icon('wall')}</span><span class="recipe-copy"><strong>БАРРИКАДА</strong><small>5 дер. · 2 мет.</small></span></button>
        <button class="recipe-button" data-recipe="turret" aria-keyshortcuts="2" aria-label="Турель: 7 металла и 2 осколка, клавиша 2"><kbd>2</kbd><span class="recipe-icon">${icon('turret')}</span><span class="recipe-copy"><strong>ТУРЕЛЬ</strong><small>7 мет. · 2 оск.</small></span></button>
        <button class="recipe-button" data-recipe="campfire" aria-keyshortcuts="3" aria-label="Костёр: 4 дерева и 1 металл, клавиша 3"><kbd>3</kbd><span class="recipe-icon">${icon('fire')}</span><span class="recipe-copy"><strong>КОСТЁР</strong><small>4 дер. · 1 мет.</small></span></button>
        <button class="recipe-button" data-recipe="repair" aria-keyshortcuts="4" aria-label="Ремонт: 3 дерева и 3 металла, клавиша 4"><kbd>4</kbd><span class="recipe-icon">${icon('repair')}</span><span class="recipe-copy"><strong>РЕМОНТ</strong><small>3 дер. · 3 мет.</small></span></button>
        <button class="recipe-button" data-recipe="medkit" aria-keyshortcuts="5" aria-label="Аптечка: 2 металла и 1 осколок, клавиша 5"><kbd>5</kbd><span class="recipe-icon">${icon('medkit')}</span><span class="recipe-copy"><strong>АПТЕЧКА</strong><small>2 мет. · 1 оск.</small></span></button>
      </div>

      <div class="mobile-controls">
        <div id="joystick" class="joystick"><div class="joystick-ring"><div id="joystick-knob" class="joystick-knob"></div></div></div>
        <div class="mobile-actions">
          <button data-mobile="dodge" aria-label="Скользящее уклонение">↯<small>СЛАЙД</small></button>
          <button data-mobile="interact" class="interact" aria-label="Добыть или взаимодействовать">E<small>ДОБЫЧА</small></button>
          <button data-mobile="attack" class="attack" aria-label="Атаковать">${icon('scrap')}<small>УДАР</small></button>
        </div>
      </div>
    </section>

    <div id="notifications" class="notifications" aria-live="polite"></div>
    <div id="loot-feed" class="loot-feed" aria-live="polite"></div>
    <div id="announcement" class="announcement">
      <span id="announce-eyebrow"></span><h2 id="announce-title"></h2><p id="announce-detail"></p>
    </div>

    <section id="pause-screen" class="screen overlay-screen" hidden>
      <div class="overlay-card pause-card">
        <span class="overlay-icon">${icon('pause')}</span>
        <div class="eyebrow centered">СМЕНА ПРИОСТАНОВЛЕНА</div>
        <h2>ПЕРЕДЫШКА</h2>
        <p>Пепел подождёт. Но ненадолго.</p>
        <button class="primary-button wide" data-action="resume"><span>ПРОДОЛЖИТЬ</span>${icon('play')}</button>
        <div class="split-buttons"><button class="secondary-button" data-action="restart">${icon('restart')} ЗАНОВО</button><button class="secondary-button" data-action="menu">${icon('home')} В МЕНЮ</button></div>
        <button class="text-button" data-action="how">Правила выживания</button>
      </div>
    </section>

    <section id="end-screen" class="screen overlay-screen" hidden>
      <div id="end-card" class="overlay-card end-card">
        <span id="end-icon" class="overlay-icon">${icon('beacon')}</span>
        <div id="end-eyebrow" class="eyebrow centered">СМЕНА ОКОНЧЕНА</div>
        <h2 id="end-title">МАЯК ПОГАС</h2>
        <p id="end-reason"></p>
        <div class="run-stats">
          <div><span>${icon('clock')} ВРЕМЯ</span><strong id="stat-time">00:00</strong></div>
          <div><span>${icon('skull')} ПЕПЕЛЬНЫХ</span><strong id="stat-kills">0</strong></div>
          <div><span>${icon('tree')} РЕСУРСОВ</span><strong id="stat-gathered">0</strong></div>
          <div><span>${icon('crystal')} ЯЧЕЙКИ</span><strong id="stat-cores">0/3</strong></div>
        </div>
        <div id="best-time" class="best-time" hidden></div>
        <button class="primary-button wide" data-action="restart"><span>ЕЩЁ ОДНА СМЕНА</span>${icon('restart')}</button>
        <button class="text-button" data-action="menu">Вернуться в главное меню</button>
      </div>
    </section>

    <section id="how-modal" class="screen overlay-screen how-modal" hidden>
      <div class="overlay-card how-card">
        <button class="icon-button modal-close" data-action="close-how" aria-label="Закрыть">${icon('close')}</button>
        <div class="eyebrow">ПОЛЕВАЯ ИНСТРУКЦИЯ · 01</div>
        <h2>КАК ВЫЖИТЬ</h2>
        <p class="how-intro">Не нужно выживать вечно. Установите три ячейки в маяк и продержитесь 40 секунд финальной зарядки.</p>
        <div class="how-grid">
          <article><span class="how-number">01</span>${icon('tree')}<h3>ДОБЫЧА</h3><p>Подойдите к дереву, металлу или кристаллу и нажмите <kbd>E</kbd>. Ресурсы восстанавливаются на рассвете.</p></article>
          <article><span class="how-number">02</span>${icon('fire')}<h3>ТЕПЛО И ОБОРОНА</h3><p><kbd>1</kbd> баррикада, <kbd>2</kbd> турель, <kbd>3</kbd> костёр. Ночью тепло падает; дозаправляйте огонь клавишей <kbd>E</kbd>.</p></article>
          <article><span class="how-number">03</span>${icon('moon')}<h3>УКЛОНЕНИЕ</h3><p>Пепельные замахиваются перед атакой. Нажмите <kbd>SPACE</kbd> или <kbd>Q</kbd> для скользящего уклонения с короткой неуязвимостью.</p></article>
        </div>
        <div class="controls-table"><span><kbd>WASD / СТРЕЛКИ</kbd> движение</span><span><kbd>SHIFT</kbd> бег</span><span><kbd>SPACE / Q</kbd> уклонение</span><span><kbd>E</kbd> действие / дрова</span><span><kbd>F / ЛКМ</kbd> атака</span><span><kbd>1–5</kbd> крафт</span><span><kbd>P / ESC</kbd> пауза</span><span><kbd>ЛКМ + ДВИЖЕНИЕ</kbd> обзор · колесо — масштаб</span></div>
        <button class="primary-button wide" data-action="close-how"><span>ПОНЯТНО</span>${icon('check')}</button>
      </div>
    </section>
  </main>
`;

const byId = <T extends HTMLElement = HTMLElement>(id: string): T => {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Element #${id} not found`);
  return element as T;
};
const hud = byId('hud');
const menu = byId('menu-screen');
const pauseScreen = byId('pause-screen');
const endScreen = byId('end-screen');
const howModal = byId('how-modal');
const announcement = byId('announcement');
const notifications = byId('notifications');
const lootFeed = byId('loot-feed');
let announcementVersion = 0;
let latestSnapshot: GameSnapshot | null = null;

function notify(message: string, tone: 'info' | 'good' | 'danger' = 'info'): void {
  const item = document.createElement('div');
  item.className = `notification ${tone}`;
  item.innerHTML = `<i></i><span></span>`;
  const text = item.querySelector('span');
  if (text) text.textContent = message;
  notifications.append(item);
  while (notifications.children.length > 4) notifications.firstElementChild?.remove();
  window.setTimeout(() => item.classList.add('leaving'), 2900);
  window.setTimeout(() => item.remove(), 3300);
}

function showAnnouncement(eyebrow: string, title: string, detail: string): void {
  announcementVersion += 1;
  const version = announcementVersion;
  byId('announce-eyebrow').textContent = eyebrow;
  byId('announce-title').textContent = title;
  byId('announce-detail').textContent = detail;
  announcement.classList.remove('visible');
  void announcement.offsetWidth;
  announcement.classList.add('visible');
  window.setTimeout(() => {
    if (version === announcementVersion) announcement.classList.remove('visible');
  }, 3500);
}

function showLoot(message: string, resource: string): void {
  const item = document.createElement('div');
  item.className = `loot-pop ${resource}`;
  item.textContent = message;
  lootFeed.append(item);
  while (lootFeed.children.length > 3) lootFeed.firstElementChild?.remove();
  window.setTimeout(() => item.remove(), 1450);
}

function fillEndScreen(stats: EndStats): void {
  const victory = stats.victory;
  byId('end-card').classList.toggle('victory', victory);
  byId('end-icon').innerHTML = icon(victory ? 'beacon' : 'skull');
  byId('end-eyebrow').textContent = victory ? 'СИГНАЛ ПРИНЯТ · СМЕНА ЗАВЕРШЕНА' : 'ЭКСПЕДИЦИЯ ПРЕРВАНА';
  byId('end-title').textContent = victory ? 'СВЕТ ПРОБИЛ ШТОРМ' : 'МАЯК ПОГАС';
  byId('end-reason').textContent = stats.reason;
  byId('stat-time').textContent = formatTime(stats.elapsed);
  byId('stat-kills').textContent = String(stats.kills);
  byId('stat-gathered').textContent = String(stats.gathered);
  byId('stat-cores').textContent = `${stats.cores}/3`;
  const best = byId('best-time');
  best.hidden = true;
  if (victory) {
    try {
      const previous = Number(localStorage.getItem('last-beacon-best') ?? 0);
      const isRecord = previous === 0 || stats.elapsed < previous;
      if (isRecord) localStorage.setItem('last-beacon-best', String(stats.elapsed));
      const bestValue = isRecord ? stats.elapsed : previous;
      best.textContent = `${isRecord ? 'НОВЫЙ РЕКОРД' : 'ЛУЧШЕЕ ВРЕМЯ'} · ${formatTime(bestValue)}`;
      best.hidden = false;
    } catch {
      best.hidden = true;
    }
  }
}

function handleState(state: GameState, stats?: EndStats): void {
  menu.hidden = state !== 'menu';
  hud.hidden = state === 'menu';
  pauseScreen.hidden = state !== 'paused';
  endScreen.hidden = state !== 'gameover' && state !== 'victory';
  if (stats) fillEndScreen(stats);
  if (state === 'menu') {
    announcement.classList.remove('visible');
    notifications.replaceChildren();
    lootFeed.replaceChildren();
  }
}

function updateSnapshot(snapshot: GameSnapshot): void {
  latestSnapshot = snapshot;
  byId('health-bar').style.width = `${snapshot.health}%`;
  byId('health-value').textContent = String(Math.ceil(snapshot.health));
  byId('stamina-bar').style.width = `${snapshot.stamina}%`;
  byId('stamina-value').textContent = String(Math.ceil(snapshot.stamina));
  byId('warmth-bar').style.width = `${snapshot.warmth}%`;
  byId('warmth-bar').classList.toggle('critical', snapshot.warmth <= 25);
  byId('warmth-value').textContent = String(Math.ceil(snapshot.warmth));
  byId('condition-value').textContent = snapshot.health <= 30 ? 'РАНЕН' : snapshot.warmth <= 25 ? 'ХОЛОД' : 'СТАБИЛЬНО';
  byId('player-status').classList.toggle('critical', snapshot.health <= 30);
  byId('player-status').classList.toggle('cold', snapshot.warmth <= 25 && snapshot.health > 30);
  byId('dodge-value').textContent = snapshot.dodgeCooldown > 0 ? `${snapshot.dodgeCooldown.toFixed(1)} С` : 'ГОТОВО';
  byId('dodge-status').classList.toggle('cooldown', snapshot.dodgeCooldown > 0);
  byId('beacon-health-bar').style.width = `${snapshot.beaconHealth}%`;
  byId('beacon-percent').textContent = `${Math.ceil(snapshot.beaconHealth)}%`;
  byId('beacon-health-bar').classList.toggle('critical', snapshot.beaconHealth <= 30);
  byId('health-bar').classList.toggle('critical', snapshot.health <= 30);
  (['wood', 'scrap', 'crystal'] as const).forEach((resource) => {
    const value = byId(`resource-${resource}`).querySelector('strong');
    if (value) value.textContent = String(snapshot.inventory[resource]);
  });

  const isFinal = snapshot.finalCharge !== null;
  byId('phase-label').textContent = isFinal ? 'ФИНАЛЬНЫЙ ШТУРМ' : `${snapshot.phase === 'day' ? 'ДЕНЬ' : 'НОЧЬ'} ${snapshot.day}`;
  byId('phase-hint').textContent = isFinal ? 'ДО СИГНАЛА' : snapshot.phase === 'day' ? 'ДО НОЧИ' : 'ДО РАССВЕТА';
  byId('phase-time').textContent = formatTime(snapshot.phaseTime);
  byId('phase-icon').innerHTML = icon(isFinal ? 'bolt' : snapshot.phase === 'day' ? 'sun' : 'moon');
  byId('phase-card').classList.toggle('night', snapshot.phase === 'night');
  byId('phase-card').classList.toggle('final', isFinal);
  byId('phase-progress').style.width = `${Math.max(0, Math.min(100, (snapshot.phaseTime / snapshot.phaseDuration) * 100))}%`;

  byId('objective-title').textContent = isFinal ? 'Переживите штурм' : snapshot.cores === 3 ? 'Держите свет' : 'Зарядите маяк';
  byId('objective-detail').textContent = isFinal
    ? `До сигнала спасателям: ${formatTime(snapshot.finalCharge ?? 0)}. Не отходите далеко.`
    : 'Для каждой ячейки: 6 металла и 4 осколка. Установка — E у маяка.';
  byId('core-count').textContent = `${snapshot.cores} / 3`;
  document.querySelectorAll('.core-socket').forEach((socket, index) => socket.classList.toggle('active', index < snapshot.cores));

  byId('enemy-status').innerHTML = snapshot.phase === 'night'
    ? `${icon('skull')} Пепельных: ${snapshot.enemies}`
    : `${icon('sun')} Остров спокоен`;
  byId('kill-count').textContent = `${snapshot.kills} убито`;
  const prompt = byId('interaction-prompt');
  prompt.hidden = snapshot.prompt.length === 0;
  byId('prompt-title').textContent = snapshot.prompt;
  byId('prompt-detail').textContent = snapshot.promptDetail;

  document.querySelectorAll<HTMLButtonElement>('[data-recipe]').forEach((button) => {
    const recipe = button.dataset.recipe as RecipeId;
    button.classList.toggle('unaffordable', !snapshot.canAfford[recipe]);
    button.classList.toggle('context-needed', recipe === 'repair' && !snapshot.nearBeacon);
    button.title = snapshot.canAfford[recipe] ? 'Создать' : 'Недостаточно ресурсов — нажмите, чтобы узнать, чего не хватает';
  });
}

const game = new Game(byId<HTMLCanvasElement>('game-canvas'), {
  onSnapshot: updateSnapshot,
  onState: handleState,
  onNotify: notify,
  onAnnounce: showAnnouncement,
  onLoot: showLoot,
  onDamage: () => {
    const flash = byId('damage-flash');
    flash.classList.remove('active');
    void flash.offsetWidth;
    flash.classList.add('active');
  },
});
void game.ready.then(() => {
  const startButton = document.querySelector<HTMLButtonElement>('[data-action="start"]');
  if (startButton) {
    startButton.disabled = false;
    const label = startButton.querySelector('span');
    if (label) label.textContent = 'НАЧАТЬ СМЕНУ';
  }
}).catch((error: unknown) => {
  console.error('Не удалось инициализировать 3D-мир:', error);
  const label = document.querySelector('[data-action="start"] span');
  if (label) label.textContent = 'ОШИБКА WEBGL — ОБНОВИТЕ СТРАНИЦУ';
});

function updateSoundButtons(): void {
  document.querySelectorAll<HTMLButtonElement>('[data-action="sound"]').forEach((button) => {
    button.innerHTML = icon(game.sound.enabled ? 'volume' : 'muted');
    button.setAttribute('aria-label', game.sound.enabled ? 'Выключить звук' : 'Включить звук');
    button.classList.toggle('muted', !game.sound.enabled);
  });
}

function closeHow(): void {
  howModal.hidden = true;
}

const actions: Record<string, () => void> = {
  start: () => game.start(),
  pause: () => game.pause(),
  resume: () => game.resume(),
  restart: () => game.start(),
  menu: () => game.toMenu(),
  how: () => { howModal.hidden = false; },
  'close-how': closeHow,
  sound: () => { game.toggleSound(); updateSoundButtons(); },
};

document.querySelectorAll<HTMLButtonElement>('[data-action]').forEach((button) => {
  button.addEventListener('click', () => {
    const action = button.dataset.action;
    if (action && actions[action]) {
      if (action !== 'sound') game.sound.play('click');
      actions[action]();
    }
  });
});
document.querySelectorAll<HTMLButtonElement>('[data-recipe]').forEach((button) => {
  button.addEventListener('click', () => game.craft(button.dataset.recipe as RecipeId));
});

howModal.addEventListener('click', (event) => {
  if (event.target === howModal) closeHow();
});
document.addEventListener('keydown', (event) => {
  if (howModal.hidden) return;
  // The field guide is modal: game shortcuts must not leak through it.
  event.stopImmediatePropagation();
  if (event.code === 'Escape') {
    event.preventDefault();
    closeHow();
  }
}, true);

const joystick = byId('joystick');
const joystickKnob = byId('joystick-knob');
let joystickPointer: number | null = null;
function moveJoystick(event: PointerEvent): void {
  const bounds = joystick.getBoundingClientRect();
  const centerX = bounds.left + bounds.width / 2;
  const centerY = bounds.top + bounds.height / 2;
  const maxDistance = bounds.width * 0.3;
  let dx = event.clientX - centerX;
  let dy = event.clientY - centerY;
  const length = Math.hypot(dx, dy);
  if (length > maxDistance) {
    dx = (dx / length) * maxDistance;
    dy = (dy / length) * maxDistance;
  }
  joystickKnob.style.transform = `translate(${dx}px, ${dy}px)`;
  game.setJoystick(dx / maxDistance, -dy / maxDistance);
}
joystick.addEventListener('pointerdown', (event) => {
  event.preventDefault();
  event.stopPropagation();
  joystickPointer = event.pointerId;
  joystick.setPointerCapture(event.pointerId);
  moveJoystick(event);
});
joystick.addEventListener('pointermove', (event) => {
  if (event.pointerId === joystickPointer) moveJoystick(event);
});
const resetJoystick = (event: PointerEvent): void => {
  if (event.pointerId !== joystickPointer) return;
  joystickPointer = null;
  joystickKnob.style.transform = 'translate(0, 0)';
  game.setJoystick(0, 0);
};
joystick.addEventListener('pointerup', resetJoystick);
joystick.addEventListener('pointercancel', resetJoystick);

document.querySelectorAll<HTMLButtonElement>('[data-mobile]').forEach((button) => {
  button.addEventListener('pointerdown', (event) => {
    event.preventDefault();
    event.stopPropagation();
    if (button.dataset.mobile === 'attack') game.attack();
    else if (button.dataset.mobile === 'interact') game.interact();
    else if (button.dataset.mobile === 'dodge') game.dodge();
  });
});

updateSoundButtons();
if (latestSnapshot) updateSnapshot(latestSnapshot);
