import '@fontsource-variable/manrope';
import '@fontsource/unbounded/400.css';
import '@fontsource/unbounded/500.css';
import './style.css';

import { AudioEngine } from './audio.js';
import { drawConstellations, drawSky, makeStars } from './constellation.js';
import { LEVELS, LOOP_DURATION, PLAYER_RADIUS, WORLD } from './levels.js';
import {
  circleIntersectsRect,
  clamp,
  constellationIdentity,
  distance,
  isGateOpen,
  isPlateActive,
  moveCircle,
  sampleRecording,
} from './logic.js';

const icons = {
  arrow: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 12h13M13 6l6 6-6 6"/></svg>',
  sound: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 10v4h3l4 4V6L8 10H5Z"/><path class="sound-waves" d="M15 9c1.5 1.7 1.5 4.3 0 6M18 6c3.2 3.3 3.2 8.7 0 12"/></svg>',
  pause: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 6v12M15 6v12"/></svg>',
  restart: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 8V4m0 0h4M5 4l3 3a7 7 0 1 1-2 8"/></svg>',
  download: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 4v11m-4-4 4 4 4-4M5 18v2h14v-2"/></svg>',
  play: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="m9 6 9 6-9 6V6Z"/></svg>',
  home: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="m4 11 8-7 8 7M7 10v10h10V10M10 20v-6h4v6"/></svg>',
  github: '<svg class="github-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M12 .7a11.5 11.5 0 0 0-3.64 22.4c.58.1.79-.25.79-.55v-2.14c-3.22.7-3.9-1.36-3.9-1.36-.53-1.35-1.3-1.71-1.3-1.71-1.06-.72.08-.7.08-.7 1.17.08 1.79 1.2 1.79 1.2 1.04 1.79 2.74 1.27 3.41.97.1-.76.4-1.27.74-1.56-2.57-.29-5.27-1.28-5.27-5.72 0-1.26.45-2.29 1.2-3.1-.12-.29-.52-1.46.11-3.05 0 0 .97-.31 3.18 1.18a11.1 11.1 0 0 1 5.8 0c2.2-1.49 3.17-1.18 3.17-1.18.63 1.59.23 2.76.12 3.05.75.81 1.2 1.84 1.2 3.1 0 4.45-2.71 5.42-5.29 5.71.42.36.78 1.06.78 2.14v3.17c0 .31.21.67.8.55A11.5 11.5 0 0 0 12 .7Z"/></svg>',
};

const app = document.querySelector('#app');
app.innerHTML = `
  <div class="app-shell">
    <header class="hud" id="hud" hidden>
      <div class="hud-left">
        <div class="mini-brand" aria-label="ЭХО"><i></i>ЭХО</div>
        <div class="chapter-meta">
          <span class="chapter-numeral" id="chapterNumeral">I</span>
          <div><span class="micro-label">ГЛАВА</span><strong id="chapterTitle">ОСТАТЬСЯ</strong></div>
        </div>
      </div>
      <div class="loop-meta" aria-label="Текущий цикл">
        <div class="loop-dial" id="loopDial"><span id="loopCount">01</span></div>
        <div><span class="micro-label">ДО ПЕРЕЗАПУСКА</span><strong id="loopTimer">00:45</strong></div>
      </div>
      <div class="hud-actions">
        <button class="icon-button sound-toggle" aria-label="Выключить звук" title="Звук (M)">${icons.sound}</button>
        <button class="icon-button" id="pauseButton" aria-label="Пауза" title="Пауза (Esc)">${icons.pause}</button>
      </div>
    </header>

    <main class="stage" id="stage">
      <canvas id="gameCanvas" aria-label="Игровое поле. Управляй светящейся точкой с помощью WASD, стрелок или касания."></canvas>
      <div class="vignette" aria-hidden="true"></div>

      <div class="chapter-intro" id="chapterIntro" hidden>
        <div class="intro-line"></div>
        <p id="introNumber">ГЛАВА I</p>
        <h2 id="introTitle">ОСТАТЬСЯ</h2>
        <span id="introSubtitle">Иногда, чтобы пройти, нужно остаться.</span>
        <small>НАЖМИ ПРОБЕЛ, ЧТОБЫ ПРОДОЛЖИТЬ</small>
      </div>

      <div class="awakening-copy" id="awakeningCopy" hidden>
        <span>ЗАПИСЬ 03 / ОШИБКА ПОВТОРА</span>
        <p id="awakeningText">Цикл завершён.</p>
      </div>
      <button class="skip-button" id="skipAwakening" hidden>Пропустить ${icons.arrow}</button>
      <div class="toast" id="toast" role="status" hidden></div>
    </main>

    <footer class="game-controls" id="gameControls" hidden>
      <div class="narrative">
        <span class="objective" id="objective"><i></i>Коснись ядра за дверью</span>
        <p id="caption" aria-live="polite">Двигайся. Свет покажет путь.</p>
      </div>
      <div class="control-actions">
        <span class="movement-hint"><kbd>WASD</kbd><span>или удерживай<br>на игровом поле</span></span>
        <button class="echo-button" id="echoButton">
          <span class="echo-button-orbit" aria-hidden="true"></span>
          <span><small>ВЕРНУТЬСЯ В НАЧАЛО</small><strong>ОСТАВИТЬ ЭХО</strong></span>
          <kbd>SPACE</kbd>
        </button>
      </div>
    </footer>

    <section class="screen start-screen" id="startScreen">
      <div class="start-top">
        <span class="edition">ЭХО <i></i> ОПЫТ № 001</span>
        <button class="text-icon-button sound-toggle" aria-label="Выключить звук">${icons.sound}<span class="sound-label">ЗВУК ВКЛ</span></button>
      </div>
      <div class="start-content">
        <div class="logo-orbits" aria-hidden="true">
          <div class="orbit orbit-one"><i></i></div>
          <div class="orbit orbit-two"><i></i></div>
          <div class="orbit orbit-three"><i></i></div>
        </div>
        <p class="kicker">ИНТЕРАКТИВНАЯ ИСТОРИЯ · 5 МИНУТ</p>
        <h1 class="logo" data-text="ЭХО">ЭХО</h1>
        <p class="tagline">Твои прошлые попытки —<br>единственный путь вперёд.</p>
        <button class="primary-button" id="startButton"><span>ВОЙТИ В ПЕТЛЮ</span>${icons.arrow}</button>
        <div class="start-instructions">
          <span><kbd>WASD</kbd> / <kbd>←↑↓→</kbd> / касание <b>движение</b></span>
          <i></i>
          <span><kbd>SPACE</kbd> <b>оставить эхо</b></span>
        </div>
      </div>
      <div class="start-bottom">
        <span><i class="headphone-icon"></i> ЛУЧШЕ СО ЗВУКОМ</span>
        <span>ТРИ ГЛАВЫ · ОДИН ВЫХОД</span>
      </div>
    </section>

    <section class="screen pause-screen" id="pauseScreen" hidden>
      <div class="modal-card">
        <p class="kicker">ПЕТЛЯ ПРИОСТАНОВЛЕНА</p>
        <h2>Время подождёт.</h2>
        <p>Твои эхо никуда не уйдут.</p>
        <div class="modal-actions">
          <button class="primary-button" id="resumeButton"><span>ПРОДОЛЖИТЬ</span>${icons.play}</button>
          <button class="secondary-button" id="restartButton">${icons.restart}<span>НАЧАТЬ ГЛАВУ ЗАНОВО</span></button>
          <button class="secondary-button" id="menuButton">${icons.home}<span>В ГЛАВНОЕ МЕНЮ</span></button>
        </div>
      </div>
    </section>

    <section class="screen end-screen" id="endScreen" hidden>
      <div class="end-card">
        <div class="end-heading">
          <p class="kicker">ПЕТЛЯ РАЗОМКНУТА</p>
          <h2>Пока ты искал выход,<br><em>эхо рисовало небо.</em></h2>
        </div>
        <div class="end-details">
          <p>Это созвездие собрано из твоих настоящих движений. Повтори игру — и небо станет другим.</p>
          <div class="constellation-identity">
            <span>ТВОЁ СОЗВЕЗДИЕ</span>
            <strong id="constellationName">Тихий маяк</strong>
            <code id="constellationCode">№ 000000</code>
          </div>
          <div class="run-stats"><span id="runCycles">7 циклов</span><i></i><span id="runTime">03:42</span><i></i><span>3 главы</span></div>
          <div class="end-actions">
            <button class="primary-button" id="saveButton"><span>СОХРАНИТЬ НЕБО</span>${icons.download}</button>
            <button class="secondary-button" id="replayButton">${icons.restart}<span>ПРОЙТИ ИНАЧЕ</span></button>
          </div>
          <a class="html-download" href="https://github.com/Ritmirchic/Game/releases/download/v1.0.0/echo.html" download="echo.html" aria-label="Скачать автономную HTML-версию игры с GitHub">
            ${icons.github}
            <span><small>ОДИН ФАЙЛ · БЕЗ УСТАНОВКИ</small><strong>СКАЧАТЬ HTML С GITHUB</strong></span>
            ${icons.download}
          </a>
        </div>
      </div>
    </section>
  </div>
`;

const $ = (selector) => document.querySelector(selector);
const canvas = $('#gameCanvas');
const ctx = canvas.getContext('2d');
const stage = $('#stage');
const hud = $('#hud');
const gameControls = $('#gameControls');
const startScreen = $('#startScreen');
const pauseScreen = $('#pauseScreen');
const endScreen = $('#endScreen');
const chapterIntro = $('#chapterIntro');
const awakeningCopy = $('#awakeningCopy');
const skipAwakeningButton = $('#skipAwakening');
const echoButton = $('#echoButton');
const caption = $('#caption');
const objective = $('#objective');
const toast = $('#toast');
const audio = new AudioEngine();
const stars = makeStars(230);

const state = {
  mode: 'menu',
  previousMode: 'playing',
  modeTime: 0,
  levelIndex: 0,
  level: LEVELS[0],
  loop: 1,
  totalLoops: 1,
  loopTime: 0,
  runElapsed: 0,
  player: { ...LEVELS[0].spawn },
  recording: [],
  echoes: [],
  echoPositions: [],
  archive: LEVELS.map(() => []),
  plateStates: new Map(),
  flags: {},
  particles: [],
  resetFlash: 0,
  shake: 0,
  awakeningEchoIndex: -1,
  awakeningStart: null,
  identity: null,
};

const input = {
  keys: new Set(),
  pointerActive: false,
  pointer: { x: 0, y: 0 },
};

const view = { width: 1, height: 1, dpr: 1, scale: 1, offsetX: 0, offsetY: 0 };
let globalTime = 0;
let toastTimer = 0;
let awakeningLine = -1;

function formatTime(seconds) {
  const value = Math.max(0, Math.floor(seconds));
  return `${String(Math.floor(value / 60)).padStart(2, '0')}:${String(value % 60).padStart(2, '0')}`;
}

function setCaption(text) {
  if (caption.textContent !== text) caption.textContent = text;
}

function showToast(text, duration = 2.2) {
  toast.textContent = text;
  toast.hidden = false;
  toastTimer = duration;
}

function resizeCanvas() {
  const rect = stage.getBoundingClientRect();
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const width = Math.max(1, rect.width);
  const height = Math.max(1, rect.height);
  if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
  }
  view.width = width;
  view.height = height;
  view.dpr = dpr;
  view.scale = Math.min((width - 24) / WORLD.width, (height - 24) / WORLD.height);
  view.offsetX = (width - WORLD.width * view.scale) / 2;
  view.offsetY = (height - WORLD.height * view.scale) / 2;
}

function pointerToWorld(event) {
  const rect = canvas.getBoundingClientRect();
  const screenX = event.clientX - rect.left;
  const screenY = event.clientY - rect.top;
  return {
    x: (screenX - view.offsetX) / view.scale,
    y: (screenY - view.offsetY) / view.scale,
  };
}

function updateSoundButtons() {
  document.querySelectorAll('.sound-toggle').forEach((button) => {
    button.classList.toggle('is-muted', !audio.enabled);
    button.setAttribute('aria-label', audio.enabled ? 'Выключить звук' : 'Включить звук');
    button.setAttribute('aria-pressed', String(audio.enabled));
    const label = button.querySelector('.sound-label');
    if (label) label.textContent = audio.enabled ? 'ЗВУК ВКЛ' : 'ЗВУК ВЫКЛ';
  });
}

async function toggleSound() {
  await audio.setEnabled(!audio.enabled);
  updateSoundButtons();
  if (audio.enabled) audio.ui();
}

function clearInput() {
  input.keys.clear();
  input.pointerActive = false;
}

function resetRun() {
  state.levelIndex = 0;
  state.level = LEVELS[0];
  state.totalLoops = 1;
  state.runElapsed = 0;
  state.archive = LEVELS.map(() => []);
  state.particles = [];
  state.identity = null;
}

async function startGame() {
  await audio.start();
  audio.ui();
  resetRun();
  startScreen.hidden = true;
  endScreen.hidden = true;
  pauseScreen.hidden = true;
  hud.hidden = false;
  gameControls.hidden = false;
  echoButton.disabled = false;
  startLevel(0);
}

function startLevel(index) {
  state.levelIndex = index;
  state.level = LEVELS[index];
  state.loop = 1;
  state.loopTime = 0;
  state.player = { ...state.level.spawn };
  state.recording = [{ t: 0, ...state.player }];
  state.echoes = [];
  state.echoPositions = [];
  state.plateStates = new Map(state.level.plates.map((plate) => [plate.id, false]));
  state.flags = {};
  state.mode = 'chapterIntro';
  state.modeTime = 0;
  state.awakeningEchoIndex = -1;
  clearInput();

  $('#chapterNumeral').textContent = state.level.numeral;
  $('#chapterTitle').textContent = state.level.title;
  $('#introNumber').textContent = `ГЛАВА ${state.level.numeral}`;
  $('#introTitle').textContent = state.level.title;
  $('#introSubtitle').textContent = state.level.subtitle;
  objective.innerHTML = `<i></i>${state.level.objective}`;
  setCaption(state.level.subtitle);
  chapterIntro.hidden = false;
  awakeningCopy.hidden = true;
  skipAwakeningButton.hidden = true;
  echoButton.classList.remove('attention');
  echoButton.disabled = false;
  updateHUD();
}

function beginPlaying() {
  if (state.mode !== 'chapterIntro') return;
  state.mode = 'playing';
  state.modeTime = 0;
  chapterIntro.hidden = true;
  setCaption(state.levelIndex === 0
    ? 'Пластина открывает дверь. Попробуй удержать её и пройти.'
    : state.level.objective);
}

function spawnBurst(x, y, color, count = 24, speed = 120) {
  const available = Math.max(0, 260 - state.particles.length);
  for (let index = 0; index < Math.min(count, available); index += 1) {
    const angle = (index / count) * Math.PI * 2 + Math.random() * 0.3;
    const velocity = speed * (0.35 + Math.random() * 0.75);
    const life = 0.65 + Math.random() * 0.9;
    state.particles.push({
      x, y,
      vx: Math.cos(angle) * velocity,
      vy: Math.sin(angle) * velocity,
      life,
      maxLife: life,
      size: 1 + Math.random() * 2.4,
      color,
    });
  }
}

function createEcho(automatic = false) {
  if (state.mode !== 'playing') return;
  if (state.loopTime < 0.35) {
    showToast('Сначала сделай хотя бы один шаг.');
    return;
  }

  const oldPlayer = { ...state.player };
  const recording = state.recording.slice();
  state.archive[state.levelIndex].push(recording);
  state.echoes.push({ points: recording, id: `${state.levelIndex}-${state.loop}` });
  if (state.echoes.length > 6) state.echoes.shift();
  state.loop += 1;
  state.totalLoops += 1;
  state.loopTime = 0;
  state.player = { ...state.level.spawn };
  state.recording = [{ t: 0, ...state.player }];
  state.echoPositions = state.echoes.map((echo) => sampleRecording(echo.points, 0));
  state.mode = 'rewind';
  state.modeTime = 0;
  state.resetFlash = 1;
  state.shake = 1;
  clearInput();
  spawnBurst(oldPlayer.x, oldPlayer.y, state.level.accent, 32, 145);
  spawnBurst(state.player.x, state.player.y, '#f4efdf', 20, 90);
  audio.echo();
  echoButton.classList.remove('attention');

  if (state.loop === 2) setCaption(state.level.firstEcho);
  else if (state.level.echoOnly && state.flags.coreTouched) setCaption('Запись сохранена. Теперь позволь эху дойти до ядра.');
  else setCaption(automatic ? 'Цикл замкнулся сам. Все прежние пути останутся с тобой.' : `Цикл ${state.loop}. Прошлое повторяется. Ты можешь выбрать иначе.`);
}

function completeLevel(echoIndex = -1) {
  if (state.mode !== 'playing') return;
  state.archive[state.levelIndex].push(state.recording.slice());
  state.mode = 'levelComplete';
  state.modeTime = 0;
  state.awakeningEchoIndex = echoIndex;
  state.awakeningStart = echoIndex >= 0 ? { ...state.echoPositions[echoIndex] } : null;
  state.shake = 0.7;
  clearInput();
  spawnBurst(state.level.core.x, state.level.core.y, state.level.accent, 70, 190);
  audio.complete();
  echoButton.disabled = true;
  setCaption(state.level.completion);
}

function startAwakening() {
  state.mode = 'awakening';
  state.modeTime = 0;
  awakeningLine = -1;
  awakeningCopy.hidden = false;
  skipAwakeningButton.hidden = false;
  objective.innerHTML = '<i></i>Запись прервана';
  setCaption('Цикл завершён. Но что-то осталось.');
  audio.awaken();
}

const AWAKENING_LINES = [
  [0, 'Цикл завершён.'],
  [1.65, 'Но одно эхо не вернулось в начало.'],
  [3.55, 'Оно сделало шаг, которого не было в записи.'],
  [5.65, 'Впервые — не повторение. Выбор.'],
];

function showEnding() {
  if (state.mode === 'ending') return;
  state.mode = 'ending';
  state.modeTime = 0;
  awakeningCopy.hidden = true;
  skipAwakeningButton.hidden = true;
  chapterIntro.hidden = true;
  hud.hidden = true;
  gameControls.hidden = true;
  pauseScreen.hidden = true;
  state.identity = constellationIdentity(state.archive);
  $('#constellationName').textContent = state.identity.name;
  $('#constellationCode').textContent = `№ ${state.identity.code}`;
  $('#runCycles').textContent = `${state.totalLoops} ${state.totalLoops === 1 ? 'цикл' : state.totalLoops < 5 ? 'цикла' : 'циклов'}`;
  $('#runTime').textContent = formatTime(state.runElapsed);
  endScreen.hidden = false;
}

function restartLevel() {
  if (state.recording.length > 1) state.archive[state.levelIndex].push(state.recording.slice());
  pauseScreen.hidden = true;
  hud.hidden = false;
  gameControls.hidden = false;
  startLevel(state.levelIndex);
}

function goToMenu() {
  state.mode = 'menu';
  state.modeTime = 0;
  clearInput();
  pauseScreen.hidden = true;
  endScreen.hidden = true;
  chapterIntro.hidden = true;
  awakeningCopy.hidden = true;
  skipAwakeningButton.hidden = true;
  hud.hidden = true;
  gameControls.hidden = true;
  startScreen.hidden = false;
}

function togglePause() {
  if (state.mode === 'paused') {
    state.mode = state.previousMode;
    pauseScreen.hidden = true;
    clearInput();
    return;
  }
  if (!['playing', 'rewind', 'chapterIntro'].includes(state.mode)) return;
  state.previousMode = state.mode;
  state.mode = 'paused';
  pauseScreen.hidden = false;
  clearInput();
}

function currentEntities() {
  return [state.player, ...state.echoPositions];
}

function gateOpen(gate) {
  return isGateOpen(gate, state.level.plates, currentEntities(), PLAYER_RADIUS);
}

function updateHints() {
  const { level, flags } = state;
  if (level.id === 'remain') {
    const plateActive = isPlateActive(level.plates[0], currentEntities());
    if (plateActive && !flags.plateHint && state.echoes.length === 0) {
      flags.plateHint = true;
      setCaption('Дверь открыта, но стоит уйти — она закроется. Останься на пластине и нажми ПРОБЕЛ.');
      echoButton.classList.add('attention');
    }
    const gate = level.gates[0];
    const nearGate = Math.abs(state.player.x - gate.x) < 58 && state.player.y > gate.y - 30 && state.player.y < gate.y + gate.h + 30;
    if (nearGate && !gateOpen(gate) && !flags.doorHint) {
      flags.doorHint = true;
      setCaption('Двери нужен кто-то у пластины. Оставь там эхо: нажми ПРОБЕЛ, затем вернёшься в начало.');
      echoButton.classList.add('attention');
    }
  }

  if (level.id === 'trust') {
    const activeCount = level.plates.filter((plate) => isPlateActive(plate, currentEntities())).length;
    if (activeCount > 0 && activeCount < 2 && !flags.plateHint) {
      flags.plateHint = true;
      setCaption('Дверь отвечает только на обе пластины сразу. Оставь здесь эхо и вернись в начало.');
      echoButton.classList.add('attention');
    }
    if (state.echoes.length === 1 && state.loopTime > 1 && !flags.secondHint) {
      flags.secondHint = true;
      setCaption('Одно прошлое уже помогает. Оставь второе эхо у другой пластины.');
    }
  }
}

function updatePlaying(dt) {
  state.loopTime += dt;
  state.echoPositions = state.echoes.map((echo) => sampleRecording(echo.points, state.loopTime));

  let horizontal = 0;
  let vertical = 0;
  if (input.keys.has('KeyA') || input.keys.has('ArrowLeft')) horizontal -= 1;
  if (input.keys.has('KeyD') || input.keys.has('ArrowRight')) horizontal += 1;
  if (input.keys.has('KeyW') || input.keys.has('ArrowUp')) vertical -= 1;
  if (input.keys.has('KeyS') || input.keys.has('ArrowDown')) vertical += 1;

  if (!horizontal && !vertical && input.pointerActive) {
    const deltaX = input.pointer.x - state.player.x;
    const deltaY = input.pointer.y - state.player.y;
    const length = Math.hypot(deltaX, deltaY);
    if (length > 7) {
      horizontal = deltaX / length;
      vertical = deltaY / length;
    }
  }

  const length = Math.hypot(horizontal, vertical);
  if (length > 0) {
    horizontal /= length;
    vertical /= length;
    const speed = 238;
    const obstacles = [
      ...state.level.walls,
      ...state.level.gates.filter((gate) => !gateOpen(gate)),
    ];
    state.player = moveCircle(state.player, horizontal * speed * dt, vertical * speed * dt, PLAYER_RADIUS, obstacles);
  }

  state.recording.push({ t: state.loopTime, x: state.player.x, y: state.player.y });

  for (const plate of state.level.plates) {
    const active = isPlateActive(plate, currentEntities());
    if (state.plateStates.get(plate) !== active) {
      state.plateStates.set(plate, active);
      audio.plate(active);
    }
  }

  updateHints();
  const core = state.level.core;
  const playerAtCore = distance(state.player, core) < core.r + PLAYER_RADIUS + 2;

  if (state.level.echoOnly) {
    if (playerAtCore && !state.flags.coreTouched) {
      state.flags.coreTouched = true;
      state.shake = 0.35;
      audio.locked();
      setCaption('Ядро не чувствует тебя. Оно ждёт эхо. Останься рядом и нажми ПРОБЕЛ.');
      objective.innerHTML = '<i></i>Позволь эху коснуться ядра';
      echoButton.classList.add('attention');
      spawnBurst(core.x, core.y, '#7ce7df', 22, 75);
    }
    const touchingEcho = state.echoPositions.findIndex((position) => position && distance(position, core) < core.r + 9);
    if (touchingEcho >= 0) {
      completeLevel(touchingEcho);
      return;
    }
  } else if (playerAtCore) {
    completeLevel();
    return;
  }

  if (state.loopTime >= LOOP_DURATION) createEcho(true);
}

function updateParticles(dt) {
  for (const particle of state.particles) {
    particle.life -= dt;
    particle.x += particle.vx * dt;
    particle.y += particle.vy * dt;
    particle.vx *= Math.pow(0.15, dt);
    particle.vy *= Math.pow(0.15, dt);
  }
  state.particles = state.particles.filter((particle) => particle.life > 0);
}

function update(dt) {
  if (state.mode === 'paused') return;
  if (!['menu', 'ending'].includes(state.mode)) state.runElapsed += dt;
  state.modeTime += dt;
  state.resetFlash = Math.max(0, state.resetFlash - dt * 1.9);
  state.shake = Math.max(0, state.shake - dt * 2.5);
  updateParticles(dt);

  if (toastTimer > 0) {
    toastTimer -= dt;
    if (toastTimer <= 0) toast.hidden = true;
  }

  switch (state.mode) {
    case 'chapterIntro':
      if (state.modeTime >= 2.8) beginPlaying();
      break;
    case 'playing':
      updatePlaying(dt);
      break;
    case 'rewind':
      if (state.modeTime >= 0.52) {
        state.mode = 'playing';
        state.modeTime = 0;
      }
      break;
    case 'levelComplete':
      if (state.modeTime >= 2.25) {
        if (state.levelIndex < LEVELS.length - 1) startLevel(state.levelIndex + 1);
        else startAwakening();
      }
      break;
    case 'awakening': {
      const lineIndex = AWAKENING_LINES.findLastIndex(([time]) => state.modeTime >= time);
      if (lineIndex !== awakeningLine && lineIndex >= 0) {
        awakeningLine = lineIndex;
        $('#awakeningText').textContent = AWAKENING_LINES[lineIndex][1];
      }
      if (state.modeTime > 1.6 && state.awakeningStart && Math.random() < dt * 18) {
        const progress = clamp((state.modeTime - 1.6) / 2.4, 0, 1);
        spawnBurst(
          state.awakeningStart.x + Math.sin(progress * Math.PI) * 22,
          state.awakeningStart.y - progress * 104,
          '#7ce7df', 1, 22,
        );
      }
      if (state.modeTime >= 7.7) showEnding();
      break;
    }
    default:
      break;
  }
}

function updateHUD() {
  $('#loopCount').textContent = String(state.loop).padStart(2, '0');
  $('#loopTimer').textContent = formatTime(Math.ceil(LOOP_DURATION - state.loopTime));
  const progress = clamp(state.loopTime / LOOP_DURATION, 0, 1);
  $('#loopDial').style.setProperty('--progress', `${progress * 360}deg`);
}

function roundedRect(context, x, y, width, height, radius) {
  context.beginPath();
  context.roundRect(x, y, width, height, radius);
}

function drawWorldBase() {
  const gradient = ctx.createRadialGradient(500, 280, 40, 500, 300, 610);
  gradient.addColorStop(0, '#101727');
  gradient.addColorStop(0.65, '#0b111e');
  gradient.addColorStop(1, '#080d17');
  roundedRect(ctx, 0, 0, WORLD.width, WORLD.height, 22);
  ctx.fillStyle = gradient;
  ctx.fill();

  ctx.save();
  roundedRect(ctx, 0, 0, WORLD.width, WORLD.height, 22);
  ctx.clip();
  ctx.strokeStyle = 'rgba(153, 181, 211, 0.045)';
  ctx.lineWidth = 1;
  for (let x = 50; x < WORLD.width; x += 50) {
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, WORLD.height); ctx.stroke();
  }
  for (let y = 50; y < WORLD.height; y += 50) {
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(WORLD.width, y); ctx.stroke();
  }

  const glow = ctx.createRadialGradient(state.player.x, state.player.y, 0, state.player.x, state.player.y, 180);
  glow.addColorStop(0, 'rgba(244, 239, 223, 0.055)');
  glow.addColorStop(1, 'rgba(244, 239, 223, 0)');
  ctx.fillStyle = glow;
  ctx.fillRect(0, 0, WORLD.width, WORLD.height);
}

function drawConnections() {
  for (const gate of state.level.gates) {
    for (const plateId of gate.plates) {
      const plate = state.level.plates.find((candidate) => candidate.id === plateId);
      const active = isPlateActive(plate, currentEntities());
      ctx.save();
      ctx.strokeStyle = active ? `${state.level.accent}66` : 'rgba(131, 153, 181, 0.12)';
      ctx.lineWidth = active ? 1.5 : 1;
      ctx.setLineDash([4, 9]);
      ctx.lineDashOffset = -globalTime * (active ? 22 : 5);
      ctx.beginPath();
      ctx.moveTo(plate.x, plate.y);
      ctx.lineTo(gate.x + gate.w / 2, plate.y);
      ctx.lineTo(gate.x + gate.w / 2, gate.y + gate.h / 2);
      ctx.stroke();
      ctx.restore();
    }
  }
}

function drawPaths() {
  state.echoes.forEach((echo, index) => {
    const points = echo.points;
    if (points.length < 2) return;
    const stride = Math.max(1, Math.floor(points.length / 120));
    ctx.save();
    ctx.strokeStyle = index % 2 ? 'rgba(169, 161, 255, 0.105)' : 'rgba(124, 231, 223, 0.12)';
    ctx.lineWidth = 1.2;
    ctx.setLineDash([2, 7]);
    ctx.beginPath();
    for (let pointIndex = 0; pointIndex < points.length; pointIndex += stride) {
      const point = points[pointIndex];
      if (pointIndex === 0) ctx.moveTo(point.x, point.y);
      else ctx.lineTo(point.x, point.y);
    }
    ctx.stroke();
    ctx.restore();
  });
}

function drawWalls() {
  for (const wall of state.level.walls) {
    const gradient = ctx.createLinearGradient(wall.x, wall.y, wall.x, wall.y + Math.min(wall.h, 80));
    gradient.addColorStop(0, '#202838');
    gradient.addColorStop(1, '#141b29');
    ctx.fillStyle = gradient;
    ctx.fillRect(wall.x, wall.y, wall.w, wall.h);
    ctx.strokeStyle = 'rgba(172, 194, 219, 0.12)';
    ctx.lineWidth = 1;
    ctx.strokeRect(wall.x + 0.5, wall.y + 0.5, wall.w - 1, wall.h - 1);
  }
}

function drawGate(gate) {
  const open = gateOpen(gate);
  const centerX = gate.x + gate.w / 2;
  ctx.save();
  ctx.fillStyle = '#283246';
  ctx.fillRect(gate.x - 5, gate.y - 7, gate.w + 10, 8);
  ctx.fillRect(gate.x - 5, gate.y + gate.h - 1, gate.w + 10, 8);

  if (open) {
    ctx.strokeStyle = `${state.level.accent}55`;
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 12]);
    ctx.lineDashOffset = -globalTime * 35;
    ctx.beginPath();
    ctx.moveTo(centerX, gate.y + 8);
    ctx.lineTo(centerX, gate.y + gate.h - 8);
    ctx.stroke();
  } else {
    const field = ctx.createLinearGradient(gate.x, 0, gate.x + gate.w, 0);
    field.addColorStop(0, 'rgba(124, 231, 223, 0.06)');
    field.addColorStop(0.5, 'rgba(124, 231, 223, 0.28)');
    field.addColorStop(1, 'rgba(124, 231, 223, 0.06)');
    ctx.fillStyle = field;
    ctx.fillRect(gate.x, gate.y, gate.w, gate.h);
    ctx.shadowColor = state.level.accent;
    ctx.shadowBlur = 9;
    for (let y = gate.y + 8; y < gate.y + gate.h - 4; y += 14) {
      ctx.strokeStyle = 'rgba(150, 239, 231, 0.52)';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(gate.x + 3, y);
      ctx.lineTo(gate.x + gate.w - 3, y);
      ctx.stroke();
    }
  }
  ctx.restore();
}

function drawPlate(plate) {
  const active = isPlateActive(plate, currentEntities());
  const color = state.level.accent;
  ctx.save();
  ctx.translate(plate.x, plate.y);
  if (active) {
    const glow = ctx.createRadialGradient(0, 0, 0, 0, 0, 75);
    glow.addColorStop(0, `${color}30`);
    glow.addColorStop(1, `${color}00`);
    ctx.fillStyle = glow;
    ctx.fillRect(-75, -75, 150, 150);
  }
  ctx.strokeStyle = active ? color : 'rgba(150, 170, 197, 0.28)';
  ctx.lineWidth = active ? 2 : 1;
  ctx.shadowColor = color;
  ctx.shadowBlur = active ? 15 : 0;
  ctx.beginPath(); ctx.arc(0, 0, plate.r, 0, Math.PI * 2); ctx.stroke();
  ctx.globalAlpha = active ? 0.7 : 0.25;
  ctx.rotate(globalTime * (active ? 0.7 : 0.15));
  ctx.setLineDash([8, 8]);
  ctx.beginPath(); ctx.arc(0, 0, plate.r + 9, 0, Math.PI * 2); ctx.stroke();
  ctx.setLineDash([]);
  ctx.rotate(-globalTime * (active ? 0.7 : 0.15));
  ctx.globalAlpha = 1;
  ctx.fillStyle = active ? color : '#283449';
  ctx.beginPath(); ctx.arc(0, 0, active ? 8 : 5, 0, Math.PI * 2); ctx.fill();
  ctx.shadowBlur = 0;
  ctx.fillStyle = active ? '#dffcf8' : 'rgba(188, 204, 224, 0.42)';
  ctx.font = '500 9px Manrope, sans-serif';
  ctx.textAlign = 'center';
  ctx.fillText(plate.label, 0, plate.r + 25);
  ctx.restore();
}

function drawCore() {
  const core = state.level.core;
  const touched = state.flags.coreTouched;
  const color = touched && state.level.echoOnly ? '#7ce7df' : state.level.accent;
  const pulse = 1 + Math.sin(globalTime * 2.4) * 0.055;
  ctx.save();
  ctx.translate(core.x, core.y);
  const glow = ctx.createRadialGradient(0, 0, 2, 0, 0, 105);
  glow.addColorStop(0, `${color}55`);
  glow.addColorStop(0.35, `${color}15`);
  glow.addColorStop(1, `${color}00`);
  ctx.fillStyle = glow;
  ctx.fillRect(-110, -110, 220, 220);
  ctx.rotate(globalTime * 0.22);
  ctx.strokeStyle = `${color}88`;
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 8]);
  ctx.beginPath(); ctx.arc(0, 0, (core.r + 17) * pulse, 0, Math.PI * 2); ctx.stroke();
  ctx.rotate(-globalTime * 0.65);
  ctx.setLineDash([18, 12]);
  ctx.beginPath(); ctx.arc(0, 0, core.r + 7, 0, Math.PI * 2); ctx.stroke();
  ctx.setLineDash([]);
  ctx.rotate(globalTime * 0.43 + Math.PI / 4);
  ctx.shadowColor = color;
  ctx.shadowBlur = 25;
  ctx.fillStyle = '#f4fff9';
  ctx.fillRect(-core.r * 0.42, -core.r * 0.42, core.r * 0.84, core.r * 0.84);
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.strokeRect(-core.r * 0.7, -core.r * 0.7, core.r * 1.4, core.r * 1.4);
  ctx.restore();
}

function drawEcho(position, index, specialPosition = null) {
  if (!position) return;
  const point = specialPosition || position;
  const echo = state.echoes[index];
  ctx.save();
  for (let trail = 3; trail >= 1; trail -= 1) {
    const past = sampleRecording(echo.points, state.loopTime - trail * 0.075);
    if (!past) continue;
    ctx.globalAlpha = 0.045 * (4 - trail);
    ctx.fillStyle = '#7ce7df';
    ctx.beginPath(); ctx.arc(past.x, past.y, 10 - trail, 0, Math.PI * 2); ctx.fill();
  }
  ctx.globalAlpha = specialPosition ? 1 : 0.78;
  const glow = ctx.createRadialGradient(point.x, point.y, 0, point.x, point.y, 40);
  glow.addColorStop(0, 'rgba(124, 231, 223, 0.35)');
  glow.addColorStop(1, 'rgba(124, 231, 223, 0)');
  ctx.fillStyle = glow;
  ctx.fillRect(point.x - 40, point.y - 40, 80, 80);
  ctx.shadowColor = '#7ce7df';
  ctx.shadowBlur = specialPosition ? 28 : 15;
  ctx.fillStyle = '#9df4ed';
  ctx.beginPath(); ctx.arc(point.x, point.y, specialPosition ? 10 : 8, 0, Math.PI * 2); ctx.fill();
  ctx.shadowBlur = 0;
  ctx.strokeStyle = 'rgba(124, 231, 223, 0.5)';
  ctx.lineWidth = 1;
  ctx.setLineDash([3, 5]);
  ctx.lineDashOffset = globalTime * 8;
  ctx.beginPath(); ctx.arc(point.x, point.y, 17 + Math.sin(globalTime * 2 + index) * 1.5, 0, Math.PI * 2); ctx.stroke();
  if (!specialPosition) {
    ctx.setLineDash([]);
    ctx.fillStyle = 'rgba(158, 217, 217, 0.46)';
    ctx.font = '500 8px Manrope, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(`ЭХО ${String(index + 1).padStart(2, '0')}`, point.x, point.y - 27);
  }
  ctx.restore();
}

function drawPlayer() {
  const fade = state.mode === 'awakening' ? Math.max(0.18, 1 - state.modeTime * 0.13) : 1;
  ctx.save();
  ctx.globalAlpha = fade;
  const recent = state.recording.slice(-42);
  recent.forEach((point, index) => {
    if (index % 5 !== 0) return;
    ctx.globalAlpha = (index / Math.max(1, recent.length)) * 0.11 * fade;
    ctx.fillStyle = '#f4efdf';
    ctx.beginPath(); ctx.arc(point.x, point.y, 3 + index / recent.length * 4, 0, Math.PI * 2); ctx.fill();
  });
  ctx.globalAlpha = fade;
  const glow = ctx.createRadialGradient(state.player.x, state.player.y, 0, state.player.x, state.player.y, 58);
  glow.addColorStop(0, 'rgba(255, 243, 209, 0.44)');
  glow.addColorStop(1, 'rgba(255, 243, 209, 0)');
  ctx.fillStyle = glow;
  ctx.fillRect(state.player.x - 58, state.player.y - 58, 116, 116);
  ctx.shadowColor = '#fff0c2';
  ctx.shadowBlur = 24;
  ctx.fillStyle = '#fff9e9';
  ctx.beginPath(); ctx.arc(state.player.x, state.player.y, 10, 0, Math.PI * 2); ctx.fill();
  ctx.shadowBlur = 0;
  ctx.strokeStyle = 'rgba(255, 241, 204, 0.58)';
  ctx.lineWidth = 1;
  ctx.beginPath(); ctx.arc(state.player.x, state.player.y, 19 + Math.sin(globalTime * 3) * 1.5, 0, Math.PI * 2); ctx.stroke();
  ctx.restore();
}

function drawParticles() {
  ctx.save();
  for (const particle of state.particles) {
    ctx.globalAlpha = clamp(particle.life / particle.maxLife, 0, 1);
    ctx.fillStyle = particle.color;
    ctx.shadowColor = particle.color;
    ctx.shadowBlur = 8;
    ctx.beginPath(); ctx.arc(particle.x, particle.y, particle.size, 0, Math.PI * 2); ctx.fill();
  }
  ctx.restore();
}

function drawPointer() {
  if (!input.pointerActive || state.mode !== 'playing') return;
  ctx.save();
  ctx.strokeStyle = 'rgba(244, 239, 223, 0.35)';
  ctx.lineWidth = 1;
  ctx.beginPath(); ctx.arc(input.pointer.x, input.pointer.y, 9 + Math.sin(globalTime * 5) * 2, 0, Math.PI * 2); ctx.stroke();
  ctx.beginPath(); ctx.arc(input.pointer.x, input.pointer.y, 2, 0, Math.PI * 2); ctx.fillStyle = '#f4efdf'; ctx.fill();
  ctx.restore();
}

function renderWorld() {
  const shakeX = Math.sin(globalTime * 91) * state.shake * 3;
  const shakeY = Math.cos(globalTime * 77) * state.shake * 2;
  ctx.save();
  ctx.translate(view.offsetX + shakeX, view.offsetY + shakeY);
  ctx.scale(view.scale, view.scale);
  drawWorldBase();
  drawConnections();
  drawPaths();
  drawWalls();
  state.level.gates.forEach(drawGate);
  state.level.plates.forEach(drawPlate);
  drawCore();

  let specialPosition = null;
  if (state.mode === 'awakening' && state.awakeningStart) {
    const progress = clamp((state.modeTime - 1.6) / 2.4, 0, 1);
    const eased = 1 - Math.pow(1 - progress, 3);
    specialPosition = {
      x: state.awakeningStart.x + Math.sin(eased * Math.PI) * 22,
      y: state.awakeningStart.y - eased * 104,
    };
  }
  state.echoPositions.forEach((position, index) => {
    drawEcho(position, index, index === state.awakeningEchoIndex ? specialPosition : null);
  });
  drawPlayer();
  drawParticles();
  drawPointer();

  if (state.resetFlash > 0) {
    ctx.globalAlpha = state.resetFlash * 0.24;
    ctx.fillStyle = '#a7fff5';
    ctx.fillRect(0, 0, WORLD.width, WORLD.height);
    ctx.globalAlpha = 1;
  }
  ctx.restore();

  ctx.save();
  ctx.strokeStyle = 'rgba(172, 201, 219, 0.1)';
  ctx.lineWidth = 1;
  roundedRect(ctx, view.offsetX, view.offsetY, WORLD.width * view.scale, WORLD.height * view.scale, 22 * view.scale);
  ctx.stroke();
  ctx.restore();
}

function render() {
  resizeCanvas();
  ctx.setTransform(view.dpr, 0, 0, view.dpr, 0, 0);
  ctx.clearRect(0, 0, view.width, view.height);
  drawSky(ctx, view.width, view.height, stars, globalTime, state.mode === 'menu' ? 0.75 : 0.48);

  if (state.mode === 'menu') return;
  if (state.mode === 'ending') {
    const reveal = clamp(state.modeTime / 2.8, 0, 1);
    drawConstellations(ctx, state.archive, view.width, view.height, {
      time: globalTime,
      reveal,
      region: { x: 0, y: 4, w: view.width, h: view.height * 0.53 },
    });
    return;
  }
  renderWorld();
  updateHUD();
}

function saveConstellation() {
  if (!state.identity) return;
  audio.ui();
  const output = document.createElement('canvas');
  output.width = 1800;
  output.height = 1200;
  const context = output.getContext('2d');
  drawSky(context, output.width, output.height, stars, 2.4, 0.9);

  context.strokeStyle = 'rgba(160, 188, 207, 0.18)';
  context.lineWidth = 1;
  context.strokeRect(48.5, 48.5, 1703, 1103);
  context.fillStyle = 'rgba(218, 231, 238, 0.55)';
  context.font = '500 18px Manrope, sans-serif';
  context.textAlign = 'left';
  context.fillText('Э Х О  /  ЛИЧНОЕ НЕБО', 92, 105);
  context.textAlign = 'right';
  context.fillText(`ЗАПИСЬ № ${state.identity.code}`, 1708, 105);

  drawConstellations(context, state.archive, output.width, output.height, {
    time: 2.4,
    reveal: 1,
    region: { x: 65, y: 105, w: 1670, h: 690 },
  });

  const gradient = context.createLinearGradient(0, 790, 0, 1200);
  gradient.addColorStop(0, 'rgba(5, 7, 13, 0)');
  gradient.addColorStop(0.35, 'rgba(5, 7, 13, 0.92)');
  gradient.addColorStop(1, '#05070d');
  context.fillStyle = gradient;
  context.fillRect(0, 760, 1800, 440);

  context.textAlign = 'center';
  context.fillStyle = 'rgba(218, 231, 238, 0.5)';
  context.font = '500 17px Manrope, sans-serif';
  context.fillText('ТВОЁ СОЗВЕЗДИЕ', 900, 870);
  context.fillStyle = '#f2f0e8';
  context.font = '400 62px Unbounded, Manrope, sans-serif';
  context.fillText(state.identity.name, 900, 955);
  context.fillStyle = 'rgba(218, 231, 238, 0.62)';
  context.font = '400 23px Manrope, sans-serif';
  context.fillText('Каждая линия — настоящее движение. Каждый поворот — твой.', 900, 1012);
  context.fillStyle = 'rgba(218, 231, 238, 0.36)';
  context.font = '500 16px Manrope, sans-serif';
  context.fillText(`${state.totalLoops} ЦИКЛОВ  ·  ${formatTime(state.runElapsed)}  ·  ${new Date().toLocaleDateString('ru-RU')}`, 900, 1090);

  output.toBlob((blob) => {
    if (!blob) return;
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `echo-${state.identity.code}.png`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    const label = $('#saveButton span');
    label.textContent = 'НЕБО СОХРАНЕНО';
    setTimeout(() => { label.textContent = 'СОХРАНИТЬ НЕБО'; }, 2200);
  }, 'image/png');
}

$('#startButton').addEventListener('click', startGame);
$('#replayButton').addEventListener('click', startGame);
$('#saveButton').addEventListener('click', saveConstellation);
$('#pauseButton').addEventListener('click', togglePause);
$('#resumeButton').addEventListener('click', togglePause);
$('#restartButton').addEventListener('click', restartLevel);
$('#menuButton').addEventListener('click', goToMenu);
$('#skipAwakening').addEventListener('click', showEnding);
echoButton.addEventListener('click', () => createEcho());
chapterIntro.addEventListener('click', beginPlaying);
document.querySelectorAll('.sound-toggle').forEach((button) => button.addEventListener('click', toggleSound));

canvas.addEventListener('pointerdown', (event) => {
  if (state.mode !== 'playing') return;
  event.preventDefault();
  input.pointerActive = true;
  input.pointer = pointerToWorld(event);
  canvas.setPointerCapture(event.pointerId);
});
canvas.addEventListener('pointermove', (event) => {
  if (!input.pointerActive) return;
  input.pointer = pointerToWorld(event);
});
canvas.addEventListener('pointerup', () => { input.pointerActive = false; });
canvas.addEventListener('pointercancel', () => { input.pointerActive = false; });

window.addEventListener('keydown', (event) => {
  const focusedButton = event.target instanceof Element && event.target.closest('button');
  const nativeButtonKey = focusedButton && ['Space', 'Enter'].includes(event.code) &&
    !['playing', 'chapterIntro', 'awakening'].includes(state.mode);
  if (nativeButtonKey) return; // Preserve normal keyboard activation in menus.
  if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Space'].includes(event.code)) event.preventDefault();
  if (event.code === 'KeyM' && !event.repeat) {
    toggleSound();
    return;
  }
  if (event.code === 'Escape' && !event.repeat) {
    if (state.mode === 'ending') return;
    togglePause();
    return;
  }
  if (event.code === 'Space' && !event.repeat) {
    if (state.mode === 'playing') createEcho();
    else if (state.mode === 'chapterIntro') beginPlaying();
    else if (state.mode === 'awakening') showEnding();
    return;
  }
  if (event.code === 'Enter' && state.mode === 'menu' && !event.repeat) {
    startGame();
    return;
  }
  input.keys.add(event.code);
});
window.addEventListener('keyup', (event) => input.keys.delete(event.code));
window.addEventListener('blur', () => {
  clearInput();
  if (state.mode === 'playing') togglePause();
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden && state.mode === 'playing') togglePause();
});
window.addEventListener('resize', resizeCanvas);

const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
if (reducedMotion) document.documentElement.classList.add('reduced-motion');

const STEP = 1 / 60;
let previousTime = performance.now();
let accumulator = 0;
function frame(now) {
  const elapsed = Math.min(0.05, Math.max(0, (now - previousTime) / 1000));
  previousTime = now;
  globalTime += elapsed;
  if (state.mode !== 'paused') {
    accumulator += elapsed;
    while (accumulator >= STEP) {
      update(STEP);
      accumulator -= STEP;
    }
  } else {
    accumulator = 0;
  }
  render();
  requestAnimationFrame(frame);
}

updateSoundButtons();
resizeCanvas();
requestAnimationFrame(frame);

// A tiny read-only surface for automated smoke checks and debugging.
window.__ECHO__ = {
  get state() {
    return {
      mode: state.mode,
      level: state.levelIndex,
      loop: state.loop,
      echoes: state.echoes.length,
      player: { ...state.player },
    };
  },
};
